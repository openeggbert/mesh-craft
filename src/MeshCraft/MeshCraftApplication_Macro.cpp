#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

namespace MeshCraft {

// ---------------------------------------------------------------------------
// Recording
// ---------------------------------------------------------------------------

void MeshCraftApplication::recordStep(const std::string& verb, std::vector<std::string> args) {
    if (!isRecording_) return;
    macroSteps_.push_back({verb, std::move(args)});
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

void MeshCraftApplication::playMacro() {
    if (macroSteps_.empty()) return;
    const bool wasRecording = isRecording_;
    isRecording_ = false; // don't record the playback itself
    for (const auto& step : macroSteps_)
        executeMacroStep(step);
    isRecording_ = wasRecording;
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Macro: %d step(s) replayed",
                  static_cast<int>(macroSteps_.size()));
    setStatusMsg(msg, false, 2.5f);
}

void MeshCraftApplication::executeMacroStep(const MacroStep& step) {
    const std::string& v = step.verb;
    const auto& a = step.args;

    auto arg = [&](int i, const char* def = "") -> std::string {
        return i < static_cast<int>(a.size()) ? a[static_cast<size_t>(i)] : def;
    };

    if (v == "add") {
        if (auto type = objectTypeFromName(arg(0)))
            addPrimitive(*type);
        else
            setStatusMsg("Macro: skipped 'add' with unknown object type", true, 3.0f);
    } else if (v == "delete") {
        deleteSelected();
    } else if (v == "duplicate") {
        duplicateSelected();
    } else if (v == "group") {
        groupSelected();
    } else if (v == "ungroup") {
        ungroupSelected();
    } else if (v == "group_scale") {
        try { groupScaleFactor_ = std::stof(arg(0, "1")); } catch (...) { groupScaleFactor_ = 1.f; }
        groupScaleSelected();
    } else if (v == "batch_rename") {
        copyToBuf(batchRenameBuf_, arg(0));
        batchRenameSelected();
    } else if (v == "linear_array") {
        try {
            arrayDupCount_   = std::stoi(arg(0, "3"));
            arrayDupAxis_    = std::stoi(arg(1, "0"));
            arrayDupSpacing_ = std::stof(arg(2, "1"));
        } catch (...) {}
        arrayDuplicate();
    } else if (v == "hide") {
        pushUndo();
        for (auto& s : selection_.selection()) s->visible = false;
        modified_ = true;
        updateWindowTitle();
    } else if (v == "show_all") {
        // SYS-W14-16: found by a dedicated undo-coverage audit -- the
        // adjacent "hide" verb above correctly calls pushUndo() first;
        // this one didn't, a straightforward copy/paste omission (the
        // Keyboard.cpp and UiMenuBar.cpp equivalents of this same
        // operation both already call pushUndo() correctly).
        pushUndo();
        std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> showAll;
        showAll = [&](auto& list) {
            for (auto& o : list) { o->visible = true; showAll(o->children); }
        };
        showAll(document_.objects);
        modified_ = true;
        updateWindowTitle();
    } else if (v == "lock") {
        for (const auto& s : selection_.selection()) lockedIds_.insert(s->id);
    } else if (v == "unlock") {
        for (const auto& s : selection_.selection()) lockedIds_.erase(s->id);
    }
}

// ---------------------------------------------------------------------------
// Save / Load
// ---------------------------------------------------------------------------

// AUD-031: previously a separate hand-copied duplicate of
// saveMacroAlg/loadMacroAlg's own tab-separated-line format logic.
// MacroStep and MacroStepAlg are structurally identical but distinct
// types (MacroStep is CNA-coupled, declared in MeshCraftApplication.hpp;
// MacroStepAlg is the CNA-free mirror), so a small copy is unavoidable
// here -- the file open/failure-check stays in production too, since
// saveMacroAlg/loadMacroAlg don't report open failure (an ofstream/
// ifstream that fails to open just silently no-ops), and that user-visible
// error message is worth preserving.
void MeshCraftApplication::saveMacro(const std::string& path) {
    if (path.empty()) { setStatusMsg("No path specified", true, 2.f); return; }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    if (!std::ofstream(path)) { setStatusMsg("Failed to save: " + path, true, 3.f); return; }
    std::vector<MacroStepAlg> steps;
    steps.reserve(macroSteps_.size());
    for (const auto& s : macroSteps_) steps.push_back({s.verb, s.args});
    saveMacroAlg(path, steps);
    setStatusMsg("Macro saved: " + std::filesystem::path(path).filename().string(), false, 2.f);
}

void MeshCraftApplication::loadMacro(const std::string& path) {
    if (path.empty()) { setStatusMsg("No path specified", true, 2.f); return; }
    if (!std::ifstream(path)) { setStatusMsg("Cannot open: " + path, true, 3.f); return; }
    isRecording_ = false;
    macroSteps_.clear();
    for (auto& s : loadMacroAlg(path)) {
        MacroStep step;
        step.verb = std::move(s.verb);
        step.args = std::move(s.args);
        macroSteps_.push_back(std::move(step));
    }
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Macro loaded: %d step(s)",
                  static_cast<int>(macroSteps_.size()));
    setStatusMsg(msg, false, 2.f);
}

} // namespace MeshCraft
