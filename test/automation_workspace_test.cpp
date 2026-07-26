#include "MeshCraft/Application/AutomationWorkspace.hpp"

#include <array>
#include <cassert>
#include <memory>
#include <string>

namespace {

std::shared_ptr<MeshCraft::Mc3::Mc3Object> objectWithId(const std::string& id)
{
    auto object = std::make_shared<MeshCraft::Mc3::Mc3Object>();
    object->id = id;
    return object;
}

} // namespace

int main()
{
    using MeshCraft::Application::AutomationWorkspace;
    using namespace MeshCraft::Mc3;

    AutomationWorkspace workspace;
    workspace.selectedScriptKey = "startup";
    workspace.selectedTriggerKey = "intro";
    workspace.selectedSceneStateKey = "night";
    workspace.selectedEventBindingIndex = 3;

    workspace.setPreviewEnabled(true);
    assert(workspace.previewEnabled);
    workspace.previewErrors.push_back("old error");
    workspace.resetPreviewRuntime();
    assert(workspace.previewEnabled);
    assert(workspace.previewErrors.empty());
    assert(workspace.selectedScriptKey == "startup");
    assert(workspace.selectedTriggerKey == "intro");
    assert(workspace.selectedSceneStateKey == "night");
    assert(workspace.selectedEventBindingIndex == 3);

    Mc3SceneState state;
    Mc3ObjectOverride known;
    known.id = "known";
    known.visible = false;
    known.position = std::array<float, 3>{1.0f, 2.0f, 3.0f};
    known.material = std::string("mat_night");
    state.overrides.push_back(known);
    Mc3ObjectOverride missing;
    missing.id = "missing";
    missing.rotation = std::array<float, 3>{10.0f, 20.0f, 30.0f};
    state.overrides.push_back(missing);

    auto object = objectWithId("known");
    const auto result = workspace.applySceneState(state, [&](const std::string& id) {
        return id == object->id ? object.get() : nullptr;
    });
    assert(result.applied == 1);
    assert(result.missing == 1);
    assert(!object->visible);
    assert((object->transform.position == std::array<float, 3>{1.0f, 2.0f, 3.0f}));
    assert(object->material == "mat_night");

    workspace.resetPreview();
    assert(!workspace.previewEnabled);
    assert(workspace.selectedScriptKey == "startup");
    return 0;
}
