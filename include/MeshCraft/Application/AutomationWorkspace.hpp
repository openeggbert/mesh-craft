#pragma once

#include "MeshCraft/Editor/EventPreviewRunner.hpp"
#include "MeshCraft/Editor/LuaScriptRunner.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"

#include <functional>
#include <string>
#include <vector>

namespace MeshCraft::Application {

// Owns the non-serialized interaction state shared by the Scripts, Triggers,
// States, and Events resource tabs.  The application deliberately supplies
// document lookup and undo/notification hooks at the edge: this workspace
// never reaches back into MeshCraftApplication.
class AutomationWorkspace final {
public:
    struct StateApplyResult {
        int applied{0};
        int missing{0};
    };

    using FindObjectById = std::function<Mc3::Mc3Object*(const std::string&)>;

    // Resource-tab selections survive preview resets and document edits until
    // their respective tab discovers that an authored entry was removed.
    std::string selectedScriptKey;
    std::string selectedTriggerKey;
    std::string selectedSceneStateKey;
    int selectedEventBindingIndex{-1};

    // Preview/Play is transient editor state. It is never serialized and is
    // intentionally reset whenever authored event data changes.
    bool previewEnabled{false};
    Editor::EventPreviewRunner previewRunner;
    Editor::EventBindingDispatchReport previewReport;
    std::vector<std::string> previewErrors;

    void setPreviewEnabled(bool enabled);
    void resetPreview();
    void resetPreviewRuntime();
    void clearPreviewAreaMembership();

    // Keeps the Lua execution boundary with the resource workspace rather
    // than in the application shell. `beforeCommit` is invoked only by the
    // runner after its isolated document has validated successfully.
    std::string runScript(const std::string& source, Mc3::Mc3Document& document,
                          Mc3::Mc3Object* target,
                          const std::function<void()>& beforeCommit = {});

    // Applies authored state overrides through caller-owned object lookup.
    // Undo, dirty-state, selection, and user feedback remain explicit UI
    // concerns; this method only owns the resource-tab mutation semantics.
    [[nodiscard]] StateApplyResult applySceneState(
        const Mc3::Mc3SceneState& state, const FindObjectById& findObject) const;

private:
    Editor::LuaScriptRunner luaScriptRunner_;
};

} // namespace MeshCraft::Application
