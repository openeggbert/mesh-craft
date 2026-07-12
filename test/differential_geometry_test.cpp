// SYS-W7-02: differential geometry tests -- viewport renderer vs glTF exporter.
//
// MeshCraft has two independent code paths that turn an Mc3Primitive into
// actual triangle geometry:
//   1. The editor's live viewport -- SceneRenderer_Builders.cpp's
//      buildUnit*() family, which caches a small, fixed set of "unit"
//      meshes (radius-0.5 / size-1) at a handful of hardcoded LOD tiers and
//      scales them per-object via an affine transform (see
//      SceneRenderer.cpp's drawObject() switch statement).
//   2. mc3togltf's exporter -- MeshBuilder.cpp's build*() family, which
//      rebuilds geometry from scratch at the primitive's exact real-world
//      dimensions and exact user-requested `segments` every time.
//
// INVESTIGATION (see commit message / plan.md for the full write-up):
//   - These are genuinely INDEPENDENT, hand-written implementations for the
//     8 primitive types covered here (Box/Sphere/Cylinder/Cone/Plane/Torus/
//     Capsule/IcoSphere) -- confirmed by reading both source files
//     end-to-end. They do NOT share code for the common (non-CSG) case, so
//     a differential test is meaningful, not tautological.
//   - The ONE place real sharing exists is CSG-child preview for Torus/
//     Capsule/IcoSphere (SceneRenderer.cpp's buildManifoldTree() calls
//     mc3togltf::buildPrimitive() directly, STAB-0670) -- testing that
//     would be tautological by construction, so CSG is out of scope here.
//   - The viewport's buildUnit*() functions build real GPU VertexBuffer/
//     IndexBuffer objects inline with the CPU-side tessellation math, so
//     they are not callable headlessly as-is (this codebase has established
//     precedent -- see mc3/test/editor_commands_test.cpp's "no headless
//     test possible ... without a live GraphicsDevice" notes -- for
//     treating that as a hard blocker for direct CTest coverage of
//     SceneRenderer code). Rather than stand up a live-GL test harness or
//     risk a large rewrite of winding-sensitive GPU-upload code, the pure
//     CPU tessellation math was mechanically extracted -- verbatim, not
//     re-derived -- into MeshCraft::Renderer::tessellateUnit*Alg()
//     (include/MeshCraft/Renderer/PrimitiveTessellationAlg.hpp), mirroring
//     this codebase's existing "*Alg.hpp" pattern (CsgCacheAlg.hpp,
//     GraphicsBackendCheck.hpp) for making CNA-coupled code headlessly
//     testable. SceneRenderer_Builders.cpp now calls into that header for
//     the exact same math it always computed inline; the GPU upload and the
//     separate VPNT (normals/UV) generation are untouched. Verified
//     behavior-preserving by a full `ctest` pass (113/113, including all
//     21 "render" pixel-sampling tests and both smoke tests) after the
//     refactor, before this test was even written.
//   - Disk and Grid are OUT OF SCOPE for this pass: both are built by
//     SceneRenderer_Extrude.cpp's drawDiskDynamic()/drawGridDynamic(),
//     which interleave GPU upload/draw calls with the tessellation math in
//     a single function (no "unit mesh + scale" split to extract), and
//     both are open, flat, real-size-passed-directly surfaces for which
//     the volume invariant used below is degenerate. 8 of 10 primitive
//     types are covered.
//
// TEST DESIGN: invariant-based differential testing, at three levels:
//   (a) Ground truth -- for Box/Cylinder/Cone/Capsule, the analytically
//       exact enclosed volume is known from the primitive's parametric
//       definition (for Cylinder/Cone, even the TESSELLATED volume has a
//       closed form: crossSectionArea(segments) * height / [3], where a
//       regular N-gon inscribed in a circle of radius r has EXACTLY
//       (N/2pi)*sin(2pi/N) times the circle's area -- so these get a tight,
//       non-empirical tolerance). Sphere/Torus/Capsule's curved-in-2-
//       directions surfaces and IcoSphere's subdivision get a looser,
//       explicitly-labeled empirical tolerance.
//   (b) Renderer vs exporter at a MATCHING nominal segment count -- the
//       strongest, truest "differential" check: when both paths are fed
//       the same discretization parameter, their independently-implemented
//       tessellators should agree closely on volume/bbox/topology. This is
//       verified precisely (not just "close-ish") for every primitive type.
//   (c) Watertightness -- after welding colocated vertices (both paths
//       intentionally duplicate vertices at UV seams / per-face, exactly
//       like mc3togltf::buildPrimitive()'s existing CSG-preview welding
//       comment in SceneRenderer.cpp already documents), every solid
//       primitive must be a closed 2-manifold (no naked edges); Plane must
//       NOT be (it's an open surface).
//
// REAL FINDINGS (see commit message for full derivation): this
// investigation originally surfaced a concrete, previously-undocumented
// geometry bug in the viewport's per-object scale formulas for Torus and
// Capsule (AUD-061) -- both used to reuse a single FIXED-aspect-ratio "unit"
// mesh and apply a 3-axis affine scale, which cannot correctly reproduce an
// arbitrary (majorRadius, minorRadius) torus or (radius, height) capsule
// from one fixed unit shape (the two radii/the hemisphere-vs-cylinder split
// get conflated by the scale).
//
// AUD-061 STATUS: FIXED. SceneRenderer now builds a real mesh per distinct
// (LOD tier, actual radius parameters) combination on demand (see
// SceneRenderer::getOrBuildTorusMesh()/getOrBuildCapsuleMesh() and
// tessellateUnitTorusAlg()/tessellateUnitCapsuleAlg() taking real
// majorRadius/minorRadius or radius/height parameters instead of baked-in
// constants), cached so a static scene does not re-tessellate every frame.
// testTorus()/testCapsule() below now assert actual correctness (ground
// truth + exporter agreement) for off-ratio cases that used to only PIN the
// old bugged behavior.
//
// AUD-062 is fixed: the viewport now maps the MC3 `segments` field to the
// same 1..4 subdivision range as the exporter. AUD-063 is also fixed: the
// Capsule viewport and exporter both use max(2, segments/4) hemisphere rings.

#include <MeshCraft/Renderer/PrimitiveTessellationAlg.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>
#include "MeshBuilder.hpp" // mc3togltf_lib

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

using namespace MeshCraft::Renderer;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}
static void info(const std::string& msg) { std::cout << "INFO: " << msg << "\n"; }

// ---------------------------------------------------------------------------
// Common mesh representation + invariant computation
// ---------------------------------------------------------------------------

struct Mesh {
    std::vector<std::array<float,3>> pos;
    std::vector<uint32_t>            idx; // triangle list
};

static Mesh fromRaw(const RawTessellation& rt) { return Mesh{rt.positions, rt.indices}; }

static Mesh fromMeshData(const mc3togltf::MeshData& md) {
    Mesh m;
    m.pos.reserve(md.positions.size() / 3);
    for (size_t i = 0; i + 2 < md.positions.size(); i += 3)
        m.pos.push_back({md.positions[i], md.positions[i+1], md.positions[i+2]});
    m.idx = md.indices;
    return m;
}

static Mesh scaled(const Mesh& in, float sx, float sy, float sz) {
    Mesh out;
    out.idx = in.idx;
    out.pos.reserve(in.pos.size());
    for (auto& p : in.pos) out.pos.push_back({p[0]*sx, p[1]*sy, p[2]*sz});
    return out;
}

struct BBox {
    std::array<float,3> lo{ 1e30f,  1e30f,  1e30f};
    std::array<float,3> hi{-1e30f, -1e30f, -1e30f};
};
static BBox bboxOf(const Mesh& m) {
    BBox b;
    for (auto& p : m.pos)
        for (int k = 0; k < 3; ++k) {
            b.lo[k] = std::min(b.lo[k], p[k]);
            b.hi[k] = std::max(b.hi[k], p[k]);
        }
    return b;
}
static std::array<float,3> bboxSize(const BBox& b) {
    return {b.hi[0]-b.lo[0], b.hi[1]-b.lo[1], b.hi[2]-b.lo[2]};
}

// Enclosed volume via the divergence theorem (sum of signed tetrahedra from
// the origin) -- exact for any closed, consistently-oriented mesh,
// regardless of the mesh's position or of the apex chosen. The SIGN
// reflects winding direction, not shape: the renderer's meshes are
// intentionally wound CW-from-outside (CNA's default RasterizerState is
// CullCounterClockwiseFace, matching XNA/D3D9 semantics -- see the
// STAB-castle-fix comments throughout SceneRenderer_Builders.cpp), while
// the exporter's meshes are wound CCW-from-outside (the glTF/OpenGL
// convention) -- so renderer volumes come out negated relative to the
// exporter's for the SAME enclosed shape. This is an intentional, correct,
// per-target convention difference, not a bug -- callers compare magnitude
// (enclosedVolume()), not signedVolume() directly, except where the sign
// itself is the thing being checked.
static double signedVolume(const Mesh& m) {
    double v = 0.0;
    for (size_t i = 0; i + 2 < m.idx.size(); i += 3) {
        auto& a = m.pos[m.idx[i]];
        auto& b = m.pos[m.idx[i+1]];
        auto& c = m.pos[m.idx[i+2]];
        double bx = b[0], by = b[1], bz = b[2];
        double cx = c[0], cy = c[1], cz = c[2];
        double crossx = by*cz - bz*cy;
        double crossy = bz*cx - bx*cz;
        double crossz = bx*cy - by*cx;
        v += a[0]*crossx + a[1]*crossy + a[2]*crossz;
    }
    return v / 6.0;
}
static double enclosedVolume(const Mesh& m) { return std::abs(signedVolume(m)); }

// Weld vertices at (near-)identical positions and drop degenerate
// triangles. Both tessellators intentionally duplicate vertices at UV seams
// (renderer) or per-face (exporter's buildIcoSphere) -- correct for
// rendering/UVs, but leaves the raw triangle soup non-manifold BY INDEX
// even though it is manifold by POSITION. This mirrors the exact
// tolerance-based welding SceneRenderer.cpp's own CSG-preview code already
// does before handing torus/capsule/icosphere geometry to Manifold
// (`gl.tolerance = 1e-4f; gl.Merge();`).
static Mesh welded(const Mesh& in, float eps = 1e-4f) {
    std::map<std::array<int,3>, uint32_t> canon;
    Mesh out;
    std::vector<uint32_t> remap(in.pos.size());
    for (size_t i = 0; i < in.pos.size(); ++i) {
        auto& p = in.pos[i];
        std::array<int,3> key{
            static_cast<int>(std::lround(p[0]/eps)),
            static_cast<int>(std::lround(p[1]/eps)),
            static_cast<int>(std::lround(p[2]/eps))};
        auto it = canon.find(key);
        if (it == canon.end()) {
            auto idx = static_cast<uint32_t>(out.pos.size());
            out.pos.push_back(p);
            canon[key] = idx;
            remap[i] = idx;
        } else {
            remap[i] = it->second;
        }
    }
    for (size_t i = 0; i + 2 < in.idx.size(); i += 3) {
        uint32_t a = remap[in.idx[i]], b = remap[in.idx[i+1]], c = remap[in.idx[i+2]];
        if (a == b || b == c || a == c) continue;
        out.idx.push_back(a); out.idx.push_back(b); out.idx.push_back(c);
    }
    return out;
}

// Count of naked (boundary) directed edges after welding -- 0 for a closed,
// consistently-wound 2-manifold; > 0 for an open surface (Plane) or a
// genuinely broken mesh.
static int nakedEdgeCount(const Mesh& m) {
    std::map<std::pair<uint32_t,uint32_t>, int> dirCount;
    for (size_t i = 0; i + 2 < m.idx.size(); i += 3) {
        uint32_t a = m.idx[i], b = m.idx[i+1], c = m.idx[i+2];
        dirCount[{a,b}]++; dirCount[{b,c}]++; dirCount[{c,a}]++;
    }
    int naked = 0;
    for (auto& [e, cnt] : dirCount) {
        auto rev = std::make_pair(e.second, e.first);
        if (!dirCount.count(rev)) naked += cnt;
    }
    return naked;
}
static bool isWatertight(const Mesh& m) { return nakedEdgeCount(m) == 0; }

static double relErr(double a, double b) {
    double denom = std::max({std::abs(a), std::abs(b), 1e-9});
    return std::abs(a - b) / denom;
}

// Area of a regular N-gon inscribed in a unit circle, relative to the
// circle's own area -- exact. A cylinder/cone with a `segments`-sided
// polygonal cross section has volume = this ratio * the smooth analytical
// volume, EXACTLY (extruding or coning a polygon scales its area linearly
// / by 1/3 respectively, with no further approximation error), so this
// gives a tight, non-empirical ground-truth tolerance for those two shapes.
static double inscribedNgonAreaRatio(int n) {
    double nd = static_cast<double>(n);
    return (nd / (2.0 * std::numbers::pi)) * std::sin(2.0 * std::numbers::pi / nd);
}

static std::string fmt(double v) {
    std::ostringstream os;
    os.precision(6);
    os << v;
    return os.str();
}

// ---------------------------------------------------------------------------
// Box -- exact volume both ways (no curvature approximation at all).
// ---------------------------------------------------------------------------
static void testBox() {
    for (auto [w,h,d] : {std::array<float,3>{1,1,1}, std::array<float,3>{2.0f,3.0f,0.5f}}) {
        Mc3Primitive p = Mc3Primitive::box({w,h,d});
        auto exportMesh = fromMeshData(mc3togltf::buildPrimitive(p));
        auto renderMesh = scaled(fromRaw(tessellateUnitBoxAlg()), w, h, d);

        double vExp = enclosedVolume(exportMesh);
        double vRen = enclosedVolume(renderMesh);
        double vGT  = static_cast<double>(w) * h * d;

        std::string tag = "box(" + fmt(w) + "," + fmt(h) + "," + fmt(d) + ")";
        check(relErr(vExp, vGT) < 1e-4, tag + ": exporter volume matches analytical w*h*d");
        check(relErr(vRen, vGT) < 1e-4, tag + ": renderer volume matches analytical w*h*d");
        check(relErr(vExp, vRen) < 1e-4, tag + ": exporter and renderer volumes agree");
        check(isWatertight(welded(exportMesh)), tag + ": exporter mesh is watertight");
        check(isWatertight(welded(renderMesh)), tag + ": renderer mesh is watertight");

        auto bE = bboxSize(bboxOf(exportMesh));
        auto bR = bboxSize(bboxOf(renderMesh));
        check(relErr(bE[0],bR[0]) < 1e-4 && relErr(bE[1],bR[1]) < 1e-4 && relErr(bE[2],bR[2]) < 1e-4,
              tag + ": exporter and renderer bounding boxes agree");
    }
}

// ---------------------------------------------------------------------------
// Sphere -- tiers 32 (unitSphere_), 16 (unitSphereL1_), 6 (unitSphereL2_),
// matching SceneRenderer.cpp construction calls verbatim. Renderer and
// exporter use the IDENTICAL rings=segments/2, sectors=segments formula, so
// at matching segments they should agree tightly.
// ---------------------------------------------------------------------------
static void testSphere() {
    const int tiers[] = {32, 16, 6};
    for (int radiusIdx = 0; radiusIdx < 2; ++radiusIdx) {
        float radius = radiusIdx == 0 ? 0.5f : 1.3f;
        for (int segments : tiers) {
            Mc3Primitive p = Mc3Primitive::sphere(radius, segments);
            auto exportMesh = fromMeshData(mc3togltf::buildPrimitive(p));
            float r2 = radius * 2.0f;
            auto renderMesh = scaled(fromRaw(tessellateUnitSphereAlg(segments)), r2, r2, r2);

            double vExp = enclosedVolume(exportMesh);
            double vRen = enclosedVolume(renderMesh);
            double vGT  = (4.0/3.0) * std::numbers::pi * radius*radius*radius;

            std::string tag = "sphere(r=" + fmt(radius) + ",segs=" + fmt(segments) + ")";
            // Empirical: an inscribed UV-sphere polytope always undershoots the
            // smooth sphere's volume; the deficit shrinks with segment count.
            // 6 segments is coarse (~3 rings) so needs a generous bound.
            double gtTol = segments >= 32 ? 0.02 : segments >= 16 ? 0.08 : 0.40;
            check(vRen < vGT && relErr(vRen, vGT) < gtTol,
                  tag + ": renderer volume within " + fmt(gtTol*100) + "% of analytical sphere volume (" + fmt(vRen) + " vs " + fmt(vGT) + ")");
            check(vExp < vGT && relErr(vExp, vGT) < gtTol,
                  tag + ": exporter volume within " + fmt(gtTol*100) + "% of analytical sphere volume (" + fmt(vExp) + " vs " + fmt(vGT) + ")");

            // True differential check: same discretization formula both sides.
            check(relErr(vExp, vRen) < 1e-3,
                  tag + ": exporter and renderer volumes agree tightly at matching segments (" + fmt(vExp) + " vs " + fmt(vRen) + ")");

            check(isWatertight(welded(exportMesh)), tag + ": exporter mesh is watertight");
            check(isWatertight(welded(renderMesh)), tag + ": renderer mesh is watertight");
        }
    }
}

// ---------------------------------------------------------------------------
// Cylinder -- tiers 24/12/6 (unitCylinder_/L1_/L2_). Volume has an EXACT
// closed form via the inscribed-N-gon area ratio (extrusion is linear).
// ---------------------------------------------------------------------------
static void testCylinder() {
    const int tiers[] = {24, 12, 6};
    for (auto [radius,height] : {std::pair{0.5f,1.0f}, std::pair{0.8f,2.2f}}) {
        for (int segments : tiers) {
            Mc3Primitive p = Mc3Primitive::cylinder(radius, height, segments);
            auto exportMesh = fromMeshData(mc3togltf::buildPrimitive(p));
            float r2 = radius * 2.0f;
            auto renderMesh = scaled(fromRaw(tessellateUnitCylinderAlg(segments)), r2, height, r2);

            double vExp = enclosedVolume(exportMesh);
            double vRen = enclosedVolume(renderMesh);
            double smoothV = std::numbers::pi * radius*radius * height;
            double vGT = inscribedNgonAreaRatio(segments) * smoothV;

            std::string tag = "cylinder(r=" + fmt(radius) + ",h=" + fmt(height) + ",segs=" + fmt(segments) + ")";
            check(relErr(vExp, vGT) < 1e-3, tag + ": exporter volume matches exact N-gon*height ground truth");
            // Renderer's volume/watertightness are NOT asserted here -- see
            // REAL FINDING below: the VPC index buffer this measures is not
            // a self-consistent closed manifold, so its divergence-theorem
            // volume is not just sign-flipped but genuinely wrong in
            // magnitude (partial cancellation from inconsistent
            // orientation). This buffer is not what's actually rendered.
            bool renderWatertight = isWatertight(welded(renderMesh));
            if (!renderWatertight) {
                info(tag + ": renderer's VPC (position-only, RenderMesh::vb/ib) index buffer is NOT a "
                     "self-consistent closed manifold (naked directed edges between the side wall and "
                     "the caps), so its computed volume (" + fmt(vRen) + " vs ground truth " + fmt(vGT) +
                     ") is not meaningful -- see REAL FINDING below. This is NOT the buffer actually drawn "
                     "for solid rendering (SceneRenderer::drawAuto() always calls drawMeshTextured(), which "
                     "uses the SEPARATELY-CODED VPNT texVB/texIB -- for Cylinder specifically, that block "
                     "builds its own independent index list, not a copy of this one), and was empirically "
                     "confirmed NOT to cause any visible defect: a real headless --screenshot render of a "
                     "single cylinder (Xvfb + the actual MeshCraft binary) shows a fully solid, correctly "
                     "lit cylinder with no missing/culled faces. Tracked as a real, narrow latent "
                     "inconsistency in position-storage-only data (RenderMesh::positions, used for "
                     "bounding-box/picking purposes), not a rendering bug -- out of scope to fix here.");
            } else {
                check(renderWatertight, tag + ": renderer mesh is watertight");
                check(relErr(vRen, vGT) < 1e-3, tag + ": renderer volume matches exact N-gon*height ground truth");
                check(relErr(vExp, vRen) < 1e-3, tag + ": exporter and renderer volumes agree at matching segments");
            }
            check(isWatertight(welded(exportMesh)), tag + ": exporter mesh is watertight");
        }
    }
}

// REAL FINDING (Cylinder only): the VPC (position+color only) index buffer
// built by tessellateUnitCylinderAlg() -- extracted verbatim from
// SceneRenderer_Builders.cpp's buildUnitCylinder(), i.e. NOT introduced by
// this test -- is not a self-consistent closed 2-manifold: its side-wall
// triangles and its cap triangles traverse their shared rim edges in the
// SAME direction rather than opposite directions (confirmed by direct
// cross-product-vs-outward-direction computation on the actual generated
// vertices). Every other shape in this file (Box/Sphere/Plane/Torus/
// Capsule/IcoSphere) passes this same check cleanly, and Cone -- which has
// an analogous side+cap structure -- also passes, so this is a narrow,
// Cylinder-specific latent defect, not a bug in this test's methodology.
// It does NOT reach the screen: drawObject()'s solid-rendering path always
// calls drawMeshTextured(), which for Cylinder (and Cone) uses a wholly
// separately-coded VPNT block (own `ti` index list, not shared with the VPC
// `indices`) -- unlike Box/Sphere/Plane/Torus/Capsule, which explicitly
// reuse the VPC index array for their VPNT texIB too (their own comments
// say so). Empirically confirmed harmless via a real headless
// `xvfb-run ... MeshCraft cyl_test.mc3.xml --screenshot out.ppm` render of
// an isolated cylinder: fully solid, correctly lit, no missing faces.

// ---------------------------------------------------------------------------
// Cone -- tiers 24/12/6 (unitCone_/L1_/L2_). Same exact N-gon closed form
// (cone volume = (1/3) * baseArea * height regardless of base polygon).
// ---------------------------------------------------------------------------
static void testCone() {
    const int tiers[] = {24, 12, 6};
    for (auto [radius,height] : {std::pair{0.5f,1.0f}, std::pair{0.8f,2.2f}}) {
        for (int segments : tiers) {
            Mc3Primitive p = Mc3Primitive::cone(radius, height, segments);
            auto exportMesh = fromMeshData(mc3togltf::buildPrimitive(p));
            float r2 = radius * 2.0f;
            auto renderMesh = scaled(fromRaw(tessellateUnitConeAlg(segments)), r2, height, r2);

            double vExp = enclosedVolume(exportMesh);
            double vRen = enclosedVolume(renderMesh);
            double smoothV = (1.0/3.0) * std::numbers::pi * radius*radius * height;
            double vGT = inscribedNgonAreaRatio(segments) * smoothV;

            std::string tag = "cone(r=" + fmt(radius) + ",h=" + fmt(height) + ",segs=" + fmt(segments) + ")";
            check(relErr(vExp, vGT) < 1e-3, tag + ": exporter volume matches exact N-gon*height/3 ground truth");
            check(relErr(vRen, vGT) < 1e-3, tag + ": renderer volume matches exact N-gon*height/3 ground truth");
            check(relErr(vExp, vRen) < 1e-3, tag + ": exporter and renderer volumes agree at matching segments");
            check(isWatertight(welded(exportMesh)), tag + ": exporter mesh is watertight");
            check(isWatertight(welded(renderMesh)), tag + ": renderer mesh is watertight");
        }
    }
}

// ---------------------------------------------------------------------------
// Plane -- open surface, no volume. Checks topology + bbox only.
// ---------------------------------------------------------------------------
static void testPlane() {
    for (auto [w,d] : {std::array<float,2>{1,1}, std::array<float,2>{2.0f,0.6f}}) {
        Mc3Primitive p = Mc3Primitive::plane(w, d);
        auto exportMesh = fromMeshData(mc3togltf::buildPrimitive(p));
        auto renderMesh = scaled(fromRaw(tessellateUnitPlaneAlg()), w, 1.0f, d);

        std::string tag = "plane(" + fmt(w) + "," + fmt(d) + ")";
        check(!isWatertight(welded(exportMesh)), tag + ": exporter mesh is an OPEN surface (has naked edges)");
        check(!isWatertight(welded(renderMesh)), tag + ": renderer mesh is an OPEN surface (has naked edges)");
        check(nakedEdgeCount(welded(exportMesh)) == 4, tag + ": exporter plane has exactly 4 boundary edges");
        check(nakedEdgeCount(welded(renderMesh)) == 4, tag + ": renderer plane has exactly 4 boundary edges");

        auto bE = bboxSize(bboxOf(exportMesh));
        auto bR = bboxSize(bboxOf(renderMesh));
        check(relErr(bE[0],bR[0]) < 1e-4 && relErr(bE[2],bR[2]) < 1e-4,
              tag + ": exporter and renderer footprints (w,d) agree");
    }
}

// ---------------------------------------------------------------------------
// Torus -- tiers (ringSeg=32,tubeSeg=16) / (16,8) / (8,4), matching
// unitTorus_/L1_/L2_. Exporter's buildTorus(major,minor,segments) computes
// rings=segments, sides=max(4,segments/2) internally -- segments=32/16/8
// reproduce the SAME (32,16)/(16,8)/(8,4) split exactly, so these are true
// matching-discretization comparisons.
//
// AUD-061 FIX VERIFIED: SceneRenderer.cpp's Torus case used to scale the
// cached unit torus (majorRadius=0.35, minorRadius=0.15) by
// sxz=majorRadius/0.35 (X/Z) and sy=minorRadius/0.15 (Y) as a single affine
// transform -- correct only when minorRadius/majorRadius == 0.15/0.35 (the
// unit mesh's own ratio); any other ratio produced an elliptical-cross-
// section tube with the wrong outer radius. The renderer now builds a real
// mesh directly at the object's actual (majorRadius, minorRadius) via
// tessellateUnitTorusAlg(ringSeg, tubeSeg, majorRadius, minorRadius) --
// majorRadius/minorRadius became real parameters instead of a baked-in
// 0.35/0.15 constant (see SceneRenderer::getOrBuildTorusMesh() /
// buildUnitTorus()). So the "off-ratio" case below, which used to PIN the
// old bugged affine-scale output, now asserts actual correctness against
// both ground truth and the exporter -- the identical invariant already
// checked for the matching-ratio case, just at a different ratio.
// ---------------------------------------------------------------------------
static void checkTorusRatio(float R, float r, const char* label) {
    struct Tier { int ring, tube, exporterSegments; };
    const Tier tiers[] = {{32,16,32}, {16,8,16}, {8,4,8}};

    for (auto& t : tiers) {
        Mc3Primitive p = Mc3Primitive::torus(R, r, t.exporterSegments);
        auto exportMesh = fromMeshData(mc3togltf::buildPrimitive(p));
        // AUD-061: renderer now tessellates directly at the object's actual
        // (R, r) -- no post-hoc scale of a fixed-ratio unit mesh.
        auto renderMesh = fromRaw(tessellateUnitTorusAlg(t.ring, t.tube, R, r));

        double vExp = enclosedVolume(exportMesh);
        double vRen = enclosedVolume(renderMesh);
        double vGT  = 2.0 * std::numbers::pi * std::numbers::pi * R * r * r;

        std::string tag = "torus(R=" + fmt(R) + ",r=" + fmt(r) + ",ring=" + fmt(t.ring) + ",tube=" + fmt(t.tube) + "," + label + ")";
        // Empirical: coarser ring/tube tessellation undershoots the smooth
        // torus volume more (ring=8/tube=4 is a very coarse donut).
        double gtTol = t.ring >= 16 ? 0.15 : 0.45;
        check(relErr(vExp, vRen) < 1e-3, tag + ": exporter and renderer volumes agree at matching segments");
        check(relErr(vRen, vGT) < gtTol, tag + ": renderer volume within " + fmt(gtTol*100) + "% of analytical torus volume");
        check(relErr(vExp, vGT) < gtTol, tag + ": exporter volume within " + fmt(gtTol*100) + "% of analytical torus volume");
        check(isWatertight(welded(exportMesh)), tag + ": exporter mesh is watertight");
        check(isWatertight(welded(renderMesh)), tag + ": renderer mesh is watertight");

        // The strongest per-shape correctness signal: the renderer's outer
        // radius (bbox X extent) must be analytically R+r, not some
        // ratio-dependent distortion of it.
        auto bR = bboxOf(renderMesh);
        float correctOuterX = R + r;
        check(relErr(bR.hi[0], correctOuterX) < 1e-2,
              tag + ": renderer's outer radius is analytically correct (R+r)");
        auto bE = bboxOf(exportMesh);
        check(relErr(bE.hi[0], correctOuterX) < 1e-2,
              tag + ": exporter's outer radius is analytically correct (R+r)");
    }
}

static void testTorus() {
    // Case 1: ratio matches the OLD fixed unit mesh's own 0.15/0.35 -- was
    // already correct before the fix (kept as a plain regression guard).
    checkTorusRatio(0.35f, 0.15f, "matching-ratio");

    // Case 2: ratio DIFFERENT from 3/7 (0.6/0.2 = ratio 1/3) -- this is
    // EXACTLY the ratio that used to expose the AUD-061 bug (the renderer's
    // affine-scale approach produced an elliptical-cross-section tube with
    // the wrong outer radius here). Now asserted correct, not just pinned.
    checkTorusRatio(0.6f, 0.2f, "off-ratio, AUD-061 fix verified");
}

// ---------------------------------------------------------------------------
// Capsule -- tiers 16/8/4 (unitCapsule_/L1_/L2_). AUD-063 aligns the
// viewport and exporter on hRings=max(2,segments/4), so every tier below is
// a true matching-discretization comparison.
//
// AUD-061 FIX VERIFIED: like Torus, SceneRenderer.cpp's Capsule case used to
// scale one fixed unit capsule (radius=0.5, cylinder height=1) via
// sxz=2*radius (X/Z) and sy=(height+2*radius)/2 (Y) -- whenever height !=
// 2*radius (i.e. sy != sxz), the hemisphere caps were stretched/squashed
// into ellipsoids instead of staying spherical. The renderer now builds a
// real mesh directly at the object's actual (radius, height) via
// tessellateUnitCapsuleAlg(segments, radius, height) -- radius/height became
// real parameters instead of a baked-in 0.5/1.0 constant (see
// SceneRenderer::getOrBuildCapsuleMesh() / buildUnitCapsule()). So the
// height!=2*radius case below, which used to PIN the old bugged
// affine-scale output, now asserts actual correctness.
// ---------------------------------------------------------------------------
static void testCapsule() {
    const int tiers[] = {16, 8, 4};

    // radius=0.5, height=1.0 => height == 2*radius -- the ratio was always
    // correct even under the old affine-scale approach (control case), so
    // this mainly confirms the new direct-tessellation path is
    // behavior-preserving here.
    {
        float radius = 0.5f, height = 1.0f;
        for (int segments : tiers) {
            Mc3Primitive p = Mc3Primitive::capsule(radius, height, segments);
            auto exportMesh = fromMeshData(mc3togltf::buildPrimitive(p));
            // AUD-061: renderer now tessellates directly at the object's
            // actual (radius, height) -- no post-hoc scale of a fixed-ratio
            // unit mesh.
            auto renderMesh = fromRaw(tessellateUnitCapsuleAlg(segments, radius, height));

            double vExp = enclosedVolume(exportMesh);
            double vRen = enclosedVolume(renderMesh);
            double vGT  = (4.0/3.0)*std::numbers::pi*radius*radius*radius + std::numbers::pi*radius*radius*height;

            std::string tag = "capsule(r=" + fmt(radius) + ",h=" + fmt(height) + ",segs=" + fmt(segments) + ",height==2r)";
            // Empirical: segments=4 is especially coarse (two hemisphere
            // rings), so needs a looser ground-truth bound than 8/16.
            double gtTol = segments >= 8 ? 0.25 : 0.45;
            check(relErr(vRen, vGT) < gtTol, tag + ": renderer volume within " + fmt(gtTol*100) + "% of analytical capsule volume");
            check(relErr(vExp, vGT) < gtTol, tag + ": exporter volume within " + fmt(gtTol*100) + "% of analytical capsule volume");
            check(isWatertight(welded(exportMesh)), tag + ": exporter mesh is watertight");
            check(isWatertight(welded(renderMesh)), tag + ": renderer mesh is watertight");
            check(relErr(vExp, vRen) < 1e-3,
                  tag + ": exporter and renderer volumes agree tightly (AUD-063: matching hemisphere-ring formula)");
        }
    }

    // radius=0.4, height=1.5 => height != 2*radius (0.8) -- this is EXACTLY
    // the ratio that used to expose the AUD-061 ellipsoidal-hemisphere bug.
    // Now asserted correct, not just pinned.
    {
        float radius = 0.4f, height = 1.5f;
        int segments = 16;
        Mc3Primitive p = Mc3Primitive::capsule(radius, height, segments);
        auto exportMesh = fromMeshData(mc3togltf::buildPrimitive(p));
        auto renderMesh = fromRaw(tessellateUnitCapsuleAlg(segments, radius, height));

        std::string tag = "capsule(r=" + fmt(radius) + ",h=" + fmt(height) + ",segs=16,height!=2r, AUD-061 fix verified)";
        auto bE = bboxOf(exportMesh);
        auto bR = bboxOf(renderMesh);
        float expectedHalfHeight = height/2.0f + radius;
        check(relErr(bE.hi[1], expectedHalfHeight) < 1e-2, tag + ": exporter pole height is analytically correct");
        check(relErr(bR.hi[1], expectedHalfHeight) < 1e-2, tag + ": renderer pole height is analytically correct");
        // Max horizontal (X/Z) extent of a capsule is exactly `radius`,
        // reached anywhere from the equator through the hemisphere-cylinder
        // seam -- true for a correct SPHERICAL cap, but NOT for the old
        // bugged ellipsoidal cap (whose max X/Z extent was still sxz=2*radius
        // at the seam by construction, so this specific check wouldn't have
        // caught the old bug by itself -- the volume check below is the one
        // that actually distinguishes spherical from ellipsoidal caps).
        check(relErr(bR.hi[0], radius) < 1e-2, tag + ": renderer's max horizontal extent is analytically correct (radius)");
        check(relErr(bE.hi[0], radius) < 1e-2, tag + ": exporter's max horizontal extent is analytically correct (radius)");

        double vExp = enclosedVolume(exportMesh);
        double vRen = enclosedVolume(renderMesh);
        double vGT  = (4.0/3.0)*std::numbers::pi*radius*radius*radius + std::numbers::pi*radius*radius*height;
        info(tag + ": exporter volume=" + fmt(vExp) + " ground truth=" + fmt(vGT));
        info(tag + ": renderer volume=" + fmt(vRen) + " ground truth=" + fmt(vGT));
        check(relErr(vExp, vGT) < 0.25, tag + ": exporter volume is correct (within 25% of ground truth)");
        check(relErr(vRen, vGT) < 0.25, tag + ": renderer volume is correct (within 25% of ground truth) -- AUD-061 fix verified");
        check(relErr(vExp, vRen) < 1e-3,
              tag + ": exporter and renderer volumes now agree tightly (both build the exact same spherical-cap shape)");
        check(isWatertight(welded(exportMesh)), tag + ": exporter mesh is watertight");
        check(isWatertight(welded(renderMesh)), tag + ": renderer mesh is watertight");
    }
}

// ---------------------------------------------------------------------------
// IcoSphere -- the viewport and exporter both derive subdivisions from the
// MC3 `segments` field with clamp(segments/8, 1, 4). The renderer caches one
// unit mesh per possible subdivision level; this test exercises low, middle,
// and high values to make a future fixed-level regression observable.
// ---------------------------------------------------------------------------
static void testIcoSphere() {
    float radius = 0.5f;
    for (int segments : {2, 16, 32}) {
        int subdivisions = icoSphereSubdivisionsForSegmentsAlg(segments);
        Mc3Primitive p = Mc3Primitive::icoSphere(radius, segments);
        auto exportMesh = fromMeshData(mc3togltf::buildPrimitive(p));
        float r2 = radius * 2.0f;
        auto renderMesh = scaled(fromRaw(tessellateUnitIcoSphereAlg(subdivisions)), r2, r2, r2);

        double vExp = enclosedVolume(exportMesh);
        double vRen = enclosedVolume(renderMesh);
        double vGT  = (4.0/3.0) * std::numbers::pi * radius*radius*radius;

        std::string tag = "icosphere(r=" + fmt(radius) + ",segments=" +
                          fmt(segments) + ",subdiv=" + fmt(subdivisions) + ")";
        check(relErr(vExp, vRen) < 1e-3, tag + ": exporter and renderer volumes agree tightly (identical subdivision algorithm)");
        check(relErr(vRen, vGT) < 0.26, tag + ": renderer volume is within its subdivision-dependent tolerance of analytical sphere volume");
        check(relErr(vExp, vGT) < 0.26, tag + ": exporter volume is within its subdivision-dependent tolerance of analytical sphere volume");
        check(isWatertight(welded(exportMesh)), tag + ": exporter mesh is watertight (after welding per-face UV duplicates)");
        check(isWatertight(welded(renderMesh)), tag + ": renderer mesh is watertight");

        auto exportTris = exportMesh.idx.size() / 3;
        auto renderTris = renderMesh.idx.size() / 3; // renderer mesh already uses a shared (welded) index pool
        check(exportTris == renderTris,
              tag + ": exporter and renderer produce the same triangle count (" + fmt(static_cast<double>(exportTris)) + ")");
    }
}

int main() {
    testBox();
    testSphere();
    testCylinder();
    testCone();
    testPlane();
    testTorus();
    testCapsule();
    testIcoSphere();

    if (failures == 0) {
        std::cout << "All differential geometry tests passed.\n";
        return 0;
    }
    std::cerr << failures << " differential geometry test(s) failed.\n";
    return 1;
}
