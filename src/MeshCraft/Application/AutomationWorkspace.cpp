#include "MeshCraft/Application/AutomationWorkspace.hpp"

namespace MeshCraft::Application {

void AutomationWorkspace::setPreviewEnabled(bool enabled)
{
    previewEnabled = enabled;
    resetPreviewRuntime();
}

void AutomationWorkspace::resetPreview()
{
    previewEnabled = false;
    resetPreviewRuntime();
}

void AutomationWorkspace::resetPreviewRuntime()
{
    previewRunner.reset();
    previewReport = {};
    previewErrors.clear();
}

void AutomationWorkspace::clearPreviewAreaMembership()
{
    previewRunner.clearAreaMembership();
}

std::string AutomationWorkspace::runScript(const std::string& source,
                                            Mc3::Mc3Document& document,
                                            Mc3::Mc3Object* target,
                                            const std::function<void()>& beforeCommit)
{
    return luaScriptRunner_.run(source, document, target, beforeCommit);
}

AutomationWorkspace::StateApplyResult AutomationWorkspace::applySceneState(
    const Mc3::Mc3SceneState& state, const FindObjectById& findObject) const
{
    StateApplyResult result;
    for (const auto& override : state.overrides) {
        Mc3::Mc3Object* object = findObject(override.id);
        if (!object) {
            ++result.missing;
            continue;
        }
        if (override.visible) object->visible = *override.visible;
        if (override.position) object->transform.position = *override.position;
        if (override.rotation) object->transform.rotation = *override.rotation;
        if (override.material) object->material = *override.material;
        ++result.applied;
    }
    return result;
}

} // namespace MeshCraft::Application
