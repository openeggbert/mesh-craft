#include "MeshCraft/Editor/EventPreviewRunner.hpp"

#include <cstdio>
#include <memory>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Editor;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) std::printf("PASS: %s\n", message.c_str());
    else { std::printf("FAIL: %s\n", message.c_str()); ++failures; }
}

Mc3EventBinding binding(std::string id, std::string source, EventBindingEvent event,
                        EventBindingTarget targetType, std::string target) {
    Mc3EventBinding value;
    value.id = std::move(id);
    value.sourceObjectId = std::move(source);
    value.event = event;
    value.targetType = targetType;
    value.targetId = std::move(target);
    return value;
}

std::shared_ptr<Mc3Object> object(std::string id, ObjectType type = ObjectType::Box) {
    auto value = std::make_shared<Mc3Object>();
    value->id = std::move(id);
    value->type = type;
    return value;
}

Mc3Object* findById(std::vector<std::shared_ptr<Mc3Object>>& objects, const std::string& id) {
    for (auto& value : objects) {
        if (!value) continue;
        if (value->id == id) return value.get();
        if (auto* child = findById(value->children, id)) return child;
    }
    return nullptr;
}

} // namespace

int main() {
    // Enter/exit is calculated from the complete Area transform, then a
    // trigger's script and playback effect publish together after one commit.
    {
        Mc3Document doc;
        auto parent = object("area_parent", ObjectType::Group);
        parent->transform.position = {2.0f, 0.0f, 0.0f};
        auto area = object("door_area", ObjectType::Area);
        Mc3Primitive areaShape;
        areaShape.size = {2.0f, 2.0f, 2.0f};
        area->primitive = areaShape;
        parent->children.push_back(area);
        auto lamp = object("lamp");
        doc.objects = {parent, lamp};
        doc.actions["open_animation"] = Mc3Action{};
        Mc3Script closeLamp;
        closeLamp.id = "close_lamp";
        closeLamp.source = "scene:find('lamp'):set_visible(false)";
        doc.scripts[closeLamp.id] = closeLamp;
        Mc3Trigger trigger;
        trigger.id = "open";
        trigger.steps = {{TriggerStepType::RunScript, "close_lamp"},
                         {TriggerStepType::PlayAction, "open_animation"}};
        doc.triggers[trigger.id] = trigger;
        doc.eventBindings.push_back(binding("area_enter", "door_area", EventBindingEvent::Enter,
                                            EventBindingTarget::Trigger, "open"));

        EventPreviewRunner runner;
        auto enter = runner.updateAreaTransitions(doc, {2.0f, 0.0f, 0.0f});
        check(enter.dispatches.size() == 1 && enter.dispatches.front().bindingId == "area_enter",
              "nested Area generates a deterministic enter dispatch");
        int commits = 0;
        auto executed = runner.execute(doc, std::move(enter), [&] { ++commits; });
        check(executed.documentCommitted && commits == 1,
              "successful Area trigger commits exactly once");
        check(!findById(doc.objects, "lamp")->visible,
              "Area trigger script commits its document mutation");
        check(executed.effects.size() == 1 &&
              executed.effects.front().type == EventPreviewEffectType::PlayAction,
              "trigger steps after a committed script remain valid and are staged until success");

        auto noRepeat = runner.updateAreaTransitions(doc, {2.0f, 0.0f, 0.0f});
        check(noRepeat.dispatches.empty(), "remaining inside an Area emits no duplicate enter");
        auto leave = runner.updateAreaTransitions(doc, {4.1f, 0.0f, 0.0f});
        check(leave.dispatches.empty(), "Area exit is silent when no exit binding is authored");
    }

    // Timer eligibility has no catch-up loop, observes cooldown/once state,
    // and does nothing unless an explicit runner update is requested.
    {
        Mc3Document doc;
        auto clock = object("clock");
        doc.objects.push_back(clock);
        doc.triggers["tick"] = Mc3Trigger{"tick", {}};
        auto timer = binding("timer_once", "clock", EventBindingEvent::Timer,
                             EventBindingTarget::Trigger, "tick");
        timer.interval = 0.5f;
        timer.once = true;
        doc.eventBindings.push_back(timer);

        EventPreviewRunner runner;
        check(runner.advanceTimers(doc, 0.49f).dispatches.empty(),
              "timer waits until its configured interval");
        auto tick = runner.advanceTimers(doc, 0.01f);
        check(tick.dispatches.size() == 1 && tick.dispatches.front().bindingId == "timer_once",
              "timer emits exactly one due dispatch");
        check(runner.advanceTimers(doc, 10.0f).dispatches.empty(),
              "one-shot timer cannot recursively catch up after firing");
    }

    // A failed later script rolls back an earlier script and suppresses the
    // earlier trigger's pending playback effects.  The callback models the
    // application's single undo/history capture at the commit boundary.
    {
        Mc3Document doc;
        auto button = object("button");
        auto lamp = object("lamp");
        lamp->visible = true;
        doc.objects = {button, lamp};
        doc.actions["flash"] = Mc3Action{};
        doc.scripts["mutate"] = Mc3Script{"mutate", "lua",
            "scene:find('lamp'):set_visible(false)"};
        doc.scripts["fail"] = Mc3Script{"fail", "lua",
            "scene:find('lamp'):set_visible(false); error('intentional failure')"};
        doc.triggers["first"] = Mc3Trigger{"first", {
            {TriggerStepType::PlayAction, "flash"},
            {TriggerStepType::RunScript, "mutate"},
        }};
        doc.triggers["second"] = Mc3Trigger{"second", {{TriggerStepType::RunScript, "fail"}}};
        auto firstBinding = binding("click_first", "button", EventBindingEvent::Click,
                                    EventBindingTarget::Trigger, "first");
        firstBinding.once = true;
        doc.eventBindings = {
            firstBinding,
            binding("click_second", "button", EventBindingEvent::Click,
                    EventBindingTarget::Trigger, "second"),
        };

        EventPreviewRunner runner;
        auto dispatch = runner.dispatchClick(doc, "button");
        check(dispatch.dispatches.size() == 2, "click dispatch includes both authored bindings");
        int commits = 0;
        auto executed = runner.execute(doc, std::move(dispatch), [&] { ++commits; });
        check(!executed.documentCommitted && commits == 0,
              "failed preview batch never reaches the commit boundary");
        check(!executed.errors.empty(), "failed preview batch returns the script error");
        check(findById(doc.objects, "lamp")->visible,
              "failed later script rolls back an earlier successful mutation");
        check(executed.effects.empty(),
              "failed later script suppresses earlier pending playback effects");
        check(runner.dispatchClick(doc, "button").dispatches.size() == 2,
              "failed preview restores one-shot/cooldown runtime eligibility");
    }

    // Scene-state targets execute in the same batch and reject an invalid
    // material rather than publishing a partially-applied state.
    {
        Mc3Document doc;
        auto source = object("switch");
        auto target = object("target");
        target->visible = true;
        doc.objects = {source, target};
        Mc3SceneState invalid;
        invalid.name = "invalid";
        Mc3ObjectOverride override;
        override.id = "target";
        override.visible = false;
        override.material = "missing_material";
        invalid.overrides.push_back(override);
        doc.sceneStates[invalid.name] = invalid;
        doc.eventBindings.push_back(binding("invalid_state", "switch", EventBindingEvent::Click,
                                            EventBindingTarget::SceneState, "invalid"));

        EventPreviewRunner runner;
        auto executed = runner.execute(doc, runner.dispatchClick(doc, "switch"));
        check(!executed.documentCommitted && !executed.errors.empty(),
              "invalid state rejects the entire preview batch");
        check(findById(doc.objects, "target")->visible,
              "invalid state leaves the live document untouched");
    }

    if (failures == 0) {
        std::printf("All event preview runner tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d event preview runner test(s) failed.\n", failures);
    return 1;
}
