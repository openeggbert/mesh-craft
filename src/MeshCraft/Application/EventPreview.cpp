#include "MeshCraft/Application/MeshCraftApplication.hpp"

#include "MeshCraft/MeshCraftPrivate.hpp"

#include <filesystem>
#include <string>
#include <utility>

namespace MeshCraft::Application {

namespace {

std::string resolveEventPreviewSource(const Mc3::Mc3Document& document, const std::string& source)
{
    if (source.empty()) return source;
    const std::filesystem::path path(source);
    return path.is_absolute() ? path.string() : (document.sourcePath / path).string();
}

} // namespace

void MeshCraftApplication::executeEventPreview(Editor::EventBindingDispatchReport dispatch)
{
    if (dispatch.dispatches.empty() && dispatch.diagnostics.empty()) return;

    const auto selectionIds = currentSelectionIds();
    auto result = automationWorkspace_.previewRunner.execute(
        document_, std::move(dispatch), [this] { pushUndo(); });
    automationWorkspace_.previewReport = result.dispatch;
    automationWorkspace_.previewErrors = result.errors;

    if (!result.errors.empty()) {
        setStatusMsg("Preview event rolled back: " + result.errors.front(), true, 5.0f);
        return;
    }

    if (result.documentCommitted) {
        // pushUndo() captured the old document before the runner swapped the
        // validated transaction in.  Invalidate only after that swap, then
        // restore stable selection identities against the new graph.
        objectIndex_.invalidate();
        restoreSelectionByIds(selectionIds);
        modified_ = true;
        updateWindowTitle();
    }

    for (const auto& effect : result.effects) {
        switch (effect.type) {
        case Editor::EventPreviewEffectType::PlayAction:
            if (!document_.actions.contains(effect.ref)) continue;
            currentActionName_ = effect.ref;
            currentActionClipName_.clear();
            clearAnimationPreviewTransition();
            animTime_ = 0.0f;
            animPlaying_ = true;
            break;
        case Editor::EventPreviewEffectType::PlaySound: {
            const auto sound = document_.sounds.find(effect.ref);
            if (sound != document_.sounds.end())
                audioPreview_.play(effect.ref, resolveEventPreviewSource(document_, sound->second.src),
                                   sound->second.loop);
            break;
        }
        case Editor::EventPreviewEffectType::PlayMusic: {
            const auto music = document_.musicTracks.find(effect.ref);
            if (music != document_.musicTracks.end())
                audioPreview_.play(effect.ref, resolveEventPreviewSource(document_, music->second.src),
                                   music->second.loop);
            break;
        }
        }
    }

    if (!result.dispatch.dispatches.empty()) {
        std::string message = "Preview dispatched " +
            std::to_string(result.dispatch.dispatches.size()) + " binding" +
            (result.dispatch.dispatches.size() == 1 ? "" : "s");
        if (result.appliedStateOverrides > 0)
            message += "; " + std::to_string(result.appliedStateOverrides) + " state override" +
                       (result.appliedStateOverrides == 1 ? "" : "s") + " applied";
        if (result.skippedTriggerSteps > 0)
            message += "; " + std::to_string(result.skippedTriggerSteps) + " trigger step" +
                       (result.skippedTriggerSteps == 1 ? "" : "s") + " skipped";
        setStatusMsg(message, false, 3.0f);
    }
}

} // namespace MeshCraft::Application
