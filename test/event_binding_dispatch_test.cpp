#include "MeshCraft/EventBindingAlgorithms.hpp"

#include <cstdio>
#include <memory>
#include <string>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Editor;

static int failures = 0;
static void check(bool condition, const std::string& message) {
    if (condition) std::printf("PASS: %s\n", message.c_str());
    else { std::printf("FAIL: %s\n", message.c_str()); ++failures; }
}

static Mc3Document testDocument() {
    Mc3Document doc;
    auto area = std::make_shared<Mc3Object>();
    area->id = "door_area";
    area->type = ObjectType::Area;
    doc.objects.push_back(area);
    doc.triggers["open_door"] = Mc3Trigger{"open_door", {}};
    doc.sceneStates["night"] = Mc3SceneState{"night", {}};
    return doc;
}

static Mc3EventBinding binding(const std::string& id, EventBindingEvent event,
                               EventBindingTarget targetType, const std::string& target) {
    Mc3EventBinding value;
    value.id = id;
    value.sourceObjectId = "door_area";
    value.event = event;
    value.targetType = targetType;
    value.targetId = target;
    return value;
}

int main() {
    Mc3Document doc = testDocument();
    auto enter = binding("enter_open", EventBindingEvent::Enter,
                         EventBindingTarget::Trigger, "open_door");
    enter.cooldown = 0.5f;
    auto exit = binding("exit_night", EventBindingEvent::Exit,
                        EventBindingTarget::SceneState, "night");
    exit.once = true;
    auto timer = binding("timer_open", EventBindingEvent::Timer,
                         EventBindingTarget::Trigger, "open_door");
    timer.interval = 0.5f;
    auto missingTarget = binding("dangling_target", EventBindingEvent::Enter,
                                 EventBindingTarget::Trigger, "gone");
    auto missingSource = binding("dangling_source", EventBindingEvent::Enter,
                                 EventBindingTarget::Trigger, "open_door");
    missingSource.sourceObjectId = "gone_area";
    doc.eventBindings = {enter, exit, timer, missingTarget, missingSource};

    EventBindingRuntimeState runtime;
    const auto originalObjects = doc.objects.size();
    const auto originalTriggers = doc.triggers.size();
    const auto originalStates = doc.sceneStates.size();

    auto enterReport = dispatchEventBindingsAlg(doc, runtime, "door_area", EventBindingEvent::Enter);
    check(enterReport.dispatches.size() == 1, "enter dispatch returns only a valid target");
    if (!enterReport.dispatches.empty()) {
        check(enterReport.dispatches[0].bindingId == "enter_open", "enter dispatch identifies binding");
        check(enterReport.dispatches[0].targetType == EventBindingTarget::Trigger,
              "enter dispatch retains trigger target kind");
    }
    check(enterReport.diagnostics.size() == 1 &&
          enterReport.diagnostics[0].kind == EventBindingDiagnosticKind::TargetMissing,
          "dangling target is reported instead of executed");
    auto missingSourceReport = dispatchEventBindingsAlg(doc, runtime, "gone_area", EventBindingEvent::Enter);
    check(missingSourceReport.dispatches.empty() && missingSourceReport.diagnostics.size() == 1 &&
          missingSourceReport.diagnostics[0].kind == EventBindingDiagnosticKind::SourceMissing,
          "dangling source is reported instead of executed");

    auto cooled = dispatchEventBindingsAlg(doc, runtime, "door_area", EventBindingEvent::Enter);
    check(cooled.dispatches.empty(), "cooldown suppresses immediate repeat");
    auto noTimerYet = advanceEventBindingRuntimeAlg(doc, runtime, 0.25f);
    check(noTimerYet.dispatches.empty(), "timer waits for configured interval");
    auto timerReport = advanceEventBindingRuntimeAlg(doc, runtime, 0.25f);
    check(timerReport.dispatches.size() == 1 && timerReport.dispatches[0].bindingId == "timer_open",
          "timer dispatch is produced after interval");
    auto afterCooldown = advanceEventBindingRuntimeAlg(doc, runtime, 0.25f);
    (void)afterCooldown;
    auto reentered = dispatchEventBindingsAlg(doc, runtime, "door_area", EventBindingEvent::Enter);
    check(reentered.dispatches.size() == 1, "elapsed time releases cooldown");

    auto firstExit = dispatchEventBindingsAlg(doc, runtime, "door_area", EventBindingEvent::Exit);
    auto secondExit = dispatchEventBindingsAlg(doc, runtime, "door_area", EventBindingEvent::Exit);
    check(firstExit.dispatches.size() == 1 && firstExit.dispatches[0].targetType == EventBindingTarget::SceneState,
          "exit binding dispatches its scene-state target");
    check(secondExit.dispatches.empty(), "one-shot binding dispatches once");

    runtime.dispatchActive = true;
    auto recursive = dispatchEventBindingsAlg(doc, runtime, "door_area", EventBindingEvent::Enter);
    runtime.dispatchActive = false;
    check(recursive.dispatches.empty() && recursive.diagnostics.size() == 1 &&
          recursive.diagnostics[0].kind == EventBindingDiagnosticKind::RecursionRejected,
          "recursion guard rejects nested dispatch");

    Mc3EventBinding extra = binding("enter_extra", EventBindingEvent::Enter,
                                    EventBindingTarget::Trigger, "open_door");
    doc.eventBindings.push_back(extra);
    EventBindingRuntimeState budgetRuntime;
    auto limited = dispatchEventBindingsAlg(doc, budgetRuntime, "door_area", EventBindingEvent::Enter, 1);
    check(limited.dispatches.size() == 1, "execution budget limits dispatch count");
    bool hasBudgetDiagnostic = false;
    for (const auto& diagnostic : limited.diagnostics)
        hasBudgetDiagnostic |= diagnostic.kind == EventBindingDiagnosticKind::BudgetExceeded;
    check(hasBudgetDiagnostic, "execution budget reports skipped dispatches");

    check(doc.objects.size() == originalObjects && doc.triggers.size() == originalTriggers &&
          doc.sceneStates.size() == originalStates && doc.objects[0]->id == "door_area",
          "dry-run dispatch never mutates authored document data");

    if (failures == 0) std::printf("All event binding dispatch tests passed.\n");
    return failures == 0 ? 0 : 1;
}
