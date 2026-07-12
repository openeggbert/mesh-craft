#pragma once
// Pure CPU-side primitive tessellation -- no CNA / ImGui / SDL / OpenGL
// dependencies. Extracted from SceneRenderer_Builders.cpp's buildUnit*()
// family (SYS-W7-02) so the exact vertex/index topology the viewport
// renderer draws can be exercised headlessly, without a live GraphicsDevice,
// for differential testing against mc3togltf's independent
// MeshBuilder.cpp tessellator.
//
// Each function here reproduces -- verbatim, not re-derived -- the
// VertexPositionColor (position-only) generation loops that used to live
// inline in the corresponding SceneRenderer::buildUnit*() method. Those
// methods now call into here to get positions+indices, then proceed to
// upload to the GPU and build the separate VPNT (normals/UV) variant exactly
// as before -- this header changes WHERE the math lives, not what it
// computes, so it does not alter rendered output.
//
// Most shapes are generated at the SAME "unit" size SceneRenderer has always
// used internally (box 1x1x1, sphere/cylinder/cone radius 0.5, icosphere
// radius 0.5) -- the object's real dimensions are applied afterwards via a
// per-primitive scale matrix (see SceneRenderer.cpp's drawObject() switch
// statement).
//
// AUD-061: Torus and Capsule are the exception. A single non-uniform affine
// scale of one fixed-ratio unit mesh cannot correctly reproduce an arbitrary
// (majorRadius, minorRadius) torus or (radius, height) capsule -- the two
// radii/the hemisphere-vs-cylinder split get conflated by the scale, so any
// object whose ratio differs from the unit mesh's own produces a visibly
// wrong (elliptical-cross-section tube / ellipsoidal-cap) shape. So
// tessellateUnitTorusAlg()/tessellateUnitCapsuleAlg() take the actual
// majorRadius/minorRadius (Torus) or radius/height (Capsule) as parameters
// (defaulting to the historical unit-mesh values for full backward
// compatibility) and SceneRenderer builds a real mesh per distinct
// (ratio, LOD-tier) combination instead of scaling one fixed unit mesh --
// see SceneRenderer::getOrBuildTorusMesh()/getOrBuildCapsuleMesh() and their
// small bounded caches in SceneRenderer.cpp.
//
// Included by SceneRenderer_Builders.cpp and by
// test/differential_geometry_test.cpp.

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <utility>
#include <vector>

namespace MeshCraft::Renderer {

// MC3 stores IcoSphere detail in its generic `segments` field, while the
// tessellator uses subdivision levels. Keep this conversion at the
// CNA-free boundary so the renderer's mesh selection is directly covered by
// the differential test as well as matching the exporter contract.
inline int icoSphereSubdivisionsForSegmentsAlg(int segments) {
    return std::clamp(segments / 8, 1, 4);
}

// Plain CPU mesh: triangle-list indices into `positions`. No normals/UVs --
// callers that need the textured (VPNT) variant still generate it
// separately, as SceneRenderer_Builders.cpp always has.
struct RawTessellation {
    std::vector<std::array<float, 3>> positions;
    std::vector<uint32_t>             indices; // triangle list, 3 per face
};

// ---------------------------------------------------------------------------
// Box -- unit cube (1x1x1) centered at origin. 8 verts, 12 triangles.
// Verbatim from SceneRenderer::buildUnitBox()'s VertexPositionColor block.
// ---------------------------------------------------------------------------
inline RawTessellation tessellateUnitBoxAlg() {
    RawTessellation rt;
    rt.positions = {
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
        { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
        { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    };
    rt.indices = {
        0,1,2, 0,2,3,  // -Z
        4,6,5, 4,7,6,  // +Z
        0,5,1, 0,4,5,  // -Y
        2,7,3, 2,6,7,  // +Y
        0,7,4, 0,3,7,  // -X
        1,6,2, 1,5,6,  // +X
    };
    return rt;
}

// ---------------------------------------------------------------------------
// Sphere (UV sphere) -- unit radius 0.5. Verbatim from
// SceneRenderer::buildUnitSphere()'s VertexPositionColor block.
// ---------------------------------------------------------------------------
inline RawTessellation tessellateUnitSphereAlg(int segments) {
    RawTessellation rt;
    int rings   = segments / 2;
    int sectors = segments;

    for (int r = 0; r <= rings; ++r) {
        float phi = std::numbers::pi_v<float> * r / rings;
        for (int s = 0; s <= sectors; ++s) {
            float theta = 2.0f * std::numbers::pi_v<float> * s / sectors;
            float x = std::sin(phi) * std::cos(theta) * 0.5f;
            float y = std::cos(phi) * 0.5f;
            float z = std::sin(phi) * std::sin(theta) * 0.5f;
            rt.positions.push_back({x, y, z});
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            int a = r * (sectors+1) + s;
            int b = a + 1;
            int c2 = (r+1) * (sectors+1) + s;
            int d  = c2 + 1;
            rt.indices.push_back(static_cast<uint32_t>(a));
            rt.indices.push_back(static_cast<uint32_t>(c2));
            rt.indices.push_back(static_cast<uint32_t>(b));
            rt.indices.push_back(static_cast<uint32_t>(b));
            rt.indices.push_back(static_cast<uint32_t>(c2));
            rt.indices.push_back(static_cast<uint32_t>(d));
        }
    }
    return rt;
}

// ---------------------------------------------------------------------------
// Cylinder -- unit radius 0.5, unit height 1 (y = -0.5..0.5). Verbatim from
// SceneRenderer::buildUnitCylinder()'s VertexPositionColor block.
// ---------------------------------------------------------------------------
inline RawTessellation tessellateUnitCylinderAlg(int segments) {
    RawTessellation rt;
    auto& verts = rt.positions;

    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * std::numbers::pi_v<float> * i / segments;
        float x = 0.5f * std::cos(a), z = 0.5f * std::sin(a);
        verts.push_back({x, -0.5f, z});  // bottom ring
    }
    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * std::numbers::pi_v<float> * i / segments;
        float x = 0.5f * std::cos(a), z = 0.5f * std::sin(a);
        verts.push_back({x, 0.5f, z});   // top ring
    }
    int botCenter = static_cast<int>(verts.size());
    verts.push_back({0,-0.5f,0});
    int topCenter = static_cast<int>(verts.size());
    verts.push_back({0, 0.5f,0});

    auto push3 = [&](int a, int b, int c) {
        rt.indices.push_back(static_cast<uint32_t>(a));
        rt.indices.push_back(static_cast<uint32_t>(b));
        rt.indices.push_back(static_cast<uint32_t>(c));
    };
    for (int i = 0; i < segments; ++i) {
        int j = (i+1) % segments;
        // Side quad
        push3(i, i+segments, j+segments);
        push3(i, j+segments, j);
        // Bottom cap
        push3(botCenter, j, i);
        // Top cap
        push3(topCenter, i+segments, j+segments);
    }
    return rt;
}

// ---------------------------------------------------------------------------
// Cone -- unit radius 0.5, unit height 1 (base y=-0.5, apex y=0.5). Verbatim
// from SceneRenderer::buildUnitCone()'s VertexPositionColor block.
// ---------------------------------------------------------------------------
inline RawTessellation tessellateUnitConeAlg(int segments) {
    RawTessellation rt;
    auto& verts = rt.positions;

    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * std::numbers::pi_v<float> * i / segments;
        verts.push_back({0.5f*std::cos(a), -0.5f, 0.5f*std::sin(a)});
    }
    int apex   = static_cast<int>(verts.size());
    verts.push_back({0, 0.5f, 0});
    int botCtr = static_cast<int>(verts.size());
    verts.push_back({0,-0.5f,0});

    auto push3 = [&](int a, int b, int c) {
        rt.indices.push_back(static_cast<uint32_t>(a));
        rt.indices.push_back(static_cast<uint32_t>(b));
        rt.indices.push_back(static_cast<uint32_t>(c));
    };
    for (int i = 0; i < segments; ++i) {
        int j = (i+1) % segments;
        push3(i, j, apex);          // side
        push3(botCtr, j, i);        // bottom cap
    }
    return rt;
}

// ---------------------------------------------------------------------------
// Plane -- unit 1x1 quad in the XZ plane, y=0. Verbatim from
// SceneRenderer::buildUnitPlane()'s VertexPositionColor block. Open surface
// (no volume) -- not watertight by construction, unlike the other shapes.
// ---------------------------------------------------------------------------
inline RawTessellation tessellateUnitPlaneAlg() {
    RawTessellation rt;
    rt.positions = {
        {-0.5f, 0.0f, -0.5f},
        { 0.5f, 0.0f, -0.5f},
        { 0.5f, 0.0f,  0.5f},
        {-0.5f, 0.0f,  0.5f},
    };
    rt.indices = {0,1,2, 0,2,3};
    return rt;
}

// ---------------------------------------------------------------------------
// Torus -- majorRadius R=0.35, minorRadius r=0.15 (outer edge at 0.5) by
// default. Verbatim from SceneRenderer::buildUnitTorus()'s
// VertexPositionColor block; majorRadius/minorRadius were extracted to
// parameters (AUD-061) so the SAME formula can build a torus at any
// object's actual (majorRadius, minorRadius) ratio, not just the fixed
// default -- calling with no radius arguments reproduces the original unit
// mesh exactly (behavior-preserving default).
// ---------------------------------------------------------------------------
inline RawTessellation tessellateUnitTorusAlg(int ringSeg, int tubeSeg,
                                               float majorRadius = 0.35f,
                                               float minorRadius = 0.15f) {
    RawTessellation rt;
    const float R = majorRadius;
    const float r = minorRadius;
    const float pi2 = 2.0f * std::numbers::pi_v<float>;

    rt.positions.reserve(static_cast<size_t>(ringSeg+1)*(tubeSeg+1));
    rt.indices.reserve(static_cast<size_t>(ringSeg)*tubeSeg*6);

    for (int i = 0; i <= ringSeg; ++i) {
        float theta = pi2 * i / ringSeg;
        float ct = std::cos(theta), st = std::sin(theta);
        for (int j = 0; j <= tubeSeg; ++j) {
            float phi = pi2 * j / tubeSeg;
            float cp = std::cos(phi), sp = std::sin(phi);
            float x = (R + r * cp) * ct;
            float y = r * sp;
            float z = (R + r * cp) * st;
            rt.positions.push_back({x, y, z});
        }
    }
    for (int i = 0; i < ringSeg; ++i) {
        for (int j = 0; j < tubeSeg; ++j) {
            int a = i * (tubeSeg+1) + j;
            int b = a + 1;
            int c2 = (i+1) * (tubeSeg+1) + j;
            int d  = c2 + 1;
            rt.indices.push_back(static_cast<uint32_t>(a));
            rt.indices.push_back(static_cast<uint32_t>(d));
            rt.indices.push_back(static_cast<uint32_t>(b));
            rt.indices.push_back(static_cast<uint32_t>(a));
            rt.indices.push_back(static_cast<uint32_t>(c2));
            rt.indices.push_back(static_cast<uint32_t>(d));
        }
    }
    return rt;
}

// ---------------------------------------------------------------------------
// Capsule -- unit radius 0.5, cylinder section height 1 (total height 2,
// y=-1..+1) by default. Verbatim from SceneRenderer::buildUnitCapsule()'s
// VertexPositionColor block; radius/height (the cylinder-section height, as
// Mc3Primitive::height means for Capsule -- matching
// mc3togltf::buildCapsule()'s `hh = height * 0.5f`) were extracted to
// parameters (AUD-061) so the SAME formula can build a capsule at any
// object's actual (radius, height) ratio, not just the fixed default --
// calling with no radius/height arguments reproduces the original unit mesh
// exactly (behavior-preserving default: radius=0.5, height=1.0 -> total
// height 2, y=-1..+1).
// ---------------------------------------------------------------------------
inline RawTessellation tessellateUnitCapsuleAlg(int segments, float radius = 0.5f, float height = 1.0f) {
    RawTessellation rt;
    auto& verts = rt.positions;

    const int hRings = std::max(4, segments / 4);
    const float pi  = std::numbers::pi_v<float>;
    const float pi2 = 2.0f * pi;
    const float halfHeight = height * 0.5f;

    auto addRing = [&](float y, float rXZ) {
        int base = static_cast<int>(verts.size());
        for (int i = 0; i < segments; ++i) {
            float a = pi2 * i / segments;
            verts.push_back({rXZ * std::cos(a), y, rXZ * std::sin(a)});
        }
        return base;
    };

    std::vector<int> ringBases;

    int botPole = static_cast<int>(verts.size());
    verts.push_back({0.0f, -(halfHeight + radius), 0.0f});
    for (int ri = 1; ri <= hRings; ++ri) {
        float phi = -pi / 2.0f + (pi / 2.0f) * float(ri) / hRings;
        float y   = -halfHeight + radius * std::sin(phi);
        float r   =  radius * std::cos(phi);
        ringBases.push_back(addRing(y, r));
    }

    ringBases.push_back(addRing(halfHeight, radius));

    for (int ri = 1; ri < hRings; ++ri) {
        float phi = (pi / 2.0f) * float(ri) / hRings;
        float y   =  halfHeight + radius * std::sin(phi);
        float r   =  radius * std::cos(phi);
        ringBases.push_back(addRing(y, r));
    }
    int topPole = static_cast<int>(verts.size());
    verts.push_back({0.0f, halfHeight + radius, 0.0f});

    auto push3 = [&](int a, int b, int c) {
        rt.indices.push_back(static_cast<uint32_t>(a));
        rt.indices.push_back(static_cast<uint32_t>(b));
        rt.indices.push_back(static_cast<uint32_t>(c));
    };

    {
        int rb = ringBases[0];
        for (int i = 0; i < segments; ++i) {
            int j = (i + 1) % segments;
            push3(botPole, rb + j, rb + i);
        }
    }
    for (int r0 = 0; r0 + 1 < static_cast<int>(ringBases.size()); ++r0) {
        int ra = ringBases[r0], rb = ringBases[r0 + 1];
        for (int i = 0; i < segments; ++i) {
            int j = (i + 1) % segments;
            push3(ra + i, rb + j, rb + i);
            push3(ra + i, ra + j, rb + j);
        }
    }
    {
        int rt_ = ringBases.back();
        for (int i = 0; i < segments; ++i) {
            int j = (i + 1) % segments;
            push3(topPole, rt_ + i, rt_ + j);
        }
    }
    return rt;
}

// ---------------------------------------------------------------------------
// IcoSphere -- unit radius 0.5. Verbatim from
// SceneRenderer::buildUnitIcoSphere()'s vertex/index generation.
// ---------------------------------------------------------------------------
inline RawTessellation tessellateUnitIcoSphereAlg(int subdivisions) {
    const float phi = (1.0f + std::sqrt(5.0f)) / 2.0f;

    auto norm05 = [](float x, float y, float z) -> std::array<float,3> {
        float len = std::sqrt(x*x + y*y + z*z);
        return {0.5f*x/len, 0.5f*y/len, 0.5f*z/len};
    };

    const float p = phi;
    std::vector<std::array<float,3>> pos = {
        norm05(-1, p, 0), norm05( 1, p, 0), norm05(-1,-p, 0), norm05( 1,-p, 0),
        norm05( 0,-1, p), norm05( 0, 1, p), norm05( 0,-1,-p), norm05( 0, 1,-p),
        norm05( p, 0,-1), norm05( p, 0, 1), norm05(-p, 0,-1), norm05(-p, 0, 1),
    };

    std::vector<std::array<int,3>> faces = {
        {0,11,5}, {0,5,1}, {0,1,7}, {0,7,10}, {0,10,11},
        {1,5,9},  {5,11,4},{11,10,2},{10,7,6}, {7,1,8},
        {3,9,4},  {3,4,2}, {3,2,6}, {3,6,8},  {3,8,9},
        {4,9,5},  {2,4,11},{6,2,10},{8,6,7},   {9,8,1},
    };

    std::map<std::pair<int,int>, int> midCache;
    for (int d = 0; d < subdivisions; ++d) {
        midCache.clear();
        std::vector<std::array<int,3>> newFaces;
        newFaces.reserve(faces.size() * 4);

        auto getMid = [&](int a, int b) -> int {
            auto key = std::make_pair(std::min(a,b), std::max(a,b));
            auto it = midCache.find(key);
            if (it != midCache.end()) return it->second;
            const auto& pa = pos[a];
            const auto& pb = pos[b];
            float mx = (pa[0]+pb[0]) * 0.5f;
            float my = (pa[1]+pb[1]) * 0.5f;
            float mz = (pa[2]+pb[2]) * 0.5f;
            float len = std::sqrt(mx*mx+my*my+mz*mz);
            int idx = static_cast<int>(pos.size());
            pos.push_back({0.5f*mx/len, 0.5f*my/len, 0.5f*mz/len});
            midCache[key] = idx;
            return idx;
        };

        for (auto& f : faces) {
            int m01 = getMid(f[0], f[1]);
            int m12 = getMid(f[1], f[2]);
            int m20 = getMid(f[2], f[0]);
            newFaces.push_back({f[0], m01, m20});
            newFaces.push_back({f[1], m12, m01});
            newFaces.push_back({f[2], m20, m12});
            newFaces.push_back({m01, m12, m20});
        }
        faces = std::move(newFaces);
    }

    RawTessellation rt;
    rt.positions = std::move(pos);
    rt.indices.reserve(faces.size() * 3);
    for (auto& f : faces) {
        rt.indices.push_back(static_cast<uint32_t>(f[0]));
        rt.indices.push_back(static_cast<uint32_t>(f[2]));
        rt.indices.push_back(static_cast<uint32_t>(f[1]));
    }
    return rt;
}

} // namespace MeshCraft::Renderer
