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
    buildWireBox();
    buildWireShapes(16);
}

// ---------------------------------------------------------------------------
// Shape builders
// ---------------------------------------------------------------------------

static uint16_t ui16(int v) { return static_cast<uint16_t>(v); }

static void storePositions(const std::vector<VertexPositionColor>& verts, RenderMesh& mesh) {
    mesh.positions.reserve(verts.size());
    for (const auto& v : verts) mesh.positions.push_back(v.Position);
}

void SceneRenderer::buildUnitBox() {
    Color c(200, 200, 200, 255);
    // 8 corners of a unit cube centered at origin
    static const float P[][3] = {
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
        { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
        { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    };
    std::vector<VertexPositionColor> verts;
    for (auto& p : P) verts.push_back({ Vector3{p[0],p[1],p[2]}, c });

    // 12 triangles (6 faces, 2 tri each)
    static const uint16_t IDX[] = {
        0,2,1, 0,3,2,  // -Z
        4,5,6, 4,6,7,  // +Z
        0,1,5, 0,5,4,  // -Y
        2,3,7, 2,7,6,  // +Y
        0,4,7, 0,7,3,  // -X
        1,2,6, 1,6,5,  // +X
    };
    unitBox_.vb = std::make_unique<VertexBuffer>(device_, 8);
    unitBox_.vb->SetData(verts.data(), 8);
    unitBox_.ib = std::make_unique<IndexBuffer>(device_, 36);
    unitBox_.ib->SetData(IDX, 36);
    unitBox_.primitiveCount = 12;
    storePositions(verts, unitBox_);

    // VPNT: 4 verts per face (unshared) with normals and UVs
    struct Face { Vector3 n; float p[4][3]; };
    static const Face FACES[6] = {
        {{ 0, 0,-1}, {{-.5f,-.5f,-.5f},{ .5f,-.5f,-.5f},{ .5f, .5f,-.5f},{-.5f, .5f,-.5f}}},
        {{ 0, 0, 1}, {{ .5f,-.5f, .5f},{-.5f,-.5f, .5f},{-.5f, .5f, .5f},{ .5f, .5f, .5f}}},
        {{ 0,-1, 0}, {{-.5f,-.5f, .5f},{ .5f,-.5f, .5f},{ .5f,-.5f,-.5f},{-.5f,-.5f,-.5f}}},
        {{ 0, 1, 0}, {{-.5f, .5f,-.5f},{ .5f, .5f,-.5f},{ .5f, .5f, .5f},{-.5f, .5f, .5f}}},
        {{-1, 0, 0}, {{-.5f,-.5f, .5f},{-.5f,-.5f,-.5f},{-.5f, .5f,-.5f},{-.5f, .5f, .5f}}},
        {{ 1, 0, 0}, {{ .5f,-.5f,-.5f},{ .5f,-.5f, .5f},{ .5f, .5f, .5f},{ .5f, .5f,-.5f}}},
    };
    static const Vector2 UVS[4] = {{0,1},{1,1},{1,0},{0,0}};
    std::vector<VertexPositionNormalTexture> tverts;
    std::vector<uint16_t> tidx;
    tverts.reserve(24); tidx.reserve(36);
    for (int f = 0; f < 6; ++f) {
        int base = f * 4;
        for (int v = 0; v < 4; ++v)
            tverts.push_back({Vector3{FACES[f].p[v][0],FACES[f].p[v][1],FACES[f].p[v][2]}, FACES[f].n, UVS[v]});
        tidx.push_back(ui16(base)); tidx.push_back(ui16(base+2)); tidx.push_back(ui16(base+1));
        tidx.push_back(ui16(base)); tidx.push_back(ui16(base+3)); tidx.push_back(ui16(base+2));
    }
    unitBox_.texVB = std::make_unique<VertexBuffer>(device_, 24);
    unitBox_.texVB->SetData(tverts.data(), 24);
    unitBox_.texIB = std::make_unique<IndexBuffer>(device_, 36);
    unitBox_.texIB->SetData(tidx.data(), 36);
    unitBox_.texPrimitiveCount = 12;
}

void SceneRenderer::buildUnitSphere(int segments) {
    Color c(200, 200, 200, 255);
    std::vector<VertexPositionColor> verts;
    std::vector<uint16_t> indices;

    int rings   = segments / 2;
    int sectors = segments;

    for (int r = 0; r <= rings; ++r) {
        float phi = std::numbers::pi_v<float> * r / rings;
        for (int s = 0; s <= sectors; ++s) {
            float theta = 2.0f * std::numbers::pi_v<float> * s / sectors;
            float x = std::sin(phi) * std::cos(theta) * 0.5f;
            float y = std::cos(phi) * 0.5f;
            float z = std::sin(phi) * std::sin(theta) * 0.5f;
            verts.push_back({ Vector3{x,y,z}, c });
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            int a = r * (sectors+1) + s;
            int b = a + 1;
            int c2 = (r+1) * (sectors+1) + s;
            int d  = c2 + 1;
            indices.push_back(ui16(a)); indices.push_back(ui16(c2)); indices.push_back(ui16(b));
            indices.push_back(ui16(b)); indices.push_back(ui16(c2)); indices.push_back(ui16(d));
        }
    }
    unitSphere_.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    unitSphere_.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    unitSphere_.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    unitSphere_.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    unitSphere_.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, unitSphere_);

    // VPNT: same topology, normals = pos*2 (unit sphere radius 0.5), UVs from ring/sector
    {
        std::vector<VertexPositionNormalTexture> tv;
        tv.reserve(verts.size());
        for (int r = 0; r <= rings; ++r) {
            float phi = std::numbers::pi_v<float> * r / rings;
            for (int s = 0; s <= sectors; ++s) {
                float theta = 2.0f * std::numbers::pi_v<float> * s / sectors;
                float x = std::sin(phi) * std::cos(theta) * 0.5f;
                float y = std::cos(phi) * 0.5f;
                float z = std::sin(phi) * std::sin(theta) * 0.5f;
                Vector3 pos{x, y, z};
                Vector3 norm{x * 2.0f, y * 2.0f, z * 2.0f};
                Vector2 uv{static_cast<float>(s) / sectors, static_cast<float>(r) / rings};
                tv.push_back({pos, norm, uv});
            }
        }
        unitSphere_.texVB = std::make_unique<VertexBuffer>(device_, static_cast<int>(tv.size()));
        unitSphere_.texVB->SetData(tv.data(), static_cast<int>(tv.size()));
        unitSphere_.texIB = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
        unitSphere_.texIB->SetData(indices.data(), static_cast<int>(indices.size()));
        unitSphere_.texPrimitiveCount = unitSphere_.primitiveCount;
    }
}

void SceneRenderer::buildUnitCylinder(int segments) {
    Color c(200, 200, 200, 255);
    std::vector<VertexPositionColor> verts;
    std::vector<uint16_t> indices;

    // Top and bottom ring + center caps
    int base = 0;
    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * std::numbers::pi_v<float> * i / segments;
        float x = 0.5f * std::cos(a), z = 0.5f * std::sin(a);
        verts.push_back({ Vector3{x, -0.5f, z}, c });  // bottom ring
    }
    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * std::numbers::pi_v<float> * i / segments;
        float x = 0.5f * std::cos(a), z = 0.5f * std::sin(a);
        verts.push_back({ Vector3{x, 0.5f, z}, c });   // top ring
    }
    int botCenter = static_cast<int>(verts.size());
    verts.push_back({ Vector3{0,-0.5f,0}, c });
    int topCenter = static_cast<int>(verts.size());
    verts.push_back({ Vector3{0, 0.5f,0}, c });

    for (int i = 0; i < segments; ++i) {
        int j = (i+1) % segments;
        // Side quad
        indices.push_back(ui16(i)); indices.push_back(ui16(i+segments)); indices.push_back(ui16(j+segments));
        indices.push_back(ui16(i)); indices.push_back(ui16(j+segments)); indices.push_back(ui16(j));
        // Bottom cap
        indices.push_back(ui16(botCenter)); indices.push_back(ui16(j)); indices.push_back(ui16(i));
        // Top cap
        indices.push_back(ui16(topCenter)); indices.push_back(ui16(i+segments)); indices.push_back(ui16(j+segments));
    }
    unitCylinder_.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    unitCylinder_.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    unitCylinder_.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    unitCylinder_.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    unitCylinder_.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, unitCylinder_);

    // VPNT: side with outward normals + flat cap normals
    {
        std::vector<VertexPositionNormalTexture> tv;
        std::vector<uint16_t> ti;
        // Side: segments+1 columns × 2 rows (seam duplicated for UV)
        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * std::numbers::pi_v<float> * i / segments;
            float cx = std::cos(a), cz = std::sin(a);
            float u = static_cast<float>(i) / segments;
            Vector3 n{cx, 0.0f, cz};
            tv.push_back({Vector3{0.5f*cx, -0.5f, 0.5f*cz}, n, Vector2{u, 1.0f}});
            tv.push_back({Vector3{0.5f*cx,  0.5f, 0.5f*cz}, n, Vector2{u, 0.0f}});
        }
        for (int i = 0; i < segments; ++i) {
            int b = i * 2;
            ti.push_back(ui16(b));   ti.push_back(ui16(b+2)); ti.push_back(ui16(b+1));
            ti.push_back(ui16(b+1)); ti.push_back(ui16(b+2)); ti.push_back(ui16(b+3));
        }
        // Bottom cap
        int capBase = static_cast<int>(tv.size());
        Vector3 botN{0,-1,0};
        tv.push_back({Vector3{0,-0.5f,0}, botN, Vector2{0.5f,0.5f}});
        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * std::numbers::pi_v<float> * i / segments;
            float cx = std::cos(a), cz = std::sin(a);
            tv.push_back({Vector3{0.5f*cx,-0.5f,0.5f*cz}, botN,
                          Vector2{0.5f+0.5f*cx, 0.5f+0.5f*cz}});
        }
        for (int i = 0; i < segments; ++i) {
            ti.push_back(ui16(capBase));
            ti.push_back(ui16(capBase+i+2));
            ti.push_back(ui16(capBase+i+1));
        }
        // Top cap
        int topBase = static_cast<int>(tv.size());
        Vector3 topN{0,1,0};
        tv.push_back({Vector3{0,0.5f,0}, topN, Vector2{0.5f,0.5f}});
        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * std::numbers::pi_v<float> * i / segments;
            float cx = std::cos(a), cz = std::sin(a);
            tv.push_back({Vector3{0.5f*cx,0.5f,0.5f*cz}, topN,
                          Vector2{0.5f+0.5f*cx, 0.5f-0.5f*cz}});
        }
        for (int i = 0; i < segments; ++i) {
            ti.push_back(ui16(topBase));
            ti.push_back(ui16(topBase+i+1));
            ti.push_back(ui16(topBase+i+2));
        }
        unitCylinder_.texVB = std::make_unique<VertexBuffer>(device_, static_cast<int>(tv.size()));
        unitCylinder_.texVB->SetData(tv.data(), static_cast<int>(tv.size()));
        unitCylinder_.texIB = std::make_unique<IndexBuffer>(device_, static_cast<int>(ti.size()));
        unitCylinder_.texIB->SetData(ti.data(), static_cast<int>(ti.size()));
        unitCylinder_.texPrimitiveCount = static_cast<int>(ti.size()) / 3;
    }
}

void SceneRenderer::buildUnitCone(int segments) {
    Color c(200, 200, 200, 255);
    std::vector<VertexPositionColor> verts;
    std::vector<uint16_t> indices;

    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * std::numbers::pi_v<float> * i / segments;
        verts.push_back({ Vector3{0.5f*std::cos(a), -0.5f, 0.5f*std::sin(a)}, c });
    }
    int apex   = static_cast<int>(verts.size());
    verts.push_back({ Vector3{0, 0.5f, 0}, c });
    int botCtr = static_cast<int>(verts.size());
    verts.push_back({ Vector3{0,-0.5f,0}, c });

    for (int i = 0; i < segments; ++i) {
        int j = (i+1) % segments;
        // Side
        indices.push_back(ui16(i)); indices.push_back(ui16(apex)); indices.push_back(ui16(j));
        // Bottom cap
        indices.push_back(ui16(botCtr)); indices.push_back(ui16(j)); indices.push_back(ui16(i));
    }
    unitCone_.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    unitCone_.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    unitCone_.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    unitCone_.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    unitCone_.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, unitCone_);

    // VPNT: side tris (apex vert per sector, with slant normal) + bottom cap
    {
        std::vector<VertexPositionNormalTexture> tv;
        std::vector<uint16_t> ti;
        // Side: per-sector quad-strip with interpolated normals
        float slopeY = 0.5f; // normal Y component for 45° slope
        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * std::numbers::pi_v<float> * i / segments;
            float cx = std::cos(a), cz = std::sin(a);
            float u = static_cast<float>(i) / segments;
            Vector3 n = Vector3::Normalize(Vector3{cx, slopeY, cz});
            tv.push_back({Vector3{0.5f*cx, -0.5f, 0.5f*cz}, n, Vector2{u, 1.0f}});
            tv.push_back({Vector3{0.0f, 0.5f, 0.0f}, n, Vector2{u + 0.5f / segments, 0.0f}});
        }
        for (int i = 0; i < segments; ++i) {
            int b = i * 2;
            ti.push_back(ui16(b)); ti.push_back(ui16(b+1)); ti.push_back(ui16(b+2));
        }
        // Bottom cap
        int capBase = static_cast<int>(tv.size());
        Vector3 botN{0,-1,0};
        tv.push_back({Vector3{0,-0.5f,0}, botN, Vector2{0.5f,0.5f}});
        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * std::numbers::pi_v<float> * i / segments;
            float cx = std::cos(a), cz = std::sin(a);
            tv.push_back({Vector3{0.5f*cx,-0.5f,0.5f*cz}, botN,
                          Vector2{0.5f+0.5f*cx, 0.5f+0.5f*cz}});
        }
        for (int i = 0; i < segments; ++i) {
            ti.push_back(ui16(capBase));
            ti.push_back(ui16(capBase+i+2));
            ti.push_back(ui16(capBase+i+1));
        }
        unitCone_.texVB = std::make_unique<VertexBuffer>(device_, static_cast<int>(tv.size()));
        unitCone_.texVB->SetData(tv.data(), static_cast<int>(tv.size()));
        unitCone_.texIB = std::make_unique<IndexBuffer>(device_, static_cast<int>(ti.size()));
        unitCone_.texIB->SetData(ti.data(), static_cast<int>(ti.size()));
        unitCone_.texPrimitiveCount = static_cast<int>(ti.size()) / 3;
    }
}

void SceneRenderer::buildUnitPlane() {
    Color c(200, 200, 200, 255);
    std::vector<VertexPositionColor> verts = {
        { Vector3{-0.5f, 0.0f, -0.5f}, c },
        { Vector3{ 0.5f, 0.0f, -0.5f}, c },
        { Vector3{ 0.5f, 0.0f,  0.5f}, c },
        { Vector3{-0.5f, 0.0f,  0.5f}, c },
    };
    static const uint16_t IDX[] = { 0,1,2, 0,2,3 };
    unitPlane_.vb = std::make_unique<VertexBuffer>(device_, 4);
    unitPlane_.vb->SetData(verts.data(), 4);
    unitPlane_.ib = std::make_unique<IndexBuffer>(device_, 6);
    unitPlane_.ib->SetData(IDX, 6);
    unitPlane_.primitiveCount = 2;
    storePositions(verts, unitPlane_);

    // VPNT: same 4 verts + normal up + UVs
    {
        Vector3 n{0,1,0};
        VertexPositionNormalTexture tv[4] = {
            {Vector3{-0.5f,0,-0.5f}, n, Vector2{0,0}},
            {Vector3{ 0.5f,0,-0.5f}, n, Vector2{1,0}},
            {Vector3{ 0.5f,0, 0.5f}, n, Vector2{1,1}},
            {Vector3{-0.5f,0, 0.5f}, n, Vector2{0,1}},
        };
        unitPlane_.texVB = std::make_unique<VertexBuffer>(device_, 4);
        unitPlane_.texVB->SetData(tv, 4);
        unitPlane_.texIB = std::make_unique<IndexBuffer>(device_, 6);
        unitPlane_.texIB->SetData(IDX, 6);
        unitPlane_.texPrimitiveCount = 2;
    }
}

void SceneRenderer::buildWireBox() {
    // 12 edges of a unit box
    Color c(255, 165, 0, 255); // orange for selection
    static const float P[][3] = {
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
        { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
        { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    };
    std::vector<VertexPositionColor> verts;
    auto edge = [&](int a, int b) {
        verts.push_back({ Vector3{P[a][0],P[a][1],P[a][2]}, c });
        verts.push_back({ Vector3{P[b][0],P[b][1],P[b][2]}, c });
    };
    edge(0,1); edge(1,2); edge(2,3); edge(3,0);
    edge(4,5); edge(5,6); edge(6,7); edge(7,4);
    edge(0,4); edge(1,5); edge(2,6); edge(3,7);

    wireBoxLineCount_ = 12;
    wireBoxVB_ = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    wireBoxVB_->SetData(verts.data(), static_cast<int>(verts.size()));
}

void SceneRenderer::buildWireShapes(int segments) {
    // Box: 12 edges
    {
        static const float P[][3] = {
            {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},
            {-0.5f,-0.5f, 0.5f},{0.5f,-0.5f, 0.5f},{0.5f,0.5f, 0.5f},{-0.5f,0.5f, 0.5f},
        };
        static const int E[][2] = {
            {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
        };
        for (auto& e : E) {
            wireShapeBox_.positions.push_back({P[e[0]][0],P[e[0]][1],P[e[0]][2]});
            wireShapeBox_.positions.push_back({P[e[1]][0],P[e[1]][1],P[e[1]][2]});
            wireShapeBox_.lineCount++;
        }
    }

    // Sphere: 3 great circles (XY, XZ, YZ planes)
    {
        int N = segments;
        for (int plane = 0; plane < 3; ++plane) {
            for (int i = 0; i < N; ++i) {
                float a0 = 2.0f * std::numbers::pi_v<float> * i     / N;
                float a1 = 2.0f * std::numbers::pi_v<float> * (i+1) / N;
                float c0 = 0.5f*std::cos(a0), s0 = 0.5f*std::sin(a0);
                float c1 = 0.5f*std::cos(a1), s1 = 0.5f*std::sin(a1);
                Vector3 v0, v1;
                if      (plane == 0) { v0={c0,s0,0};  v1={c1,s1,0};  }  // XY
                else if (plane == 1) { v0={c0,0,s0};  v1={c1,0,s1};  }  // XZ
                else                 { v0={0,c0,s0};  v1={0,c1,s1};  }  // YZ
                wireShapeSphere_.positions.push_back(v0);
                wireShapeSphere_.positions.push_back(v1);
                wireShapeSphere_.lineCount++;
            }
        }
    }

    // Cylinder: bottom ring + top ring + 4 vertical lines
    {
        int N = segments;
        for (int i = 0; i < N; ++i) {
            float a0 = 2.0f*std::numbers::pi_v<float>*i    /N;
            float a1 = 2.0f*std::numbers::pi_v<float>*(i+1)/N;
            wireShapeCylinder_.positions.push_back({0.5f*std::cos(a0),-0.5f,0.5f*std::sin(a0)});
            wireShapeCylinder_.positions.push_back({0.5f*std::cos(a1),-0.5f,0.5f*std::sin(a1)});
            wireShapeCylinder_.lineCount++;
            wireShapeCylinder_.positions.push_back({0.5f*std::cos(a0), 0.5f,0.5f*std::sin(a0)});
            wireShapeCylinder_.positions.push_back({0.5f*std::cos(a1), 0.5f,0.5f*std::sin(a1)});
            wireShapeCylinder_.lineCount++;
        }
        for (int i = 0; i < 4; ++i) {
            float a = 2.0f*std::numbers::pi_v<float>*i/4;
            float x = 0.5f*std::cos(a), z = 0.5f*std::sin(a);
            wireShapeCylinder_.positions.push_back({x,-0.5f,z});
            wireShapeCylinder_.positions.push_back({x, 0.5f,z});
            wireShapeCylinder_.lineCount++;
        }
    }

    // Cone: bottom ring + 4 lines to apex
    {
        int N = segments;
        for (int i = 0; i < N; ++i) {
            float a0 = 2.0f*std::numbers::pi_v<float>*i    /N;
            float a1 = 2.0f*std::numbers::pi_v<float>*(i+1)/N;
            wireShapeCone_.positions.push_back({0.5f*std::cos(a0),-0.5f,0.5f*std::sin(a0)});
            wireShapeCone_.positions.push_back({0.5f*std::cos(a1),-0.5f,0.5f*std::sin(a1)});
            wireShapeCone_.lineCount++;
        }
        for (int i = 0; i < 4; ++i) {
            float a = 2.0f*std::numbers::pi_v<float>*i/4;
            wireShapeCone_.positions.push_back({0.5f*std::cos(a),-0.5f,0.5f*std::sin(a)});
            wireShapeCone_.positions.push_back({0.0f, 0.5f, 0.0f});
            wireShapeCone_.lineCount++;
        }
    }

    // Plane: 4 boundary edges
    {
        wireShapePlane_.positions.push_back({-0.5f,0,-0.5f}); wireShapePlane_.positions.push_back({ 0.5f,0,-0.5f}); wireShapePlane_.lineCount++;
        wireShapePlane_.positions.push_back({ 0.5f,0,-0.5f}); wireShapePlane_.positions.push_back({ 0.5f,0, 0.5f}); wireShapePlane_.lineCount++;
        wireShapePlane_.positions.push_back({ 0.5f,0, 0.5f}); wireShapePlane_.positions.push_back({-0.5f,0, 0.5f}); wireShapePlane_.lineCount++;
        wireShapePlane_.positions.push_back({-0.5f,0, 0.5f}); wireShapePlane_.positions.push_back({-0.5f,0,-0.5f}); wireShapePlane_.lineCount++;
    }
}

// ---------------------------------------------------------------------------
// Gizmo
// ---------------------------------------------------------------------------

void SceneRenderer::drawGizmo(const Mc3Object* obj,
                               const Matrix& view, const Matrix& proj,
                               float gizmoLength)
{
    if (!obj) return;

    float px = obj->transform.position[0];
    float py = obj->transform.position[1];
    float pz = obj->transform.position[2];
    float L  = gizmoLength;

    // 3 axis lines (6 verts)
    VertexPositionColor lineVerts[6] = {
        { {px,   py, pz}, Color(210, 60,  60,  255) },  // X from
        { {px+L, py, pz}, Color(210, 60,  60,  255) },  // X to
        { {px, py,   pz}, Color(60,  210, 60,  255) },  // Y from
        { {px, py+L, pz}, Color(60,  210, 60,  255) },  // Y to
        { {px, py, pz  }, Color(60,  60,  210, 255) },  // Z from
        { {px, py, pz+L}, Color(60,  60,  210, 255) },  // Z to
    };

    VertexBuffer lineVB(device_, 6);
    lineVB.SetData(lineVerts, 6);

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&lineVB);
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, 3);
    device_.SetVertexBuffer(nullptr);

    // Small cube at each axis tip as a drag handle
    float hs = L * 0.10f;
    Color tipCols[3] = {
        Color(210, 60,  60,  255),
        Color(60,  210, 60,  255),
        Color(60,  60,  210, 255),
    };
    float tips[3][3] = { {px+L, py, pz}, {px, py+L, pz}, {px, py, pz+L} };
    for (int i = 0; i < 3; ++i) {
        Matrix world = Matrix::CreateScale(hs) *
                       Matrix::CreateTranslation({tips[i][0], tips[i][1], tips[i][2]});
        drawMesh(unitBox_, world, view, proj, tipCols[i]);
    }
}

void SceneRenderer::drawScaleGizmo(const Mc3Object* obj,
                                    const Matrix& view, const Matrix& proj,
                                    float gizmoLength)
{
    if (!obj) return;

    float px = obj->transform.position[0];
    float py = obj->transform.position[1];
    float pz = obj->transform.position[2];
    float L  = gizmoLength;

    VertexPositionColor lineVerts[6] = {
        { {px,   py, pz}, Color(210, 60,  60,  255) },
        { {px+L, py, pz}, Color(210, 60,  60,  255) },
        { {px, py,   pz}, Color(60,  210, 60,  255) },
        { {px, py+L, pz}, Color(60,  210, 60,  255) },
        { {px, py, pz  }, Color(60,  60,  210, 255) },
        { {px, py, pz+L}, Color(60,  60,  210, 255) },
    };

    VertexBuffer lineVB(device_, 6);
    lineVB.SetData(lineVerts, 6);

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&lineVB);
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, 3);
    device_.SetVertexBuffer(nullptr);

    // Flat-square tips (thin slab perpendicular to each axis)
    float hs = L * 0.12f;
    float thin = hs * 0.25f;
    Color tipCols[3] = {
        Color(210, 60,  60,  255),
        Color(60,  210, 60,  255),
        Color(60,  60,  210, 255),
    };
    float tips[3][3] = { {px+L, py, pz}, {px, py+L, pz}, {px, py, pz+L} };
    // Each tip is a flat box: thin along the axis direction, wide in the other two
    Microsoft::Xna::Framework::Vector3 tipScales[3] = {
        {thin, hs, hs},  // X tip: thin on X, square in YZ
        {hs, thin, hs},  // Y tip: thin on Y, square in XZ
        {hs, hs, thin},  // Z tip: thin on Z, square in XY
    };
    for (int i = 0; i < 3; ++i) {
        Matrix world = Matrix::CreateScale(tipScales[i]) *
                       Matrix::CreateTranslation({tips[i][0], tips[i][1], tips[i][2]});
        drawMesh(unitBox_, world, view, proj, tipCols[i]);
    }
}

void SceneRenderer::drawRotateGizmo(const Mc3Object* obj,
                                     const Matrix& view, const Matrix& proj,
                                     float gizmoLength)
{
    if (!obj) return;

    const float px = obj->transform.position[0];
    const float py = obj->transform.position[1];
    const float pz = obj->transform.position[2];
    const float L  = gizmoLength;
    const int   N  = 32;

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    Color cols[3] = {
        Color(210, 60,  60,  255),
        Color(60,  210, 60,  255),
        Color(60,  60,  210, 255),
    };

    for (int ax = 0; ax < 3; ++ax) {
        std::vector<VertexPositionColor> verts(N + 1);
        for (int j = 0; j <= N; ++j) {
            float t = 2.0f * std::numbers::pi_v<float> * j / N;
            float c = std::cos(t), s = std::sin(t);
            float wx, wy, wz;
            if      (ax == 0) { wx = px;       wy = py+L*c; wz = pz+L*s; } // X: circle in YZ
            else if (ax == 1) { wx = px+L*c;   wy = py;     wz = pz+L*s; } // Y: circle in XZ
            else              { wx = px+L*c;   wy = py+L*s; wz = pz;     } // Z: circle in XY
            verts[j] = { {wx, wy, wz}, cols[ax] };
        }
        VertexBuffer circleVB(device_, N + 1);
        circleVB.SetData(verts.data(), N + 1);
        device_.SetVertexBuffer(&circleVB);
        device_.DrawPrimitives(Graphics::PrimitiveType::LineStrip, 0, N);
        device_.SetVertexBuffer(nullptr);
    }
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
    int n = mesh.vb->VertexCount();

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
    int n = mesh.texVB->VertexCount();

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
    Color color  = materialColor(obj.material, doc);
    bool  sel    = isSelected(obj, selected);

    // Deform: geometry-level non-uniform scale, applied before primitive size and world transform
    Matrix deform = obj.deform
        ? Matrix::CreateScale({obj.deform->scale[0], obj.deform->scale[1], obj.deform->scale[2]})
        : Matrix::getIdentityProperty();

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

// ---------------------------------------------------------------------------
// Extrude path mesh generation
// ---------------------------------------------------------------------------

namespace {

struct ExtFrame { Vector3 pos, tan, nor, bi; };

static Vector3 vn3(Vector3 v) { return Vector3::Normalize(v); }

static Vector3 ptransportNor(Vector3 nor, Vector3 newTan) {
    Vector3 n = nor - newTan * Vector3::Dot(nor, newTan);
    float len = n.Length();
    return (len < 1e-7f) ? nor : (n / len);
}

static ExtFrame initExtFrame(Vector3 startPos, Vector3 tan) {
    Vector3 worldUp = (std::abs(tan.Y) < 0.9f) ? Vector3{0.0f,1.0f,0.0f} : Vector3{1.0f,0.0f,0.0f};
    Vector3 nor = vn3(worldUp - tan * Vector3::Dot(worldUp, tan));
    return { startPos, tan, nor, Vector3::Cross(tan, nor) };
}

static Vector3 evalCubicBezier(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {
    float mt = 1.0f - t;
    return p0*(mt*mt*mt) + p1*(3.0f*mt*mt*t) + p2*(3.0f*mt*t*t) + p3*(t*t*t);
}

static Vector3 evalCubicBezierTan(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {
    float mt = 1.0f - t;
    Vector3 d = (p1-p0)*(3.0f*mt*mt) + (p2-p1)*(6.0f*mt*t) + (p3-p2)*(3.0f*t*t);
    float len = d.Length();
    return (len < 1e-7f) ? vn3(p3-p0) : d / len;
}

static std::vector<ExtFrame> makePathFrames(const Mc3ExtrudePath& path, int segs) {
    const float pi2 = 2.0f * std::numbers::pi_v<float>;
    std::vector<ExtFrame> fr;
    fr.reserve(segs + 1);

    switch (path.type) {

    case ExtrudePathType::Line: {
        Vector3 dir, nor, bi;
        if      (path.axis == "x") { dir={1,0,0}; nor={0,1,0}; bi={0,0,1}; }
        else if (path.axis == "z") { dir={0,0,1}; nor={1,0,0}; bi={0,1,0}; }
        else                        { dir={0,1,0}; nor={1,0,0}; bi={0,0,1}; }
        for (int i = 0; i <= segs; ++i) {
            float t = static_cast<float>(i) / segs * path.length;
            fr.push_back({ dir * t, dir, nor, bi });
        }
        break;
    }

    case ExtrudePathType::Arc: {
        float R     = path.arcRadius;
        float total = path.arcAngle * (std::numbers::pi_v<float> / 180.0f);
        // Arc starts at origin, sweeps in XZ plane around center (R,0,0)
        for (int i = 0; i <= segs; ++i) {
            float theta = total * i / segs;
            float c = std::cos(theta), s = std::sin(theta);
            Vector3 pos = { R*(1.0f - c), 0.0f, R*s };
            Vector3 tan = { s, 0.0f, c };          // normalised tangent
            Vector3 nor = { c, 0.0f, -s };         // toward arc center
            Vector3 bi  = Vector3::Cross(tan, nor);
            fr.push_back({ pos, tan, nor, bi });
        }
        break;
    }

    case ExtrudePathType::Helix: {
        float R     = path.helixRadius;
        float H     = path.helixHeight;
        float omega = path.helixTurns * pi2;
        for (int i = 0; i <= segs; ++i) {
            float t     = static_cast<float>(i) / segs;
            float theta = omega * t;
            Vector3 pos = { R * std::cos(theta), H * t, R * std::sin(theta) };
            Vector3 tan = vn3({ -R * omega * std::sin(theta), H,
                                  R * omega * std::cos(theta) });
            if (fr.empty()) {
                fr.push_back(initExtFrame(pos, tan));
            } else {
                Vector3 nor = ptransportNor(fr.back().nor, tan);
                fr.push_back({ pos, tan, nor, Vector3::Cross(tan, nor) });
            }
        }
        break;
    }

    case ExtrudePathType::Polyline: {
        const auto& pts = path.points;
        if (pts.size() < 2) {
            fr.push_back(initExtFrame({0,0,0},{0,1,0}));
            fr.push_back(initExtFrame({0,1,0},{0,1,0}));
            break;
        }
        int numSeg  = static_cast<int>(pts.size()) - 1;
        int perSeg  = std::max(1, segs / numSeg);
        for (int seg = 0; seg < numSeg; ++seg) {
            const auto& a = pts[seg].position;
            const auto& b = pts[seg+1].position;
            Vector3 A = {a[0],a[1],a[2]}, B = {b[0],b[1],b[2]};
            Vector3 tan = vn3(B - A);
            int start = (seg == 0) ? 0 : 1;
            for (int s = start; s <= perSeg; ++s) {
                float t   = static_cast<float>(s) / perSeg;
                Vector3 p = A + (B - A) * t;
                if (fr.empty()) fr.push_back(initExtFrame(p, tan));
                else {
                    Vector3 nor = ptransportNor(fr.back().nor, tan);
                    fr.push_back({ p, tan, nor, Vector3::Cross(tan, nor) });
                }
            }
        }
        break;
    }

    case ExtrudePathType::Bezier: {
        const auto& pts = path.points;
        if (pts.size() < 2) {
            fr.push_back(initExtFrame({0,0,0},{0,1,0}));
            fr.push_back(initExtFrame({0,1,0},{0,1,0}));
            break;
        }
        int numCurves    = static_cast<int>(pts.size()) - 1;
        int segsPerCurve = std::max(2, segs / numCurves);
        for (int seg = 0; seg < numCurves; ++seg) {
            const auto& pp0 = pts[seg];
            const auto& pp1 = pts[seg+1];
            Vector3 P0 = {pp0.position[0], pp0.position[1], pp0.position[2]};
            Vector3 P3 = {pp1.position[0], pp1.position[1], pp1.position[2]};
            Vector3 c0 = {pp0.controlIn[0], pp0.controlIn[1], pp0.controlIn[2]};
            Vector3 c1 = {pp1.controlIn[0], pp1.controlIn[1], pp1.controlIn[2]};
            Vector3 P1 = (c0.Length() > 1e-6f) ? (P0 + c0) : Vector3::Lerp(P0, P3, 1.0f/3.0f);
            Vector3 P2 = (c1.Length() > 1e-6f) ? (P3 - c1) : Vector3::Lerp(P0, P3, 2.0f/3.0f);
            int start = (seg == 0) ? 0 : 1;
            for (int s = start; s <= segsPerCurve; ++s) {
                float t     = static_cast<float>(s) / segsPerCurve;
                Vector3 pos = evalCubicBezier(P0, P1, P2, P3, t);
                Vector3 tan = evalCubicBezierTan(P0, P1, P2, P3, t);
                if (fr.empty()) fr.push_back(initExtFrame(pos, tan));
                else {
                    Vector3 nor = ptransportNor(fr.back().nor, tan);
                    fr.push_back({ pos, tan, nor, Vector3::Cross(tan, nor) });
                }
            }
        }
        break;
    }

    } // switch
    return fr;
}

struct Pt2 { float u, v; };

static std::vector<Pt2> makeProfile(const Mc3CrossSection& cs) {
    std::vector<Pt2> pts;
    switch (cs.type) {
    case CrossSectionType::Rect: {
        float hu = cs.width  * 0.5f, hv = cs.height * 0.5f;
        pts = { {-hu,-hv},{hu,-hv},{hu,hv},{-hu,hv} };
        break;
    }
    case CrossSectionType::Circle: {
        int N = std::max(4, cs.segments);
        for (int i = 0; i < N; ++i) {
            float a = 2.0f * std::numbers::pi_v<float> * i / N;
            pts.push_back({ cs.radius * std::cos(a), cs.radius * std::sin(a) });
        }
        break;
    }
    case CrossSectionType::Polygon: {
        int N = std::max(3, cs.sides);
        for (int i = 0; i < N; ++i) {
            float a = 2.0f * std::numbers::pi_v<float> * i / N;
            pts.push_back({ cs.radius * std::cos(a), cs.radius * std::sin(a) });
        }
        break;
    }
    case CrossSectionType::Custom:
        for (const auto& p : cs.customPoints)
            pts.push_back({ p.x, p.y });
        if (pts.empty())
            pts = { {-0.5f,-0.5f},{0.5f,-0.5f},{0.5f,0.5f},{-0.5f,0.5f} };
        break;
    }
    return pts;
}

} // anonymous namespace

void SceneRenderer::drawExtrudeDynamic(const Mc3Extrude& ex,
                                        const Matrix& world,
                                        const Matrix& view, const Matrix& proj,
                                        Color color)
{
    int pathSegs = std::max(3, ex.segments);
    auto frames  = makePathFrames(ex.path, pathSegs);
    auto profile = makeProfile(ex.crossSection);

    int M = static_cast<int>(frames.size());
    int N = static_cast<int>(profile.size());
    if (M < 2 || N < 3) { drawMesh(unitBox_, world, view, proj, color); return; }

    bool hollow = (ex.crossSection.innerRadius > 0.0f) &&
                  (ex.crossSection.type == CrossSectionType::Circle ||
                   ex.crossSection.type == CrossSectionType::Polygon);
    float innerScale = hollow ? (ex.crossSection.innerRadius / ex.crossSection.radius) : 0.0f;

    float twistRad     = ex.twist * (std::numbers::pi_v<float> / 180.0f);
    float twistPerStep = (M > 1) ? twistRad / (M - 1) : 0.0f;

    std::vector<VertexPositionColor> verts;
    verts.reserve(hollow ? 2*M*N : M*N + 2);

    // Outer rings
    for (int i = 0; i < M; ++i) {
        const auto& f = frames[i];
        float angle = twistPerStep * i;
        float ca = std::cos(angle), sa = std::sin(angle);
        for (int j = 0; j < N; ++j) {
            float u = profile[j].u * ca - profile[j].v * sa;
            float v = profile[j].u * sa + profile[j].v * ca;
            verts.push_back({ f.pos + f.nor * u + f.bi * v, color });
        }
    }

    int innerBase = static_cast<int>(verts.size());
    int botCtrIdx = -1, topCtrIdx = -1;

    if (hollow) {
        // Inner rings (scaled to innerRadius)
        for (int i = 0; i < M; ++i) {
            const auto& f = frames[i];
            float angle = twistPerStep * i;
            float ca = std::cos(angle), sa = std::sin(angle);
            for (int j = 0; j < N; ++j) {
                float u = profile[j].u * innerScale * ca - profile[j].v * innerScale * sa;
                float v = profile[j].u * innerScale * sa + profile[j].v * innerScale * ca;
                verts.push_back({ f.pos + f.nor * u + f.bi * v, color });
            }
        }
    } else {
        botCtrIdx = static_cast<int>(verts.size());
        verts.push_back({ frames[0].pos,   color });
        topCtrIdx = static_cast<int>(verts.size());
        verts.push_back({ frames[M-1].pos, color });
    }

    std::vector<uint16_t> indices;
    indices.reserve(hollow ? (M-1)*N*12 + (ex.caps ? N*12 : 0)
                           : (M-1)*N*6  + (ex.caps ? N*6  : 0));

    auto oVi = [N](int ring, int pt) -> uint16_t {
        return static_cast<uint16_t>(ring * N + pt % N);
    };
    auto iVi = [N, innerBase](int ring, int pt) -> uint16_t {
        return static_cast<uint16_t>(innerBase + ring * N + pt % N);
    };

    // Outer wall
    for (int i = 0; i < M-1; ++i) {
        for (int j = 0; j < N; ++j) {
            int j1 = (j+1) % N;
            indices.push_back(oVi(i,   j));
            indices.push_back(oVi(i+1, j));
            indices.push_back(oVi(i,   j1));
            indices.push_back(oVi(i+1, j));
            indices.push_back(oVi(i+1, j1));
            indices.push_back(oVi(i,   j1));
        }
    }

    if (hollow) {
        // Inner wall (reversed winding so normals face inward)
        for (int i = 0; i < M-1; ++i) {
            for (int j = 0; j < N; ++j) {
                int j1 = (j+1) % N;
                indices.push_back(iVi(i,   j));
                indices.push_back(iVi(i,   j1));
                indices.push_back(iVi(i+1, j));
                indices.push_back(iVi(i+1, j));
                indices.push_back(iVi(i,   j1));
                indices.push_back(iVi(i+1, j1));
            }
        }
        if (ex.caps) {
            // Bottom annular cap (facing -path direction)
            for (int j = 0; j < N; ++j) {
                int j1 = (j+1) % N;
                indices.push_back(oVi(0, j));
                indices.push_back(oVi(0, j1));
                indices.push_back(iVi(0, j1));
                indices.push_back(oVi(0, j));
                indices.push_back(iVi(0, j1));
                indices.push_back(iVi(0, j));
            }
            // Top annular cap (facing +path direction)
            for (int j = 0; j < N; ++j) {
                int j1 = (j+1) % N;
                indices.push_back(oVi(M-1, j));
                indices.push_back(iVi(M-1, j));
                indices.push_back(iVi(M-1, j1));
                indices.push_back(oVi(M-1, j));
                indices.push_back(iVi(M-1, j1));
                indices.push_back(oVi(M-1, j1));
            }
        }
    } else {
        if (ex.caps) {
            for (int j = 0; j < N; ++j) {
                indices.push_back(static_cast<uint16_t>(botCtrIdx));
                indices.push_back(oVi(0, (j+1)%N));
                indices.push_back(oVi(0, j));
            }
            for (int j = 0; j < N; ++j) {
                indices.push_back(static_cast<uint16_t>(topCtrIdx));
                indices.push_back(oVi(M-1, j));
                indices.push_back(oVi(M-1, (j+1)%N));
            }
        }
    }

    if (indices.empty()) { drawMesh(unitBox_, world, view, proj, color); return; }

    int numVerts = static_cast<int>(verts.size());
    int numTris  = static_cast<int>(indices.size()) / 3;

    if (numVerts > 65535) { drawMesh(unitBox_, world, view, proj, color); return; }

    VertexBuffer tmpVB(device_, numVerts);
    tmpVB.SetData(verts.data(), numVerts);

    IndexBuffer tmpIB(device_, static_cast<int>(indices.size()));
    tmpIB.SetData(indices.data(), static_cast<int>(indices.size()));

    effect_->World      = world;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&tmpVB);
    device_.SetIndexBuffer(&tmpIB);
    device_.DrawIndexedPrimitives(
        Graphics::PrimitiveType::TriangleList,
        0, 0, numVerts, 0, numTris);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);
}

// ---------------------------------------------------------------------------
// Edge overlay
// ---------------------------------------------------------------------------

void SceneRenderer::drawWireShape(const WireShape& wire,
                                   const Matrix& world, const Matrix& view, const Matrix& proj,
                                   Color color)
{
    if (wire.positions.empty()) return;
    int n = static_cast<int>(wire.positions.size());
    std::vector<VertexPositionColor> verts(n);
    for (int i = 0; i < n; ++i)
        verts[i] = { wire.positions[i], color };

    VertexBuffer vb(device_, n);
    vb.SetData(verts.data(), n);

    effect_->World      = world;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&vb);
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, wire.lineCount);
    device_.SetVertexBuffer(nullptr);
}

void SceneRenderer::drawObjectEdges(const Mc3Object& obj, const Mc3Document& doc,
                                     const Matrix& parentWorld, const Matrix& view, const Matrix& proj,
                                     int depth)
{
    if (!obj.visible) return;
    if (depth > 16) return;

    Matrix world = objectWorldMatrix(obj.transform) * parentWorld;
    Color edgeColor(0, 0, 0, 220);

    // Slight outward push (scale in local space) to avoid z-fighting with the solid mesh
    constexpr float kPush = 1.003f;

    Matrix deform = obj.deform
        ? Matrix::CreateScale({obj.deform->scale[0], obj.deform->scale[1], obj.deform->scale[2]})
        : Matrix::getIdentityProperty();

    switch (obj.type) {
    case ObjectType::Box:
    case ObjectType::Cube: {
        float sx = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float sy = obj.primitive ? obj.primitive->size[1] : 1.0f;
        float sz = obj.primitive ? obj.primitive->size[2] : 1.0f;
        Matrix m = deform * Matrix::CreateScale({sx*kPush, sy*kPush, sz*kPush}) * world;
        drawWireShape(wireShapeBox_, m, view, proj, edgeColor);
        break;
    }
    case ObjectType::Sphere: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        Matrix m = deform * Matrix::CreateScale({r*kPush, r*kPush, r*kPush}) * world;
        drawWireShape(wireShapeSphere_, m, view, proj, edgeColor);
        break;
    }
    case ObjectType::Cylinder: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        Matrix m = deform * Matrix::CreateScale({r*kPush, h*kPush, r*kPush}) * world;
        drawWireShape(wireShapeCylinder_, m, view, proj, edgeColor);
        break;
    }
    case ObjectType::Cone: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        Matrix m = deform * Matrix::CreateScale({r*kPush, h*kPush, r*kPush}) * world;
        drawWireShape(wireShapeCone_, m, view, proj, edgeColor);
        break;
    }
    case ObjectType::Plane: {
        float w = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float d = obj.primitive ? obj.primitive->size[2] : 1.0f;
        Matrix m = deform * Matrix::CreateScale({w*kPush, 1.0f, d*kPush}) * world;
        drawWireShape(wireShapePlane_, m, view, proj, edgeColor);
        break;
    }
    case ObjectType::Group:
    case ObjectType::Area:
    case ObjectType::Union:
    case ObjectType::Intersection:
    case ObjectType::Difference:
        for (const auto& child : obj.children)
            drawObjectEdges(*child, doc, world, view, proj, depth + 1);
        break;
    case ObjectType::Instance: {
        auto it = doc.definitions.find(obj.definition);
        if (it != doc.definitions.end() && it->second)
            drawObjectEdges(*it->second, doc, world, view, proj, depth + 1);
        else
            drawWireShape(wireShapeBox_, world, view, proj, edgeColor);
        break;
    }
    case ObjectType::Extrude: {
        if (!obj.extrude) { drawWireShape(wireShapeBox_, deform * world, view, proj, edgeColor); break; }
        const auto& ex = obj.extrude.value();

        // Cap segments for overlay quality (no need to match the solid mesh exactly)
        int pathSegs = std::max(3, std::min(ex.segments, 20));
        auto frames  = makePathFrames(ex.path, pathSegs);
        auto profile = makeProfile(ex.crossSection);

        int M = static_cast<int>(frames.size());
        int N = static_cast<int>(profile.size());
        if (M < 2 || N < 2) { drawWireShape(wireShapeBox_, deform * world, view, proj, edgeColor); break; }

        bool hollow = (ex.crossSection.innerRadius > 0.0f) &&
                      (ex.crossSection.type == CrossSectionType::Circle ||
                       ex.crossSection.type == CrossSectionType::Polygon);
        float innerScale = hollow ? (ex.crossSection.innerRadius / ex.crossSection.radius) : 0.0f;

        float twistRad     = ex.twist * (std::numbers::pi_v<float> / 180.0f);
        float twistPerStep = (M > 1) ? twistRad / (M - 1) : 0.0f;
        Matrix localToWorld = deform * world;

        auto wposScaled = [&](int i, int j, float scale) -> Vector3 {
            const auto& f = frames[i];
            float angle = twistPerStep * i;
            float ca = std::cos(angle), sa = std::sin(angle);
            float u = (profile[j].u * ca - profile[j].v * sa) * scale;
            float v = (profile[j].u * sa + profile[j].v * ca) * scale;
            return Vector3::Transform(f.pos + f.nor * u + f.bi * v, localToWorld);
        };

        auto wpos = [&](int i, int j) -> Vector3 {
            return wposScaled(i, j, kPush);
        };
        auto wposInner = [&](int i, int j) -> Vector3 {
            return wposScaled(i, j, innerScale);
        };

        std::vector<VertexPositionColor> lines;

        // Profile rings — show up to 20 evenly-spaced rings (always include first and last)
        int ringStep = std::max(1, (M - 1) / 19);
        for (int i = 0; i < M; i += ringStep) {
            for (int j = 0; j < N; ++j) {
                lines.push_back({ wpos(i, j),       edgeColor });
                lines.push_back({ wpos(i, (j+1)%N), edgeColor });
            }
            if (hollow) {
                for (int j = 0; j < N; ++j) {
                    lines.push_back({ wposInner(i, j),       edgeColor });
                    lines.push_back({ wposInner(i, (j+1)%N), edgeColor });
                }
            }
        }
        if ((M - 1) % ringStep != 0) {
            for (int j = 0; j < N; ++j) {
                lines.push_back({ wpos(M-1, j),       edgeColor });
                lines.push_back({ wpos(M-1, (j+1)%N), edgeColor });
            }
            if (hollow) {
                for (int j = 0; j < N; ++j) {
                    lines.push_back({ wposInner(M-1, j),       edgeColor });
                    lines.push_back({ wposInner(M-1, (j+1)%N), edgeColor });
                }
            }
        }

        // Spine lines along the path — min(N, 8) evenly-spaced profile points
        int spineN = std::min(N, 8);
        for (int s = 0; s < spineN; ++s) {
            int j = s * N / spineN;
            for (int i = 0; i < M-1; ++i) {
                lines.push_back({ wpos(i,   j), edgeColor });
                lines.push_back({ wpos(i+1, j), edgeColor });
                if (hollow) {
                    lines.push_back({ wposInner(i,   j), edgeColor });
                    lines.push_back({ wposInner(i+1, j), edgeColor });
                }
            }
        }

        if (!lines.empty()) drawLineList(lines, view, proj);
        break;
    }
    default:
        drawWireShape(wireShapeBox_, world, view, proj, edgeColor);
        break;
    }
}

void SceneRenderer::drawEdgeOverlay(const Mc3Document& doc,
                                     const Matrix& view, const Matrix& proj)
{
    Matrix identity = Matrix::getIdentityProperty();
    for (const auto& obj : doc.objects)
        drawObjectEdges(*obj, doc, identity, view, proj);
}

} // namespace MeshCraft::Renderer
