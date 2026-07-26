#include "MeshCraft/Renderer/SceneRenderer.hpp"
#include "MeshCraft/CoordinateSystemAlgorithms.hpp"
#include "MeshCraft/Renderer/CsgCacheAlg.hpp"
#include "MeshCraft/Renderer/PrimitiveTessellationAlg.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"
#include "MeshCraft/GraphicsBackendCheck.hpp"
#include <iostream>

#include <Microsoft/Xna/Framework/Graphics/BufferUsage.hpp>
#include <Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp>
#include <Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp>
#include <Microsoft/Xna/Framework/Graphics/SamplerState.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp>
#include <Microsoft/Xna/Framework/MathHelper.hpp>
#include <Microsoft/Xna/Framework/Vector2.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <numeric>
#include <numbers>
#include <optional>
#include <vector>

#include <manifold/manifold.h>
#include <tiny_obj_loader.h>
#include "CsgEvaluator.hpp"
#include "MeshBuilder.hpp"  // mc3togltf_lib -- buildPrimitive(), shared with CsgEvaluator.cpp (STAB-0670)
#include "SvgRasterizer.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Renderer;

namespace {

// CNA's Matrix is intentionally kept out of CoordinateSystemAlgorithms.hpp.
// This is the renderer adapter from the shared MC3 convention to a row-vector
// world matrix: authored Z-up values are rotated into native Y-up space.
Matrix coordinateSystemRootMatrix(const Mc3Document& doc) {
    return MeshCraft::usesRightHandedZUpAlg(doc.coordinateSystem)
        ? Matrix::CreateRotationX(-std::numbers::pi_v<float> / 2.0f)
        : Matrix::getIdentityProperty();
}

Vector3 coordinateSystemDirectionToYUp(const Mc3Document& doc,
                                       const std::array<float, 3>& value) {
    const auto converted = MeshCraft::coordinateToYUpAlg(doc.coordinateSystem, value);
    return {converted[0], converted[1], converted[2]};
}

// AUD-085.  ShaderEffect's 3D path supplies these matrices for an ordinary
// DrawIndexedPrimitives call; writing gl_FragCoord.z keeps precisely the same
// non-linear depth convention that the former sampled DEPTH_COMPONENT texture
// used.  It is stored in a normal color RenderTarget2D because CNA deliberately
// does not expose render-target depth attachments as Texture2D objects.
constexpr const char* kDepthPassVertSrc = R"(#version 300 es
precision highp float;
layout(location = 0) in vec3 aPosition;
uniform mat4 World;
uniform mat4 View;
uniform mat4 Projection;
void main() {
    gl_Position = Projection * View * World * vec4(aPosition, 1.0);
}
)";

constexpr const char* kDepthPassFragSrc = R"(#version 300 es
precision highp float;
out vec4 fragColor;
void main() {
    float rawDepth = gl_FragCoord.z;
    fragColor = vec4(rawDepth, rawDepth, rawDepth, 1.0);
}
)";

SamplerState samplerStateForSvg(const Mc3SvgTexture& texture) {
    SamplerState sampler = texture.filter == "nearest"
        ? SamplerState::PointWrap : SamplerState::LinearWrap;
    auto addressMode = [](const std::string& value) {
        if (value == "clamp") return TextureAddressMode::Clamp;
        if (value == "mirror") return TextureAddressMode::Mirror;
        return TextureAddressMode::Wrap;
    };
    sampler.setAddressUProperty(addressMode(texture.wrapU));
    sampler.setAddressVProperty(addressMode(texture.wrapV));
    // Texture2D::CreateFromPixels supplies only level zero. mipMaps is still
    // preserved and honored by glTF export; generating a live CNA mip chain
    // would require a CNA API this repository does not own.
    return sampler;
}

} // namespace

// ---------------------------------------------------------------------------
// CSG helpers (file scope)
// ---------------------------------------------------------------------------

// Defined below the OBJ loader. Keeping this forward declaration here lets the
// CSG path share the same guarded MeshData-to-CNA upload used by embedded GLB
// meshes instead of maintaining a second, flat-shaded buffer builder.
static RenderMesh uploadMeshData(GraphicsDevice& device, const mc3togltf::MeshData& source);

// XNA row-major → manifold mat3x4 (3 rows × 4 cols, linalg column-major storage)
// XNA: v' = v * M  →  manifold Transform: v' = M * (v,1)
// manifold mat3x4[col][row]: col0=X axis, col1=Y axis, col2=Z axis, col3=translation
static manifold::mat3x4 xnaToManifoldMat(const Matrix& m) {
    return manifold::mat3x4(
        {m.M11, m.M12, m.M13},   // col 0: X basis
        {m.M21, m.M22, m.M23},   // col 1: Y basis
        {m.M31, m.M32, m.M33},   // col 2: Z basis
        {m.M41, m.M42, m.M43}    // col 3: translation
    );
}

static Matrix computeObjWorldMatrix(const Mc3Object& obj) {
    const auto& t = obj.transform;
    constexpr float d = std::numbers::pi_v<float> / 180.0f;
    float px = t.pivot[0], py = t.pivot[1], pz = t.pivot[2];
    return Matrix::CreateTranslation({-px,-py,-pz}) *
           Matrix::CreateScale({t.scale[0], t.scale[1], t.scale[2]}) *
           Matrix::CreateFromYawPitchRoll(t.rotation[1]*d, t.rotation[0]*d, t.rotation[2]*d) *
           Matrix::CreateTranslation({t.position[0]+px, t.position[1]+py, t.position[2]+pz});
}

// Content-based CSG cache invalidation (K1): csgSubtreeHashAlg()/csgHashMixAlg()
// live in include/MeshCraft/Renderer/CsgCacheAlg.hpp (STAB-0214/0215) so the
// hash logic can be unit tested directly without a live GraphicsDevice.
using MeshCraft::csgHashMixAlg;
using MeshCraft::csgSubtreeHashAlg;

// Build a manifold::Manifold for `obj` and its subtree.
// parentToWorld: cumulative transform from the CSG root's parent space to world.
// Leaf primitives are placed in world space; result drawn with identity matrix.
static manifold::Manifold buildManifoldTree(
    const Mc3Object& obj, const Mc3Document& doc,
    const Matrix& parentToWorld, int depth)
{
    using namespace manifold;
    if (depth > 12 || !obj.visible) return Manifold{};

    // STAB-0671: matches mc3togltf/src/CsgEvaluator.cpp's CSG_SEGMENTS (was
    // 24 here vs 32 there) -- curved-primitive CSG previews were visibly
    // less smooth than the final export for no functional reason.
    constexpr int SEG = 32;
    Matrix objWorld = computeObjWorldMatrix(obj) * parentToWorld;
    Matrix deformMat = obj.deform
        ? Matrix::CreateScale({obj.deform->scale[0], obj.deform->scale[1], obj.deform->scale[2]})
        : Matrix::getIdentityProperty();
    Matrix fullWorld = deformMat * objWorld;

    auto applyTransform = [&](Manifold m) {
        return m.Transform(xnaToManifoldMat(fullWorld));
    };

    switch (obj.type) {
    case ObjectType::Box:
    case ObjectType::Cube: {
        float sx = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float sy = obj.primitive ? obj.primitive->size[1] : 1.0f;
        float sz = obj.primitive ? obj.primitive->size[2] : 1.0f;
        return applyTransform(Manifold::Cube({sx, sy, sz}, /*center=*/true));
    }
    case ObjectType::Sphere: {
        float r = obj.primitive ? obj.primitive->radius : 0.5f;
        return applyTransform(Manifold::Sphere(r, SEG));
    }
    case ObjectType::Cylinder: {
        float r = obj.primitive ? obj.primitive->radius : 0.5f;
        float h = obj.primitive ? obj.primitive->height : 1.0f;
        return applyTransform(Manifold::Cylinder(h, r, r, SEG, /*center=*/true));
    }
    case ObjectType::Cone: {
        float r = obj.primitive ? obj.primitive->radius : 0.5f;
        float h = obj.primitive ? obj.primitive->height : 1.0f;
        return applyTransform(Manifold::Cylinder(h, r, 0.0f, SEG, /*center=*/true));
    }
    // STAB-0670: Torus/Capsule/IcoSphere have no native Manifold primitive
    // constructor (unlike Box/Sphere/Cylinder/Cone above) -- previously fell
    // through to `default: return Manifold{}`, silently previewing as empty
    // even though CsgEvaluator.cpp (export-time) has always supported them.
    // Reuses mc3togltf_lib's buildPrimitive() so the preview is built from
    // the exact same triangulation as the real export, not a re-derived
    // approximation.
    case ObjectType::Torus:
    case ObjectType::Capsule:
    case ObjectType::IcoSphere: {
        if (!obj.primitive) return Manifold{};
        mc3togltf::MeshData md = mc3togltf::buildPrimitive(*obj.primitive);
        if (md.empty() || md.positions.empty() || md.indices.empty()) return Manifold{};
        MeshGL gl;
        gl.numProp = 3;
        gl.vertProperties.assign(md.positions.begin(), md.positions.end());
        gl.triVerts.assign(md.indices.begin(), md.indices.end());
        // STAB-0670 follow-up: buildPrimitive()'s output duplicates vertices
        // at UV seams (correct for rendering, but leaves the raw triangle
        // soup non-manifold) -- Merge() welds colocated verts within
        // tolerance so Manifold's strict topology check succeeds. Without
        // this, Torus/Capsule/IcoSphere fail NotManifold here exactly like
        // they do in CsgEvaluator.cpp (same fix applied there).
        //
        // STAB-0700: an explicit tolerance is required for Torus specifically
        // -- Merge()'s own auto-computed baseline tolerance is too tight for
        // its seam vertices (root-caused and verified safe across scales in
        // CsgEvaluator.cpp's matching comment; same fix applied here to keep
        // the preview and export paths identical).
        gl.tolerance = 1e-4f;
        gl.Merge();
        Manifold m(gl);
        if (m.Status() != Manifold::Error::NoError) return Manifold{};
        return applyTransform(m);
    }
    case ObjectType::Union: {
        Manifold result;
        for (const auto& child : obj.children)
            result = result + buildManifoldTree(*child, doc, objWorld, depth + 1);
        return result;
    }
    case ObjectType::Intersection: {
        if (obj.children.empty()) return Manifold{};
        Manifold result = buildManifoldTree(*obj.children[0], doc, objWorld, depth + 1);
        for (size_t i = 1; i < obj.children.size(); ++i)
            result = result ^ buildManifoldTree(*obj.children[i], doc, objWorld, depth + 1);
        return result;
    }
    case ObjectType::Difference: {
        Manifold base;
        for (const auto& child : obj.children)
            if (!child->isCutter)
                base = base + buildManifoldTree(*child, doc, objWorld, depth + 1);
        for (const auto& child : obj.children)
            if (child->isCutter)
                base = base - buildManifoldTree(*child, doc, objWorld, depth + 1);
        return base;
    }
    case ObjectType::Group:
    case ObjectType::Area: {
        Manifold result;
        for (const auto& child : obj.children)
            result = result + buildManifoldTree(*child, doc, objWorld, depth + 1);
        return result;
    }
    case ObjectType::Instance: {
        auto it = doc.definitions.find(obj.resolvedInstanceDefinitionKey());
        if (it != doc.definitions.end() && it->second)
            return buildManifoldTree(*it->second, doc, objWorld, depth + 1);
        return Manifold{};
    }
    default:
        return Manifold{};  // Extrude/Mesh: not supported in CSG boolean
    }
}

// STAB-0672: mirrors buildManifoldTree()'s own recursion/bail-out rules
// (depth limit, invisible-subtree skip, Instance-definition resolution) to
// report *why* a CSG result may have silently dropped content, rather than
// leaving the author to guess from "Tris: 0" alone whether that's a
// legitimate empty result or an unsupported child being ignored.
static std::string csgSubtreeWarning(const Mc3Object& obj, const Mc3Document& doc, int depth) {
    if (!obj.visible) return {};   // matches buildManifoldTree: hidden subtrees contribute nothing, not an error
    if (depth > 12) return "max CSG nesting depth (12) exceeded — deeper content is dropped";

    switch (obj.type) {
    case ObjectType::Mesh:
    case ObjectType::Extrude:
        return "contains unsupported Mesh/Extrude geometry (not representable in CSG booleans)";
    case ObjectType::Union:
    case ObjectType::Intersection:
    case ObjectType::Difference:
    case ObjectType::Group:
    case ObjectType::Area:
        for (const auto& child : obj.children) {
            std::string w = csgSubtreeWarning(*child, doc, depth + 1);
            if (!w.empty()) return w;
        }
        return {};
    case ObjectType::Instance: {
        auto it = doc.definitions.find(obj.resolvedInstanceDefinitionKey());
        if (it != doc.definitions.end() && it->second)
            return csgSubtreeWarning(*it->second, doc, depth + 1);
        return {};
    }
    default:
        return {};   // primitives buildManifoldTree already knows how to build
    }
}

static void applyCsgUvMapping(mc3togltf::MeshData& mesh, const Mc3Object& obj) {
    if (!obj.uvMapping.has_value()) {
        mesh.applyBoxProjectionUv();
        return;
    }
    const auto& uv = *obj.uvMapping;
    if (uv.projection == UvProjection::Box) {
        mesh.applyBoxProjectionUv();
    } else if (uv.projection == UvProjection::Sphere) {
        mesh.applySphereProjectionUv();
    } else {
        mesh.applyPlanarProjectionUv();
    }
    mesh.applyUvMapping(uv.scaleU, uv.scaleV, uv.offsetU, uv.offsetV, uv.rotation);
}

// Convert a Manifold result to the same smooth-normal/UV layout used by glTF
// CSG export. `m` is already world-space in the viewport path, and therefore
// its generated projection is world-anchored; the glTF exporter performs the
// equivalent operation in the CSG root's local space before node transforms.
static RenderMesh manifoldToRenderMesh(GraphicsDevice& device, const manifold::Manifold& m,
                                       const Mc3Object& csgRoot) {
    mc3togltf::MeshData data = mc3togltf::meshDataFromCsgManifold(m);
    if (data.empty()) return {};
    applyCsgUvMapping(data, csgRoot);
    return uploadMeshData(device, data);
}

// ---------------------------------------------------------------------------
// OBJ mesh loader
// ---------------------------------------------------------------------------

// Load an OBJ file and return a RenderMesh with both a VertexPositionColor VB
// (for flat-colour rendering) and a VertexPositionNormalTexture VB (for lit/
// textured rendering). Both use triangle-soup layout with sequential uint32
// indices to avoid vertex-count limits.
// Returns an empty RenderMesh (no vb) on failure.
static RenderMesh loadObjMesh(GraphicsDevice& device, const std::string& path)
{
    RenderMesh mesh;

    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate     = true;
    cfg.mtl_search_path = std::filesystem::path(path).parent_path().string();

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, cfg)) return mesh;  // error → empty

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    if (shapes.empty() || attrib.vertices.empty()) return mesh;

    // Guard against enormous meshes
    size_t totalTris = 0;
    for (const auto& s : shapes)
        for (auto fv : s.mesh.num_face_vertices)
            if (fv == 3) ++totalTris;
    if (totalTris > 300000) return mesh;   // too large for live preview

    Color grey(180, 180, 180, 255);
    std::vector<VertexPositionColor>          cverts;
    std::vector<VertexPositionNormalTexture>  tverts;

    for (const auto& shape : shapes) {
        size_t off = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int fv = shape.mesh.num_face_vertices[f];
            if (fv != 3) { off += fv; continue; }

            Vector3 p[3];
            for (int v = 0; v < 3; ++v) {
                int vi = shape.mesh.indices[off + v].vertex_index;
                p[v] = {attrib.vertices[3*vi], attrib.vertices[3*vi+1], attrib.vertices[3*vi+2]};
                cverts.push_back({p[v], grey});
            }

            // Face normal fallback
            Vector3 e1 = {p[1].X-p[0].X, p[1].Y-p[0].Y, p[1].Z-p[0].Z};
            Vector3 e2 = {p[2].X-p[0].X, p[2].Y-p[0].Y, p[2].Z-p[0].Z};
            Vector3 faceN = Vector3::Cross(e1, e2);
            faceN.Normalize();

            for (int v = 0; v < 3; ++v) {
                const auto& idx = shape.mesh.indices[off + v];
                Vector3 n = faceN;
                if (idx.normal_index >= 0) {
                    int ni = idx.normal_index;
                    n = {attrib.normals[3*ni], attrib.normals[3*ni+1], attrib.normals[3*ni+2]};
                }
                Vector2 uv = {0.0f, 0.0f};
                if (idx.texcoord_index >= 0) {
                    int ti = idx.texcoord_index;
                    uv = {attrib.texcoords[2*ti], 1.0f - attrib.texcoords[2*ti+1]};
                }
                tverts.push_back({p[v], n, uv});
            }
            off += fv;
        }
    }

    if (cverts.empty()) return mesh;

    const int nV = static_cast<int>(cverts.size());  // = nTris * 3

    // Sequential triangle-soup indices (same for both VBs)
    std::vector<uint32_t> seq(nV);
    std::iota(seq.begin(), seq.end(), 0u);

    // Colored VB / IB
    mesh.positions.reserve(nV);
    for (auto& v : cverts) mesh.positions.push_back(v.Position);
    mesh.vb = std::make_unique<VertexBuffer>(device, nV);
    mesh.vb->SetData(cverts.data(), nV);
    mesh.ib = std::make_unique<IndexBuffer>(device, IndexElementSize::ThirtyTwoBits, nV, BufferUsage::None);
    mesh.ib->SetData(seq.data(), nV);
    mesh.primitiveCount = nV / 3;

    // Lit / textured VB / IB
    mesh.texVB = std::make_unique<VertexBuffer>(device, nV);
    mesh.texVB->SetData(tverts.data(), nV);
    mesh.texIB = std::make_unique<IndexBuffer>(device, IndexElementSize::ThirtyTwoBits, nV, BufferUsage::None);
    mesh.texIB->SetData(seq.data(), nV);
    mesh.texPrimitiveCount = nV / 3;

    return mesh;
}

// Convert the CNA-independent MeshData shared with mc3togltf into the two
// viewport buffer layouts.  GLB embeds are flattened into triangle soup by
// loadEmbeddedGltfMesh(), but this routine deliberately accepts indexed data
// too so the bounds checks remain at the renderer boundary.
static RenderMesh uploadMeshData(GraphicsDevice& device, const mc3togltf::MeshData& source)
{
    RenderMesh mesh;
    if (source.indices.empty() || source.indices.size() % 3 != 0 ||
        source.positions.size() % 3 != 0 || source.indices.size() / 3 > 300000)
        return mesh;
    const size_t vertexCount = source.positions.size() / 3;
    const bool hasNormals = source.normals.size() == source.positions.size();
    const bool hasTexcoords = source.texcoords.size() == vertexCount * 2;
    Color grey(180, 180, 180, 255);
    std::vector<VertexPositionColor> cverts;
    std::vector<VertexPositionNormalTexture> tverts;
    cverts.reserve(source.indices.size());
    tverts.reserve(source.indices.size());

    for (size_t tri = 0; tri < source.indices.size(); tri += 3) {
        Vector3 points[3];
        for (int corner = 0; corner < 3; ++corner) {
            const uint32_t index = source.indices[tri + corner];
            if (index >= vertexCount) return RenderMesh{};
            points[corner] = {source.positions[index * 3], source.positions[index * 3 + 1],
                              source.positions[index * 3 + 2]};
            if (!std::isfinite(points[corner].X) || !std::isfinite(points[corner].Y) ||
                !std::isfinite(points[corner].Z)) return RenderMesh{};
        }
        Vector3 fallbackNormal{0.0f, 1.0f, 0.0f};
        if (!hasNormals) {
            fallbackNormal = Vector3::Cross(
                {points[1].X - points[0].X, points[1].Y - points[0].Y, points[1].Z - points[0].Z},
                {points[2].X - points[0].X, points[2].Y - points[0].Y, points[2].Z - points[0].Z});
            fallbackNormal.Normalize();
        }
        for (int corner = 0; corner < 3; ++corner) {
            const uint32_t index = source.indices[tri + corner];
            Vector3 normal = fallbackNormal;
            if (hasNormals)
                normal = {source.normals[index * 3], source.normals[index * 3 + 1],
                          source.normals[index * 3 + 2]};
            Vector2 uv{0.0f, 0.0f};
            if (hasTexcoords) uv = {source.texcoords[index * 2], source.texcoords[index * 2 + 1]};
            cverts.push_back({points[corner], grey});
            tverts.push_back({points[corner], normal, uv});
        }
    }
    if (cverts.empty()) return mesh;

    const int nVerts = static_cast<int>(cverts.size());
    std::vector<uint32_t> sequential(static_cast<size_t>(nVerts));
    std::iota(sequential.begin(), sequential.end(), 0u);
    mesh.positions.reserve(cverts.size());
    for (const auto& vertex : cverts) mesh.positions.push_back(vertex.Position);
    mesh.vb = std::make_unique<VertexBuffer>(device, nVerts);
    mesh.vb->SetData(cverts.data(), nVerts);
    mesh.ib = std::make_unique<IndexBuffer>(device, IndexElementSize::ThirtyTwoBits,
                                            nVerts, BufferUsage::None);
    mesh.ib->SetData(sequential.data(), nVerts);
    mesh.primitiveCount = nVerts / 3;
    mesh.texVB = std::make_unique<VertexBuffer>(device, nVerts);
    mesh.texVB->SetData(tverts.data(), nVerts);
    mesh.texIB = std::make_unique<IndexBuffer>(device, IndexElementSize::ThirtyTwoBits,
                                               nVerts, BufferUsage::None);
    mesh.texIB->SetData(sequential.data(), nVerts);
    mesh.texPrimitiveCount = nVerts / 3;
    return mesh;
}

static RenderMesh loadEmbeddedGltfMesh(GraphicsDevice& device,
                                       const std::filesystem::path& basePath,
                                       const Mc3EmbedGltf& embed)
{
    try {
        return uploadMeshData(device, mc3togltf::loadEmbeddedGltfMesh(basePath, embed));
    } catch (const std::exception& error) {
        std::cerr << "Warning: " << error.what() << '\n';
        return {};
    }
}

namespace MeshCraft::Renderer {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SceneRenderer::SceneRenderer(GraphicsDevice& device)
    : device_(device)
{
    effect_ = std::make_unique<BasicEffect>(device_);
    effect_->VertexColorEnabled = true;
    effect_->EnableDefaultLighting();           // set up 3-point light colors/directions once
    effect_->setAmbientLightColorProperty(Vector3{0.35f, 0.35f, 0.35f}); // brighter fill so shadow sides are not pitch-black
    effect_->setPreferPerPixelLightingProperty(true); // smoother on curved surfaces if CNA supports
    effect_->setLightingEnabledProperty(false); // off by default; enabled per draw in drawMeshTextured

    if (supportsTextShaderEffects()) {
        depthEffect_.emplace(device_, kDepthPassVertSrc, kDepthPassFragSrc);
        if (!depthEffect_->IsEffectValid()) {
            std::cerr << "[SSAO] Failed to compile depth-prepass shader\n";
            depthEffect_.reset();
        }
    }

    buildUnitBox();
    buildUnitSphere(32, unitSphere_);    buildUnitSphere(16, unitSphereL1_);   buildUnitSphere(6,  unitSphereL2_);
    buildUnitCylinder(24, unitCylinder_); buildUnitCylinder(12, unitCylinderL1_); buildUnitCylinder(6, unitCylinderL2_);
    buildUnitCone(24, unitCone_);         buildUnitCone(12, unitConeL1_);          buildUnitCone(6,  unitConeL2_);
    buildUnitPlane();
    buildUnitTorus(32, 16, unitTorus_);   buildUnitTorus(16, 8, unitTorusL1_);   buildUnitTorus(8, 4, unitTorusL2_);
    buildUnitCapsule(16, unitCapsule_);   buildUnitCapsule(8,  unitCapsuleL1_);   buildUnitCapsule(4, unitCapsuleL2_);
    // AUD-062: match mc3togltf::buildPrimitive()'s segments -> subdivision
    // mapping.  These are small, bounded meshes (20 * 4^n triangles for
    // n=1..4), so building all four once avoids any per-frame tessellation.
    for (int subdivisions = 1; subdivisions <= 4; ++subdivisions)
        buildUnitIcoSphere(subdivisions, unitIcoSpheres_[subdivisions - 1]);
    buildWireBox();
    buildWireShapes(16);
}


// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------

Matrix SceneRenderer::objectWorldMatrix(const Mc3Transform& t) const {
    float rx = t.rotation[0] * (std::numbers::pi_v<float> / 180.0f);
    float ry = t.rotation[1] * (std::numbers::pi_v<float> / 180.0f);
    float rz = t.rotation[2] * (std::numbers::pi_v<float> / 180.0f);

    // Pivot: world = T(-pivot) * S * R * T(pos + pivot)
    float px = t.pivot[0], py = t.pivot[1], pz = t.pivot[2];
    return Matrix::CreateTranslation({-px, -py, -pz}) *
           Matrix::CreateScale({t.scale[0], t.scale[1], t.scale[2]}) *
           Matrix::CreateFromYawPitchRoll(ry, rx, rz) *
           Matrix::CreateTranslation({t.position[0] + px, t.position[1] + py, t.position[2] + pz});
}

// AUD-031: was a hand-copied duplicate of materialColorAlg's own
// find-material/clamp/fallback-gray logic (materialColorAlg returns a
// plain float array instead of CNA's Color type, since it must stay
// CNA-free to be headlessly testable); now delegates to it and converts
// the result to Color here at the CNA boundary.
Color SceneRenderer::materialColor(const std::string& matId, const Mc3Document& doc) const {
    auto c = materialColorAlg(matId, doc);
    return Color(static_cast<int>(c[0] * 255), static_cast<int>(c[1] * 255),
                 static_cast<int>(c[2] * 255), static_cast<int>(c[3] * 255));
}

bool SceneRenderer::isSelected(const Mc3Object& obj, const std::vector<const Mc3Object*>& sel) const {
    return std::any_of(sel.begin(), sel.end(), [&](const Mc3Object* p){ return p == &obj; });
}

void SceneRenderer::drawMesh(const RenderMesh& mesh,
                              const Matrix& world, const Matrix& view, const Matrix& proj,
                              Color color)
{
    int n = mesh.vb->getVertexCountProperty();

    // Build a temporary VB tinted with the material color (cheap for small meshes)
    std::vector<VertexPositionColor> tinted(n);
    // We don't have a way to read back VB data, so we store the original verts in RenderMesh.
    // For now, use the color directly as a uniform tint by building a new VB.
    // This works because our unit shapes have few vertices.
    for (int i = 0; i < n; ++i)
        tinted[i] = { mesh.positions[i], color };

    VertexBuffer tmpVB(device_, n);
    tmpVB.SetData(tinted.data(), n);

    effect_->World      = world;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty()) {
        pass.Apply();
    }

    device_.SetVertexBuffer(&tmpVB);
    device_.SetIndexBuffer(mesh.ib.get());
    device_.DrawIndexedPrimitives(
        Graphics::PrimitiveType::TriangleList,
        0, 0,
        n,
        0,
        mesh.primitiveCount);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);
}

void SceneRenderer::drawMeshTextured(const RenderMesh& mesh,
                                      const Matrix& world, const Matrix& view, const Matrix& proj,
                                      Color color, Texture2D* tex, const SamplerState* sampler)
{
    if (!mesh.texVB || !mesh.texIB) {
        drawMesh(mesh, world, view, proj, color);
        return;
    }
    int n = mesh.texVB->getVertexCountProperty();

    effect_->World      = world;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = false;
    effect_->setLightingEnabledProperty(true);
    if (tex) {
        effect_->setTextureEnabledProperty(true);
        effect_->setTextureProperty(tex);
    } else {
        effect_->setTextureEnabledProperty(false);
    }
    effect_->setDiffuseColorProperty(Vector3{
        std::clamp(color.getRProperty() / 255.0f, 0.0f, 1.0f),
        std::clamp(color.getGProperty() / 255.0f, 0.0f, 1.0f),
        std::clamp(color.getBProperty() / 255.0f, 0.0f, 1.0f)});
    effect_->setAlphaProperty(std::clamp(color.getAProperty() / 255.0f, 0.0f, 1.0f));

    std::optional<SamplerState> previousSampler;
    if (tex && sampler) {
        auto& samplerSlot = device_.getSamplerStatesProperty()[0];
        previousSampler = samplerSlot;
        samplerSlot = *sampler;
    }

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(mesh.texVB.get());
    device_.SetIndexBuffer(mesh.texIB.get());
    device_.DrawIndexedPrimitives(
        Graphics::PrimitiveType::TriangleList,
        0, 0, n, 0, mesh.texPrimitiveCount);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);

    if (previousSampler)
        device_.getSamplerStatesProperty()[0] = *previousSampler;

    effect_->setTextureProperty(nullptr);
    effect_->setTextureEnabledProperty(false);
    effect_->setLightingEnabledProperty(false);
    effect_->VertexColorEnabled = true;
    effect_->setDiffuseColorProperty(Vector3{1,1,1});
    effect_->setAlphaProperty(1.0f);
}

bool SceneRenderer::depthPassAvailable() const
{
    return depthEffect_.has_value() && depthEffect_->IsEffectValid();
}

void SceneRenderer::drawDepthMesh(const RenderMesh& mesh,
                                  const Matrix& world, const Matrix& view, const Matrix& proj)
{
    if (!depthPassAvailable() || !mesh.vb || !mesh.ib || mesh.primitiveCount <= 0) return;
    depthEffect_->setWorldProperty(world);
    depthEffect_->setViewProperty(view);
    depthEffect_->setProjectionProperty(proj);
    depthEffect_->Apply();
    device_.SetVertexBuffer(mesh.vb.get());
    device_.SetIndexBuffer(mesh.ib.get());
    device_.DrawIndexedPrimitives(Graphics::PrimitiveType::TriangleList,
                                  0, 0, mesh.vb->getVertexCountProperty(),
                                  0, mesh.primitiveCount);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);
}

void SceneRenderer::drawDepthTriangles(const std::vector<VertexPositionColor>& vertices,
                                       const std::vector<uint16_t>& indices,
                                       const Matrix& world, const Matrix& view, const Matrix& proj)
{
    if (!depthPassAvailable() || vertices.empty() || indices.empty()) return;
    VertexBuffer vb(device_, static_cast<int>(vertices.size()));
    vb.SetData(const_cast<VertexPositionColor*>(vertices.data()), static_cast<int>(vertices.size()));
    IndexBuffer ib(device_, static_cast<int>(indices.size()));
    ib.SetData(const_cast<uint16_t*>(indices.data()), static_cast<int>(indices.size()));
    depthEffect_->setWorldProperty(world);
    depthEffect_->setViewProperty(view);
    depthEffect_->setProjectionProperty(proj);
    depthEffect_->Apply();
    device_.SetVertexBuffer(&vb);
    device_.SetIndexBuffer(&ib);
    device_.DrawIndexedPrimitives(Graphics::PrimitiveType::TriangleList,
                                  0, 0, static_cast<int>(vertices.size()), 0,
                                  static_cast<int>(indices.size()) / 3);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);
}

void SceneRenderer::drawDepthObject(const Mc3Object& obj, const Mc3Document& doc,
                                    const Matrix& parentWorld, const Matrix& view,
                                    const Matrix& proj, int depth)
{
    if (depth > 16 || !obj.visible) return;

    // Match the main draw's animated transform so the depth field never lags
    // an animation frame. Material/deform color overrides are immaterial here;
    // the deform scale itself still changes geometry and must be retained.
    Mc3Transform transform = obj.transform;
    std::array<float,3> deformScale{1.0f, 1.0f, 1.0f};
    if (!obj.name.empty()) {
        if (auto it = animOverrides_.find(obj.name); it != animOverrides_.end()) {
            const auto& ov = it->second;
            if (ov.visible && !*ov.visible) return;
            if (ov.position) transform.position = *ov.position;
            if (ov.rotation) transform.rotation = *ov.rotation;
            if (ov.scale)    transform.scale = *ov.scale;
            if (ov.deformScale) deformScale = *ov.deformScale;
            else if (obj.deform) deformScale = obj.deform->scale;
        } else if (obj.deform) {
            deformScale = obj.deform->scale;
        }
    } else if (obj.deform) {
        deformScale = obj.deform->scale;
    }
    const Matrix world = objectWorldMatrix(transform) * parentWorld;
    const Matrix deform = Matrix::CreateScale({deformScale[0], deformScale[1], deformScale[2]});

    auto depthStatic = [&](const RenderMesh& mesh, const Matrix& matrix) {
        drawDepthMesh(mesh, matrix, view, proj);
    };
    switch (obj.type) {
    case ObjectType::Box:
    case ObjectType::Cube: {
        const float sx = obj.primitive ? obj.primitive->size[0] : 1.0f;
        const float sy = obj.primitive ? obj.primitive->size[1] : 1.0f;
        const float sz = obj.primitive ? obj.primitive->size[2] : 1.0f;
        depthStatic(unitBox_, deform * Matrix::CreateScale({sx, sy, sz}) * world);
        break;
    }
    case ObjectType::Sphere: {
        const float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        depthStatic(unitSphere_, deform * Matrix::CreateScale({r, r, r}) * world);
        break;
    }
    case ObjectType::Cylinder: {
        const float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        const float h = obj.primitive ? obj.primitive->height : 1.0f;
        const std::string& axis = obj.primitive ? obj.primitive->axis : "y";
        Matrix axisRotation = Matrix::getIdentityProperty();
        if (axis == "x") axisRotation = Matrix::CreateRotationZ(-std::numbers::pi_v<float> / 2.0f);
        else if (axis == "z") axisRotation = Matrix::CreateRotationX(std::numbers::pi_v<float> / 2.0f);
        depthStatic(unitCylinder_, deform * Matrix::CreateScale({r, h, r}) * axisRotation * world);
        break;
    }
    case ObjectType::Cone: {
        const float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        const float h = obj.primitive ? obj.primitive->height : 1.0f;
        depthStatic(unitCone_, deform * Matrix::CreateScale({r, h, r}) * world);
        break;
    }
    case ObjectType::Plane: {
        const float w = obj.primitive ? obj.primitive->size[0] : 1.0f;
        const float d = obj.primitive ? obj.primitive->size[2] : 1.0f;
        depthStatic(unitPlane_, deform * Matrix::CreateScale({w, 1.0f, d}) * world);
        break;
    }
    case ObjectType::Torus: {
        const float major = obj.primitive ? obj.primitive->majorRadius : 0.35f;
        const float minor = obj.primitive ? obj.primitive->minorRadius : 0.15f;
        depthStatic(getOrBuildTorusMesh(32, 16, major, minor), deform * world);
        break;
    }
    case ObjectType::Capsule: {
        const float r = obj.primitive ? obj.primitive->radius : 0.5f;
        const float h = obj.primitive ? obj.primitive->height : 1.0f;
        depthStatic(getOrBuildCapsuleMesh(16, r, h), deform * world);
        break;
    }
    case ObjectType::IcoSphere: {
        const float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        const int segments = obj.primitive ? obj.primitive->segments : 2;
        depthStatic(icoSphereMeshForSegments(segments),
                    deform * Matrix::CreateScale({r, r, r}) * world);
        break;
    }
    case ObjectType::Group:
    case ObjectType::Area:
        for (const auto& child : obj.children)
            if (child) drawDepthObject(*child, doc, world, view, proj, depth + 1);
        break;
    case ObjectType::Union:
    case ObjectType::Intersection:
    case ObjectType::Difference: {
        std::size_t fingerprint = csgSubtreeHashAlg(obj, doc, 0);
        auto mix = [&](float value) { fingerprint = csgHashMixAlg(fingerprint, std::hash<float>{}(value)); };
        mix(parentWorld.M11); mix(parentWorld.M12); mix(parentWorld.M13); mix(parentWorld.M14);
        mix(parentWorld.M21); mix(parentWorld.M22); mix(parentWorld.M23); mix(parentWorld.M24);
        mix(parentWorld.M31); mix(parentWorld.M32); mix(parentWorld.M33); mix(parentWorld.M34);
        mix(parentWorld.M41); mix(parentWorld.M42); mix(parentWorld.M43); mix(parentWorld.M44);
        if (csgMeshCache_.size() > 128) csgMeshCache_.clear();
        auto cached = csgMeshCache_.find(fingerprint);
        if (cached == csgMeshCache_.end()) {
            ++csgCacheEvaluations_;
            csgMeshCache_[fingerprint] = manifoldToRenderMesh(
                device_, buildManifoldTree(obj, doc, parentWorld, 0), obj);
            cached = csgMeshCache_.find(fingerprint);
        }
        if (cached->second.vb) depthStatic(cached->second, Matrix::getIdentityProperty());
        else for (const auto& child : obj.children)
            if (child) drawDepthObject(*child, doc, world, view, proj, depth + 1);
        break;
    }
    case ObjectType::Instance: {
        // The normal scene pass records the selection for this frame. Reuse
        // it so depth/SSAO cannot occlude a metadata-culled instance.
        const auto selectedLod = assetLodSelectionMap_.find(obj.id);
        if (selectedLod != assetLodSelectionMap_.end() && selectedLod->second.culled) return;
        const std::string& definitionKey = selectedLod != assetLodSelectionMap_.end()
            ? selectedLod->second.definitionId : obj.resolvedInstanceDefinitionKey();
        auto it = doc.definitions.find(definitionKey);
        if (it != doc.definitions.end() && it->second)
            drawDepthObject(*it->second, doc, world, view, proj, depth + 1);
        else
            depthStatic(unitBox_, world);
        break;
    }
    case ObjectType::Mesh: {
        const RenderMesh* mesh = nullptr;
        if (obj.meshSource.rfind("embed:", 0) == 0)
            mesh = loadOrGetEmbeddedMesh(doc, obj.meshSource);
        else if (!obj.meshSource.empty() && !doc.sourcePath.empty())
            mesh = loadOrGetMesh((doc.sourcePath / obj.meshSource).string());
        if (mesh) {
            depthStatic(*mesh, deform * world);
            break;
        }
        depthStatic(unitBox_, deform * world);
        break;
    }
    case ObjectType::Disk: {
        const float outer = obj.primitive ? obj.primitive->radius : 0.5f;
        const float inner = obj.primitive ? obj.primitive->minorRadius : 0.0f;
        const int segments = obj.primitive ? obj.primitive->segments : 32;
        drawDiskDynamic(outer, inner, segments, deform * world, view, proj, Color::White, true);
        break;
    }
    case ObjectType::Grid: {
        const float sx = obj.primitive ? obj.primitive->size[0] : 1.0f;
        const float sz = obj.primitive ? obj.primitive->size[2] : 1.0f;
        const int subX = obj.primitive ? obj.primitive->subdivisionsX : 4;
        const int subZ = obj.primitive ? obj.primitive->subdivisionsZ : 4;
        drawGridDynamic(sx, sz, subX, subZ, deform * world, view, proj, Color::White, true);
        break;
    }
    case ObjectType::Extrude:
        if (obj.extrude) drawExtrudeDynamic(*obj.extrude, deform * world, view, proj, Color::White, true);
        else depthStatic(unitBox_, deform * world);
        break;
    default:
        depthStatic(unitBox_, deform * world);
        break;
    }
}

void SceneRenderer::drawDepthPass(const Mc3Document& doc, const Matrix& view, const Matrix& proj)
{
    if (!depthPassAvailable()) return;
    device_.SetDepthTestEnabled(true);
    device_.SetDepthWriteEnabled(true);
    const Matrix identity = coordinateSystemRootMatrix(doc);
    for (const auto& object : doc.objects)
        if (object) drawDepthObject(*object, doc, identity, view, proj);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);
}

Texture2D* SceneRenderer::loadOrGetTexture(const std::string& absPath)
{
    auto it = textureCache_.find(absPath);
    if (it != textureCache_.end()) return &it->second;
    try {
        Texture2D tex(absPath, device_);
        auto [ins, ok] = textureCache_.emplace(absPath, std::move(tex));
        (void)ok;
        return &ins->second;
    } catch (...) {
        // Mark as failed with a sentinel by inserting an empty slot — but Texture2D has no
        // default invalid state. Just return nullptr and try again next frame (cheap miss).
        return nullptr;
    }
}

const RenderMesh* SceneRenderer::loadOrGetMesh(const std::string& absPath)
{
    auto it = meshCache_.find(absPath);
    if (it != meshCache_.end()) return it->second.vb ? &it->second : nullptr;

    RenderMesh loaded = loadObjMesh(device_, absPath);
    if (!loaded.vb) {
        std::cerr << "Warning: failed to load mesh \"" << absPath
                   << "\" — rendering placeholder box\n";
    }
    auto [ins, ok] = meshCache_.emplace(absPath, std::move(loaded));
    (void)ok;
    return ins->second.vb ? &ins->second : nullptr;
}

const RenderMesh* SceneRenderer::loadOrGetEmbeddedMesh(const Mc3Document& doc,
                                                        const std::string& embedReference)
{
    const std::string embedId = embedReference.substr(std::string("embed:").size());
    const auto embedIt = doc.embeds.find(embedId);
    if (embedId.empty() || embedIt == doc.embeds.end()) {
        std::cerr << "Warning: mesh references unknown embed '" << embedId << "'\n";
        return nullptr;
    }
    const auto& embed = embedIt->second;
    // A compact content key avoids retaining a potentially 64 MiB inline
    // base64 payload in meshCache_, while still replacing the viewport mesh
    // when the editor changes the embed.  External meshes retain the same
    // explicit-reload semantics as ordinary OBJ mesh sources.
    const std::string& identity = embed.isInline() ? embed.base64Content : embed.src;
    const std::string cacheKey = "embed:" + doc.sourcePath.generic_string() + ":" + embedId + ":" +
                                 std::to_string(identity.size()) + ":" +
                                 std::to_string(std::hash<std::string>{}(identity));
    auto it = meshCache_.find(cacheKey);
    if (it != meshCache_.end()) return it->second.vb ? &it->second : nullptr;

    RenderMesh loaded = loadEmbeddedGltfMesh(device_, doc.sourcePath, embed);
    if (!loaded.vb)
        std::cerr << "Warning: failed to load embedded mesh '" << embedId
                  << "' — rendering placeholder box\n";
    auto [ins, ok] = meshCache_.emplace(cacheKey, std::move(loaded));
    (void)ok;
    return ins->second.vb ? &ins->second : nullptr;
}

// ---------------------------------------------------------------------------
// AUD-061: per-object-ratio Torus/Capsule mesh cache
// ---------------------------------------------------------------------------
// A single fixed-ratio unit mesh plus a non-uniform affine scale cannot
// correctly reproduce an arbitrary (majorRadius, minorRadius) torus or
// (radius, height) capsule -- see PrimitiveTessellationAlg.hpp's file
// header. So instead of scaling one cached unit mesh (as every other
// primitive type still does), Torus/Capsule build a real mesh at the
// object's actual parameters and cache it keyed on exactly what determines
// its shape: the LOD tier's segment counts plus the actual radii. A static
// scene redrawn every frame hits this cache every time after the first
// draw of each distinct (tier, ratio) combination -- it does NOT
// re-tessellate per frame. Bounded the same way csgMeshCache_ already is
// (clear entirely past 128 entries) so a scene with many differently-sized
// tori/capsules can't grow this unboundedly.
const RenderMesh& SceneRenderer::getOrBuildTorusMesh(int ringSeg, int tubeSeg,
                                                       float majorRadius, float minorRadius)
{
    auto key = std::make_tuple(ringSeg, tubeSeg, majorRadius, minorRadius);
    auto it = torusMeshCache_.find(key);
    if (it != torusMeshCache_.end()) return it->second;

    if (torusMeshCache_.size() > 128) torusMeshCache_.clear();
    RenderMesh mesh;
    buildUnitTorus(ringSeg, tubeSeg, mesh, majorRadius, minorRadius);
    auto [ins, ok] = torusMeshCache_.emplace(key, std::move(mesh));
    (void)ok;
    return ins->second;
}

const RenderMesh& SceneRenderer::getOrBuildCapsuleMesh(int segments, float radius, float height)
{
    auto key = std::make_tuple(segments, radius, height);
    auto it = capsuleMeshCache_.find(key);
    if (it != capsuleMeshCache_.end()) return it->second;

    if (capsuleMeshCache_.size() > 128) capsuleMeshCache_.clear();
    RenderMesh mesh;
    buildUnitCapsule(segments, mesh, radius, height);
    auto [ins, ok] = capsuleMeshCache_.emplace(key, std::move(mesh));
    (void)ok;
    return ins->second;
}

const RenderMesh& SceneRenderer::icoSphereMeshForSegments(int segments) const
{
    const int subdivisions = icoSphereSubdivisionsForSegmentsAlg(segments);
    return unitIcoSpheres_[subdivisions - 1];
}

void SceneRenderer::drawObjectWireframe(const Mc3Object& obj, const Mc3Document& doc,
                                         const Matrix& view, const Matrix& proj, Color color)
{
    // Scale wire shape to object's bounding size
    float sx = 1.0f, sy = 1.0f, sz = 1.0f;
    if (obj.primitive) {
        const auto& p = obj.primitive.value();
        switch (obj.type) {
        case ObjectType::Box:
        case ObjectType::Cube:
            sx = p.size[0]; sy = p.size[1]; sz = p.size[2]; break;
        case ObjectType::Sphere:
            sx = sy = sz = p.radius * 2.0f; break;
        case ObjectType::Cylinder:
            sx = sz = p.radius * 2.0f; sy = p.height; break;
        case ObjectType::Cone:
            sx = sz = p.radius * 2.0f; sy = p.height; break;
        case ObjectType::Plane:
            sx = p.size[0]; sy = 0.01f; sz = p.size[2]; break;
        case ObjectType::Torus:
            sx = sz = (p.majorRadius + p.minorRadius) * 2.0f; sy = p.minorRadius * 2.0f; break;
        case ObjectType::Capsule:
            sx = sz = p.radius * 2.0f; sy = p.height + p.radius * 2.0f; break;
        case ObjectType::Disk:
            sx = sz = p.radius * 2.0f; sy = 0.01f; break;
        case ObjectType::Grid:
            sx = p.size[0]; sy = 0.01f; sz = p.size[2]; break;
        case ObjectType::IcoSphere:
            sx = sy = sz = p.radius * 2.0f; break;
        default: break;
        }
    }

    Matrix scaleM = Matrix::CreateScale({ sx * obj.transform.scale[0],
                                          sy * obj.transform.scale[1],
                                          sz * obj.transform.scale[2] });
    float rx = obj.transform.rotation[0] * (std::numbers::pi_v<float> / 180.0f);
    float ry = obj.transform.rotation[1] * (std::numbers::pi_v<float> / 180.0f);
    float rz = obj.transform.rotation[2] * (std::numbers::pi_v<float> / 180.0f);
    Matrix rotM = Matrix::CreateFromYawPitchRoll(ry, rx, rz);
    Matrix transM = Matrix::getIdentityProperty();
    transM.setTranslationProperty({ obj.transform.position[0],
                                    obj.transform.position[1],
                                    obj.transform.position[2] });
    Matrix wireWorld = scaleM * rotM * transM * coordinateSystemRootMatrix(doc);

    const WireShape* ws = &wireShapeBox_;
    if      (obj.type == ObjectType::Sphere)   ws = &wireShapeSphere_;
    else if (obj.type == ObjectType::Cylinder) ws = &wireShapeCylinder_;
    else if (obj.type == ObjectType::Cone)     ws = &wireShapeCone_;
    else if (obj.type == ObjectType::Plane)    ws = &wireShapePlane_;
    else if (obj.type == ObjectType::Torus)    ws = &wireShapeTorus_;
    else if (obj.type == ObjectType::Capsule)  ws = &wireShapeCapsule_;
    else if (obj.type == ObjectType::Disk)     ws = &wireShapeDisk_;
    else if (obj.type == ObjectType::Grid)      ws = &wireShapeGrid_;
    else if (obj.type == ObjectType::IcoSphere) ws = &wireShapeSphere_;
    drawWireShape(*ws, wireWorld, view, proj, color);
}

void SceneRenderer::drawWireSphereAt(const Vector3& center, float radius, const Mc3Document& doc,
                                      const Matrix& view, const Matrix& proj, Color color)
{
    Matrix wireWorld = Matrix::CreateScale({ radius * 2.0f, radius * 2.0f, radius * 2.0f }) *
                        Matrix::CreateTranslation(center) * coordinateSystemRootMatrix(doc);
    drawWireShape(wireShapeSphere_, wireWorld, view, proj, color);
}

void SceneRenderer::drawObject(const Mc3Object& obj, const Mc3Document& doc,
                                const Matrix& parentWorld,
                                const Matrix& view, const Matrix& proj,
                                const std::vector<const Mc3Object*>& selected,
                                int depth)
{
    if (depth > 16) return; // guard against infinite instance recursion

    // Apply per-object animation overrides (transform + visibility)
    bool effectiveVisible = obj.visible;
    std::optional<Mc3Transform> animTransform;
    if (!obj.name.empty()) {
        auto oit = animOverrides_.find(obj.name);
        if (oit != animOverrides_.end()) {
            const auto& ov = oit->second;
            if (ov.visible) effectiveVisible = *ov.visible;
            if (ov.position || ov.rotation || ov.scale) {
                animTransform = obj.transform;
                if (ov.position) animTransform->position = *ov.position;
                if (ov.rotation) animTransform->rotation = *ov.rotation;
                if (ov.scale)    animTransform->scale    = *ov.scale;
            }
        }
    }
    if (!effectiveVisible) return;

    const Mc3Transform& tf = animTransform.has_value() ? *animTransform : obj.transform;
    Matrix world = objectWorldMatrix(tf) * parentWorld;
    bool  sel    = isSelected(obj, selected);

    // SYS-W14-29: select an authored definition LOD before the regular draw
    // path. This is deliberately here (rather than in the procedural LOD
    // block below): one operation chooses a reusable definition/culls an
    // Instance, while the other chooses sphere/cylinder tessellation.
    std::optional<AssetLodSelection> assetLod;
    if (obj.type == ObjectType::Instance) {
        const float dx = camPosX_ - world.M41;
        const float dy = camPosY_ - world.M42;
        const float dz = camPosZ_ - world.M43;
        const float distanceM = std::sqrt(dx * dx + dy * dy + dz * dz);
        std::optional<AssetLodTier> previous;
        if (const auto prev = assetLodPreviousTiers_.find(obj.id);
            prev != assetLodPreviousTiers_.end())
            previous = prev->second;
        assetLod = resolveAssetLodForInstanceAlg(obj, doc.definitions, distanceM,
                                                  assetLodConfig_, previous);
        if (!obj.id.empty()) {
            assetLodPreviousTiers_[obj.id] = assetLod->tier;
            assetLodSelectionMap_[obj.id] = *assetLod;
        }
        if (assetLod->culled) return;
    }

    // Resolve base color — may be overridden by a material animation channel
    Color color = materialColor(obj.material, doc);
    if (!obj.name.empty()) {
        auto oit = animOverrides_.find(obj.name);
        if (oit != animOverrides_.end() && oit->second.baseColor) {
            const auto& bc = *oit->second.baseColor;
            color = Color(
                static_cast<int>(std::clamp(bc[0], 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(bc[1], 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(bc[2], 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(bc[3], 0.0f, 1.0f) * 255));
        }
    }

    // Deform: geometry-level non-uniform scale — may be overridden by animation
    std::array<float,3> deformScale{1.0f, 1.0f, 1.0f};
    if (!obj.name.empty()) {
        auto oit2 = animOverrides_.find(obj.name);
        if (oit2 != animOverrides_.end() && oit2->second.deformScale)
            deformScale = *oit2->second.deformScale;
        else if (obj.deform)
            deformScale = obj.deform->scale;
    } else if (obj.deform) {
        deformScale = obj.deform->scale;
    }
    Matrix deform = Matrix::CreateScale({deformScale[0], deformScale[1], deformScale[2]});

    // Resolve baseColorTexture → GPU texture (null if none or failed to load)
    Texture2D* tex = nullptr;
    std::optional<SamplerState> svgSampler;
    if (!obj.material.empty()) {
        auto matIt = doc.materials.find(obj.material);
        if (matIt != doc.materials.end() && !matIt->second.baseColorTexture.empty()) {
            auto texIt = doc.textures.find(matIt->second.baseColorTexture);
            if (texIt != doc.textures.end() && !texIt->second.uri.empty()) {
                auto absPath = (doc.sourcePath / texIt->second.uri).string();
                tex = loadOrGetTexture(absPath);
            } else {
                auto svgIt = doc.svgTextures.find(matIt->second.baseColorTexture);
                if (svgIt != doc.svgTextures.end()) {
                    const std::string cacheKey = mc3togltf::svgTextureCacheKey(svgIt->second,
                                                                                 doc.sourcePath);
                    const auto sourceStamp = mc3togltf::svgTextureLastWriteTime(svgIt->second,
                                                                                  doc.sourcePath);
                    auto cached = textureCache_.find(cacheKey);
                    auto cacheState = svgTextureCacheState_.find(cacheKey);
                    const bool sourceChanged = svgIt->second.isExternal() &&
                        cacheState != svgTextureCacheState_.end() &&
                        cacheState->second.lastWriteTime != sourceStamp;
                    if (sourceChanged) {
                        textureCache_.erase(cacheKey);
                        svgTextureCacheState_.erase(cacheKey);
                        svgTextureFailureState_.erase(cacheKey);
                        cached = textureCache_.end();
                    }

                    auto failed = svgTextureFailureState_.find(cacheKey);
                    const bool unchangedFailure = failed != svgTextureFailureState_.end() &&
                        failed->second.lastWriteTime == sourceStamp;
                    if (cached != textureCache_.end()) {
                        tex = &cached->second;
                    } else if (!unchangedFailure) {
                        std::string error;
                        auto raster = mc3togltf::rasterizeSvgTexture(svgIt->second,
                                                                       doc.sourcePath, &error);
                        if (!raster.rgba.empty()) {
                            auto [inserted, ok] = textureCache_.emplace(
                                cacheKey, Texture2D::CreateFromPixels(device_, raster.width,
                                                                       raster.height, raster.rgba));
                            (void)ok;
                            tex = &inserted->second;
                            svgTextureCacheState_[cacheKey] = {sourceStamp};
                            svgTextureFailureState_.erase(cacheKey);
                        } else {
                            std::cerr << "Warning: SVG texture '" << svgIt->first
                                      << "' skipped in viewport: " << error << "\n";
                            svgTextureFailureState_[cacheKey] = {sourceStamp};
                        }
                    }
                    svgSampler = samplerStateForSvg(svgIt->second);
                }
            }
        }
    }

    auto drawAuto = [&](const RenderMesh& mesh, const Matrix& m) {
        drawMeshTextured(mesh, m, view, proj, color, tex,
                         svgSampler ? &*svgSampler : nullptr);
    };

    // G8: pick LOD level based on camera distance to object pivot
    float objX = world.M41, objY = world.M42, objZ = world.M43;
    float dx = camPosX_ - objX, dy = camPosY_ - objY, dz = camPosZ_ - objZ;
    float distSq = dx*dx + dy*dy + dz*dz;
    // 0 = full quality (<10 units), 1 = mid (10..40), 2 = lo (>40)
    int lodLevel = (distSq > 40.0f*40.0f) ? 2 : (distSq > 10.0f*10.0f) ? 1 : 0;
    lodLevelMap_[obj.id] = lodLevel;

    // I3: per-object fog — mix color toward fog color based on camera distance
    if (doc.environment && doc.environment->fog) {
        const auto& f = *doc.environment->fog;
        float dist = std::sqrt(distSq);
        float fogFactor = 0.0f;
        if (f.mode == Mc3::FogMode::Linear) {
            if (f.end > f.start)
                fogFactor = std::clamp((dist - f.start) / (f.end - f.start), 0.0f, 1.0f);
        } else {
            fogFactor = std::clamp(1.0f - std::exp(-f.density * dist), 0.0f, 1.0f);
        }
        if (fogFactor > 0.0f) {
            float r = color.getRProperty() / 255.0f;
            float g = color.getGProperty() / 255.0f;
            float b = color.getBProperty() / 255.0f;
            r += (f.color[0] - r) * fogFactor;
            g += (f.color[1] - g) * fogFactor;
            b += (f.color[2] - b) * fogFactor;
            color = Color(
                static_cast<int>(std::clamp(r, 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(g, 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(b, 0.0f, 1.0f) * 255),
                static_cast<int>(color.getAProperty()));
        }
    }

    auto lodMesh = [&](const RenderMesh& hi, const RenderMesh& mid, const RenderMesh& lo)
        -> const RenderMesh& {
        return lodLevel == 2 ? lo : lodLevel == 1 ? mid : hi;
    };

    switch (obj.type) {
    case ObjectType::Box:
    case ObjectType::Cube: {
        float sx = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float sy = obj.primitive ? obj.primitive->size[1] : 1.0f;
        float sz = obj.primitive ? obj.primitive->size[2] : 1.0f;
        drawAuto(unitBox_, deform * Matrix::CreateScale({sx,sy,sz}) * world);
        break;
    }
    case ObjectType::Sphere: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        drawAuto(lodMesh(unitSphere_, unitSphereL1_, unitSphereL2_),
                 deform * Matrix::CreateScale({r,r,r}) * world);
        break;
    }
    case ObjectType::Cylinder: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        const std::string& cylAxis = obj.primitive ? obj.primitive->axis : "y";
        Matrix axisRot = Matrix::getIdentityProperty();
        if      (cylAxis == "x") axisRot = Matrix::CreateRotationZ(-std::numbers::pi_v<float> / 2.0f);
        else if (cylAxis == "z") axisRot = Matrix::CreateRotationX( std::numbers::pi_v<float> / 2.0f);
        drawAuto(lodMesh(unitCylinder_, unitCylinderL1_, unitCylinderL2_),
                 deform * Matrix::CreateScale({r,h,r}) * axisRot * world);
        break;
    }
    case ObjectType::Cone: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        drawAuto(lodMesh(unitCone_, unitConeL1_, unitConeL2_),
                 deform * Matrix::CreateScale({r,h,r}) * world);
        break;
    }
    case ObjectType::Plane: {
        float w = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float d = obj.primitive ? obj.primitive->size[2] : 1.0f;
        drawAuto(unitPlane_, deform * Matrix::CreateScale({w,1.0f,d}) * world);
        break;
    }
    case ObjectType::Torus: {
        float R = obj.primitive ? obj.primitive->majorRadius : 0.35f;
        float r = obj.primitive ? obj.primitive->minorRadius : 0.15f;
        // AUD-061: a single non-uniform affine scale of one fixed-ratio unit
        // torus (majorRadius=0.35/minorRadius=0.15) can only reproduce a
        // torus whose OWN ratio happens to match 0.35/0.15 -- any other
        // ratio produced an elliptical-cross-section tube. Build a real mesh
        // at this object's actual ratio instead (cached per LOD tier +
        // ratio, see getOrBuildTorusMesh()).
        struct TorusTier { int ring, tube; };
        static const TorusTier torusTiers[3] = {{32,16}, {16,8}, {8,4}};
        const TorusTier& tt = torusTiers[lodLevel];
        drawAuto(getOrBuildTorusMesh(tt.ring, tt.tube, R, r), deform * world);
        break;
    }
    case ObjectType::Capsule: {
        float r = obj.primitive ? obj.primitive->radius : 0.5f;
        float h = obj.primitive ? obj.primitive->height : 1.0f;
        // AUD-061: same structural bug as Torus above -- a single affine
        // scale of one fixed-ratio unit capsule stretched the hemisphere
        // caps into ellipsoids whenever height != 2*radius. Build a real
        // mesh at this object's actual radius/height instead (cached per LOD
        // tier + radius/height, see getOrBuildCapsuleMesh()).
        static const int capsuleTiers[3] = {16, 8, 4};
        drawAuto(getOrBuildCapsuleMesh(capsuleTiers[lodLevel], r, h), deform * world);
        break;
    }
    case ObjectType::Disk: {
        float outerR = obj.primitive ? obj.primitive->radius    : 0.5f;
        float innerR = obj.primitive ? obj.primitive->minorRadius : 0.0f;
        int   segs   = obj.primitive ? obj.primitive->segments  : 32;
        drawDiskDynamic(outerR, innerR, segs, deform * world, view, proj, color);
        break;
    }
    case ObjectType::Grid: {
        float sX  = obj.primitive ? obj.primitive->size[0]      : 1.0f;
        float sZ  = obj.primitive ? obj.primitive->size[2]      : 1.0f;
        int   subX = obj.primitive ? obj.primitive->subdivisionsX : 4;
        int   subZ = obj.primitive ? obj.primitive->subdivisionsZ : 4;
        drawGridDynamic(sX, sZ, subX, subZ, deform * world, view, proj, color);
        break;
    }
    case ObjectType::IcoSphere: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        int segments = obj.primitive ? obj.primitive->segments : 2;
        drawAuto(icoSphereMeshForSegments(segments),
                 deform * Matrix::CreateScale({r,r,r}) * world);
        break;
    }
    case ObjectType::Group:
    case ObjectType::Area:
        for (const auto& child : obj.children)
            drawObject(*child, doc, world, view, proj, selected, depth + 1);
        break;
    case ObjectType::Union:
    case ObjectType::Intersection:
    case ObjectType::Difference: {
        // Content-hash cache: recomputes only when inputs actually change (K1).
        // Folding in parentWorld ensures a moved parent triggers re-evaluation.
        std::size_t fp = csgSubtreeHashAlg(obj, doc, 0);
        auto hfmat = [&](float v) { fp = csgHashMixAlg(fp, std::hash<float>{}(v)); };
        hfmat(parentWorld.M11); hfmat(parentWorld.M12); hfmat(parentWorld.M13); hfmat(parentWorld.M14);
        hfmat(parentWorld.M21); hfmat(parentWorld.M22); hfmat(parentWorld.M23); hfmat(parentWorld.M24);
        hfmat(parentWorld.M31); hfmat(parentWorld.M32); hfmat(parentWorld.M33); hfmat(parentWorld.M34);
        hfmat(parentWorld.M41); hfmat(parentWorld.M42); hfmat(parentWorld.M43); hfmat(parentWorld.M44);
        if (csgMeshCache_.size() > 128) csgMeshCache_.clear();
        auto cit = csgMeshCache_.find(fp);
        if (cit == csgMeshCache_.end()) {
            ++csgCacheEvaluations_;
            manifold::Manifold m = buildManifoldTree(obj, doc, parentWorld, 0);
            csgMeshCache_[fp] = manifoldToRenderMesh(device_, m, obj);
            cit = csgMeshCache_.find(fp);
        }
        csgTriCountMap_[obj.id] = cit->second.primitiveCount;  // K4
        csgWarningMap_[obj.id] = csgSubtreeWarning(obj, doc, 0);   // STAB-0672
        if (cit->second.vb) {
            drawMeshTextured(cit->second, Matrix::getIdentityProperty(), view, proj, color, tex,
                             svgSampler ? &*svgSampler : nullptr);
        } else {
            // Fallback: manifold failed or empty — render children individually
            for (const auto& child : obj.children)
                drawObject(*child, doc, world, view, proj, selected, depth + 1);
        }
        break;
    }
    case ObjectType::Instance: {
        const std::string& definitionKey = assetLod ? assetLod->definitionId
                                                     : obj.resolvedInstanceDefinitionKey();
        auto it = doc.definitions.find(definitionKey);
        if (it != doc.definitions.end() && it->second)
            drawObject(*it->second, doc, world, view, proj, selected, depth + 1);
        else
            drawMesh(unitBox_, world, view, proj, color); // definition not found
        break;
    }
    case ObjectType::Extrude: {
        if (!obj.extrude) { drawMesh(unitBox_, deform * world, view, proj, color); break; }
        drawExtrudeDynamic(obj.extrude.value(), deform * world, view, proj, color);
        break;
    }
    case ObjectType::Mesh: {
        const RenderMesh* loaded = nullptr;
        if (obj.meshSource.rfind("embed:", 0) == 0)
            loaded = loadOrGetEmbeddedMesh(doc, obj.meshSource);
        else if (!obj.meshSource.empty() && !doc.sourcePath.empty())
            loaded = loadOrGetMesh((doc.sourcePath / obj.meshSource).string());
        if (loaded) {
            // Always use the lit VPNT path (proper normals); tex may be nullptr
            drawMeshTextured(*loaded, deform * world, view, proj, color, tex,
                             svgSampler ? &*svgSampler : nullptr);
            break;
        }
        drawMesh(unitBox_, deform * world, view, proj, color);  // fallback placeholder
        break;
    }
    default:
        drawMesh(unitBox_, world, view, proj, color);
        break;
    }

    if (sel) {
        // Draw bright wireframe always-on-top (depth test off) for Blender-like visibility
        device_.SetDepthTestEnabled(false);
        drawObjectWireframe(obj, doc, view, proj, Color(255, 210, 0, 255));
        device_.SetDepthTestEnabled(true);
    }
}

// ---------------------------------------------------------------------------
// Public draw
// ---------------------------------------------------------------------------

// 2026-07-20 audit finding #4: see the declaration comment
// (SceneRenderer.hpp) for the full rationale. Maps up to the first 3
// Directional lights onto BasicEffect's DirectionalLight0-2 and the first
// Ambient light onto AmbientLightColor; falls back to the original fixed
// 3-point default rig (set up once in the constructor via
// EnableDefaultLighting()) when the document has no Directional/Ambient
// lights to represent, preserving today's look for the common case of an
// unlit-by-design scene.
void SceneRenderer::applyDocumentLighting(const Mc3Document& doc)
{
    const Mc3Light* dirLights[3] = {nullptr, nullptr, nullptr};
    int dirCount = 0;
    const Mc3Light* ambientLight = nullptr;

    for (const auto& light : doc.lights) {
        if (light.type == LightType::Directional && dirCount < 3) {
            dirLights[dirCount++] = &light;
        } else if (light.type == LightType::Ambient && !ambientLight) {
            ambientLight = &light;
        }
    }

    if (dirCount == 0 && !ambientLight) {
        // Nothing this API can represent -- keep the default rig
        // (already applied once in the constructor) untouched.
        return;
    }

    DirectionalLight* slots[3] = {
        &effect_->DirectionalLight0, &effect_->DirectionalLight1, &effect_->DirectionalLight2
    };
    for (int i = 0; i < 3; ++i) {
        if (i >= dirCount) {
            slots[i]->setEnabledProperty(false);
            continue;
        }
        const Mc3Light& l = *dirLights[i];
        Vector3 dir = coordinateSystemDirectionToYUp(doc, l.direction);
        // A hostile/malformed document could author direction="0 0 0" --
        // Vector3::Normalize on a zero vector is undefined, so fall back
        // to the struct's own documented default (straight down), matching
        // drawLightGizmos()'s own zero-length guard for the same field.
        dir = (dir.Length() > 1e-5f) ? Vector3::Normalize(dir) : Vector3{0.0f, -1.0f, 0.0f};
        Vector3 col{
            std::clamp(l.color[0] * l.brightness, 0.0f, 1.0f),
            std::clamp(l.color[1] * l.brightness, 0.0f, 1.0f),
            std::clamp(l.color[2] * l.brightness, 0.0f, 1.0f)
        };
        slots[i]->setDirectionProperty(dir);
        slots[i]->setDiffuseColorProperty(col);
        slots[i]->setSpecularColorProperty(col);
        slots[i]->setEnabledProperty(true);
    }

    if (ambientLight) {
        const Mc3Light& l = *ambientLight;
        effect_->setAmbientLightColorProperty(Vector3{
            std::clamp(l.color[0] * l.brightness, 0.0f, 1.0f),
            std::clamp(l.color[1] * l.brightness, 0.0f, 1.0f),
            std::clamp(l.color[2] * l.brightness, 0.0f, 1.0f)
        });
    } else {
        // No authored ambient -- matches the constructor's own brighter-
        // than-XNA-default fill (0.35) so directional-only documents don't
        // go pitch-black on their shadow side.
        effect_->setAmbientLightColorProperty(Vector3{0.35f, 0.35f, 0.35f});
    }
}

void SceneRenderer::draw(const Mc3Document& doc,
                          const Matrix& view, const Matrix& proj,
                          const std::vector<const Mc3Object*>& selected)
{
    device_.SetDepthTestEnabled(true);
    device_.SetDepthWriteEnabled(true);

    applyDocumentLighting(doc);

    // 2026-07-20 audit finding #5: BasicEffect's own GPU fog used to be
    // enabled here too, unconditionally whenever doc.environment->fog
    // existed, ALWAYS using linear start/end regardless of f.mode -- this
    // is now removed. drawObject()'s per-object CPU-side blend (below,
    // "I3: per-object fog") already correctly handles both Linear and
    // Exponential mode and re-colors each object's own draw color before
    // rasterization -- it's a complete, correct implementation on its own.
    // Layering BasicEffect's GPU fog on top double-applied it in Linear
    // mode (visibly over-fogged: the already-fogged color got a second,
    // independent fog blend from the GPU using its own distance metric)
    // and applied a WRONG extra linear contribution in Exponential mode
    // (BasicEffect has no exponential-fog concept at all, so it always
    // used f.start/f.end regardless of f.mode, on top of the already-
    // correct exponential CPU result). Fog is real Mc3Environment content
    // and doesn't need CNA's fixed-function fog at all to work correctly.

    // Extract camera world position from view matrix for LOD (G8)
    camPosX_ = -(view.M41*view.M11 + view.M42*view.M21 + view.M43*view.M31);
    camPosY_ = -(view.M41*view.M12 + view.M42*view.M22 + view.M43*view.M32);
    camPosZ_ = -(view.M41*view.M13 + view.M42*view.M23 + view.M43*view.M33);

    Matrix identity = coordinateSystemRootMatrix(doc);

    for (const auto& obj : doc.objects)
        drawObject(*obj, doc, identity, view, proj, selected);
}

// ---------------------------------------------------------------------------
// Bloom emissive pass (I6)
// ---------------------------------------------------------------------------

void SceneRenderer::drawEmissiveObject(
    const Mc3Object& obj, const Mc3Document& doc,
    const Matrix& parentWorld, const Matrix& view, const Matrix& proj, int depth)
{
    if (depth > 16 || !obj.visible) return;

    const Mc3Transform& tf = obj.transform;
    Matrix world  = objectWorldMatrix(tf) * parentWorld;
    Matrix deform = obj.deform
        ? Matrix::CreateScale({obj.deform->scale[0], obj.deform->scale[1], obj.deform->scale[2]})
        : Matrix::getIdentityProperty();

    // Only render if the material has a non-zero emissive color
    Color eCol(0, 0, 0, 255);
    bool  hasEmissive = false;
    if (!obj.material.empty()) {
        auto matIt = doc.materials.find(obj.material);
        if (matIt != doc.materials.end()) {
            const auto& ec = matIt->second.emissiveColor;
            if (ec[0] > 0.005f || ec[1] > 0.005f || ec[2] > 0.005f) {
                eCol = Color(
                    static_cast<int>(std::min(ec[0], 1.0f) * 255),
                    static_cast<int>(std::min(ec[1], 1.0f) * 255),
                    static_cast<int>(std::min(ec[2], 1.0f) * 255),
                    255);
                hasEmissive = true;
            }
        }
    }

    auto drawE = [&](const RenderMesh& mesh, const Matrix& m) {
        if (hasEmissive) { drawMesh(mesh, m, view, proj, eCol); ++emissiveDrawCount_; }
    };

    bool recurseChildren = true;
    switch (obj.type) {
    case ObjectType::Box:
    case ObjectType::Cube: {
        float sx = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float sy = obj.primitive ? obj.primitive->size[1] : 1.0f;
        float sz = obj.primitive ? obj.primitive->size[2] : 1.0f;
        drawE(unitBox_, deform * Matrix::CreateScale({sx, sy, sz}) * world);
        break;
    }
    case ObjectType::Sphere: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        drawE(unitSphere_, deform * Matrix::CreateScale({r, r, r}) * world);
        break;
    }
    case ObjectType::Cylinder: {
        float r  = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h  = obj.primitive ? obj.primitive->height         : 1.0f;
        const std::string& ax = obj.primitive ? obj.primitive->axis : "y";
        Matrix axisRot = Matrix::getIdentityProperty();
        if      (ax == "x") axisRot = Matrix::CreateRotationZ(-std::numbers::pi_v<float> / 2.0f);
        else if (ax == "z") axisRot = Matrix::CreateRotationX( std::numbers::pi_v<float> / 2.0f);
        drawE(unitCylinder_, deform * Matrix::CreateScale({r, h, r}) * axisRot * world);
        break;
    }
    case ObjectType::Cone: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        drawE(unitCone_, deform * Matrix::CreateScale({r, h, r}) * world);
        break;
    }
    case ObjectType::Plane: {
        float w = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float d = obj.primitive ? obj.primitive->size[2] : 1.0f;
        drawE(unitPlane_, deform * Matrix::CreateScale({w, 1.0f, d}) * world);
        break;
    }
    case ObjectType::Torus: {
        // AUD-061: same fix as drawObject()'s Torus case -- build the real
        // mesh at the object's actual ratio instead of an affine scale of
        // the fixed-ratio unit mesh. Emissive pass always uses the
        // full-quality tier (no camera-distance LOD here, matching the
        // pre-existing behavior of every other shape in this function).
        // Guarded on hasEmissive (unlike the other cases' unconditional
        // drawE() call) so a non-emissive Torus doesn't pay for a cache
        // lookup/build every frame -- referencing unitTorus_ used to be
        // free, but getOrBuildTorusMesh() can be a real tessellation on a
        // cache miss.
        if (hasEmissive) {
            float R = obj.primitive ? obj.primitive->majorRadius : 0.35f;
            float r = obj.primitive ? obj.primitive->minorRadius : 0.15f;
            drawE(getOrBuildTorusMesh(32, 16, R, r), deform * world);
        }
        break;
    }
    case ObjectType::Capsule: {
        // AUD-061: same fix (and same hasEmissive guard) as the Torus case above.
        if (hasEmissive) {
            float r = obj.primitive ? obj.primitive->radius : 0.5f;
            float h = obj.primitive ? obj.primitive->height : 1.0f;
            drawE(getOrBuildCapsuleMesh(16, r, h), deform * world);
        }
        break;
    }
    case ObjectType::IcoSphere: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        int segments = obj.primitive ? obj.primitive->segments : 2;
        drawE(icoSphereMeshForSegments(segments),
              deform * Matrix::CreateScale({r, r, r}) * world);
        break;
    }
    case ObjectType::Group:
    case ObjectType::Area:
        for (const auto& child : obj.children)
            drawEmissiveObject(*child, doc, world, view, proj, depth + 1);
        recurseChildren = false;
        break;
    case ObjectType::Instance: {
        const auto selectedLod = assetLodSelectionMap_.find(obj.id);
        if (selectedLod != assetLodSelectionMap_.end() && selectedLod->second.culled) return;
        const std::string& definitionKey = selectedLod != assetLodSelectionMap_.end()
            ? selectedLod->second.definitionId : obj.resolvedInstanceDefinitionKey();
        auto it = doc.definitions.find(definitionKey);
        if (it != doc.definitions.end() && it->second)
            drawEmissiveObject(*it->second, doc, world, view, proj, depth + 1);
        recurseChildren = false;
        break;
    }
    default:
        break;
    }

    if (recurseChildren) {
        for (const auto& child : obj.children)
            drawEmissiveObject(*child, doc, world, view, proj, depth + 1);
    }
}

void SceneRenderer::drawEmissivePass(
    const Mc3Document& doc, const Matrix& view, const Matrix& proj)
{
    device_.SetDepthTestEnabled(false);
    device_.SetDepthWriteEnabled(false);
    // Flat unlit rendering — emissive color must not be modulated by directional light
    const bool prevLighting = effect_->getLightingEnabledProperty();
    effect_->setLightingEnabledProperty(false);
    Matrix identity = coordinateSystemRootMatrix(doc);
    for (const auto& obj : doc.objects)
        drawEmissiveObject(*obj, doc, identity, view, proj, 0);
    effect_->setLightingEnabledProperty(prevLighting);
    device_.SetDepthTestEnabled(true);
    device_.SetDepthWriteEnabled(true);
}

// ---------------------------------------------------------------------------
// Scene gizmos (lights / cameras)
// ---------------------------------------------------------------------------

void SceneRenderer::drawLineList(
    const std::vector<VertexPositionColor>& verts,
    const Matrix& view, const Matrix& proj)
{
    if (verts.empty() || verts.size() % 2 != 0) return;
    VertexBuffer vb(device_, static_cast<int>(verts.size()));
    vb.SetData(const_cast<VertexPositionColor*>(verts.data()),
               static_cast<int>(verts.size()));

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&vb);
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0,
                           static_cast<int>(verts.size()) / 2);
    device_.SetVertexBuffer(nullptr);
}

void SceneRenderer::drawLightGizmos(
    const Mc3Document& doc,
    const Matrix& view, const Matrix& proj)
{
    const auto& lights = doc.lights;
    std::vector<VertexPositionColor> lines;

    const auto authoredUpArray = coordinateFromYUpAlg(doc.coordinateSystem, {0.0f, 1.0f, 0.0f});
    const Vector3 authoredUp{authoredUpArray[0], authoredUpArray[1], authoredUpArray[2]};

    auto vc = [&](std::array<float,3> p, Color c) -> VertexPositionColor {
        const auto yUp = coordinateToYUpAlg(doc.coordinateSystem, p);
        return { Vector3{yUp[0], yUp[1], yUp[2]}, c };
    };
    auto addLine = [&](std::array<float,3> a, std::array<float,3> b, Color c) {
        lines.push_back(vc(a, c));
        lines.push_back(vc(b, c));
    };

    for (const auto& li : lights) {
        Color col(
            static_cast<int>(std::clamp(li.color[0], 0.0f, 1.0f) * 255),
            static_cast<int>(std::clamp(li.color[1], 0.0f, 1.0f) * 255),
            static_cast<int>(std::clamp(li.color[2], 0.0f, 1.0f) * 255),
            220);

        switch (li.type) {

        case Mc3::LightType::Ambient:
            break; // no position — nothing to draw

        case Mc3::LightType::Directional: {
            // Three parallel arrows from above showing direction
            float dx = li.direction[0], dy = li.direction[1], dz = li.direction[2];
            float len = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (len < 1e-5f) break;
            dx /= len; dy /= len; dz /= len;
            const float shaftLen = 1.5f;
            std::array<float,3> offsets[] = {{-1.0f,0,-1.0f},{0,0,0},{1.0f,0,1.0f}};
            for (auto& o : offsets) {
                // anchor arrows up in the sky at a fixed symbolic position
                std::array<float,3> from = coordinateFromYUpAlg(
                    doc.coordinateSystem, {o[0], 6.0f + o[2], o[0]});
                std::array<float,3> to   = { from[0]+dx*shaftLen,
                                             from[1]+dy*shaftLen,
                                             from[2]+dz*shaftLen };
                addLine(from, to, col);
            }
            break;
        }

        case Mc3::LightType::Point: {
            const auto& p = li.position;
            // Solid sphere gizmo in light color
            Matrix sph = Matrix::CreateScale({0.13f, 0.13f, 0.13f}) *
                         Matrix::CreateTranslation({p[0], p[1], p[2]}) *
                         coordinateSystemRootMatrix(doc);
            drawMesh(unitSphere_, sph, view, proj, col);
            // Diamond ray lines radiating outward
            const float r = 0.28f;
            addLine({p[0]-r,p[1],p[2]}, {p[0]+r,p[1],p[2]}, col);
            addLine({p[0],p[1]-r,p[2]}, {p[0],p[1]+r,p[2]}, col);
            addLine({p[0],p[1],p[2]-r}, {p[0],p[1],p[2]+r}, col);
            addLine({p[0]+r,p[1],p[2]}, {p[0],p[1]+r,p[2]}, col);
            addLine({p[0],p[1]+r,p[2]}, {p[0]-r,p[1],p[2]}, col);
            addLine({p[0]-r,p[1],p[2]}, {p[0],p[1]-r,p[2]}, col);
            addLine({p[0],p[1]-r,p[2]}, {p[0]+r,p[1],p[2]}, col);
            break;
        }

        case Mc3::LightType::Spot: {
            const auto& p = li.position;
            // Small sphere at spotlight apex
            Matrix spotSph = Matrix::CreateScale({0.10f, 0.10f, 0.10f}) *
                             Matrix::CreateTranslation({p[0], p[1], p[2]}) *
                             coordinateSystemRootMatrix(doc);
            drawMesh(unitSphere_, spotSph, view, proj, col);

            float dx = li.direction[0], dy = li.direction[1], dz = li.direction[2];
            float len = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (len < 1e-5f) break;
            dx /= len; dy /= len; dz /= len;

            const float coneLen = 1.0f;
            float halfAngle = li.angle * (std::numbers::pi_v<float> / 180.0f);
            float coneR = coneLen * std::tan(halfAngle);

            // Build a perpendicular basis
            Vector3 dir{dx, dy, dz};
            Vector3 up = (std::abs(Vector3::Dot(dir, authoredUp)) < 0.9f)
                ? authoredUp : Vector3{1,0,0};
            Vector3 right = Vector3::Cross(dir, up);
            right = Vector3::Normalize(right);
            Vector3 up2  = Vector3::Cross(right, dir);

            std::array<float,3> tip = { p[0]+dx*coneLen, p[1]+dy*coneLen, p[2]+dz*coneLen };
            const int segs = 8;
            for (int s = 0; s < segs; ++s) {
                float a0 = s      * 2.0f * std::numbers::pi_v<float> / segs;
                float a1 = (s+1)  * 2.0f * std::numbers::pi_v<float> / segs;
                std::array<float,3> e0 = {
                    tip[0] + (right.X*std::cos(a0) + up2.X*std::sin(a0)) * coneR,
                    tip[1] + (right.Y*std::cos(a0) + up2.Y*std::sin(a0)) * coneR,
                    tip[2] + (right.Z*std::cos(a0) + up2.Z*std::sin(a0)) * coneR };
                std::array<float,3> e1 = {
                    tip[0] + (right.X*std::cos(a1) + up2.X*std::sin(a1)) * coneR,
                    tip[1] + (right.Y*std::cos(a1) + up2.Y*std::sin(a1)) * coneR,
                    tip[2] + (right.Z*std::cos(a1) + up2.Z*std::sin(a1)) * coneR };
                addLine(p, e0, col);   // spoke from apex to ring
                addLine(e0, e1, col);  // ring segment
            }
            break;
        }
        }
    }

    if (!lines.empty())
        drawLineList(lines, view, proj);
}

Vector3 SceneRenderer::cameraForwardFromRotation(const std::array<float,3>& rotationDegrees) {
    constexpr float d = std::numbers::pi_v<float> / 180.0f;
    Matrix rot = Matrix::CreateFromYawPitchRoll(
        rotationDegrees[1] * d, rotationDegrees[0] * d, rotationDegrees[2] * d);
    return Vector3::TransformNormal(Vector3{0.0f, 0.0f, -1.0f}, rot);
}

void SceneRenderer::drawCameraGizmos(
    const Mc3Document& doc,
    const Matrix& view, const Matrix& proj)
{
    const auto& cameras = doc.cameras;
    std::vector<VertexPositionColor> lines;
    Color col(180, 220, 255, 220); // light-blue
    const auto authoredUpArray = coordinateFromYUpAlg(doc.coordinateSystem, {0.0f, 1.0f, 0.0f});
    const Vector3 authoredUp{authoredUpArray[0], authoredUpArray[1], authoredUpArray[2]};

    auto addLine = [&](std::array<float,3> a, std::array<float,3> b) {
        const auto aYUp = coordinateToYUpAlg(doc.coordinateSystem, a);
        const auto bYUp = coordinateToYUpAlg(doc.coordinateSystem, b);
        lines.push_back({ Vector3{aYUp[0],aYUp[1],aYUp[2]}, col });
        lines.push_back({ Vector3{bYUp[0],bYUp[1],bYUp[2]}, col });
    };

    for (const auto& cam : cameras) {
        const auto& p = cam.position;

        // 2026-07-20 audit finding #6: a camera authored with `rotation`
        // instead of `target` (target left at its {0,0,0} default) used
        // to always compute this gizmo's direction from target-minus-
        // position, silently pointing it at the origin.
        float dx, dy, dz;
        if (cam.rotation.has_value()) {
            Vector3 fwd = cameraForwardFromRotation(*cam.rotation);
            dx = fwd.X; dy = fwd.Y; dz = fwd.Z;
        } else {
            dx = cam.target[0]-p[0]; dy = cam.target[1]-p[1]; dz = cam.target[2]-p[2];
        }
        std::array<float,3> t = { p[0]+dx, p[1]+dy, p[2]+dz };

        // Line from position to target
        addLine(p, t);

        // Frustum pyramid: 4 spokes to a small rect in the view direction
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len < 1e-5f) continue;
        dx /= len; dy /= len; dz /= len;

        Vector3 dir{dx, dy, dz};
        Vector3 worldUp = (std::abs(Vector3::Dot(dir, authoredUp)) < 0.9f)
            ? authoredUp : Vector3{1,0,0};
        Vector3 right = Vector3::Normalize(Vector3::Cross(dir, worldUp));
        Vector3 up    = Vector3::Cross(right, dir);

        const float d = 0.6f;   // distance to near-plane square
        const float hw = 0.3f;  // half-width of the square

        std::array<float,4> cx = { hw, hw, -hw, -hw };
        std::array<float,4> cy = { hw, -hw, -hw, hw };
        std::array<std::array<float,3>, 4> corners;
        for (int i = 0; i < 4; ++i) {
            corners[i] = {
                p[0] + dx*d + right.X*cx[i] + up.X*cy[i],
                p[1] + dy*d + right.Y*cx[i] + up.Y*cy[i],
                p[2] + dz*d + right.Z*cx[i] + up.Z*cy[i] };
        }
        for (int i = 0; i < 4; ++i) {
            addLine(p, corners[i]);
            addLine(corners[i], corners[(i+1)%4]);
        }
    }

    if (!lines.empty())
        drawLineList(lines, view, proj);
}

// ---------------------------------------------------------------------------
// CSG gizmos — colored box outlines for Union / Difference / Intersection
// ---------------------------------------------------------------------------

void SceneRenderer::drawCsgGizmos(const Mc3::Mc3Document& doc,
                                   const Matrix& view, const Matrix& proj)
{
    using namespace Mc3;

    static const float P[][3] = {
        {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f},{0.5f,-0.5f, 0.5f},{0.5f,0.5f, 0.5f},{-0.5f,0.5f, 0.5f},
    };
    static const int E[][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };

    std::vector<VertexPositionColor> lines;

    std::function<void(const Mc3Object&, const Matrix&, int)> visit;
    visit = [&](const Mc3Object& obj, const Matrix& parentWorld, int depth) {
        if (depth > 16) return;  // SYS-W1-07: guard against a cyclic children graph
        Matrix world = objectWorldMatrix(obj.transform) * parentWorld;

        if (obj.type == ObjectType::Union ||
            obj.type == ObjectType::Difference ||
            obj.type == ObjectType::Intersection)
        {
            Color c = (obj.type == ObjectType::Union)        ? Color( 60, 220,  60, 200)
                    : (obj.type == ObjectType::Difference)   ? Color(220,  60,  60, 200)
                    :                                          Color( 60, 120, 220, 200);

            for (auto& e : E) {
                Vector3 a = Vector3::Transform(Vector3{P[e[0]][0],P[e[0]][1],P[e[0]][2]}, world);
                Vector3 b = Vector3::Transform(Vector3{P[e[1]][0],P[e[1]][1],P[e[1]][2]}, world);
                lines.push_back({a, c});
                lines.push_back({b, c});
            }
        }

        for (const auto& child : obj.children)
            visit(*child, world, depth + 1);
    };

    Matrix identity = coordinateSystemRootMatrix(doc);
    for (const auto& obj : doc.objects)
        visit(*obj, identity, 0);

    if (!lines.empty())
        drawLineList(lines, view, proj);
}



Matrix SceneRenderer::computeObjectWorldMatrix(const Mc3Object& target,
                                               const Mc3Document& doc) const {
    Matrix result = Matrix::getIdentityProperty();
    std::function<bool(const std::vector<std::shared_ptr<Mc3Object>>&, const Matrix&, int)> find;
    find = [&](const std::vector<std::shared_ptr<Mc3Object>>& list, const Matrix& parent, int depth) -> bool {
        if (depth > 16) return false;  // SYS-W1-07: guard against a cyclic children graph
        for (const auto& obj : list) {
            Matrix world = objectWorldMatrix(obj->transform) * parent;
            if (obj.get() == &target) { result = world; return true; }
            if (!obj->children.empty() && find(obj->children, world, depth + 1)) return true;
        }
        return false;
    };
    find(doc.objects, coordinateSystemRootMatrix(doc), 0);
    return result;
}

void SceneRenderer::objectPolyStats(const Mc3::Mc3Object& obj,
                                    int& verts, int& tris) const {
    verts = 0; tris = 0;
    std::function<void(const Mc3::Mc3Object&, int)> walk;
    walk = [&](const Mc3::Mc3Object& o, int depth) {
        if (depth > 16) return;  // SYS-W1-07: guard against a cyclic children graph
        const RenderMesh* rm = nullptr;
        switch (o.type) {
        case Mc3::ObjectType::Box:
        case Mc3::ObjectType::Cube:      rm = &unitBox_;       break;
        case Mc3::ObjectType::Sphere:    rm = &unitSphere_;    break;
        case Mc3::ObjectType::Cylinder:  rm = &unitCylinder_;  break;
        case Mc3::ObjectType::Cone:      rm = &unitCone_;      break;
        case Mc3::ObjectType::Plane:     rm = &unitPlane_;     break;
        case Mc3::ObjectType::Torus:     rm = &unitTorus_;     break;
        case Mc3::ObjectType::Capsule:   rm = &unitCapsule_;   break;
        case Mc3::ObjectType::IcoSphere:
            rm = &icoSphereMeshForSegments(o.primitive ? o.primitive->segments : 2);
            break;
        case Mc3::ObjectType::Disk: {
            int segs = o.primitive ? o.primitive->segments : 32;
            verts += segs * 2; tris += segs * 2; break;
        }
        case Mc3::ObjectType::Grid: {
            int subX = o.primitive ? o.primitive->subdivisionsX : 4;
            int subZ = o.primitive ? o.primitive->subdivisionsZ : 4;
            verts += (subX + 1) * (subZ + 1);
            tris  += subX * subZ * 2; break;
        }
        case Mc3::ObjectType::Mesh: {
            if (!o.meshSource.empty()) {
                auto it = meshCache_.find(o.meshSource);
                if (it != meshCache_.end()) rm = &it->second;
            }
            break;
        }
        default: break;
        }
        if (rm) {
            verts += static_cast<int>(rm->positions.size());
            tris  += rm->primitiveCount;
        }
        for (const auto& child : o.children) if (child) walk(*child, depth + 1);
    };
    walk(obj, 0);
}

void SceneRenderer::scenePolyStats(const Mc3::Mc3Document& doc,
                                    int& totalVerts, int& totalTris) const {
    totalVerts = 0;
    totalTris  = 0;

    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&, int)> walk;
    walk = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list, int depth) {
        if (depth > 16) return;  // SYS-W1-07: guard against a cyclic children graph
        for (const auto& obj : list) {
            if (!obj->visible) { walk(obj->children, depth + 1); continue; }
            int v = 0, t = 0;
            objectPolyStats(*obj, v, t);
            totalVerts += v;
            totalTris  += t;
        }
    };
    walk(doc.objects, 0);
}

// ---------------------------------------------------------------------------
// K3: Export CSG result as OBJ
// ---------------------------------------------------------------------------
bool SceneRenderer::exportCsgMesh(const Mc3Object& obj, const Mc3Document& doc,
                                   const std::string& path, std::string& err) {
    try {
        manifold::Manifold m = buildManifoldTree(obj, doc, Matrix::getIdentityProperty(), 0);
        if (m.IsEmpty()) { err = "CSG result is empty"; return false; }
        mc3togltf::MeshData mesh = mc3togltf::meshDataFromCsgManifold(m);
        if (mesh.empty() || mesh.positions.size() % 3 != 0 ||
            mesh.normals.size() != mesh.positions.size()) {
            err = "Mesh has no geometry";
            return false;
        }
        applyCsgUvMapping(mesh, obj);
        const int nVerts = mesh.vertexCount();
        const int nTris = static_cast<int>(mesh.indices.size() / 3);

        std::ofstream f(path);
        if (!f) { err = "Cannot open file for writing: " + path; return false; }

        f << "# CSG export from MeshCraft\n";
        f << "# Vertices: " << nVerts << "  Triangles: " << nTris << "\n";
        f << std::fixed;

        for (int i = 0; i < nVerts; ++i) {
            float x = mesh.positions[i * 3 + 0];
            float y = mesh.positions[i * 3 + 1];
            float z = mesh.positions[i * 3 + 2];
            f << "v " << x << " " << y << " " << z << "\n";
        }
        for (int i = 0; i < nVerts; ++i) {
            f << "vt " << mesh.texcoords[i * 2] << " " << mesh.texcoords[i * 2 + 1] << "\n";
            f << "vn " << mesh.normals[i * 3] << " " << mesh.normals[i * 3 + 1]
              << " " << mesh.normals[i * 3 + 2] << "\n";
        }

        for (int ti = 0; ti < nTris; ++ti) {
            uint32_t i0 = mesh.indices[ti*3+0] + 1;  // OBJ is 1-indexed
            uint32_t i1 = mesh.indices[ti*3+1] + 1;
            uint32_t i2 = mesh.indices[ti*3+2] + 1;
            f << "f " << i0 << "/" << i0 << "/" << i0
              << " " << i1 << "/" << i1 << "/" << i1
              << " " << i2 << "/" << i2 << "/" << i2 << "\n";
        }

        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

} // namespace MeshCraft::Renderer
