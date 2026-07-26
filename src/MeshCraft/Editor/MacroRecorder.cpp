#include "MeshCraft/Editor/MacroRecorder.hpp"
#include "MeshCraft/Editor/ObjectTypeName.hpp"
#include "MeshCraft/EditorPreferencesAlgorithms.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace MeshCraft::Editor {

void MacroRecorder::recordStep(const std::string& verb, std::vector<std::string> args) {
    if (!isRecording_) return;
    steps_.push_back({verb, std::move(args)});
}

void MacroRecorder::play(const Context& ctx) {
    if (steps_.empty()) return;
    const bool wasRecording = isRecording_;
    isRecording_ = false; // don't record the playback itself
    for (const auto& step : steps_)
        executeStep(step, ctx);
    isRecording_ = wasRecording;
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Macro: %d step(s) replayed",
                  static_cast<int>(steps_.size()));
    ctx.setStatusMsg(msg, false, 2.5f);
}

void MacroRecorder::executeStep(const Step& step, const Context& ctx) {
    const std::string& v = step.verb;
    const auto& a = step.args;

    auto arg = [&](int i, const char* def = "") -> std::string {
        return i < static_cast<int>(a.size()) ? a[static_cast<size_t>(i)] : def;
    };

    if (v == "add") {
        if (auto type = objectTypeFromName(arg(0)))
            ctx.addPrimitive(*type);
        else
            ctx.setStatusMsg("Macro: skipped 'add' with unknown object type", true, 3.0f);
    } else if (v == "delete") {
        ctx.deleteSelected();
    } else if (v == "duplicate") {
        ctx.duplicateSelected();
    } else if (v == "group") {
        ctx.groupSelected();
    } else if (v == "ungroup") {
        ctx.ungroupSelected();
    } else if (v == "group_scale") {
        float factor = 1.f;
        try { factor = std::stof(arg(0, "1")); } catch (...) { factor = 1.f; }
        ctx.groupScaleSelected(factor);
    } else if (v == "batch_rename") {
        ctx.batchRenameSelected(arg(0));
    } else if (v == "linear_array") {
        int count = 3, axis = 0;
        float spacing = 1.f;
        try {
            count   = std::stoi(arg(0, "3"));
            axis    = std::stoi(arg(1, "0"));
            spacing = std::stof(arg(2, "1"));
        } catch (...) {}
        ctx.linearArrayDuplicate(count, axis, spacing);
    } else if (v == "hide") {
        ctx.hideSelected();
    } else if (v == "show_all") {
        ctx.showAllObjects();
    } else if (v == "lock") {
        ctx.lockSelected();
    } else if (v == "unlock") {
        ctx.unlockSelected();
    }
}

// AUD-031: saveMacroAlg/loadMacroAlg (EditorAlgorithms.hpp) are the
// CNA-free mirror of this format, kept as a separate MacroStepAlg type
// since they're unit-tested standalone without linking this class --
// converting at this boundary is the smallest duplication that allows
// that, matching the precedent this codebase already established (see
// EditorAlgorithms.hpp's own comment next to MacroStepAlg). The file
// open/failure-check stays here in production too, since saveMacroAlg/
// loadMacroAlg don't report open failure (an ofstream/ifstream that fails
// to open just silently no-ops), and that user-visible error message is
// worth preserving.
void MacroRecorder::save(const std::string& path, const Context& ctx) const {
    if (path.empty()) { ctx.setStatusMsg("No path specified", true, 2.f); return; }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    if (!std::ofstream(path)) { ctx.setStatusMsg("Failed to save: " + path, true, 3.f); return; }
    std::vector<MacroStepAlg> algSteps;
    algSteps.reserve(steps_.size());
    for (const auto& s : steps_) algSteps.push_back({s.verb, s.args});
    saveMacroAlg(path, algSteps);
    ctx.setStatusMsg("Macro saved: " + std::filesystem::path(path).filename().string(), false, 2.f);
}

void MacroRecorder::load(const std::string& path, const Context& ctx) {
    if (path.empty()) { ctx.setStatusMsg("No path specified", true, 2.f); return; }
    if (!std::ifstream(path)) { ctx.setStatusMsg("Cannot open: " + path, true, 3.f); return; }
    isRecording_ = false;
    steps_.clear();
    for (auto& s : loadMacroAlg(path))
        steps_.push_back({std::move(s.verb), std::move(s.args)});
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Macro loaded: %d step(s)",
                  static_cast<int>(steps_.size()));
    ctx.setStatusMsg(msg, false, 2.f);
}

} // namespace MeshCraft::Editor
