#pragma once
// Pure editor PERSISTENCE algorithms (auto-save, backup rotation, unsaved-
// changes confirmation, scene merge, Save-As path resolution, export
// selection/subtree/material, drag-drop file routing, recent files list) —
// no CNA / ImGui / SDL / OpenGL dependencies.
//
// SYS-W3-05: split out of the former monolithic EditorAlgorithms.hpp so
// consumers that only need file/document persistence helpers don't have to
// pull in selection, transform-drag, event, preferences, and utility
// algorithms they never call. exportSelectionAlg() deep-copies the selected
// subtree, so this depends on EditorCommandAlgorithms.hpp for
// deepCopyObjectAlg().

#include <MeshCraft/EditorCommandAlgorithms.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MeshCraft {

// ── Auto-save (STAB-0265) ─────────────────────────────────────────────────────
//
// Mirrors two pieces of CNA-coupled logic so "the auto-save interval is
// configurable" can be exercised headlessly:
//   - autoSavePath()  in MeshCraftApplication_FileOps.cpp
//   - the countdown/trigger branch in MeshCraftApplication::update()
//     (MeshCraftApplication.cpp), which reads as:
//       if (hasFile && modified && interval > 0) {
//           countdown -= dt;
//           if (countdown <= 0) { save(); countdown = interval; }
//       } else {
//           countdown = interval > 0 ? interval : 60.0f;
//       }
// Kept in sync manually with the real code (same convention as
// deepCopyObjectAlg mirroring deepCopyDoc in MeshCraftPrivate.hpp).

inline std::string autoSavePathAlg(const std::string& file)
{
    return file + ".autosave";
}

// Advances the auto-save countdown by dt seconds. Returns true if this tick
// should perform an auto-save, in which case countdown is reset to
// intervalSeconds; otherwise countdown is updated exactly as the real
// update-loop branch does (including the disabled/no-file/unmodified cases).
inline bool autoSaveTickAlg(bool hasCurrentFile, bool modified,
                            float intervalSeconds, float dt, float& countdown)
{
    if (hasCurrentFile && modified && intervalSeconds > 0.0f) {
        countdown -= dt;
        if (countdown <= 0.0f) {
            countdown = intervalSeconds;
            return true;
        }
        return false;
    }
    countdown = intervalSeconds > 0.0f ? intervalSeconds : 60.0f;
    return false;
}

// ── Backup rotation (STAB-0267/0268) ──────────────────────────────────────────
//
// Mirrors the "F6: rotate backups before overwriting" block in
// MeshCraftApplication::saveFile() (MeshCraftApplication_FileOps.cpp:130-154):
//   if exists(file):
//     if exists(file.backup.1) rename(file.backup.1 -> file.backup.2)
//     copy_file(file -> file.backup.1, overwrite)
//   <caller then overwrites `file` with the new content>
// This is a fixed 2-slot ring buffer (no configurable depth): backup.1 is
// always the immediately-previous version, backup.2 the one before that;
// anything older is discarded. Call this BEFORE overwriting `file`.

inline void rotateBackupsAlg(const std::filesystem::path& file)
{
    if (!std::filesystem::exists(file)) return;
    auto b1 = std::filesystem::path(file.string() + ".backup.1");
    auto b2 = std::filesystem::path(file.string() + ".backup.2");
    std::error_code ec;
    if (std::filesystem::exists(b1)) std::filesystem::rename(b1, b2, ec);
    std::filesystem::copy_file(file, b1,
        std::filesystem::copy_options::overwrite_existing, ec);
}

// ── Unsaved-changes confirmation (STAB-0264) ──────────────────────────────────
//
// Mirrors confirmIfModified() (MeshCraftApplication_FileOps.cpp:85-90): a
// pending action (new scene / open file / open recent) runs immediately only
// if the document is NOT modified; otherwise it must be deferred to the
// "Unsaved Changes" dialog rather than silently discarding changes.
inline bool confirmIfModifiedAlg(bool modified)
{
    return !modified; // true => run the pending action now; false => defer to the dialog
}

// The three choices in the "Unsaved Changes" dialog
// (MeshCraftApplication_UiOverlays.cpp:1223-1253).
enum class UnsavedDialogChoiceAlg { Save, DontSave, Cancel };

// Mirrors the dialog's three button bodies: Save only lets the pending
// action proceed if there's a file to save to (Save is a no-op + refusal
// when currentFile_ is empty); Don't Save always discards and proceeds;
// Cancel always aborts.
inline bool unsavedDialogResolvesToExecuteAlg(UnsavedDialogChoiceAlg choice, bool hasCurrentFile)
{
    switch (choice) {
    case UnsavedDialogChoiceAlg::Save:     return hasCurrentFile;
    case UnsavedDialogChoiceAlg::DontSave: return true;
    case UnsavedDialogChoiceAlg::Cancel:   return false;
    }
    return false;
}

// ── Merge scene id collision resolution (STAB-0289) ───────────────────────────
//
// Object ids live on every object in the tree (top-level and nested children
// alike), not just a flat map like textures/materials/actions, so merging
// two documents that happen to share an id (plausible: both built from the
// same template, or both simply leaving id empty) previously produced a
// destination document with two objects silently sharing one id — nothing
// was overwritten (doc.objects is a vector, not a map) but anything that
// looks an object up *by id* post-merge could resolve ambiguously.

inline void collectObjectIdsAlg(const std::vector<std::shared_ptr<Mc3::Mc3Object>>& objs,
                                 std::set<std::string>& ids)
{
    for (const auto& o : objs) {
        if (!o) continue;
        if (!o->id.empty()) ids.insert(o->id);
        collectObjectIdsAlg(o->children, ids);
    }
}

// Renames obj->id (and recursively every descendant's id) with a _2, _3, ...
// suffix whenever it collides with something already in existingIds — same
// suffix idiom as the materials/actions maps below — then registers the
// (possibly new) id in existingIds so later siblings/descendants in the same
// merge see it too. Empty ids are left alone (nothing to collide with).
inline void resolveObjectIdCollisionsAlg(const std::shared_ptr<Mc3::Mc3Object>& obj,
                                          std::set<std::string>& existingIds)
{
    if (!obj) return;
    if (!obj->id.empty()) {
        if (existingIds.count(obj->id)) {
            std::string base = obj->id;
            std::string k = base;
            int n = 2;
            while (existingIds.count(k)) k = base + "_" + std::to_string(n++);
            obj->id = k;
        }
        existingIds.insert(obj->id);
    }
    for (auto& child : obj->children)
        resolveObjectIdCollisionsAlg(child, existingIds);
}

// ── Merge scene (STAB-0271, STAB-0289, STAB-0469) ─────────────────────────────
//
// Mirrors MeshCraftApplication::mergeSceneFromFile() (MeshCraftApplication_
// FileOps.cpp:258-299), minus pushUndo()/modified_/updateWindowTitle()/
// setStatusMsg(). Textures: skip on key collision (existing document wins).
// Materials and actions: on key collision, suffix with _2, _3, ... until
// unique, and rename the copy's `name` field to match its new key (STAB-0469
// added action-merging to the real method — this mirror had fallen out of
// sync and never merged actions at all until STAB-0534/0535 caught it).
// Objects: id collisions (top-level or nested) are resolved the same way via
// resolveObjectIdCollisionsAlg() above, then appended by sharing the same
// shared_ptr (matches the real code — safe because src is always freshly
// loaded from file and shares no ownership with dst). Returns the number of
// objects appended.

inline int mergeDocumentsAlg(Mc3::Mc3Document& dst, const Mc3::Mc3Document& src)
{
    for (const auto& [key, tex] : src.textures) {
        if (!dst.textures.count(key))
            dst.textures[key] = tex;
    }

    for (const auto& [key, mat] : src.materials) {
        std::string k = key;
        int n = 2;
        while (dst.materials.count(k)) k = key + "_" + std::to_string(n++);
        dst.materials[k] = mat;
        dst.materials[k].name = k;
    }

    std::set<std::string> existingIds;
    collectObjectIdsAlg(dst.objects, existingIds);

    int added = 0;
    for (const auto& obj : src.objects) {
        resolveObjectIdCollisionsAlg(obj, existingIds);
        dst.objects.push_back(obj);
        ++added;
    }

    for (const auto& [key, action] : src.actions) {
        std::string k = key;
        int n = 2;
        while (dst.actions.count(k)) k = key + "_" + std::to_string(n++);
        dst.actions[k] = action;
        dst.actions[k].name = k;
    }
    return added;
}

// ── Save As path resolution (STAB-0272, extended SYS-W14-11) ─────────────────
//
// Mirrors the path-normalization logic in the "Save As" dialog's Save button
// body (MeshCraftApplication_UiOverlays.cpp): a path already ending in
// ".mcb"/".json"/".mc3.json" is saved via the matching writer as-is; any
// other path gets ".mc3.xml" appended unless it already contains that
// suffix. Returns {resolvedPath, format}.
enum class SaveAsFormat { Xml, Mcb, Json };

inline std::pair<std::string, SaveAsFormat> resolveSaveAsPathAlg(const std::string& rawPath)
{
    std::string path = rawPath;
    auto endsWith = [&](const char* suffix) {
        std::string_view s(suffix);
        return path.size() >= s.size() && path.compare(path.size() - s.size(), s.size(), s) == 0;
    };
    if (endsWith(".mcb"))  return {path, SaveAsFormat::Mcb};
    if (endsWith(".json")) return {path, SaveAsFormat::Json}; // covers both .json and .mc3.json
    if (path.find(".mc3.xml") == std::string::npos) path += ".mc3.xml";
    return {path, SaveAsFormat::Xml};
}

// ── Export selection (STAB-0273/0274) ─────────────────────────────────────────
//
// Mirrors MeshCraftApplication::exportSelectionToFile() (MeshCraftApplication_
// FileOps.cpp:209-250), minus the file write and setStatusMsg(). Builds a new
// document containing deep copies of the selected objects (and their
// children) plus every material and texture those objects (recursively)
// reference — so exporting a selection never silently drops a material or
// texture it depends on, but also never carries the whole scene's asset set.

inline Mc3::Mc3Document exportSelectionAlg(
    const Mc3::Mc3Document&                             doc,
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected)
{
    Mc3::Mc3Document tmp;

    std::set<std::string> matKeys;
    std::function<void(const Mc3::Mc3Object&)> collectMats =
        [&](const Mc3::Mc3Object& obj) {
            if (!obj.material.empty())         matKeys.insert(obj.material);
            if (!obj.materialOverride.empty()) matKeys.insert(obj.materialOverride);
            for (const auto& c : obj.children) collectMats(*c);
        };

    for (const auto& sel : selected) {
        tmp.objects.push_back(deepCopyObjectAlg(*sel));
        collectMats(*sel);
    }

    std::set<std::string> texKeys;
    for (const auto& key : matKeys) {
        auto it = doc.materials.find(key);
        if (it == doc.materials.end()) continue;
        tmp.materials[key] = it->second;
        const auto& m = it->second;
        for (const auto& tk : { m.baseColorTexture, m.normalTexture,
                                 m.emissiveTexture, m.metallicRoughnessTexture,
                                 m.occlusionTexture })
            if (!tk.empty()) texKeys.insert(tk);
    }

    for (const auto& key : texKeys) {
        auto it = doc.textures.find(key);
        if (it != doc.textures.end()) tmp.textures[key] = it->second;
    }

    return tmp;
}

// ── Export subtree as template (STAB-0536) ────────────────────────────────────
//
// Mirrors the file-save half of MeshCraftApplication::exportSubtreeAsTemplate()
// (MeshCraftApplication_Commands.cpp:514-536). Before this fix, that method
// wrote only `tmp.definitions[defName] = defObj` — no materials or textures —
// so a template exported to its own file (e.g. for reuse as an <include>
// library, matching test/mc3_library.mc3.xml's shape) silently lost the
// appearance of any object in it that referenced a material, which would
// then resolve to the default-gray fallback (STAB-0500) wherever the
// template file was loaded standalone. This mirrors exportSelectionAlg's
// material/texture collection so the template's dependencies travel with it.

inline Mc3::Mc3Document exportSubtreeTemplateAlg(
    const Mc3::Mc3Document&           doc,
    const std::string&                defName,
    std::shared_ptr<Mc3::Mc3Object>   defObj)
{
    Mc3::Mc3Document tmp;

    std::set<std::string> matKeys;
    std::function<void(const Mc3::Mc3Object&)> collectMats =
        [&](const Mc3::Mc3Object& obj) {
            if (!obj.material.empty())         matKeys.insert(obj.material);
            if (!obj.materialOverride.empty()) matKeys.insert(obj.materialOverride);
            for (const auto& c : obj.children) collectMats(*c);
        };
    collectMats(*defObj);

    std::set<std::string> texKeys;
    for (const auto& key : matKeys) {
        auto it = doc.materials.find(key);
        if (it == doc.materials.end()) continue;
        tmp.materials[key] = it->second;
        const auto& m = it->second;
        for (const auto& tk : { m.baseColorTexture, m.normalTexture,
                                 m.emissiveTexture, m.metallicRoughnessTexture,
                                 m.occlusionTexture })
            if (!tk.empty()) texKeys.insert(tk);
    }

    for (const auto& key : texKeys) {
        auto it = doc.textures.find(key);
        if (it != doc.textures.end()) tmp.textures[key] = it->second;
    }

    tmp.definitions[defName] = std::move(defObj);
    return tmp;
}

// ── Export/import a single material (STAB-0544/0545) ─────────────────────────
//
// Mirrors the Material Export/Import dialogs' button bodies
// (MeshCraftApplication_UiOverlays.cpp:1626-1637 and :1664-1688), minus the
// file I/O and setStatusMsg(). Before this fix, exportMaterialAlg's real-code
// counterpart wrote only the material itself — no referenced textures — so
// an exported .mc3mat.xml file would contain a dangling base_color_texture
// (etc.) reference wherever it was re-imported standalone, silently losing
// its appearance (STAB-0500's gray fallback). The import side didn't even
// look at loaded.textures at all, so re-importing that same (now-fixed)
// file would still drop them. Both are fixed together here: export collects
// the material's referenced textures (same pattern as exportSelectionAlg /
// exportSubtreeTemplateAlg); import re-adds them, reusing an existing
// texture under the same key only if its `uri` actually matches (otherwise
// suffixing a new key and re-pointing the material's own reference field —
// a *different* texture must never silently masquerade as the one this
// material was exported with).

inline Mc3::Mc3Document exportMaterialAlg(const Mc3::Mc3Document& doc, const std::string& matId)
{
    Mc3::Mc3Document tmp;
    auto it = doc.materials.find(matId);
    if (it == doc.materials.end()) return tmp;
    tmp.materials[matId] = it->second;

    const auto& m = it->second;
    for (const auto& tk : { m.baseColorTexture, m.normalTexture, m.emissiveTexture,
                             m.metallicRoughnessTexture, m.occlusionTexture }) {
        if (tk.empty()) continue;
        auto texIt = doc.textures.find(tk);
        if (texIt != doc.textures.end()) tmp.textures[tk] = texIt->second;
    }
    return tmp;
}

// Imports every material in `loaded` into `dest`, suffixing the material's
// own key on collision (_2, _3, ...) and renaming the copy's `name` field to
// match, exactly like mergeDocumentsAlg's material handling. Any texture the
// material references (found in loaded.textures) is imported alongside it:
// reused as-is if dest already has a texture under that key with the same
// `uri`, otherwise inserted under a suffixed key with the material's own
// reference field re-pointed to match. Returns the number of materials
// imported.

inline int importMaterialsAlg(Mc3::Mc3Document& dest, const Mc3::Mc3Document& loaded,
                               std::string* lastKeyOut = nullptr)
{
    auto importTexRef = [&](std::string& texRef) {
        if (texRef.empty()) return;
        auto texIt = loaded.textures.find(texRef);
        if (texIt == loaded.textures.end()) return;

        auto existing = dest.textures.find(texRef);
        if (existing != dest.textures.end() && existing->second.uri == texIt->second.uri)
            return; // same texture already present under this key — reuse it

        std::string key = texRef;
        int n = 2;
        while (dest.textures.count(key)) key = texRef + "_" + std::to_string(n++);
        dest.textures[key] = texIt->second;
        texRef = key;
    };

    int added = 0;
    for (const auto& [id, srcMat] : loaded.materials) {
        std::string key = id;
        int n = 2;
        while (dest.materials.count(key)) key = id + "_" + std::to_string(n++);

        Mc3::Mc3Material mat = srcMat;
        importTexRef(mat.baseColorTexture);
        importTexRef(mat.normalTexture);
        importTexRef(mat.emissiveTexture);
        importTexRef(mat.metallicRoughnessTexture);
        importTexRef(mat.occlusionTexture);

        dest.materials[key] = mat;
        dest.materials[key].name = key;
        if (lastKeyOut) *lastKeyOut = key;
        ++added;
    }
    return added;
}

// ── Drag-drop file routing (STAB-0275) ────────────────────────────────────────
//
// Mirrors the extension check in MeshCraftApplication's SDL_EVENT_DROP_FILE
// watcher (MeshCraftApplication.cpp:332): a dropped path is routed to the
// scene loader if its extension is ".xml" or its path contains ".mc3"
// (covers both "scene.mc3.xml" and the bare-extension "scene.mc3" case);
// anything else (images, unrelated files) is rejected with a status message
// instead of being handed to the XML parser.

inline bool isDroppableScenePathAlg(const std::filesystem::path& path)
{
    return path.extension() == ".xml" || path.string().find(".mc3") != std::string::npos;
}

// ── Recent files list (STAB-0288) ─────────────────────────────────────────────
//
// Mirrors loadRecentFiles()/saveRecentFiles()/addRecentFile()
// (MeshCraftApplication_FileOps.cpp:61-89): a newline-separated absolute-path
// list, most-recently-used first, capped at kMaxRecentFiles (10). A path that
// no longer exists on disk is silently skipped on load (not removed from the
// file — only pruned in-memory for the current session). addRecentFile()
// de-duplicates (moving an existing entry to the front rather than adding a
// second copy), inserts at the front, then truncates to the cap.

inline void loadRecentFilesAlg(const std::filesystem::path& path,
                                std::vector<std::filesystem::path>& recentFiles,
                                int maxRecentFiles)
{
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line) && static_cast<int>(recentFiles.size()) < maxRecentFiles) {
        if (!line.empty() && std::filesystem::exists(line))
            recentFiles.emplace_back(line);
    }
}

inline void saveRecentFilesAlg(const std::filesystem::path& path,
                                const std::vector<std::filesystem::path>& recentFiles)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path);
    for (const auto& r : recentFiles)
        f << r.string() << "\n";
}

inline void addRecentFileAlg(std::vector<std::filesystem::path>& recentFiles,
                              const std::filesystem::path& newPath,
                              int maxRecentFiles)
{
    auto abs = std::filesystem::absolute(newPath);
    recentFiles.erase(
        std::remove_if(recentFiles.begin(), recentFiles.end(),
            [&](const auto& r) { return r == abs; }),
        recentFiles.end());
    recentFiles.insert(recentFiles.begin(), abs);
    if (static_cast<int>(recentFiles.size()) > maxRecentFiles)
        recentFiles.resize(static_cast<size_t>(maxRecentFiles));
}

} // namespace MeshCraft
