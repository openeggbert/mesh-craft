#include "MeshBuilder.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <tiny_gltf.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
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

void MeshData::applyUvMapping(float scaleU, float scaleV,
                              float offsetU, float offsetV,
                              float rotationDegrees) {
    const float rad = rotationDegrees * std::numbers::pi_v<float> / 180.0f;
    const float c = std::cos(rad), s = std::sin(rad);
    for (size_t i = 0; i < texcoords.size(); i += 2) {
        float u = texcoords[i] * scaleU;
        float v = texcoords[i+1] * scaleV;
        float ru = u * c - v * s;
        float rv = u * s + v * c;
        texcoords[i]   = ru + offsetU;
        texcoords[i+1] = rv + offsetV;
    }
}

void MeshData::applyPlanarProjectionUv() {
    texcoords.clear();
    texcoords.reserve(static_cast<size_t>(vertexCount()) * 2);
    for (size_t i = 0; i + 2 < positions.size(); i += 3) {
        texcoords.push_back(positions[i]);
        texcoords.push_back(positions[i + 2]);
    }
}

namespace {
// Local bounding-box center, used by both projections below: as the
// position-based fallback axis source for box projection when a vertex
// has no normal, and as the origin sphere projection's radius/direction
// is measured from (so an off-center primitive still maps sensibly).
std::array<float,3> boundingBoxCenter(const std::vector<float>& positions) {
    if (positions.empty()) return {0.0f, 0.0f, 0.0f};
    float minX = positions[0], maxX = positions[0];
    float minY = positions[1], maxY = positions[1];
    float minZ = positions[2], maxZ = positions[2];
    for (size_t i = 0; i < positions.size(); i += 3) {
        minX = std::min(minX, positions[i]);   maxX = std::max(maxX, positions[i]);
        minY = std::min(minY, positions[i+1]); maxY = std::max(maxY, positions[i+1]);
        minZ = std::min(minZ, positions[i+2]); maxZ = std::max(maxZ, positions[i+2]);
    }
    return {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f};
}
} // namespace

void MeshData::applyBoxProjectionUv() {
    if (positions.empty()) return;
    const auto center = boundingBoxCenter(positions);
    const bool haveNormals = normals.size() == positions.size();

    texcoords.assign(positions.size() / 3 * 2, 0.0f);
    for (size_t i = 0, vi = 0; i < positions.size(); i += 3, vi += 2) {
        const float px = positions[i], py = positions[i+1], pz = positions[i+2];
        float ax, ay, az;
        if (haveNormals) {
            ax = normals[i]; ay = normals[i+1]; az = normals[i+2];
        } else {
            ax = px - center[0]; ay = py - center[1]; az = pz - center[2];
        }
        const float absX = std::fabs(ax), absY = std::fabs(ay), absZ = std::fabs(az);

        float u, v;
        if (absX >= absY && absX >= absZ) {
            u = pz; v = py;               // dominant axis X -> project onto ZY
        } else if (absY >= absX && absY >= absZ) {
            u = px; v = pz;               // dominant axis Y -> project onto XZ
        } else {
            u = px; v = py;               // dominant axis Z -> project onto XY
        }
        texcoords[vi]   = u;
        texcoords[vi+1] = v;
    }
}

void MeshData::applySphereProjectionUv() {
    if (positions.empty()) return;
    const auto center = boundingBoxCenter(positions);

    texcoords.assign(positions.size() / 3 * 2, 0.0f);
    for (size_t i = 0, vi = 0; i < positions.size(); i += 3, vi += 2) {
        const float dx = positions[i]   - center[0];
        const float dy = positions[i+1] - center[1];
        const float dz = positions[i+2] - center[2];
        const float r = std::sqrt(dx*dx + dy*dy + dz*dz);

        float u, v;
        if (r < 1e-8f) {
            u = 0.5f; v = 0.5f;
        } else {
            u = 0.5f + std::atan2(dz, dx) / (2.0f * std::numbers::pi_v<float>);
            const float clampedY = std::clamp(dy / r, -1.0f, 1.0f);
            v = 0.5f - std::asin(clampedY) / std::numbers::pi_v<float>;
        }
        texcoords[vi]   = u;
        texcoords[vi+1] = v;
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
    // 2026-07-20 audit finding #2: segments in {0,1} makes rings==0 (integer
    // division), and the pole/rim loops below use "<= rings"/"<= sectors"
    // bounds that still execute once at r==0/s==0 even when rings/sectors
    // is 0 -- computing e.g. `pi * 0 / 0` (0/0), a NaN that then propagates
    // into the exported POSITION accessor. mc3.xsd/the parsers only reject
    // negative segments, so 0/1 are legal document values that reached this
    // unclamped. Matches drawDiskDynamic()'s established std::max(3, ...)
    // convention (SceneRenderer_Extrude.cpp) for a closed-loop dimension.
    int rings = std::max(1, segments / 2);
    int sectors = std::max(3, segments);
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
    // STAB-0669: at the poles (r==0 and r==rings-1's far ring), all
    // sectors+1 vertices in that ring collapse to the same 3D position
    // (sin(phi)==0), so one of each quad's two triangles has two corners at
    // an identical position -- a zero-area triangle. Emit only the
    // non-degenerate triangle (a proper fan wedge) at each pole; interior
    // rings keep both triangles as before. Guard rings==1 (topPole and
    // bottomPole would otherwise coincide and drop every triangle, leaving
    // an empty mesh): fall back to emitting both original triangles.
    // STAB-0702: was {i0,i2,i1, i1,i2,i3} -- confirmed numerically (winding-
    // derived face normal vs. this function's own stored per-vertex normal)
    // that every triangle was wound backwards, uniformly. This is the same
    // bug/fix as buildTorus() -- swapping to {i0,i1,i2, i1,i3,i2} flips
    // every triangle to match the outward normal.
    for (int r = 0; r < rings; ++r) {
        bool topPole    = (r == 0) && rings > 1;
        bool bottomPole = (r == rings - 1) && rings > 1;
        for (int s = 0; s < sectors; ++s) {
            auto i0 = static_cast<uint32_t>(r * (sectors+1) + s);
            auto i1 = i0 + 1;
            auto i2 = i0 + (sectors+1);
            auto i3 = i2 + 1;
            if (!topPole)    m.indices.insert(m.indices.end(), {i0, i1, i2});
            if (!bottomPole) m.indices.insert(m.indices.end(), {i1, i3, i2});
        }
    }
    return m;
}

// ---------------------------------------------------------------------------
// Cylinder
// ---------------------------------------------------------------------------

MeshData buildCylinder(float radius, float height, int segments, const std::string& axis) {
    MeshData m;
    // 2026-07-20 audit finding #2: the side loop below is already safe at
    // segments==0 (a "< segments" bound simply emits nothing), but both cap
    // rim loops use "<= segments" -- still executing once at i==0 even when
    // segments is 0, computing `2*pi*0/0` (NaN) into the exported top/bottom
    // cap center-adjacent vertex.
    segments = std::max(3, segments);
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
    // 2026-07-20 audit finding #2: same "<= segments" bottom-cap-rim NaN
    // risk as buildCylinder() above (the side loop is already safe, using
    // "< segments").
    segments = std::max(3, segments);
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

        // STAB-0702: was {apex, apex+1, apex+2} (rim0 before rim1), which
        // winds this triangle backwards relative to its own stored outward
        // normal -- confirmed numerically (face normal via winding has a
        // NEGATIVE dot product with the analytically-correct outward
        // normal) -- while the bottom cap's fan below is already correctly
        // wound. That mismatch meant the side and cap contributed opposite
        // signs to any consistent-outward-orientation check (e.g. the
        // divergence-theorem volume check added in STAB-0666), a real
        // backface-culling bug under the default single-sided glTF
        // material. Swapping rim1/rim0 here fixes the side to match the
        // cap's already-correct winding.
        m.indices.insert(m.indices.end(), {apex, apex+2, apex+1});
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
    // 2026-07-20 audit finding #2: `rings` (unlike `sides` right below,
    // which was already guarded) was segments unclamped -- segments==0
    // makes the "<= rings" loop below compute `2*pi*0/0` (NaN) for its
    // r==0 iteration, which still runs even though rings is 0.
    int rings = std::max(3, segments);
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

    // STAB-0702: was {i0,i2,i1, i1,i2,i3} -- confirmed numerically (winding-
    // derived face normal vs. the analytically-correct stored normal above)
    // that this pattern is wound backwards for every single quad, uniformly
    // (unlike Cone's side-vs-cap-only mismatch). A torus is not star-shaped
    // from the origin, so the divergence-theorem-from-origin volume check
    // used elsewhere (STAB-0666) can't diagnose this -- it always reports a
    // mixed pos/neg split for a torus regardless of whether the winding is
    // actually correct or backwards, since some origin-tetrahedra
    // necessarily subtract to account for the donut hole either way. The
    // local per-triangle-vs-own-normal check is the one that actually
    // caught this. Swapping to {i0,i1,i2, i1,i3,i2} flips every triangle at
    // once to match the outward normal.
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sides; ++s) {
            uint32_t i0 = uint32_t(r * rowSize + s);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = uint32_t((r+1) * rowSize + s);
            uint32_t i3 = i2 + 1;
            m.indices.insert(m.indices.end(), {i0, i1, i2,  i1, i3, i2});
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
    // 2026-07-20 audit finding #2: `sectors` (unlike `rings` right above,
    // which was already guarded) was segments unclamped -- segments==0
    // makes the "<= sectors" loop below compute `2*pi*0/0` (NaN) for its
    // s==0 iteration, which still runs even though sectors is 0.
    int sectors = std::max(3, segments);
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

    // STAB-0702: was {i0,i2,i1, i1,i2,i3} -- same uniform backwards-winding
    // bug (and same fix) as buildSphere()/buildTorus(), confirmed
    // numerically against this function's own stored normals.
    //
    // STAB-0669-style pole fix, extended to Capsule (STAB-0669 itself only
    // touched buildSphere()): row 0 and row totalRows-1 are the poles,
    // where every sector's vertex collapses to the same 3D position (like
    // buildSphere()'s poles) -- one triangle per pole quad is therefore
    // zero-area. Emit only the non-degenerate fan-wedge triangle at each
    // pole quad; interior rows keep both triangles.
    for (int r = 0; r < totalRows - 1; ++r) {
        bool topPole    = (r == 0);
        bool bottomPole = (r == totalRows - 2);
        for (int s = 0; s < sectors; ++s) {
            uint32_t i0 = uint32_t(r * rowSize + s);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = uint32_t((r+1) * rowSize + s);
            uint32_t i3 = i2 + 1;
            if (!topPole)    m.indices.insert(m.indices.end(), {i0, i1, i2});
            if (!bottomPole) m.indices.insert(m.indices.end(), {i1, i3, i2});
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
    // 2026-07-20 audit finding #2: both branches below use "<= segments"
    // rim loops -- still executing once at i==0 even when segments is 0,
    // computing `2*pi*0/0` (NaN). Matches drawDiskDynamic()'s own
    // std::max(3, segments) convention for this exact primitive
    // (SceneRenderer_Extrude.cpp) -- the live-viewport dynamic-disk path
    // was already guarded; only this export-side builder was not.
    segments = std::max(3, segments);

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
    // AUD-067: a degenerate (zero-length) tangent -- e.g. two consecutive
    // identical <point> elements in a Polyline extrude path, which (unlike
    // the Bezier path's own {0,1,0} fallback a few functions up) has no
    // fallback and passes a {0,0,0} tangent straight through -- makes
    // bl == 0 here. Guard it the same way norm3() above guards its own
    // divide, rather than dividing by zero into NaN. b staying {0,0,0} is
    // fine: n below is cross(b,t), which is always {0,0,0} when t == {0,0,0}
    // regardless of what b is, so there is no fallback direction that would
    // avoid a degenerate frame here anyway -- the honest result of a
    // zero-length path segment is a single pinched (zero-radius), but
    // finite, ring at that one point, not a NaN-corrupted mesh.
    if (bl > 1e-6f) { bx/=bl; by/=bl; bz/=bl; }
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

// Ear-clipping triangulation of a simple 2D polygon. Returns triangles as
// index triples into `pts`, preserving the polygon's winding order (so the
// caller can flip winding uniformly). Correct for CONCAVE polygons (e.g. the
// built-in Star cross-section, or an arbitrary Custom outline) — a plain
// triangle fan is only valid for convex polygons and produces overlapping,
// self-covering caps on concave inputs.
std::vector<std::array<uint32_t,3>>
earClipPolygon(const std::vector<std::array<float,2>>& pts) {
    std::vector<std::array<uint32_t,3>> tris;
    const int n = static_cast<int>(pts.size());
    if (n < 3) return tris;

    auto cross = [](const std::array<float,2>& a, const std::array<float,2>& b,
                    const std::array<float,2>& c) {
        return (b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0]);
    };
    // Winding from the signed area (positive == counter-clockwise).
    float area2 = 0.0f;
    for (int i = 0; i < n; ++i) {
        const auto& a = pts[i];
        const auto& b = pts[(i+1) % n];
        area2 += a[0]*b[1] - b[0]*a[1];
    }
    const bool ccw = area2 > 0.0f;

    auto pointInTri = [&](const std::array<float,2>& p, const std::array<float,2>& a,
                          const std::array<float,2>& b, const std::array<float,2>& c) {
        float d1 = cross(a, b, p), d2 = cross(b, c, p), d3 = cross(c, a, p);
        bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(neg && pos);   // p is inside/on the triangle
    };

    std::vector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;

    int guard = 0;
    const int maxGuard = n * n + 16;
    while (static_cast<int>(idx.size()) > 3 && guard++ < maxGuard) {
        const int m = static_cast<int>(idx.size());
        bool clipped = false;
        for (int i = 0; i < m; ++i) {
            int i0 = idx[(i + m - 1) % m], i1 = idx[i], i2 = idx[(i + 1) % m];
            const auto& a = pts[i0]; const auto& b = pts[i1]; const auto& c = pts[i2];
            float cr = cross(a, b, c);
            bool convex = ccw ? (cr > 0.0f) : (cr < 0.0f);
            if (!convex) continue;                 // reflex vertex — not an ear
            bool ear = true;
            for (int j = 0; j < m; ++j) {
                int vj = idx[j];
                if (vj == i0 || vj == i1 || vj == i2) continue;
                if (pointInTri(pts[vj], a, b, c)) { ear = false; break; }
            }
            if (!ear) continue;
            tris.push_back({static_cast<uint32_t>(i0), static_cast<uint32_t>(i1),
                            static_cast<uint32_t>(i2)});
            idx.erase(idx.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) break;                        // degenerate polygon — stop
    }
    if (idx.size() == 3)
        tris.push_back({static_cast<uint32_t>(idx[0]), static_cast<uint32_t>(idx[1]),
                        static_cast<uint32_t>(idx[2])});
    return tris;
}

void assertResourceAllowed(const std::filesystem::path& basePath,
                           const std::string& rawPath,
                           bool allowExternal,
                           const char* kind) {
    if (allowExternal || rawPath.empty()) return;
    // Not filesystem paths — resolved elsewhere.
    if (rawPath.rfind("embed:", 0) == 0 || rawPath.rfind("data:", 0) == 0) return;

    std::filesystem::path p(rawPath);
    if (p.is_absolute())
        throw std::runtime_error(
            std::string("mc3togltf: ") + kind + " '" + rawPath +
            "' is an absolute path outside the document root; refusing to read it "
            "(pass --allow-external-resources to override).");

    // doc.sourcePath (basePath) is EMPTY when the document was opened via a bare
    // relative filename with no directory component (the common
    // `mc3togltf scene.mc3.xml out.glb` invocation, run from the scene's own
    // directory). weakly_canonical("") returns an empty path rather than
    // resolving to the current working directory, so an empty basePath must be
    // normalized to "." first -- otherwise `root` stays empty, relative(cand,
    // root) against an empty base returns empty too, and every same-directory
    // resource would be wrongly rejected as "escaping the document root".
    const auto& effectiveBase = basePath.empty() ? std::filesystem::path(".") : basePath;
    std::error_code ec;
    auto cand = std::filesystem::weakly_canonical(effectiveBase / p, ec);
    auto root = std::filesystem::weakly_canonical(effectiveBase, ec);
    std::error_code ec2;
    auto rel = std::filesystem::relative(cand, root, ec2);
    if (ec || ec2 || rel.empty() || rel.native().rfind("..", 0) == 0)
        throw std::runtime_error(
            std::string("mc3togltf: ") + kind + " '" + rawPath +
            "' escapes the document root; refusing to read it "
            "(pass --allow-external-resources to override).");
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
        // Triangulate the cross-section polygon once (ear-clipping, so concave
        // shapes like Star/Custom cap correctly instead of self-overlapping).
        const auto capTris = earClipPolygon(csPoints);

        auto addCap = [&](const std::vector<std::array<float,3>>& ring,
                          std::array<float,3> n, bool flip) {
            auto base = static_cast<uint32_t>(m.vertexCount());
            for (size_t i = 0; i < ring.size(); ++i) {
                m.positions.insert(m.positions.end(), ring[i].begin(), ring[i].end());
                m.normals.insert(m.normals.end(), n.begin(), n.end());
                float a = 2.0f * pi * i / ring.size();
                m.texcoords.insert(m.texcoords.end(), {0.5f+std::cos(a)*0.5f, 0.5f+std::sin(a)*0.5f});
            }
            for (const auto& t : capTris) {
                if (flip) m.indices.insert(m.indices.end(), {base+t[0], base+t[2], base+t[1]});
                else      m.indices.insert(m.indices.end(), {base+t[0], base+t[1], base+t[2]});
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
// Embedded GLB loading
// ---------------------------------------------------------------------------
//
// MC3 embeds are deliberately self-contained GLB assets.  Supporting a
// loose .gltf here would let tinygltf follow arbitrary buffer/image URIs from
// an MC3 document, bypassing the resource-path policy enforced by the caller.
// It would also make an inline <embed> ambiguous because it has no directory
// in which to resolve companion files.  A GLB has one binary payload and is
// therefore both portable and safely bounded as one resource.
namespace {

constexpr size_t kMaxEmbeddedGlbBytes = 64ull * 1024ull * 1024ull;
constexpr size_t kMaxEmbeddedTriangles = 300'000; // matches live-preview cap
constexpr int kMaxEmbeddedNodeDepth = 64;

[[noreturn]] void glbError(const std::string& source, const std::string& detail) {
    throw std::runtime_error("Embedded GLB load failed (" + source + "): " + detail);
}

std::string lowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

std::vector<unsigned char> decodeEmbedBase64(const std::string& encoded,
                                             const std::string& source) {
    std::string compact;
    compact.reserve(encoded.size());
    for (unsigned char ch : encoded) {
        if (std::isspace(ch)) continue;
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '+' || ch == '/' || ch == '=') {
            compact.push_back(static_cast<char>(ch));
        } else {
            glbError(source, "inline base64 contains an invalid character");
        }
    }
    if (compact.empty() || compact.size() % 4 != 0)
        glbError(source, "inline base64 has an invalid length");

    size_t padding = 0;
    if (!compact.empty() && compact.back() == '=') ++padding;
    if (compact.size() > 1 && compact[compact.size() - 2] == '=') ++padding;
    const size_t firstPadding = compact.find('=');
    if (padding > 2 || (padding == 0 ? firstPadding != std::string::npos
                                     : firstPadding != compact.size() - padding))
        glbError(source, "inline base64 has invalid padding");
    const size_t decodedBytes = compact.size() / 4 * 3 - padding;
    if (decodedBytes > kMaxEmbeddedGlbBytes)
        glbError(source, "decoded inline GLB exceeds the 64 MiB safety limit");

    auto sextet = [&](char ch) -> unsigned char {
        if (ch >= 'A' && ch <= 'Z') return static_cast<unsigned char>(ch - 'A');
        if (ch >= 'a' && ch <= 'z') return static_cast<unsigned char>(ch - 'a' + 26);
        if (ch >= '0' && ch <= '9') return static_cast<unsigned char>(ch - '0' + 52);
        if (ch == '+') return 62;
        if (ch == '/') return 63;
        glbError(source, "inline base64 has invalid padding placement");
    };

    std::vector<unsigned char> bytes;
    bytes.reserve(decodedBytes);
    for (size_t i = 0; i < compact.size(); i += 4) {
        const char c2 = compact[i + 2];
        const char c3 = compact[i + 3];
        if ((c2 == '=' || c3 == '=') && i + 4 != compact.size())
            glbError(source, "inline base64 padding appears before the final quartet");
        const unsigned char a = sextet(compact[i]);
        const unsigned char b = sextet(compact[i + 1]);
        const unsigned char c = c2 == '=' ? 0 : sextet(c2);
        const unsigned char d = c3 == '=' ? 0 : sextet(c3);
        bytes.push_back(static_cast<unsigned char>((a << 2) | (b >> 4)));
        if (c2 != '=') bytes.push_back(static_cast<unsigned char>((b << 4) | (c >> 2)));
        if (c3 != '=') bytes.push_back(static_cast<unsigned char>((c << 6) | d));
    }
    return bytes;
}

struct GlbMat4 {
    // Column-major, matching glTF's matrix storage and p' = M * p.
    std::array<double, 16> m{1.0, 0.0, 0.0, 0.0,
                             0.0, 1.0, 0.0, 0.0,
                             0.0, 0.0, 1.0, 0.0,
                             0.0, 0.0, 0.0, 1.0};

    static GlbMat4 multiply(const GlbMat4& a, const GlbMat4& b) {
        GlbMat4 out{};
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row) {
                out.m[col * 4 + row] = 0.0;
                for (int k = 0; k < 4; ++k)
                    out.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
        return out;
    }

    static GlbMat4 fromNode(const tinygltf::Node& node) {
        if (node.matrix.size() == 16) {
            GlbMat4 result{};
            std::copy(node.matrix.begin(), node.matrix.end(), result.m.begin());
            return result;
        }
        const double tx = node.translation.size() == 3 ? node.translation[0] : 0.0;
        const double ty = node.translation.size() == 3 ? node.translation[1] : 0.0;
        const double tz = node.translation.size() == 3 ? node.translation[2] : 0.0;
        const double qx = node.rotation.size() == 4 ? node.rotation[0] : 0.0;
        const double qy = node.rotation.size() == 4 ? node.rotation[1] : 0.0;
        const double qz = node.rotation.size() == 4 ? node.rotation[2] : 0.0;
        const double qw = node.rotation.size() == 4 ? node.rotation[3] : 1.0;
        const double sx = node.scale.size() == 3 ? node.scale[0] : 1.0;
        const double sy = node.scale.size() == 3 ? node.scale[1] : 1.0;
        const double sz = node.scale.size() == 3 ? node.scale[2] : 1.0;
        const double xx = qx * qx, yy = qy * qy, zz = qz * qz;
        const double xy = qx * qy, xz = qx * qz, yz = qy * qz;
        const double wx = qw * qx, wy = qw * qy, wz = qw * qz;
        GlbMat4 result{};
        result.m = {
            (1.0 - 2.0 * (yy + zz)) * sx, (2.0 * (xy + wz)) * sx,       (2.0 * (xz - wy)) * sx,       0.0,
            (2.0 * (xy - wz)) * sy,       (1.0 - 2.0 * (xx + zz)) * sy, (2.0 * (yz + wx)) * sy,       0.0,
            (2.0 * (xz + wy)) * sz,       (2.0 * (yz - wx)) * sz,       (1.0 - 2.0 * (xx + yy)) * sz, 0.0,
            tx,                             ty,                             tz,                             1.0
        };
        return result;
    }

    std::array<float, 3> transformPoint(const std::array<float, 3>& p) const {
        return {
            static_cast<float>(m[0] * p[0] + m[4] * p[1] + m[8]  * p[2] + m[12]),
            static_cast<float>(m[1] * p[0] + m[5] * p[1] + m[9]  * p[2] + m[13]),
            static_cast<float>(m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14])
        };
    }

    std::array<float, 3> transformNormal(const std::array<float, 3>& n,
                                         const std::string& source) const {
        const double a00 = m[0], a01 = m[4], a02 = m[8];
        const double a10 = m[1], a11 = m[5], a12 = m[9];
        const double a20 = m[2], a21 = m[6], a22 = m[10];
        const double c00 = a11 * a22 - a12 * a21;
        const double c01 = a12 * a20 - a10 * a22;
        const double c02 = a10 * a21 - a11 * a20;
        const double c10 = a02 * a21 - a01 * a22;
        const double c11 = a00 * a22 - a02 * a20;
        const double c12 = a01 * a20 - a00 * a21;
        const double c20 = a01 * a12 - a02 * a11;
        const double c21 = a02 * a10 - a00 * a12;
        const double c22 = a00 * a11 - a01 * a10;
        const double determinant = a00 * c00 + a01 * c01 + a02 * c02;
        if (std::fabs(determinant) < 1e-12)
            glbError(source, "a scene node has a singular transform, so normals are undefined");
        std::array<float, 3> out{
            static_cast<float>((c00 * n[0] + c01 * n[1] + c02 * n[2]) / determinant),
            static_cast<float>((c10 * n[0] + c11 * n[1] + c12 * n[2]) / determinant),
            static_cast<float>((c20 * n[0] + c21 * n[1] + c22 * n[2]) / determinant)
        };
        const float length = std::sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
        if (!(length > 1e-12f) || !std::isfinite(length))
            glbError(source, "a scene node produced a non-finite normal");
        out[0] /= length; out[1] /= length; out[2] /= length;
        return out;
    }
};

const tinygltf::Accessor& checkedAccessor(const tinygltf::Model& model, int index,
                                          int componentType, int type,
                                          const std::string& source, const char* label) {
    if (index < 0 || index >= static_cast<int>(model.accessors.size()))
        glbError(source, std::string(label) + " accessor index is invalid");
    const auto& accessor = model.accessors[index];
    if (accessor.componentType != componentType || accessor.type != type ||
        accessor.bufferView < 0 || accessor.sparse.isSparse)
        glbError(source, std::string(label) + " must be a non-sparse supported accessor");
    if (accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
        glbError(source, std::string(label) + " buffer view index is invalid");
    const auto& view = model.bufferViews[accessor.bufferView];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size()))
        glbError(source, std::string(label) + " buffer index is invalid");
    const size_t elementBytes = type == TINYGLTF_TYPE_VEC3 ? 12 : 8;
    const size_t stride = view.byteStride ? view.byteStride : elementBytes;
    if (stride < elementBytes || accessor.byteOffset > view.byteLength ||
        accessor.count > (std::numeric_limits<size_t>::max() - accessor.byteOffset) / stride ||
        accessor.count * stride > view.byteLength - accessor.byteOffset)
        glbError(source, std::string(label) + " range exceeds its buffer view");
    if (view.byteOffset > model.buffers[view.buffer].data.size() ||
        view.byteLength > model.buffers[view.buffer].data.size() - view.byteOffset)
        glbError(source, std::string(label) + " buffer view exceeds its buffer");
    return accessor;
}

std::array<float, 3> readVec3(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
                              size_t element, const std::string& source, const char* label) {
    const auto& view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    const size_t stride = view.byteStride ? view.byteStride : 12;
    const size_t offset = view.byteOffset + accessor.byteOffset + element * stride;
    std::array<float, 3> value{};
    std::memcpy(value.data(), buffer.data.data() + offset, sizeof(float) * 3);
    for (float component : value)
        if (!std::isfinite(component)) glbError(source, std::string(label) + " contains a non-finite value");
    return value;
}

std::array<float, 2> readVec2(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
                              size_t element, const std::string& source) {
    const auto& view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    const size_t stride = view.byteStride ? view.byteStride : 8;
    const size_t offset = view.byteOffset + accessor.byteOffset + element * stride;
    std::array<float, 2> value{};
    std::memcpy(value.data(), buffer.data.data() + offset, sizeof(float) * 2);
    for (float component : value)
        if (!std::isfinite(component)) glbError(source, "TEXCOORD_0 contains a non-finite value");
    return value;
}

const tinygltf::Accessor& checkedIndexAccessor(const tinygltf::Model& model, int index,
                                                const std::string& source) {
    if (index < 0 || index >= static_cast<int>(model.accessors.size()))
        glbError(source, "index accessor index is invalid");
    const auto& accessor = model.accessors[index];
    if ((accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE &&
         accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT &&
         accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) ||
        accessor.type != TINYGLTF_TYPE_SCALAR || accessor.bufferView < 0 || accessor.sparse.isSparse)
        glbError(source, "indices must use an unsigned non-sparse scalar accessor");
    if (accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
        glbError(source, "index accessor buffer view index is invalid");
    const auto& view = model.bufferViews[accessor.bufferView];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size()))
        glbError(source, "index accessor buffer index is invalid");
    const size_t bytes = accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ? 1
                       : accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 2 : 4;
    const size_t stride = view.byteStride ? view.byteStride : bytes;
    if (stride < bytes || accessor.byteOffset > view.byteLength ||
        accessor.count > (std::numeric_limits<size_t>::max() - accessor.byteOffset) / stride ||
        accessor.count * stride > view.byteLength - accessor.byteOffset ||
        view.byteOffset > model.buffers[view.buffer].data.size() ||
        view.byteLength > model.buffers[view.buffer].data.size() - view.byteOffset)
        glbError(source, "index accessor range exceeds its buffer");
    return accessor;
}

uint32_t readIndex(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t element) {
    const auto& view = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[view.buffer];
    const size_t bytes = accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ? 1
                       : accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 2 : 4;
    const size_t stride = view.byteStride ? view.byteStride : bytes;
    const unsigned char* ptr = buffer.data.data() + view.byteOffset + accessor.byteOffset + element * stride;
    if (bytes == 1) return *ptr;
    if (bytes == 2) {
        uint16_t value{};
        std::memcpy(&value, ptr, sizeof(value));
        return value;
    }
    uint32_t value{};
    std::memcpy(&value, ptr, sizeof(value));
    return value;
}

std::array<float, 3> faceNormal(const std::array<float, 3>& p0,
                                const std::array<float, 3>& p1,
                                const std::array<float, 3>& p2,
                                const std::string& source) {
    std::array<float, 3> normal{
        (p1[1] - p0[1]) * (p2[2] - p0[2]) - (p1[2] - p0[2]) * (p2[1] - p0[1]),
        (p1[2] - p0[2]) * (p2[0] - p0[0]) - (p1[0] - p0[0]) * (p2[2] - p0[2]),
        (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p1[1] - p0[1]) * (p2[0] - p0[0])
    };
    const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (!(length > 1e-12f) || !std::isfinite(length))
        glbError(source, "contains a degenerate triangle without normals");
    normal[0] /= length; normal[1] /= length; normal[2] /= length;
    return normal;
}

void appendPrimitive(MeshData& out, const tinygltf::Model& model,
                     const tinygltf::Primitive& primitive, const GlbMat4& world,
                     const std::string& source) {
    if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
        glbError(source, "contains a non-triangle mesh primitive");
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end())
        glbError(source, "a mesh primitive has no POSITION accessor");
    const auto& positions = checkedAccessor(model, posIt->second, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                            TINYGLTF_TYPE_VEC3, source, "POSITION");
    const tinygltf::Accessor* normals = nullptr;
    if (auto it = primitive.attributes.find("NORMAL"); it != primitive.attributes.end()) {
        normals = &checkedAccessor(model, it->second, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                   TINYGLTF_TYPE_VEC3, source, "NORMAL");
        if (normals->count != positions.count)
            glbError(source, "NORMAL count does not match POSITION count");
    }
    const tinygltf::Accessor* texcoords = nullptr;
    if (auto it = primitive.attributes.find("TEXCOORD_0"); it != primitive.attributes.end()) {
        texcoords = &checkedAccessor(model, it->second, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                     TINYGLTF_TYPE_VEC2, source, "TEXCOORD_0");
        if (texcoords->count != positions.count)
            glbError(source, "TEXCOORD_0 count does not match POSITION count");
    }

    std::vector<uint32_t> indices;
    if (primitive.indices >= 0) {
        const auto& indexAccessor = checkedIndexAccessor(model, primitive.indices, source);
        if (indexAccessor.count % 3 != 0) glbError(source, "index count is not divisible by three");
        indices.reserve(indexAccessor.count);
        for (size_t i = 0; i < indexAccessor.count; ++i) {
            const uint32_t value = readIndex(model, indexAccessor, i);
            if (value >= positions.count) glbError(source, "an index is outside POSITION");
            indices.push_back(value);
        }
    } else {
        if (positions.count % 3 != 0) glbError(source, "non-indexed POSITION count is not divisible by three");
        indices.reserve(positions.count);
        for (size_t i = 0; i < positions.count; ++i) indices.push_back(static_cast<uint32_t>(i));
    }
    if (indices.size() / 3 > kMaxEmbeddedTriangles - out.indices.size() / 3)
        glbError(source, "combined embedded geometry exceeds the 300000-triangle safety limit");

    for (size_t i = 0; i < indices.size(); i += 3) {
        std::array<std::array<float, 3>, 3> points{};
        for (int vertex = 0; vertex < 3; ++vertex)
            points[vertex] = world.transformPoint(readVec3(model, positions, indices[i + vertex], source, "POSITION"));
        const std::array<float, 3> fallbackNormal = normals
            ? std::array<float, 3>{0.0f, 0.0f, 0.0f}
            : faceNormal(points[0], points[1], points[2], source);
        for (int vertex = 0; vertex < 3; ++vertex) {
            const uint32_t sourceIndex = indices[i + vertex];
            const auto normal = normals
                ? world.transformNormal(readVec3(model, *normals, sourceIndex, source, "NORMAL"), source)
                : fallbackNormal;
            const auto uv = texcoords
                ? readVec2(model, *texcoords, sourceIndex, source)
                : std::array<float, 2>{0.0f, 0.0f};
            out.positions.insert(out.positions.end(), points[vertex].begin(), points[vertex].end());
            out.normals.insert(out.normals.end(), normal.begin(), normal.end());
            out.texcoords.insert(out.texcoords.end(), uv.begin(), uv.end());
            out.indices.push_back(static_cast<uint32_t>(out.indices.size()));
        }
    }
}

void appendNode(MeshData& out, const tinygltf::Model& model, int nodeIndex,
                const GlbMat4& parent, std::vector<bool>& active,
                int depth, const std::string& source) {
    if (depth > kMaxEmbeddedNodeDepth) glbError(source, "scene hierarchy exceeds depth 64");
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size()))
        glbError(source, "scene references an invalid node");
    if (active[nodeIndex]) glbError(source, "scene hierarchy contains a node cycle");
    active[nodeIndex] = true;
    const auto& node = model.nodes[nodeIndex];
    const GlbMat4 world = GlbMat4::multiply(parent, GlbMat4::fromNode(node));
    if (node.mesh >= 0) {
        if (node.mesh >= static_cast<int>(model.meshes.size())) glbError(source, "node references an invalid mesh");
        for (const auto& primitive : model.meshes[node.mesh].primitives)
            appendPrimitive(out, model, primitive, world, source);
    }
    for (int child : node.children)
        appendNode(out, model, child, world, active, depth + 1, source);
    active[nodeIndex] = false;
}

MeshData flattenEmbeddedGlb(const tinygltf::Model& model, const std::string& source) {
    if (model.scenes.empty()) glbError(source, "has no scene to import");
    const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= static_cast<int>(model.scenes.size())) glbError(source, "default scene index is invalid");
    MeshData result;
    std::vector<bool> active(model.nodes.size(), false);
    const GlbMat4 identity{};
    for (int node : model.scenes[sceneIndex].nodes)
        appendNode(result, model, node, identity, active, 0, source);
    if (result.empty()) glbError(source, "default scene contains no triangle geometry");
    return result;
}

MeshData selectEmbeddedGlbPrimitive(const tinygltf::Model& model,
                                    const EmbeddedGltfSelection& selection,
                                    const std::string& source) {
    if (selection.meshIndex < 0 || selection.meshIndex >= static_cast<int>(model.meshes.size()))
        glbError(source, "selected mesh index is invalid");
    const auto& mesh = model.meshes[static_cast<size_t>(selection.meshIndex)];
    if (selection.primitiveIndex < 0 ||
        selection.primitiveIndex >= static_cast<int>(mesh.primitives.size()))
        glbError(source, "selected primitive index is invalid");
    MeshData result;
    const GlbMat4 identity{};
    appendPrimitive(result, model, mesh.primitives[static_cast<size_t>(selection.primitiveIndex)],
                    identity, source);
    if (result.empty()) glbError(source, "selected primitive contains no triangle geometry");
    return result;
}

} // namespace

std::optional<EmbeddedGltfSelection>
parseEmbeddedGltfSelection(const std::map<std::string, std::string>& metadata)
{
    const auto mesh = metadata.find(std::string(kGltfMeshIndexMetadataKey));
    const auto primitive = metadata.find(std::string(kGltfPrimitiveIndexMetadataKey));
    if (mesh == metadata.end() && primitive == metadata.end()) return std::nullopt;
    if (mesh == metadata.end() || primitive == metadata.end()) return std::nullopt;
    auto parse = [](const std::string& value) -> std::optional<int> {
        if (value.empty()) return std::nullopt;
        int parsed = -1;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} || end != value.data() + value.size() || parsed < 0)
            return std::nullopt;
        return parsed;
    };
    const auto meshIndex = parse(mesh->second);
    const auto primitiveIndex = parse(primitive->second);
    if (!meshIndex || !primitiveIndex) return std::nullopt;
    return EmbeddedGltfSelection{*meshIndex, *primitiveIndex};
}

MeshData loadEmbeddedGltfMesh(const std::filesystem::path& basePath,
                              const MeshCraft::Mc3::Mc3EmbedGltf& embed,
                              std::optional<EmbeddedGltfSelection> selection) {
    const std::string source = embed.id.empty() ? "unnamed embed" : "embed:" + embed.id;
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string error, warning;
    bool loaded = false;
    if (embed.isExternal()) {
        std::filesystem::path path = embed.src;
        if (path.is_relative()) path = basePath / path;
        if (lowerExtension(path) != ".glb")
            glbError(source, "external embeds must be self-contained .glb files");
        std::error_code ec;
        const uintmax_t bytes = std::filesystem::file_size(path, ec);
        if (ec) glbError(source, "cannot inspect external GLB '" + path.string() + "'");
        if (bytes > kMaxEmbeddedGlbBytes)
            glbError(source, "external GLB exceeds the 64 MiB safety limit");
        loaded = loader.LoadBinaryFromFile(&model, &error, &warning, path.string());
    } else if (embed.isInline()) {
        const std::vector<unsigned char> bytes = decodeEmbedBase64(embed.base64Content, source);
        loaded = loader.LoadBinaryFromMemory(&model, &error, &warning, bytes.data(),
                                              static_cast<unsigned int>(bytes.size()), basePath.string());
    } else {
        glbError(source, "has neither an external source nor inline base64 data");
    }
    if (!loaded) glbError(source, error.empty() ? "tinygltf rejected the GLB" : error);
    if (!warning.empty()) std::cerr << "Embedded GLB warning (" << source << "): " << warning << '\n';
    return selection ? selectEmbeddedGlbPrimitive(model, *selection, source)
                     : flattenEmbeddedGlb(model, source);
}

// ---------------------------------------------------------------------------
// OBJ mesh loading
// ---------------------------------------------------------------------------

std::optional<int> parseObjMaterialIndex(std::string_view value)
{
    if (value.empty()) return std::nullopt;
    int parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size()) return std::nullopt;
    return parsed;
}

namespace {

float objMaterialValue(float value, const std::filesystem::path& path,
                       const std::string& materialName, const char* property)
{
    if (std::isfinite(value)) return std::clamp(value, 0.0f, 1.0f);
    throw std::runtime_error("OBJ load failed (" + path.string() + "): MTL material '" +
                             materialName + "' has non-finite " + property);
}

bool differsFromZero(const float (&values)[3])
{
    return values[0] != 0.0f || values[1] != 0.0f || values[2] != 0.0f;
}

void addUnsupportedMaterialWarnings(const tinyobj::material_t& source,
                                    std::vector<std::string>& warnings)
{
    const std::string materialName = source.name.empty() ? "<unnamed>" : source.name;
    auto warn = [&](const std::string& property) {
        warnings.push_back("MTL material '" + materialName + "': " + property +
                           " is not faithfully represented in MC3");
    };
    if (differsFromZero(source.ambient))       warn("ambient color (Ka)");
    if (differsFromZero(source.specular))      warn("specular color (Ks)");
    if (differsFromZero(source.transmittance)) warn("transmittance color (Tf)");
    if (source.shininess != 0.0f)              warn("specular exponent (Ns)");
    if (source.ior != 1.0f)                    warn("index of refraction (Ni)");
    if (source.illum != 0)                     warn("illumination model (illum)");
    for (const auto& [name, value] : std::initializer_list<std::pair<const char*, const std::string*>>{
             {"ambient texture (map_Ka)", &source.ambient_texname},
             {"diffuse texture (map_Kd)", &source.diffuse_texname},
             {"specular texture (map_Ks)", &source.specular_texname},
             {"specular-highlight texture (map_Ns)", &source.specular_highlight_texname},
             {"bump texture (map_bump)", &source.bump_texname},
             {"displacement texture (disp)", &source.displacement_texname},
             {"alpha texture (map_d)", &source.alpha_texname},
             {"reflection texture (refl)", &source.reflection_texname},
             {"roughness texture (map_Pr)", &source.roughness_texname},
             {"metallic texture (map_Pm)", &source.metallic_texname},
             {"sheen texture (map_Ps)", &source.sheen_texname},
             {"emissive texture (map_Ke)", &source.emissive_texname},
             {"normal texture (norm)", &source.normal_texname},
         }) {
        if (!value->empty()) warn(name);
    }
    if (source.sheen != 0.0f)               warn("sheen (Ps)");
    if (source.clearcoat_thickness != 0.0f) warn("clearcoat thickness (Pc)");
    if (source.clearcoat_roughness != 0.0f) warn("clearcoat roughness (Pcr)");
    if (source.anisotropy != 0.0f)          warn("anisotropy (aniso)");
    if (source.anisotropy_rotation != 0.0f) warn("anisotropy rotation (anisor)");
    for (const auto& [name, _] : source.unknown_parameter) warn("unknown property '" + name + "'");
}

MeshCraft::Mc3::Mc3Material mapObjMaterial(const tinyobj::material_t& source,
                                           const std::filesystem::path& path,
                                           std::vector<std::string>& warnings)
{
    MeshCraft::Mc3::Mc3Material target;
    target.name = source.name;
    target.baseColor = {
        objMaterialValue(source.diffuse[0], path, source.name, "diffuse red"),
        objMaterialValue(source.diffuse[1], path, source.name, "diffuse green"),
        objMaterialValue(source.diffuse[2], path, source.name, "diffuse blue"),
        objMaterialValue(source.dissolve, path, source.name, "dissolve"),
    };
    target.roughness = objMaterialValue(source.roughness, path, source.name, "roughness");
    target.metallic = objMaterialValue(source.metallic, path, source.name, "metallic");
    target.emissiveColor = {
        objMaterialValue(source.emission[0], path, source.name, "emission red"),
        objMaterialValue(source.emission[1], path, source.name, "emission green"),
        objMaterialValue(source.emission[2], path, source.name, "emission blue"),
    };
    if (target.baseColor[3] < 1.0f) target.alphaMode = "blend";
    addUnsupportedMaterialWarnings(source, warnings);
    return target;
}

} // namespace

ObjMaterialImportResult importObjMaterialGroups(const std::filesystem::path& basePath,
                                                const std::string& source)
{
    std::filesystem::path objPath = source;
    if (objPath.is_relative()) objPath = basePath / objPath;

    tinyobj::ObjReaderConfig cfg;
    cfg.mtl_search_path = objPath.parent_path().string();
    cfg.triangulate = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(objPath.string(), cfg)) {
        throw std::runtime_error("OBJ load failed (" + objPath.string() + "): " + reader.Error());
    }

    ObjMaterialImportResult imported;
    if (!reader.Warning().empty()) imported.warnings.push_back("OBJ parser: " + reader.Warning());
    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    // Reject non-finite vertex coordinates (e.g. "1e400" overflowing to inf)
    // rather than silently exporting a spec-invalid glTF (inf in the binary
    // buffer, null in the JSON accessor min/max).
    for (float value : attrib.vertices) {
        if (!std::isfinite(value)) {
            throw std::runtime_error("OBJ load failed (" + objPath.string() +
                                     "): non-finite vertex coordinate");
        }
    }

    // AUD-002: tinyobjloader's plain triangle fast path does not reject all
    // out-of-range indices. Validate every source index before dereferencing.
    auto checkIndex = [&](int index, size_t arraySize, int stride, const char* what) {
        if (index < 0 || static_cast<size_t>(index) * static_cast<size_t>(stride) +
                             static_cast<size_t>(stride - 1) >= arraySize) {
            throw std::runtime_error("OBJ load failed (" + objPath.string() + "): " + what +
                                     " index " + std::to_string(index) + " out of range (array has " +
                                     std::to_string(arraySize / static_cast<size_t>(stride)) + " entries)");
        }
    };

    auto groupFor = [&](int materialIndex) -> ObjMaterialGroup& {
        auto existing = std::find_if(imported.groups.begin(), imported.groups.end(),
                                     [materialIndex](const ObjMaterialGroup& group) {
                                         return group.materialIndex == materialIndex;
                                     });
        if (existing != imported.groups.end()) return *existing;
        ObjMaterialGroup group;
        group.materialIndex = materialIndex;
        if (materialIndex >= 0 && static_cast<size_t>(materialIndex) < materials.size()) {
            group.materialName = materials[static_cast<size_t>(materialIndex)].name;
            group.material = mapObjMaterial(materials[static_cast<size_t>(materialIndex)], objPath,
                                            imported.warnings);
        } else if (materialIndex >= 0) {
            imported.warnings.push_back("OBJ face references unavailable MTL material index " +
                                        std::to_string(materialIndex));
        }
        imported.groups.push_back(std::move(group));
        return imported.groups.back();
    };

    for (const auto& shape : shapes) {
        const auto& indices = shape.mesh.indices;
        size_t offset = 0;
        for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
            const size_t vertexCount = shape.mesh.num_face_vertices[face];
            if (offset + vertexCount > indices.size()) {
                throw std::runtime_error("OBJ load failed (" + objPath.string() +
                                         "): face index list is truncated");
            }
            if (vertexCount != 3) {
                throw std::runtime_error("OBJ load failed (" + objPath.string() +
                                         "): triangulation produced a non-triangle face");
            }
            const int materialIndex = face < shape.mesh.material_ids.size()
                ? shape.mesh.material_ids[face] : -1;
            ObjMaterialGroup& group = groupFor(materialIndex);
            const tinyobj::index_t* triangle[3] = {
                &indices[offset], &indices[offset + 1], &indices[offset + 2]};

            std::array<float,3> faceNormal{0.0f, 1.0f, 0.0f};
            const bool needFaceNormal = triangle[0]->normal_index < 0 ||
                                        triangle[1]->normal_index < 0 ||
                                        triangle[2]->normal_index < 0;
            if (needFaceNormal) {
                auto position = [&](int corner) -> std::array<float,3> {
                    checkIndex(triangle[corner]->vertex_index, attrib.vertices.size(), 3, "vertex");
                    const size_t index = static_cast<size_t>(triangle[corner]->vertex_index);
                    return {attrib.vertices[index * 3], attrib.vertices[index * 3 + 1],
                            attrib.vertices[index * 3 + 2]};
                };
                const auto p0 = position(0), p1 = position(1), p2 = position(2);
                faceNormal = cross3({p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]},
                                    {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]});
                norm3(faceNormal);
            }

            for (int corner = 0; corner < 3; ++corner) {
                const auto& index = *triangle[corner];
                checkIndex(index.vertex_index, attrib.vertices.size(), 3, "vertex");
                const size_t vertex = static_cast<size_t>(index.vertex_index);
                group.mesh.positions.insert(group.mesh.positions.end(), {
                    attrib.vertices[vertex * 3], attrib.vertices[vertex * 3 + 1],
                    attrib.vertices[vertex * 3 + 2]});
                if (index.normal_index >= 0) {
                    checkIndex(index.normal_index, attrib.normals.size(), 3, "normal");
                    const size_t normal = static_cast<size_t>(index.normal_index);
                    group.mesh.normals.insert(group.mesh.normals.end(), {
                        attrib.normals[normal * 3], attrib.normals[normal * 3 + 1],
                        attrib.normals[normal * 3 + 2]});
                } else {
                    group.mesh.normals.insert(group.mesh.normals.end(),
                                              {faceNormal[0], faceNormal[1], faceNormal[2]});
                }
                if (index.texcoord_index >= 0) {
                    checkIndex(index.texcoord_index, attrib.texcoords.size(), 2, "texcoord");
                    const size_t texcoord = static_cast<size_t>(index.texcoord_index);
                    group.mesh.texcoords.insert(group.mesh.texcoords.end(), {
                        attrib.texcoords[texcoord * 2], 1.0f - attrib.texcoords[texcoord * 2 + 1]});
                } else {
                    group.mesh.texcoords.insert(group.mesh.texcoords.end(), {0.0f, 0.0f});
                }
                group.mesh.indices.push_back(static_cast<uint32_t>(group.mesh.indices.size()));
            }
            offset += vertexCount;
        }
        if (offset != indices.size()) {
            throw std::runtime_error("OBJ load failed (" + objPath.string() +
                                     "): face index list has trailing entries");
        }
    }
    return imported;
}

MeshData loadObjMesh(const std::filesystem::path& basePath, const std::string& source,
                     std::optional<int> materialIndex)
{
    ObjMaterialImportResult imported = importObjMaterialGroups(basePath, source);
    MeshData merged;
    for (auto& group : imported.groups) {
        if (materialIndex.has_value() && group.materialIndex != *materialIndex) continue;
        const uint32_t offset = static_cast<uint32_t>(merged.positions.size() / 3);
        merged.positions.insert(merged.positions.end(), group.mesh.positions.begin(), group.mesh.positions.end());
        merged.normals.insert(merged.normals.end(), group.mesh.normals.begin(), group.mesh.normals.end());
        merged.texcoords.insert(merged.texcoords.end(), group.mesh.texcoords.begin(), group.mesh.texcoords.end());
        for (uint32_t index : group.mesh.indices) merged.indices.push_back(offset + index);
    }
    return merged;
}

} // namespace mc3togltf
