// Editor::ObjectIndex test (SYS-W5-04).
//
// No test existed for this subsystem before its extraction (new this
// session). Covers: id/name lookup over a nested tree, "first match in
// document order" duplicate-id/name semantics matching
// MeshCraftApplication::flatFindById()/flatFindByName()'s pre-existing
// linear-walk behavior, not-found cases, invalidate()-then-rebuild picking
// up a mutation, and the cyclic-children guard.

#include "MeshCraft/Editor/ObjectIndex.hpp"

#include <cstdio>

using namespace MeshCraft::Editor;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const char* msg) {
    if (cond) std::printf("PASS: %s\n", msg);
    else      { std::printf("FAIL: %s\n", msg); ++failures; }
}

static std::shared_ptr<Mc3Object> makeObj(std::string id, std::string name) {
    auto o = std::make_shared<Mc3Object>();
    o->id = std::move(id);
    o->name = std::move(name);
    return o;
}

int main() {
    ObjectIndex idx;

    // Empty document
    {
        Mc3Document doc;
        check(idx.findById(doc, "a") == nullptr, "empty document: findById returns nullptr");
        check(idx.findByName(doc, "a") == nullptr, "empty document: findByName returns nullptr");
        check(idx.findSharedById(doc, "a") == nullptr, "empty document: findSharedById returns nullptr");
    }

    // Nested tree
    Mc3Document doc;
    auto root1 = makeObj("r1", "Root1");
    auto child1 = makeObj("c1", "Child1");
    auto grand1 = makeObj("g1", "Grand1");
    child1->children.push_back(grand1);
    root1->children.push_back(child1);
    auto root2 = makeObj("r2", "Root2");
    doc.objects.push_back(root1);
    doc.objects.push_back(root2);
    idx.invalidate();

    check(idx.findById(doc, "r1") == root1.get(), "finds a root object by id");
    check(idx.findById(doc, "c1") == child1.get(), "finds a nested child by id");
    check(idx.findById(doc, "g1") == grand1.get(), "finds a deeply nested grandchild by id");
    check(idx.findById(doc, "nope") == nullptr, "unknown id returns nullptr");
    check(idx.findByName(doc, "Grand1") == grand1.get(), "finds a nested object by name");
    check(idx.findByName(doc, "nope") == nullptr, "unknown name returns nullptr");
    check(idx.findSharedById(doc, "c1") == child1, "findSharedById returns the same shared_ptr instance");

    // Repeated lookups against the same (unmodified) document must be stable
    // -- exercises the "already built, serve from cache" path, not just the
    // first rebuild.
    check(idx.findById(doc, "r2") == root2.get(), "repeated lookup after cache is built still finds root2");

    // Duplicate ids: SYS-W1-04 confirmed parsing stays permissive on these,
    // so the cache must match flatFindById's own "first match in document
    // order" (pre-order depth-first) semantics, not silently pick some
    // other one.
    {
        Mc3Document dupDoc;
        auto first = makeObj("dup", "First");
        auto second = makeObj("dup", "Second");
        dupDoc.objects.push_back(first);
        dupDoc.objects.push_back(second);
        ObjectIndex dupIdx;
        check(dupIdx.findById(dupDoc, "dup") == first.get(),
              "duplicate id resolves to the first occurrence in document order");
    }
    {
        Mc3Document dupDoc;
        auto outer = makeObj("o1", "dup");
        auto inner = makeObj("i1", "dup");
        outer->children.push_back(inner);
        dupDoc.objects.push_back(outer);
        ObjectIndex dupIdx;
        check(dupIdx.findByName(dupDoc, "dup") == outer.get(),
              "duplicate name resolves to the pre-order-first occurrence (parent before child)");
    }

    // Invalidate + rebuild: a mutation made after the cache was already
    // built must not be visible until invalidate() is called, and must be
    // visible immediately after.
    {
        Mc3Document mutDoc;
        auto a = makeObj("a", "A");
        mutDoc.objects.push_back(a);
        ObjectIndex mutIdx;
        check(mutIdx.findById(mutDoc, "a") != nullptr, "invalidate/rebuild: initial lookup finds 'a'");
        check(mutIdx.findById(mutDoc, "b") == nullptr, "invalidate/rebuild: 'b' not present yet");
        auto b = makeObj("b", "B");
        mutDoc.objects.push_back(b);
        mutIdx.invalidate();
        check(mutIdx.findById(mutDoc, "b") == b.get(),
              "invalidate/rebuild: 'b' found after invalidate() following the mutation");
    }

    // Cyclic children graph must throw instead of stack-overflow-crashing,
    // matching this codebase's established SYS-W1-05/06/07 convention for
    // new recursive Mc3Object::children walks.
    {
        Mc3Document cyclicDoc;
        auto x = makeObj("x", "X");
        auto y = makeObj("y", "Y");
        x->children.push_back(y);
        y->children.push_back(x); // 2-cycle
        cyclicDoc.objects.push_back(x);
        ObjectIndex cyclicIdx;
        bool threw = false;
        try {
            (void)cyclicIdx.findById(cyclicDoc, "x");
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "cyclic children graph throws instead of crashing");
    }

    if (failures == 0) std::printf("All ObjectIndex tests passed.\n");
    else                std::printf("%d ObjectIndex test(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
