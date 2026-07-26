#pragma once

#include "MeshCraft/Editor/LuaScriptRunner.hpp"
#include "MeshCraft/EditorCommandAlgorithms.hpp"
#include "MeshCraft/EditorSelectionAlgorithms.hpp"
#include "MeshCraft/EventBindingAlgorithms.hpp"
#include "MeshCraft/Mc3/Mc3Validation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace MeshCraft::Editor {

// An effect is deliberately recorded rather than played by the CNA-free
// runner.  The editor applies these only after the associated document
// transaction commits, so a later failed script cannot leave playback running
// for an event that was rolled back.
enum class EventPreviewEffectType { PlayAction, PlaySound, PlayMusic };

struct EventPreviewEffect {
    EventPreviewEffectType type;
    std::string ref;
};

struct EventPreviewExecutionReport {
    EventBindingDispatchReport dispatch;
    std::vector<EventPreviewEffect> effects;
    std::vector<std::string> errors;
    std::size_t appliedStateOverrides{0};
    std::size_t skippedTriggerSteps{0};
    bool documentCommitted{false};
};

namespace Detail {

inline Mc3::Mc3Object* findEventPreviewObjectById(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>& objects, const std::string& id,
    int depth = 0)
{
    if (depth > 256) throw std::runtime_error("event preview object nesting exceeds 256 levels");
    for (auto& object : objects) {
        if (!object) throw std::runtime_error("event preview encountered a null object");
        if (object->id == id) return object.get();
        if (auto* found = findEventPreviewObjectById(object->children, id, depth + 1)) return found;
    }
    return nullptr;
}

inline Mc3::Mc3Document deepCopyEventPreviewDocument(const Mc3::Mc3Document& source)
{
    Mc3::Mc3Document copy = source;
    copy.objects.clear();
    for (const auto& object : source.objects) {
        if (!object) throw std::runtime_error("event preview encountered a null root object");
        copy.objects.push_back(deepCopyObjectAlg(*object));
    }
    copy.definitions.clear();
    for (const auto& [id, object] : source.definitions) {
        if (!object) throw std::runtime_error("event preview definition '" + id + "' is null");
        copy.definitions[id] = deepCopyObjectAlg(*object);
    }
    return copy;
}

inline void requireFiniteEventPreviewValue(const std::array<float, 3>& values,
                                           const char* field)
{
    for (const float value : values) {
        if (!std::isfinite(value))
            throw std::runtime_error(std::string("event preview rejected non-finite ") + field);
    }
}

inline std::size_t applyEventPreviewState(Mc3::Mc3Document& document,
                                          const Mc3::Mc3SceneState& state)
{
    std::size_t applied = 0;
    for (const auto& override : state.overrides) {
        Mc3::Mc3Object* object = findEventPreviewObjectById(document.objects, override.id);
        if (!object) continue;
        if (override.position) requireFiniteEventPreviewValue(*override.position, "state position");
        if (override.rotation) requireFiniteEventPreviewValue(*override.rotation, "state rotation");
        if (override.material && !override.material->empty() &&
            !document.materials.contains(*override.material)) {
            throw std::runtime_error("event preview rejected unknown state material: " +
                                     *override.material);
        }
        if (override.visible) object->visible = *override.visible;
        if (override.position) object->transform.position = *override.position;
        if (override.rotation) object->transform.rotation = *override.rotation;
        if (override.material) object->material = *override.material;
        ++applied;
    }
    return applied;
}

inline void validateEventPreviewDocument(const Mc3::Mc3Document& document)
{
    // LuaScriptRunner performs the same final format validation for each
    // individual script.  A preview batch can additionally apply states, so
    // repeat the MC3 whole-document boundary before publishing the batch.
    Mc3::Mc3Validation validation;
    document.validate(validation);
    for (const auto& entry : validation.entries) {
        if (entry.severity == Mc3::Mc3ValidationSeverity::Error)
            throw std::runtime_error("event preview validation rejected result: " + entry.message);
    }
}

inline bool pointInsideEventPreviewAabb(const std::array<float, 3>& point,
                                        const std::array<float, 3>& minimum,
                                        const std::array<float, 3>& maximum)
{
    return point[0] >= minimum[0] && point[0] <= maximum[0] &&
           point[1] >= minimum[1] && point[1] <= maximum[1] &&
           point[2] >= minimum[2] && point[2] <= maximum[2];
}

inline void collectEventPreviewAreas(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& objects,
    const ObjectAffineTransformAlg& parentTransform, std::string_view rotationUnits,
    std::string_view eulerOrder, const std::array<float, 3>& playerPosition,
    std::set<std::string>& containedIds, int depth = 0)
{
    if (depth > 256) throw std::runtime_error("event preview area nesting exceeds 256 levels");
    for (const auto& object : objects) {
        if (!object) throw std::runtime_error("event preview encountered a null object");
        const auto worldTransform = composeObjectTransformsAlg(
            localObjectTransformAlg(object->transform, rotationUnits, eulerOrder), parentTransform);
        if (object->type == Mc3::ObjectType::Area && !object->id.empty()) {
            std::array<float, 3> minimum, maximum;
            objectAABBAlg(*object, worldTransform, minimum, maximum);
            if (pointInsideEventPreviewAabb(playerPosition, minimum, maximum))
                containedIds.insert(object->id);
        }
        collectEventPreviewAreas(object->children, worldTransform, rotationUnits, eulerOrder,
                                 playerPosition, containedIds, depth + 1);
    }
}

inline void appendEventPreviewDispatch(EventBindingDispatchReport& destination,
                                       EventBindingDispatchReport&& source)
{
    destination.dispatches.insert(destination.dispatches.end(),
                                  std::make_move_iterator(source.dispatches.begin()),
                                  std::make_move_iterator(source.dispatches.end()));
    destination.diagnostics.insert(destination.diagnostics.end(),
                                   std::make_move_iterator(source.diagnostics.begin()),
                                   std::make_move_iterator(source.diagnostics.end()));
}

} // namespace Detail

// A short-lived, explicit Preview/Play execution environment.  Authoring
// normally leaves this class unused; entering preview is the only route that
// can generate Area enter/exit, click, or timer dispatches.
class EventPreviewRunner {
public:
    static constexpr std::size_t defaultDispatchBudget{32};

    void reset() {
        bindingRuntime_.reset();
        containedAreaIds_.clear();
        dispatchCheckpoint_.reset();
    }

    [[nodiscard]] const EventBindingRuntimeState& bindingRuntime() const { return bindingRuntime_; }

    void clearAreaMembership() { containedAreaIds_.clear(); }

    EventBindingDispatchReport dispatchEvent(const Mc3::Mc3Document& document,
                                             const std::string& sourceObjectId,
                                             Mc3::EventBindingEvent event,
                                             std::size_t budget = defaultDispatchBudget)
    {
        const RuntimeCheckpoint checkpoint{bindingRuntime_, containedAreaIds_};
        auto result = dispatchEventBindingsAlg(document, bindingRuntime_, sourceObjectId, event, budget);
        rememberDispatchCheckpoint(checkpoint, result);
        return result;
    }

    EventBindingDispatchReport dispatchClick(const Mc3::Mc3Document& document,
                                             const std::string& sourceObjectId,
                                             std::size_t budget = defaultDispatchBudget)
    {
        return dispatchEvent(document, sourceObjectId, Mc3::EventBindingEvent::Click, budget);
    }

    EventBindingDispatchReport advanceTimers(const Mc3::Mc3Document& document, float elapsedSeconds,
                                             std::size_t budget = defaultDispatchBudget)
    {
        const RuntimeCheckpoint checkpoint{bindingRuntime_, containedAreaIds_};
        auto result = advanceEventBindingRuntimeAlg(document, bindingRuntime_, elapsedSeconds, budget);
        rememberDispatchCheckpoint(checkpoint, result);
        return result;
    }

    // `playerPosition` is in the document's authored coordinate system.  The
    // full parent transform, pivot, units, and Euler convention are reused
    // from the shared picker bounds algorithm, so Areas agree with viewport
    // interaction even in radians/non-XYZ documents.
    EventBindingDispatchReport updateAreaTransitions(
        const Mc3::Mc3Document& document, const std::array<float, 3>& playerPosition,
        std::size_t budget = defaultDispatchBudget)
    {
        EventBindingDispatchReport result;
        const RuntimeCheckpoint checkpoint{bindingRuntime_, containedAreaIds_};
        std::set<std::string> nowContained;
        Detail::collectEventPreviewAreas(document.objects, {}, document.rotationUnits,
                                         document.eulerOrder, playerPosition, nowContained);

        const auto append = [&](const std::string& id, Mc3::EventBindingEvent event) {
            const std::size_t remaining = result.dispatches.size() < budget
                ? budget - result.dispatches.size() : 0;
            Detail::appendEventPreviewDispatch(
                result, dispatchEventBindingsAlg(document, bindingRuntime_, id, event, remaining));
        };
        for (const auto& id : nowContained)
            if (!containedAreaIds_.contains(id)) append(id, Mc3::EventBindingEvent::Enter);
        for (const auto& id : containedAreaIds_)
            if (!nowContained.contains(id)) append(id, Mc3::EventBindingEvent::Exit);
        containedAreaIds_ = std::move(nowContained);
        rememberDispatchCheckpoint(checkpoint, result);
        return result;
    }

    // Execute a previously eligible dispatch list against a deep copy.  State
    // application and each Lua execution stay inside that copy.  An error in
    // any target, especially a script after an earlier state mutation, drops
    // the complete batch; no effect is returned and `document` is untouched.
    EventPreviewExecutionReport execute(Mc3::Mc3Document& document,
                                        EventBindingDispatchReport dispatch,
                                        const std::function<void()>& beforeCommit = {})
    {
        EventPreviewExecutionReport result;
        result.dispatch = std::move(dispatch);
        if (result.dispatch.dispatches.empty()) return result;

        try {
            Mc3::Mc3Document working = Detail::deepCopyEventPreviewDocument(document);
            bool documentChanged = false;

            for (const auto& event : result.dispatch.dispatches) {
                if (event.targetType == Mc3::EventBindingTarget::SceneState) {
                    const auto state = working.sceneStates.find(event.targetId);
                    if (state == working.sceneStates.end())
                        throw std::runtime_error("event preview state target disappeared: " + event.targetId);
                    const std::size_t applied = Detail::applyEventPreviewState(working, state->second);
                    result.appliedStateOverrides += applied;
                    documentChanged |= applied != 0;
                    continue;
                }

                const auto trigger = working.triggers.find(event.targetId);
                if (trigger == working.triggers.end())
                    throw std::runtime_error("event preview trigger target disappeared: " + event.targetId);
                // LuaScriptRunner publishes its own isolated script result by
                // swapping `working`. Keep the authored step list by value so
                // a RunScript in the middle cannot leave this loop holding an
                // iterator/reference into the replaced document graph.
                const std::string triggerId = trigger->second.id;
                const auto steps = trigger->second.steps;
                Mc3::Mc3Object* scriptTarget = Detail::findEventPreviewObjectById(
                    working.objects, event.sourceObjectId);
                if (!scriptTarget)
                    throw std::runtime_error("event preview source object disappeared: " + event.sourceObjectId);

                for (const auto& step : steps) {
                    switch (step.type) {
                    case Mc3::TriggerStepType::PlayAction:
                        if (working.actions.contains(step.ref))
                            result.effects.push_back({EventPreviewEffectType::PlayAction, step.ref});
                        else ++result.skippedTriggerSteps;
                        break;
                    case Mc3::TriggerStepType::PlaySound:
                        if (working.sounds.contains(step.ref))
                            result.effects.push_back({EventPreviewEffectType::PlaySound, step.ref});
                        else ++result.skippedTriggerSteps;
                        break;
                    case Mc3::TriggerStepType::PlayMusic:
                        if (working.musicTracks.contains(step.ref))
                            result.effects.push_back({EventPreviewEffectType::PlayMusic, step.ref});
                        else ++result.skippedTriggerSteps;
                        break;
                    case Mc3::TriggerStepType::RunScript: {
                        const auto script = working.scripts.find(step.ref);
                        if (script == working.scripts.end()) {
                            ++result.skippedTriggerSteps;
                            break;
                        }
                        // Copy the source out by value BEFORE calling run():
                        // LuaScriptRunner atomically replaces `working` via
                        // move-assignment on success, so `script` (an
                        // iterator into working.scripts) is dangling the
                        // instant run() returns -- same hazard the
                        // scriptTarget re-lookup below already guards
                        // against, just missed here (confirmed by ASan:
                        // heap-use-after-free reading script->second.source
                        // after this exact call, SYS-W11-09).
                        const std::string scriptSource = script->second.source;
                        const std::string error = scriptRunner_.run(scriptSource, working,
                                                                    scriptTarget);
                        if (!error.empty())
                            throw std::runtime_error("trigger '" + triggerId +
                                                     "' script '" + step.ref + "' failed: " + error);
                        documentChanged |= !scriptSource.empty();
                        // LuaScriptRunner atomically replaced `working`, so a
                        // pointer into its old object graph must not be reused
                        // by a later RunScript step in this same trigger.
                        scriptTarget = Detail::findEventPreviewObjectById(
                            working.objects, event.sourceObjectId);
                        if (!scriptTarget)
                            throw std::runtime_error("event preview script removed its source object");
                        break;
                    }
                    }
                }
            }

            if (documentChanged) {
                Detail::validateEventPreviewDocument(working);
                if (beforeCommit) beforeCommit();
                document = std::move(working);
                result.documentCommitted = true;
            }
            dispatchCheckpoint_.reset();
            return result;
        } catch (const std::exception& error) {
            if (dispatchCheckpoint_) {
                bindingRuntime_ = std::move(dispatchCheckpoint_->bindingRuntime);
                containedAreaIds_ = std::move(dispatchCheckpoint_->containedAreaIds);
                dispatchCheckpoint_.reset();
            }
            result.effects.clear();
            result.errors.push_back(error.what());
            return result;
        }
    }

private:
    struct RuntimeCheckpoint {
        EventBindingRuntimeState bindingRuntime;
        std::set<std::string> containedAreaIds;
    };

    void rememberDispatchCheckpoint(const RuntimeCheckpoint& checkpoint,
                                    const EventBindingDispatchReport& report)
    {
        if (!report.dispatches.empty()) dispatchCheckpoint_ = checkpoint;
    }

    EventBindingRuntimeState bindingRuntime_;
    std::set<std::string> containedAreaIds_;
    std::optional<RuntimeCheckpoint> dispatchCheckpoint_;
    LuaScriptRunner scriptRunner_;
};

} // namespace MeshCraft::Editor
