#include "EditorAlgorithms.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

static int failures = 0;

static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }

#define CHECK(cond, msg)  do { if (cond) pass(msg); else fail(msg); } while(0)
#define CHECKF(a, b, msg) CHECK(std::abs((a)-(b)) < 1e-5f, msg)

static std::shared_ptr<Mc3Object> makeObj(const std::string& id,
                                          const std::string& name,
                                          Mc3::ObjectType type = Mc3::ObjectType::Box)
{
    auto o = std::make_shared<Mc3Object>();
    o->id   = id;
    o->name = name;
    o->type = type;
    return o;
}

// ─────────────────────────────────────────────────────────────────────────────
// Undo/redo helpers (STAB-0279)
//
// The editor's undo/redo is snapshot-based: pushUndo() stores a full
// deepCopyDoc(document_) onto undoStack_ before each command, and undo/redo
// swap those whole-document snapshots (see MeshCraftApplication_Commands.cpp +
// deepCopyDoc in MeshCraftPrivate.hpp). That private header is CNA-coupled and
// can't be included here, so snapshotDoc() reproduces deepCopyDoc() exactly
// using the CNA-free deepCopyObjectAlg primitive the real code is built on.
// ─────────────────────────────────────────────────────────────────────────────

static Mc3Document snapshotDoc(const Mc3Document& src)
{
    Mc3Document copy = src;          // value copy — object shared_ptrs are shallow
    copy.objects.clear();
    for (const auto& obj : src.objects)
        copy.objects.push_back(deepCopyObjectAlg(*obj));
    copy.definitions.clear();
    for (const auto& [key, obj] : src.definitions)
        copy.definitions[key] = deepCopyObjectAlg(*obj);
    return copy;
}

// Serialise a document to canonical XML and return it as a string. Used as a
// thorough deep-equality oracle: it captures every serialised field, so an
// incomplete snapshot/restore is caught — not only the fields a command edits.
static std::string docToXml(const Mc3Document& doc)
{
    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_cmd_undo_" + std::to_string(tmpIdx++) + ".mc3.xml");
    doc.saveToFile(path);
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return ss.str();
}

// Runs the full snapshot -> mutate -> undo -> redo cycle the editor performs and
// asserts the document state round-trips exactly.
template <class Mutate>
static void checkUndoRedo(const std::string& label, Mc3Document& doc, Mutate mutate)
{
    const std::string before = docToXml(doc);

    Mc3Document snap = snapshotDoc(doc);   // pushUndo()
    mutate(doc);                           // command executes
    const std::string after = docToXml(doc);
    CHECK(after != before, label + ": command actually mutates the document");

    // Undo — restore the pre-command snapshot.
    doc = snapshotDoc(snap);
    CHECK(docToXml(doc) == before, label + ": undo restores the original state");

    // Redo — re-run the command on the restored document.
    mutate(doc);
    CHECK(docToXml(doc) == after, label + ": redo reproduces the mutated state");
}

// A small scene with non-default fields (position, material, visibility,
// nested child) so the round-trip exercises more than just names.
static Mc3Document makeUndoScene()
{
    Mc3Document doc;
    doc.model = "UndoScene";

    auto a = makeObj("a", "BoxA");
    a->transform.position[0] = 1.0f;
    a->transform.position[1] = 2.0f;
    a->transform.position[2] = 3.0f;
    a->material = "stone";
    auto child = makeObj("a_child", "BoxChild", Mc3::ObjectType::Sphere);
    a->children.push_back(child);

    auto b = makeObj("b", "BoxB");
    b->visible = false;

    doc.objects = {a, b};
    return doc;
}

// ─────────────────────────────────────────────────────────────────────────────
// applyRenamePatternAlg
// ─────────────────────────────────────────────────────────────────────────────

static void testApplyRenamePattern()
{
    // {index} → 1-based counter
    CHECK(applyRenamePatternAlg("{index}", "Box", 1, "Box") == "1",
          "pattern {index} at idx=1");
    CHECK(applyRenamePatternAlg("{index}", "Box", 5, "Box") == "5",
          "pattern {index} at idx=5");

    // {index:02d}
    CHECK(applyRenamePatternAlg("{index:02d}", "X", 3, "Sphere") == "03",
          "pattern {index:02d}");

    // {index:03d}
    CHECK(applyRenamePatternAlg("{index:03d}", "X", 7, "Sphere") == "007",
          "pattern {index:03d}");

    // {index0} → 0-based
    CHECK(applyRenamePatternAlg("{index0}", "X", 1, "Box") == "0",
          "pattern {index0} at idx=1 → 0");
    CHECK(applyRenamePatternAlg("{index0}", "X", 3, "Box") == "2",
          "pattern {index0} at idx=3 → 2");

    // {name} substitution
    CHECK(applyRenamePatternAlg("copy_{name}", "Door", 1, "Mesh") == "copy_Door",
          "pattern {name}");

    // {type} substitution
    CHECK(applyRenamePatternAlg("{type}_{index}", "X", 2, "Cylinder") == "Cylinder_2",
          "pattern {type}_{index}");

    // Combined
    CHECK(applyRenamePatternAlg("{name}_{index:03d}", "Wall", 4, "Box") == "Wall_004",
          "pattern {name}_{index:03d}");

    // No tokens → string unchanged
    CHECK(applyRenamePatternAlg("static_name", "Box", 1, "Box") == "static_name",
          "pattern with no tokens");
}

// ─────────────────────────────────────────────────────────────────────────────
// batchRenameObjects
// ─────────────────────────────────────────────────────────────────────────────

static void testBatchRename()
{
    auto o1 = makeObj("id1", "Box");
    auto o2 = makeObj("id2", "Sphere", Mc3::ObjectType::Sphere);
    auto o3 = makeObj("id3", "Plane",  Mc3::ObjectType::Plane);
    std::vector<std::shared_ptr<Mc3Object>> sel = {o1, o2, o3};
    std::set<std::string> locked;

    // Pattern: {type}_{index:02d}
    int renamed = batchRenameObjects(sel, locked, "{type}_{index:02d}");
    CHECK(renamed == 3, "batch rename: 3 objects renamed");
    CHECK(o1->name == "Box_01",    "batch rename o1 → Box_01");
    CHECK(o2->name == "Sphere_02", "batch rename o2 → Sphere_02");
    CHECK(o3->name == "Plane_03",  "batch rename o3 → Plane_03");

    // With one locked object — locked object keeps original name, index still advances
    auto a = makeObj("a", "Alpha");
    auto b = makeObj("b", "Beta");
    auto c = makeObj("c", "Gamma");
    std::vector<std::shared_ptr<Mc3Object>> sel2 = {a, b, c};
    std::set<std::string> locked2 = {"b"};

    int renamed2 = batchRenameObjects(sel2, locked2, "item_{index}");
    CHECK(renamed2 == 2,           "batch rename with lock: 2 objects renamed");
    CHECK(a->name == "item_1",     "batch rename a → item_1");
    CHECK(b->name == "Beta",       "batch rename b unchanged (locked)");
    CHECK(c->name == "item_3",     "batch rename c → item_3 (index continues past lock)");

    // {name} token keeps original name embedded
    auto x = makeObj("x", "Chair");
    std::vector<std::shared_ptr<Mc3Object>> sel3 = {x};
    batchRenameObjects(sel3, {}, "copy_{name}");
    CHECK(x->name == "copy_Chair", "batch rename {name} token");
}

// ─────────────────────────────────────────────────────────────────────────────
// find-replace helpers
// ─────────────────────────────────────────────────────────────────────────────

static void testFindReplace()
{
    // Basic case-sensitive replace
    {
        auto o1 = makeObj("1", "BoxA");
        auto o2 = makeObj("2", "BoxB");
        auto o3 = makeObj("3", "Sphere");
        std::vector<std::shared_ptr<Mc3Object>> objects = {o1, o2, o3};
        std::set<std::string> locked, selIds;

        int cnt = countFindReplaceMatches(objects, locked, "Box", "Cube",
                                          /*caseSensitive=*/true, false, selIds);
        CHECK(cnt == 2, "find-replace count: 2 matches for 'Box'");
        // names not yet changed
        CHECK(o1->name == "BoxA", "count-only does not mutate");

        int applied = applyFindReplaceNames(objects, locked, "Box", "Cube",
                                            true, false, selIds);
        CHECK(applied == 2,       "find-replace applied: 2");
        CHECK(o1->name == "CubeA", "find-replace o1 → CubeA");
        CHECK(o2->name == "CubeB", "find-replace o2 → CubeB");
        CHECK(o3->name == "Sphere","find-replace o3 unchanged");
    }

    // Case-insensitive
    {
        auto o1 = makeObj("1", "boxDoor");
        auto o2 = makeObj("2", "BOXTOP");
        std::vector<std::shared_ptr<Mc3Object>> objects = {o1, o2};
        std::set<std::string> locked, selIds;

        int cnt = countFindReplaceMatches(objects, locked, "box", "wall",
                                          /*caseSensitive=*/false, false, selIds);
        CHECK(cnt == 2, "case-insensitive count: 2 matches");

        applyFindReplaceNames(objects, locked, "box", "wall", false, false, selIds);
        CHECK(o1->name == "wallDoor", "case-insensitive replace o1");
        CHECK(o2->name == "wallTOP",  "case-insensitive replace o2");
    }

    // Locked objects are skipped
    {
        auto o1 = makeObj("1", "Fence");
        auto o2 = makeObj("2", "Fence");
        std::vector<std::shared_ptr<Mc3Object>> objects = {o1, o2};
        std::set<std::string> locked = {"1"};
        std::set<std::string> selIds;

        int cnt = countFindReplaceMatches(objects, locked, "Fence", "Wall",
                                          true, false, selIds);
        CHECK(cnt == 1, "find-replace count skips locked");

        applyFindReplaceNames(objects, locked, "Fence", "Wall", true, false, selIds);
        CHECK(o1->name == "Fence", "locked object not renamed");
        CHECK(o2->name == "Wall",  "unlocked object renamed");
    }

    // selectedOnly=true: only rename objects whose ids are in selIds
    {
        auto o1 = makeObj("1", "Door");
        auto o2 = makeObj("2", "Door");
        auto o3 = makeObj("3", "Door");
        std::vector<std::shared_ptr<Mc3Object>> objects = {o1, o2, o3};
        std::set<std::string> locked;
        std::set<std::string> selIds = {"1", "3"};

        applyFindReplaceNames(objects, locked, "Door", "Gate", true,
                              /*selectedOnly=*/true, selIds);
        CHECK(o1->name == "Gate", "selectedOnly renames selected o1");
        CHECK(o2->name == "Door", "selectedOnly leaves unselected o2");
        CHECK(o3->name == "Gate", "selectedOnly renames selected o3");
    }

    // Recursive tree walk: replaces in children too
    {
        auto parent = makeObj("p", "BoxParent");
        auto child  = makeObj("c", "BoxChild");
        parent->children.push_back(child);
        std::vector<std::shared_ptr<Mc3Object>> objects = {parent};
        std::set<std::string> locked, selIds;

        applyFindReplaceNames(objects, locked, "Box", "Cube", true, false, selIds);
        CHECK(parent->name == "CubeParent", "find-replace parent renamed");
        CHECK(child->name  == "CubeChild",  "find-replace child renamed recursively");
    }

    // No matches → count = 0
    {
        auto o1 = makeObj("1", "Tree");
        std::vector<std::shared_ptr<Mc3Object>> objects = {o1};
        std::set<std::string> locked, selIds;

        int cnt = countFindReplaceMatches(objects, locked, "Box", "Cube",
                                          true, false, selIds);
        CHECK(cnt == 0, "find-replace no matches → count 0");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// arrayDuplicateObjects
// ─────────────────────────────────────────────────────────────────────────────

static void testArrayDuplicate()
{
    // Relative mode: count=3, axis=X, spacing=2.0
    {
        auto src = makeObj("box1", "Box");
        src->transform.position[0] = 1.0f;   // X=1
        src->transform.position[1] = 5.0f;
        src->transform.position[2] = 0.0f;

        std::vector<std::shared_ptr<Mc3Object>> root = {src};
        std::vector<std::shared_ptr<Mc3Object>> sources = {src};

        auto created = arrayDuplicateObjects(root, sources,
                                             /*count=*/3, /*axis=*/0,
                                             /*spacing=*/2.0f, /*relative=*/true);
        CHECK(created.size() == 2, "array dup relative: 2 copies created (count-1)");
        CHECK(root.size() == 3,    "array dup: 3 objects in root after dup");

        // copy i=1: pos[0] = 1 + 1*2 = 3
        CHECKF(created[0]->transform.position[0], 3.0f, "array dup copy1 X=3");
        // copy i=2: pos[0] = 1 + 2*2 = 5
        CHECKF(created[1]->transform.position[0], 5.0f, "array dup copy2 X=5");
        // Y unchanged
        CHECKF(created[0]->transform.position[1], 5.0f, "array dup Y unchanged");

        // Names and IDs
        CHECK(created[0]->name == "Box_1",      "array dup copy1 name=Box_1");
        CHECK(created[1]->name == "Box_2",      "array dup copy2 name=Box_2");
        CHECK(created[0]->id   == "box1_arr1",  "array dup copy1 id=box1_arr1");
    }

    // Absolute mode: count=3, axis=Y, spacing=4.0
    {
        auto src = makeObj("s", "Sphere", Mc3::ObjectType::Sphere);
        src->transform.position[1] = 10.0f;   // Y=10, should be ignored in abs mode

        std::vector<std::shared_ptr<Mc3Object>> root = {src};
        auto created = arrayDuplicateObjects(root, {src},
                                             3, /*axis=*/1,
                                             4.0f, /*relative=*/false);
        CHECK(created.size() == 2, "array dup absolute: 2 copies");
        // copy i=1: pos[1] = 1*4 = 4
        CHECKF(created[0]->transform.position[1], 4.0f,  "array dup abs copy1 Y=4");
        // copy i=2: pos[1] = 2*4 = 8
        CHECKF(created[1]->transform.position[1], 8.0f,  "array dup abs copy2 Y=8");
    }

    // count=2 → exactly 1 copy
    {
        auto src = makeObj("x", "X");
        std::vector<std::shared_ptr<Mc3Object>> root = {src};
        auto created = arrayDuplicateObjects(root, {src}, 2, 0, 1.0f, true);
        CHECK(created.size() == 1, "array dup count=2 → 1 copy");
    }

    // count=1 is clamped to 2 → still 1 copy
    {
        auto src = makeObj("y", "Y");
        std::vector<std::shared_ptr<Mc3Object>> root = {src};
        auto created = arrayDuplicateObjects(root, {src}, 1, 0, 1.0f, true);
        CHECK(created.size() == 1, "array dup count=1 clamped to 2 → 1 copy");
    }

    // Order in root: copies inserted after source, in correct order
    {
        auto a   = makeObj("a", "A");
        auto b   = makeObj("b", "B");
        a->transform.position[0] = 0.0f;

        std::vector<std::shared_ptr<Mc3Object>> root = {a, b};
        arrayDuplicateObjects(root, {a}, 3, 0, 1.0f, true);
        // root should be [A, A_1, A_2, B]
        CHECK(root.size() == 4,          "array dup insertion order: 4 objects");
        CHECK(root[0]->name == "A",      "root[0] = original A");
        CHECK(root[1]->name == "A_1",    "root[1] = A_1");
        CHECK(root[2]->name == "A_2",    "root[2] = A_2");
        CHECK(root[3]->name == "B",      "root[3] = B (not displaced)");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// deepCopyObjectAlg
// ─────────────────────────────────────────────────────────────────────────────

static void testDeepCopy()
{
    auto src = makeObj("orig", "Original");
    src->transform.position[0] = 3.0f;
    auto child = makeObj("ch", "Child");
    child->transform.position[1] = 7.0f;
    src->children.push_back(child);

    auto copy = deepCopyObjectAlg(*src);

    CHECK(copy->id   == "orig",     "deep copy id");
    CHECK(copy->name == "Original", "deep copy name");
    CHECKF(copy->transform.position[0], 3.0f, "deep copy position");

    CHECK(copy->children.size() == 1, "deep copy child count");
    CHECK(copy->children[0]->name == "Child", "deep copy child name");
    CHECKF(copy->children[0]->transform.position[1], 7.0f, "deep copy child position");

    // Mutation independence
    copy->name = "Copy";
    CHECK(src->name  == "Original", "deep copy is independent (name)");
    copy->children[0]->name = "ChangedChild";
    CHECK(child->name == "Child",   "deep copy child is independent");
}

// ─────────────────────────────────────────────────────────────────────────────
// Undo/redo round-trip per command (STAB-0279)
// ─────────────────────────────────────────────────────────────────────────────

static void testUndoRedoBatchRename()
{
    Mc3Document doc = makeUndoScene();
    std::set<std::string> locked;
    checkUndoRedo("batchRename", doc, [&](Mc3Document& d) {
        batchRenameObjects(d.objects, locked, "{type}_{index:02d}");
    });
}

static void testUndoRedoFindReplace()
{
    Mc3Document doc = makeUndoScene();
    std::set<std::string> locked, selIds;
    checkUndoRedo("findReplace", doc, [&](Mc3Document& d) {
        // Recurses into children too, so the nested BoxChild is renamed.
        applyFindReplaceNames(d.objects, locked, "Box", "Cube",
                              /*caseSensitive=*/true, /*selectedOnly=*/false, selIds);
    });
}

static void testUndoRedoArrayDuplicate()
{
    Mc3Document doc = makeUndoScene();
    checkUndoRedo("arrayDuplicate", doc, [&](Mc3Document& d) {
        std::vector<std::shared_ptr<Mc3Object>> sources = { d.objects.front() };
        arrayDuplicateObjects(d.objects, sources,
                              /*count=*/3, /*axis=*/0, /*spacing=*/2.0f, /*relative=*/true);
    });
}

// The snapshot must be a deep, independent copy: mutating the live document
// after snapshotting must not change the snapshot. A shallow copy (sharing
// object shared_ptrs) would silently corrupt the undo history.
static void testSnapshotIndependence()
{
    Mc3Document doc = makeUndoScene();
    Mc3Document snap = snapshotDoc(doc);            // pushUndo()
    const std::string snapBefore = docToXml(snap);

    // Mutate the live document in several ways.
    batchRenameObjects(doc.objects, {}, "MUT_{index}");
    doc.objects.front()->transform.position[0] = 999.0f;
    doc.objects.front()->children.clear();
    doc.objects.pop_back();

    CHECK(docToXml(snap) == snapBefore,
          "snapshot independence: mutating the live document leaves the snapshot intact");
}

// ─────────────────────────────────────────────────────────────────────────────
// autoSaveTickAlg / autoSavePathAlg (STAB-0265)
// ─────────────────────────────────────────────────────────────────────────────

static void testAutoSavePath()
{
    CHECK(autoSavePathAlg("scene.mc3.xml") == "scene.mc3.xml.autosave",
          "auto-save path appends .autosave suffix");
    CHECK(autoSavePathAlg("/tmp/proj/scene.mc3.xml") ==
              "/tmp/proj/scene.mc3.xml.autosave",
          "auto-save path preserves directory");
}

static void testAutoSaveIntervalConfigurable()
{
    // A custom (non-default) interval of 5s: countdown reaches 0 after 5s of
    // elapsed dt and triggers exactly once, then resets to the same interval.
    {
        float countdown = 5.0f;
        bool triggered = false;
        for (int i = 0; i < 5; ++i)
            triggered = autoSaveTickAlg(/*hasFile=*/true, /*modified=*/true,
                                        /*interval=*/5.0f, /*dt=*/1.0f, countdown) || triggered;
        CHECK(triggered, "custom 5s interval: triggers once 5s elapse");
        CHECKF(countdown, 5.0f, "custom 5s interval: countdown resets to the configured interval");
    }
    // A different interval (2s) changes when the trigger fires — proves the
    // interval is actually read, not hard-coded.
    {
        float countdown = 2.0f;
        bool triggeredEarly = autoSaveTickAlg(true, true, 2.0f, 1.0f, countdown);
        CHECK(!triggeredEarly, "custom 2s interval: no trigger after 1 of 2s");
        bool triggeredNow = autoSaveTickAlg(true, true, 2.0f, 1.0f, countdown);
        CHECK(triggeredNow, "custom 2s interval: triggers after the full 2s");
    }
}

static void testAutoSaveDisabledWhenIntervalIsZero()
{
    float countdown = 60.0f;
    bool everTriggered = false;
    for (int i = 0; i < 1000; ++i)
        everTriggered = autoSaveTickAlg(/*hasFile=*/true, /*modified=*/true,
                                        /*interval=*/0.0f, /*dt=*/1.0f, countdown) || everTriggered;
    CHECK(!everTriggered, "interval=0 disables auto-save entirely");
    CHECKF(countdown, 60.0f, "interval=0 pins countdown at the 60s fallback");
}

static void testAutoSaveSkippedWhenNotModifiedOrNoFile()
{
    float countdown = 1.0f;
    CHECK(!autoSaveTickAlg(/*hasFile=*/true, /*modified=*/false,
                           /*interval=*/5.0f, /*dt=*/10.0f, countdown),
          "unmodified document never auto-saves even past the interval");
    CHECKF(countdown, 5.0f, "unmodified document: countdown reset to the interval");

    countdown = 1.0f;
    CHECK(!autoSaveTickAlg(/*hasFile=*/false, /*modified=*/true,
                           /*interval=*/5.0f, /*dt=*/10.0f, countdown),
          "no current file never auto-saves even past the interval");
    CHECKF(countdown, 5.0f, "no current file: countdown reset to the interval");
}

// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    testApplyRenamePattern();
    testBatchRename();
    testFindReplace();
    testArrayDuplicate();
    testDeepCopy();
    testUndoRedoBatchRename();
    testUndoRedoFindReplace();
    testUndoRedoArrayDuplicate();
    testSnapshotIndependence();
    testAutoSavePath();
    testAutoSaveIntervalConfigurable();
    testAutoSaveDisabledWhenIntervalIsZero();
    testAutoSaveSkippedWhenNotModifiedOrNoFile();

    std::cout << "\n";
    if (failures == 0)
        std::cout << "All tests passed.\n";
    else
        std::cout << failures << " test(s) FAILED.\n";

    return failures == 0 ? 0 : 1;
}
