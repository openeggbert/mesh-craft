// SYS-W14-20 (2026-07-20) — doc.sceneStates (Mc3SceneState: named per-object
// visibility/position/rotation/material overrides, e.g. "day"/"night"
// variants) were fully editable in the "States" tab, but selecting/
// applying a state never touched the live document_ objects -- there was
// no runtime "apply state" logic at all, only round-tripped data.
//
// Adds an "Apply State" button (MeshCraftApplication_UiLeftPanel.cpp's
// States tab) that writes each override's SET fields onto the matching
// live object (found by id via flatFindById()), so state transitions
// become previewable in the editor itself.
//
// MeshCraftApplication is CNA-coupled and not headlessly instantiable, so
// this mirrors the button's exact control-flow shape against plain
// Mc3Document data, with a small local findById() standing in for
// flatFindById() (a MeshCraftApplication member, not reachable here).

#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3SceneState.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) std::printf("PASS: %s\n", msg.c_str());
    else      { std::printf("FAIL: %s\n", msg.c_str()); ++failures; }
}

// Mirrors MeshCraftApplication::flatFindById()'s contract (recursive
// search over doc.objects by id) closely enough for this test's purposes.
static Mc3Object* findById(std::vector<std::shared_ptr<Mc3Object>>& list, const std::string& id) {
    for (auto& o : list) {
        if (!o) continue;
        if (o->id == id) return o.get();
        if (auto* found = findById(o->children, id)) return found;
    }
    return nullptr;
}

struct MockUndoState {
    int pushUndoCalls = 0;
    bool modifiedCalled = false;
};

// Byte-for-byte mirror of the "Apply State" button's logic, with
// pushUndo()/modified_/updateWindowTitle()/setStatusMsg() replaced by
// mock equivalents.
static std::string applyState(Mc3Document& doc, MockUndoState& mock, const Mc3SceneState& state) {
    bool anyResolvable = false;
    for (const auto& ov : state.overrides)
        if (findById(doc.objects, ov.id)) { anyResolvable = true; break; }
    if (anyResolvable) ++mock.pushUndoCalls;

    int applied = 0, missing = 0;
    for (const auto& ov : state.overrides) {
        Mc3Object* obj = findById(doc.objects, ov.id);
        if (!obj) { ++missing; continue; }
        if (ov.visible.has_value())  obj->visible = *ov.visible;
        if (ov.position.has_value()) obj->transform.position = *ov.position;
        if (ov.rotation.has_value()) obj->transform.rotation = *ov.rotation;
        if (ov.material.has_value()) obj->material = *ov.material;
        ++applied;
    }
    if (applied > 0) mock.modifiedCalled = true;

    std::string msg = "State applied: " + std::to_string(applied) + " object" +
        (applied == 1 ? "" : "s") + " updated";
    if (missing > 0)
        msg += ", " + std::to_string(missing) + " skipped (object id not found)";
    return msg;
}

static std::shared_ptr<Mc3Object> makeObj(const std::string& id) {
    auto o = std::make_shared<Mc3Object>();
    o->id = id;
    return o;
}

int main() {
    // --- Applying a state with visible/position/rotation/material
    // overrides actually mutates the real live objects. ---
    {
        Mc3Document doc;
        auto lamp = makeObj("lamp");
        lamp->visible = true;
        auto sun = makeObj("sun");
        sun->visible = false;
        sun->material = "day_mat";
        doc.objects.push_back(lamp);
        doc.objects.push_back(sun);

        Mc3SceneState night;
        night.name = "night";
        Mc3ObjectOverride lampOv;
        lampOv.id = "lamp";
        lampOv.visible = true;
        lampOv.position = std::array<float,3>{0.0f, 3.0f, 0.0f};
        night.overrides.push_back(lampOv);
        Mc3ObjectOverride sunOv;
        sunOv.id = "sun";
        sunOv.visible = false;
        night.overrides.push_back(sunOv);

        MockUndoState mock;
        std::string msg = applyState(doc, mock, night);

        check(lamp->visible == true, "Apply: lamp.visible overridden to true");
        check(lamp->transform.position[1] == 3.0f, "Apply: lamp.position overridden");
        check(sun->visible == false, "Apply: sun.visible overridden to false");
        check(sun->material == "day_mat", "Apply: sun.material NOT touched (no material override on this step)");
        check(mock.pushUndoCalls == 1, "Apply: exactly one undo snapshot pushed");
        check(mock.modifiedCalled, "Apply: modified_ set (objects were mutated)");
        check(msg.find("2 objects updated") != std::string::npos,
              "Apply: status message reports 2 objects updated (got: " + msg + ")");
    }

    // --- Only the SET fields on an override are applied -- an unset
    // field leaves the live object's existing value untouched. ---
    {
        Mc3Document doc;
        auto obj = makeObj("obj1");
        obj->transform.position = {5.0f, 5.0f, 5.0f};
        obj->material = "original_mat";
        doc.objects.push_back(obj);

        Mc3SceneState state;
        state.name = "partial";
        Mc3ObjectOverride ov;
        ov.id = "obj1";
        ov.visible = false; // only visible is set
        state.overrides.push_back(ov);

        MockUndoState mock;
        applyState(doc, mock, state);

        check(!obj->visible, "Partial override: the set field (visible) is applied");
        check(obj->transform.position[0] == 5.0f,
              "Partial override: an unset field (position) is left untouched");
        check(obj->material == "original_mat",
              "Partial override: an unset field (material) is left untouched");
    }

    // --- A missing object id is reported, not crashed, and doesn't
    // block the OTHER overrides in the same state from applying. ---
    {
        Mc3Document doc;
        auto obj = makeObj("real_obj");
        doc.objects.push_back(obj);

        Mc3SceneState state;
        state.name = "mixed";
        Mc3ObjectOverride ghostOv;
        ghostOv.id = "ghost_obj";
        ghostOv.visible = false;
        state.overrides.push_back(ghostOv);
        Mc3ObjectOverride realOv;
        realOv.id = "real_obj";
        realOv.visible = false;
        state.overrides.push_back(realOv);

        MockUndoState mock;
        std::string msg = applyState(doc, mock, state);

        check(!obj->visible, "Mixed: the real object's override still applied");
        check(msg.find("1 object updated") != std::string::npos,
              "Mixed: status message reports exactly 1 object updated (got: " + msg + ")");
        check(msg.find("1 skipped (object id not found)") != std::string::npos,
              "Mixed: status message reports the missing id (got: " + msg + ")");
        check(mock.pushUndoCalls == 1,
              "Mixed: undo snapshot still pushed once (at least one override was resolvable)");
    }

    // --- A state whose overrides ALL reference missing objects pushes no
    // undo snapshot at all (no-op-undo avoidance, matching this session's
    // own established discipline). ---
    {
        Mc3Document doc; // no objects at all
        Mc3SceneState state;
        state.name = "all_missing";
        Mc3ObjectOverride ov;
        ov.id = "does_not_exist";
        ov.visible = true;
        state.overrides.push_back(ov);

        MockUndoState mock;
        std::string msg = applyState(doc, mock, state);

        check(mock.pushUndoCalls == 0,
              "All-missing state: no undo snapshot pushed (nothing was actually mutated)");
        check(!mock.modifiedCalled, "All-missing state: modified_ not set");
        check(msg.find("0 objects updated") != std::string::npos,
              "All-missing state: status message reports 0 updated (got: " + msg + ")");
    }

    // --- An empty-overrides state is a clean no-op. ---
    {
        Mc3Document doc;
        Mc3SceneState state;
        state.name = "empty";
        MockUndoState mock;
        std::string msg = applyState(doc, mock, state);
        check(mock.pushUndoCalls == 0, "Empty state: no undo snapshot pushed");
        check(msg.find("0 object") != std::string::npos, "Empty state: reports 0 objects (got: " + msg + ")");
    }

    if (failures == 0) { std::printf("All scene-state-apply tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d scene-state-apply test(s) failed.\n", failures);
    return 1;
}
