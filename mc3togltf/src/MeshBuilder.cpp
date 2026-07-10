#include "MeshBuilder.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <numbers>
#include <stdexcept>
#include <tuple>

namespace mc3togltf {

// ---------------------------------------------------------------------------
// MeshData helpers
// ---------------------------------------------------------------------------

void MeshData::applyScale(float sx, float sy, float sz) {
    for (size_t i = 0; i < positions.size(); i += 3) {
        positions[i]   *= sx;
        positions[i+1] *= sy;
        positions[i+2] *= sz;
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Append one face quad (4 vertices, 2 triangles) to MeshData.
// CCW winding when viewed from the direction of 'n'.
static void addQuad(MeshData& m,
                    std::array<float,3> v0, std::array<float,3> v1,
                    std::array<float,3> v2, std::array<float,3> v3,
                    std::array<float,3> n,
                    std::array<float,2> uv0 = {0,0},
                    std::array<float,2> uv1 = {1,0},
                    std::array<float,2> uv2 = {1,1},
                    std::array<float,2> uv3 = {0,1})
{
    auto base = static_cast<uint32_t>(m.vertexCount());
    for (auto& v : {v0, v1, v2, v3}) {
        m.positions.insert(m.positions.end(), v.begin(), v.end());
        m.normals.insert(m.normals.end(), n.begin(), n.end());
    }
    m.texcoords.insert(m.texcoords.end(), {uv0[0], uv0[1]});
    m.texcoords.insert(m.texcoords.end(), {uv1[0], uv1[1]});
    m.texcoords.insert(m.texcoords.end(), {uv2[0], uv2[1]});
    m.texcoords.insert(m.texcoords.end(), {uv3[0], uv3[1]});
    m.indices.insert(m.indices.end(), {base, base+1, base+2,  base, base+2, base+3});
}

// Apply axis remapping so that the generated mesh uses the requested axis as height.
static void remapAxis(MeshData& m, const std::string& axis) {
    if (axis == "y") return; // default — no change
    for (size_t i = 0; i < m.positions.size(); i += 3) {
        float x = m.positions[i], y = m.positions[i+1], z = m.positions[i+2];
        float nx = m.normals[i],  ny = m.normals[i+1],  nz = m.normals[i+2];
        if (axis == "x") {
            // y → x, -x → y: rotate -90° around Z
            m.positions[i] = y; m.positions[i+1] = -x; m.positions[i+2] = z;
            m.normals[i]   = ny; m.normals[i+1]  = -nx; m.normals[i+2]  = nz;
        } else if (axis == "z") {
            // y → z, -z → y: rotate +90° around X
            m.positions[i] = x; m.positions[i+1] = -z; m.positions[i+2] = y;
            m.normals[i]   = nx; m.normals[i+1]  = -nz; m.normals[i+2]  = ny;
        }
    }
}

// ---------------------------------------------------------------------------
// Box
// ---------------------------------------------------------------------------

MeshData buildBox(float w, float h, float d) {
    MeshData m;
    float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;

    // CCW winding verified with (v1-v0)×(v2-v0) · normal > 0 for each face
    addQuad(m, {hw,-hh, hd}, {hw,-hh,-hd}, {hw, hh,-hd}, {hw, hh, hd}, { 1, 0, 0}); // +X
    addQuad(m, {-hw,-hh,-hd},{-hw,-hh, hd},{-hw, hh, hd},{-hw, hh,-hd}, {-1, 0, 0}); // -X
    addQuad(m, {-hw, hh,-hd},{-hw, hh, hd},{ hw, hh, hd},{ hw, hh,-hd}, { 0, 1, 0}); // +Y
    addQuad(m, {-hw,-hh, hd},{-hw,-hh,-hd},{ hw,-hh,-hd},{ hw,-hh, hd}, { 0,-1, 0}); // -Y
    addQuad(m, {-hw,-hh, hd},{ hw,-hh, hd},{ hw, hh, hd},{-hw, hh, hd}, { 0, 0, 1}); // +Z
    addQuad(m, { hw,-hh,-hd},{-hw,-hh,-hd},{-hw, hh,-hd},{ hw, hh,-hd}, { 0, 0,-1}); // -Z

    return m;
}

// ---------------------------------------------------------------------------
// Sphere (UV sphere)
// ---------------------------------------------------------------------------

MeshData buildSphere(float radius, int segments) {
    MeshData m;
    int rings = segments / 2;
    int sectors = segments;
    const float pi = std::numbers::pi_v<float>;

    // vertices (rings+1) * (sectors+1)
    for (int r = 0; r <= rings; ++r) {
        float phi   = pi * r / rings;          // 0..pi (top to bottom)
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int s = 0; s <= sectors; ++s) {
            float theta = 2.0f * pi * s / sectors; // 0..2pi
            float x = std::cos(theta) * sinPhi;
            float y = cosPhi;
            float z = std::sin(theta) * sinPhi;

            m.positions.insert(m.positions.end(), {x*radius, y*radius, z*radius});
            m.normals.insert(m.normals.end(), {x, y, z});
            m.texcoords.insert(m.texcoords.end(), {
                static_cast<float>(s) / sectors,
                static_cast<float>(r) / rings
            });
        }
    }

    // indices
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            auto i0 = static_cast<uint32_t>(r * (sectors+1) + s);
            auto i1 = i0 + 1;
            auto i2 = i0 + (sectors+1);
            auto i3 = i2 + 1;
            m.indices.insert(m.indices.end(), {i0, i2, i1,  i1, i2, i3});
        }
    }
    return m;
}

// ---------------------------------------------------------------------------
// Cylinder
// ---------------------------------------------------------------------------

MeshData buildCylinder(float radius, float height, int segments, const std::string& axis) {
    MeshData m;
    const float pi = std::numbers::pi_v<float>;
    float hh = height * 0.5f;

    // Side
    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * pi * i / segments;
        float a1 = 2.0f * pi * (i+1) / segments;

        float c0 = std::cos(a0), s0 = std::sin(a0);
        float c1 = std::cos(a1), s1 = std::sin(a1);

        // v0=bottom_i, v1=top_i, v2=top_{i+1}, v3=bottom_{i+1}  (CCW from outside)
        std::array<float,3> b0 = {c0*radius, -hh, s0*radius};
        std::array<float,3> t0 = {c0*radius,  hh, s0*radius};
        std::array<float,3> t1 = {c1*radius,  hh, s1*radius};
        std::array<float,3> b1 = {c1*radius, -hh, s1*radius};

        float u0 = static_cast<float>(i)   / segments;
        float u1 = static_cast<float>(i+1) / segments;

        auto base = static_cast<uint32_t>(m.vertexCount());
        for (auto& [v, nx, ny, nz, u, vt] : std::initializer_list<std::tuple<
                std::array<float,3>, float,float,float, float,float>>{
                {b0, c0,0,s0, u0,0},
                {t0, c0,0,s0, u0,1},
                {t1, c1,0,s1, u1,1},
                {b1, c1,0,s1, u1,0}}) {
            m.positions.insert(m.positions.end(), v.begin(), v.end());
            m.normals.insert(m.normals.end(), {nx, ny, nz});
            m.texcoords.insert(m.texcoords.end(), {u, vt});
        }
        m.indices.insert(m.indices.end(), {base, base+1, base+2,  base, base+2, base+3});
    }

    // Top cap (+Y, normal (0,1,0))
    {
        auto center = static_cast<uint32_t>(m.vertexCount());
        m.positions.insert(m.positions.end(), {0, hh, 0});
        m.normals.insert(m.normals.end(), {0,1,0});
        m.texcoords.insert(m.texcoords.end(), {0.5f, 0.5f});

        uint32_t rimBase = center + 1;
        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * pi * i / segments;
            float c = std::cos(a), s = std::sin(a);
            m.positions.insert(m.positions.end(), {c*radius, hh, s*radius});
            m.normals.insert(m.normals.end(), {0,1,0});
            m.texcoords.insert(m.texcoords.end(), {0.5f+c*0.5f, 0.5f+s*0.5f});
        }
        for (int i = 0; i < segments; ++i) {
            // CCW from above: center, rim[i+1], rim[i]
            m.indices.insert(m.indices.end(), {center, rimBase+i+1, rimBase+i});
        }
    }

    // Bottom cap (-Y, normal (0,-1,0))
    {
        auto center = static_cast<uint32_t>(m.vertexCount());
        m.positions.insert(m.positions.end(), {0, -hh, 0});
        m.normals.insert(m.normals.end(), {0,-1,0});
        m.texcoords.insert(m.texcoords.end(), {0.5f, 0.5f});

        uint32_t rimBase = center + 1;
        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * pi * i / segments;
            float c = std::cos(a), s = std::sin(a);
            m.positions.insert(m.positions.end(), {c*radius, -hh, s*radius});
            m.normals.insert(m.normals.end(), {0,-1,0});
            m.texcoords.insert(m.texcoords.end(), {0.5f+c*0.5f, 0.5f+s*0.5f});
        }
        for (int i = 0; i < segments; ++i) {
            // CCW from below: center, rim[i], rim[i+1]
            m.indices.insert(m.indices.end(), {center, rimBase+i, rimBase+i+1});
        }
    }

    remapAxis(m, axis);
    return m;
}

// ---------------------------------------------------------------------------
// Cone
// ---------------------------------------------------------------------------

MeshData buildCone(float radius, float height, int segments) {
    MeshData m;
    const float pi = std::numbers::pi_v<float>;
    float hh = height * 0.5f;

    // Side: from rim at -hh to apex at +hh
    // Slant normal: outward at angle
    float slopeLen = std::sqrt(radius*radius + height*height);
    // STAB-0668: a degenerate cone (radius==0 and height==0) makes
    // slopeLen==0, so ny/nr below would be a 0/0 NaN -- fall back to a
    // straight-up normal (arbitrary but finite) rather than propagating
    // NaN into the exported NORMAL accessor.
    float ny = slopeLen > 1e-8f ? radius / slopeLen : 1.0f;
    float nr = slopeLen > 1e-8f ? height / slopeLen : 0.0f; // radial component of normal

    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * pi * i / segments;
        float a1 = 2.0f * pi * (i+1) / segments;
        float c0 = std::cos(a0), s0 = std::sin(a0);
        float c1 = std::cos(a1), s1 = std::sin(a1);

        auto apex = static_cast<uint32_t>(m.vertexCount());
        // Apex (shared normal = average of its two edges)
        float amidC = std::cos((a0+a1)*0.5f), amidS = std::sin((a0+a1)*0.5f);
        m.positions.insert(m.positions.end(), {0, hh, 0});
        m.normals.insert(m.normals.end(), {amidC*nr, ny, amidS*nr});
        m.texcoords.insert(m.texcoords.end(), {(static_cast<float>(i)+0.5f)/segments, 1});

        m.positions.insert(m.positions.end(), {c0*radius, -hh, s0*radius});
        m.normals.insert(m.normals.end(), {c0*nr, ny, s0*nr});
        m.texcoords.insert(m.texcoords.end(), {static_cast<float>(i)/segments, 0});

        m.positions.insert(m.positions.end(), {c1*radius, -hh, s1*radius});
        m.normals.insert(m.normals.end(), {c1*nr, ny, s1*nr});
        m.texcoords.insert(m.texcoords.end(), {static_cast<float>(i+1)/segments, 0});

        m.indices.insert(m.indices.end(), {apex, apex+1, apex+2});
    }

    // Bottom cap
    {
        auto center = static_cast<uint32_t>(m.vertexCount());
        m.positions.insert(m.positions.end(), {0, -hh, 0});
        m.normals.insert(m.normals.end(), {0,-1,0});
        m.texcoords.insert(m.texcoords.end(), {0.5f, 0.5f});

        uint32_t rimBase = center + 1;
        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * pi * i / segments;
            float c = std::cos(a), s = std::sin(a);
            m.positions.insert(m.positions.end(), {c*radius, -hh, s*radius});
            m.normals.insert(m.normals.end(), {0,-1,0});
            m.texcoords.insert(m.texcoords.end(), {0.5f+c*0.5f, 0.5f+s*0.5f});
        }
        for (int i = 0; i < segments; ++i)
            m.indices.insert(m.indices.end(), {center, rimBase+i, rimBase+i+1});
    }

    return m;
}

// ---------------------------------------------------------------------------
// Plane
// ---------------------------------------------------------------------------

MeshData buildPlane(float w, float d, const std::string& axis) {
    MeshData m;
    float hw = w * 0.5f, hd = d * 0.5f;
    // Flat quad in XZ plane (normal = +Y)
    addQuad(m,
        {-hw, 0, -hd}, {-hw, 0, hd}, {hw, 0, hd}, {hw, 0, -hd},
        {0, 1, 0},
        {0,0}, {0,1}, {1,1}, {1,0});
    remapAxis(m, axis);
    return m;
}

// ---------------------------------------------------------------------------
// Torus
// ---------------------------------------------------------------------------

MeshData buildTorus(float majorRadius, float minorRadius, int segments) {
    MeshData m;
    const float pi = std::numbers::pi_v<float>;
    int rings = segments;
    int sides = std::max(4, segments / 2);
    int rowSize = sides + 1;

    for (int r = 0; r <= rings; ++r) {
        float theta = 2.0f * pi * r / rings;
        float cosT = std::cos(theta), sinT = std::sin(theta);

        for (int s = 0; s <= sides; ++s) {
            float phi = 2.0f * pi * s / sides;
            float cosP = std::cos(phi), sinP = std::sin(phi);

            float x = (majorRadius + minorRadius * cosP) * cosT;
            float y = minorRadius * sinP;
            float z = (majorRadius + minorRadius * cosP) * sinT;

            m.positions.insert(m.positions.end(), {x, y, z});
            m.normals.insert(m.normals.end(), {cosP * cosT, sinP, cosP * sinT});
            m.texcoords.insert(m.texcoords.end(), {float(r)/rings, float(s)/sides});
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sides; ++s) {
            uint32_t i0 = uint32_t(r * rowSize + s);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = uint32_t((r+1) * rowSize + s);
            uint32_t i3 = i2 + 1;
            m.indices.insert(m.indices.end(), {i0, i2, i1,  i1, i2, i3});
        }
    }
    return m;
}

// ---------------------------------------------------------------------------
// Capsule
// ---------------------------------------------------------------------------

MeshData buildCapsule(float radius, float height, int segments, const std::string& axis) {
    MeshData m;
    const float pi = std::numbers::pi_v<float>;
    int rings   = std::max(2, segments / 4);  // per hemisphere
    int sectors = segments;
    float hh    = height * 0.5f;

    // rows 0..rings = top hemisphere (phi 0..π/2, shifted +hh)
    // rows rings+1..2*rings+1 = bottom hemisphere (phi π/2..π, shifted -hh)
    int totalRows = 2 * rings + 2;
    int rowSize   = sectors + 1;

    for (int r = 0; r < totalRows; ++r) {
        float phi, yOff;
        if (r <= rings) {
            phi  = (pi * 0.5f) * float(r) / rings;
            yOff = +hh;
        } else {
            // STAB-0662: was `(r - rings)`, which skips phi==pi/2 entirely
            // for the bottom hemisphere's first row (jumping straight past
            // the cylinder-body seam ring) and overshoots phi==pi (past the
            // pole) on the last row -- the capsule's cylindrical mid-section
            // was missing and the bottom pole didn't close. `r - rings - 1`
            // makes the bottom hemisphere's own phi sweep exactly [0, pi/2]
            // over `rings` steps (mirroring the top hemisphere), so row
            // rings+1 is the bottom cylinder seam (phi=pi/2) and the last
            // row is the exact pole (phi=pi).
            phi  = (pi * 0.5f) + (pi * 0.5f) * float(r - rings - 1) / rings;
            yOff = -hh;
        }
        float sinP = std::sin(phi), cosP = std::cos(phi);

        for (int s = 0; s <= sectors; ++s) {
            float theta = 2.0f * pi * s / sectors;
            float cosT = std::cos(theta), sinT = std::sin(theta);

            float nx = cosT * sinP, ny = cosP, nz = sinT * sinP;
            m.positions.insert(m.positions.end(), {nx*radius, ny*radius + yOff, nz*radius});
            m.normals.insert(m.normals.end(), {nx, ny, nz});
            m.texcoords.insert(m.texcoords.end(), {float(s)/sectors, float(r)/(totalRows-1)});
        }
    }

    for (int r = 0; r < totalRows - 1; ++r) {
        for (int s = 0; s < sectors; ++s) {
            uint32_t i0 = uint32_t(r * rowSize + s);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = uint32_t((r+1) * rowSize + s);
            uint32_t i3 = i2 + 1;
            m.indices.insert(m.indices.end(), {i0, i2, i1,  i1, i2, i3});
        }
    }

    remapAxis(m, axis);
    return m;
}

// ---------------------------------------------------------------------------
// Disk (solid or annular ring when innerRadius > 0)
// ---------------------------------------------------------------------------

MeshData buildDisk(float radius, float innerRadius, int segments, const std::string& axis) {
    MeshData m;
    const float pi = std::numbers::pi_v<float>;

    if (innerRadius <= 0.0f) {
        // Solid disk: center + rim fan
        m.positions.insert(m.positions.end(), {0.0f, 0.0f, 0.0f});
        m.normals.insert(m.normals.end(), {0.0f, 1.0f, 0.0f});
        m.texcoords.insert(m.texcoords.end(), {0.5f, 0.5f});

        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * pi * i / segments;
            float c = std::cos(a), s = std::sin(a);
            m.positions.insert(m.positions.end(), {c*radius, 0.0f, s*radius});
            m.normals.insert(m.normals.end(), {0.0f, 1.0f, 0.0f});
            m.texcoords.insert(m.texcoords.end(), {0.5f+c*0.5f, 0.5f+s*0.5f});
        }
        for (int i = 0; i < segments; ++i)
            m.indices.insert(m.indices.end(), {0u, uint32_t(i+2), uint32_t(i+1)});
    } else {
        // Ring disk: outer rim at [0..segments], inner rim at [segments+1..2*segments+1]
        if (innerRadius >= radius)
            throw std::runtime_error("Disk inner_radius must be less than radius");
        float irScale = innerRadius / radius;

        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * pi * i / segments;
            float c = std::cos(a), s = std::sin(a);
            m.positions.insert(m.positions.end(), {c*radius, 0.0f, s*radius});
            m.normals.insert(m.normals.end(), {0.0f, 1.0f, 0.0f});
            m.texcoords.insert(m.texcoords.end(), {0.5f+c*0.5f, 0.5f+s*0.5f});
        }
        for (int i = 0; i <= segments; ++i) {
            float a = 2.0f * pi * i / segments;
            float c = std::cos(a), s = std::sin(a);
            m.positions.insert(m.positions.end(), {c*innerRadius, 0.0f, s*innerRadius});
            m.normals.insert(m.normals.end(), {0.0f, 1.0f, 0.0f});
            m.texcoords.insert(m.texcoords.end(), {0.5f+c*0.5f*irScale, 0.5f+s*0.5f*irScale});
        }
        // outer[i] = i, outer[i+1] = i+1
        // inner[i] = segments+1+i, inner[i+1] = segments+1+i+1
        // CCW from +Y: {outer[i], inner[i], inner[i+1]}  {outer[i], inner[i+1], outer[i+1]}
        for (int i = 0; i < segments; ++i) {
            uint32_t o0 = uint32_t(i);
            uint32_t o1 = uint32_t(i + 1);
            uint32_t i0 = uint32_t(segments + 1 + i);
            uint32_t i1 = uint32_t(segments + 1 + i + 1);
            m.indices.insert(m.indices.end(), {o0, i0, i1,  o0, i1, o1});
        }
    }

    remapAxis(m, axis);
    return m;
}

// ---------------------------------------------------------------------------
// Grid
// ---------------------------------------------------------------------------

MeshData buildGrid(float w, float d, int subdX, int subdZ) {
    MeshData m;
    float hw = w * 0.5f, hd = d * 0.5f;
    int rowSize = subdX + 1;

    for (int iz = 0; iz <= subdZ; ++iz) {
        float tz = float(iz) / subdZ;
        float z  = -hd + tz * d;
        for (int ix = 0; ix <= subdX; ++ix) {
            float tx = float(ix) / subdX;
            float x  = -hw + tx * w;
            m.positions.insert(m.positions.end(), {x, 0.0f, z});
            m.normals.insert(m.normals.end(), {0.0f, 1.0f, 0.0f});
            m.texcoords.insert(m.texcoords.end(), {tx, tz});
        }
    }

    for (int iz = 0; iz < subdZ; ++iz) {
        for (int ix = 0; ix < subdX; ++ix) {
            uint32_t i0 = uint32_t(iz * rowSize + ix);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = uint32_t((iz+1) * rowSize + ix);
            uint32_t i3 = i2 + 1;
            m.indices.insert(m.indices.end(), {i0, i2, i3,  i0, i3, i1});
        }
    }
    return m;
}

// ---------------------------------------------------------------------------
// IcoSphere
// ---------------------------------------------------------------------------

MeshData buildIcoSphere(float radius, int subdivisions) {
    const float pi  = std::numbers::pi_v<float>;
    const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;

    // Icosahedron vertices (normalized)
    auto norm3v = [](std::array<float,3> v) {
        float l = std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
        return std::array<float,3>{v[0]/l, v[1]/l, v[2]/l};
    };
    std::vector<std::array<float,3>> verts;
    for (auto& v : std::initializer_list<std::array<float,3>>{
            {-1,phi,0},{1,phi,0},{-1,-phi,0},{1,-phi,0},
            {0,-1,phi},{0,1,phi},{0,-1,-phi},{0,1,-phi},
            {phi,0,-1},{phi,0,1},{-phi,0,-1},{-phi,0,1}})
        verts.push_back(norm3v(v));

    std::vector<std::array<int,3>> faces = {
        {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
        {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
        {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
    };

    for (int sub = 0; sub < subdivisions; ++sub) {
        std::vector<std::array<int,3>> newFaces;
        std::map<std::pair<int,int>, int> cache;

        auto mid = [&](int a, int b) -> int {
            auto key = std::make_pair(std::min(a,b), std::max(a,b));
            auto it = cache.find(key);
            if (it != cache.end()) return it->second;
            int idx = int(verts.size());
            verts.push_back(norm3v({(verts[a][0]+verts[b][0])*0.5f,
                                    (verts[a][1]+verts[b][1])*0.5f,
                                    (verts[a][2]+verts[b][2])*0.5f}));
            cache[key] = idx;
            return idx;
        };

        for (auto& f : faces) {
            int a = mid(f[0], f[1]), b = mid(f[1], f[2]), c = mid(f[2], f[0]);
            newFaces.insert(newFaces.end(), {{f[0],a,c},{f[1],b,a},{f[2],c,b},{a,b,c}});
        }
        faces = newFaces;
    }

    MeshData m;
    for (auto& f : faces) {
        // STAB-0663: each face gets its own unwelded copy of its 3 vertices
        // (no cross-triangle index sharing here), so a triangle straddling
        // the atan2 seam (u wraps 1.0->0.0) can be fixed purely locally:
        // compute all 3 raw u values first, then nudge any that are on the
        // "low" side back up across the seam if the triangle spans it --
        // this can't affect any OTHER triangle since nothing is shared.
        float us[3], vs[3];
        for (int i = 0; i < 3; ++i) {
            auto& v = verts[f[i]];
            us[i] = 0.5f + std::atan2(v[2], v[0]) / (2.0f * pi);
            vs[i] = 0.5f - std::asin(std::clamp(v[1], -1.0f, 1.0f)) / pi;
        }
        float uMin = std::min({us[0], us[1], us[2]});
        float uMax = std::max({us[0], us[1], us[2]});
        if (uMax - uMin > 0.5f) {
            for (float& u : us) if (u < 0.5f) u += 1.0f;
        }

        for (int i = 0; i < 3; ++i) {
            auto& v = verts[f[i]];
            m.positions.insert(m.positions.end(), {v[0]*radius, v[1]*radius, v[2]*radius});
            m.normals.insert(m.normals.end(), {v[0], v[1], v[2]});
            m.texcoords.insert(m.texcoords.end(), {us[i], vs[i]});
            m.indices.push_back(uint32_t(m.indices.size()));
        }
    }
    return m;
}

// ---------------------------------------------------------------------------
// Extrude
// ---------------------------------------------------------------------------

// Shared math helpers
static std::array<float,3> cross3(std::array<float,3> a, std::array<float,3> b) {
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}
static void norm3(std::array<float,3>& v) {
    float l = std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (l > 1e-6f) { v[0]/=l; v[1]/=l; v[2]/=l; }
}

// Compute the local frame axes (normal, binormal) from a tangent
static void frameAxes(std::array<float,3> t,
                      float& nx, float& ny, float& nz,
                      float& bx, float& by, float& bz)
{
    std::array<float,3> up = (std::abs(t[1]) > 0.99f)
                             ? std::array<float,3>{1,0,0}
                             : std::array<float,3>{0,1,0};
    bx = t[1]*up[2]-t[2]*up[1]; by = t[2]*up[0]-t[0]*up[2]; bz = t[0]*up[1]-t[1]*up[0];
    float bl = std::sqrt(bx*bx+by*by+bz*bz);
    bx/=bl; by/=bl; bz/=bl;
    nx = by*t[2]-bz*t[1]; ny = bz*t[0]-bx*t[2]; nz = bx*t[1]-by*t[0];
}

// Sample the cross-section as a flat polygon in the XY plane
static std::vector<std::array<float,2>> sampleCrossSection(
    const MeshCraft::Mc3::Mc3CrossSection& cs)
{
    using CT = MeshCraft::Mc3::CrossSectionType;
    const float pi = std::numbers::pi_v<float>;
    std::vector<std::array<float,2>> pts;

    switch (cs.type) {
    case CT::Rect: {
        float hw = cs.width * 0.5f, hh = cs.height * 0.5f;
        pts = {{-hw,-hh},{hw,-hh},{hw,hh},{-hw,hh}};
        break;
    }
    case CT::Circle: {
        for (int i = 0; i < cs.segments; ++i) {
            float a = 2.0f * pi * i / cs.segments;
            pts.push_back({std::cos(a)*cs.radius, std::sin(a)*cs.radius});
        }
        break;
    }
    case CT::Polygon: {
        for (int i = 0; i < cs.sides; ++i) {
            float a = 2.0f * pi * i / cs.sides;
            pts.push_back({std::cos(a)*cs.radius, std::sin(a)*cs.radius});
        }
        break;
    }
    case CT::Star: {
        int n = std::max(3, cs.sides);
        float outerR = cs.radius;
        float innerR = (cs.innerRadius > 0.0f && cs.innerRadius < cs.radius)
            ? cs.innerRadius : cs.radius * 0.5f;
        const float offset = -pi / 2.0f; // start from top
        for (int i = 0; i < n; ++i) {
            float aOuter = 2.0f * pi * i / n + offset;
            float aInner = aOuter + pi / n;
            pts.push_back({outerR*std::cos(aOuter), outerR*std::sin(aOuter)});
            pts.push_back({innerR*std::cos(aInner), innerR*std::sin(aInner)});
        }
        break;
    }
    case CT::Custom:
        for (auto& p : cs.customPoints) pts.push_back({p.x, p.y});
        break;
    }
    return pts;
}

// Sample the extrusion path as a list of (position, tangent) frames
struct PathFrame {
    std::array<float,3> pos;
    std::array<float,3> tangent;
};

static std::vector<PathFrame> samplePath(const MeshCraft::Mc3::Mc3ExtrudePath& path,
                                          int segments) {
    using PT = MeshCraft::Mc3::ExtrudePathType;
    const float pi = std::numbers::pi_v<float>;
    std::vector<PathFrame> frames;

    switch (path.type) {
    case PT::Line: {
        float len = path.length;
        if (path.axis == "x") { for (int i=0;i<=segments;++i) frames.push_back({{len*i/segments,0,0},{1,0,0}}); break; }
        if (path.axis == "z") { for (int i=0;i<=segments;++i) frames.push_back({{0,0,len*i/segments},{0,0,1}}); break; }
        for (int i = 0; i <= segments; ++i)
            frames.push_back({{0, len*i/segments, 0}, {0, 1, 0}});
        break;
    }
    case PT::Arc: {
        float r = path.arcRadius;
        float totalAngle = path.arcAngle * pi / 180.0f;
        for (int i = 0; i <= segments; ++i) {
            float t = totalAngle * i / segments;
            float x = r * std::sin(t);
            float y = r * (1.0f - std::cos(t));
            float tx = std::cos(t), ty = std::sin(t);
            frames.push_back({{x, y, 0}, {tx, ty, 0}});
        }
        break;
    }
    case PT::Helix: {
        float r = path.helixRadius, h = path.helixHeight, turns = path.helixTurns;
        float totalAngle = 2.0f * pi * turns;
        for (int i = 0; i <= segments; ++i) {
            float t = totalAngle * i / segments;
            float x = r * std::cos(t);
            float z = r * std::sin(t);
            float y = h * i / segments;
            float tx = -std::sin(t), ty = h / (r * totalAngle), tz = std::cos(t);
            float tl = std::sqrt(tx*tx+ty*ty+tz*tz);
            frames.push_back({{x, y, z}, {tx/tl, ty/tl, tz/tl}});
        }
        break;
    }
    case PT::Polyline: {
        if (path.points.size() < 2) {
            frames.push_back({{0,0,0},{0,1,0}});
            frames.push_back({{0,1,0},{0,1,0}});
            break;
        }
        // Compute total arc length for uniform segment distribution
        int np = static_cast<int>(path.points.size());
        for (int seg = 0; seg < np - 1; ++seg) {
            auto& p0 = path.points[seg].position;
            auto& p1 = path.points[seg+1].position;
            float dx = p1[0]-p0[0], dy = p1[1]-p0[1], dz = p1[2]-p0[2];
            float len = std::sqrt(dx*dx+dy*dy+dz*dz);
            float tang = len > 1e-6f ? 1.0f/len : 1.0f;
            int subSegs = std::max(1, segments / std::max(1, np-1));
            for (int i = 0; i <= subSegs; ++i) {
                if (seg > 0 && i == 0) continue; // avoid duplicate at joints
                float t2 = static_cast<float>(i) / subSegs;
                PathFrame f;
                f.pos = {p0[0]+dx*t2, p0[1]+dy*t2, p0[2]+dz*t2};
                f.tangent = {dx*tang, dy*tang, dz*tang};
                frames.push_back(f);
            }
        }
        break;
    }
    case PT::Bezier: {
        // Catmull-Rom through path.points (auto-tangents from neighbours)
        int np = static_cast<int>(path.points.size());
        if (np < 2) {
            frames.push_back({{0,0,0},{0,1,0}});
            frames.push_back({{0,1,0},{0,1,0}});
            break;
        }
        // Catmull-Rom tangent at point i = 0.5 * (P[i+1] - P[i-1])
        // Clamp endpoints: tangent[0] = P[1]-P[0], tangent[n-1] = P[n-1]-P[n-2]
        auto catmullPoint = [&](int i0, int i1, int i2, int i3, float t2)
            -> std::pair<std::array<float,3>, std::array<float,3>>
        {
            const auto& p0 = path.points[i0].position;
            const auto& p1 = path.points[i1].position;
            const auto& p2 = path.points[i2].position;
            const auto& p3 = path.points[i3].position;
            // Position (Catmull-Rom cubic):
            float t3 = t2*t2, t4 = t3*t2;
            std::array<float,3> pos;
            for (int k=0;k<3;++k) {
                float a = -0.5f*p0[k] + 1.5f*p1[k] - 1.5f*p2[k] + 0.5f*p3[k];
                float b =       p0[k] - 2.5f*p1[k] + 2.0f*p2[k] - 0.5f*p3[k];
                float c = -0.5f*p0[k]               + 0.5f*p2[k];
                float d =                    p1[k];
                pos[k] = a*t4 + b*t3 + c*t2 + d;
            }
            // Tangent (derivative):
            std::array<float,3> tan;
            for (int k=0;k<3;++k) {
                float a = -0.5f*p0[k] + 1.5f*p1[k] - 1.5f*p2[k] + 0.5f*p3[k];
                float b =       p0[k] - 2.5f*p1[k] + 2.0f*p2[k] - 0.5f*p3[k];
                float c = -0.5f*p0[k]               + 0.5f*p2[k];
                tan[k] = 3.0f*a*t3 + 2.0f*b*t2 + c;
            }
            float tl = std::sqrt(tan[0]*tan[0]+tan[1]*tan[1]+tan[2]*tan[2]);
            if (tl > 1e-6f) { tan[0]/=tl; tan[1]/=tl; tan[2]/=tl; }
            else { tan = {0,1,0}; }
            return {pos, tan};
        };

        int subSegs = std::max(1, segments / std::max(1, np-1));
        for (int seg = 0; seg < np-1; ++seg) {
            int i0 = std::max(0, seg-1);
            int i1 = seg;
            int i2 = seg+1;
            int i3 = std::min(np-1, seg+2);
            for (int i = 0; i <= subSegs; ++i) {
                if (seg > 0 && i == 0) continue;
                float t2 = static_cast<float>(i) / subSegs;
                auto [pos, tan] = catmullPoint(i0, i1, i2, i3, t2);
                frames.push_back({pos, tan});
            }
        }
        break;
    }
    }
    return frames;
}

// ---------------------------------------------------------------------------
// Hollow extrude (innerRadius > 0): outer wall + inner wall + annular caps
// ---------------------------------------------------------------------------

static MeshData buildHollowExtrude(const MeshCraft::Mc3::Mc3Extrude& ext) {
    MeshData m;
    const float pi = std::numbers::pi_v<float>;

    auto frames = samplePath(ext.path, ext.segments);
    if (static_cast<int>(frames.size()) < 2) return m;

    const int   nf      = static_cast<int>(frames.size());
    const int   ncs     = ext.crossSection.segments;
    const float outerR  = ext.crossSection.radius;
    const float innerR  = ext.crossSection.innerRadius;
    const float twist   = ext.twist * pi / 180.0f;

    // Build both rings for a single path frame
    auto makeRings = [&](const PathFrame& pf, int fi)
        -> std::pair<std::vector<std::array<float,3>>,
                     std::vector<std::array<float,3>>>
    {
        float nx, ny, nz, bx, by, bz;
        frameAxes(pf.tangent, nx, ny, nz, bx, by, bz);

        float ta = (nf > 1) ? twist * fi / (nf - 1) : 0.0f;
        float ct = std::cos(ta), st = std::sin(ta);

        std::vector<std::array<float,3>> outer(ncs), inner(ncs);
        for (int i = 0; i < ncs; ++i) {
            float a  = 2.0f * pi * i / ncs;
            float cx = std::cos(a), cy = std::sin(a);
            float rx = cx*ct - cy*st, ry = cx*st + cy*ct;

            auto place = [&](float r) -> std::array<float,3> {
                return { pf.pos[0] + rx*r*nx + ry*r*bx,
                         pf.pos[1] + rx*r*ny + ry*r*by,
                         pf.pos[2] + rx*r*nz + ry*r*bz };
            };
            outer[i] = place(outerR);
            inner[i] = place(innerR);
        }
        return {outer, inner};
    };

    auto [prevOuter, prevInner] = makeRings(frames[0], 0);

    for (int fi = 1; fi < nf; ++fi) {
        auto [currOuter, currInner] = makeRings(frames[fi], fi);

        for (int ci = 0; ci < ncs; ++ci) {
            int  ci1 = (ci + 1) % ncs;
            float u0 = float(ci)   / ncs,  u1 = float(ci+1) / ncs;
            float v0 = float(fi-1) / (nf-1), v1 = float(fi) / (nf-1);

            // Outer face (normal outward)
            {
                auto e1 = std::array<float,3>{currOuter[ci][0]-prevOuter[ci][0],
                                              currOuter[ci][1]-prevOuter[ci][1],
                                              currOuter[ci][2]-prevOuter[ci][2]};
                auto e2 = std::array<float,3>{prevOuter[ci1][0]-prevOuter[ci][0],
                                              prevOuter[ci1][1]-prevOuter[ci][1],
                                              prevOuter[ci1][2]-prevOuter[ci][2]};
                auto n = cross3(e1, e2); norm3(n);
                addQuad(m, prevOuter[ci], currOuter[ci], currOuter[ci1], prevOuter[ci1], n,
                        {u0,v0},{u0,v1},{u1,v1},{u1,v0});
            }
            // Inner face (normal inward — reversed winding)
            {
                auto e1 = std::array<float,3>{prevInner[ci1][0]-prevInner[ci][0],
                                              prevInner[ci1][1]-prevInner[ci][1],
                                              prevInner[ci1][2]-prevInner[ci][2]};
                auto e2 = std::array<float,3>{currInner[ci][0]-prevInner[ci][0],
                                              currInner[ci][1]-prevInner[ci][1],
                                              currInner[ci][2]-prevInner[ci][2]};
                auto n = cross3(e1, e2); norm3(n);
                addQuad(m, prevInner[ci], prevInner[ci1], currInner[ci1], currInner[ci], n,
                        {u0,v0},{u1,v0},{u1,v1},{u0,v1});
            }
        }
        prevOuter = currOuter;
        prevInner = currInner;
    }

    // Annular caps
    if (ext.caps) {
        auto [s0, s1] = makeRings(frames[0],    0);
        auto [e0, e1] = makeRings(frames[nf-1], nf-1);
        const auto& st = frames[0].tangent;
        const auto& et = frames[nf-1].tangent;

        for (int ci = 0; ci < ncs; ++ci) {
            int ci1 = (ci + 1) % ncs;
            float u0 = float(ci) / ncs, u1 = float(ci+1) / ncs;

            std::array<float,3> sn = {-st[0], -st[1], -st[2]};
            addQuad(m, s0[ci], s1[ci], s1[ci1], s0[ci1], sn,
                    {u0,0},{u0,1},{u1,1},{u1,0});

            std::array<float,3> en = {et[0], et[1], et[2]};
            addQuad(m, e0[ci], e0[ci1], e1[ci1], e1[ci], en,
                    {u0,0},{u1,0},{u1,1},{u0,1});
        }
    }

    // Smooth normals
    if (ext.smooth) {
        using PosKey = std::tuple<int,int,int>;
        auto quant = [](float v){ return static_cast<int>(std::round(v*10000.0f)); };
        std::map<PosKey, std::array<float,3>> acc;
        int vc = m.vertexCount();
        for (int i=0;i<vc;++i) {
            PosKey k{quant(m.positions[i*3]),quant(m.positions[i*3+1]),quant(m.positions[i*3+2])};
            auto& n=acc[k]; n[0]+=m.normals[i*3]; n[1]+=m.normals[i*3+1]; n[2]+=m.normals[i*3+2];
        }
        for (auto&[k,n]:acc){ float l=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]); if(l>1e-6f){n[0]/=l;n[1]/=l;n[2]/=l;} }
        for (int i=0;i<vc;++i) {
            PosKey k{quant(m.positions[i*3]),quant(m.positions[i*3+1]),quant(m.positions[i*3+2])};
            auto& n=acc[k]; m.normals[i*3]=n[0]; m.normals[i*3+1]=n[1]; m.normals[i*3+2]=n[2];
        }
    }

    return m;
}

// ---------------------------------------------------------------------------
// Extrude (solid cross-section)
// ---------------------------------------------------------------------------

MeshData buildExtrude(const MeshCraft::Mc3::Mc3Extrude& ext) {
    // Hollow circle cross-section → specialized path
    if (ext.crossSection.type == MeshCraft::Mc3::CrossSectionType::Circle &&
        ext.crossSection.innerRadius > 0.0f &&
        ext.crossSection.innerRadius < ext.crossSection.radius) {
        return buildHollowExtrude(ext);
    }

    MeshData m;
    auto csPoints = sampleCrossSection(ext.crossSection);
    if (csPoints.empty()) return m;

    auto frames = samplePath(ext.path, ext.segments);
    if (frames.size() < 2) return m;

    int ncs = static_cast<int>(csPoints.size());
    int nf  = static_cast<int>(frames.size());

    const float pi = std::numbers::pi_v<float>;
    float twistPerFrame = (ext.twist * pi / 180.0f) / (nf - 1);

    // For each frame, compute a local coordinate frame (tangent, normal, binormal)
    // and place the cross-section points in world space.
    auto buildFrame = [&](const PathFrame& pf, int fi) {
        float nx, ny, nz, bx, by, bz;
        frameAxes(pf.tangent, nx, ny, nz, bx, by, bz);

        // Apply twist rotation in the cross-section plane
        float twistAngle = twistPerFrame * fi;
        float ct = std::cos(twistAngle), st = std::sin(twistAngle);

        std::vector<std::array<float,3>> worldPts;
        for (auto& [cx, cy] : csPoints) {
            float rx = cx*ct - cy*st;
            float ry = cx*st + cy*ct;
            worldPts.push_back({
                pf.pos[0] + rx*nx + ry*bx,
                pf.pos[1] + rx*ny + ry*by,
                pf.pos[2] + rx*nz + ry*bz
            });
        }
        return worldPts;
    };

    // Build quads between consecutive frames
    auto prev = buildFrame(frames[0], 0);
    for (int fi = 1; fi < nf; ++fi) {
        auto curr = buildFrame(frames[fi], fi);

        for (int ci = 0; ci < ncs; ++ci) {
            int ci1 = (ci + 1) % ncs;
            auto& p00 = prev[ci]; auto& p10 = curr[ci];
            auto& p01 = prev[ci1]; auto& p11 = curr[ci1];

            // Normal: cross product of two edges
            std::array<float,3> e1 = {p10[0]-p00[0], p10[1]-p00[1], p10[2]-p00[2]};
            std::array<float,3> e2 = {p01[0]-p00[0], p01[1]-p00[1], p01[2]-p00[2]};
            auto n = cross3(e1, e2);
            norm3(n);

            float u0 = static_cast<float>(ci)  / ncs;
            float u1 = static_cast<float>(ci+1) / ncs;
            float v0 = static_cast<float>(fi-1) / (nf-1);
            float v1 = static_cast<float>(fi)   / (nf-1);

            addQuad(m, p00, p10, p11, p01, n,
                    {u0,v0}, {u0,v1}, {u1,v1}, {u1,v0});
        }
        prev = curr;
    }

    // Caps
    if (ext.caps && ncs >= 3) {
        // Helper: fan triangulate a flat polygon
        auto addCap = [&](const std::vector<std::array<float,3>>& ring,
                          std::array<float,3> n, bool flip) {
            auto base = static_cast<uint32_t>(m.vertexCount());
            for (size_t i = 0; i < ring.size(); ++i) {
                m.positions.insert(m.positions.end(), ring[i].begin(), ring[i].end());
                m.normals.insert(m.normals.end(), n.begin(), n.end());
                float a = 2.0f * pi * i / ring.size();
                m.texcoords.insert(m.texcoords.end(), {0.5f+std::cos(a)*0.5f, 0.5f+std::sin(a)*0.5f});
            }
            for (uint32_t i = 1; i + 1 < ring.size(); ++i) {
                if (flip) m.indices.insert(m.indices.end(), {base, base+i+1, base+i});
                else      m.indices.insert(m.indices.end(), {base, base+i,   base+i+1});
            }
        };

        auto startRing = buildFrame(frames.front(), 0);
        auto endRing   = buildFrame(frames.back(), nf-1);
        auto& tf = frames.front().tangent;
        auto& tb = frames.back().tangent;
        addCap(startRing, {-tf[0],-tf[1],-tf[2]}, true);
        addCap(endRing,   { tb[0], tb[1], tb[2]}, false);
    }

    // Smooth normals: average normals at coincident positions
    if (ext.smooth) {
        using PosKey = std::tuple<int,int,int>;
        auto quant = [](float v) { return static_cast<int>(std::round(v * 10000.0f)); };

        std::map<PosKey, std::array<float,3>> accumNorm;
        int vc = m.vertexCount();

        for (int i = 0; i < vc; ++i) {
            PosKey k{quant(m.positions[i*3]), quant(m.positions[i*3+1]), quant(m.positions[i*3+2])};
            auto& n = accumNorm[k];
            n[0] += m.normals[i*3];
            n[1] += m.normals[i*3+1];
            n[2] += m.normals[i*3+2];
        }
        for (auto& [k, n] : accumNorm) {
            float len = std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
            if (len > 1e-6f) { n[0]/=len; n[1]/=len; n[2]/=len; }
        }
        for (int i = 0; i < vc; ++i) {
            PosKey k{quant(m.positions[i*3]), quant(m.positions[i*3+1]), quant(m.positions[i*3+2])};
            auto& n = accumNorm[k];
            m.normals[i*3]   = n[0];
            m.normals[i*3+1] = n[1];
            m.normals[i*3+2] = n[2];
        }
    }

    return m;
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

MeshData buildPrimitive(const MeshCraft::Mc3::Mc3Primitive& p) {
    using PT = MeshCraft::Mc3::PrimitiveType;
    switch (p.primitiveType) {
    case PT::Box:
    case PT::Cube:      return buildBox(p.size[0], p.size[1], p.size[2]);
    case PT::Sphere:    return buildSphere(p.radius, p.segments);
    case PT::Cylinder:  return buildCylinder(p.radius, p.height, p.segments, p.axis);
    case PT::Cone:      return buildCone(p.radius, p.height, p.segments);
    case PT::Plane:     return buildPlane(p.size[0], p.size[2], p.axis);
    case PT::Torus:    return buildTorus(p.majorRadius, p.minorRadius, p.segments);
    case PT::Capsule:  return buildCapsule(p.radius, p.height, p.segments, p.axis);
    case PT::Disk:     return buildDisk(p.radius, p.minorRadius, p.segments, p.axis);
    case PT::Grid:     return buildGrid(p.size[0], p.size[2], p.subdivisionsX, p.subdivisionsZ);
    case PT::IcoSphere: return buildIcoSphere(p.radius, std::max(1, std::min(4, p.segments/8)));
    }
    return {};
}

// ---------------------------------------------------------------------------
// OBJ mesh loading
// ---------------------------------------------------------------------------

MeshData loadObjMesh(const std::filesystem::path& basePath, const std::string& source)
{
    std::filesystem::path objPath = source;
    if (objPath.is_relative()) objPath = basePath / source;

    tinyobj::ObjReaderConfig cfg;
    cfg.mtl_search_path = basePath.string();
    cfg.triangulate     = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(objPath.string(), cfg)) {
        throw std::runtime_error("OBJ load failed (" + objPath.string() + "): " + reader.Error());
    }
    if (!reader.Warning().empty())
        std::cerr << "OBJ warning: " << reader.Warning() << '\n';

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    // Reject non-finite vertex coordinates (e.g. "1e400" overflowing to inf)
    // rather than silently exporting a spec-invalid glTF (inf in the binary
    // buffer, null in the JSON accessor min/max).
    for (float v : attrib.vertices) {
        if (!std::isfinite(v)) {
            throw std::runtime_error("OBJ load failed (" + objPath.string() +
                                      "): non-finite vertex coordinate");
        }
    }

    MeshData m;
    for (const auto& shape : shapes) {
        const auto& idxList = shape.mesh.indices;
        // tinyobjloader already triangulates, iterate in steps of 3
        for (size_t i = 0; i + 2 < idxList.size(); i += 3) {
            const tinyobj::index_t* tri[3] = {&idxList[i], &idxList[i+1], &idxList[i+2]};

            // Compute face normal when any vertex is missing a normal
            std::array<float,3> faceNormal{0.0f, 1.0f, 0.0f};
            bool needFaceNormal = (tri[0]->normal_index < 0 ||
                                   tri[1]->normal_index < 0 ||
                                   tri[2]->normal_index < 0);
            if (needFaceNormal) {
                auto pos = [&](int k) -> std::array<float,3> {
                    auto vi = static_cast<size_t>(tri[k]->vertex_index);
                    return {attrib.vertices[3*vi], attrib.vertices[3*vi+1], attrib.vertices[3*vi+2]};
                };
                auto p0 = pos(0), p1 = pos(1), p2 = pos(2);
                std::array<float,3> e1{p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2]};
                std::array<float,3> e2{p2[0]-p0[0], p2[1]-p0[1], p2[2]-p0[2]};
                faceNormal = cross3(e1, e2);
                norm3(faceNormal);
            }

            for (int k = 0; k < 3; ++k) {
                const auto& idx = *tri[k];
                auto vi = static_cast<size_t>(idx.vertex_index);
                m.positions.push_back(attrib.vertices[3*vi+0]);
                m.positions.push_back(attrib.vertices[3*vi+1]);
                m.positions.push_back(attrib.vertices[3*vi+2]);

                if (idx.normal_index >= 0) {
                    auto ni = static_cast<size_t>(idx.normal_index);
                    m.normals.push_back(attrib.normals[3*ni+0]);
                    m.normals.push_back(attrib.normals[3*ni+1]);
                    m.normals.push_back(attrib.normals[3*ni+2]);
                } else {
                    m.normals.push_back(faceNormal[0]);
                    m.normals.push_back(faceNormal[1]);
                    m.normals.push_back(faceNormal[2]);
                }

                if (idx.texcoord_index >= 0) {
                    auto ti = static_cast<size_t>(idx.texcoord_index);
                    m.texcoords.push_back(attrib.texcoords[2*ti+0]);
                    m.texcoords.push_back(1.0f - attrib.texcoords[2*ti+1]); // flip V
                } else {
                    m.texcoords.push_back(0.0f);
                    m.texcoords.push_back(0.0f);
                }

                m.indices.push_back(static_cast<uint32_t>(m.indices.size()));
            }
        }
    }
    return m;
}

} // namespace mc3togltf
