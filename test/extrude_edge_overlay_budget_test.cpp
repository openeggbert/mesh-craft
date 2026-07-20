// 2026-07-20 audit F5: SceneRenderer_Extrude.cpp's drawObjectEdges()
// Extrude case had no vertex-budget guard at all, unlike its solid-mesh
// sibling drawExtrudeDynamic() (AUD-072, same file) -- see
// extrudeEdgeOverlayExceedsVertexBudgetAlg()'s own doc comment
// (PrimitiveTessellationAlg.hpp) for the full reasoning on why this is
// reachable (a loaded file's cross-section point count, not the
// interactive UI) and why the fix landed as an extracted pure function
// rather than an inline formula: drawObjectEdges() itself is CNA-coupled
// (needs a live GL device) AND its edge-overlay code path is gated behind
// a showEdgeOverlay_/showWireframeMode_ UI toggle that defaults off with
// no headless CLI equivalent (unlike drawExtrudeDynamic()'s always-on
// solid-mesh path, which AUD-072's own smoke-test timing check could
// exercise directly). This test calls the REAL extracted function
// directly -- not a mirrored duplicate -- so there is zero drift risk
// between what's tested and what SceneRenderer_Extrude.cpp actually runs.

#include <MeshCraft/Renderer/PrimitiveTessellationAlg.hpp>

#include <iostream>
#include <string>

using namespace MeshCraft::Renderer;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

int main() {
    // Reproduces the exact scenario the audit found reachable: path
    // segments already capped to 20 (SceneRenderer_Extrude.cpp's own
    // `std::min(ex.segments, 20)`), cross-section points near the parser
    // ceiling (kMaxTessellation=4096) -- (hollow?4:2)*20*4096 vastly
    // exceeds any sane vertex budget.
    check(extrudeEdgeOverlayExceedsVertexBudgetAlg(20, 4096, false),
          "M=20, N=4096, solid: exceeds the budget (matches the audit's own reachable scenario)");
    check(extrudeEdgeOverlayExceedsVertexBudgetAlg(20, 4096, true),
          "M=20, N=4096, hollow: exceeds the budget (hollow doubles the vertex count again)");

    // A genuinely ordinary scene (small M, small N, well within the
    // interactive UI's own 64-max slider range) must NOT be flagged --
    // the fix must not be a false-positive trap on real content.
    check(!extrudeEdgeOverlayExceedsVertexBudgetAlg(20, 32, false),
          "M=20, N=32, solid: an ordinary scene stays under budget");
    check(!extrudeEdgeOverlayExceedsVertexBudgetAlg(20, 32, true),
          "M=20, N=32, hollow: an ordinary hollow scene stays under budget");

    // Hollow exactly doubles the solid-case vertex count (matches the
    // ring-building loop's own behavior: an inner ring is built in
    // addition to the outer one) -- confirms the multiplier itself, not
    // just "some" difference between hollow and solid.
    {
        // Find an N where solid is just under budget but hollow (2x) tips
        // it over -- proves hollow's multiplier is exactly 2x, not some
        // other factor.
        const int M = 20;
        int nSolidOk = 0;
        for (int n = 1; n <= 4096; ++n) {
            if (extrudeEdgeOverlayExceedsVertexBudgetAlg(M, n, false)) break;
            nSolidOk = n;
        }
        check(nSolidOk > 0 && nSolidOk < 4096,
              "found an N where solid is right at the budget boundary (N=" +
              std::to_string(nSolidOk) + ")");
        check(!extrudeEdgeOverlayExceedsVertexBudgetAlg(M, nSolidOk, false),
              "that N is confirmed still under budget for solid");
        check(extrudeEdgeOverlayExceedsVertexBudgetAlg(M, nSolidOk, true),
              "the SAME N, hollow, exceeds the budget (hollow's 2x multiplier tips it over "
              "right at the boundary where solid didn't)");
    }

    // Boundary: exactly at maxVerts must NOT exceed; one over must.
    check(!extrudeEdgeOverlayExceedsVertexBudgetAlg(1, 1, false, 2),
          "boundary: exactly at maxVerts (2*1*1=2, cap 2) does not exceed");
    check(extrudeEdgeOverlayExceedsVertexBudgetAlg(1, 1, false, 1),
          "boundary: one over maxVerts (2*1*1=2, cap 1) exceeds");

    if (failures == 0) { std::cout << "All extrude-edge-overlay-budget tests passed.\n"; return 0; }
    std::cerr << failures << " extrude-edge-overlay-budget test(s) FAILED.\n";
    return 1;
}
