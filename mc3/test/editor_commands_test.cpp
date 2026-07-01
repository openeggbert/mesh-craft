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

static std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
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
    std::string xml = readFile(path);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return xml;
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
// duplicateObjectsAlg / groupObjectsAlg / ungroupObjectAlg (STAB-0280/0281/0282)
// ─────────────────────────────────────────────────────────────────────────────

static void testDuplicateObjects()
{
    auto a = makeObj("a", "A");
    auto b = makeObj("b", "B");
    a->transform.position[0] = 1.0f;
    std::vector<std::shared_ptr<Mc3Object>> root = {a, b};

    auto created = duplicateObjectsAlg(root, {a});
    CHECK(created.size() == 1,        "duplicate: 1 copy created");
    CHECK(root.size() == 3,           "duplicate: root grows by 1");
    CHECK(root[0]->name == "A",       "duplicate: original stays at index 0");
    CHECK(root[1]->name == "A_copy",  "duplicate: copy inserted right after original");
    CHECK(root[1]->id   == "a_copy",  "duplicate: copy id suffixed with _copy");
    CHECK(root[2]->name == "B",       "duplicate: B not displaced");
    CHECKF(root[1]->transform.position[0], 1.0f, "duplicate: copy preserves transform");

    // Copy is independent of the original.
    root[1]->transform.position[0] = 99.0f;
    CHECKF(a->transform.position[0], 1.0f, "duplicate: copy is a deep, independent copy");
}

static void testGroupAndUngroupObjects()
{
    auto a = makeObj("a", "A");
    auto b = makeObj("b", "B");
    auto c = makeObj("c", "C");
    std::vector<std::shared_ptr<Mc3Object>> root = {a, b, c};

    auto group = groupObjectsAlg(root, {a, c}, "Group1");
    CHECK(group != nullptr,        "group: returns the new group object");
    CHECK(root.size() == 2,        "group: a and c removed from root, group inserted");
    CHECK(root[0]->name == "Group1" || root[1]->name == "Group1",
          "group: the new group is somewhere in root");
    CHECK(group->children.size() == 2, "group: group has 2 children");
    CHECK(group->children[0]->name == "A", "group: child order preserved (A)");
    CHECK(group->children[1]->name == "C", "group: child order preserved (C)");
    bool bStillTopLevel = std::any_of(root.begin(), root.end(),
        [&](const auto& o){ return o.get() == b.get(); });
    CHECK(bStillTopLevel, "group: B (not selected) stays top-level, ungrouped");

    auto restored = ungroupObjectAlg(root, group);
    CHECK(restored.size() == 2, "ungroup: returns the 2 former children");
    CHECK(root.size() == 3,     "ungroup: root back to 3 top-level objects");
    bool groupGone = std::none_of(root.begin(), root.end(),
        [&](const auto& o){ return o.get() == group.get(); });
    CHECK(groupGone, "ungroup: the group object itself is removed");
    bool aRestored = std::any_of(root.begin(), root.end(),
        [&](const auto& o){ return o.get() == a.get(); });
    bool cRestored = std::any_of(root.begin(), root.end(),
        [&](const auto& o){ return o.get() == c.get(); });
    CHECK(aRestored && cRestored, "ungroup: A and C are back at top level");
}

static void testUngroupRejectsNonGroupOrEmptyGroup()
{
    auto a = makeObj("a", "A");
    std::vector<std::shared_ptr<Mc3Object>> root = {a};
    CHECK(ungroupObjectAlg(root, a).empty(),
          "ungroup: rejects a non-Group object (no-op)");
    CHECK(root.size() == 1, "ungroup: root unchanged after rejected ungroup");

    auto emptyGroup = std::make_shared<Mc3Object>();
    emptyGroup->type = Mc3::ObjectType::Group;
    emptyGroup->name = "Empty";
    root.push_back(emptyGroup);
    CHECK(ungroupObjectAlg(root, emptyGroup).empty(),
          "ungroup: rejects a Group with no children (no-op)");
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

static void testUndoRedoDuplicate()
{
    Mc3Document doc = makeUndoScene();
    checkUndoRedo("duplicate", doc, [&](Mc3Document& d) {
        duplicateObjectsAlg(d.objects, { d.objects.front() });
    });
}

static void testUndoRedoGroup()
{
    Mc3Document doc = makeUndoScene();
    checkUndoRedo("group", doc, [&](Mc3Document& d) {
        groupObjectsAlg(d.objects, { d.objects[0], d.objects[1] }, "TestGroup");
    });
}

static void testUndoRedoUngroup()
{
    // Ungroup acts on a pre-existing group, so build one before the command
    // under test runs (group creation itself is covered by testUndoRedoGroup).
    Mc3Document doc = makeUndoScene();
    groupObjectsAlg(doc.objects, { doc.objects[0], doc.objects[1] }, "PreGroup");
    checkUndoRedo("ungroup", doc, [&](Mc3Document& d) {
        // Re-locate the group by name on every call (including the post-undo
        // redo call) rather than capturing a shared_ptr from setup: after
        // undo, `d` has been replaced by a fresh deep copy (snapshotDoc), so
        // a captured pointer from before snapshotting would be stale.
        auto it = std::find_if(d.objects.begin(), d.objects.end(),
            [](const auto& o){ return o->name == "PreGroup"; });
        if (it != d.objects.end()) ungroupObjectAlg(d.objects, *it);
    });
}

// Material edits (STAB-0283) go through PropertiesPanel.cpp's ImGui
// ColorEdit4 widget in the real app, which can't run headlessly — but the
// *document*-level mutation it performs is a plain field assignment on
// Mc3Document::materials, which is exactly what the generic snapshot/undo
// mechanism (STAB-0279) needs to round-trip. No new Alg function needed.
static void testUndoRedoMaterialEdit()
{
    Mc3Document doc = makeUndoScene();
    doc.materials["stone"] = Mc3::Mc3Material("stone", {0.5f, 0.5f, 0.5f, 1.0f});
    checkUndoRedo("materialEdit", doc, [&](Mc3Document& d) {
        d.materials["stone"].baseColor = {0.1f, 0.2f, 0.3f, 0.9f};
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
// Auto-save writes a separate file (STAB-0266)
//
// Mirrors performAutoSave() (MeshCraftApplication_FileOps.cpp:40-46):
//   document_.saveToFile(autoSavePath(currentFile_))
// Mc3Document::saveToFile is itself CNA-free and shared with production code
// (same primitive docToXml() above uses), so this exercises real file I/O —
// only autoSavePathAlg is a mirror (already covered path-wise by STAB-0265).
// ─────────────────────────────────────────────────────────────────────────────

static void testAutoSaveWritesSeparateFileNotOriginal()
{
    Mc3Document doc = makeUndoScene();

    static int tmpIdx = 0;
    auto original = std::filesystem::temp_directory_path() /
                    ("mc3_autosave_orig_" + std::to_string(tmpIdx++) + ".mc3.xml");
    doc.saveToFile(original);
    const std::string originalContentBefore = readFile(original);

    const std::string autosavePath = autoSavePathAlg(original.string());
    CHECK(autosavePath == original.string() + ".autosave",
          "auto-save target derived from the original path via autoSavePathAlg");
    CHECK(!std::filesystem::exists(autosavePath),
          "auto-save file does not exist before the first auto-save");

    // Mutate in-memory (as an edit would) then "auto-save" — must land on the
    // .autosave path, leaving the on-disk original file untouched.
    doc.objects.front()->name = "MutatedByEdit";
    doc.saveToFile(autosavePath);

    CHECK(std::filesystem::exists(autosavePath), "auto-save creates the .autosave file");
    CHECK(readFile(original) == originalContentBefore,
          "auto-save leaves the original on-disk file unmodified");

    const std::string autosaveContent = readFile(autosavePath);
    CHECK(autosaveContent != originalContentBefore,
          "auto-save file content differs from the (unmodified) original");
    CHECK(autosaveContent.find("MutatedByEdit") != std::string::npos,
          "auto-save file contains the in-memory edit");

    std::error_code ec;
    std::filesystem::remove(original, ec);
    std::filesystem::remove(autosavePath, ec);
}

// ─────────────────────────────────────────────────────────────────────────────
// Backup rotation (STAB-0267/0268)
//
// Mirrors the "F6: rotate backups before overwriting" block of
// MeshCraftApplication::saveFile() (MeshCraftApplication_FileOps.cpp:130-154):
// each save calls rotateBackupsAlg(path) BEFORE overwriting path with the new
// content, exactly as the real code rotates backups before
// document_.saveToFile(currentFile_).
// ─────────────────────────────────────────────────────────────────────────────

// 2-digit zero-padded tag so no version's marker is a substring of another
// (e.g. "v01" vs "v10"), unlike bare "v1"/"v10".
static std::string verTag(int n)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "model=\"v%02d\"", n);
    return buf;
}

static void testBackupRotationCreatesBackup1And2()
{
    Mc3Document doc = makeUndoScene();
    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_backup_rot_" + std::to_string(tmpIdx++) + ".mc3.xml");
    auto b1 = std::filesystem::path(path.string() + ".backup.1");
    auto b2 = std::filesystem::path(path.string() + ".backup.2");
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(b1, ec);
    std::filesystem::remove(b2, ec);

    // Save 1: path doesn't exist yet — nothing to back up.
    doc.model = "v01"; rotateBackupsAlg(path); doc.saveToFile(path);
    CHECK(!std::filesystem::exists(b1),
          "first save creates no backup (nothing to back up yet)");

    // Save 2: path existed (v01) — becomes backup.1; no backup.2 yet.
    doc.model = "v02"; rotateBackupsAlg(path); doc.saveToFile(path);
    CHECK(std::filesystem::exists(b1), "second save creates backup.1");
    CHECK(!std::filesystem::exists(b2),
          "second save does not create backup.2 yet (only one prior version)");
    CHECK(readFile(b1).find(verTag(1)) != std::string::npos,
          "backup.1 holds the immediately-previous version (v01)");

    // Save 3: backup.1(v01) rotates to backup.2; new backup.1 = v02.
    doc.model = "v03"; rotateBackupsAlg(path); doc.saveToFile(path);
    CHECK(std::filesystem::exists(b2), "third save creates backup.2");
    CHECK(readFile(b1).find(verTag(2)) != std::string::npos,
          "backup.1 now holds v02 (the previous version)");
    CHECK(readFile(b2).find(verTag(1)) != std::string::npos,
          "backup.2 now holds v01 (two versions back)");
    CHECK(readFile(path).find(verTag(3)) != std::string::npos,
          "current file holds the latest version (v03)");

    std::filesystem::remove(path, ec);
    std::filesystem::remove(b1, ec);
    std::filesystem::remove(b2, ec);
}

static void testBackupRotationLimitedToTwoSlots()
{
    Mc3Document doc = makeUndoScene();
    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_backup_limit_" + std::to_string(tmpIdx++) + ".mc3.xml");
    auto b1 = std::filesystem::path(path.string() + ".backup.1");
    auto b2 = std::filesystem::path(path.string() + ".backup.2");
    auto b3 = std::filesystem::path(path.string() + ".backup.3");
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(b1, ec);
    std::filesystem::remove(b2, ec);
    std::filesystem::remove(b3, ec);

    const int kSaves = 10;
    for (int i = 1; i <= kSaves; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "v%02d", i);
        doc.model = buf;
        rotateBackupsAlg(path);
        doc.saveToFile(path);
    }

    CHECK(std::filesystem::exists(b1), "after 10 saves, backup.1 still exists");
    CHECK(std::filesystem::exists(b2), "after 10 saves, backup.2 still exists");
    CHECK(!std::filesystem::exists(b3),
          "after 10 saves, no backup.3 is ever created (fixed 2-slot limit)");
    CHECK(readFile(b1).find(verTag(9)) != std::string::npos,
          "backup.1 holds only the most recent prior version (v09)");
    CHECK(readFile(b2).find(verTag(8)) != std::string::npos,
          "backup.2 holds only the second-most-recent prior version (v08)");
    CHECK(readFile(path).find(verTag(10)) != std::string::npos,
          "current file holds the latest version (v10)");

    std::filesystem::remove(path, ec);
    std::filesystem::remove(b1, ec);
    std::filesystem::remove(b2, ec);
}

// ─────────────────────────────────────────────────────────────────────────────
// AI panel dialog lifecycle (STAB-0299/0300/0301)
// ─────────────────────────────────────────────────────────────────────────────

static void testAiPanelResetClearsPendingAndError()
{
    AiPanelStateAlg st;
    st.aiPendingDocSet    = true;
    st.validationErrorSet = true;
    aiResetAlg(st);
    CHECK(!st.aiPendingDocSet,    "AI Reset clears aiPendingDoc_");
    CHECK(!st.validationErrorSet, "AI Reset clears aiValidationError_");
}

static void testAiPanelApplyDoesNotClearPending()
{
    AiPanelStateAlg st;
    st.aiPendingDocSet = true;
    aiApplyToSceneAlg(st);
    CHECK(st.aiPendingDocSet,
          "AI Apply to Scene leaves aiPendingDoc_ set (Save to Registry stays available)");
}

static void testAiPanelResetRegistryDialogBehavior()
{
    // Registry dialog opened FROM this AI result: Reset must close it.
    AiPanelStateAlg st;
    st.aiPendingDocSet = true;
    st.regSaveFromAi   = true;
    st.regSaveDlgOpen  = true;
    aiResetAlg(st);
    CHECK(!st.regSaveDlgOpen, "AI Reset closes the registry save dialog opened from this AI result");
    CHECK(!st.regSaveFromAi,  "AI Reset clears regSaveFromAi_");

    // Registry dialog opened independently (not from AI): Reset must leave it alone.
    AiPanelStateAlg st2;
    st2.regSaveFromAi = false;
    st2.regSaveDlgOpen = true;
    aiResetAlg(st2);
    CHECK(st2.regSaveDlgOpen, "AI Reset leaves an independently-opened registry dialog alone");
}

// ─────────────────────────────────────────────────────────────────────────────
// No crash on an externally-removed selected object (STAB-0303)
//
// PropertiesPanel.cpp never looks up the selected object in the document
// tree (it only reads/writes fields on the shared_ptr directly, which stays
// valid even if the object was removed from document_.objects elsewhere),
// and the sole delete path (MeshCraftApplication::deleteSelected(), reached
// from SceneHierarchyPanel.cpp's "Delete" menu item) always clears
// selection_ right after removing — so no code path holds a "selected but
// removed" object at once. Verified by inspection.
//
// The remaining risk is command functions that DO look the object up in the
// tree (duplicate/group/ungroup, mirrored below) if a caller ever invoked
// them with a stale selection. Confirm they no-op gracefully rather than
// dereferencing a null parent list.
// ─────────────────────────────────────────────────────────────────────────────

static void testCommandsIgnoreObjectNotInTree()
{
    auto a = makeObj("a", "A");
    auto orphan = makeObj("orphan", "Orphan"); // never added to root
    std::vector<std::shared_ptr<Mc3Object>> root = {a};

    auto dup = duplicateObjectsAlg(root, {orphan});
    CHECK(dup.empty(), "duplicate: no-op for an object not present in the tree");
    CHECK(root.size() == 1, "duplicate: root unchanged");

    auto group = groupObjectsAlg(root, {orphan}, "ShouldNotExist");
    // groupObjectsAlg doesn't search for the object's parent (it only removes
    // it if found), so it still creates a group containing the orphan — but
    // must not crash, and must leave the real root object (`a`) alone.
    CHECK(group != nullptr, "group: does not crash when grouping an untracked object");
    CHECK(root.size() == 2, "group: `a` plus the new group, orphan was never in root to begin with");
    bool aUntouched = std::any_of(root.begin(), root.end(),
        [&](const auto& o){ return o.get() == a.get(); });
    CHECK(aUntouched, "group: the real root object (a) is untouched");

    // A Group (with a child, so it passes the type/empty checks) that was
    // never added to root: findParentListAlg won't find it, so ungroup must
    // no-op rather than dereference a null parent list.
    auto orphanGroup = std::make_shared<Mc3Object>();
    orphanGroup->type = Mc3::ObjectType::Group;
    orphanGroup->name = "OrphanGroup";
    orphanGroup->children.push_back(makeObj("oc", "OrphanChild"));
    auto restored = ungroupObjectAlg(root, orphanGroup);
    CHECK(restored.empty(), "ungroup: no-op for a group not present in the tree");
}

// ─────────────────────────────────────────────────────────────────────────────
// Unsaved-changes confirmation (STAB-0264)
// ─────────────────────────────────────────────────────────────────────────────

static void testConfirmIfModifiedGate()
{
    CHECK(confirmIfModifiedAlg(/*modified=*/false),
          "not modified: pending action runs immediately, no dialog");
    CHECK(!confirmIfModifiedAlg(/*modified=*/true),
          "modified: pending action must be deferred to the unsaved-changes dialog");
}

static void testUnsavedDialogChoices()
{
    CHECK(unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg::Save, /*hasCurrentFile=*/true),
          "Save with a current file: proceeds with the pending action");
    CHECK(!unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg::Save, /*hasCurrentFile=*/false),
          "Save with no current file: refuses (can't save), does not proceed");
    CHECK(unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg::DontSave, true),
          "Don't Save: discards and proceeds regardless of currentFile_ (true case)");
    CHECK(unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg::DontSave, false),
          "Don't Save: discards and proceeds regardless of currentFile_ (false case)");
    CHECK(!unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg::Cancel, true),
          "Cancel: never proceeds, regardless of currentFile_ (true case)");
    CHECK(!unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg::Cancel, false),
          "Cancel: never proceeds, regardless of currentFile_ (false case)");
}

// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    testApplyRenamePattern();
    testBatchRename();
    testFindReplace();
    testArrayDuplicate();
    testDuplicateObjects();
    testGroupAndUngroupObjects();
    testUngroupRejectsNonGroupOrEmptyGroup();
    testDeepCopy();
    testUndoRedoBatchRename();
    testUndoRedoFindReplace();
    testUndoRedoArrayDuplicate();
    testUndoRedoDuplicate();
    testUndoRedoGroup();
    testUndoRedoUngroup();
    testUndoRedoMaterialEdit();
    testSnapshotIndependence();
    testAutoSavePath();
    testAutoSaveIntervalConfigurable();
    testAutoSaveDisabledWhenIntervalIsZero();
    testAutoSaveSkippedWhenNotModifiedOrNoFile();
    testAutoSaveWritesSeparateFileNotOriginal();
    testBackupRotationCreatesBackup1And2();
    testBackupRotationLimitedToTwoSlots();
    testAiPanelResetClearsPendingAndError();
    testAiPanelApplyDoesNotClearPending();
    testAiPanelResetRegistryDialogBehavior();
    testCommandsIgnoreObjectNotInTree();
    testConfirmIfModifiedGate();
    testUnsavedDialogChoices();

    std::cout << "\n";
    if (failures == 0)
        std::cout << "All tests passed.\n";
    else
        std::cout << failures << " test(s) FAILED.\n";

    return failures == 0 ? 0 : 1;
}
