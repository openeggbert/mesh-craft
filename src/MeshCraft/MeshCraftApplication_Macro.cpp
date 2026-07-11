#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"

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

void MeshCraftApplication::saveMacro(const std::string& path) {
    if (path.empty()) { setStatusMsg("No path specified", true, 2.f); return; }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream f(path);
    if (!f) { setStatusMsg("Failed to save: " + path, true, 3.f); return; }
    for (const auto& step : macroSteps_) {
        f << step.verb;
        for (const auto& arg : step.args) f << '\t' << arg;
        f << '\n';
    }
    setStatusMsg("Macro saved: " + std::filesystem::path(path).filename().string(), false, 2.f);
}

void MeshCraftApplication::loadMacro(const std::string& path) {
    if (path.empty()) { setStatusMsg("No path specified", true, 2.f); return; }
    std::ifstream f(path);
    if (!f) { setStatusMsg("Cannot open: " + path, true, 3.f); return; }
    isRecording_ = false;
    macroSteps_.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        MacroStep step;
        std::istringstream ss(line);
        std::string tok;
        bool first = true;
        while (std::getline(ss, tok, '\t')) {
            if (first) { step.verb = tok; first = false; }
            else step.args.push_back(tok);
        }
        if (!step.verb.empty()) macroSteps_.push_back(std::move(step));
    }
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Macro loaded: %d step(s)",
                  static_cast<int>(macroSteps_.size()));
    setStatusMsg(msg, false, 2.f);
}

} // namespace MeshCraft
