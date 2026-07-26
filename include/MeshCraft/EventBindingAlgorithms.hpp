#pragma once

#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3EventBinding.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace MeshCraft::Editor {

// These structures are deliberately separate from Mc3Document.  They model
// only a transient editor/runtime preview: dispatch never executes a trigger
// or applies a state and therefore cannot alter authored data or undo state.
struct EventBindingRuntimeState {
    bool dispatchActive{false};
    std::unordered_map<std::string, float> cooldownRemaining;
    std::unordered_map<std::string, float> timerElapsed;
    std::unordered_set<std::string> oneShotFired;

    void reset() {
        dispatchActive = false;
        cooldownRemaining.clear();
        timerElapsed.clear();
        oneShotFired.clear();
    }
};

enum class EventBindingDiagnosticKind { SourceMissing, TargetMissing, BudgetExceeded, RecursionRejected };

struct EventBindingDiagnostic {
    EventBindingDiagnosticKind kind;
    std::string bindingId;
    std::string message;
};

struct EventBindingDispatch {
    std::string bindingId;
    std::string sourceObjectId;
    Mc3::EventBindingEvent event;
    Mc3::EventBindingTarget targetType;
    std::string targetId;
};

struct EventBindingDispatchReport {
    std::vector<EventBindingDispatch> dispatches;
    std::vector<EventBindingDiagnostic> diagnostics;
};

inline bool eventBindingObjectExistsAlg(const Mc3::Mc3Document& doc, const std::string& id) {
    const auto contains = [&id](const auto& self, const auto& objects) -> bool {
        for (const auto& object : objects) {
            if (!object) continue;
            if (object->id == id) return true;
            if (self(self, object->children)) return true;
        }
        return false;
    };
    return contains(contains, doc.objects);
}

inline bool eventBindingTargetExistsAlg(const Mc3::Mc3Document& doc,
                                        const Mc3::Mc3EventBinding& binding) {
    return binding.targetType == Mc3::EventBindingTarget::Trigger
        ? doc.triggers.contains(binding.targetId)
        : doc.sceneStates.contains(binding.targetId);
}

namespace Detail {

inline bool eventBindingOnCooldown(const EventBindingRuntimeState& runtime,
                                   const Mc3::Mc3EventBinding& binding) {
    const auto it = runtime.cooldownRemaining.find(binding.id);
    return it != runtime.cooldownRemaining.end() && it->second > 0.0f;
}

inline bool dispatchOneEventBindingAlg(const Mc3::Mc3Document& doc,
                                       EventBindingRuntimeState& runtime,
                                       const Mc3::Mc3EventBinding& binding,
                                       std::size_t budget,
                                       EventBindingDispatchReport& report) {
    if (!binding.enabled || runtime.oneShotFired.contains(binding.id) ||
        eventBindingOnCooldown(runtime, binding))
        return true;
    if (!eventBindingObjectExistsAlg(doc, binding.sourceObjectId)) {
        report.diagnostics.push_back({EventBindingDiagnosticKind::SourceMissing, binding.id,
            "Binding '" + binding.id + "' source object '" + binding.sourceObjectId + "' was not found."});
        return true;
    }
    if (!eventBindingTargetExistsAlg(doc, binding)) {
        report.diagnostics.push_back({EventBindingDiagnosticKind::TargetMissing, binding.id,
            "Binding '" + binding.id + "' target '" + binding.targetId + "' was not found."});
        return true;
    }
    if (report.dispatches.size() >= budget) {
        report.diagnostics.push_back({EventBindingDiagnosticKind::BudgetExceeded, binding.id,
            "Event binding dispatch budget was exhausted; remaining bindings were skipped."});
        return false;
    }

    report.dispatches.push_back({binding.id, binding.sourceObjectId, binding.event,
                                 binding.targetType, binding.targetId});
    if (binding.once) runtime.oneShotFired.insert(binding.id);
    if (std::isfinite(binding.cooldown) && binding.cooldown > 0.0f)
        runtime.cooldownRemaining[binding.id] = binding.cooldown;
    return true;
}

struct DispatchScope {
    EventBindingRuntimeState& runtime;
    explicit DispatchScope(EventBindingRuntimeState& state) : runtime(state) { runtime.dispatchActive = true; }
    ~DispatchScope() { runtime.dispatchActive = false; }
};

inline EventBindingDispatchReport rejectRecursiveDispatchAlg(const std::string& id = {}) {
    EventBindingDispatchReport report;
    report.diagnostics.push_back({EventBindingDiagnosticKind::RecursionRejected, id,
        "Event binding dispatch was rejected while another dispatch is active."});
    return report;
}

} // namespace Detail

// Dry-run an authored enter/exit/click/timer event.  The result describes
// what would be invoked; no trigger steps or state overrides are executed.
inline EventBindingDispatchReport dispatchEventBindingsAlg(
    const Mc3::Mc3Document& doc, EventBindingRuntimeState& runtime,
    const std::string& sourceObjectId, Mc3::EventBindingEvent event,
    std::size_t executionBudget = 32) {
    if (runtime.dispatchActive) return Detail::rejectRecursiveDispatchAlg();
    Detail::DispatchScope scope(runtime);
    EventBindingDispatchReport report;
    for (const auto& binding : doc.eventBindings) {
        if (binding.sourceObjectId != sourceObjectId || binding.event != event) continue;
        if (!Detail::dispatchOneEventBindingAlg(doc, runtime, binding, executionBudget, report)) break;
    }
    return report;
}

// Advance only the preview runtime state and dry-run due timer bindings.
// This is bounded by one dispatch attempt per binding per call; a very large
// frame delta therefore cannot turn into an unbounded catch-up loop.
inline EventBindingDispatchReport advanceEventBindingRuntimeAlg(
    const Mc3::Mc3Document& doc, EventBindingRuntimeState& runtime, float elapsedSeconds,
    std::size_t executionBudget = 32) {
    if (runtime.dispatchActive) return Detail::rejectRecursiveDispatchAlg();
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f) return {};
    Detail::DispatchScope scope(runtime);
    EventBindingDispatchReport report;

    for (auto it = runtime.cooldownRemaining.begin(); it != runtime.cooldownRemaining.end();) {
        it->second = std::max(0.0f, it->second - elapsedSeconds);
        if (it->second == 0.0f) it = runtime.cooldownRemaining.erase(it);
        else ++it;
    }

    for (const auto& binding : doc.eventBindings) {
        if (binding.event != Mc3::EventBindingEvent::Timer) continue;
        const float interval = std::isfinite(binding.interval) && binding.interval > 0.0f
            ? binding.interval : 1.0f;
        float& elapsed = runtime.timerElapsed[binding.id];
        elapsed += elapsedSeconds;
        if (elapsed < interval) continue;
        elapsed = std::fmod(elapsed, interval);
        if (!Detail::dispatchOneEventBindingAlg(doc, runtime, binding, executionBudget, report)) break;
    }
    return report;
}

} // namespace MeshCraft::Editor
