#pragma once
// Pure editor command algorithms — no CNA / ImGui / SDL / OpenGL dependencies.
// Included by MeshCraftApplication_Commands.cpp and editor_commands_test.cpp.

#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace MeshCraft {

// ── Object type name ──────────────────────────────────────────────────────────

inline const char* objectTypeNameAlg(Mc3::ObjectType t)
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

// ── Deep copy ─────────────────────────────────────────────────────────────────

inline std::shared_ptr<Mc3::Mc3Object> deepCopyObjectAlg(const Mc3::Mc3Object& src)
{
    auto copy = std::make_shared<Mc3::Mc3Object>(src);
    copy->children.clear();
    for (const auto& child : src.children)
        copy->children.push_back(deepCopyObjectAlg(*child));
    return copy;
}

// ── Tree helpers ──────────────────────────────────────────────────────────────

inline std::vector<std::shared_ptr<Mc3::Mc3Object>>*
findParentListAlg(std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
                  const Mc3::Mc3Object* target)
{
    for (auto& obj : list) {
        if (obj.get() == target) return &list;
        if (!obj->children.empty()) {
            auto* found = findParentListAlg(obj->children, target);
            if (found) return found;
        }
    }
    return nullptr;
}

// ── Rename pattern ────────────────────────────────────────────────────────────
// Tokens: {name}, {type}, {index}, {index:02d}, {index:03d},
//         {index0}, {index0:02d}, {index0:03d}

inline std::string applyRenamePatternAlg(const std::string& pat,
                                         const std::string& origName,
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

// ── Batch rename ──────────────────────────────────────────────────────────────

// Applies rename pattern to selected objects (skipping locked ones).
// Index is 1-based, assigned by position in the selection vector.
// Returns count of objects whose name was changed.
inline int batchRenameObjects(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const std::set<std::string>&                         lockedIds,
    const std::string&                                   pattern)
{
    int renamed = 0;
    int idx = 1;
    for (const auto& s : selected) {
        if (lockedIds.count(s->id)) { ++idx; continue; }
        std::string newName = applyRenamePatternAlg(pattern, s->name, idx,
                                                    objectTypeNameAlg(s->type));
        if (!newName.empty()) { s->name = newName; ++renamed; }
        ++idx;
    }
    return renamed;
}

// ── Find-replace helpers ──────────────────────────────────────────────────────

namespace detail {

inline std::string replaceAllInString(const std::string& src,
                                      const std::string& findStr,
                                      const std::string& replStr,
                                      bool caseSensitive)
{
    std::string haystack = src;
    std::string needle   = findStr;
    if (!caseSensitive) {
        for (auto& c : haystack) c = (char)std::tolower((unsigned char)c);
        for (auto& c : needle)   c = (char)std::tolower((unsigned char)c);
    }
    std::string result;
    size_t lastPos = 0, pos;
    bool found = false;
    while ((pos = haystack.find(needle, lastPos)) != std::string::npos) {
        result += src.substr(lastPos, pos - lastPos);
        result += replStr;
        lastPos = pos + needle.size();
        found = true;
    }
    if (!found) return src;
    result += src.substr(lastPos);
    return result;
}

inline void walkFindReplace(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>& objects,
    const std::set<std::string>&                   lockedIds,
    const std::string&                             findStr,
    const std::string&                             replStr,
    bool                                           caseSensitive,
    bool                                           selectedOnly,
    const std::set<std::string>&                   selectedIds,
    bool                                           apply,
    int&                                           count)
{
    for (auto& o : objects) {
        if (!lockedIds.count(o->id)) {
            bool inScope = !selectedOnly || selectedIds.count(o->id);
            if (inScope) {
                std::string replaced = replaceAllInString(o->name, findStr, replStr, caseSensitive);
                if (replaced != o->name) {
                    ++count;
                    if (apply) o->name = replaced;
                }
            }
        }
        walkFindReplace(o->children, lockedIds, findStr, replStr,
                        caseSensitive, selectedOnly, selectedIds, apply, count);
    }
}

} // namespace detail

// ── Find-replace names ────────────────────────────────────────────────────────

// Returns count of objects whose names would change (dry run, no mutation).
inline int countFindReplaceMatches(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& objects,
    const std::set<std::string>&                         lockedIds,
    const std::string&                                   findStr,
    const std::string&                                   replStr,
    bool                                                 caseSensitive,
    bool                                                 selectedOnly,
    const std::set<std::string>&                         selectedIds)
{
    int count = 0;
    auto mutableObjects = const_cast<std::vector<std::shared_ptr<Mc3::Mc3Object>>&>(objects);
    detail::walkFindReplace(mutableObjects, lockedIds, findStr, replStr,
                            caseSensitive, selectedOnly, selectedIds,
                            /*apply=*/false, count);
    return count;
}

// Applies find-replace to all object names in the tree.
// Returns count of objects renamed.
inline int applyFindReplaceNames(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>& objects,
    const std::set<std::string>&                   lockedIds,
    const std::string&                             findStr,
    const std::string&                             replStr,
    bool                                           caseSensitive,
    bool                                           selectedOnly,
    const std::set<std::string>&                   selectedIds)
{
    int count = 0;
    detail::walkFindReplace(objects, lockedIds, findStr, replStr,
                            caseSensitive, selectedOnly, selectedIds,
                            /*apply=*/true, count);
    return count;
}

// ── Linear array duplicate ────────────────────────────────────────────────────

// Duplicates each source object (count-1) times along the given axis (0=X,1=Y,2=Z).
// Inserts copies into their parent list immediately after the source.
// If relative=true, position = source_pos + i*spacing; else position = i*spacing.
// Returns all newly created objects.
inline std::vector<std::shared_ptr<Mc3::Mc3Object>> arrayDuplicateObjects(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>&       rootObjects,
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& sources,
    int   count,
    int   axis,
    float spacing,
    bool  relative)
{
    count = std::max(2, count);
    axis  = std::clamp(axis, 0, 2);

    std::vector<std::shared_ptr<Mc3::Mc3Object>> created;

    for (const auto& src : sources) {
        auto* parentList = findParentListAlg(rootObjects, src.get());
        if (!parentList) continue;
        auto it = std::find_if(parentList->begin(), parentList->end(),
            [&](const auto& o){ return o.get() == src.get(); });
        if (it == parentList->end()) continue;
        auto insertIt = it + 1;

        const float basePos = src->transform.position[axis];
        for (int i = 1; i < count; ++i) {
            auto copy = deepCopyObjectAlg(*src);
            copy->name = src->name + "_" + std::to_string(i);
            copy->id   = src->id   + "_arr" + std::to_string(i);
            copy->transform.position[axis] = relative
                ? basePos + static_cast<float>(i) * spacing
                : static_cast<float>(i) * spacing;
            insertIt = parentList->insert(insertIt, copy);
            ++insertIt;
            created.push_back(copy);
        }
    }
    return created;
}

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

} // namespace MeshCraft
