#pragma once

#include "MeshCraft/Application/AutomationWorkspace.hpp"

#include <functional>
#include <string>
#include <vector>

namespace MeshCraft::Application::UI {

struct AutomationWorkspaceScriptFrame {
    Mc3::Mc3Document& document;
    std::function<void()> pushUndo;
    std::function<void()> markModified;
    std::function<void(std::string, bool)> reportStatus;
    std::function<Mc3::Mc3Object*()> selectedTarget;
    std::function<std::vector<std::string>()> currentSelectionIds;
    std::function<void()> invalidateObjectIndex;
    std::function<void(const std::vector<std::string>&)> restoreSelection;
};

struct AutomationWorkspaceStateFrame {
    Mc3::Mc3Document& document;
    std::function<void()> pushUndo;
    std::function<void()> markModified;
    std::function<void(std::string, bool)> reportStatus;
    AutomationWorkspace::FindObjectById findObjectById;
};

struct AutomationWorkspaceTriggerFrame {
    Mc3::Mc3Document& document;
    std::function<void()> pushUndo;
    std::function<void()> markModified;
    std::function<void(std::string, bool)> reportStatus;
    std::function<Mc3::Mc3Object*()> selectedTarget;
    std::function<std::vector<std::string>()> currentSelectionIds;
    std::function<void()> invalidateObjectIndex;
    std::function<void(const std::vector<std::string>&)> restoreSelection;
    std::function<void(const std::string&)> playAction;
    std::function<void(const std::string&, const std::string&, bool)> playAudio;
};

// Application-owned effects are provided as frame-local callbacks. The Events
// renderer can therefore edit only AutomationWorkspace and authored MC3 data;
// it has no dependency on MeshCraftApplication or a global UI context.
struct AutomationWorkspaceEventFrame {
    Mc3::Mc3Document& document;
    std::function<void()> pushUndo;
    std::function<void()> markModified;
    std::function<bool(bool)> undoOnActivate;
    std::function<std::string()> selectedObjectId;
    AutomationWorkspace::FindObjectById findObjectById;
    std::function<void(Editor::EventBindingDispatchReport)> executePreview;
};

class AutomationWorkspacePanel final {
public:
    static void drawScripts(AutomationWorkspace& workspace,
                            const AutomationWorkspaceScriptFrame& frame);
    static void drawStates(AutomationWorkspace& workspace,
                           const AutomationWorkspaceStateFrame& frame);
    static void drawTriggers(AutomationWorkspace& workspace,
                             const AutomationWorkspaceTriggerFrame& frame);
    static void drawEvents(AutomationWorkspace& workspace,
                           const AutomationWorkspaceEventFrame& frame);
};

} // namespace MeshCraft::Application::UI
