#include "MeshCraft/Renderer/SceneRenderer.hpp"

#include <Microsoft/Xna/Framework/Graphics/BufferUsage.hpp>
#include <Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp>
#include <Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp>
#include <Microsoft/Xna/Framework/MathHelper.hpp>
#include <Microsoft/Xna/Framework/Vector2.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <numbers>
#include <optional>
#include <vector>

#include <manifold/manifold.h>
#include <tiny_obj_loader.h>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Renderer;

// ---------------------------------------------------------------------------
// CSG helpers (file scope)
// ---------------------------------------------------------------------------

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

// Build a manifold::Manifold for `obj` and its subtree.
// parentToWorld: cumulative transform from the CSG root's parent space to world.
// Leaf primitives are placed in world space; result drawn with identity matrix.
static manifold::Manifold buildManifoldTree(
    const Mc3Object& obj, const Mc3Document& doc,
    const Matrix& parentToWorld, int depth)
{
    using namespace manifold;
    if (depth > 12 || !obj.visible) return Manifold{};

    constexpr int SEG = 24;
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
        auto it = doc.definitions.find(obj.definition);
        if (it != doc.definitions.end() && it->second)
            return buildManifoldTree(*it->second, doc, objWorld, depth + 1);
        return Manifold{};
    }
    default:
        return Manifold{};  // Extrude/Mesh: not supported in CSG boolean
    }
}

// Convert a manifold::Manifold to a RenderMesh (VertexPositionColor, world-space)
static RenderMesh manifoldToRenderMesh(GraphicsDevice& device, const manifold::Manifold& m) {
    RenderMesh mesh;
    if (m.IsEmpty()) return mesh;

    manifold::MeshGL gl = m.GetMeshGL();
    int nVerts = static_cast<int>(gl.vertProperties.size()) / static_cast<int>(gl.numProp);
    int nTris  = static_cast<int>(gl.triVerts.size()) / 3;
    if (nVerts <= 0 || nTris <= 0) return mesh;

    Color c(180, 180, 180, 255);
    std::vector<VertexPositionColor> verts(nVerts);
    mesh.positions.reserve(nVerts);
    for (int i = 0; i < nVerts; ++i) {
        float x = gl.vertProperties[i * gl.numProp + 0];
        float y = gl.vertProperties[i * gl.numProp + 1];
        float z = gl.vertProperties[i * gl.numProp + 2];
        verts[i] = {Vector3{x, y, z}, c};
        mesh.positions.push_back({x, y, z});
    }

    // Use 32-bit indices to handle large manifold output
    std::vector<uint32_t> idx32(gl.triVerts.begin(), gl.triVerts.end());

    mesh.vb = std::make_unique<VertexBuffer>(device, nVerts);
    mesh.vb->SetData(verts.data(), nVerts);
    mesh.ib = std::make_unique<IndexBuffer>(device, IndexElementSize::ThirtyTwoBits, nTris * 3, BufferUsage::None);
    mesh.ib->SetData(idx32.data(), nTris * 3);
    mesh.primitiveCount = nTris;
    return mesh;
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

namespace MeshCraft::Renderer {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SceneRenderer::SceneRenderer(GraphicsDevice& device)
    : device_(device)
{
    effect_ = std::make_unique<BasicEffect>(device_);
    effect_->VertexColorEnabled = true;

    buildUnitBox();
    buildUnitSphere(12);
    buildUnitCylinder(12);
    buildUnitCone(12);
    buildUnitPlane();
    buildUnitTorus(32, 16);
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

Color SceneRenderer::materialColor(const std::string& matId, const Mc3Document& doc) const {
    if (!matId.empty()) {
        auto it = doc.materials.find(matId);
        if (it != doc.materials.end()) {
            const auto& bc = it->second.baseColor;
            return Color(
                static_cast<int>(std::clamp(bc[0], 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(bc[1], 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(bc[2], 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(bc[3], 0.0f, 1.0f) * 255));
        }
    }
    return Color(180, 180, 180, 255);
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
                                      Color color, Texture2D* tex)
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

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(mesh.texVB.get());
    device_.SetIndexBuffer(mesh.texIB.get());
    device_.DrawIndexedPrimitives(
        Graphics::PrimitiveType::TriangleList,
        0, 0, n, 0, mesh.texPrimitiveCount);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);

    effect_->setTextureProperty(nullptr);
    effect_->setTextureEnabledProperty(false);
    effect_->VertexColorEnabled = true;
    effect_->setDiffuseColorProperty(Vector3{1,1,1});
    effect_->setAlphaProperty(1.0f);
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
    auto [ins, ok] = meshCache_.emplace(absPath, std::move(loaded));
    (void)ok;
    return ins->second.vb ? &ins->second : nullptr;
}

void SceneRenderer::drawObjectWireframe(const Mc3Object& obj,
                                         const Matrix& view, const Matrix& proj, Color color)
{
    (void)color;
    Matrix world = objectWorldMatrix(obj.transform);

    // Scale wire box to object's bounding size
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
    Matrix wireWorld = scaleM * rotM * transM;

    effect_->World      = wireWorld;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty()) {
        pass.Apply();
    }

    device_.SetVertexBuffer(wireBoxVB_.get());
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, wireBoxLineCount_);
    device_.SetVertexBuffer(nullptr);
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
    if (!obj.material.empty()) {
        auto matIt = doc.materials.find(obj.material);
        if (matIt != doc.materials.end() && !matIt->second.baseColorTexture.empty()) {
            auto texIt = doc.textures.find(matIt->second.baseColorTexture);
            if (texIt != doc.textures.end() && !texIt->second.uri.empty()) {
                auto absPath = (doc.sourcePath / texIt->second.uri).string();
                tex = loadOrGetTexture(absPath);
            }
        }
    }

    auto drawAuto = [&](const RenderMesh& mesh, const Matrix& m) {
        if (tex) drawMeshTextured(mesh, m, view, proj, color, tex);
        else     drawMesh(mesh, m, view, proj, color);
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
        drawAuto(unitSphere_, deform * Matrix::CreateScale({r,r,r}) * world);
        break;
    }
    case ObjectType::Cylinder: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        drawAuto(unitCylinder_, deform * Matrix::CreateScale({r,h,r}) * world);
        break;
    }
    case ObjectType::Cone: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        drawAuto(unitCone_, deform * Matrix::CreateScale({r,h,r}) * world);
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
        // Scale unit torus (built with R=0.35, r=0.15) to match parameters
        float sxz = R / 0.35f;
        float sy  = r / 0.15f;
        drawAuto(unitTorus_, deform * Matrix::CreateScale({sxz, sy, sxz}) * world);
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
        // Look up or evaluate the CSG boolean mesh (world-space, drawn with identity)
        auto cit = csgMeshCache_.find(&obj);
        if (cit == csgMeshCache_.end()) {
            manifold::Manifold m = buildManifoldTree(obj, doc, parentWorld, 0);
            csgMeshCache_[&obj] = manifoldToRenderMesh(device_, m);
            cit = csgMeshCache_.find(&obj);
        }
        if (cit->second.vb) {
            drawMesh(cit->second, Matrix::getIdentityProperty(), view, proj, color);
        } else {
            // Fallback: manifold failed or empty — render children individually
            for (const auto& child : obj.children)
                drawObject(*child, doc, world, view, proj, selected, depth + 1);
        }
        break;
    }
    case ObjectType::Instance: {
        auto it = doc.definitions.find(obj.definition);
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
        if (!obj.meshSource.empty() && !doc.sourcePath.empty()) {
            auto absPath = (doc.sourcePath / obj.meshSource).string();
            const RenderMesh* loaded = loadOrGetMesh(absPath);
            if (loaded) {
                // Always use the lit VPNT path (proper normals); tex may be nullptr
                drawMeshTextured(*loaded, deform * world, view, proj, color, tex);
                break;
            }
        }
        drawMesh(unitBox_, deform * world, view, proj, color);  // fallback placeholder
        break;
    }
    default:
        drawMesh(unitBox_, world, view, proj, color);
        break;
    }

    if (sel) {
        // Draw orange wireframe over selected object
        drawObjectWireframe(obj, view, proj, Color(255,165,0,255));
    }
}

// ---------------------------------------------------------------------------
// Public draw
// ---------------------------------------------------------------------------

void SceneRenderer::draw(const Mc3Document& doc,
                          const Matrix& view, const Matrix& proj,
                          const std::vector<const Mc3Object*>& selected)
{
    device_.SetDepthTestEnabled(true);
    device_.SetDepthWriteEnabled(true);

    Matrix identity = Matrix::getIdentityProperty();

    for (const auto& obj : doc.objects)
        drawObject(*obj, doc, identity, view, proj, selected);
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
    const std::vector<Mc3::Mc3Light>& lights,
    const Matrix& view, const Matrix& proj)
{
    std::vector<VertexPositionColor> lines;

    auto vc = [](std::array<float,3> p, Color c) -> VertexPositionColor {
        return { Vector3{p[0], p[1], p[2]}, c };
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
                std::array<float,3> from = { o[0], 6.0f + o[2], o[0] };
                std::array<float,3> to   = { from[0]+dx*shaftLen,
                                             from[1]+dy*shaftLen,
                                             from[2]+dz*shaftLen };
                addLine(from, to, col);
            }
            break;
        }

        case Mc3::LightType::Point: {
            const auto& p = li.position;
            const float r = 0.25f;
            addLine({p[0]-r,p[1],p[2]}, {p[0]+r,p[1],p[2]}, col);
            addLine({p[0],p[1]-r,p[2]}, {p[0],p[1]+r,p[2]}, col);
            addLine({p[0],p[1],p[2]-r}, {p[0],p[1],p[2]+r}, col);
            // diamond outline
            addLine({p[0]+r,p[1],p[2]}, {p[0],p[1]+r,p[2]}, col);
            addLine({p[0],p[1]+r,p[2]}, {p[0]-r,p[1],p[2]}, col);
            addLine({p[0]-r,p[1],p[2]}, {p[0],p[1]-r,p[2]}, col);
            addLine({p[0],p[1]-r,p[2]}, {p[0]+r,p[1],p[2]}, col);
            break;
        }

        case Mc3::LightType::Spot: {
            const auto& p = li.position;
            float dx = li.direction[0], dy = li.direction[1], dz = li.direction[2];
            float len = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (len < 1e-5f) break;
            dx /= len; dy /= len; dz /= len;

            const float coneLen = 1.0f;
            float halfAngle = li.angle * (std::numbers::pi_v<float> / 180.0f);
            float coneR = coneLen * std::tan(halfAngle);

            // Build a perpendicular basis
            Vector3 dir{dx, dy, dz};
            Vector3 up = (std::abs(dy) < 0.9f) ? Vector3{0,1,0} : Vector3{1,0,0};
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

void SceneRenderer::drawCameraGizmos(
    const std::vector<Mc3::Mc3Camera>& cameras,
    const Matrix& view, const Matrix& proj)
{
    std::vector<VertexPositionColor> lines;
    Color col(180, 220, 255, 220); // light-blue

    auto addLine = [&](std::array<float,3> a, std::array<float,3> b) {
        lines.push_back({ Vector3{a[0],a[1],a[2]}, col });
        lines.push_back({ Vector3{b[0],b[1],b[2]}, col });
    };

    for (const auto& cam : cameras) {
        const auto& p = cam.position;
        const auto& t = cam.target;

        // Line from position to target
        addLine(p, t);

        // Frustum pyramid: 4 spokes to a small rect in the view direction
        float dx = t[0]-p[0], dy = t[1]-p[1], dz = t[2]-p[2];
        float len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len < 1e-5f) continue;
        dx /= len; dy /= len; dz /= len;

        Vector3 dir{dx, dy, dz};
        Vector3 worldUp = (std::abs(dy) < 0.9f) ? Vector3{0,1,0} : Vector3{1,0,0};
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

    std::function<void(const Mc3Object&, const Matrix&)> visit;
    visit = [&](const Mc3Object& obj, const Matrix& parentWorld) {
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
            visit(*child, world);
    };

    Matrix identity = Matrix::getIdentityProperty();
    for (const auto& obj : doc.objects)
        visit(*obj, identity);

    if (!lines.empty())
        drawLineList(lines, view, proj);
}



} // namespace MeshCraft::Renderer
