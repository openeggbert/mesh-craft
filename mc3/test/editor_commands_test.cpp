#include <MeshCraft/EditorAlgorithms.hpp>
#include <MeshCraft/Renderer/CsgCacheAlg.hpp>

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include <array>
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

    // STAB-0047: empty pattern → empty result (no fallback to origName)
    CHECK(applyRenamePatternAlg("", "Box", 1, "Box").empty(),
          "empty pattern yields empty result");

    // STAB-0047: a pattern containing a literal backslash passes through
    // unchanged — applyRenamePatternAlg does plain substring replacement,
    // no regex/escape handling, so backslashes aren't special.
    CHECK(applyRenamePatternAlg("back\\{name}", "Box", 1, "Box") == "back\\Box",
          "pattern with literal backslash before a token");
    CHECK(applyRenamePatternAlg("C:\\models\\{name}", "Chair", 1, "Mesh") == "C:\\models\\Chair",
          "pattern with backslashes as literal path separators");
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

    // STAB-0047: empty pattern resolves to an empty name, which
    // batchRenameObjects must skip rather than actually applying (an
    // object's name should never be silently wiped to "").
    auto e = makeObj("e", "Elephant");
    std::vector<std::shared_ptr<Mc3Object>> sel4 = {e};
    int renamedEmpty = batchRenameObjects(sel4, {}, "");
    CHECK(renamedEmpty == 0, "batch rename with empty pattern: 0 objects counted as renamed");
    CHECK(e->name == "Elephant", "batch rename with empty pattern: name unchanged, not wiped to empty");
}

// STAB-0456: renaming an object must not silently orphan animation channels
// that targeted its old name (Mc3Channel::targetObject is a plain name
// string, not a stable id reference).
static void testRenameObjectInActionsAlg()
{
    Mc3::Mc3Action action;
    action.name = "Spin";
    Mc3::Mc3Channel ch;
    ch.targetObject = "OldName";
    ch.property     = Mc3::AnimatedProperty::RotationY;
    ch.keyframes    = {Mc3::Mc3Keyframe::linear(0.0f, 0.0f), Mc3::Mc3Keyframe::linear(1.0f, 360.0f)};
    action.channels.push_back(ch);
    std::map<std::string, Mc3::Mc3Action> actions = {{"Spin", action}};

    renameObjectInActionsAlg(actions, "OldName", "NewName");
    CHECK(actions.at("Spin").channels[0].targetObject == "NewName",
          "rename in actions: channel.targetObject updated to new name");

    // No-op cases: empty old name, or old==new.
    renameObjectInActionsAlg(actions, "", "Whatever");
    CHECK(actions.at("Spin").channels[0].targetObject == "NewName",
          "rename in actions: empty oldName is a no-op");
    renameObjectInActionsAlg(actions, "NewName", "NewName");
    CHECK(actions.at("Spin").channels[0].targetObject == "NewName",
          "rename in actions: oldName==newName is a no-op");

    // A channel targeting an unrelated object must be untouched.
    Mc3::Mc3Channel other;
    other.targetObject = "Unrelated";
    other.property     = Mc3::AnimatedProperty::PositionX;
    actions["Spin"].channels.push_back(other);
    renameObjectInActionsAlg(actions, "NewName", "NewerName");
    CHECK(actions.at("Spin").channels[0].targetObject == "NewerName",
          "rename in actions: matching channel renamed again");
    CHECK(actions.at("Spin").channels[1].targetObject == "Unrelated",
          "rename in actions: unrelated channel untouched");

    // batchRenameObjects with the optional actions pointer applies the fixup.
    auto obj = makeObj("r1", "OldName");
    std::vector<std::shared_ptr<Mc3Object>> sel = {obj};
    std::map<std::string, Mc3::Mc3Action> actions2 = {{"Spin", action}};
    batchRenameObjects(sel, {}, "Renamed_{name}", &actions2);
    CHECK(obj->name == "Renamed_OldName", "batch rename with actions: object renamed");
    CHECK(actions2.at("Spin").channels[0].targetObject == "Renamed_OldName",
          "batch rename with actions: channel.targetObject followed the rename");
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

    // STAB-0494: a negative count (only reachable via a hand-edited/hostile
    // .mc3macro linear_array step — the real ImGui slider is clamped to
    // [2,20]) does not crash. It is clamped to the same minimum-of-2 floor
    // as count=0/1 above (1 copy created), not silently dropped to zero —
    // that clamp-to-minimum, not a hard reject, is this function's actual,
    // intentional "graceful" behavior for any too-small count.
    {
        auto src = makeObj("z", "Z");
        std::vector<std::shared_ptr<Mc3Object>> root = {src};
        auto created = arrayDuplicateObjects(root, {src}, -1, 0, 1.0f, true);
        CHECK(created.size() == 1, "array dup count=-1: no crash, clamped to 2 → 1 copy");
        CHECK(root.size() == 2, "array dup count=-1: root grows by exactly 1, no runaway/negative-loop");
    }
    {
        // A very negative count must not somehow be (mis)interpreted as a
        // huge unsigned loop count (would hang/OOM) — still just 1 copy.
        auto src = makeObj("w", "W");
        std::vector<std::shared_ptr<Mc3Object>> root = {src};
        auto created = arrayDuplicateObjects(root, {src}, -1000000, 0, 1.0f, true);
        CHECK(created.size() == 1, "array dup count=-1000000: no crash/hang, clamped to 2 → 1 copy");
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
// convertToDefinitionAlg (STAB-0482)
// ─────────────────────────────────────────────────────────────────────────────

static void testConvertToDefinition()
{
    Mc3Document doc;
    auto src = makeObj("box1", "MyBox", Mc3::ObjectType::Sphere);
    src->transform.position = {5.0f, 1.0f, -2.0f};
    src->transform.rotation = {0.0f, 45.0f, 0.0f};
    src->transform.scale    = {2.0f, 2.0f, 2.0f};
    src->visible = false;
    src->tags    = {"prop", "reusable"};
    auto child = makeObj("box1_child", "Child");
    src->children.push_back(child);
    doc.objects.push_back(src);

    auto inst = convertToDefinitionAlg(doc, src);

    // A new definition was created, keyed "def_1" (first free key).
    CHECK(doc.definitions.count("def_1") == 1, "convert: definition 'def_1' created");
    if (doc.definitions.count("def_1")) {
        const auto& def = doc.definitions.at("def_1");
        CHECK(def->id == "def_1" && def->name == "def_1", "convert: definition id/name set to the key");
        CHECK(def->type == Mc3::ObjectType::Sphere, "convert: definition keeps the original object type");
        CHECKF(def->transform.position[0], 0.0f, "convert: definition transform reset (position.x)");
        CHECKF(def->transform.rotation[1], 0.0f, "convert: definition transform reset (rotation.y)");
        CHECKF(def->transform.scale[0],    1.0f, "convert: definition transform reset (scale.x)");
        CHECK(def->children.size() == 1 && def->children[0]->name == "Child",
              "convert: definition is a deep copy including children");
        CHECK(def.get() != src.get(), "convert: definition is a distinct object, not the same pointer as src");
    }

    // src was replaced in-place by an Instance preserving its transform/visibility/tags.
    CHECK(doc.objects.size() == 1, "convert: object count unchanged (replaced in place, not appended)");
    CHECK(inst != nullptr && doc.objects[0].get() == inst.get(),
          "convert: root list's entry is now the returned Instance");
    CHECK(inst->type == Mc3::ObjectType::Instance, "convert: replacement is an Instance");
    CHECK(inst->definition == "def_1", "convert: Instance references the new definition key");
    CHECK(inst->id == "box1", "convert: Instance keeps src's original id");
    CHECK(inst->name == "MyBox", "convert: Instance keeps src's original name");
    CHECKF(inst->transform.position[0], 5.0f, "convert: Instance keeps src's original position.x");
    CHECKF(inst->transform.rotation[1], 45.0f, "convert: Instance keeps src's original rotation.y");
    CHECKF(inst->transform.scale[0],    2.0f, "convert: Instance keeps src's original scale.x");
    CHECK(inst->visible == false, "convert: Instance keeps src's original visibility");
    CHECK(inst->tags.size() == 2 && inst->tags[0] == "prop" && inst->tags[1] == "reusable",
          "convert: Instance keeps src's original tags");

    // A second conversion (of the new Instance's sibling, here just re-run on
    // inst itself) must not collide with "def_1" — proves unique-key search works.
    auto inst2 = convertToDefinitionAlg(doc, inst);
    CHECK(doc.definitions.count("def_2") == 1,
          "convert: second conversion gets a distinct key 'def_2', no collision with 'def_1'");
    CHECK(inst2->definition == "def_2", "convert: second Instance references 'def_2'");
}

// ─────────────────────────────────────────────────────────────────────────────
// breakInstanceAlg (STAB-0483)
// ─────────────────────────────────────────────────────────────────────────────

static void testBreakInstance()
{
    Mc3Document doc;

    // A definition with children (mirrors a typical "Car" prop: a body + 2 wheels).
    auto defObj = makeObj("car_def", "car_def", Mc3::ObjectType::Group);
    auto body   = makeObj("body", "Body");
    auto wheel  = makeObj("wheel", "Wheel");
    defObj->children = {body, wheel};
    doc.definitions["car_def"] = defObj;

    auto inst1 = makeObj("inst1", "Car1", Mc3::ObjectType::Instance);
    inst1->definition = "car_def";
    inst1->transform.position = {1.0f, 0.0f, 0.0f};
    inst1->visible = false;
    inst1->tags    = {"vehicle"};
    doc.objects.push_back(inst1);

    auto copy1 = breakInstanceAlg(doc, inst1);
    CHECK(copy1 != nullptr, "break: returns the new object");
    if (copy1) {
        CHECK(copy1->type == Mc3::ObjectType::Group, "break: copy keeps the definition's own type");
        CHECK(copy1->name == "Car1", "break: copy keeps the instance's name");
        CHECKF(copy1->transform.position[0], 1.0f, "break: copy keeps the instance's transform");
        CHECK(copy1->visible == false, "break: copy keeps the instance's visibility");
        CHECK(copy1->tags.size() == 1 && copy1->tags[0] == "vehicle", "break: copy keeps the instance's tags");
        CHECK(copy1->children.size() == 2, "break: copy has both children from the definition");
    }
    CHECK(doc.objects.size() == 1 && doc.objects[0].get() == copy1.get(),
          "break: root list's entry is now the returned copy (replaced in place)");

    // STAB-0483's actual bug: break a SECOND Instance of the SAME definition.
    // Before the fix, both copies' children kept the definition template's
    // original ids verbatim ("body"/"wheel") — an exact duplicate.
    auto inst2 = makeObj("inst2", "Car2", Mc3::ObjectType::Instance);
    inst2->definition = "car_def";
    doc.objects.push_back(inst2);
    auto copy2 = breakInstanceAlg(doc, inst2);
    CHECK(copy2 != nullptr, "break (2nd): returns the new object");
    if (copy1 && copy2) {
        CHECK(copy1->id != copy2->id, "break (2nd): top-level copies have distinct ids");
        CHECK(copy1->children[0]->id != copy2->children[0]->id,
              "break (2nd): first children ('Body') have distinct ids, not both 'body'");
        CHECK(copy1->children[1]->id != copy2->children[1]->id,
              "break (2nd): second children ('Wheel') have distinct ids, not both 'wheel'");
        // Every id in both subtrees must be unique across the whole document.
        std::set<std::string> allIds;
        std::function<void(const std::shared_ptr<Mc3Object>&)> collect =
            [&](const std::shared_ptr<Mc3Object>& o) {
                CHECK(allIds.insert(o->id).second,
                      "break (2nd): id '" + o->id + "' is unique across the whole document");
                for (auto& c : o->children) collect(c);
            };
        for (auto& o : doc.objects) collect(o);
    }

    // Rejections: non-Instance, and Instance with an unknown definition.
    auto notInst = makeObj("plain", "Plain", Mc3::ObjectType::Box);
    CHECK(breakInstanceAlg(doc, notInst) == nullptr, "break: rejects a non-Instance object");
    auto badInst = makeObj("bad", "Bad", Mc3::ObjectType::Instance);
    badInst->definition = "does_not_exist";
    CHECK(breakInstanceAlg(doc, badInst) == nullptr, "break: rejects an Instance with an unknown definition");
}

// ─────────────────────────────────────────────────────────────────────────────
// alignToObjectAlg (STAB-0484)
// ─────────────────────────────────────────────────────────────────────────────

static void testAlignToObject()
{
    auto target = makeObj("t", "Target");
    target->transform.position = {5.0f, 2.0f, -3.0f};
    auto a = makeObj("a", "A");
    a->transform.position = {0.0f, 0.0f, 0.0f};
    a->transform.rotation = {10.0f, 20.0f, 30.0f}; // must survive untouched
    auto b = makeObj("b", "B");
    b->transform.position = {1.0f, 1.0f, 1.0f};

    std::vector<std::shared_ptr<Mc3Object>> sel = {target, a, b};
    int aligned = alignToObjectAlg(sel, {});
    CHECK(aligned == 2, "align: both non-target objects aligned");
    CHECKF(a->transform.position[0], 5.0f, "align: A.position.x matches target");
    CHECKF(a->transform.position[1], 2.0f, "align: A.position.y matches target");
    CHECKF(a->transform.position[2], -3.0f, "align: A.position.z matches target");
    CHECKF(a->transform.rotation[0], 10.0f, "align: A.rotation untouched");
    CHECKF(b->transform.position[0], 5.0f, "align: B.position.x matches target");
    CHECKF(target->transform.position[0], 5.0f, "align: target itself unchanged");

    // Locked objects are skipped.
    auto c = makeObj("c", "C");
    c->transform.position = {9.0f, 9.0f, 9.0f};
    std::vector<std::shared_ptr<Mc3Object>> sel2 = {target, c};
    int aligned2 = alignToObjectAlg(sel2, {"c"});
    CHECK(aligned2 == 0, "align: locked object not counted as aligned");
    CHECKF(c->transform.position[0], 9.0f, "align: locked object's position unchanged");

    // Fewer than 2 selected objects: no-op.
    std::vector<std::shared_ptr<Mc3Object>> selOne = {target};
    CHECK(alignToObjectAlg(selOne, {}) == 0, "align: fewer than 2 selected is a no-op");
    std::vector<std::shared_ptr<Mc3Object>> selEmpty = {};
    CHECK(alignToObjectAlg(selEmpty, {}) == 0, "align: empty selection is a no-op");
}

// ─────────────────────────────────────────────────────────────────────────────
// scatterAlongCurveAlg / scatterCurvePositionAlg (STAB-0485)
// ─────────────────────────────────────────────────────────────────────────────

static void testScatterAlongCurve()
{
    auto zeroRng = []() -> float { return 0.0f; };

    // Line mode: count=10 -> exactly 9 NEW objects (the UI's own "(N-1 new)"
    // label documents this as intended; the original stays untouched).
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto src = makeObj("s", "Src");
        src->transform.position = {1.0f, 2.0f, 3.0f};
        roots.push_back(src);

        ScatterCurveParamsAlg params;
        params.count   = 10;
        params.mode    = 0; // line
        params.axis    = 1; // Y
        params.spacing = 2.0f;

        auto added = scatterAlongCurveAlg(roots, {src}, params, zeroRng);
        CHECK(added.size() == 9, "scatter line: count=10 selected=1 -> 9 new objects");
        CHECK(roots.size() == 10, "scatter line: original root list grows by 9 (10 total)");
        CHECKF(src->transform.position[1], 2.0f, "scatter line: original object itself untouched");

        CHECKF(added[0]->transform.position[0], 1.0f, "scatter line: copy1.x unchanged (axis=Y)");
        CHECKF(added[0]->transform.position[1], 4.0f, "scatter line: copy1.y = base + 1*spacing");
        CHECKF(added[0]->transform.position[2], 3.0f, "scatter line: copy1.z unchanged (axis=Y)");
        CHECKF(added[8]->transform.position[1], 20.0f, "scatter line: copy9.y = base + 9*spacing");
        CHECK(added[0]->name == "Src_sc1", "scatter line: copy naming convention");
        CHECK(added[0]->id   == "s_sc1",   "scatter line: copy id convention (must stay unique)");
    }

    // Multiple selected sources: each independently gets count-1 new copies,
    // inserted into its own parent list right after itself.
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto a = makeObj("a", "A"); a->transform.position = {0,0,0};
        auto b = makeObj("b", "B"); b->transform.position = {10,0,0};
        roots.push_back(a);
        roots.push_back(b);

        ScatterCurveParamsAlg params;
        params.count   = 3;
        params.mode    = 0;
        params.axis    = 0;
        params.spacing = 1.0f;

        auto added = scatterAlongCurveAlg(roots, {a, b}, params, zeroRng);
        CHECK(added.size() == 4, "scatter multi-source: 2 sources * (count-1)=2 each -> 4 new");
        CHECK(roots.size() == 6, "scatter multi-source: root list grows by 4 (6 total)");
        // a, a_sc1, a_sc2, b, b_sc1, b_sc2 in order.
        CHECK(roots[0]->id == "a" && roots[1]->id == "a_sc1" && roots[2]->id == "a_sc2",
              "scatter multi-source: A's copies inserted right after A");
        CHECK(roots[3]->id == "b" && roots[4]->id == "b_sc1" && roots[5]->id == "b_sc2",
              "scatter multi-source: B's copies inserted right after B");
    }

    // Arc mode: copy at i=0 conceptually sits at the source (angle=0 -> dx=dz=0).
    {
        auto src = makeObj("s", "Src");
        src->transform.position = {0.0f, 0.0f, 0.0f};

        ScatterCurveParamsAlg params;
        params.count       = 4;
        params.mode        = 1; // arc
        params.axis        = 1; // Y-up: arc in XZ plane
        params.arcAngleDeg = 180.0f;
        params.radius      = 5.0f;

        auto p1 = scatterCurvePositionAlg(*src, 1, params);
        auto p3 = scatterCurvePositionAlg(*src, 3, params); // i=count-1 -> t=1 -> angle=180deg
        CHECKF(p3[0], -10.0f, "scatter arc: last point x = radius*cos(180)-radius = -2*radius");
        CHECK(std::abs(p3[2]) < 1e-3f, "scatter arc: last point z ~= 0 at angle=180deg");
        CHECK(std::abs(p1[0]) > 0.01f || std::abs(p1[2]) > 0.01f,
              "scatter arc: intermediate point is actually displaced off the source");
    }

    // Jitter is applied per-axis via the injected RNG.
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto src = makeObj("s", "Src");
        roots.push_back(src);

        ScatterCurveParamsAlg params;
        params.count  = 2;
        params.mode   = 0;
        params.axis   = 0;
        params.jitter = 10.0f;

        auto onesRng = []() -> float { return 1.0f; };
        auto added = scatterAlongCurveAlg(roots, {src}, params, onesRng);
        CHECK(added.size() == 1, "scatter jitter: count=2 -> 1 new copy");
        CHECKF(added[0]->transform.position[1], 10.0f, "scatter jitter: jitterRng()*jitter added to Y");
        CHECKF(added[0]->transform.position[2], 10.0f, "scatter jitter: jitterRng()*jitter added to Z");
    }

    // No-ops.
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto src = makeObj("s", "Src");
        roots.push_back(src);
        ScatterCurveParamsAlg params;
        params.count = 1; // < 2
        CHECK(scatterAlongCurveAlg(roots, {src}, params, zeroRng).empty(),
              "scatter: count < 2 is a no-op");
        params.count = 5;
        CHECK(scatterAlongCurveAlg(roots, {}, params, zeroRng).empty(),
              "scatter: empty selection is a no-op");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// applyProportionalFalloffAlg / proportionalFalloffWeightAlg (STAB-0489)
// ─────────────────────────────────────────────────────────────────────────────

static void testProportionalFalloff()
{
    // Weight function itself: 1.0 at zero distance, 0 at/beyond the radius,
    // strictly decreasing in between.
    {
        CHECKF(proportionalFalloffWeightAlg(0.0f, 10.0f), 1.0f,
               "falloff weight: distance 0 -> weight 1.0");
        CHECK(proportionalFalloffWeightAlg(100.0f, 10.0f) == 0.0f,
              "falloff weight: distance == radius^2 -> weight 0 (edge excluded)");
        CHECK(proportionalFalloffWeightAlg(400.0f, 10.0f) == 0.0f,
              "falloff weight: distance beyond radius -> weight 0");
        float wNear = proportionalFalloffWeightAlg(4.0f, 10.0f);
        float wFar  = proportionalFalloffWeightAlg(64.0f, 10.0f);
        CHECK(wNear > 0.0f && wFar > 0.0f && wNear > wFar,
              "falloff weight: strictly decreasing with distance within radius");
        CHECK(proportionalFalloffWeightAlg(1.0f, 0.0f) == 0.0f,
              "falloff weight: zero radius -> always 0");
    }

    // A selected object at the origin, one unselected neighbor well within
    // radius, one unselected neighbor outside it, one locked neighbor inside
    // radius (must be skipped despite being unselected).
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto sel = makeObj("sel", "Selected");
        sel->transform.position = {0, 0, 0};
        auto near = makeObj("near", "Near");
        near->transform.position = {1, 0, 0};
        auto far = makeObj("far", "Far");
        far->transform.position = {100, 0, 0};
        auto locked = makeObj("locked", "Locked");
        locked->transform.position = {0, 1, 0};
        roots.push_back(sel);
        roots.push_back(near);
        roots.push_back(far);
        roots.push_back(locked);

        std::set<std::string> lockedIds = {"locked"};
        int affected = applyProportionalFalloffAlg(roots, {sel}, lockedIds,
                                                     /*deltaX=*/10.0f, 0.0f, 0.0f,
                                                     /*radius=*/5.0f);
        CHECK(affected == 1, "falloff apply: only the near, unlocked neighbor is affected");
        CHECK(near->transform.position[0] > 0.0f,
              "falloff apply: near neighbor nudged toward the delta direction");
        CHECKF(far->transform.position[0], 100.0f,
               "falloff apply: out-of-radius neighbor is untouched");
        CHECKF(locked->transform.position[1], 1.0f,
               "falloff apply: locked neighbor is untouched even though in-radius");
        CHECKF(sel->transform.position[0], 0.0f,
               "falloff apply: the selected object itself is untouched (moved separately)");
    }

    // Multi-object selection: center is the average position.
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto a = makeObj("a", "A"); a->transform.position = {-2, 0, 0};
        auto b = makeObj("b", "B"); b->transform.position = {2, 0, 0};
        auto mid = makeObj("mid", "Mid"); mid->transform.position = {0, 0, 0};
        roots.push_back(a);
        roots.push_back(b);
        roots.push_back(mid);

        int affected = applyProportionalFalloffAlg(roots, {a, b}, {},
                                                     5.0f, 0.0f, 0.0f, 3.0f);
        CHECK(affected == 1, "falloff apply: selection center is the average of selected positions");
        CHECK(mid->transform.position[0] > 0.0f,
              "falloff apply: object at the averaged selection center is affected");
    }

    // No-ops: empty selection, radius <= 0.
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto sel = makeObj("sel", "Sel");
        auto other = makeObj("other", "Other");
        roots.push_back(sel);
        roots.push_back(other);
        CHECK(applyProportionalFalloffAlg(roots, {}, {}, 1, 0, 0, 5.0f) == 0,
              "falloff apply: empty selection is a no-op");
        CHECK(applyProportionalFalloffAlg(roots, {sel}, {}, 1, 0, 0, 0.0f) == 0,
              "falloff apply: zero radius is a no-op");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// vertexSnapToNearestAlg (STAB-0490)
// ─────────────────────────────────────────────────────────────────────────────

static void testVertexSnap()
{
    // A snap target within threshold: selection snaps by the exact delta to
    // that target's position; a second selected object moves by the same
    // delta (rigid offset, not each independently re-centered).
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto sel0 = makeObj("s0", "Sel0"); sel0->transform.position = {0, 0, 0};
        auto sel1 = makeObj("s1", "Sel1"); sel1->transform.position = {5, 0, 0};
        auto target = makeObj("t", "Target"); target->transform.position = {1, 2, 0};
        roots.push_back(sel0);
        roots.push_back(sel1);
        roots.push_back(target);

        bool snapped = vertexSnapToNearestAlg(roots, {sel0, sel1}, {},
                                               /*ref=*/0.0f, 0.0f, 0.0f,
                                               /*threshold=*/10.0f);
        CHECK(snapped, "vertex snap: target within threshold is found and applied");
        CHECKF(sel0->transform.position[0], 1.0f, "vertex snap: anchor snaps exactly onto target.x");
        CHECKF(sel0->transform.position[1], 2.0f, "vertex snap: anchor snaps exactly onto target.y");
        CHECKF(sel1->transform.position[0], 6.0f, "vertex snap: other selected object moves by the same offset (x)");
        CHECKF(sel1->transform.position[1], 2.0f, "vertex snap: other selected object moves by the same offset (y)");
    }

    // No target within threshold: nothing moves, returns false.
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto sel0 = makeObj("s0", "Sel0"); sel0->transform.position = {0, 0, 0};
        auto far = makeObj("far", "Far"); far->transform.position = {100, 0, 0};
        roots.push_back(sel0);
        roots.push_back(far);

        bool snapped = vertexSnapToNearestAlg(roots, {sel0}, {}, 0, 0, 0, /*threshold=*/1.0f);
        CHECK(!snapped, "vertex snap: no target within threshold returns false");
        CHECKF(sel0->transform.position[0], 0.0f, "vertex snap: no-op leaves position untouched");
    }

    // The nearest of several candidates wins, and the selected object itself
    // is never its own snap target.
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto sel0 = makeObj("s0", "Sel0"); sel0->transform.position = {0, 0, 0};
        auto near = makeObj("near", "Near"); near->transform.position = {2, 0, 0};
        auto nearer = makeObj("nearer", "Nearer"); nearer->transform.position = {1, 0, 0};
        roots.push_back(sel0);
        roots.push_back(near);
        roots.push_back(nearer);

        vertexSnapToNearestAlg(roots, {sel0}, {}, 0, 0, 0, 10.0f);
        CHECKF(sel0->transform.position[0], 1.0f, "vertex snap: snaps to the nearest candidate, not just the first");
    }

    // Locked objects don't move even if selected; empty selection is a no-op.
    {
        std::vector<std::shared_ptr<Mc3Object>> roots;
        auto sel0 = makeObj("s0", "Sel0"); sel0->transform.position = {0, 0, 0};
        auto target = makeObj("t", "Target"); target->transform.position = {3, 0, 0};
        roots.push_back(sel0);
        roots.push_back(target);

        bool snapped = vertexSnapToNearestAlg(roots, {sel0}, {"s0"}, 0, 0, 0, 10.0f);
        CHECK(snapped, "vertex snap: target still found even if the only selected object is locked");
        CHECKF(sel0->transform.position[0], 0.0f, "vertex snap: locked selected object is not moved");

        CHECK(!vertexSnapToNearestAlg(roots, {}, {}, 0, 0, 0, 10.0f),
              "vertex snap: empty selection is a no-op");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// applyRotationDragAlg (STAB-0491)
// ─────────────────────────────────────────────────────────────────────────────

static void testRotationDragSnap()
{
    // Basic accumulate on the given axis, no snap: delta is added as-is.
    {
        auto a = makeObj("a", "A"); a->transform.rotation = {0, 10, 0};
        auto b = makeObj("b", "B"); b->transform.rotation = {0, 20, 0};
        int rotated = applyRotationDragAlg({a, b}, {}, /*axIdx=*/1, /*delta=*/5.0f,
                                            /*shouldSnap=*/false, 45.0f);
        CHECK(rotated == 2, "rotate drag: both unlocked objects rotated");
        CHECKF(a->transform.rotation[1], 15.0f, "rotate drag: A's Y rotation accumulates delta");
        CHECKF(b->transform.rotation[1], 25.0f, "rotate drag: B's Y rotation accumulates delta");
    }

    // Only the targeted axis is touched.
    {
        auto a = makeObj("a", "A"); a->transform.rotation = {1, 2, 3};
        applyRotationDragAlg({a}, {}, /*axIdx=*/0, /*delta=*/10.0f, false, 45.0f);
        CHECKF(a->transform.rotation[0], 11.0f, "rotate drag: targeted axis (X) modified");
        CHECKF(a->transform.rotation[1], 2.0f,  "rotate drag: untargeted axis (Y) untouched");
        CHECKF(a->transform.rotation[2], 3.0f,  "rotate drag: untargeted axis (Z) untouched");
    }

    // Locked object is skipped and not counted.
    {
        auto a = makeObj("a", "A"); a->transform.rotation = {0, 10, 0};
        auto locked = makeObj("l", "L"); locked->transform.rotation = {0, 10, 0};
        int rotated = applyRotationDragAlg({a, locked}, {"l"}, 1, 5.0f, false, 45.0f);
        CHECK(rotated == 1, "rotate drag: locked object not counted as rotated");
        CHECKF(locked->transform.rotation[1], 10.0f, "rotate drag: locked object's rotation untouched");
        CHECKF(a->transform.rotation[1], 15.0f, "rotate drag: unlocked object still rotated");
    }

    // Snap rounds the post-delta value to the nearest increment (45 deg here,
    // matching the STAB-0491 row's example — snapRotate_ itself defaults to
    // 15 deg and is user-configurable; 45 is one of its quick-select presets).
    {
        auto a = makeObj("a", "A"); a->transform.rotation = {0, 10, 0};
        applyRotationDragAlg({a}, {}, 1, /*delta=*/28.0f, /*shouldSnap=*/true, /*snapIncrement=*/45.0f);
        // 10 + 28 = 38 -> round(38/45)*45 = round(0.844)*45 = 45
        CHECKF(a->transform.rotation[1], 45.0f, "rotate drag: snap rounds to nearest 45 deg increment");
    }
    {
        auto a = makeObj("a", "A"); a->transform.rotation = {0, 0, 0};
        applyRotationDragAlg({a}, {}, 1, /*delta=*/20.0f, /*shouldSnap=*/true, /*snapIncrement=*/45.0f);
        // 0 + 20 = 20 -> round(20/45)*45 = round(0.444)*45 = 0
        CHECKF(a->transform.rotation[1], 0.0f, "rotate drag: snap rounds down when closer to the lower increment");
    }

    // No selection is a no-op.
    {
        CHECK(applyRotationDragAlg({}, {}, 1, 10.0f, true, 45.0f) == 0,
              "rotate drag: empty selection is a no-op");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// flattenDescendantsAlg (STAB-0492)
// ─────────────────────────────────────────────────────────────────────────────

static void testFlattenDescendants()
{
    // 2 levels of children below a parent: grandchildren must be included,
    // not just direct children.
    auto grandchild1 = makeObj("gc1", "GC1");
    auto grandchild2 = makeObj("gc2", "GC2");
    auto child1 = makeObj("c1", "C1");
    child1->children = {grandchild1, grandchild2};
    auto child2 = makeObj("c2", "C2");
    auto greatGrandchild = makeObj("ggc", "GGC");
    grandchild1->children = {greatGrandchild};

    auto flat = flattenDescendantsAlg({child1, child2});

    CHECK(flat.size() == 5, "flatten descendants: all 5 descendants across 3 levels are included");
    std::set<std::string> ids;
    for (const auto& o : flat) ids.insert(o->id);
    CHECK(ids.count("c1") && ids.count("c2") && ids.count("gc1") &&
          ids.count("gc2") && ids.count("ggc"),
          "flatten descendants: every descendant id is present, at every depth");

    // Pre-order: a parent appears before its own children.
    auto indexOf = [&](const std::string& id) -> int {
        for (size_t i = 0; i < flat.size(); ++i) if (flat[i]->id == id) return static_cast<int>(i);
        return -1;
    };
    CHECK(indexOf("c1") < indexOf("gc1"), "flatten descendants: pre-order — parent before child");
    CHECK(indexOf("gc1") < indexOf("ggc"), "flatten descendants: pre-order — grandparent before great-grandchild");

    // A leaf with no children flattens to an empty list.
    auto leaf = makeObj("leaf", "Leaf");
    CHECK(flattenDescendantsAlg(leaf->children).empty(),
          "flatten descendants: object with no children -> empty list");
}

// ─────────────────────────────────────────────────────────────────────────────
// groupScaleAlg (STAB-0495)
// ─────────────────────────────────────────────────────────────────────────────

static void testGroupScale()
{
    // The row's own example: 2 objects at (0,0,0) and (2,0,0), scale 2x ->
    // (-1,0,0) and (3,0,0) (centroid 1,0,0, each pushed twice as far away).
    {
        auto a = makeObj("a", "A"); a->transform.position = {0, 0, 0};
        auto b = makeObj("b", "B"); b->transform.position = {2, 0, 0};
        int scaled = groupScaleAlg({a, b}, {}, 2.0f);
        CHECK(scaled == 2, "group scale: both unlocked objects scaled");
        CHECKF(a->transform.position[0], -1.0f, "group scale: A moves to -1 (row's example)");
        CHECKF(b->transform.position[0], 3.0f,  "group scale: B moves to 3 (row's example)");
        CHECKF(a->transform.scale[0], 2.0f, "group scale: A's own scale.x doubled");
        CHECKF(b->transform.scale[0], 2.0f, "group scale: B's own scale.x doubled");
    }

    // factor < 1 shrinks toward the centroid.
    {
        auto a = makeObj("a", "A"); a->transform.position = {0, 0, 0};
        auto b = makeObj("b", "B"); b->transform.position = {4, 0, 0};
        groupScaleAlg({a, b}, {}, 0.5f);
        // centroid = 2; a: 2 + (0-2)*0.5 = 1; b: 2 + (4-2)*0.5 = 3
        CHECKF(a->transform.position[0], 1.0f, "group scale: factor<1 shrinks toward centroid (A)");
        CHECKF(b->transform.position[0], 3.0f, "group scale: factor<1 shrinks toward centroid (B)");
    }

    // Locked object is skipped (position and scale both untouched) and not
    // counted, but still contributes to the centroid computation.
    {
        auto a = makeObj("a", "A"); a->transform.position = {0, 0, 0};
        auto locked = makeObj("l", "L"); locked->transform.position = {2, 0, 0};
        locked->transform.scale = {1, 1, 1};
        int scaled = groupScaleAlg({a, locked}, {"l"}, 2.0f);
        CHECK(scaled == 1, "group scale: locked object not counted as scaled");
        CHECKF(locked->transform.position[0], 2.0f, "group scale: locked object's position untouched");
        CHECKF(locked->transform.scale[0], 1.0f, "group scale: locked object's scale untouched");
        // centroid is still (0+2)/2=1, so A moves to 1+(0-1)*2 = -1 (same as
        // if L weren't locked — confirms L still contributes to the centroid).
        CHECKF(a->transform.position[0], -1.0f, "group scale: locked object still contributes to centroid");
    }

    // Empty selection is a no-op.
    CHECK(groupScaleAlg({}, {}, 2.0f) == 0, "group scale: empty selection is a no-op");
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
// "Save to Registry" from the AI panel (STAB-0347/0348)
// ─────────────────────────────────────────────────────────────────────────────

static void testAiSaveToRegistryUsesAiPendingDoc()
{
    AiPanelStateAlg st;
    st.aiPendingDocSet = true;
    aiSaveToRegistryClickAlg(st, "goblinHut");

    CHECK(st.regSaveDefId == "goblinHut",
          "AI Save to Registry: records the AI result's definition id to pre-fill");
    CHECK(st.regSaveFromAi,
          "AI Save to Registry: marks the pending save as AI-sourced");
    CHECK(registrySaveUsesAiDefinitionsAlg(st),
          "STAB-0347: after Save to Registry from AI, the save dialog sources definitions "
          "from aiPendingDoc_, not document_");

    // An independently-opened save (never went through the AI button) must
    // list the scene document's definitions instead.
    AiPanelStateAlg independent;
    CHECK(!registrySaveUsesAiDefinitionsAlg(independent),
          "an independently-opened registry save dialog uses document_'s definitions, not AI's");
}

static void testAiSaveToRegistryWorksWithoutApply()
{
    // The real "Save to Registry" button's visibility gate and click handler
    // both reference only aiPendingDoc_ (MeshCraftApplication_UiAi.cpp:329) —
    // never a "was Apply clicked" flag, because no such flag exists. Model
    // that directly: reach a successful Save-to-Registry state transition
    // WITHOUT ever calling aiApplyToSceneAlg() first.
    AiPanelStateAlg st;
    st.aiPendingDocSet = true; // AI response validated — this alone is the gate
    aiSaveToRegistryClickAlg(st, "def1"); // no aiApplyToSceneAlg(st) call before this
    CHECK(st.regSaveFromAi,
          "STAB-0348: Save to Registry succeeds without a prior Apply to Scene");
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
// AI apply is undoable (STAB-0270/0278)
//
// MeshCraftApplication::drawAiPanel()'s "Apply to Scene" button
// (MeshCraftApplication_UiAi.cpp:317-324) does `pushUndo(); document_ =
// *aiPendingDoc_;` — a plain full-document replacement, no new Alg needed.
// checkUndoRedo confirms the generic snapshot/undo mechanism (STAB-0279)
// round-trips a full-document replacement exactly like it does the smaller
// per-field mutations (batchRename, materialEdit, ...).
// ─────────────────────────────────────────────────────────────────────────────

static void testUndoRedoAiApply()
{
    Mc3Document doc = makeUndoScene();
    Mc3Document aiResult;
    aiResult.model = "AiGenerated";
    aiResult.objects = { makeObj("ai1", "AiBox") };

    checkUndoRedo("aiApply", doc, [&](Mc3Document& d) {
        d = aiResult;
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Merge scene (STAB-0271)
// ─────────────────────────────────────────────────────────────────────────────

static void testUndoRedoMergeScene()
{
    Mc3Document doc = makeUndoScene();
    Mc3Document src;
    src.objects = { makeObj("merged", "MergedBox") };

    checkUndoRedo("mergeScene", doc, [&](Mc3Document& d) {
        mergeDocumentsAlg(d, src);
    });
}

static void testMergeSceneCollisionHandling()
{
    Mc3Document dst = makeUndoScene();
    dst.textures["wood"]  = Mc3::Mc3Texture("wood", "wood_old.png");
    dst.materials["stone"] = Mc3::Mc3Material("stone", {0.5f, 0.5f, 0.5f, 1.0f});
    dst.actions["Walk"] = Mc3::Mc3Action::make("Walk", 1.0f);

    Mc3Document src;
    src.textures["wood"]  = Mc3::Mc3Texture("wood", "wood_new.png"); // collides
    src.textures["metal"] = Mc3::Mc3Texture("metal", "metal.png");   // new
    src.materials["stone"] = Mc3::Mc3Material("stone", {0.1f, 0.1f, 0.1f, 1.0f}); // collides
    src.actions["Walk"] = Mc3::Mc3Action::make("Walk", 2.0f); // collides
    src.actions["Jump"] = Mc3::Mc3Action::make("Jump", 0.5f); // new
    src.objects = { makeObj("s1", "SrcBoxA"), makeObj("s2", "SrcBoxB") };

    int added = mergeDocumentsAlg(dst, src);

    CHECK(added == 2, "mergeScene: returns the number of objects appended");
    CHECK(dst.objects.size() == 4, "mergeScene: source objects appended to destination");

    CHECK(dst.textures.at("wood").uri == "wood_old.png",
          "mergeScene: texture key collision — existing destination texture wins");
    CHECK(dst.textures.count("metal") == 1,
          "mergeScene: non-colliding source texture is merged in");

    CHECK(dst.materials.count("stone") == 1, "mergeScene: original 'stone' material untouched");
    CHECK(dst.materials.at("stone").baseColor[0] == 0.5f,
          "mergeScene: material key collision — existing destination material is not overwritten");
    CHECK(dst.materials.count("stone_2") == 1,
          "mergeScene: colliding source material is inserted under a suffixed key");
    CHECK(dst.materials.at("stone_2").name == "stone_2",
          "mergeScene: suffixed material's name field is updated to match its new key");
    CHECK(dst.materials.at("stone_2").baseColor[0] == 0.1f,
          "mergeScene: suffixed material keeps the source's field values");

    CHECK(dst.actions.count("Walk") == 1, "mergeScene: original 'Walk' action untouched");
    CHECK(dst.actions.at("Walk").duration == 1.0f,
          "mergeScene: action key collision — existing destination action is not overwritten");
    CHECK(dst.actions.count("Walk_2") == 1,
          "mergeScene: colliding source action is inserted under a suffixed key");
    CHECK(dst.actions.at("Walk_2").name == "Walk_2",
          "mergeScene: suffixed action's name field is updated to match its new key");
    CHECK(dst.actions.at("Walk_2").duration == 2.0f,
          "mergeScene: suffixed action keeps the source's field values");
    CHECK(dst.actions.count("Jump") == 1,
          "mergeScene: non-colliding source action is merged in");
}

// STAB-0289: object ids live on every object in the tree (top-level and
// nested children alike), not just a flat map like textures/materials/
// actions. mergeDocumentsAlg() now resolves any id collision (top-level or
// nested) against the destination's existing tree the same way materials/
// actions suffix a colliding key, via collectObjectIdsAlg()/
// resolveObjectIdCollisionsAlg() (EditorAlgorithms.hpp).
static void testMergeSceneObjectIdCollisionResolved()
{
    // Top-level collision: src's top-level object shares dst's top-level "a".
    {
        Mc3Document dst = makeUndoScene(); // objects: "a" (with nested child "a_child"), "b"
        Mc3Document src;
        src.objects = { makeObj("a", "SrcBoxWithCollidingId") };

        int added = mergeDocumentsAlg(dst, src);

        CHECK(added == 1, "mergeScene: object with a colliding id is still appended (not dropped)");
        int countWithIdA = 0;
        for (const auto& o : dst.objects) if (o->id == "a") ++countWithIdA;
        CHECK(countWithIdA == 1,
              "mergeScene (STAB-0289): original destination object keeps id \"a\" unchanged");
        auto it = std::find_if(dst.objects.begin(), dst.objects.end(),
            [](const auto& o){ return o->name == "SrcBoxWithCollidingId"; });
        CHECK(it != dst.objects.end() && (*it)->id == "a_2",
              "mergeScene (STAB-0289): colliding top-level object id is suffixed to a unique "
              "value (\"a_2\"), matching the materials/actions suffix idiom");
    }

    // Nested collision: src's child object shares dst's *nested* child id
    // "a_child" (makeUndoScene()'s "a" object has a child with that id) —
    // proves the fix walks the whole subtree, not just top-level objects.
    {
        Mc3Document dst = makeUndoScene();
        Mc3Document src;
        auto srcChild  = makeObj("a_child", "SrcNestedChild");
        auto srcParent = makeObj("newparent", "SrcParent");
        srcParent->children.push_back(srcChild);
        src.objects = { srcParent };

        int added = mergeDocumentsAlg(dst, src);
        CHECK(added == 1, "mergeScene: parent with a nested colliding child id is still appended");

        auto parentIt = std::find_if(dst.objects.begin(), dst.objects.end(),
            [](const auto& o){ return o->name == "SrcParent"; });
        CHECK(parentIt != dst.objects.end(), "mergeScene: merged parent present");
        if (parentIt != dst.objects.end()) {
            CHECK((*parentIt)->id == "newparent",
                  "mergeScene: merged parent's own (non-colliding) id is untouched");
            CHECK(!(*parentIt)->children.empty() &&
                  (*parentIt)->children[0]->id == "a_child_2",
                  "mergeScene (STAB-0289): nested child id colliding with the destination's "
                  "own nested child (\"a_child\") is suffixed to a unique value, not just "
                  "top-level ids");
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Save As does not overwrite the original file (STAB-0272)
// ─────────────────────────────────────────────────────────────────────────────

static void testResolveSaveAsPath()
{
    auto [p1, mcb1] = resolveSaveAsPathAlg("scene");
    CHECK(p1 == "scene.mc3.xml" && !mcb1,
          "Save As: bare name gets .mc3.xml appended");

    auto [p2, mcb2] = resolveSaveAsPathAlg("scene.mc3.xml");
    CHECK(p2 == "scene.mc3.xml" && !mcb2,
          "Save As: already-suffixed path is left unchanged");

    auto [p3, mcb3] = resolveSaveAsPathAlg("scene.mcb");
    CHECK(p3 == "scene.mcb" && mcb3,
          "Save As: .mcb path is routed to the MCB writer, not suffixed with .mc3.xml");

    auto [p4, mcb4] = resolveSaveAsPathAlg("/tmp/proj/house");
    CHECK(p4 == "/tmp/proj/house.mc3.xml" && !mcb4,
          "Save As: directory prefix is preserved");
}

static void testSaveAsDoesNotOverwriteOriginal()
{
    Mc3Document doc = makeUndoScene();
    static int tmpIdx = 0;
    auto dir = std::filesystem::temp_directory_path();
    auto original = dir / ("mc3_saveas_orig_" + std::to_string(tmpIdx) + ".mc3.xml");
    auto newPath  = dir / ("mc3_saveas_new_"  + std::to_string(tmpIdx++) + ".mc3.xml");
    std::error_code ec;
    std::filesystem::remove(original, ec);
    std::filesystem::remove(newPath, ec);

    doc.model = "Original";
    doc.saveToFile(original);
    const std::string originalContent = readFile(original);

    // Simulate "Save As" to a different path: same document, resolved path.
    doc.model = "SavedAs";
    auto [resolved, isMcb] = resolveSaveAsPathAlg(newPath.string());
    CHECK(!isMcb, "Save As target resolves to the XML writer");
    doc.saveToFile(resolved);

    CHECK(std::filesystem::exists(original), "Save As: original file still exists");
    CHECK(readFile(original) == originalContent,
          "Save As: original file's content is untouched");
    CHECK(std::filesystem::exists(newPath), "Save As: new file was created");
    CHECK(readFile(newPath).find("model=\"SavedAs\"") != std::string::npos,
          "Save As: new file holds the current (post-edit) document");

    std::filesystem::remove(original, ec);
    std::filesystem::remove(newPath, ec);
}

// ─────────────────────────────────────────────────────────────────────────────
// Export Selection scoping (STAB-0273/0274)
// ─────────────────────────────────────────────────────────────────────────────

static void testExportSelectionOnlySelectedObjects()
{
    Mc3Document doc;
    auto a = makeObj("a", "A"); a->material = "stone";
    auto b = makeObj("b", "B"); b->material = "wood";
    auto c = makeObj("c", "C");
    doc.objects = { a, b, c };
    doc.materials["stone"] = Mc3::Mc3Material("stone", {1,0,0,1});
    doc.materials["wood"]  = Mc3::Mc3Material("wood",  {0,1,0,1});

    Mc3Document exported = exportSelectionAlg(doc, { a, c });

    CHECK(exported.objects.size() == 2,
          "Export Selection: only the selected objects are included");
    bool hasA = std::any_of(exported.objects.begin(), exported.objects.end(),
        [](const auto& o){ return o->id == "a"; });
    bool hasC = std::any_of(exported.objects.begin(), exported.objects.end(),
        [](const auto& o){ return o->id == "c"; });
    bool hasB = std::any_of(exported.objects.begin(), exported.objects.end(),
        [](const auto& o){ return o->id == "b"; });
    CHECK(hasA && hasC, "Export Selection: selected objects a and c are present");
    CHECK(!hasB, "Export Selection: unselected object b is excluded");
    CHECK(exported.materials.count("wood") == 0,
          "Export Selection: materials belonging only to unselected objects are excluded");
}

static void testExportSelectionIncludesDependentMaterialsAndTextures()
{
    Mc3Document doc;
    auto a = makeObj("a", "A");
    a->material = "stone";
    a->materialOverride = "trim";
    auto child = makeObj("a_child", "AChild");
    child->material = "wood"; // referenced only via a nested child
    a->children.push_back(child);
    doc.objects = { a };

    doc.materials["stone"] = Mc3::Mc3Material("stone", {1,0,0,1});
    doc.materials["stone"].baseColorTexture = "stoneTex";
    doc.materials["trim"]  = Mc3::Mc3Material("trim",  {0,0,1,1});
    doc.materials["wood"]  = Mc3::Mc3Material("wood",  {0,1,0,1});
    doc.materials["unrelated"] = Mc3::Mc3Material("unrelated", {1,1,1,1});

    doc.textures["stoneTex"]     = Mc3::Mc3Texture("stoneTex", "stone.png");
    doc.textures["unrelatedTex"] = Mc3::Mc3Texture("unrelatedTex", "unrelated.png");

    Mc3Document exported = exportSelectionAlg(doc, { a });

    CHECK(exported.materials.count("stone") == 1,
          "Export Selection: material referenced by 'material' is included");
    CHECK(exported.materials.count("trim") == 1,
          "Export Selection: material referenced by 'materialOverride' is included");
    CHECK(exported.materials.count("wood") == 1,
          "Export Selection: material referenced only by a nested child is included");
    CHECK(exported.materials.count("unrelated") == 0,
          "Export Selection: material not referenced by the selection is excluded");
    CHECK(exported.textures.count("stoneTex") == 1,
          "Export Selection: texture referenced by an included material is included");
    CHECK(exported.textures.count("unrelatedTex") == 0,
          "Export Selection: texture not referenced by any included material is excluded");
    CHECK(exported.objects.front()->children.size() == 1,
          "Export Selection: nested children are deep-copied along with their parent");
}

// ─────────────────────────────────────────────────────────────────────────────
// Export subtree as template (STAB-0536): before this fix, the exported file
// carried only the definition object — no materials/textures it referenced —
// so a standalone template file silently lost its appearance (fell back to
// default gray, STAB-0500) when loaded elsewhere.
// ─────────────────────────────────────────────────────────────────────────────

static void testExportSubtreeTemplateIncludesDependentMaterialsAndTextures()
{
    Mc3Document doc;
    doc.materials["stone"] = Mc3::Mc3Material("stone", {0.5f, 0.45f, 0.4f, 1.0f});
    doc.materials["stone"].baseColorTexture = "stoneTex";
    doc.materials["unrelated"] = Mc3::Mc3Material("unrelated", {1, 1, 1, 1});
    doc.textures["stoneTex"]     = Mc3::Mc3Texture("stoneTex", "stone.png");
    doc.textures["unrelatedTex"] = Mc3::Mc3Texture("unrelatedTex", "unrelated.png");

    auto pillar = makeObj("pillar_def", "Pillar");
    pillar->material = "stone";

    Mc3Document tmp = exportSubtreeTemplateAlg(doc, "pillar", pillar);

    CHECK(tmp.definitions.count("pillar") == 1,
          "Export Subtree Template: definition is present under its key");
    CHECK(tmp.materials.count("stone") == 1,
          "Export Subtree Template: material referenced by the definition is included");
    CHECK(tmp.textures.count("stoneTex") == 1,
          "Export Subtree Template: texture referenced by the included material is included");
    CHECK(tmp.materials.count("unrelated") == 0,
          "Export Subtree Template: material not referenced by the definition is excluded");
    CHECK(tmp.textures.count("unrelatedTex") == 0,
          "Export Subtree Template: texture not referenced by any included material is excluded");
    CHECK(tmp.objects.empty(),
          "Export Subtree Template: exported file has no top-level objects, only a definition");

    // Round-trip through the real XML writer/parser (same code path
    // saveToFile()/loadFromFile() use everywhere) to confirm the fix
    // actually produces a schema-shaped, reloadable file — not just a
    // correct in-memory document.
    auto path = std::filesystem::temp_directory_path() / "mc3_subtree_template_test.mc3.xml";
    tmp.saveToFile(path);
    Mc3Document reloaded = Mc3Document::loadFromFile(path);

    CHECK(reloaded.definitions.count("pillar") == 1,
          "Export Subtree Template: definition survives a save/reload round-trip");
    CHECK(reloaded.materials.count("stone") == 1,
          "Export Subtree Template: material survives a save/reload round-trip");
    CHECK(reloaded.textures.count("stoneTex") == 1,
          "Export Subtree Template: texture survives a save/reload round-trip");

    // STAB-0295: the parser round-trip above proves the file is well-formed
    // and semantically reloadable, but tinyxml2 (Mc3XmlParser) is lenient
    // about element order, while mc3.xsd's root-element order is strict —
    // so also validate against the real schema via the project's own
    // test/validate_xsd.py (lxml), the same tool the xsd_validation ctest
    // uses for every committed fixture. Degrades to a SKIP (not a failure)
    // if python3/lxml aren't available in this environment, mirroring
    // validate_xsd.py's own "lxml missing" diagnostic (STAB-0015).
    auto xsdPath = std::filesystem::path(__FILE__).parent_path().parent_path() / "mc3.xsd";
    auto validatorPath = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
                          / "test" / "validate_xsd.py";
    bool lxmlAvailable = std::system("python3 -c \"import lxml\" > /dev/null 2>&1") == 0;
    if (!lxmlAvailable || !std::filesystem::exists(xsdPath) || !std::filesystem::exists(validatorPath)) {
        std::cout << "SKIP: Export Subtree Template XSD validation -- "
                      "python3/lxml or validate_xsd.py/mc3.xsd not available in this environment\n";
    } else {
        std::string cmd = "python3 \"" + validatorPath.string() + "\" \"" + xsdPath.string() +
                           "\" \"" + path.string() + "\" > /dev/null 2>&1";
        int rc = std::system(cmd.c_str());
        CHECK(rc == 0,
              "Export Subtree Template: exported file validates against mc3.xsd "
              "(run manually to see the error: " + cmd + ")");
    }

    std::filesystem::remove(path);
}

// ─────────────────────────────────────────────────────────────────────────────
// Material export/import round-trip + collision handling (STAB-0544/0545):
// before this fix, exporting a single material dropped any texture it
// referenced, and importing never looked at a file's <textures> section at
// all -- so even a correctly-fixed export would still lose its texture on
// reimport. Both sides are fixed together.
// ─────────────────────────────────────────────────────────────────────────────

static void testExportMaterialIncludesReferencedTexture()
{
    Mc3Document doc;
    doc.materials["stone"] = Mc3::Mc3Material("stone", {0.5f, 0.45f, 0.4f, 1.0f});
    doc.materials["stone"].baseColorTexture = "stoneTex";
    doc.materials["unrelated"] = Mc3::Mc3Material("unrelated", {1, 1, 1, 1});
    doc.textures["stoneTex"]     = Mc3::Mc3Texture("stoneTex", "stone.png");
    doc.textures["unrelatedTex"] = Mc3::Mc3Texture("unrelatedTex", "unrelated.png");

    Mc3Document tmp = exportMaterialAlg(doc, "stone");

    CHECK(tmp.materials.count("stone") == 1, "Export Material: the material itself is included");
    CHECK(tmp.textures.count("stoneTex") == 1,
          "Export Material: texture referenced by the material is included");
    CHECK(tmp.materials.count("unrelated") == 0,
          "Export Material: unrelated material is excluded");
    CHECK(tmp.textures.count("unrelatedTex") == 0,
          "Export Material: unrelated texture is excluded");

    // Round-trip through the real XML writer/parser.
    auto path = std::filesystem::temp_directory_path() / "mc3_material_export_test.mc3.xml";
    tmp.saveToFile(path);
    Mc3Document reloaded = Mc3Document::loadFromFile(path);
    std::filesystem::remove(path);

    CHECK(reloaded.materials.count("stone") == 1, "Export Material: material survives save/reload");
    CHECK(reloaded.textures.count("stoneTex") == 1, "Export Material: texture survives save/reload");
    CHECK(reloaded.materials.at("stone").baseColorTexture == "stoneTex",
          "Export Material: material's texture reference still points at the reloaded texture's key");
}

static void testImportMaterialsNoCollision()
{
    Mc3Document dest;
    Mc3Document loaded;
    loaded.materials["stone"] = Mc3::Mc3Material("stone", {0.5f, 0.45f, 0.4f, 1.0f});
    loaded.materials["stone"].baseColorTexture = "stoneTex";
    loaded.textures["stoneTex"] = Mc3::Mc3Texture("stoneTex", "stone.png");

    std::string lastKey;
    int added = importMaterialsAlg(dest, loaded, &lastKey);

    CHECK(added == 1, "Import Material: returns the number of materials imported");
    CHECK(lastKey == "stone", "Import Material: no collision — key is unchanged");
    CHECK(dest.materials.count("stone") == 1, "Import Material: material is present under its key");
    CHECK(dest.textures.count("stoneTex") == 1, "Import Material: referenced texture is imported alongside it");
    CHECK(dest.materials.at("stone").baseColorTexture == "stoneTex",
          "Import Material: material's texture reference is unchanged when there's no collision");
}

static void testImportMaterialsCollisionHandling()
{
    Mc3Document dest;
    dest.materials["stone"] = Mc3::Mc3Material("stone", {0.1f, 0.1f, 0.1f, 1.0f}); // pre-existing, different
    dest.textures["stoneTex"] = Mc3::Mc3Texture("stoneTex", "existing_stone.png"); // pre-existing, different uri

    Mc3Document loaded;
    loaded.materials["stone"] = Mc3::Mc3Material("stone", {0.9f, 0.8f, 0.7f, 1.0f});
    loaded.materials["stone"].baseColorTexture = "stoneTex";
    loaded.textures["stoneTex"] = Mc3::Mc3Texture("stoneTex", "imported_stone.png");

    std::string lastKey;
    int added = importMaterialsAlg(dest, loaded, &lastKey);

    CHECK(added == 1, "Import Material collision: returns the number of materials imported");
    CHECK(lastKey == "stone_2", "Import Material collision: colliding material key is suffixed");
    CHECK(dest.materials.count("stone") == 1 && dest.materials.at("stone").baseColor[0] == 0.1f,
          "Import Material collision: pre-existing 'stone' material is untouched");
    CHECK(dest.materials.count("stone_2") == 1 && dest.materials.at("stone_2").baseColor[0] == 0.9f,
          "Import Material collision: imported material is inserted under the suffixed key");
    CHECK(dest.materials.at("stone_2").name == "stone_2",
          "Import Material collision: suffixed material's name field matches its new key");

    CHECK(dest.textures.count("stoneTex") == 1 && dest.textures.at("stoneTex").uri == "existing_stone.png",
          "Import Material collision: pre-existing 'stoneTex' texture is untouched");
    CHECK(dest.textures.count("stoneTex_2") == 1 && dest.textures.at("stoneTex_2").uri == "imported_stone.png",
          "Import Material collision: imported (different) texture is inserted under a suffixed key");
    CHECK(dest.materials.at("stone_2").baseColorTexture == "stoneTex_2",
          "Import Material collision: suffixed material's texture reference is re-pointed to the suffixed "
          "texture key, not left dangling at the pre-existing unrelated texture");
}

static void testImportMaterialsReusesIdenticalExistingTexture()
{
    Mc3Document dest;
    dest.textures["stoneTex"] = Mc3::Mc3Texture("stoneTex", "stone.png"); // same uri as import

    Mc3Document loaded;
    loaded.materials["stone2"] = Mc3::Mc3Material("stone2", {0.9f, 0.8f, 0.7f, 1.0f});
    loaded.materials["stone2"].baseColorTexture = "stoneTex";
    loaded.textures["stoneTex"] = Mc3::Mc3Texture("stoneTex", "stone.png"); // identical uri

    int added = importMaterialsAlg(dest, loaded);

    CHECK(added == 1, "Import Material reuse: returns the number of materials imported");
    CHECK(dest.textures.size() == 1,
          "Import Material reuse: an identical existing texture is reused, not duplicated under a suffix");
    CHECK(dest.materials.at("stone2").baseColorTexture == "stoneTex",
          "Import Material reuse: material's texture reference still points at the reused key");
}

// ─────────────────────────────────────────────────────────────────────────────
// Drag-drop MC3 file routing (STAB-0275)
// ─────────────────────────────────────────────────────────────────────────────

static void testDroppableScenePathDetection()
{
    CHECK(isDroppableScenePathAlg("house.mc3.xml"),
          "drag-drop: a .mc3.xml path is treated as a droppable scene file");
    CHECK(isDroppableScenePathAlg("/tmp/scenes/house.mc3.xml"),
          "drag-drop: droppable detection works with a full path");
    CHECK(isDroppableScenePathAlg("plain.xml"),
          "drag-drop: a bare .xml path is treated as a droppable scene file");
    CHECK(!isDroppableScenePathAlg("texture.png"),
          "drag-drop: a .png path is NOT treated as a droppable scene file");
    CHECK(!isDroppableScenePathAlg("readme.txt"),
          "drag-drop: an unrelated .txt path is NOT treated as a droppable scene file");
}

// ─────────────────────────────────────────────────────────────────────────────
// Invalid file load produces a named error, not a crash (STAB-0276)
//
// Mirrors the catch block in the "Open File" dialog (MeshCraftApplication_
// UiOverlays.cpp:1160-1179): Mc3Document::loadFromFile() is called directly
// (no Alg mirror needed — it's already CNA-free), and the dialog relies on it
// throwing std::exception with a non-empty, descriptive message for any
// unparseable input rather than crashing.
// ─────────────────────────────────────────────────────────────────────────────

static void testInvalidFileLoadThrowsNamedError()
{
    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_invalid_" + std::to_string(tmpIdx++) + ".mc3.xml");
    std::error_code ec;

    {
        std::ofstream f(path);
        f << "This is not XML at all { } <<<";
    }
    bool threwForGarbage = false;
    std::string garbageMsg;
    try { Mc3Document::loadFromFile(path); }
    catch (const std::exception& e) { threwForGarbage = true; garbageMsg = e.what(); }
    CHECK(threwForGarbage, "loading a non-XML file throws instead of crashing");
    CHECK(!garbageMsg.empty(), "the thrown exception has a non-empty, named message");
    std::filesystem::remove(path, ec);

    {
        std::ofstream f(path);
        f << "<?xml version=\"1.0\"?><notmc3></notmc3>";
    }
    bool threwForWrongRoot = false;
    std::string wrongRootMsg;
    try { Mc3Document::loadFromFile(path); }
    catch (const std::exception& e) { threwForWrongRoot = true; wrongRootMsg = e.what(); }
    CHECK(threwForWrongRoot, "loading well-formed XML with the wrong root element throws");
    CHECK(!wrongRootMsg.empty(), "the wrong-root exception has a non-empty, named message");
    std::filesystem::remove(path, ec);

    auto missingPath = std::filesystem::temp_directory_path() /
                       ("mc3_invalid_missing_" + std::to_string(tmpIdx++) + ".mc3.xml");
    bool threwForMissing = false;
    try { Mc3Document::loadFromFile(missingPath); }
    catch (const std::exception&) { threwForMissing = true; }
    CHECK(threwForMissing, "loading a nonexistent path throws instead of crashing");
}

// ─────────────────────────────────────────────────────────────────────────────
// Animation keyframe insertion is undoable (STAB-0284)
// ─────────────────────────────────────────────────────────────────────────────

static void testUndoRedoAnimKeyframe()
{
    Mc3Document doc = makeUndoScene();
    doc.actions["Walk"] = Mc3::Mc3Action::make("Walk", 2.0f);
    checkUndoRedo("animKeyframe", doc, [&](Mc3Document& d) {
        insertAnimKeyframesAlg(d, d.actions["Walk"], *d.objects.front(),
            { Mc3::AnimatedProperty::PositionX, Mc3::AnimatedProperty::Visible }, 0.5f);
    });
}

static void testInsertAnimKeyframesCreatesAndReplaces()
{
    Mc3Document doc = makeUndoScene();
    auto& obj = *doc.objects.front();
    obj.transform.position[0] = 3.0f;
    Mc3::Mc3Action action = Mc3::Mc3Action::make("Walk");

    insertAnimKeyframesAlg(doc, action, obj, { Mc3::AnimatedProperty::PositionX }, 0.0f);
    CHECK(action.channels.size() == 1,
          "anim keyframe: creates a new channel for a never-animated property");
    CHECK(action.channels[0].keyframes.size() == 1,
          "anim keyframe: creates one keyframe at time 0");
    CHECKF(action.channels[0].keyframes[0].value, 3.0f,
          "anim keyframe: captures the object's current value");

    obj.transform.position[0] = 9.0f;
    insertAnimKeyframesAlg(doc, action, obj, { Mc3::AnimatedProperty::PositionX }, 1.0f);
    CHECK(action.channels.size() == 1,
          "anim keyframe: reuses the existing channel for the same property");
    CHECK(action.channels[0].keyframes.size() == 2,
          "anim keyframe: adds a second keyframe at a new time");

    insertAnimKeyframesAlg(doc, action, obj, { Mc3::AnimatedProperty::PositionX }, 1.0f);
    CHECK(action.channels[0].keyframes.size() == 2,
          "anim keyframe: re-inserting at the same time replaces, not duplicates");
    CHECKF(action.channels[0].keyframes[1].value, 9.0f,
          "anim keyframe: the replaced keyframe holds the latest value");
}

// Regression: the headless mirror used to capture Deform/Material keyframes as
// 0.0 (it fell through to evaluateChannel on an empty channel) while production
// captured the object's live value. Both now share resolveObjectPropertyValueAlg,
// so a Deform/Material keyframe must seed with the live default, not 0.0.
static void testInsertAnimKeyframesDeformMaterialLiveValue()
{
    Mc3Document doc = makeUndoScene();
    auto& obj = *doc.objects.front();
    Mc3::Mc3Action action = Mc3::Mc3Action::make("Fx");

    // Object has no deform and no material -> documented live defaults apply.
    insertAnimKeyframesAlg(doc, action, obj,
        { Mc3::AnimatedProperty::DeformX,
          Mc3::AnimatedProperty::MaterialBaseColorR,
          Mc3::AnimatedProperty::MaterialBaseColorA,
          Mc3::AnimatedProperty::MaterialRoughness }, 0.0f);

    auto val = [&](Mc3::AnimatedProperty p) -> float {
        for (const auto& ch : action.channels)
            if (ch.property == p && !ch.keyframes.empty())
                return ch.keyframes[0].value;
        return -999.0f;
    };
    CHECKF(val(Mc3::AnimatedProperty::DeformX), 1.0f,
          "deform keyframe seeds with live default 1.0, not 0.0");
    CHECKF(val(Mc3::AnimatedProperty::MaterialBaseColorR), 0.8f,
          "material base-color R keyframe seeds with default 0.8, not 0.0");
    CHECKF(val(Mc3::AnimatedProperty::MaterialBaseColorA), 1.0f,
          "material base-color A keyframe seeds with default 1.0, not 0.0");
    CHECKF(val(Mc3::AnimatedProperty::MaterialRoughness), 0.5f,
          "material roughness keyframe seeds with default 0.5, not 0.0");

    // With an actual material assigned, the live material value is captured.
    Mc3::Mc3Material mat; mat.name = "Red";
    mat.baseColor = {0.9f, 0.1f, 0.2f, 0.7f};
    doc.materials["Red"] = mat;
    obj.material = "Red";
    Mc3::Mc3Action action2 = Mc3::Mc3Action::make("Fx2");
    insertAnimKeyframesAlg(doc, action2, obj,
        { Mc3::AnimatedProperty::MaterialBaseColorR }, 0.0f);
    float r = -999.0f;
    for (const auto& ch : action2.channels)
        if (ch.property == Mc3::AnimatedProperty::MaterialBaseColorR && !ch.keyframes.empty())
            r = ch.keyframes[0].value;
    CHECKF(r, 0.9f, "material keyframe captures the assigned material's live value");
}

// ─────────────────────────────────────────────────────────────────────────────
// Registry insert is undoable (STAB-0285)
//
// The "Insert" button (MeshCraftApplication_UiRegistry.cpp:80-101) does
// `pushUndo(); document_.objects.push_back(obj); modified_ = true;` — a plain
// append, no new Alg needed. checkUndoRedo confirms the generic snapshot/undo
// mechanism (STAB-0279) round-trips it, same as STAB-0270/0278/0283.
// ─────────────────────────────────────────────────────────────────────────────

static void testUndoRedoRegistryInsert()
{
    Mc3Document doc = makeUndoScene();
    checkUndoRedo("registryInsert", doc, [&](Mc3Document& d) {
        auto obj = makeObj("reg_chair01", "Chair", Mc3::ObjectType::Instance);
        obj->definition = "chair01";
        d.objects.push_back(obj);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Keybinding persistence (STAB-0286)
// ─────────────────────────────────────────────────────────────────────────────

static void testKeyBindStringFormat()
{
    KeyBindAlg unbound;
    CHECK(keyBindToStringAlg(unbound).empty(), "unbound key serializes to an empty string");
    CHECK(keyBindFromStringAlg("").key == 0, "an empty string parses back to unbound");

    KeyBindAlg b{true, false, true, 1}; // ctrl+alt+A
    std::string s = keyBindToStringAlg(b);
    CHECK(s == "ctrl+alt+A", "modifiers serialize in ctrl/shift/alt order, only when set");
    KeyBindAlg parsed = keyBindFromStringAlg(s);
    CHECK(parsed.ctrl && !parsed.shift && parsed.alt && parsed.key == 1,
          "a serialized binding parses back to the same value");

    KeyBindAlg parsedUpper = keyBindFromStringAlg("CTRL+SHIFT+f1");
    CHECK(parsedUpper.ctrl && parsedUpper.shift && !parsedUpper.alt && parsedUpper.key == 4,
          "parsing is case-insensitive for both modifier and key-name tokens");
}

static void testKeybindingPersistenceRoundTrip()
{
    std::map<std::string, KeyBindAlg> bindings;
    bindings["edit.undo"]     = {true,  false, false, 3}; // ctrl+Z
    bindings["tool.move"]     = {false, false, false, 2}; // bare S, no modifiers
    bindings["ui.cmdPalette"] = {true,  true,  true,  4}; // ctrl+shift+alt+F1
    bindings["edit.delete"]   = {false, false, false, 6};

    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_keybind_" + std::to_string(tmpIdx++) + ".ini");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    saveKeybindingsAlg(path, bindings);

    // Simulate a restart: start from a fresh defaults map (as
    // initDefaultBindings() would populate), then load the saved file.
    std::map<std::string, KeyBindAlg> reloaded;
    reloaded["edit.undo"]  = {true, false, false, 1}; // different default; must be overwritten
    reloaded["view.focus"] = {false, false, false, 5}; // not in the file; must survive untouched
    loadKeybindingsAlg(path, reloaded);

    CHECK(reloaded["edit.undo"].ctrl && reloaded["edit.undo"].key == 3,
          "keybinding persistence: a saved binding overwrites the pre-load default on reload");
    CHECK(reloaded["tool.move"].key == 2 && !reloaded["tool.move"].ctrl,
          "keybinding persistence: a no-modifier binding round-trips");
    CHECK(reloaded["ui.cmdPalette"].ctrl && reloaded["ui.cmdPalette"].shift && reloaded["ui.cmdPalette"].alt,
          "keybinding persistence: all three modifiers round-trip together");
    CHECK(reloaded["view.focus"].key == 5,
          "keybinding persistence: a binding absent from the file keeps its pre-load (default) value");

    std::filesystem::remove(path, ec);
}

// STAB-0328: a corrupted/hand-edited keybindings.ini must not crash or throw
// on load -- a line with no '=' is skipped entirely, and a line with an
// unrecognized key name resolves to "no key bound" (0) rather than throwing,
// since keyBindFromStringAlg() does no numeric parsing at all (pure string
// comparisons against a fixed name table).
static void testKeybindingsLoadSkipsGarbageLines()
{
    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_keybind_garbage_" + std::to_string(tmpIdx++) + ".ini");
    {
        std::ofstream f(path);
        f << "this line has no equals sign at all\n";
        f << "edit.undo=ctrl+totally_not_a_real_key_name\n";
        f << "\n"; // blank line
        f << "tool.move=S\n"; // well-formed, must still load correctly
    }

    std::map<std::string, KeyBindAlg> bindings;
    bindings["edit.undo"] = {true, false, false, 99}; // pre-load sentinel

    loadKeybindingsAlg(path, bindings); // must not throw/crash

    CHECK(bindings.count("edit.undo") == 1,
          "keybindings: a line with an unrecognized key name is loaded (not skipped), "
          "resolving to key=0 rather than crashing");
    CHECK(bindings["edit.undo"].ctrl && bindings["edit.undo"].key == 0,
          "keybindings: unrecognized key name resolves to key=0 (no binding), modifiers still parse");
    CHECK(bindings.count("tool.move") == 1 && bindings["tool.move"].key != 0,
          "keybindings: a well-formed line later in the file still loads correctly "
          "after garbage lines earlier");

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// ─────────────────────────────────────────────────────────────────────────────
// Preferences persistence (STAB-0287)
// ─────────────────────────────────────────────────────────────────────────────

static void testPrefsPersistenceRoundTrip()
{
    PrefsAlg p;
    p.autoSaveInterval = 30.0f;
    p.snapTranslate    = 0.5f;
    p.snapRotate       = 5.0f;
    p.snapScale        = 0.05f;
    p.gridSpacing      = 2.0f;
    p.theme            = 2;

    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_prefs_" + std::to_string(tmpIdx++) + ".ini");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    savePrefsAlg(path, p);

    PrefsAlg reloaded; // fresh defaults, as on a restarted process
    loadPrefsAlg(path, reloaded);

    CHECKF(reloaded.autoSaveInterval, 30.0f, "prefs: autoSaveInterval persists across restart");
    CHECKF(reloaded.snapTranslate,    0.5f,  "prefs: snapTranslate persists across restart");
    CHECKF(reloaded.snapRotate,       5.0f,  "prefs: snapRotate persists across restart");
    CHECKF(reloaded.snapScale,        0.05f, "prefs: snapScale persists across restart");
    CHECKF(reloaded.gridSpacing,      2.0f,  "prefs: gridSpacing persists across restart");
    CHECK(reloaded.theme == 2, "prefs: theme persists across restart");

    std::filesystem::remove(path, ec);
}

static void testPrefsLoadIgnoresMissingFile()
{
    PrefsAlg p; // defaults
    auto path = std::filesystem::temp_directory_path() / "mc3_prefs_never_written.ini";
    std::error_code ec;
    std::filesystem::remove(path, ec);
    loadPrefsAlg(path, p);
    CHECK(p.theme == 0, "prefs: loading a missing file leaves defaults untouched");
}

static void testPrefsLoadSkipsMalformedLines()
{
    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_prefs_malformed_" + std::to_string(tmpIdx++) + ".ini");
    std::error_code ec;
    {
        std::ofstream f(path);
        f << "autoSaveInterval=not_a_number\n";
        f << "snapRotate=7.5\n";
        f << "unknownKey=123\n";
    }
    PrefsAlg p;
    loadPrefsAlg(path, p);
    CHECKF(p.autoSaveInterval, 60.0f,
          "prefs: an unparseable value leaves that field at its prior (default) value");
    CHECKF(p.snapRotate, 7.5f, "prefs: a later, well-formed line still loads correctly");
    std::filesystem::remove(path, ec);
}

// ─────────────────────────────────────────────────────────────────────────────
// GLB export settings persist across dialog re-open (STAB-0290)
//
// glbExportFmt_ / glbAllowApproxCSG_ are plain MeshCraftApplication members
// (declared in MeshCraftApplication.hpp), not local dialog state — and
// exportGltf() (MeshCraftApplication_FileOps.cpp:164-180), which runs every
// time the "Export to glTF/GLB" dialog is (re)opened, only resets
// glbExportOutBuf_ / glbExportErr_ / glbExportOpen_. It never touches
// glbExportFmt_ or glbAllowApproxCSG_, so both trivially persist across
// repeated dialog opens within a running session. No test possible without a
// CNA/ImGui harness (same as STAB-0277/0302) — verified by inspection only.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// Macro save/load round-trip (STAB-0292)
// ─────────────────────────────────────────────────────────────────────────────

static void testMacroSaveLoadRoundTrip()
{
    std::vector<MacroStepAlg> steps = {
        {"add",           {"Sphere"}},
        {"linear_array",  {"5", "0", "2.0"}},
        {"lock",          {}},
        {"batch_rename",  {"{type}_{index:02d}"}},
    };

    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_macro_" + std::to_string(tmpIdx++) + ".mc3macro");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    saveMacroAlg(path, steps);
    std::vector<MacroStepAlg> loaded = loadMacroAlg(path);

    CHECK(loaded.size() == steps.size(), "macro round-trip: same step count");
    bool allMatch = loaded.size() == steps.size();
    for (size_t i = 0; allMatch && i < steps.size(); ++i)
        allMatch = loaded[i].verb == steps[i].verb && loaded[i].args == steps[i].args;
    CHECK(allMatch, "macro round-trip: every step's verb and args match exactly, in order");

    std::filesystem::remove(path, ec);
}

static void testMacroLoadSkipsBlankLines()
{
    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_macro_blank_" + std::to_string(tmpIdx++) + ".mc3macro");
    {
        std::ofstream f(path);
        f << "add\tBox\n\n\ndelete\n";
    }
    std::vector<MacroStepAlg> loaded = loadMacroAlg(path);
    CHECK(loaded.size() == 2, "macro load: blank lines between steps are skipped");
    CHECK(loaded[0].verb == "add" && loaded[0].args.size() == 1 && loaded[0].args[0] == "Box",
          "macro load: a step with one arg parses correctly");
    CHECK(loaded[1].verb == "delete" && loaded[1].args.empty(),
          "macro load: a step with no args parses correctly");
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// ─────────────────────────────────────────────────────────────────────────────
// Recent files list (STAB-0288) — loadRecentFilesAlg/saveRecentFilesAlg/
// addRecentFileAlg (EditorAlgorithms.hpp), mirroring loadRecentFiles()/
// saveRecentFiles()/addRecentFile() (MeshCraftApplication_FileOps.cpp).
// ─────────────────────────────────────────────────────────────────────────────

static void testRecentFilesLoadSkipsNonexistentPaths()
{
    static int tmpIdx = 0;
    auto listPath = std::filesystem::temp_directory_path() /
                    ("mc3_recent_" + std::to_string(tmpIdx++) + ".txt");
    auto realFile = std::filesystem::temp_directory_path() /
                    ("mc3_recent_real_" + std::to_string(tmpIdx++) + ".mc3.xml");
    { std::ofstream f(realFile); f << "<mc3/>"; }

    {
        std::ofstream f(listPath);
        f << (std::filesystem::temp_directory_path() / "does_not_exist_at_all.mc3.xml").string() << "\n";
        f << realFile.string() << "\n";
    }

    std::vector<std::filesystem::path> recent;
    loadRecentFilesAlg(listPath, recent, 10);
    CHECK(recent.size() == 1, "recent files: a path that no longer exists on disk is skipped on load");
    CHECK(recent.size() == 1 && recent[0] == realFile,
          "recent files: the still-existing path is kept");

    std::error_code ec;
    std::filesystem::remove(listPath, ec);
    std::filesystem::remove(realFile, ec);
}

static void testRecentFilesLoadRespectsMaxCap()
{
    static int tmpIdx = 0;
    auto listPath = std::filesystem::temp_directory_path() /
                    ("mc3_recent_cap_" + std::to_string(tmpIdx++) + ".txt");
    std::vector<std::filesystem::path> realFiles;
    {
        std::ofstream f(listPath);
        for (int i = 0; i < 15; ++i) {
            auto p = std::filesystem::temp_directory_path() /
                     ("mc3_recent_cap_file_" + std::to_string(tmpIdx) + "_" + std::to_string(i) + ".mc3.xml");
            { std::ofstream rf(p); rf << "<mc3/>"; }
            realFiles.push_back(p);
            f << p.string() << "\n";
        }
    }

    std::vector<std::filesystem::path> recent;
    loadRecentFilesAlg(listPath, recent, /*maxRecentFiles=*/10);
    CHECK(recent.size() == 10, "recent files: load stops at maxRecentFiles even if the file has more entries");

    std::error_code ec;
    std::filesystem::remove(listPath, ec);
    for (const auto& p : realFiles) std::filesystem::remove(p, ec);
}

static void testAddRecentFileDedupsMovesToFrontAndCaps()
{
    static int tmpIdx = 0;
    std::vector<std::filesystem::path> realFiles;
    auto makeFile = [&](int i) {
        auto p = std::filesystem::temp_directory_path() /
                 ("mc3_recent_add_" + std::to_string(tmpIdx) + "_" + std::to_string(i) + ".mc3.xml");
        { std::ofstream f(p); f << "<mc3/>"; }
        realFiles.push_back(p);
        return p;
    };
    tmpIdx++;

    std::vector<std::filesystem::path> recent;
    auto fileA = makeFile(0);
    auto fileB = makeFile(1);
    auto fileC = makeFile(2);

    addRecentFileAlg(recent, fileA, 10);
    addRecentFileAlg(recent, fileB, 10);
    addRecentFileAlg(recent, fileC, 10);
    CHECK(recent.size() == 3 && recent[0] == fileC && recent[1] == fileB && recent[2] == fileA,
          "recent files: most-recently-added file is at the front");

    // Re-adding fileA (already in the list) must move it to the front, not duplicate it.
    addRecentFileAlg(recent, fileA, 10);
    CHECK(recent.size() == 3, "recent files: re-adding an existing entry doesn't duplicate it");
    CHECK(!recent.empty() && recent[0] == std::filesystem::absolute(fileA),
          "recent files: re-adding an existing entry moves it to the front");

    // Cap enforcement: adding beyond maxRecentFiles drops the oldest.
    std::vector<std::filesystem::path> capped;
    std::vector<std::filesystem::path> capFiles;
    for (int i = 0; i < 5; ++i) capFiles.push_back(makeFile(100 + i));
    for (const auto& p : capFiles) addRecentFileAlg(capped, p, /*maxRecentFiles=*/3);
    CHECK(capped.size() == 3, "recent files: list never exceeds maxRecentFiles");
    CHECK(!capped.empty() && capped[0] == std::filesystem::absolute(capFiles.back()),
          "recent files: most-recently-added file is still at the front after capping");
    CHECK(std::find(capped.begin(), capped.end(), std::filesystem::absolute(capFiles.front())) == capped.end(),
          "recent files: the oldest entry is dropped once the cap is exceeded");

    std::error_code ec;
    for (const auto& p : realFiles) std::filesystem::remove(p, ec);
}

static void testRecentFilesPersistenceRoundTrip()
{
    static int tmpIdx = 0;
    auto listPath = std::filesystem::temp_directory_path() /
                    ("mc3_recent_roundtrip_" + std::to_string(tmpIdx++) + ".txt");
    auto file1 = std::filesystem::temp_directory_path() /
                 ("mc3_recent_rt1_" + std::to_string(tmpIdx) + ".mc3.xml");
    auto file2 = std::filesystem::temp_directory_path() /
                 ("mc3_recent_rt2_" + std::to_string(tmpIdx) + ".mc3.xml");
    { std::ofstream f(file1); f << "<mc3/>"; }
    { std::ofstream f(file2); f << "<mc3/>"; }

    std::vector<std::filesystem::path> recent = {file2, file1};
    saveRecentFilesAlg(listPath, recent);

    std::vector<std::filesystem::path> reloaded; // fresh, as on a restarted process
    loadRecentFilesAlg(listPath, reloaded, 10);
    CHECK(reloaded.size() == 2 && reloaded[0] == file2 && reloaded[1] == file1,
          "recent files: save/load round-trip preserves order across a simulated restart");

    std::error_code ec;
    std::filesystem::remove(listPath, ec);
    std::filesystem::remove(file1, ec);
    std::filesystem::remove(file2, ec);
}

// ─────────────────────────────────────────────────────────────────────────────
// Hierarchy filter: type/layer/tag/material-only filters actually narrow the
// list, not just text search (STAB-0306) — regression test for a real bug
// where SceneHierarchyPanel.cpp gated the skip-decision on `filtering`
// (text-only) instead of `anyFiltering`, so a type/layer/tag/material filter
// with no search text typed silently had no effect. Fixed alongside this
// mirror (see EditorAlgorithms.hpp's HierarchyFilterAlg comment).
// ─────────────────────────────────────────────────────────────────────────────

static void testHierarchyFilterTypeOnlyNarrowsWithoutText()
{
    auto box    = makeObj("box",    "MyBox",    Mc3::ObjectType::Box);
    auto sphere = makeObj("sphere", "MySphere", Mc3::ObjectType::Sphere);
    auto mesh   = makeObj("mesh",   "MyMesh",   Mc3::ObjectType::Mesh);

    HierarchyFilterAlg f;
    f.typeFilter = 2; // "Mesh" only, per hierarchyMatchesTypeAlg
    // No text typed — this is exactly the case the bug affected.

    auto shouldSkip = [&](const std::shared_ptr<Mc3Object>& o) {
        return hierarchyAnyFilterActiveAlg(f) && !hierarchyFilterMatchesAlg(f, *o);
    };

    CHECK(shouldSkip(box),    "hierarchy filter: type-only filter (no text) skips a non-matching Box");
    CHECK(shouldSkip(sphere), "hierarchy filter: type-only filter (no text) skips a non-matching Sphere");
    CHECK(!shouldSkip(mesh),  "hierarchy filter: type-only filter (no text) keeps a matching Mesh");
}

static void testHierarchyFilterRecursesIntoChildren()
{
    auto child  = makeObj("c", "TargetMesh", Mc3::ObjectType::Mesh);
    auto parent = makeObj("p", "Group",      Mc3::ObjectType::Group);
    parent->children.push_back(child);

    HierarchyFilterAlg f;
    f.typeFilter = 2; // Mesh only — parent itself is a Group, doesn't match directly

    CHECK(hierarchyFilterMatchesAlg(f, *parent),
          "hierarchy filter: a non-matching parent still matches via a matching descendant");
}

static void testHierarchyFilterOrMode()
{
    auto box = makeObj("box", "SpecialName", Mc3::ObjectType::Box);

    HierarchyFilterAlg f;
    f.textLower  = "special";       // matches by name
    f.typeFilter = 2;               // does NOT match by type (Box != Mesh)
    f.orMode     = true;

    CHECK(hierarchyFilterMatchesAlg(f, *box),
          "hierarchy filter: OR mode matches on any active filter, not all of them");

    f.orMode = false;
    CHECK(!hierarchyFilterMatchesAlg(f, *box),
          "hierarchy filter: AND mode (default) requires every active filter to match");
}

static void testHierarchyFilterNoActiveFiltersMatchesEverything()
{
    auto box = makeObj("box", "Anything", Mc3::ObjectType::Box);
    HierarchyFilterAlg f; // all fields at their inactive default
    CHECK(hierarchyFilterMatchesAlg(f, *box),
          "hierarchy filter: with no filter active, every object matches");
    CHECK(!hierarchyAnyFilterActiveAlg(f),
          "hierarchy filter: hierarchyAnyFilterActiveAlg is false when nothing is active");
}

// ─────────────────────────────────────────────────────────────────────────────
// Invalid material reference doesn't crash the renderer (STAB-0304)
// ─────────────────────────────────────────────────────────────────────────────

static void testMaterialColorFallsBackForMissingReference()
{
    Mc3Document doc;
    doc.materials["stone"] = Mc3::Mc3Material("stone", {0.2f, 0.4f, 0.6f, 1.0f});

    auto emptyRef = materialColorAlg("", doc);
    auto missingRef = materialColorAlg("nonexistent", doc);
    const float expectedGray = 180.0f / 255.0f;

    CHECKF(emptyRef[0], expectedGray, "material color: empty material id falls back to default gray (R)");
    CHECKF(emptyRef[3], 1.0f,         "material color: default gray fallback is fully opaque");
    CHECKF(missingRef[0], expectedGray,
          "material color: a material id not present in the document falls back to default gray, not a crash");
}

static void testMaterialColorResolvesExistingMaterial()
{
    Mc3Document doc;
    doc.materials["stone"] = Mc3::Mc3Material("stone", {0.2f, 0.4f, 0.6f, 1.0f});
    auto c = materialColorAlg("stone", doc);
    CHECKF(c[0], 0.2f, "material color: resolves an existing material's base color (R)");
    CHECKF(c[1], 0.4f, "material color: resolves an existing material's base color (G)");
    CHECKF(c[2], 0.6f, "material color: resolves an existing material's base color (B)");
    CHECKF(c[3], 1.0f, "material color: resolves an existing material's base color (A)");

    doc.materials["hot"] = Mc3::Mc3Material("hot", {1.5f, -0.5f, 0.5f, 1.0f});
    auto hot = materialColorAlg("hot", doc);
    CHECKF(hot[0], 1.0f, "material color: an out-of-range base color component is clamped high");
    CHECKF(hot[1], 0.0f, "material color: an out-of-range base color component is clamped low");
}

// ─────────────────────────────────────────────────────────────────────────────
// Duplicate material IDs (STAB-0308) — a document-model-level guarantee, not
// a mirror: `Mc3Document::materials` is a std::map, so a duplicate key can
// only arise while *parsing* XML (two <material id="dup"> elements), never
// in-memory. Confirms the parser's policy is "last wins", tested directly
// against the real (already CNA-free) Mc3Document::loadFromFile.
// ─────────────────────────────────────────────────────────────────────────────

static void testDuplicateMaterialIdLastWins()
{
    static int tmpIdx = 0;
    auto path = std::filesystem::temp_directory_path() /
                ("mc3_dupmat_" + std::to_string(tmpIdx++) + ".mc3.xml");
    {
        std::ofstream f(path);
        f << "<?xml version=\"1.0\"?>\n"
             "<mc3 version=\"0.3\" model=\"DupMat\">\n"
             "  <materials>\n"
             "    <material id=\"dup\" roughness=\"0.1\"><base_color>1.0 0.0 0.0 1.0</base_color></material>\n"
             "    <material id=\"dup\" roughness=\"0.9\"><base_color>0.0 1.0 0.0 1.0</base_color></material>\n"
             "  </materials>\n"
             "</mc3>\n";
    }

    Mc3Document doc = Mc3Document::loadFromFile(path);

    CHECK(doc.materials.size() == 1, "duplicate material id: only one entry survives (map semantics)");
    CHECKF(doc.materials.at("dup").baseColor[1], 1.0f,
          "duplicate material id: the LAST <material> element in document order wins");
    CHECKF(doc.materials.at("dup").roughness, 0.9f,
          "duplicate material id: the last element's other fields win too, not a per-field merge");

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// ─────────────────────────────────────────────────────────────────────────────
// applyRenamePattern: {index} zero-padding over a sequence (STAB-0475)
//
// The single-value cases (idx=3 → "03", idx=7 → "007") are already covered
// by testApplyRenamePattern(); this locks in the *sequence* behavior a real
// batch rename produces: consecutive indices 1..3 each padded independently,
// not just one isolated value.
// ─────────────────────────────────────────────────────────────────────────────

static void testApplyRenamePatternZeroPaddingSequence()
{
    CHECK(applyRenamePatternAlg("{index:02d}", "X", 1, "Box") == "01",
          "zero-padding sequence: idx=1 -> \"01\"");
    CHECK(applyRenamePatternAlg("{index:02d}", "X", 2, "Box") == "02",
          "zero-padding sequence: idx=2 -> \"02\"");
    CHECK(applyRenamePatternAlg("{index:02d}", "X", 10, "Box") == "10",
          "zero-padding sequence: idx=10 -> \"10\" (no truncation once 2 digits are needed)");
    CHECK(applyRenamePatternAlg("{index:03d}", "X", 1, "Box") == "001",
          "zero-padding sequence: {index:03d} idx=1 -> \"001\"");
    CHECK(applyRenamePatternAlg("{index:03d}", "X", 42, "Box") == "042",
          "zero-padding sequence: {index:03d} idx=42 -> \"042\"");
}

// STAB-0476 (batchRename skips locked objects, algorithm-level) is already
// covered by testBatchRename()'s "with one locked object" case above
// (locked object keeps its name, index still advances past it) — that IS
// the algorithm-level test this item asks for, not a separate app-level one.
// No new test needed.

// ─────────────────────────────────────────────────────────────────────────────
// findReplace treats the search string literally, not as a regex (STAB-0477)
//
// replaceAllInString() (EditorAlgorithms.hpp) uses plain std::string::find(),
// never std::regex — so characters with special regex meaning (., *, +, (, ),
// [, ]) are matched literally by construction. This test locks that contract
// in explicitly, since a future refactor to a regex-based implementation
// would silently change behavior for names containing those characters.
// ─────────────────────────────────────────────────────────────────────────────

static void testFindReplaceTreatsSpecialCharsLiterally()
{
    auto o1 = makeObj("1", "Box.001");
    auto o2 = makeObj("2", "Box_001");   // would ALSO match "Box.001" if '.' were a regex wildcard
    auto o3 = makeObj("3", "BoxX001");   // same
    std::vector<std::shared_ptr<Mc3Object>> objects = {o1, o2, o3};
    std::set<std::string> locked, selIds;

    int cnt = countFindReplaceMatches(objects, locked, "Box.001", "Wall.001",
                                      /*caseSensitive=*/true, false, selIds);
    CHECK(cnt == 1, "find-replace: \".\" in the search string matches only a literal '.', not any char");

    applyFindReplaceNames(objects, locked, "Box.001", "Wall.001", true, false, selIds);
    CHECK(o1->name == "Wall.001", "find-replace: the literal '.' match is replaced");
    CHECK(o2->name == "Box_001",  "find-replace: an '_' in that position is NOT treated as a '.' wildcard match");
    CHECK(o3->name == "BoxX001",  "find-replace: an 'X' in that position is NOT treated as a '.' wildcard match");

    // A search string that would be an invalid/greedy regex (unbalanced
    // parens, '*' with nothing to repeat) must not throw or behave oddly —
    // it's just a literal substring that happens not to match anything.
    auto o4 = makeObj("4", "Normal");
    std::vector<std::shared_ptr<Mc3Object>> objects2 = {o4};
    int cnt2 = countFindReplaceMatches(objects2, locked, "(unbalanced*", "x",
                                       true, false, selIds);
    CHECK(cnt2 == 0, "find-replace: a regex-invalid search string is just a literal no-match, not an error");
}

// ─────────────────────────────────────────────────────────────────────────────
// deepCopyObjectAlg's identity contract (STAB-0478/0479)
//
// testDeepCopy() above already confirms children are preserved (STAB-0478).
// STAB-0479 asks that "the copied object's ID differs from the original" —
// but that's the wrong expectation for THIS function specifically:
// deepCopyObjectAlg is the snapshot primitive checkUndoRedo() is built on
// (see snapshotDoc() at the top of this file), and undo/redo MUST restore
// the exact same ids, or every existing checkUndoRedo test in this file
// (which compares XML-serialized equality after undo) would fail. Making a
// COPY with a genuinely different id is a *different* operation — that's
// what duplicateObjectsAlg does, layered on top of deepCopyObjectAlg (see
// testDuplicateObjects() above: `root[1]->id == "a_copy"`, already tested).
// This test documents and locks in that distinction explicitly.
// ─────────────────────────────────────────────────────────────────────────────

static void testDeepCopyPreservesIdentityForSnapshots()
{
    auto src = makeObj("orig_id", "OrigName");
    auto copy = deepCopyObjectAlg(*src);
    CHECK(copy->id == src->id,
          "deepCopyObjectAlg: preserves the original id exactly (required for undo/redo snapshot equality)");

    // The layer that DOES need a new id (duplicateObjectsAlg) gets one —
    // confirmed already by testDuplicateObjects(); cross-referenced here so
    // the distinction isn't lost.
    std::vector<std::shared_ptr<Mc3Object>> root = {src};
    auto dup = duplicateObjectsAlg(root, {src});
    CHECK(dup.front()->id != src->id,
          "duplicateObjectsAlg (built on deepCopyObjectAlg): the DUPLICATE command does assign a new id");
}

// ─────────────────────────────────────────────────────────────────────────────
// Undo/redo stack depth cap (STAB-0481)
// ─────────────────────────────────────────────────────────────────────────────

static void testUndoStackDepthCapped()
{
    const int kCap = 20;
    std::vector<Mc3Document> stack;
    for (int i = 1; i <= 200; ++i) {
        Mc3Document d;
        d.model = "v" + std::to_string(i);
        pushWithCapAlg(stack, std::move(d), kCap);
    }
    CHECK(static_cast<int>(stack.size()) == kCap,
          "undo stack: after 200 pushes, size is capped at 20, not unbounded");
    CHECK(stack.front().model == "v181",
          "undo stack: the oldest surviving entry is the 181st push (oldest 180 were dropped)");
    CHECK(stack.back().model == "v200",
          "undo stack: the newest entry is the most recent push");
}

static void testUndoStackBelowCapUnaffected()
{
    const int kCap = 20;
    std::vector<Mc3Document> stack;
    for (int i = 1; i <= 5; ++i) {
        Mc3Document d;
        d.model = "v" + std::to_string(i);
        pushWithCapAlg(stack, std::move(d), kCap);
    }
    CHECK(stack.size() == 5, "undo stack: below the cap, nothing is dropped");
    CHECK(stack.front().model == "v1", "undo stack: below the cap, the first push is still there");
}

// ─────────────────────────────────────────────────────────────────────────────
// Viewport ray-cast picking (STAB-0503)
//
// Found and fixed a real bug: the real click-to-select handler
// (handleMouseInput(), MeshCraftApplication_Mouse.cpp) only ray-cast-tested
// objects with `obj->primitive` set — i.e. only the 11 primitive shape
// types. Instance/Mesh/Group/Extrude/CSG objects (8 other ObjectType
// values) could never be selected by clicking directly on them in the 3D
// viewport, unlike box-select (drag-rectangle selection), which already
// selects by screen-projected position regardless of type. Fixed by
// extracting pickObjectByRayAlg(), which tests every visible object
// (defaulting to a 0.5-unit half-extent AABB, scaled by transform.scale,
// for any non-primitive type) and wiring the real handler to call it.
// ─────────────────────────────────────────────────────────────────────────────

static void testPickObjectByRay()
{
    // A ray straight down +Z, tested against objects placed at z=0 with
    // varying x offsets.
    std::array<float,3> rayOrig{0.0f, 0.0f, -10.0f};
    std::array<float,3> rayDir{0.0f, 0.0f, 1.0f};

    // Non-primitive object (Instance) at the origin: no `primitive` set at
    // all. Pre-fix, this was silently unpickable; now it gets a default
    // 0.5-unit half-extent box and the ray (which passes through x=0,y=0)
    // hits it.
    auto inst = makeObj("inst1", "Instance1", Mc3::ObjectType::Instance);
    inst->transform.position = {0.0f, 0.0f, 0.0f};
    std::vector<std::shared_ptr<Mc3Object>> rootsA{inst};
    auto pickedA = pickObjectByRayAlg(rootsA, rayOrig, rayDir);
    CHECK(pickedA == inst,
          "a non-primitive Instance object is now pickable by ray-cast (the STAB-0503 bug fix)");

    // Same Instance, but far enough off-axis that the ray misses its
    // default 0.5-unit half-extent box.
    auto instFar = makeObj("inst2", "Instance2", Mc3::ObjectType::Instance);
    instFar->transform.position = {5.0f, 0.0f, 0.0f};
    std::vector<std::shared_ptr<Mc3Object>> rootsB{instFar};
    CHECK(pickObjectByRayAlg(rootsB, rayOrig, rayDir) == nullptr,
          "a ray that misses even the default half-extent box picks nothing");

    // Primitive sized larger than the 0.5 default (sphere radius 2): a ray
    // offset at x=1.8 (inside the real radius, well outside the generic
    // default) must still hit — confirms primitives keep using their real
    // size, not the non-primitive fallback.
    auto sphereObj = makeObj("sphere1", "BigSphere", Mc3::ObjectType::Sphere);
    sphereObj->transform.position = {0.0f, 0.0f, 0.0f};
    sphereObj->primitive = Mc3::Mc3Primitive::sphere(2.0f);
    std::array<float,3> rayOffsetOrig{1.8f, 0.0f, -10.0f};
    std::vector<std::shared_ptr<Mc3Object>> rootsC{sphereObj};
    CHECK(pickObjectByRayAlg(rootsC, rayOffsetOrig, rayDir) == sphereObj,
          "a primitive keeps using its real size (radius 2 sphere hit at x=1.8, well outside the 0.5 default)");
    std::array<float,3> rayOffsetOrig2{3.0f, 0.0f, -10.0f};
    CHECK(pickObjectByRayAlg(rootsC, rayOffsetOrig2, rayDir) == nullptr,
          "a ray outside a primitive's real radius (x=3 vs. radius 2) still correctly misses");

    // Invisible objects are never pickable even if the ray would hit them.
    auto hidden = makeObj("hidden1", "Hidden", Mc3::ObjectType::Box);
    hidden->transform.position = {0.0f, 0.0f, 0.0f};
    hidden->visible = false;
    std::vector<std::shared_ptr<Mc3Object>> rootsD{hidden};
    CHECK(pickObjectByRayAlg(rootsD, rayOrig, rayDir) == nullptr,
          "an invisible object is never picked, even directly on the ray");

    // Closest-wins: two objects on the same ray at different depths.
    auto near = makeObj("near1", "Near", Mc3::ObjectType::Box);
    near->transform.position = {0.0f, 0.0f, -2.0f};
    auto far = makeObj("far1", "Far", Mc3::ObjectType::Box);
    far->transform.position = {0.0f, 0.0f, 5.0f};
    std::vector<std::shared_ptr<Mc3Object>> rootsE{far, near}; // far listed first
    CHECK(pickObjectByRayAlg(rootsE, rayOrig, rayDir) == near,
          "of two objects along the same ray, the nearer one wins regardless of list order");

    // Recursion: a child object in the ray's path is found even when its
    // parent (elsewhere in space) is not itself hit.
    auto parent = makeObj("group1", "Group1", Mc3::ObjectType::Group);
    parent->transform.position = {50.0f, 50.0f, 50.0f};
    auto child = makeObj("child1", "Child1", Mc3::ObjectType::Instance);
    child->transform.position = {0.0f, 0.0f, 0.0f};
    parent->children.push_back(child);
    std::vector<std::shared_ptr<Mc3Object>> rootsF{parent};
    CHECK(pickObjectByRayAlg(rootsF, rayOrig, rayDir) == child,
          "a child object in the ray's path is found by recursion, even though its parent isn't hit");

    // Empty scene / no hit at all.
    std::vector<std::shared_ptr<Mc3Object>> rootsG{};
    CHECK(pickObjectByRayAlg(rootsG, rayOrig, rayDir) == nullptr,
          "an empty object list picks nothing");
}

// ─────────────────────────────────────────────────────────────────────────────
// Click-to-select resolution (STAB-0504)
//
// Confirmed by reading handleMouseInput() (MeshCraftApplication_Mouse.cpp)
// that clicking empty space (no Ctrl) already clears the selection, via
// `if (!ctrl) selection_.clear(); if (bestObj) selection_.select(bestObj);`
// right after the ray-cast pick. Extracted this into resolveClickSelectionAlg()
// (single source of truth, wired the real handler to call it) so the
// clear-on-miss / additive-on-Ctrl behavior is directly tested rather than
// just read.
// ─────────────────────────────────────────────────────────────────────────────

static void testResolveClickSelection()
{
    using MeshCraft::Editor::SelectionManager;

    auto a = makeObj("a", "A");
    auto b = makeObj("b", "B");

    // Click on empty space (no Ctrl) clears the entire selection.
    SelectionManager sel1;
    sel1.select(a);
    resolveClickSelectionAlg(sel1, nullptr, false);
    CHECK(!sel1.hasSelection(),
          "clicking empty space without Ctrl clears the selection entirely (STAB-0504)");

    // Click on a new object (no Ctrl) replaces the selection.
    SelectionManager sel2;
    sel2.select(a);
    resolveClickSelectionAlg(sel2, b, false);
    CHECK(sel2.selection().size() == 1 && sel2.selection()[0] == b,
          "clicking a new object without Ctrl replaces the previous selection");

    // Ctrl+click on empty space leaves the existing selection untouched.
    SelectionManager sel3;
    sel3.select(a);
    resolveClickSelectionAlg(sel3, nullptr, true);
    CHECK(sel3.selection().size() == 1 && sel3.selection()[0] == a,
          "Ctrl+click on empty space never loses the existing selection");

    // Ctrl+click on a new object is additive, not a replacement.
    SelectionManager sel4;
    sel4.select(a);
    resolveClickSelectionAlg(sel4, b, true);
    CHECK(sel4.selection().size() == 2,
          "Ctrl+click on a new object adds to the selection instead of replacing it");
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera view presets (STAB-0506)
//
// Verifies the actual preset table used by the "Front/Top/Right/Persp"
// button row (MeshCraftApplication_UiOverlays.cpp, now shared via
// cameraPresetsAlg()) against cameraOrbitPositionAlg() — the same
// spherical-to-Cartesian formula EditorCamera::position() uses — so the
// resulting camera position for each preset is checked directly, not just
// read.
// ─────────────────────────────────────────────────────────────────────────────

static void testCameraPresets()
{
    const auto& presets = cameraPresetsAlg();
    CHECK(presets.size() == 4, "there are exactly 4 camera presets: Front, Top, Right, Persp");

    const CameraPresetAlg* front = nullptr;
    const CameraPresetAlg* top   = nullptr;
    const CameraPresetAlg* right = nullptr;
    const CameraPresetAlg* persp = nullptr;
    for (const auto& p : presets) {
        if (std::string(p.label) == "Front") front = &p;
        if (std::string(p.label) == "Top")   top   = &p;
        if (std::string(p.label) == "Right") right = &p;
        if (std::string(p.label) == "Persp") persp = &p;
    }
    CHECK(front && top && right && persp, "all 4 expected preset labels are present");
    if (!front || !top || !right || !persp) return;

    CHECK(!front->reset && !top->reset && !right->reset,
          "Front/Top/Right only set yaw+pitch, they don't trigger a full camera reset");
    CHECK(persp->reset, "Persp triggers a full camera reset (distance/target back to default)");

    const float dist = 10.0f;
    const std::array<float,3> origin{0.0f, 0.0f, 0.0f};

    // Front: camera sits on the target's +Z axis, looking back toward -Z.
    auto frontPos = cameraOrbitPositionAlg(front->yaw, front->pitch, dist, origin);
    CHECKF(frontPos[0], 0.0f,  "Front preset: camera X stays at the target's X");
    CHECKF(frontPos[1], 0.0f,  "Front preset: camera Y stays at the target's Y");
    CHECKF(frontPos[2], dist,  "Front preset: camera sits on the +Z axis (looks toward -Z)");

    // Right: camera sits on the target's +X axis.
    auto rightPos = cameraOrbitPositionAlg(right->yaw, right->pitch, dist, origin);
    CHECKF(rightPos[0], dist, "Right preset: camera sits on the +X axis");
    CHECK(std::abs(rightPos[2]) < 1e-3f, "Right preset: camera Z stays near the target's Z");

    // Top: a deliberately near-90deg (not exact) pitch, so the camera ends up
    // almost directly above the target with a small residual Z offset,
    // dodging the gimbal singularity viewMatrix() guards for |pitch| > 1.47.
    auto topPos = cameraOrbitPositionAlg(top->yaw, top->pitch, dist, origin);
    CHECK(topPos[1] > dist * 0.99f, "Top preset: camera Y is almost the full distance above the target");
    CHECK(std::abs(topPos[0]) < 1e-3f, "Top preset: camera X stays at the target's X (yaw=0)");
    CHECK(topPos[2] > 0.0f && topPos[2] < dist * 0.2f,
          "Top preset: a small residual +Z offset avoids the exact-90deg gimbal singularity");

    // The orbit position formula tracks an arbitrary (non-origin) target too.
    auto offsetPos = cameraOrbitPositionAlg(0.0f, 0.0f, 5.0f, {2.0f, 3.0f, 4.0f});
    CHECKF(offsetPos[0], 2.0f, "camera position tracks a non-origin target's X");
    CHECKF(offsetPos[1], 3.0f, "camera position tracks a non-origin target's Y");
    CHECKF(offsetPos[2], 9.0f, "camera position tracks a non-origin target's Z plus the orbit distance");
}

// ─────────────────────────────────────────────────────────────────────────────
// Every command pushes an undo entry (STAB-0480)
//
// Audited every MeshCraftApplication_Commands.cpp function for a document
// mutation without a preceding pushUndo(). **Found and fixed a real bug**:
// resetPivot() mutated selected objects' transform.position/pivot and set
// modified_ = true, but never called pushUndo() — resetting a pivot could
// not be undone with Ctrl+Z, unlike every other mutating command in that
// file. Fixed by adding a hasSelection() guard + pushUndo() at the top,
// matching the pattern every other command in the file already uses.
// (Not independently listed as toggleIsolate()'s missing pushUndo(): that
// toggle is intentionally self-reversing — re-toggling restores the exact
// pre-isolate visibility from preisolateVisibility_ — so treating it as a
// discrete undo step would be redundant, not a bug.)
// No headless test possible: resetPivot()'s pivot-compensation math uses
// Microsoft::Xna::Framework::Matrix (a CNA type), so it isn't separable
// into a CNA-free mirror the way the other command fixes in this suite
// were. Verified by code inspection + confirming the fix compiles clean.
//
// Follow-up (plan_deep_audit.md AUDIT-0014, 2026-07-09): the fix above gave
// resetPivot() its own internal pushUndo(), but the UI call site
// (Scene/PropertiesPanel.cpp's "Reset##piv" button) still had its own
// pushUndo() immediately before calling resetPivot() -- a leftover from
// before this fix landed, when the UI-side push was the only one. Net
// effect: one click pushed two identical undo snapshots, so Ctrl+Z was a
// no-op on the first press. Removed the now-redundant outer pushUndo() at
// the UI call site. Same "no headless test possible" constraint applies
// (same CNA-dependent function) -- verified by code inspection + full
// ninja build + ctest 66/66 pass, same standard as the fix above.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// STAB-0214/0215: SceneRenderer's CSG content-hash preview cache (K1) —
// csgSubtreeHashAlg()/csgHashMixAlg() (CsgCacheAlg.hpp) exercised directly,
// headlessly, without a live GraphicsDevice.
// ─────────────────────────────────────────────────────────────────────────────

static void testCsgSubtreeHashChangesOnChildMove()
{
    Mc3Document doc;
    auto csgNode = std::make_shared<Mc3Object>();
    csgNode->type = ObjectType::Union;
    auto child = std::make_shared<Mc3Object>();
    child->id = "c1"; child->type = ObjectType::Box;
    child->primitive = Mc3Primitive{};
    csgNode->children.push_back(child);

    auto h1 = csgSubtreeHashAlg(*csgNode, doc);
    auto h1b = csgSubtreeHashAlg(*csgNode, doc);
    CHECK(h1 == h1b, "csg hash: unchanged subtree hashes identically across calls (real cache hit)");

    // STAB-0214: moving a child must change the subtree hash (cache invalidates).
    child->transform.position = {1.0f, 0.0f, 0.0f};
    auto h2 = csgSubtreeHashAlg(*csgNode, doc);
    CHECK(h1 != h2, "csg hash: moving a child changes the subtree hash (cache invalidates)");
}

static void testCsgSubtreeHashChangesOnParentTransform()
{
    // Mirrors SceneRenderer.cpp's actual cache-key construction: the subtree's
    // own content hash is folded together with the accumulated parentWorld
    // matrix (csgHashMixAlg per component), so a moved *parent* (not just a
    // child) also invalidates the cache — STAB-0215.
    Mc3Document doc;
    auto csgNode = std::make_shared<Mc3Object>();
    csgNode->type = ObjectType::Difference;
    auto base = std::make_shared<Mc3Object>();
    base->id = "base"; base->type = ObjectType::Box; base->primitive = Mc3Primitive{};
    csgNode->children.push_back(base);

    auto subtreeHash = csgSubtreeHashAlg(*csgNode, doc);

    auto foldMatrix = [](std::size_t h, const std::array<float, 16>& m) {
        for (float v : m) h = csgHashMixAlg(h, std::hash<float>{}(v));
        return h;
    };

    const std::array<float, 16> identity  = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::array<float, 16> translated = identity;
    translated[12] = 5.0f; // translation.x, column-major 4x4 (row 3 = translation row)

    auto fpIdentity   = foldMatrix(subtreeHash, identity);
    auto fpTranslated = foldMatrix(subtreeHash, translated);

    CHECK(fpIdentity != fpTranslated,
          "csg cache key: a moved CSG parent (different parentWorld matrix folded in) "
          "changes the final cache key even though the subtree's own content hash is unchanged");
}

static void testCsgSubtreeHashIsContentBasedNotPointerBased()
{
    // STAB-0249: undo/redo restores a document via whole-document snapshot
    // copies — the restored CSG object is a genuinely different C++ object
    // at a different memory address than the one that existed before the
    // undo. If csgSubtreeHashAlg() hashed the object's *address* (or any
    // pointer-derived value), every undo/redo would be a guaranteed cache
    // miss even when the content is identical. Build two separate Mc3Object
    // instances (deliberately different addresses, mimicking pre-undo vs.
    // post-undo) with identical content and confirm they hash identically.
    Mc3Document doc;

    auto buildEquivalentCsgNode = []() {
        auto node = std::make_shared<Mc3Object>();
        node->id = "csg1"; node->type = ObjectType::Difference;
        auto base = std::make_shared<Mc3Object>();
        base->id = "base"; base->type = ObjectType::Box; base->primitive = Mc3Primitive{};
        auto cutter = std::make_shared<Mc3Object>();
        cutter->id = "cutter"; cutter->type = ObjectType::Sphere;
        cutter->primitive = Mc3Primitive{}; cutter->primitive->radius = 0.4f;
        cutter->isCutter = true;
        node->children.push_back(base);
        node->children.push_back(cutter);
        return node;
    };

    auto preUndoNode  = buildEquivalentCsgNode();
    auto postUndoNode = buildEquivalentCsgNode(); // distinct shared_ptr, distinct address

    CHECK(preUndoNode.get() != postUndoNode.get(),
          "csg hash pointer-independence: sanity check that the two test objects are "
          "genuinely different instances (different addresses), not the same object reused");

    auto hPre  = csgSubtreeHashAlg(*preUndoNode, doc);
    auto hPost = csgSubtreeHashAlg(*postUndoNode, doc);
    CHECK(hPre == hPost,
          "csg hash: two distinct Mc3Object instances (different memory addresses) with "
          "identical content hash identically — the cache key is content-based, not "
          "pointer-based, so undo/redo to an unchanged CSG subtree is a real cache hit");
}

static void testCsgSubtreeHashChangesOnOperationTypeChange()
{
    // AUDIT-0020: switching a CSG node's own operation (Union/Difference/
    // Intersection) with no change to any child's geometry/transform must
    // still change the subtree hash, or the preview cache never invalidates
    // and keeps showing the stale (wrong) boolean-op result.
    Mc3Document doc;
    auto buildNode = [](CsgType t) {
        auto node = std::make_shared<Mc3Object>();
        node->type = ObjectType::Union; // container object type is irrelevant here
        node->csgOperation = Mc3CsgOperation{};
        node->csgOperation->csgType = t;
        auto child = std::make_shared<Mc3Object>();
        child->id = "c1"; child->type = ObjectType::Box; child->primitive = Mc3Primitive{};
        node->children.push_back(child);
        return node;
    };

    auto unionNode = buildNode(CsgType::Union);
    auto diffNode  = buildNode(CsgType::Difference);

    auto hUnion = csgSubtreeHashAlg(*unionNode, doc);
    auto hDiff  = csgSubtreeHashAlg(*diffNode, doc);
    CHECK(hUnion != hDiff,
          "csg hash: changing csgOperation (Union -> Difference) with identical children "
          "changes the subtree hash (cache invalidates instead of showing a stale preview)");
}

static void testCsgSubtreeHashChangesOnExtrudeAndMeshSource()
{
    // AUDIT-0021: an Extrude object's geometry lives in obj.extrude (not
    // obj.primitive), and a Mesh object's geometry is selected by
    // obj.meshSource — both must be hashed or editing them inside a CSG tree
    // won't invalidate the cached preview.
    Mc3Document doc;

    auto extrudeA = std::make_shared<Mc3Object>();
    extrudeA->id = "e1"; extrudeA->type = ObjectType::Extrude;
    extrudeA->extrude = Mc3Extrude{};
    extrudeA->extrude->twist = 0.0f;

    auto extrudeB = std::make_shared<Mc3Object>();
    extrudeB->id = "e1"; extrudeB->type = ObjectType::Extrude;
    extrudeB->extrude = Mc3Extrude{};
    extrudeB->extrude->twist = 45.0f;

    CHECK(csgSubtreeHashAlg(*extrudeA, doc) != csgSubtreeHashAlg(*extrudeB, doc),
          "csg hash: changing an Extrude object's profile (twist) changes the subtree hash");

    auto meshA = std::make_shared<Mc3Object>();
    meshA->id = "m1"; meshA->type = ObjectType::Mesh; meshA->meshSource = "meshA";

    auto meshB = std::make_shared<Mc3Object>();
    meshB->id = "m1"; meshB->type = ObjectType::Mesh; meshB->meshSource = "meshB";

    CHECK(csgSubtreeHashAlg(*meshA, doc) != csgSubtreeHashAlg(*meshB, doc),
          "csg hash: changing a Mesh object's meshSource reference changes the subtree hash");
}

// ─────────────────────────────────────────────────────────────────────────────
// STAB-0217/0218: investigated, no new headless test added — findings below.
//
// STAB-0217 (exportCsgMesh() OBJ triangle count matches the live preview's
// csgCachedTriCount()): both code paths call the identical buildManifoldTree()
// (SceneRenderer.cpp) on the identical object subtree — exportCsgMesh() with
// an identity parent matrix, the live preview with the real accumulated
// parentWorld. Manifold::Transform() is a pure affine map on already-built
// vertex positions (it does not re-tessellate), and a single consistent
// affine transform applied to an entire CSG subtree preserves every
// sibling's relative geometry (overlaps/intersections), so boolean-op
// topology — and therefore triangle count — is invariant to which global
// transform (identity vs. parentWorld) is used. The two paths are therefore
// provably equal by construction, not just by coincidence. exportCsgMesh()
// is only reachable via an ImGui button (MeshCraftApplication_UiOverlays.cpp)
// with no CLI/headless entry point; adding one purely to empirically re-prove
// what's already provable by this argument would be scope creep beyond a P2
// verification task.
//
// STAB-0218 (CSG child reorder via PropertiesPanel.cpp's drag-and-drop is
// undoable): confirmed by code inspection that `ctx.pushUndo()` is called
// immediately before the reorder mutation (matching the placement of every
// other undo-tracked mutation in that file). Undo itself is snapshot-based
// (whole-Mc3Document copies, see testDeepCopyPreservesIdentityForSnapshots
// above) — std::vector copy/assignment inherently preserves element order,
// so ANY mutation preceded by pushUndo(), including a children-vector
// reorder, is restorable by construction; no reorder-specific undo logic
// exists (or is needed) to verify separately. Like resetPivot() elsewhere in
// this file, the drag-and-drop detection itself is ImGui-interaction-coupled
// and not separable into a CNA-free Alg mirror, so no headless test drives
// the actual drag gesture.
// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    testApplyRenamePattern();
    testBatchRename();
    testRenameObjectInActionsAlg();
    testFindReplace();
    testArrayDuplicate();
    testDuplicateObjects();
    testGroupAndUngroupObjects();
    testUngroupRejectsNonGroupOrEmptyGroup();
    testConvertToDefinition();
    testBreakInstance();
    testAlignToObject();
    testScatterAlongCurve();
    testProportionalFalloff();
    testVertexSnap();
    testRotationDragSnap();
    testFlattenDescendants();
    testGroupScale();
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
    testAiSaveToRegistryUsesAiPendingDoc();
    testAiSaveToRegistryWorksWithoutApply();
    testCommandsIgnoreObjectNotInTree();
    testConfirmIfModifiedGate();
    testUnsavedDialogChoices();
    testUndoRedoAiApply();
    testUndoRedoMergeScene();
    testMergeSceneCollisionHandling();
    testMergeSceneObjectIdCollisionResolved();
    testResolveSaveAsPath();
    testSaveAsDoesNotOverwriteOriginal();
    testExportSelectionOnlySelectedObjects();
    testExportSelectionIncludesDependentMaterialsAndTextures();
    testExportSubtreeTemplateIncludesDependentMaterialsAndTextures();
    testExportMaterialIncludesReferencedTexture();
    testImportMaterialsNoCollision();
    testImportMaterialsCollisionHandling();
    testImportMaterialsReusesIdenticalExistingTexture();
    testDroppableScenePathDetection();
    testInvalidFileLoadThrowsNamedError();
    testUndoRedoAnimKeyframe();
    testInsertAnimKeyframesCreatesAndReplaces();
    testInsertAnimKeyframesDeformMaterialLiveValue();
    testUndoRedoRegistryInsert();
    testKeyBindStringFormat();
    testKeybindingPersistenceRoundTrip();
    testKeybindingsLoadSkipsGarbageLines();
    testPrefsPersistenceRoundTrip();
    testPrefsLoadIgnoresMissingFile();
    testPrefsLoadSkipsMalformedLines();
    testMacroSaveLoadRoundTrip();
    testMacroLoadSkipsBlankLines();
    testRecentFilesLoadSkipsNonexistentPaths();
    testRecentFilesLoadRespectsMaxCap();
    testAddRecentFileDedupsMovesToFrontAndCaps();
    testRecentFilesPersistenceRoundTrip();
    testHierarchyFilterTypeOnlyNarrowsWithoutText();
    testHierarchyFilterRecursesIntoChildren();
    testHierarchyFilterOrMode();
    testHierarchyFilterNoActiveFiltersMatchesEverything();
    testMaterialColorFallsBackForMissingReference();
    testMaterialColorResolvesExistingMaterial();
    testDuplicateMaterialIdLastWins();
    testApplyRenamePatternZeroPaddingSequence();
    testFindReplaceTreatsSpecialCharsLiterally();
    testDeepCopyPreservesIdentityForSnapshots();
    testUndoStackDepthCapped();
    testUndoStackBelowCapUnaffected();
    testPickObjectByRay();
    testResolveClickSelection();
    testCameraPresets();
    testCsgSubtreeHashChangesOnChildMove();
    testCsgSubtreeHashChangesOnParentTransform();
    testCsgSubtreeHashIsContentBasedNotPointerBased();
    testCsgSubtreeHashChangesOnOperationTypeChange();
    testCsgSubtreeHashChangesOnExtrudeAndMeshSource();

    std::cout << "\n";
    if (failures == 0)
        std::cout << "All tests passed.\n";
    else
        std::cout << failures << " test(s) FAILED.\n";

    return failures == 0 ? 0 : 1;
}
