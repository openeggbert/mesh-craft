// AUD-073: SceneRenderer_Extrude.cpp's drawExtrudeDynamic() and
// drawObjectEdges() each independently computed
//   bool hollow = (cs.innerRadius > 0.0f) && (type == Circle || Polygon);
//   float innerScale = hollow ? (cs.innerRadius / cs.radius) : 0.0f;
// with no check that cs.radius is actually nonzero. radius==0 is a legal
// document value (the XML/JSON parsers only reject negatives), and a
// hollow cross-section with radius=0 + innerRadius>0 (already a
// nonsensical shape -- the "inner" radius would be larger than the
// "outer" one) made innerScale evaluate to +inf, propagating NaN vertex
// positions into the live viewport (and the edge-overlay wireframe).
//
// SceneRenderer's own methods are CNA-coupled (need a live GL device) and
// can't be called from a CNA-free unit test, so -- matching this
// codebase's own established "Alg mirror" precedent for exactly this
// situation (see differential_geometry_test.cpp, which compares mc3togltf
// against a formula mirror of SceneRenderer rather than calling
// SceneRenderer directly) -- this test mirrors the EXACT two-line formula
// verbatim (both occurrences in SceneRenderer_Extrude.cpp are byte-for-
// byte identical) rather than the whole rendering function. Low
// drift risk: it's two lines, not a multi-hundred-line algorithm.
//
// A live-render smoke check was also tried and deliberately NOT kept as a
// committed test: a radius=0 cross-section produces byte-identical
// screenshots whether or not this fix is applied, because
// makeProfile()'s Circle case scales a unit circle by radius, so the
// OUTER ring (unaffected by this bug) is ALSO already fully degenerate
// (every profile point collapses to (0,0)) when radius==0 -- the whole
// extruded tube is invisible either way, for a reason unrelated to this
// fix. This formula-level test is what can actually discriminate the bug.

#include <MeshCraft/Mc3/Mc3Extrude.hpp>

#include <cmath>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

// Mirrors the FIXED formula (SceneRenderer_Extrude.cpp, both
// drawExtrudeDynamic() and drawObjectEdges()).
static void fixedHollow(const Mc3CrossSection& cs, bool& hollow, float& innerScale) {
    hollow = (cs.innerRadius > 0.0f) && (cs.radius > 1e-6f) &&
             (cs.type == CrossSectionType::Circle || cs.type == CrossSectionType::Polygon);
    innerScale = hollow ? (cs.innerRadius / cs.radius) : 0.0f;
}

// Mirrors the PRE-FIX formula, to concretely demonstrate the bug this fix
// closes (not just assert the fix's own behavior in isolation).
static void preFixHollow(const Mc3CrossSection& cs, bool& hollow, float& innerScale) {
    hollow = (cs.innerRadius > 0.0f) &&
             (cs.type == CrossSectionType::Circle || cs.type == CrossSectionType::Polygon);
    innerScale = hollow ? (cs.innerRadius / cs.radius) : 0.0f;
}

int main() {
    // The exact triggering case: radius=0, innerRadius>0, Circle.
    {
        Mc3CrossSection cs;
        cs.type = CrossSectionType::Circle;
        cs.radius = 0.0f;
        cs.innerRadius = 0.3f;

        bool preHollow; float preScale;
        preFixHollow(cs, preHollow, preScale);
        check(preHollow, "sanity: the pre-fix formula treats radius=0 as hollow");
        check(!std::isfinite(preScale),
              "sanity: the pre-fix formula's innerScale is +inf for radius=0 (0.3f/0.0f) -- "
              "concretely reproduces the bug this fix closes, not just a hypothetical");

        bool hollow; float innerScale;
        fixedHollow(cs, hollow, innerScale);
        check(!hollow, "fixed: radius=0 is no longer treated as hollow, even with innerRadius>0");
        check(std::isfinite(innerScale) && innerScale == 0.0f,
              "fixed: innerScale stays a finite 0.0f for radius=0, not +inf/NaN");
    }

    // Same trigger, Polygon cross-section (the fix's other affected type).
    {
        Mc3CrossSection cs;
        cs.type = CrossSectionType::Polygon;
        cs.radius = 0.0f;
        cs.innerRadius = 0.3f;
        cs.sides = 6;

        bool hollow; float innerScale;
        fixedHollow(cs, hollow, innerScale);
        check(!hollow, "fixed: radius=0 Polygon cross-section is also not treated as hollow");
        check(std::isfinite(innerScale) && innerScale == 0.0f,
              "fixed: innerScale stays finite for a radius=0 Polygon cross-section");
    }

    // Near-zero (not exactly 0) radius must also be treated as solid --
    // the guard uses the same 1e-6f epsilon convention as norm3()/AUD-067
    // elsewhere in this file, not an exact-zero check.
    {
        Mc3CrossSection cs;
        cs.type = CrossSectionType::Circle;
        cs.radius = 1e-9f;
        cs.innerRadius = 0.3f;

        bool hollow; float innerScale;
        fixedHollow(cs, hollow, innerScale);
        check(!hollow, "fixed: a near-zero (1e-9) radius is also treated as solid, not just exact 0");
        check(std::isfinite(innerScale), "fixed: innerScale stays finite for a near-zero radius");
    }

    // A legitimate hollow cross-section (radius comfortably > 0,
    // innerRadius < radius) must be entirely unaffected by this fix.
    {
        Mc3CrossSection cs;
        cs.type = CrossSectionType::Circle;
        cs.radius = 1.0f;
        cs.innerRadius = 0.3f;

        bool hollow; float innerScale;
        fixedHollow(cs, hollow, innerScale);
        check(hollow, "fixed: a legitimate hollow cross-section (radius=1, innerRadius=0.3) "
              "still reports hollow=true");
        check(std::isfinite(innerScale) && std::abs(innerScale - 0.3f) < 1e-6f,
              "fixed: a legitimate hollow cross-section's innerScale is still computed correctly (0.3)");
    }

    // A solid (non-hollow, innerRadius==0) cross-section with radius=0 is
    // unaffected either way -- this fix only changes behavior for the
    // hollow+radius=0 combination, not solid degenerate cross-sections
    // (already a separate, pre-existing degenerate-geometry case, not part
    // of this fix's scope).
    {
        Mc3CrossSection cs;
        cs.type = CrossSectionType::Circle;
        cs.radius = 0.0f;
        cs.innerRadius = 0.0f;

        bool hollow; float innerScale;
        fixedHollow(cs, hollow, innerScale);
        check(!hollow, "solid (innerRadius=0) radius=0 cross-section reports hollow=false, as before");
        check(innerScale == 0.0f, "solid radius=0 cross-section's innerScale is 0.0f, as before");
    }

    if (failures == 0) { std::cout << "All extrude-hollow-zero-radius tests passed.\n"; return 0; }
    std::cerr << failures << " extrude-hollow-zero-radius test(s) FAILED.\n";
    return 1;
}
