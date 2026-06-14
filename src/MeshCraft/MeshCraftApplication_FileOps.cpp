#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <string>

namespace MeshCraft {

void MeshCraftApplication::newScene() {
    document_ = Mc3::Mc3Document{};
    document_.model = "Untitled";
    selection_.clear();
    modified_ = false;
    currentFile_.clear();
    currentActionName_.clear();
    animTime_    = 0.0f;
    animPlaying_ = false;
    if (sceneRenderer_) sceneRenderer_->setAnimOverrides({});
    std::cout << "[MeshCraft] New scene\n";
    updateWindowTitle();
}

std::filesystem::path MeshCraftApplication::autoSavePath(const std::filesystem::path& file) {
    return std::filesystem::path(file.string() + ".autosave");
}

void MeshCraftApplication::performAutoSave() {
    if (currentFile_.empty()) return;
    try {
        document_.saveToFile(autoSavePath(currentFile_));
        setStatusMsg("Auto-saved", false, 1.5f);
    } catch (...) {}
}

void MeshCraftApplication::setStatusMsg(std::string msg, bool isError, float duration) {
    statusMsg_ = std::move(msg);
    statusMsgIsError_ = isError;
    statusMsgTimer_ = duration;
}


void MeshCraftApplication::loadRecentFiles() {
    std::ifstream f(recentFilesPath());
    std::string line;
    while (std::getline(f, line) && static_cast<int>(recentFiles_.size()) < kMaxRecentFiles) {
        if (!line.empty() && std::filesystem::exists(line))
            recentFiles_.emplace_back(line);
    }
}

void MeshCraftApplication::saveRecentFiles() {
    auto p = recentFilesPath();
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream f(p);
    for (const auto& r : recentFiles_)
        f << r.string() << "\n";
}

void MeshCraftApplication::addRecentFile(const std::filesystem::path& path) {
    auto abs = std::filesystem::absolute(path);
    recentFiles_.erase(
        std::remove_if(recentFiles_.begin(), recentFiles_.end(),
            [&](const auto& r){ return r == abs; }),
        recentFiles_.end());
    recentFiles_.insert(recentFiles_.begin(), abs);
    if (static_cast<int>(recentFiles_.size()) > kMaxRecentFiles)
        recentFiles_.resize(static_cast<size_t>(kMaxRecentFiles));
    saveRecentFiles();
}

void MeshCraftApplication::confirmIfModified(PendingAction action, std::filesystem::path path) {
    pendingAction_   = action;
    pendingOpenPath_ = std::move(path);
    if (!modified_) executePendingAction();
    else            unsavedDlgOpen_ = true;
}

void MeshCraftApplication::executePendingAction() {
    switch (pendingAction_) {
    case PendingAction::NewScene:
        newScene();
        break;
    case PendingAction::OpenFile:
        openFile();
        break;
    case PendingAction::OpenRecentFile:
        if (!pendingOpenPath_.empty()) {
            try {
                document_ = Mc3::Mc3Document::loadFromFile(pendingOpenPath_);
                currentFile_ = pendingOpenPath_;
                addRecentFile(currentFile_);
                selection_.clear();
                undoStack_.clear(); redoStack_.clear();
                modified_ = false;
                setStatusMsg("Opened " + currentFile_.filename().string(), false, 2.0f);
                updateWindowTitle();
            } catch (...) {}
        }
        break;
    case PendingAction::ExitApp:
        Exit();
        break;
    default:
        break;
    }
    pendingAction_ = PendingAction::None;
}

void MeshCraftApplication::openFile() {
    openDialogBuf_[0] = '\0';
    openDialogErr_[0] = '\0';
    openDialogOpen_ = true;
}

void MeshCraftApplication::saveFile() {
    if (currentFile_.empty()) { saveFileAs(); return; }
    try {
        document_.saveToFile(currentFile_);
        addRecentFile(currentFile_);
        modified_ = false;
        autoSaveCountdown_ = 60.0f;
        { std::error_code ec; std::filesystem::remove(autoSavePath(currentFile_), ec); }
        std::cout << "[MeshCraft] Saved: " << currentFile_ << "\n";
        setStatusMsg("Saved " + currentFile_.filename().string(), false, 2.0f);
        updateWindowTitle();
    } catch (const std::exception& e) {
        std::cerr << "[MeshCraft] Save error: " << e.what() << "\n";
        setStatusMsg(std::string("Save error: ") + e.what(), true);
    }
}

void MeshCraftApplication::saveFileAs() {
    auto s = currentFile_.string();
    std::strncpy(saveDialogBuf_, s.c_str(), sizeof(saveDialogBuf_) - 1);
    saveDialogBuf_[sizeof(saveDialogBuf_) - 1] = '\0';
    saveDialogErr_[0] = '\0';
    saveDialogOpen_ = true;
}

void MeshCraftApplication::exportGltf() {
    if (currentFile_.empty()) {
        setStatusMsg("Export failed: save the file first", true);
        return;
    }
    std::string mc3togltf = "mc3togltf";
    for (const auto& candidate : {
        std::filesystem::path("mc3togltf/build/mc3togltf"),        // standalone build (default)
        std::filesystem::path("cmake-build-debug/mc3togltf/mc3togltf"), // integrated root build
        std::filesystem::path("build-mc3togltf/mc3togltf"),
        std::filesystem::path("../build-mc3togltf/mc3togltf"),
        std::filesystem::path("../mc3togltf/build/mc3togltf"),
    }) {
        if (std::filesystem::exists(candidate)) { mc3togltf = candidate.string(); break; }
    }
    std::string outPath = currentFile_.string();
    auto pos = outPath.rfind(".mc3.xml");
    if (pos != std::string::npos) outPath.replace(pos, 8, ".glb");
    else outPath += ".glb";

    std::string cmd = mc3togltf + " \"" + currentFile_.string() + "\" \"" + outPath + "\"";
    std::cout << "[MeshCraft] Exporting: " << cmd << "\n";
    int ret = std::system(cmd.c_str());
    if (ret == 0) {
        std::cout << "[MeshCraft] Exported to: " << outPath << "\n";
        auto name = std::filesystem::path(outPath).filename().string();
        setStatusMsg("Exported to " + name);
    } else {
        std::cerr << "[MeshCraft] Export failed (exit code " << ret << ")\n";
        setStatusMsg("Export failed (exit " + std::to_string(ret) + ")", true);
    }
}

// ---------------------------------------------------------------------------
// Edit operations
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// F3: Export selection to MC3 XML
// ---------------------------------------------------------------------------
void MeshCraftApplication::exportSelectionToFile(const std::string& path) {
    if (path.empty() || !selection_.hasSelection()) return;

    Mc3::Mc3Document tmp;

    // Collect material + texture keys referenced in an object subtree
    std::set<std::string> matKeys;
    std::function<void(const Mc3::Mc3Object&)> collectMats =
        [&](const Mc3::Mc3Object& obj) {
            if (!obj.material.empty())         matKeys.insert(obj.material);
            if (!obj.materialOverride.empty()) matKeys.insert(obj.materialOverride);
            for (const auto& c : obj.children) collectMats(*c);
        };

    for (const auto& sel : selection_.selection()) {
        tmp.objects.push_back(deepCopyObject(*sel));
        collectMats(*sel);
    }

    // Copy referenced materials
    std::set<std::string> texKeys;
    for (const auto& key : matKeys) {
        auto it = document_.materials.find(key);
        if (it == document_.materials.end()) continue;
        tmp.materials[key] = it->second;
        const auto& m = it->second;
        for (const auto& tk : { m.baseColorTexture, m.normalTexture,
                                 m.emissiveTexture, m.metallicRoughnessTexture,
                                 m.occlusionTexture })
            if (!tk.empty()) texKeys.insert(tk);
    }

    // Copy referenced textures
    for (const auto& key : texKeys) {
        auto it = document_.textures.find(key);
        if (it != document_.textures.end()) tmp.textures[key] = it->second;
    }

    tmp.saveToFile(path);
    setStatusMsg("Exported " + std::to_string(selection_.selection().size()) +
                 " object(s) → " + path, false, 3.0f);
}

// ---------------------------------------------------------------------------
// F4: Merge scene from MC3 XML
// ---------------------------------------------------------------------------
void MeshCraftApplication::mergeSceneFromFile(const std::string& path) {
    Mc3::Mc3Document src = Mc3::Mc3Document::loadFromFile(path);
    pushUndo();

    // Merge textures (skip on key collision — existing wins)
    for (auto& [key, tex] : src.textures) {
        if (!document_.textures.count(key))
            document_.textures[key] = tex;
    }

    // Merge materials (suffix on collision)
    for (auto& [key, mat] : src.materials) {
        std::string k = key;
        int n = 2;
        while (document_.materials.count(k)) k = key + "_" + std::to_string(n++);
        document_.materials[k] = mat;
        document_.materials[k].name = k;
    }

    // Append objects (deep-copy already done by loadFromFile)
    int added = 0;
    for (auto& obj : src.objects) {
        document_.objects.push_back(obj);
        ++added;
    }

    modified_ = true; updateWindowTitle();
    setStatusMsg("Merged " + std::to_string(added) + " object(s) from " + path, false, 3.0f);
}

} // namespace MeshCraft
