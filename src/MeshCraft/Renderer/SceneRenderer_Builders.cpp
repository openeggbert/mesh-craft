#include "MeshCraft/Renderer/SceneRenderer.hpp"
#include "MeshCraft/Renderer/PrimitiveTessellationAlg.hpp"

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
#include <map>
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
    // 8 corners of a unit cube centered at origin, 12 triangles (6 faces, 2
    // tri each).
    //
    // STAB-castle-fix: was wound CCW-from-outside (the glTF/OpenGL
    // convention) on all 12 triangles, confirmed via direct cross-product-
    // vs-normal computation -- but CNA's default RasterizerState
    // (CullCounterClockwiseFace, matching true XNA/D3D9 semantics, per a
    // sibling investigation's own test-verified analysis of the identical
    // bug class in easy-3d/CubeMesh.cpp) requires CW-from-outside. Every
    // face was backface-culled when viewed from outside, showing the
    // mirrored interior of the far side instead -- reported as "3 of 6
    // walls transparent" (from any camera angle, ~3 faces face the camera
    // and get wrongly culled). Fixed by swapping the last two indices of
    // every triangle (flips winding without changing which vertex gets
    // which position/normal/UV).
    //
    // SYS-W7-02: this vertex/index generation now lives in
    // tessellateUnitBoxAlg() (PrimitiveTessellationAlg.hpp) so it can be
    // exercised headlessly for differential testing against the exporter's
    // independent MeshBuilder.cpp::buildBox() -- moved verbatim, winding
    // unchanged.
    RawTessellation rtb = tessellateUnitBoxAlg();
    std::vector<VertexPositionColor> verts;
    verts.reserve(rtb.positions.size());
    for (auto& p : rtb.positions) verts.push_back({ Vector3{p[0],p[1],p[2]}, c });
    std::vector<uint16_t> IDX;
    IDX.reserve(rtb.indices.size());
    for (auto idx : rtb.indices) IDX.push_back(ui16(static_cast<int>(idx)));

    unitBox_.vb = std::make_unique<VertexBuffer>(device_, 8);
    unitBox_.vb->SetData(verts.data(), 8);
    unitBox_.ib = std::make_unique<IndexBuffer>(device_, 36);
    unitBox_.ib->SetData(IDX.data(), 36);
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
        // STAB-castle-fix: same winding fix as the VPC IDX[] array above.
        tidx.push_back(ui16(base)); tidx.push_back(ui16(base+1)); tidx.push_back(ui16(base+2));
        tidx.push_back(ui16(base)); tidx.push_back(ui16(base+2)); tidx.push_back(ui16(base+3));
    }
    unitBox_.texVB = std::make_unique<VertexBuffer>(device_, 24);
    unitBox_.texVB->SetData(tverts.data(), 24);
    unitBox_.texIB = std::make_unique<IndexBuffer>(device_, 36);
    unitBox_.texIB->SetData(tidx.data(), 36);
    unitBox_.texPrimitiveCount = 12;
    unitBox_.texturedVertices = tverts;
}

void SceneRenderer::buildUnitSphere(int segments, RenderMesh& target) {
    Color c(200, 200, 200, 255);
    int rings   = segments / 2;
    int sectors = segments;

    // SYS-W7-02: vertex/index generation now lives in
    // tessellateUnitSphereAlg() (PrimitiveTessellationAlg.hpp), moved
    // verbatim so it can be exercised headlessly for differential testing
    // against the exporter's independent MeshBuilder.cpp::buildSphere().
    RawTessellation rts = tessellateUnitSphereAlg(segments);
    std::vector<VertexPositionColor> verts;
    verts.reserve(rts.positions.size());
    for (auto& p : rts.positions) verts.push_back({ Vector3{p[0],p[1],p[2]}, c });
    std::vector<uint16_t> indices;
    indices.reserve(rts.indices.size());
    for (auto idx : rts.indices) indices.push_back(ui16(static_cast<int>(idx)));

    target.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    target.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    target.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    target.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    target.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, target);

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
        target.texVB = std::make_unique<VertexBuffer>(device_, static_cast<int>(tv.size()));
        target.texVB->SetData(tv.data(), static_cast<int>(tv.size()));
        target.texIB = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
        target.texIB->SetData(indices.data(), static_cast<int>(indices.size()));
        target.texPrimitiveCount = static_cast<int>(indices.size()) / 3;
        target.texturedVertices = std::move(tv);
    }
}

void SceneRenderer::buildUnitCylinder(int segments, RenderMesh& target) {
    Color c(200, 200, 200, 255);

    // SYS-W7-02: vertex/index generation now lives in
    // tessellateUnitCylinderAlg() (PrimitiveTessellationAlg.hpp), moved
    // verbatim so it can be exercised headlessly for differential testing
    // against the exporter's independent MeshBuilder.cpp::buildCylinder().
    RawTessellation rtc = tessellateUnitCylinderAlg(segments);
    std::vector<VertexPositionColor> verts;
    verts.reserve(rtc.positions.size());
    for (auto& p : rtc.positions) verts.push_back({ Vector3{p[0],p[1],p[2]}, c });
    std::vector<uint16_t> indices;
    indices.reserve(rtc.indices.size());
    for (auto idx : rtc.indices) indices.push_back(ui16(static_cast<int>(idx)));

    target.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    target.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    target.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    target.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    target.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, target);

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
        target.texVB = std::make_unique<VertexBuffer>(device_, static_cast<int>(tv.size()));
        target.texVB->SetData(tv.data(), static_cast<int>(tv.size()));
        target.texIB = std::make_unique<IndexBuffer>(device_, static_cast<int>(ti.size()));
        target.texIB->SetData(ti.data(), static_cast<int>(ti.size()));
        target.texPrimitiveCount = static_cast<int>(ti.size()) / 3;
        target.texturedVertices = std::move(tv);
    }
}

void SceneRenderer::buildUnitCone(int segments, RenderMesh& target) {
    Color c(200, 200, 200, 255);

    // SYS-W7-02: vertex/index generation now lives in
    // tessellateUnitConeAlg() (PrimitiveTessellationAlg.hpp) -- includes the
    // STAB-castle-fix winding swap ((i,j,apex), not (i,apex,j)) -- moved
    // verbatim so it can be exercised headlessly for differential testing
    // against the exporter's independent MeshBuilder.cpp::buildCone().
    RawTessellation rtco = tessellateUnitConeAlg(segments);
    std::vector<VertexPositionColor> verts;
    verts.reserve(rtco.positions.size());
    for (auto& p : rtco.positions) verts.push_back({ Vector3{p[0],p[1],p[2]}, c });
    std::vector<uint16_t> indices;
    indices.reserve(rtco.indices.size());
    for (auto idx : rtco.indices) indices.push_back(ui16(static_cast<int>(idx)));

    target.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    target.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    target.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    target.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    target.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, target);

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
        // STAB-castle-fix: same winding fix as the VPC side loop above --
        // was (b, b+1, b+2), wound CCW-from-outside.
        for (int i = 0; i < segments; ++i) {
            int b = i * 2;
            ti.push_back(ui16(b)); ti.push_back(ui16(b+2)); ti.push_back(ui16(b+1));
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
        target.texVB = std::make_unique<VertexBuffer>(device_, static_cast<int>(tv.size()));
        target.texVB->SetData(tv.data(), static_cast<int>(tv.size()));
        target.texIB = std::make_unique<IndexBuffer>(device_, static_cast<int>(ti.size()));
        target.texIB->SetData(ti.data(), static_cast<int>(ti.size()));
        target.texPrimitiveCount = static_cast<int>(ti.size()) / 3;
        target.texturedVertices = std::move(tv);
    }
}

void SceneRenderer::buildUnitPlane() {
    Color c(200, 200, 200, 255);
    // SYS-W7-02: vertex/index generation now lives in
    // tessellateUnitPlaneAlg() (PrimitiveTessellationAlg.hpp), moved
    // verbatim so it can be exercised headlessly for differential testing
    // against the exporter's independent MeshBuilder.cpp::buildPlane().
    RawTessellation rtp = tessellateUnitPlaneAlg();
    std::vector<VertexPositionColor> verts;
    verts.reserve(rtp.positions.size());
    for (auto& p : rtp.positions) verts.push_back({ Vector3{p[0],p[1],p[2]}, c });
    std::vector<uint16_t> IDX;
    IDX.reserve(rtp.indices.size());
    for (auto idx : rtp.indices) IDX.push_back(ui16(static_cast<int>(idx)));

    unitPlane_.vb = std::make_unique<VertexBuffer>(device_, 4);
    unitPlane_.vb->SetData(verts.data(), 4);
    unitPlane_.ib = std::make_unique<IndexBuffer>(device_, 6);
    unitPlane_.ib->SetData(IDX.data(), 6);
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
        unitPlane_.texIB->SetData(IDX.data(), 6);
        unitPlane_.texPrimitiveCount = 2;
        unitPlane_.texturedVertices.assign(tv, tv + 4);
    }
}

void SceneRenderer::buildUnitTorus(int ringSeg, int tubeSeg, RenderMesh& target,
                                    float majorRadius, float minorRadius) {
    // Unit torus: majorRadius R=0.35, minorRadius r=0.15 (outer edge at 0.5)
    // by default -- AUD-061: majorRadius/minorRadius are now real parameters
    // (default-preserving the historical unit ratio) so callers building a
    // per-object-ratio mesh (getOrBuildTorusMesh()) can pass the object's
    // actual radii instead of relying on a post-hoc non-uniform scale, which
    // cannot correctly reproduce an arbitrary ratio (see
    // PrimitiveTessellationAlg.hpp's file header).
    const float R = majorRadius;
    const float r = minorRadius;
    const float pi2 = 2.0f * std::numbers::pi_v<float>;
    Color c(200, 200, 200, 255);

    // SYS-W7-02: vertex/index generation now lives in
    // tessellateUnitTorusAlg() (PrimitiveTessellationAlg.hpp) -- includes
    // the STAB-castle-fix winding swap ({a,d,b} / {a,c2,d}, not {a,b,d} /
    // {a,d,c2}) -- moved verbatim so it can be exercised headlessly for
    // differential testing against the exporter's independent
    // MeshBuilder.cpp::buildTorus(). This same index array is reused below
    // for the VPNT texIB.
    RawTessellation rtt = tessellateUnitTorusAlg(ringSeg, tubeSeg, R, r);
    std::vector<VertexPositionColor> verts;
    verts.reserve(rtt.positions.size());
    for (auto& p : rtt.positions) verts.push_back({ Vector3{p[0],p[1],p[2]}, c });
    std::vector<uint16_t> indices;
    indices.reserve(rtt.indices.size());
    for (auto idx : rtt.indices) indices.push_back(ui16(static_cast<int>(idx)));

    int nv = static_cast<int>(verts.size());
    int ni = static_cast<int>(indices.size());
    target.vb = std::make_unique<VertexBuffer>(device_, nv);
    target.vb->SetData(verts.data(), nv);
    target.ib = std::make_unique<IndexBuffer>(device_, ni);
    target.ib->SetData(indices.data(), ni);
    target.primitiveCount = ni / 3;
    storePositions(verts, target);

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
        target.texVB = std::make_unique<VertexBuffer>(device_, nv);
        target.texVB->SetData(tv.data(), nv);
        target.texIB = std::make_unique<IndexBuffer>(device_, ni);
        target.texIB->SetData(indices.data(), ni);
        target.texPrimitiveCount = ni / 3;
        target.texturedVertices = std::move(tv);
    }
}

void SceneRenderer::buildUnitCapsule(int segments, RenderMesh& target,
                                      float radius, float height) {
    // Unit capsule: radius=0.5, cylinder height=1.0, total height=2.0 (y=-1..+1)
    // by default. Bottom hemisphere center at y=-halfHeight, top at y=+halfHeight.
    //
    // AUD-061: radius/height are now real parameters (default-preserving the
    // historical unit values) so callers building a per-object mesh
    // (getOrBuildCapsuleMesh()) can pass the object's actual radius/height
    // instead of relying on a post-hoc non-uniform scale of one fixed-ratio
    // unit capsule, which stretches the hemisphere caps into ellipsoids
    // whenever height != 2*radius (see PrimitiveTessellationAlg.hpp's file
    // header).
    const float pi  = std::numbers::pi_v<float>;
    const float pi2 = 2.0f * pi;
    const float halfHeight = height * 0.5f;
    Color c(200, 200, 200, 255);

    // SYS-W7-02: vertex/index generation (poles, hemisphere rings, cylinder
    // seam ring, and the STAB-castle-fix {ra+i,rb+j,rb+i} / {ra+i,ra+j,rb+j}
    // winding) now lives in tessellateUnitCapsuleAlg()
    // (PrimitiveTessellationAlg.hpp), moved verbatim so it can be exercised
    // headlessly for differential testing against the exporter's
    // independent MeshBuilder.cpp::buildCapsule(). This same `indices`
    // array is copied verbatim into the VPNT `ti` array further below.
    RawTessellation rtcap = tessellateUnitCapsuleAlg(segments, radius, height);
    std::vector<VertexPositionColor> verts;
    verts.reserve(rtcap.positions.size());
    for (auto& p : rtcap.positions) verts.push_back({ Vector3{p[0],p[1],p[2]}, c });
    std::vector<uint16_t> indices;
    indices.reserve(rtcap.indices.size());
    for (auto idx : rtcap.indices) indices.push_back(ui16(static_cast<int>(idx)));

    target.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    target.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    target.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    target.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    target.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, target);

    // VPNT with normals (for textured rendering).
    //
    // AUD-061: was a position back-classification against hardcoded
    // radius=0.5-relative thresholds (`p.Y < -0.49f`, `abs(p.X) < 0.01f`,
    // etc.), which only worked for the fixed unit capsule. Replaced with
    // exact analytic per-ring normals computed the SAME way buildUnitTorus()
    // already does -- mirroring tessellateUnitCapsuleAlg()'s own ring-by-ring
    // construction (bottom pole, hRings bottom-hemisphere rings, one
    // cylinder-seam ring, hRings-1 top-hemisphere rings, top pole) so the
    // normal at each vertex is derived directly from the SAME (phi, sector)
    // parameters used to place it, not reverse-engineered from its final
    // Cartesian position. This is correct for any radius/height, not just
    // the historical 0.5/1.0 default, and removes the old formula's implicit
    // dependency on radius==0.5 entirely.
    {
        // Must match tessellateUnitCapsuleAlg() exactly: `tv` and the VPC
        // mesh share an index buffer, so a different ring count would make
        // the textured capsule use mismatched vertex/index topology.
        const int hRings = std::max(2, segments / 4);
        std::vector<VertexPositionNormalTexture> tv;
        tv.reserve(verts.size());

        auto addTexRing = [&](float y, float rXZ, float phi) {
            float cp = std::cos(phi), sp = std::sin(phi);
            for (int i = 0; i < segments; ++i) {
                float a = pi2 * i / segments;
                float cx = std::cos(a), cz = std::sin(a);
                Vector3 pos{ rXZ * cx, y, rXZ * cz };
                Vector3 norm{ cx * cp, sp, cz * cp };
                tv.push_back({ pos, norm, Vector2{ 0.5f, 0.5f } }); // simple UV, matching the historical value
            }
        };

        tv.push_back({ Vector3{0.0f, -(halfHeight + radius), 0.0f}, Vector3{0,-1,0}, Vector2{0.5f,0.5f} });
        for (int ri = 1; ri <= hRings; ++ri) {
            float phi = -pi / 2.0f + (pi / 2.0f) * float(ri) / hRings;
            addTexRing(-halfHeight + radius * std::sin(phi), radius * std::cos(phi), phi);
        }
        addTexRing(halfHeight, radius, 0.0f);
        for (int ri = 1; ri < hRings; ++ri) {
            float phi = (pi / 2.0f) * float(ri) / hRings;
            addTexRing(halfHeight + radius * std::sin(phi), radius * std::cos(phi), phi);
        }
        tv.push_back({ Vector3{0.0f, halfHeight + radius, 0.0f}, Vector3{0,1,0}, Vector2{0.5f,0.5f} });

        // Same topology as the VPC index buffer above (tv is built in the
        // identical botPole/ring/.../topPole order as `verts`, one vertex
        // per sector per ring, no seam duplicate -- see tessellateUnitCapsuleAlg()).
        target.texVB = std::make_unique<VertexBuffer>(device_, static_cast<int>(tv.size()));
        target.texVB->SetData(tv.data(), static_cast<int>(tv.size()));
        target.texIB = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
        target.texIB->SetData(indices.data(), static_cast<int>(indices.size()));
        target.texPrimitiveCount = static_cast<int>(indices.size()) / 3;
        target.texturedVertices = std::move(tv);
    }
}

void SceneRenderer::buildUnitIcoSphere(int subdivisions, RenderMesh& target) {
    // SYS-W7-02: icosahedron construction, subdivision, and the
    // STAB-castle-fix winding swap (f[0],f[2],f[1], not f[0],f[1],f[2]) now
    // live in tessellateUnitIcoSphereAlg() (PrimitiveTessellationAlg.hpp),
    // moved verbatim so it can be exercised headlessly for differential
    // testing against the exporter's independent
    // MeshBuilder.cpp::buildIcoSphere().
    RawTessellation rti = tessellateUnitIcoSphereAlg(subdivisions);

    Color c(200, 200, 200, 255);
    int nv = static_cast<int>(rti.positions.size());

    std::vector<VertexPositionColor> verts(nv);
    for (int i = 0; i < nv; ++i)
        verts[i] = { Vector3{rti.positions[i][0], rti.positions[i][1], rti.positions[i][2]}, c };

    std::vector<uint16_t> indices;
    indices.reserve(rti.indices.size());
    for (auto idx : rti.indices) indices.push_back(ui16(static_cast<int>(idx)));

    int ni = static_cast<int>(indices.size());
    target.vb = std::make_unique<VertexBuffer>(device_, nv);
    target.vb->SetData(verts.data(), nv);
    target.ib = std::make_unique<IndexBuffer>(device_, ni);
    target.ib->SetData(indices.data(), ni);
    target.primitiveCount = ni / 3;
    storePositions(verts, target);
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

    // Disk: outer circle + inner circle (at 50% ratio) + 4 radial lines in XZ plane
    {
        const float pi2 = 2.0f * std::numbers::pi_v<float>;
        // Outer circle
        for (int i = 0; i < segments; ++i) {
            float a0 = pi2 * i / segments, a1 = pi2 * (i+1) / segments;
            wireShapeDisk_.positions.push_back({0.5f*std::cos(a0), 0.0f, 0.5f*std::sin(a0)});
            wireShapeDisk_.positions.push_back({0.5f*std::cos(a1), 0.0f, 0.5f*std::sin(a1)});
            wireShapeDisk_.lineCount++;
        }
        // Inner circle at 50% radius
        for (int i = 0; i < segments; ++i) {
            float a0 = pi2 * i / segments, a1 = pi2 * (i+1) / segments;
            wireShapeDisk_.positions.push_back({0.25f*std::cos(a0), 0.0f, 0.25f*std::sin(a0)});
            wireShapeDisk_.positions.push_back({0.25f*std::cos(a1), 0.0f, 0.25f*std::sin(a1)});
            wireShapeDisk_.lineCount++;
        }
        // 4 radial spokes
        for (int q = 0; q < 4; ++q) {
            float a = pi2 * q / 4;
            wireShapeDisk_.positions.push_back({0.25f*std::cos(a), 0.0f, 0.25f*std::sin(a)});
            wireShapeDisk_.positions.push_back({0.50f*std::cos(a), 0.0f, 0.50f*std::sin(a)});
            wireShapeDisk_.lineCount++;
        }
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

    // Capsule (radius=0.5, cylinder height=1.0): equator ring + 4 meridian
    // arcs + cylinder edges.
    //
    // AUD-061: moved here, out of buildUnitCapsule(), which now also gets
    // called at the object's ACTUAL radius/height to build a per-object mesh
    // (getOrBuildCapsuleMesh()) -- wireShapeCapsule_ is a single shared
    // member (unlike the per-target RenderMesh outputs), so it must stay
    // built exactly once, at the fixed unit ratio, matching every other
    // shape's wireShape* member (all built here in buildWireShapes(), not in
    // their buildUnit*() mesh builders) -- otherwise a later per-object-ratio
    // buildUnitCapsule() call would silently overwrite the selection-outline
    // wire shape with the wrong (non-unit) dimensions.
    {
        const float pi  = std::numbers::pi_v<float>;
        const float pi2 = 2.0f * pi;
        // Equator at y=-0.5 and y=0.5 (cylinder rings)
        for (int bot = 0; bot < 2; ++bot) {
            float yw = bot ? -0.5f : 0.5f;
            for (int i = 0; i < segments; ++i) {
                float a0 = pi2 * i / segments;
                float a1 = pi2 * (i+1) / segments;
                wireShapeCapsule_.positions.push_back({0.5f*std::cos(a0), yw, 0.5f*std::sin(a0)});
                wireShapeCapsule_.positions.push_back({0.5f*std::cos(a1), yw, 0.5f*std::sin(a1)});
                wireShapeCapsule_.lineCount++;
            }
        }
        // 4 vertical cylinder edges
        for (int q = 0; q < 4; ++q) {
            float a = pi2 * q / 4;
            float x = 0.5f * std::cos(a), z = 0.5f * std::sin(a);
            wireShapeCapsule_.positions.push_back({x, -0.5f, z});
            wireShapeCapsule_.positions.push_back({x,  0.5f, z});
            wireShapeCapsule_.lineCount++;
        }
        // 2 hemisphere arcs (XZ and YZ planes)
        for (int plane = 0; plane < 2; ++plane) {
            for (int cap = 0; cap < 2; ++cap) {
                float yOff = cap ? -0.5f : 0.5f;
                float ySign = cap ? -1.0f : 1.0f;
                for (int i = 0; i < segments / 2; ++i) {
                    float phi0 = pi * i / (segments / 2);
                    float phi1 = pi * (i+1) / (segments / 2);
                    float y0 = yOff + 0.5f * std::sin(phi0) * ySign;
                    float y1 = yOff + 0.5f * std::sin(phi1) * ySign;
                    float r0 = 0.5f * std::cos(phi0);
                    float r1 = 0.5f * std::cos(phi1);
                    if (plane == 0) {
                        wireShapeCapsule_.positions.push_back({r0, y0, 0.0f});
                        wireShapeCapsule_.positions.push_back({r1, y1, 0.0f});
                    } else {
                        wireShapeCapsule_.positions.push_back({0.0f, y0, r0});
                        wireShapeCapsule_.positions.push_back({0.0f, y1, r1});
                    }
                    wireShapeCapsule_.lineCount++;
                }
            }
        }
    }

    // Grid wire shape: 1×1 unit XZ plane with 4×4 subdivisions (5 lines each axis)
    {
        const int N = 4; // subdivisions per axis
        for (int i = 0; i <= N; ++i) {
            float t = -0.5f + static_cast<float>(i) / N;
            // Horizontal line (parallel to X axis, constant Z)
            wireShapeGrid_.positions.push_back({-0.5f, 0.0f, t});
            wireShapeGrid_.positions.push_back({ 0.5f, 0.0f, t});
            wireShapeGrid_.lineCount++;
            // Vertical line (parallel to Z axis, constant X)
            wireShapeGrid_.positions.push_back({t, 0.0f, -0.5f});
            wireShapeGrid_.positions.push_back({t, 0.0f,  0.5f});
            wireShapeGrid_.lineCount++;
        }
    }
}

} // namespace MeshCraft::Renderer
