// CsgEvaluator.cpp's NESTED Intersection case (NEXT.md task, 2026-07-19):
// every other nested-CSG case (Union/Difference/Group/Area) in
// buildManifoldNode()'s switch guards EVERY child with `if (child)` before
// dereferencing it, but Intersection used to seed its result from
// `*obj.children[0]` unconditionally -- a null first child would be
// dereferenced with no guard at all.
//
// Important: evaluateCsgNode() (the public entry point) has its OWN
// separate, already-correctly-guarded Union/Difference/Intersection logic
// for the CSG ROOT itself (every child checked with `if (child)`/
// `if (!child) continue;`) -- the bug is unreachable from a CSG root's
// direct children. It only lives in buildManifoldNode()'s switch, reached
// when an Intersection node is NESTED inside another CSG node's children
// (e.g. a Union containing an Intersection child). Every scenario below
// therefore wraps the Intersection under test in a parent Union, so it's
// actually routed through the buggy code path, not evaluateCsgNode()'s
// separate root-level handling.
//
// No current parser path produces a null child (Mc3XmlParser/Mc3JsonParser
// always construct a real object or omit the entry entirely), so this test
// builds the Mc3Object tree programmatically instead, the same way
// pre_export_validation_test.cpp already does for its own
// parser-unreachable scenario.

#include "CsgEvaluator.hpp"

#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <string>

using namespace MeshCraft::Mc3;
using namespace mc3togltf;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

// Wraps `intersection` (an Intersection-type node) in a single-child Union,
// so evaluateCsgNode()'s Union handling recurses into buildManifoldNode(),
// which then dispatches to the switch's Intersection case under test --
// NOT evaluateCsgNode()'s own already-guarded root-level Intersection path.
static std::shared_ptr<Mc3Object> wrapInUnion(std::shared_ptr<Mc3Object> intersection) {
    return Mc3Object::makeUnion("wrapper", {std::move(intersection)});
}

int main() {
    std::map<std::string, std::shared_ptr<Mc3Object>> noDefinitions;

    // The bug: children[0] is null, children[1] is the real box. Pre-fix,
    // buildManifoldNode()'s Intersection case dereferences `*obj.children[0]`
    // unconditionally to seed `result` -- a null-pointer dereference before
    // ever reaching the `if (child)` guard later in the same loop.
    auto boxA = Mc3Object::makeBox("boxA");
    auto nestedNullFirst = Mc3Object::makeIntersection("nested", {nullptr, boxA});
    auto rootNullFirst = wrapInUnion(nestedNullFirst);

    MeshData resultNullFirst = evaluateCsgNode(*rootNullFirst, noDefinitions);
    check(!resultNullFirst.empty(),
          "nested Intersection with a null first child does not crash and produces geometry");

    // Baseline: the same nested intersection WITHOUT the null entry at all.
    // The fix finds the first non-null child to seed from (boxB), so a null
    // leading entry must be geometrically inert -- identical vertex/index
    // counts to the same intersection without it.
    auto boxB = Mc3Object::makeBox("boxB");
    auto nestedNoNull = Mc3Object::makeIntersection("nested", {boxB});
    auto rootNoNull = wrapInUnion(nestedNoNull);
    MeshData resultNoNull = evaluateCsgNode(*rootNoNull, noDefinitions);

    check(resultNullFirst.vertexCount() == resultNoNull.vertexCount(),
          "a null leading child is geometrically inert (same vertex count as without it)");
    check(resultNullFirst.indices.size() == resultNoNull.indices.size(),
          "a null leading child is geometrically inert (same index count as without it)");

    // A null child in a LATER position (already guarded pre-fix, both here
    // and at the root level) must keep working the same way, confirming the
    // fix didn't change that already-correct path.
    auto boxC = Mc3Object::makeBox("boxC");
    auto nestedNullLater = Mc3Object::makeIntersection("nested", {boxC, nullptr});
    auto rootNullLater = wrapInUnion(nestedNullLater);
    MeshData resultNullLater = evaluateCsgNode(*rootNullLater, noDefinitions);
    check(!resultNullLater.empty(),
          "nested Intersection with a null NON-first child still works (already-guarded path)");
    check(resultNullLater.vertexCount() == resultNoNull.vertexCount(),
          "a null trailing child is geometrically inert, same as a null leading child");

    // All-null children: no real geometry anywhere in the nested node.
    auto nestedAllNull = Mc3Object::makeIntersection("nested", {nullptr, nullptr});
    auto rootAllNull = wrapInUnion(nestedAllNull);
    MeshData resultAllNull = evaluateCsgNode(*rootAllNull, noDefinitions);
    check(resultAllNull.empty(),
          "nested Intersection with only null children produces empty geometry, not a crash");

    if (failures == 0) {
        std::cout << "\nAll csg_null_child checks passed.\n";
        return 0;
    }
    std::cerr << "\n" << failures << " csg_null_child check(s) FAILED.\n";
    return 1;
}
