#pragma once
// Private implementation helpers shared across MeshCraftApplication_*.cpp files.
// Do NOT include from public headers.

#include "MeshCraft/Editor/ObjectTypeName.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace MeshCraft {

inline bool justPressed(const Microsoft::Xna::Framework::Input::KeyboardState& cur,
                        const Microsoft::Xna::Framework::Input::KeyboardState& prev,
                        Microsoft::Xna::Framework::Input::Keys k)
{
    return cur.IsKeyDown(k) && prev.IsKeyUp(k);
}

inline std::filesystem::path meshcraftConfigDir()
{
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    std::filesystem::path base = appdata && appdata[0]
        ? std::filesystem::path(appdata)
        : std::filesystem::path(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : ".");
    return base / "meshcraft";
#else
    const char* cfg = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base = cfg && cfg[0]
        ? std::filesystem::path(cfg)
        : std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / ".config";
    return base / "meshcraft";
#endif
}

inline std::filesystem::path recentFilesPath()   { return meshcraftConfigDir() / "recent.txt"; }
inline std::filesystem::path prefsPath()         { return meshcraftConfigDir() / "prefs.ini"; }
inline std::filesystem::path keybindingsPath()   { return meshcraftConfigDir() / "keybindings.ini"; }
inline std::filesystem::path macroPath()         { return meshcraftConfigDir() / "macro.mc3macro"; }

// AUD-033: removeFromList/deepCopyObject/findParentList/applyRenamePattern
// used to be byte-identical duplicates of EditorAlgorithms.hpp's
// removeFromListAlg/deepCopyObjectAlg/findParentListAlg/
// applyRenamePatternAlg, with both copies compiled into the same
// translation units and production using the ones formerly here -- a
// change to one copy (e.g. a new batch-rename token) would silently not
// affect the other, since tests only ever exercised the *Alg versions.
// Deleted; all former call sites now use the *Alg versions directly.

inline std::shared_ptr<Mc3::Mc3Object> deepCopyObj(const std::shared_ptr<Mc3::Mc3Object>& src)
{
    auto copy = std::make_shared<Mc3::Mc3Object>(*src);
    copy->children.clear();
    for (const auto& child : src->children)
        copy->children.push_back(deepCopyObj(child));
    return copy;
}

inline Mc3::Mc3Document deepCopyDoc(const Mc3::Mc3Document& src)
{
    Mc3::Mc3Document copy = src;
    copy.objects.clear();
    for (const auto& obj : src.objects)
        copy.objects.push_back(deepCopyObj(obj));
    copy.definitions.clear();
    for (const auto& [key, obj] : src.definitions)
        copy.definitions[key] = deepCopyObj(obj);
    return copy;
}

// Returns the direct parent Mc3Object whose children list contains `target`,
// or nullptr if `target` is at the root level (not found in any child list).
inline Mc3::Mc3Object* findParentObject(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
    const Mc3::Mc3Object* target)
{
    for (auto& obj : list) {
        for (const auto& child : obj->children)
            if (child.get() == target) return obj.get();
        if (!obj->children.empty()) {
            auto* found = findParentObject(obj->children, target);
            if (found) return found;
        }
    }
    return nullptr;
}

// objectTypeName() and objectTypeFromName() are the canonical, exhaustive,
// bidirectional mapping in MeshCraft/Editor/ObjectTypeName.hpp (included above).
//
// applyRenamePattern (AUD-033): deleted, see the note above deepCopyObj --
// use applyRenamePatternAlg (MeshCraft/EditorAlgorithms.hpp) instead.

} // namespace MeshCraft
