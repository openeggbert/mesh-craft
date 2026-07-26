#include "MeshCraft/Application/UI/AutomationWorkspace.hpp"

#include "MeshCraft/EditorAlgorithms.hpp"
#include "MeshCraft/EventBindingAlgorithms.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace MeshCraft::Application::UI {

void AutomationWorkspacePanel::drawScripts(AutomationWorkspace& workspace,
                                            const AutomationWorkspaceScriptFrame& frame)
{
    if (!workspace.selectedScriptKey.empty() &&
        !frame.document.scripts.count(workspace.selectedScriptKey)) {
        workspace.selectedScriptKey.clear();
    }

    if (ImGui::SmallButton("+##scriptadd")) {
        frame.pushUndo();
        int n = 1;
        std::string key;
        do { key = "script_" + std::to_string(n++); }
        while (frame.document.scripts.count(key));
        Mc3::Mc3Script script;
        script.id = key;
        script.type = "lua";
        frame.document.scripts[key] = script;
        workspace.selectedScriptKey = key;
        frame.markModified();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add script");
    ImGui::SameLine();
    if (ImGui::SmallButton("-##scriptremove") && !workspace.selectedScriptKey.empty()) {
        frame.pushUndo();
        const int cleared = clearScriptReferencesAlg(frame.document, workspace.selectedScriptKey);
        frame.document.scripts.erase(workspace.selectedScriptKey);
        workspace.selectedScriptKey.clear();
        frame.markModified();
        if (cleared > 0) {
            frame.reportStatus("Script deleted (cleared " + std::to_string(cleared) +
                               " reference" + (cleared == 1 ? "" : "s") + ")", false);
        }
    }

    ImGui::Separator();
    for (const auto& [key, script] : frame.document.scripts) {
        const bool selected = key == workspace.selectedScriptKey;
        const std::string label = key + "  (" + (script.type.empty() ? "lua" : script.type) + ")";
        ImGui::PushID(("script_" + key).c_str());
        if (ImGui::Selectable(label.c_str(), selected)) workspace.selectedScriptKey = key;
        ImGui::PopID();
    }

    if (workspace.selectedScriptKey.empty() ||
        !frame.document.scripts.count(workspace.selectedScriptKey)) return;

    auto& script = frame.document.scripts[workspace.selectedScriptKey];
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("ID");
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy##scriptid"))
        ImGui::SetClipboardText(workspace.selectedScriptKey.c_str());
    ImGui::TextUnformatted(workspace.selectedScriptKey.c_str());

    ImGui::TextDisabled("Type");
    {
        char buf[64];
        std::strncpy(buf, script.type.c_str(), sizeof(buf) - 1); buf[63] = '\0';
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##scripttype", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            frame.pushUndo(); script.type = buf; frame.markModified();
        }
    }

    ImGui::TextDisabled("Source (no syntax highlighting)");
    {
        char buf[16384];
        std::strncpy(buf, script.source.c_str(), sizeof(buf) - 1); buf[16383] = '\0';
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextMultiline("##scriptsource", buf, sizeof(buf), ImVec2(-1, 240),
                                      ImGuiInputTextFlags_EnterReturnsTrue)) {
            frame.pushUndo(); script.source = buf; frame.markModified();
        }
    }

    if (ImGui::Button("\xe2\x96\xb6 Run Script", ImVec2(-1, 0))) {
        const auto selectionIds = frame.currentSelectionIds();
        const std::string error = workspace.runScript(
            script.source, frame.document, frame.selectedTarget(), frame.pushUndo);
        if (error.empty()) {
            frame.invalidateObjectIndex();
            frame.restoreSelection(selectionIds);
            frame.markModified();
            frame.reportStatus("Script '" + workspace.selectedScriptKey + "' ran successfully", false);
        } else {
            frame.reportStatus("Script '" + workspace.selectedScriptKey + "' failed: " + error, true);
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Runs this script's source now, against the current "
                           "selection (if any) as its 'def' target");
}

void AutomationWorkspacePanel::drawStates(AutomationWorkspace& workspace,
                                           const AutomationWorkspaceStateFrame& frame)
{
    if (!workspace.selectedSceneStateKey.empty() &&
        !frame.document.sceneStates.count(workspace.selectedSceneStateKey)) {
        workspace.selectedSceneStateKey.clear();
    }

    if (ImGui::SmallButton("+##stateadd")) {
        frame.pushUndo();
        int n = 1;
        std::string key;
        do { key = "state_" + std::to_string(n++); }
        while (frame.document.sceneStates.count(key));
        Mc3::Mc3SceneState state;
        state.name = key;
        frame.document.sceneStates[key] = state;
        workspace.selectedSceneStateKey = key;
        frame.markModified();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add scene state");
    ImGui::SameLine();
    if (ImGui::SmallButton("-##stateremove") && !workspace.selectedSceneStateKey.empty()) {
        frame.pushUndo();
        frame.document.sceneStates.erase(workspace.selectedSceneStateKey);
        workspace.selectedSceneStateKey.clear();
        frame.markModified();
    }

    ImGui::Separator();
    for (const auto& [key, state] : frame.document.sceneStates) {
        const bool selected = key == workspace.selectedSceneStateKey;
        const std::string label = key + "  (" + std::to_string(state.overrides.size()) + " override" +
            (state.overrides.size() == 1 ? "" : "s") + ")";
        ImGui::PushID(("state_" + key).c_str());
        if (ImGui::Selectable(label.c_str(), selected)) workspace.selectedSceneStateKey = key;
        ImGui::PopID();
    }

    if (workspace.selectedSceneStateKey.empty() ||
        !frame.document.sceneStates.count(workspace.selectedSceneStateKey)) return;

    auto& state = frame.document.sceneStates[workspace.selectedSceneStateKey];
    ImGui::Spacing();
    ImGui::TextDisabled("Name");
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy##stateid"))
        ImGui::SetClipboardText(workspace.selectedSceneStateKey.c_str());
    ImGui::TextUnformatted(workspace.selectedSceneStateKey.c_str());

    if (ImGui::Button("\xe2\x96\xb6 Apply State", ImVec2(-1, 0))) {
        bool anyResolvable = false;
        for (const auto& override : state.overrides) {
            if (frame.findObjectById(override.id)) { anyResolvable = true; break; }
        }
        if (anyResolvable) frame.pushUndo();
        const auto result = workspace.applySceneState(state, frame.findObjectById);
        if (result.applied > 0) frame.markModified();

        std::string message = "State '" + workspace.selectedSceneStateKey + "' applied: " +
            std::to_string(result.applied) + " object" +
            (result.applied == 1 ? "" : "s") + " updated";
        if (result.missing > 0)
            message += ", " + std::to_string(result.missing) + " skipped (object id not found)";
        frame.reportStatus(std::move(message), result.missing > 0);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Writes this state's overrides onto the matching live "
                           "objects (by id), right now");

    ImGui::Spacing();
    ImGui::TextDisabled("Object Overrides");
    int removeIndex = -1;
    for (size_t i = 0; i < state.overrides.size(); ++i) {
        auto& override = state.overrides[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Separator();

        char idBuffer[128];
        std::strncpy(idBuffer, override.id.c_str(), sizeof(idBuffer) - 1); idBuffer[127] = '\0';
        ImGui::TextDisabled("Object ID");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-40);
        if (ImGui::InputText("##ovid", idBuffer, sizeof(idBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            frame.pushUndo(); override.id = idBuffer; frame.markModified();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x##ovrm")) removeIndex = static_cast<int>(i);

        {
            bool has = override.visible.has_value();
            if (ImGui::Checkbox("Override Visible##ovvis", &has)) {
                frame.pushUndo();
                override.visible = has ? std::optional<bool>(true) : std::nullopt;
                frame.markModified();
            }
            if (override.visible) {
                ImGui::SameLine();
                bool visible = *override.visible;
                if (ImGui::Checkbox("Value##ovvisval", &visible)) {
                    frame.pushUndo(); override.visible = visible; frame.markModified();
                }
            }
        }

        {
            bool has = override.position.has_value();
            if (ImGui::Checkbox("Override Position##ovpos", &has)) {
                frame.pushUndo();
                override.position = has ? std::optional<std::array<float, 3>>({0, 0, 0}) : std::nullopt;
                frame.markModified();
            }
            if (override.position) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(180);
                const bool changed = ImGui::DragFloat3("##ovposval", override.position->data(), 0.01f);
                if (ImGui::IsItemActivated()) frame.pushUndo();
                if (changed) frame.markModified();
            }
        }

        {
            bool has = override.rotation.has_value();
            if (ImGui::Checkbox("Override Rotation##ovrot", &has)) {
                frame.pushUndo();
                override.rotation = has ? std::optional<std::array<float, 3>>({0, 0, 0}) : std::nullopt;
                frame.markModified();
            }
            if (override.rotation) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(180);
                const bool changed = ImGui::DragFloat3("##ovrotval", override.rotation->data(), 0.5f);
                if (ImGui::IsItemActivated()) frame.pushUndo();
                if (changed) frame.markModified();
            }
        }

        {
            bool has = override.material.has_value();
            if (ImGui::Checkbox("Override Material##ovmat", &has)) {
                frame.pushUndo();
                override.material = has ? std::optional<std::string>("") : std::nullopt;
                frame.markModified();
            }
            if (override.material) {
                ImGui::SameLine();
                char materialBuffer[128];
                std::strncpy(materialBuffer, override.material->c_str(), sizeof(materialBuffer) - 1);
                materialBuffer[127] = '\0';
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##ovmatval", materialBuffer, sizeof(materialBuffer),
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
                    frame.pushUndo(); override.material = std::string(materialBuffer); frame.markModified();
                }
            }
        }
        ImGui::PopID();
    }
    if (removeIndex >= 0) {
        frame.pushUndo();
        state.overrides.erase(state.overrides.begin() + removeIndex);
        frame.markModified();
    }
    ImGui::Separator();
    if (ImGui::SmallButton("+ Add Override")) {
        frame.pushUndo();
        state.overrides.push_back(Mc3::Mc3ObjectOverride{});
        frame.markModified();
    }
}

void AutomationWorkspacePanel::drawTriggers(AutomationWorkspace& workspace,
                                             const AutomationWorkspaceTriggerFrame& frame)
{
    if (!workspace.selectedTriggerKey.empty() &&
        !frame.document.triggers.count(workspace.selectedTriggerKey)) {
        workspace.selectedTriggerKey.clear();
    }

    const auto resolveSource = [&frame](const std::string& source) {
        if (source.empty()) return source;
        const std::filesystem::path path(source);
        return path.is_absolute() ? path.string() : (frame.document.sourcePath / path).string();
    };
    const auto fireTrigger = [&workspace, &frame, &resolveSource](const Mc3::Mc3Trigger& trigger) {
        // LuaScriptRunner commits by swapping the document. Never retain a
        // reference into that graph across a RunScript step.
        const std::string triggerId = trigger.id;
        const auto steps = trigger.steps;
        int fired = 0, missing = 0, scriptErrors = 0;
        std::string lastScriptError;
        bool scriptCommitted = false;
        const auto selectionIds = frame.currentSelectionIds();
        const auto beforeScriptCommit = [&frame, &scriptCommitted] {
            if (!scriptCommitted) {
                frame.pushUndo();
                scriptCommitted = true;
            }
        };

        for (const auto& step : steps) {
            switch (step.type) {
            case Mc3::TriggerStepType::PlayAction:
                if (frame.document.actions.count(step.ref)) {
                    frame.playAction(step.ref);
                    ++fired;
                } else ++missing;
                break;
            case Mc3::TriggerStepType::PlaySound:
                if (const auto sound = frame.document.sounds.find(step.ref);
                    sound != frame.document.sounds.end()) {
                    frame.playAudio(step.ref, resolveSource(sound->second.src), sound->second.loop);
                    ++fired;
                } else ++missing;
                break;
            case Mc3::TriggerStepType::PlayMusic:
                if (const auto music = frame.document.musicTracks.find(step.ref);
                    music != frame.document.musicTracks.end()) {
                    frame.playAudio(step.ref, resolveSource(music->second.src), music->second.loop);
                    ++fired;
                } else ++missing;
                break;
            case Mc3::TriggerStepType::RunScript:
                if (const auto script = frame.document.scripts.find(step.ref);
                    script != frame.document.scripts.end()) {
                    const std::string source = script->second.source;
                    const std::string error = workspace.runScript(
                        source, frame.document, frame.selectedTarget(), beforeScriptCommit);
                    if (error.empty()) {
                        ++fired;
                        if (scriptCommitted) {
                            frame.invalidateObjectIndex();
                            frame.restoreSelection(selectionIds);
                        }
                    } else {
                        ++scriptErrors;
                        lastScriptError = error;
                    }
                } else ++missing;
                break;
            }
        }
        std::string message = "Trigger '" + triggerId + "' fired: " +
            std::to_string(fired) + " step" + (fired == 1 ? "" : "s") + " ran";
        if (missing > 0)
            message += ", " + std::to_string(missing) + " skipped (ref not found)";
        if (scriptErrors > 0)
            message += ", " + std::to_string(scriptErrors) + " script error" +
                (scriptErrors == 1 ? "" : "s") + " (" + lastScriptError + ")";
        if (scriptCommitted) frame.markModified();
        frame.reportStatus(std::move(message), missing > 0 || scriptErrors > 0);
    };

    if (ImGui::SmallButton("+##triggeradd")) {
        frame.pushUndo();
        int n = 1;
        std::string key;
        do { key = "trigger_" + std::to_string(n++); }
        while (frame.document.triggers.count(key));
        Mc3::Mc3Trigger trigger;
        trigger.id = key;
        frame.document.triggers[key] = trigger;
        workspace.selectedTriggerKey = key;
        frame.markModified();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add trigger");
    ImGui::SameLine();
    if (ImGui::SmallButton("-##triggerremove") && !workspace.selectedTriggerKey.empty()) {
        frame.pushUndo();
        frame.document.triggers.erase(workspace.selectedTriggerKey);
        workspace.selectedTriggerKey.clear();
        frame.markModified();
    }

    ImGui::Separator();
    for (const auto& [key, trigger] : frame.document.triggers) {
        const bool selected = key == workspace.selectedTriggerKey;
        const std::string label = key + "  (" + std::to_string(trigger.steps.size()) + " step" +
            (trigger.steps.size() == 1 ? "" : "s") + ")";
        ImGui::PushID(("trigger_" + key).c_str());
        if (ImGui::SmallButton("▶")) fireTrigger(trigger);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fire this trigger now");
        ImGui::SameLine();
        if (ImGui::Selectable(label.c_str(), selected)) workspace.selectedTriggerKey = key;
        ImGui::PopID();
    }

    if (workspace.selectedTriggerKey.empty() ||
        !frame.document.triggers.count(workspace.selectedTriggerKey)) return;
    auto& trigger = frame.document.triggers[workspace.selectedTriggerKey];
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("ID");
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy##triggerid"))
        ImGui::SetClipboardText(workspace.selectedTriggerKey.c_str());
    ImGui::TextUnformatted(workspace.selectedTriggerKey.c_str());

    if (ImGui::Button("▶ Fire Trigger", ImVec2(-1, 0))) fireTrigger(trigger);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Executes every step below, right now, in order "
                           "(manual/on-demand firing)");
    ImGui::Spacing();
    ImGui::TextDisabled("Steps");
    static const char* kStepTypeNames[] = {"Play Action", "Play Sound", "Run Script", "Play Music"};
    int removeIndex = -1;
    for (size_t i = 0; i < trigger.steps.size(); ++i) {
        auto& step = trigger.steps[i];
        ImGui::PushID(static_cast<int>(i));
        int typeIndex = static_cast<int>(step.type);
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo("##steptype", &typeIndex, kStepTypeNames, 4)) {
            frame.pushUndo();
            step.type = static_cast<Mc3::TriggerStepType>(typeIndex);
            frame.markModified();
        }
        ImGui::SameLine();
        char buffer[256];
        std::strncpy(buffer, step.ref.c_str(), sizeof(buffer) - 1); buffer[255] = '\0';
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputText("##stepref", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            frame.pushUndo(); step.ref = buffer; frame.markModified();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) removeIndex = static_cast<int>(i);
        ImGui::PopID();
    }
    if (removeIndex >= 0) {
        frame.pushUndo();
        trigger.steps.erase(trigger.steps.begin() + removeIndex);
        frame.markModified();
    }
    if (ImGui::SmallButton("+ Add Step")) {
        frame.pushUndo();
        trigger.steps.push_back(Mc3::Mc3TriggerStep{});
        frame.markModified();
    }
}

void AutomationWorkspacePanel::drawEvents(AutomationWorkspace& workspace,
                                           const AutomationWorkspaceEventFrame& frame)
{
    ImGui::TextDisabled("Bind an object or Area event to a named trigger or state.");
    ImGui::TextDisabled("Normal editing never dispatches events. Preview/Play commits only validated batches.");

    bool preview = workspace.previewEnabled;
    if (ImGui::Checkbox("Preview / Play events", &preview))
        workspace.setPreviewEnabled(preview);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Enables timer, Area enter/exit (in Walk Mode), and picked-object click events. "
                           "Every batch either commits once or rolls back completely.");

    if (ImGui::SmallButton("+ Add Binding")) {
        frame.pushUndo();
        Mc3::Mc3EventBinding binding;
        int n = 1;
        do {
            binding.id = "event_" + std::to_string(n++);
        } while (std::any_of(frame.document.eventBindings.begin(), frame.document.eventBindings.end(),
                             [&binding](const Mc3::Mc3EventBinding& other) {
                                 return other.id == binding.id;
                             }));
        binding.sourceObjectId = frame.selectedObjectId();
        if (!frame.document.triggers.empty()) binding.targetId = frame.document.triggers.begin()->first;
        else if (!frame.document.sceneStates.empty()) {
            binding.targetType = Mc3::EventBindingTarget::SceneState;
            binding.targetId = frame.document.sceneStates.begin()->first;
        }
        frame.document.eventBindings.push_back(std::move(binding));
        workspace.selectedEventBindingIndex = static_cast<int>(frame.document.eventBindings.size()) - 1;
        workspace.resetPreviewRuntime();
        frame.markModified();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("- Remove Binding") && workspace.selectedEventBindingIndex >= 0 &&
        workspace.selectedEventBindingIndex < static_cast<int>(frame.document.eventBindings.size())) {
        frame.pushUndo();
        frame.document.eventBindings.erase(
            frame.document.eventBindings.begin() + workspace.selectedEventBindingIndex);
        workspace.selectedEventBindingIndex = std::min(
            workspace.selectedEventBindingIndex,
            static_cast<int>(frame.document.eventBindings.size()) - 1);
        workspace.resetPreviewRuntime();
        frame.markModified();
    }

    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(frame.document.eventBindings.size()); ++i) {
        const auto& binding = frame.document.eventBindings[static_cast<size_t>(i)];
        const char* eventName = binding.event == Mc3::EventBindingEvent::Enter ? "enter" :
            binding.event == Mc3::EventBindingEvent::Exit ? "exit" :
            binding.event == Mc3::EventBindingEvent::Click ? "click" : "timer";
        const char* targetName = binding.targetType == Mc3::EventBindingTarget::Trigger ? "trigger" : "state";
        std::string label = binding.id + "  " + eventName + " -> " + targetName + ":" + binding.targetId;
        if (ImGui::Selectable(label.c_str(), workspace.selectedEventBindingIndex == i))
            workspace.selectedEventBindingIndex = i;
    }

    if (workspace.selectedEventBindingIndex >= 0 &&
        workspace.selectedEventBindingIndex < static_cast<int>(frame.document.eventBindings.size())) {
        auto& binding = frame.document.eventBindings[
            static_cast<size_t>(workspace.selectedEventBindingIndex)];
        auto authoredChanged = [&]() {
            workspace.resetPreviewRuntime();
            frame.markModified();
        };

        ImGui::Separator();
        ImGui::PushID(workspace.selectedEventBindingIndex);

        char idBuf[128];
        std::strncpy(idBuf, binding.id.c_str(), sizeof(idBuf) - 1); idBuf[127] = '\0';
        ImGui::TextDisabled("Binding ID");
        ImGui::SetNextItemWidth(-1);
        if (frame.undoOnActivate(ImGui::InputText("##eventid", idBuf, sizeof(idBuf),
                                                  ImGuiInputTextFlags_EnterReturnsTrue))) {
            binding.id = idBuf; authoredChanged();
        }

        char sourceBuf[128];
        std::strncpy(sourceBuf, binding.sourceObjectId.c_str(), sizeof(sourceBuf) - 1); sourceBuf[127] = '\0';
        ImGui::TextDisabled("Source object / Area ID");
        ImGui::SetNextItemWidth(-1);
        if (frame.undoOnActivate(ImGui::InputText("##eventsource", sourceBuf, sizeof(sourceBuf),
                                                  ImGuiInputTextFlags_EnterReturnsTrue))) {
            binding.sourceObjectId = sourceBuf; authoredChanged();
        }
        if (!binding.sourceObjectId.empty() && !frame.findObjectById(binding.sourceObjectId))
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Source is currently missing.");

        static const char* kEvents[] = {"Enter", "Exit", "Click", "Timer"};
        int eventIndex = static_cast<int>(binding.event);
        ImGui::TextDisabled("Event"); ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo("##eventkind", &eventIndex, kEvents, 4)) {
            frame.pushUndo(); binding.event = static_cast<Mc3::EventBindingEvent>(eventIndex); authoredChanged();
        }

        static const char* kTargetTypes[] = {"Trigger", "Scene State"};
        int targetType = static_cast<int>(binding.targetType);
        ImGui::TextDisabled("Target kind"); ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo("##eventtargetkind", &targetType, kTargetTypes, 2)) {
            frame.pushUndo(); binding.targetType = static_cast<Mc3::EventBindingTarget>(targetType); authoredChanged();
        }

        char targetBuf[128];
        std::strncpy(targetBuf, binding.targetId.c_str(), sizeof(targetBuf) - 1); targetBuf[127] = '\0';
        ImGui::TextDisabled("Target ID");
        ImGui::SetNextItemWidth(-1);
        if (frame.undoOnActivate(ImGui::InputText("##eventtarget", targetBuf, sizeof(targetBuf),
                                                  ImGuiInputTextFlags_EnterReturnsTrue))) {
            binding.targetId = targetBuf; authoredChanged();
        }
        if (!Editor::eventBindingTargetExistsAlg(frame.document, binding))
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Target is currently missing.");

        bool enabled = binding.enabled;
        if (ImGui::Checkbox("Enabled", &enabled)) {
            frame.pushUndo(); binding.enabled = enabled; authoredChanged();
        }
        bool oneShot = binding.once;
        if (ImGui::Checkbox("One-shot", &oneShot)) {
            frame.pushUndo(); binding.once = oneShot; authoredChanged();
        }
        float cooldown = binding.cooldown;
        ImGui::SetNextItemWidth(140);
        if (frame.undoOnActivate(ImGui::DragFloat("Cooldown (s)", &cooldown, 0.05f, 0.0f, 3600.0f))) {
            binding.cooldown = cooldown; authoredChanged();
        }
        if (binding.event == Mc3::EventBindingEvent::Timer) {
            float interval = binding.interval;
            ImGui::SetNextItemWidth(140);
            if (frame.undoOnActivate(ImGui::DragFloat("Timer interval (s)", &interval, 0.05f, 0.01f, 3600.0f))) {
                binding.interval = interval; authoredChanged();
            }
        }

        if (!workspace.previewEnabled) ImGui::BeginDisabled();
        if (ImGui::Button("Preview selected event", ImVec2(-1, 0))) {
            frame.executePreview(workspace.previewRunner.dispatchEvent(
                frame.document, binding.sourceObjectId, binding.event));
        }
        if (!workspace.previewEnabled) ImGui::EndDisabled();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(workspace.previewEnabled
                ? "Runs this binding through the same atomic Preview/Play transaction."
                : "Enable Preview / Play events first; normal editing never dispatches bindings.");
        ImGui::PopID();
    }

    if (!workspace.previewReport.dispatches.empty() ||
        !workspace.previewReport.diagnostics.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Preview report");
        for (const auto& dispatch : workspace.previewReport.dispatches) {
            ImGui::Text("Dispatched %s '%s' from binding '%s'.",
                dispatch.targetType == Mc3::EventBindingTarget::Trigger ? "trigger" : "state",
                dispatch.targetId.c_str(), dispatch.bindingId.c_str());
        }
        for (const auto& diagnostic : workspace.previewReport.diagnostics)
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "%s", diagnostic.message.c_str());
        for (const auto& error : workspace.previewErrors)
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", error.c_str());
    }
}

} // namespace MeshCraft::Application::UI
