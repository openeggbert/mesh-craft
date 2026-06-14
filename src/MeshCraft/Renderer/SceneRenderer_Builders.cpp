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
#include <numbers>
#include <vector>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Renderer;

namespace MeshCraft::Renderer {

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

void SceneRenderer::buildUnitTorus(int ringSeg, int tubeSeg) {
    // Unit torus: majorRadius R=0.35, minorRadius r=0.15 (outer edge at 0.5)
    const float R = 0.35f;
    const float r = 0.15f;
    const float pi2 = 2.0f * std::numbers::pi_v<float>;
    Color c(200, 200, 200, 255);

    std::vector<VertexPositionColor> verts;
    std::vector<uint16_t> indices;
    verts.reserve((ringSeg+1)*(tubeSeg+1));
    indices.reserve(ringSeg*tubeSeg*6);

    for (int i = 0; i <= ringSeg; ++i) {
        float theta = pi2 * i / ringSeg;
        float ct = std::cos(theta), st = std::sin(theta);
        for (int j = 0; j <= tubeSeg; ++j) {
            float phi = pi2 * j / tubeSeg;
            float cp = std::cos(phi), sp = std::sin(phi);
            float x = (R + r * cp) * ct;
            float y = r * sp;
            float z = (R + r * cp) * st;
            verts.push_back({ Vector3{x, y, z}, c });
        }
    }
    for (int i = 0; i < ringSeg; ++i) {
        for (int j = 0; j < tubeSeg; ++j) {
            int a = i * (tubeSeg+1) + j;
            int b = a + 1;
            int c2 = (i+1) * (tubeSeg+1) + j;
            int d  = c2 + 1;
            indices.push_back(ui16(a)); indices.push_back(ui16(b)); indices.push_back(ui16(d));
            indices.push_back(ui16(a)); indices.push_back(ui16(d)); indices.push_back(ui16(c2));
        }
    }

    int nv = static_cast<int>(verts.size());
    int ni = static_cast<int>(indices.size());
    unitTorus_.vb = std::make_unique<VertexBuffer>(device_, nv);
    unitTorus_.vb->SetData(verts.data(), nv);
    unitTorus_.ib = std::make_unique<IndexBuffer>(device_, ni);
    unitTorus_.ib->SetData(indices.data(), ni);
    unitTorus_.primitiveCount = ni / 3;
    storePositions(verts, unitTorus_);

    // VPNT version: normals + UVs
    {
        std::vector<VertexPositionNormalTexture> tv;
        tv.reserve(verts.size());
        for (int i = 0; i <= ringSeg; ++i) {
            float theta = pi2 * i / ringSeg;
            float ct = std::cos(theta), st = std::sin(theta);
            for (int j = 0; j <= tubeSeg; ++j) {
                float phi = pi2 * j / tubeSeg;
                float cp = std::cos(phi), sp = std::sin(phi);
                float x = (R + r * cp) * ct;
                float y = r * sp;
                float z = (R + r * cp) * st;
                Vector3 pos{x, y, z};
                Vector3 norm = Vector3::Normalize({cp * ct, sp, cp * st});
                Vector2 uv{static_cast<float>(i)/ringSeg, static_cast<float>(j)/tubeSeg};
                tv.push_back({pos, norm, uv});
            }
        }
        unitTorus_.texVB = std::make_unique<VertexBuffer>(device_, nv);
        unitTorus_.texVB->SetData(tv.data(), nv);
        unitTorus_.texIB = std::make_unique<IndexBuffer>(device_, ni);
        unitTorus_.texIB->SetData(indices.data(), ni);
        unitTorus_.texPrimitiveCount = ni / 3;
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

    // Torus (R=0.35, r=0.15): outer ring, inner ring, 4 tube cross-sections
    {
        const float R = 0.35f, r = 0.15f;
        const float pi2 = 2.0f * std::numbers::pi_v<float>;
        // Outer and inner circles in XZ plane
        for (int ring = 0; ring < 2; ++ring) {
            float rad = (ring == 0) ? (R + r) : (R - r);
            for (int i = 0; i < segments; ++i) {
                float a0 = pi2 * i / segments;
                float a1 = pi2 * (i+1) / segments;
                wireShapeTorus_.positions.push_back({rad*std::cos(a0), 0.0f, rad*std::sin(a0)});
                wireShapeTorus_.positions.push_back({rad*std::cos(a1), 0.0f, rad*std::sin(a1)});
                wireShapeTorus_.lineCount++;
            }
        }
        // 4 tube cross-section circles at 0, 90, 180, 270 degrees
        for (int q = 0; q < 4; ++q) {
            float theta = pi2 * q / 4;
            float ct = std::cos(theta), st = std::sin(theta);
            for (int i = 0; i < segments; ++i) {
                float phi0 = pi2 * i / segments;
                float phi1 = pi2 * (i+1) / segments;
                wireShapeTorus_.positions.push_back({(R+r*std::cos(phi0))*ct, r*std::sin(phi0), (R+r*std::cos(phi0))*st});
                wireShapeTorus_.positions.push_back({(R+r*std::cos(phi1))*ct, r*std::sin(phi1), (R+r*std::cos(phi1))*st});
                wireShapeTorus_.lineCount++;
            }
        }
    }
}

} // namespace MeshCraft::Renderer
