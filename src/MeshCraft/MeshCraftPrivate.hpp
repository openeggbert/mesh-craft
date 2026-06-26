#pragma once
// Private implementation helpers shared across MeshCraftApplication_*.cpp files.
// Do NOT include from public headers.

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
    const char* cfg = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base = cfg && cfg[0]
        ? std::filesystem::path(cfg)
        : std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / ".config";
    return base / "meshcraft";
}

inline std::filesystem::path recentFilesPath()   { return meshcraftConfigDir() / "recent.txt"; }
inline std::filesystem::path prefsPath()         { return meshcraftConfigDir() / "prefs.ini"; }
inline std::filesystem::path keybindingsPath()   { return meshcraftConfigDir() / "keybindings.ini"; }
inline std::filesystem::path macroPath()         { return meshcraftConfigDir() / "macro.mc3macro"; }

inline Mc3::ObjectType objectTypeFromName(const std::string& n) {
    if (n == "Sphere")       return Mc3::ObjectType::Sphere;
    if (n == "Cylinder")     return Mc3::ObjectType::Cylinder;
    if (n == "Cone")         return Mc3::ObjectType::Cone;
    if (n == "Plane")        return Mc3::ObjectType::Plane;
    if (n == "Torus")        return Mc3::ObjectType::Torus;
    if (n == "Capsule")      return Mc3::ObjectType::Capsule;
    if (n == "Disk")         return Mc3::ObjectType::Disk;
    if (n == "Grid")         return Mc3::ObjectType::Grid;
    if (n == "IcoSphere")    return Mc3::ObjectType::IcoSphere;
    if (n == "Extrude")      return Mc3::ObjectType::Extrude;
    if (n == "Union")        return Mc3::ObjectType::Union;
    if (n == "Difference")   return Mc3::ObjectType::Difference;
    if (n == "Intersection") return Mc3::ObjectType::Intersection;
    if (n == "Group")        return Mc3::ObjectType::Group;
    return Mc3::ObjectType::Box;
}

inline void removeFromList(std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
                           const Mc3::Mc3Object* target)
{
    list.erase(std::remove_if(list.begin(), list.end(),
        [&](const auto& o){ return o.get() == target; }), list.end());
    for (auto& obj : list)
        if (!obj->children.empty())
            removeFromList(obj->children, target);
}

inline std::shared_ptr<Mc3::Mc3Object> deepCopyObject(const Mc3::Mc3Object& src)
{
    auto copy = std::make_shared<Mc3::Mc3Object>(src);
    copy->children.clear();
    for (const auto& child : src.children)
        copy->children.push_back(deepCopyObject(*child));
    return copy;
}

inline std::vector<std::shared_ptr<Mc3::Mc3Object>>*
findParentList(std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
               const Mc3::Mc3Object* target)
{
    for (auto& obj : list) {
        if (obj.get() == target) return &list;
        if (!obj->children.empty()) {
            auto* found = findParentList(obj->children, target);
            if (found) return found;
        }
    }
    return nullptr;
}

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

inline const char* objectTypeName(Mc3::ObjectType t)
{
    switch (t) {
        case Mc3::ObjectType::Box:          return "Box";
        case Mc3::ObjectType::Cube:         return "Cube";
        case Mc3::ObjectType::Sphere:       return "Sphere";
        case Mc3::ObjectType::Cylinder:     return "Cylinder";
        case Mc3::ObjectType::Cone:         return "Cone";
        case Mc3::ObjectType::Plane:        return "Plane";
        case Mc3::ObjectType::Mesh:         return "Mesh";
        case Mc3::ObjectType::Extrude:      return "Extrude";
        case Mc3::ObjectType::Group:        return "Group";
        case Mc3::ObjectType::Instance:     return "Instance";
        case Mc3::ObjectType::Union:        return "Union";
        case Mc3::ObjectType::Difference:   return "Difference";
        case Mc3::ObjectType::Intersection: return "Intersection";
        case Mc3::ObjectType::Area:         return "Area";
        default:                            return "Object";
    }
}

inline std::string applyRenamePattern(const std::string& pat, const std::string& origName,
                                      int idx1, const char* typeName)
{
    std::string r = pat;
    auto rep = [&](const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = r.find(from, pos)) != std::string::npos) {
            r.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%03d", idx1);     rep("{index:03d}", buf);
    std::snprintf(buf, sizeof(buf), "%02d", idx1);     rep("{index:02d}", buf);
    std::snprintf(buf, sizeof(buf), "%d",   idx1);     rep("{index}",     buf);
    std::snprintf(buf, sizeof(buf), "%03d", idx1 - 1); rep("{index0:03d}", buf);
    std::snprintf(buf, sizeof(buf), "%02d", idx1 - 1); rep("{index0:02d}", buf);
    std::snprintf(buf, sizeof(buf), "%d",   idx1 - 1); rep("{index0}",     buf);
    rep("{name}", origName);
    rep("{type}", typeName);
    return r;
}

} // namespace MeshCraft
