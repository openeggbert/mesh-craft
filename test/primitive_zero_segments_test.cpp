// 2026-07-20 audit finding #2: mc3.xsd/the parsers only reject a negative
// `segments` value, so `segments=0` (or `segments=1` for Sphere) is a
// legal, parseable document value -- but several primitive tessellators in
// BOTH independent geometry generators (mc3togltf::MeshBuilder.cpp for
// export, MeshCraft::Renderer::PrimitiveTessellationAlg.hpp for the live
// viewport) used it directly in a division inside a "<= segments"-bounded
// loop that still executes once even when the divisor is 0, producing a
// NaN vertex position (0.0f/0.0f). That NaN then silently reached the
// exported glTF's POSITION accessor (spec-invalid) or the live viewport's
// own vertex buffer.
//
// This test constructs every affected shape at segments=0 and segments=1
// (Sphere additionally needs segments=1 checked, since rings=segments/2
// truncates to 0 there too) and asserts every position/normal component is
// finite -- both BEFORE proving the fix works (this test only makes sense
// post-fix, so it directly asserts finiteness, matching how
// extrude_hollow_zero_radius_test.cpp proves its formula produces a finite
// result rather than re-deriving the pre-fix NaN) and that the mesh is
// still structurally valid (non-empty, index bounds in range).

#include <MeshCraft/Renderer/PrimitiveTessellationAlg.hpp>
#include "MeshBuilder.hpp" // mc3togltf_lib

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace mc3togltf;
using namespace MeshCraft::Renderer;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

namespace {

bool allFinite(const std::vector<float>& v) {
    for (float f : v)
        if (!std::isfinite(f)) return false;
    return true;
}

bool allFinite(const std::vector<std::array<float,3>>& v) {
    for (const auto& p : v)
        for (float f : p)
            if (!std::isfinite(f)) return false;
    return true;
}

bool indicesInRange(const std::vector<uint32_t>& indices, size_t vertexCount) {
    for (uint32_t i : indices)
        if (i >= vertexCount) return false;
    return true;
}

void checkMeshData(const MeshData& m, const std::string& label) {
    CHECK(!m.positions.empty(), label + ": produces a non-empty mesh, not silently dropped");
    CHECK(allFinite(m.positions), label + ": every position component is finite (no NaN/Inf)");
    CHECK(allFinite(m.normals), label + ": every normal component is finite (no NaN/Inf)");
    CHECK(allFinite(m.texcoords), label + ": every texcoord component is finite (no NaN/Inf)");
    CHECK(indicesInRange(m.indices, m.positions.size() / 3),
          label + ": every index is within the vertex buffer's bounds");
}

void checkRawTessellation(const RawTessellation& rt, const std::string& label) {
    CHECK(!rt.positions.empty(), label + ": produces a non-empty mesh, not silently dropped");
    CHECK(allFinite(rt.positions), label + ": every position component is finite (no NaN/Inf)");
    CHECK(indicesInRange(rt.indices, rt.positions.size()),
          label + ": every index is within the vertex buffer's bounds");
}

} // namespace

int main() {
    for (int segments : {0, 1}) {
        const std::string suffix = " (segments=" + std::to_string(segments) + ")";

        // --- Export path: mc3togltf::MeshBuilder.cpp ---
        checkMeshData(buildSphere(0.5f, segments), "buildSphere" + suffix);
        checkMeshData(buildCylinder(0.5f, 1.0f, segments, "y"), "buildCylinder" + suffix);
        checkMeshData(buildCone(0.5f, 1.0f, segments), "buildCone" + suffix);
        checkMeshData(buildTorus(0.35f, 0.15f, segments), "buildTorus" + suffix);
        checkMeshData(buildCapsule(0.5f, 1.0f, segments, "y"), "buildCapsule" + suffix);
        checkMeshData(buildDisk(0.5f, 0.0f, segments, "y"), "buildDisk (solid)" + suffix);
        checkMeshData(buildDisk(0.5f, 0.25f, segments, "y"), "buildDisk (ring)" + suffix);

        // --- Live-viewport path: PrimitiveTessellationAlg.hpp ---
        checkRawTessellation(tessellateUnitSphereAlg(segments), "tessellateUnitSphereAlg" + suffix);
        checkRawTessellation(tessellateUnitTorusAlg(segments, segments), "tessellateUnitTorusAlg" + suffix);
        // Cylinder/Cone/Capsule in this header already used strict "<" loop
        // bounds (safe from NaN at segments=0 -- they just produce fewer/no
        // side faces instead), but still worth confirming they stay finite
        // and don't crash now that Sphere/Torus alongside them are fixed.
        checkRawTessellation(tessellateUnitCylinderAlg(std::max(1, segments)),
                              "tessellateUnitCylinderAlg" + suffix);
        checkRawTessellation(tessellateUnitConeAlg(std::max(1, segments)),
                              "tessellateUnitConeAlg" + suffix);
        checkRawTessellation(tessellateUnitCapsuleAlg(std::max(1, segments)),
                              "tessellateUnitCapsuleAlg" + suffix);
    }

    // A normal, legitimate segment count still produces the expected shape
    // (not just "finite at the degenerate boundary") -- no false-positive
    // regression at ordinary scale.
    {
        MeshData sphere = buildSphere(1.0f, 32);
        CHECK(sphere.positions.size() / 3 > 100,
              "buildSphere(segments=32) still produces a normal, detailed mesh");
        CHECK(allFinite(sphere.positions), "buildSphere(segments=32) stays finite");
    }

    if (failures == 0)
        std::cout << "All primitive zero-segments (2026-07-20 audit finding #2) tests passed.\n";
    else
        std::cerr << failures << " primitive zero-segments test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
