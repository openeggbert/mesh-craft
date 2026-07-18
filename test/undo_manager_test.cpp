// Editor::UndoManager test (SYS-W3-01 Phase 4).
//
// No test existed for this subsystem before its extraction (new this
// session; undo_gesture_frame_test.cpp already covers the redo-
// invalidation and selection-pairing *invariants* against hand-rolled
// stand-ins, since MeshCraftApplication's real pushUndo()/performUndo()/
// performRedo() aren't headlessly callable -- this test instead exercises
// the REAL extracted class directly). Covers: push()/undo()/redo() basic
// semantics, redo-stack invalidation on a new push, the kMax depth cap on
// both stacks, selection ids traveling in lockstep with each document,
// jumpTo()'s multi-step redo-stack rebuild (including its own kMax cap and
// out-of-range rejection), and popUndoWithoutApplying()/clear().

#include "MeshCraft/Editor/UndoManager.hpp"

#include <cstdio>

using namespace MeshCraft::Editor;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const char* msg) {
    if (cond) std::printf("PASS: %s\n", msg);
    else      { std::printf("FAIL: %s\n", msg); ++failures; }
}

// A document's own identity for these tests is tracked via its objects'
// ids (Mc3Document has no other cheap-to-compare distinguishing field),
// so each "version" is built with one uniquely-named root object.
static Mc3Document makeDoc(const std::string& tag) {
    Mc3Document doc;
    auto obj = std::make_shared<Mc3Object>();
    obj->id = tag;
    doc.objects.push_back(obj);
    return doc;
}

static std::string tagOf(const Mc3Document& doc) {
    return doc.objects.empty() ? "" : doc.objects.front()->id;
}

int main() {
    // Fresh manager.
    {
        UndoManager mgr;
        check(!mgr.canUndo() && !mgr.canRedo(), "starts with empty undo/redo stacks");
        check(mgr.undoCount() == 0 && mgr.redoCount() == 0, "starts with zero counts");
    }

    // push() basics + redo invalidation.
    {
        UndoManager mgr;
        mgr.push(makeDoc("v1"), {"a"});
        check(mgr.canUndo() && mgr.undoCount() == 1, "push() adds one undo entry");
        mgr.push(makeDoc("v2"), {"a", "b"});
        check(mgr.undoCount() == 2, "a second push() adds a second undo entry");

        auto e1 = mgr.undo(makeDoc("current"), {"x"});
        check(e1.has_value() && tagOf(e1->doc) == "v2", "undo() returns the most recently pushed document");
        check(e1->selectionIds.size() == 2 && e1->selectionIds[0] == "a" && e1->selectionIds[1] == "b",
              "undo() returns the selection ids paired with that document");
        check(mgr.canRedo() && mgr.redoCount() == 1, "undo() moves the current state onto the redo stack");
        check(mgr.undoCount() == 1, "undo() pops exactly one entry off the undo stack");

        // A brand-new push() while redo history exists must invalidate it.
        mgr.push(makeDoc("v3"), {"c"});
        check(!mgr.canRedo(), "push() after an undo clears the redo stack (redo invalidation)");
    }

    // redo() basics.
    {
        UndoManager mgr;
        mgr.push(makeDoc("v1"), {"a"});
        auto undone = mgr.undo(makeDoc("v2"), {"b"});
        check(undone.has_value(), "undo() succeeds with one entry present");
        check(mgr.canRedo(), "redo stack has one entry after undo()");

        auto redone = mgr.redo(makeDoc("v1-restored"), {"a"});
        check(redone.has_value() && tagOf(redone->doc) == "v2", "redo() returns the document undo() had displaced");
        check(redone->selectionIds.size() == 1 && redone->selectionIds[0] == "b",
              "redo() returns the selection ids paired with that document");
        check(!mgr.canRedo(), "redo() pops the redo stack back to empty");
        check(mgr.canUndo() && mgr.undoCount() == 1, "redo() pushes the pre-redo state back onto the undo stack");
    }

    // undo()/redo() are no-ops (return nullopt) on an empty stack.
    {
        UndoManager mgr;
        check(!mgr.undo(makeDoc("x"), {}).has_value(), "undo() on an empty undo stack returns nullopt");
        check(!mgr.redo(makeDoc("x"), {}).has_value(), "redo() on an empty redo stack returns nullopt");
        check(mgr.undoCount() == 0 && mgr.redoCount() == 0,
              "a no-op undo()/redo() does not add spurious entries to either stack");
    }

    // kMax depth cap: pushing well past kMax must keep only the most
    // recent kMax entries (oldest evicted first).
    {
        UndoManager mgr;
        for (int i = 0; i < UndoManager::kMax + 10; ++i)
            mgr.push(makeDoc("v" + std::to_string(i)), {});
        check(mgr.undoCount() == UndoManager::kMax, "push() caps the undo stack at kMax entries");
        auto e = mgr.undo(makeDoc("current"), {});
        check(e.has_value() && tagOf(e->doc) == ("v" + std::to_string(UndoManager::kMax + 9)),
              "the cap keeps the most recent entries, not the oldest");
    }

    // jumpTo(): jumping straight to an older entry rebuilds the redo stack
    // with everything newer than the target, in the correct order, and
    // trims the undo stack to just what's older than the target.
    {
        UndoManager mgr;
        mgr.push(makeDoc("v1"), {"1"});
        mgr.push(makeDoc("v2"), {"2"});
        mgr.push(makeDoc("v3"), {"3"});
        check(mgr.undoCount() == 3, "3 entries pushed before jumpTo test");

        // stepsAgo=1 is the most recent (v3); jump to stepsAgo=3 (v1, the
        // oldest), which must push current+v3+v2 onto the redo stack.
        auto jumped = mgr.jumpTo(3, makeDoc("current"), {"cur"});
        check(jumped.has_value() && tagOf(jumped->doc) == "v1", "jumpTo(3) returns the oldest (3rd-from-top) entry");
        check(jumped->selectionIds.size() == 1 && jumped->selectionIds[0] == "1",
              "jumpTo() returns the selection ids paired with the target entry");
        check(mgr.undoCount() == 0, "jumpTo() to the oldest entry empties the undo stack");
        check(mgr.redoCount() == 3, "jumpTo() pushes current + every newer entry onto the redo stack");

        // Redo stack must now pop back in the reverse of how they became
        // "newer" -- current first (most recently redo()-able is v2, then
        // v3, then the pre-jump current state).
        auto r1 = mgr.redo(makeDoc("after-v1"), {"1"});
        check(r1.has_value() && tagOf(r1->doc) == "v2", "after jumpTo(oldest), first redo() restores v2");
        auto r2 = mgr.redo(makeDoc("after-v2"), {"2"});
        check(r2.has_value() && tagOf(r2->doc) == "v3", "after jumpTo(oldest), second redo() restores v3");
        auto r3 = mgr.redo(makeDoc("after-v3"), {"3"});
        check(r3.has_value() && tagOf(r3->doc) == "current", "after jumpTo(oldest), third redo() restores the pre-jump current state");
        check(!mgr.canRedo(), "redo stack is empty after redoing everything jumpTo() had pushed");
    }

    // jumpTo() out-of-range requests are rejected without mutating state.
    {
        UndoManager mgr;
        mgr.push(makeDoc("v1"), {});
        check(!mgr.jumpTo(0, makeDoc("x"), {}).has_value(), "jumpTo(0) is rejected (stepsAgo is 1-based)");
        check(!mgr.jumpTo(2, makeDoc("x"), {}).has_value(), "jumpTo() past undoCount() is rejected");
        check(!mgr.jumpTo(-1, makeDoc("x"), {}).has_value(), "jumpTo() with a negative stepsAgo is rejected");
        check(mgr.undoCount() == 1 && mgr.redoCount() == 0,
              "a rejected jumpTo() call leaves both stacks unmodified");
    }

    // jumpTo()'s own redo-stack cap (AUDIT-0056): jumping past many entries
    // at once must not let the rebuilt redo stack exceed kMax.
    {
        UndoManager mgr;
        for (int i = 0; i < UndoManager::kMax + 5; ++i)
            mgr.push(makeDoc("v" + std::to_string(i)), {});
        check(mgr.undoCount() == UndoManager::kMax, "setup: undo stack capped at kMax before the jump");
        auto jumped = mgr.jumpTo(mgr.undoCount(), makeDoc("current"), {}); // jump to the oldest surviving entry
        check(jumped.has_value(), "jumpTo() to the oldest surviving entry succeeds");
        check(mgr.redoCount() == UndoManager::kMax,
              "jumpTo() across many entries still caps the rebuilt redo stack at kMax");
    }

    // popUndoWithoutApplying(): used by SYS-W12-02's benchmark to measure
    // push() cost without leaving a stray entry behind.
    {
        UndoManager mgr;
        mgr.push(makeDoc("v1"), {"a"});
        check(mgr.undoCount() == 1, "setup: one entry pushed");
        mgr.popUndoWithoutApplying();
        check(mgr.undoCount() == 0, "popUndoWithoutApplying() removes the most recent entry");
        check(!mgr.canRedo(), "popUndoWithoutApplying() does not touch the redo stack");
        mgr.popUndoWithoutApplying(); // must not crash/underflow on an empty stack
        check(mgr.undoCount() == 0, "popUndoWithoutApplying() on an empty undo stack is a safe no-op");
    }

    // clear(): discards all history (Open/New/autosave-recovery).
    {
        UndoManager mgr;
        mgr.push(makeDoc("v1"), {"a"});
        mgr.push(makeDoc("v2"), {"b"});
        mgr.undo(makeDoc("v3"), {"c"});
        check(mgr.canUndo() && mgr.canRedo(), "setup: both stacks non-empty before clear()");
        mgr.clear();
        check(!mgr.canUndo() && !mgr.canRedo(), "clear() empties both the undo and redo stacks");
        check(mgr.undoCount() == 0 && mgr.redoCount() == 0, "clear() resets both counts to zero");
    }

    if (failures == 0) std::printf("All UndoManager tests passed.\n");
    else                std::printf("%d UndoManager test(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
