// F7 regression test: Model Registry "Insert" undo ordering.
//
// MeshCraftApplication_UiRegistry.cpp's drawRegistryPanel() Insert button
// handler used to call registry_.insertIntoScene(document_, entry) --
// which writes doc.textures/materials/definitions directly (see
// ModelRegistry.cpp:342) -- BEFORE pushUndo(), so only the trailing
// document_.objects.push_back(obj) was covered by the undo snapshot.
// Pressing Ctrl+Z right after an Insert removed the placed Instance object
// but left the imported definition and its textures/materials permanently
// orphaned in the document.
//
// The fix moves pushUndo() to before insertIntoScene(), and -- since
// insertIntoScene() only ever throws BEFORE touching doc (its temp-file
// I/O and its "no definitions" check both precede any doc mutation, per
// ModelRegistry.cpp:342-415) -- pops that snapshot back off unapplied via
// UndoManager::popUndoWithoutApplying() if insertIntoScene() throws, so a
// failed Insert doesn't leave a no-op undo entry behind (this codebase's
// established no-op-undo rule; see the Checkbox-mutates-in-place fix and
// EditorAlgorithms.hpp's anySelectedUnlockedAlg).
//
// This test exercises the REAL MeshCraft::Editor::UndoManager (not a
// mock stack, matching undo_manager_test.cpp's precedent) against a
// hand-rolled stand-in for the Insert handler's exact control-flow shape,
// since MeshCraftApplication's real drawRegistryPanel() is not headlessly
// callable (ImGui panel body tied into the full application).

#include "MeshCraft/Editor/UndoManager.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

using namespace MeshCraft::Editor;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) std::printf("PASS: %s\n", msg.c_str());
    else      { std::printf("FAIL: %s\n", msg.c_str()); ++failures; }
}

// Stand-in for ModelRegistry::insertIntoScene(): mirrors its real
// throw-only-before-mutating-doc contract. `shouldThrow` models a
// malformed registry entry (e.g. corrupted XML, no <definition>).
static std::string fakeInsertIntoScene(Mc3Document& doc, bool shouldThrow) {
    if (shouldThrow) throw std::runtime_error("Registry entry has no definitions");
    doc.textures["tex1"]     = Mc3Texture{};
    doc.materials["mat1"]    = Mc3Material{};
    doc.definitions["def1"]  = std::make_shared<Mc3Object>();
    return "def1";
}

// Mirrors the FIXED handler body in
// MeshCraftApplication_UiRegistry.cpp's Insert button:
//     pushUndo();
//     try { defId = registry_.insertIntoScene(document_, entry); }
//     catch (...) { undoManager_.popUndoWithoutApplying(); throw; }
//     ...document_.objects.push_back(obj); modified_ = true;...
static bool runInsert(Mc3Document& doc, UndoManager& undoMgr, bool registryThrows) {
    try {
        undoMgr.push(doc, {});
        std::string defId;
        try {
            defId = fakeInsertIntoScene(doc, registryThrows);
        } catch (...) {
            undoMgr.popUndoWithoutApplying();
            throw;
        }
        auto obj      = std::make_shared<Mc3Object>();
        obj->type     = ObjectType::Instance;
        obj->definition = defId;
        obj->id       = "reg_" + defId;
        doc.objects.push_back(obj);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int main() {
    // Success case: one Insert must push exactly ONE undo snapshot that
    // covers BOTH the textures/materials/definitions AND the placed
    // object -- not just the trailing objects.push_back().
    {
        Mc3Document doc;
        UndoManager undoMgr;

        bool ok = runInsert(doc, undoMgr, /*registryThrows=*/false);
        check(ok, "Insert: succeeds when the registry entry is well-formed");
        check(doc.textures.count("tex1") == 1, "Insert: texture was added to the document");
        check(doc.materials.count("mat1") == 1, "Insert: material was added to the document");
        check(doc.definitions.count("def1") == 1, "Insert: definition was added to the document");
        check(doc.objects.size() == 1, "Insert: the instance object was placed");
        check(undoMgr.undoCount() == 1,
              "Insert: exactly one undo snapshot for the whole operation (got " +
              std::to_string(undoMgr.undoCount()) + ")");

        auto entry = undoMgr.undo(doc, {});
        check(entry.has_value(), "Undo: an entry is available after one Insert");
        if (entry) {
            const Mc3Document& restored = *entry->snapshot;
            check(restored.textures.empty(),
                  "Undo: reverts the texture insertIntoScene() added (not just the object)");
            check(restored.materials.empty(),
                  "Undo: reverts the material insertIntoScene() added (not just the object)");
            check(restored.definitions.empty(),
                  "Undo: reverts the definition insertIntoScene() added (not just the object)");
            check(restored.objects.empty(),
                  "Undo: reverts the placed instance object");
        }
    }

    // Failure case: a malformed registry entry (insertIntoScene() throws
    // before touching doc) must leave NO undo entry behind -- the
    // pre-fix code never reached pushUndo() at all in this path, and the
    // fix must not regress that into a stray no-op snapshot instead.
    {
        Mc3Document doc;
        UndoManager undoMgr;

        bool ok = runInsert(doc, undoMgr, /*registryThrows=*/true);
        check(!ok, "Insert: reports failure when the registry entry is malformed");
        check(doc.textures.empty(), "Insert failure: document textures untouched");
        check(doc.materials.empty(), "Insert failure: document materials untouched");
        check(doc.definitions.empty(), "Insert failure: document definitions untouched");
        check(doc.objects.empty(), "Insert failure: no object was placed");
        check(undoMgr.undoCount() == 0,
              "Insert failure: no orphaned no-op undo snapshot left behind (got " +
              std::to_string(undoMgr.undoCount()) + ")");
        check(!undoMgr.canUndo(), "Insert failure: Ctrl+Z has nothing to do (no-op, not a crash)");
    }

    // Two successful Inserts in a row must each get their own snapshot,
    // and undoing once must only revert the SECOND Insert, leaving the
    // first intact -- proves snapshots aren't merged/dropped across calls.
    {
        Mc3Document doc;
        UndoManager undoMgr;

        check(runInsert(doc, undoMgr, false), "Second-insert setup: first Insert succeeds");
        // Second entry needs a distinct id to avoid the registry's own
        // collision-suffix logic changing the shape of this assertion.
        undoMgr.push(doc, {});
        doc.definitions["def2"] = std::make_shared<Mc3Object>();
        auto obj2 = std::make_shared<Mc3Object>();
        obj2->id  = "reg_def2";
        doc.objects.push_back(obj2);

        check(undoMgr.undoCount() == 2,
              "Two Inserts: two independent undo snapshots (got " +
              std::to_string(undoMgr.undoCount()) + ")");
        auto entry = undoMgr.undo(doc, {});
        check(entry.has_value(), "Two Inserts: undo after the second Insert returns an entry");
        if (entry) {
            check(entry->snapshot->definitions.count("def1") == 1,
                  "Two Inserts: undoing the second Insert leaves the first Insert's "
                  "definition intact");
            check(entry->snapshot->definitions.count("def2") == 0,
                  "Two Inserts: undoing the second Insert removes only that Insert's "
                  "definition");
        }
    }

    if (failures == 0) { std::printf("All registry-insert-undo tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d registry-insert-undo test(s) failed.\n", failures);
    return 1;
}
