#pragma once
// Pure editor command algorithms — no CNA / ImGui / SDL / OpenGL dependencies.
// Included by MeshCraftApplication_Commands.cpp and editor_commands_test.cpp.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <numbers>
#include <set>
#include <sstream>
#include <string>
#include <utility>
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

// Mirrors removeFromList() in MeshCraftPrivate.hpp: removes target from list
// (searching recursively into children) wherever it appears.
inline void removeFromListAlg(std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
                              const Mc3::Mc3Object* target)
{
    list.erase(std::remove_if(list.begin(), list.end(),
        [&](const auto& o){ return o.get() == target; }), list.end());
    for (auto& obj : list)
        if (!obj->children.empty())
            removeFromListAlg(obj->children, target);
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

// ── Rename fixup (STAB-0456) ───────────────────────────────────────────────────

// Mc3Channel::targetObject is a plain object-name string, not a stable id
// reference — renaming an object anywhere (Properties panel, hierarchy
// panel, batch rename) must call this immediately after, or every action
// channel that targeted the object's old name becomes silently orphaned
// (safe — no crash, both export and live playback already guard a missing
// target — but the animation stops working with no indication why).
inline void renameObjectInActionsAlg(std::map<std::string, Mc3::Mc3Action>& actions,
                                      const std::string& oldName,
                                      const std::string& newName)
{
    if (oldName.empty() || oldName == newName) return;
    for (auto& [actionName, action] : actions)
        for (auto& ch : action.channels)
            if (ch.targetObject == oldName) ch.targetObject = newName;
}

// ── Batch rename ──────────────────────────────────────────────────────────────

// Applies rename pattern to selected objects (skipping locked ones).
// Index is 1-based, assigned by position in the selection vector.
// Returns count of objects whose name was changed.
// `actions`, when non-null, gets each renamed object's channels fixed up via
// renameObjectInActionsAlg (optional/nullable so existing headless tests that
// construct objects without a full document keep compiling unchanged).
inline int batchRenameObjects(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const std::set<std::string>&                         lockedIds,
    const std::string&                                   pattern,
    std::map<std::string, Mc3::Mc3Action>*                actions = nullptr)
{
    int renamed = 0;
    int idx = 1;
    for (const auto& s : selected) {
        if (lockedIds.count(s->id)) { ++idx; continue; }
        std::string oldName = s->name;
        std::string newName = applyRenamePatternAlg(pattern, s->name, idx,
                                                    objectTypeNameAlg(s->type));
        if (!newName.empty()) {
            s->name = newName;
            if (actions) renameObjectInActionsAlg(*actions, oldName, newName);
            ++renamed;
        }
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

// ── Duplicate / Group / Ungroup (STAB-0280/0281/0282) ─────────────────────────
//
// Mirror the object-tree mutation performed by MeshCraftApplication::
// duplicateSelected() / groupSelected() / ungroupSelected()
// (MeshCraftApplication_Commands.cpp:178-296). Only the document mutation is
// mirrored — the app-state bookkeeping those methods also do (selection_,
// modified_, updateWindowTitle(), recordStep()) doesn't affect whether the
// *document* round-trips through undo/redo, which is what's under test.

// Duplicates each selected object in place (inserted right after the
// original in its parent list) with "_copy" name/id suffixes. Returns the
// new objects.
inline std::vector<std::shared_ptr<Mc3::Mc3Object>> duplicateObjectsAlg(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>&       rootObjects,
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected)
{
    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;
    for (const auto& s : selected) {
        auto* parent = findParentListAlg(rootObjects, s.get());
        if (!parent) continue;
        auto copy = deepCopyObjectAlg(*s);
        copy->name = s->name + "_copy";
        copy->id   = s->id.empty() ? copy->name : s->id + "_copy";
        auto it = std::find_if(parent->begin(), parent->end(),
            [&](const auto& o){ return o.get() == s.get(); });
        if (it != parent->end()) ++it;
        parent->insert(it, copy);
        newObjs.push_back(copy);
    }
    return newObjs;
}

// Groups the selected objects as children of a new Group object, inserted at
// the position of the earliest-selected object. Returns the new group (or
// nullptr if selected is empty).
inline std::shared_ptr<Mc3::Mc3Object> groupObjectsAlg(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>&       rootObjects,
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const std::string&                                   groupName)
{
    if (selected.empty()) return nullptr;

    size_t insertIdx = rootObjects.size();
    for (const auto& s : selected)
        for (size_t i = 0; i < rootObjects.size(); ++i)
            if (rootObjects[i].get() == s.get()) { insertIdx = std::min(insertIdx, i); break; }

    auto group = std::make_shared<Mc3::Mc3Object>();
    group->type = Mc3::ObjectType::Group;
    group->name = groupName;

    for (const auto& s : selected) {
        group->children.push_back(s);
        removeFromListAlg(rootObjects, s.get());
    }
    insertIdx = std::min(insertIdx, rootObjects.size());
    rootObjects.insert(rootObjects.begin() + static_cast<std::ptrdiff_t>(insertIdx), group);
    return group;
}

// Ungroups groupObj: its children are spliced into groupObj's former
// position in its parent list, and groupObj itself is removed. Returns the
// (now top-level-in-that-list) children, or empty if groupObj isn't a
// non-empty Group or isn't found in rootObjects.
inline std::vector<std::shared_ptr<Mc3::Mc3Object>> ungroupObjectAlg(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>& rootObjects,
    const std::shared_ptr<Mc3::Mc3Object>&        groupObj)
{
    if (groupObj->type != Mc3::ObjectType::Group || groupObj->children.empty())
        return {};
    auto children = groupObj->children;
    auto* parentList = findParentListAlg(rootObjects, groupObj.get());
    if (!parentList) return {};
    auto it = std::find_if(parentList->begin(), parentList->end(),
        [&](const auto& o) { return o.get() == groupObj.get(); });
    if (it == parentList->end()) return {};
    auto insertIt = parentList->erase(it);
    for (const auto& child : children) { insertIt = parentList->insert(insertIt, child); ++insertIt; }
    return children;
}

// ── Convert to Definition (STAB-0482) ──────────────────────────────────────────
//
// Mirrors MeshCraftApplication::convertToDefinition()
// (MeshCraftApplication_Commands.cpp) — moves src into doc.definitions
// (transform reset to identity, since a definition is a reusable template)
// and replaces it in-place with an Instance node that preserves src's
// original transform/visibility/tags. Returns the new Instance (its
// `.definition` field is the generated key, e.g. "def_1").
inline std::shared_ptr<Mc3::Mc3Object> convertToDefinitionAlg(
    Mc3::Mc3Document&                       doc,
    const std::shared_ptr<Mc3::Mc3Object>&  src)
{
    int n = 1;
    std::string defKey;
    do { defKey = "def_" + std::to_string(n++); }
    while (doc.definitions.count(defKey));

    auto defObj = deepCopyObjectAlg(*src);
    defObj->id   = defKey;
    defObj->name = defKey;
    defObj->transform.position = {0.0f, 0.0f, 0.0f};
    defObj->transform.rotation = {0.0f, 0.0f, 0.0f};
    defObj->transform.scale    = {1.0f, 1.0f, 1.0f};
    doc.definitions[defKey] = defObj;

    auto inst = std::make_shared<Mc3::Mc3Object>();
    inst->id         = src->id;
    inst->name       = src->name.empty() ? defKey : src->name;
    inst->type       = Mc3::ObjectType::Instance;
    inst->definition = defKey;
    inst->transform  = src->transform;
    inst->visible    = src->visible;
    inst->tags       = src->tags;

    auto* parentList = findParentListAlg(doc.objects, src.get());
    if (parentList) {
        for (auto& obj : *parentList)
            if (obj.get() == src.get()) { obj = inst; break; }
    } else {
        doc.objects.push_back(inst);
    }
    return inst;
}

// ── Break Instance (STAB-0483) ─────────────────────────────────────────────────
//
// Mirrors MeshCraftApplication::flatFindById() — recursive id lookup across
// the whole object tree.
inline Mc3::Mc3Object* flatFindByIdAlg(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& rootObjects,
    const std::string& id)
{
    for (const auto& obj : rootObjects) {
        if (obj->id == id) return obj.get();
        if (!obj->children.empty()) {
            auto* r = flatFindByIdAlg(obj->children, id);
            if (r) return r;
        }
    }
    return nullptr;
}

// Recursively assigns a fresh, document-unique id (derived from each
// object's own name) to obj and every descendant. The original
// breakInstance() only uniquified the *top-level* copy's id — every
// descendant kept the definition template's original id verbatim, so
// breaking a second Instance of the same definition produced exact
// duplicate child ids (a real bug: id is used elsewhere as a set key,
// e.g. `lockedIds_`). `assignedThisWalk` additionally guards against two
// descendants in the *same* subtree colliding with each other.
inline void regenerateSubtreeIdsAlg(
    Mc3::Mc3Object&                                      obj,
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>&  rootObjects,
    std::set<std::string>&                               assignedThisWalk)
{
    int n = 1;
    std::string newId;
    do { newId = obj.name + "_" + std::to_string(n++); }
    while (assignedThisWalk.count(newId) || flatFindByIdAlg(rootObjects, newId));
    obj.id = newId;
    assignedThisWalk.insert(newId);
    for (auto& child : obj.children)
        if (child) regenerateSubtreeIdsAlg(*child, rootObjects, assignedThisWalk);
}

// Mirrors MeshCraftApplication::breakInstance() — expands inst into an
// independent deep copy of its definition's content (preserving inst's own
// name/transform/visibility/tags), replacing inst in place. Every object in
// the copied subtree gets a fresh, document-unique id (see
// regenerateSubtreeIdsAlg). Returns the new object, or nullptr if inst isn't
// a valid Instance or its definition doesn't exist.
inline std::shared_ptr<Mc3::Mc3Object> breakInstanceAlg(
    Mc3::Mc3Document&                      doc,
    const std::shared_ptr<Mc3::Mc3Object>& inst)
{
    if (inst->type != Mc3::ObjectType::Instance) return nullptr;
    auto defIt = doc.definitions.find(inst->definition);
    if (defIt == doc.definitions.end()) return nullptr;

    auto copy = deepCopyObjectAlg(*defIt->second);
    copy->name      = inst->name;
    copy->transform = inst->transform;
    copy->visible   = inst->visible;
    copy->tags      = inst->tags;

    std::set<std::string> assigned;
    regenerateSubtreeIdsAlg(*copy, doc.objects, assigned);

    auto* parentList = findParentListAlg(doc.objects, inst.get());
    if (parentList) {
        for (auto& obj : *parentList)
            if (obj.get() == inst.get()) { obj = copy; break; }
    } else {
        doc.objects.push_back(copy);
    }
    return copy;
}

// ── Align to Object (STAB-0484) ────────────────────────────────────────────────
//
// Mirrors MeshCraftApplication::alignToObject() — sets every OTHER selected
// object's *position* (rotation/scale untouched) to match the first selected
// object's position. Locked objects (by id) are skipped. Returns the count
// of objects actually aligned (0 if fewer than 2 objects are selected — no
// mutation happens in that case, matching the real command's early return).
inline int alignToObjectAlg(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const std::set<std::string>&                        lockedIds)
{
    if (selected.size() < 2) return 0;
    const auto& src = selected.front();
    int aligned = 0;
    for (size_t i = 1; i < selected.size(); ++i) {
        const auto& s = selected[i];
        if (lockedIds.count(s->id)) continue;
        s->transform.position = src->transform.position;
        ++aligned;
    }
    return aligned;
}

// ── Scatter Along Curve (STAB-0485) ────────────────────────────────────────────
//
// Mirrors MeshCraftApplication::scatterAlongCurve() (H4). `count` is the
// *total* number of points along the curve, including the original at index
// 0 — the UI's own live label already says "(count-1) new" next to the
// count slider, so producing `count-1` new copies per source object is the
// documented, intended behavior, not an off-by-one bug.
struct ScatterCurveParamsAlg {
    int   count       = 2;      // total points including the original; must be >= 2
    int   mode        = 0;      // 0 = straight line along `axis`; anything else = arc
    int   axis        = 1;      // 0=X, 1=Y, 2=Z — line direction, or the arc plane's "up" axis
    float spacing     = 1.0f;   // line mode: distance between consecutive points
    float arcAngleDeg = 180.0f; // arc mode: total sweep angle
    float radius      = 1.0f;   // arc mode: arc radius
    float jitter      = 0.0f;   // max per-axis random offset added to each new copy's position
};

// Computes copy index i's position (i=0 is the untouched original's own
// position) relative to src, for either curve mode. Exposed separately so
// it can be tested in isolation from the RNG/document-mutation side.
inline std::array<float,3> scatterCurvePositionAlg(
    const Mc3::Mc3Object& src, int i, const ScatterCurveParamsAlg& p)
{
    float bx = src.transform.position[0];
    float by = src.transform.position[1];
    float bz = src.transform.position[2];

    if (p.mode == 0) {
        float offset = static_cast<float>(i) * p.spacing;
        std::array<float,3> pos = {bx, by, bz};
        pos[p.axis] += offset;
        return pos;
    }
    float t     = static_cast<float>(i) / static_cast<float>(p.count - 1);
    float angle = t * p.arcAngleDeg * (std::numbers::pi_v<float> / 180.0f);
    float dx = p.radius * std::cos(angle) - p.radius; // 0 at angle=0 (i.e. at the source)
    float dz = p.radius * std::sin(angle);
    switch (p.axis) {
    case 0:  return {bx,      by + dx, bz + dz}; // arc in YZ plane (X is up)
    case 1:  return {bx + dx, by,      bz + dz}; // arc in XZ plane (Y is up)
    default: return {bx + dx, by + dz, bz};      // arc in XY plane (Z is up)
    }
}

// For each selected source object, inserts (params.count - 1) new deep
// copies immediately after it in its parent list, positioned along the
// curve (see scatterCurvePositionAlg) with `jitterRng() * params.jitter`
// added per axis. `jitterRng` should return a value in [-1, 1] — pass a
// fixed-seed or always-zero generator for deterministic/testable output.
// Returns only the newly created objects (not the untouched originals).
// No-op (returns empty) if `selected` is empty or params.count < 2.
inline std::vector<std::shared_ptr<Mc3::Mc3Object>> scatterAlongCurveAlg(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>&       rootObjects,
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const ScatterCurveParamsAlg&                        params,
    const std::function<float()>&                       jitterRng)
{
    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;
    if (selected.empty() || params.count < 2) return newObjs;

    for (const auto& src : selected) {
        auto* parentList = findParentListAlg(rootObjects, src.get());
        if (!parentList) continue;
        auto it = std::find_if(parentList->begin(), parentList->end(),
            [&](const auto& o){ return o.get() == src.get(); });
        if (it == parentList->end()) continue;
        auto insertIt = it + 1;

        for (int i = 1; i < params.count; ++i) {
            auto copy = deepCopyObjectAlg(*src);
            copy->name = src->name + "_sc" + std::to_string(i);
            copy->id   = src->id   + "_sc" + std::to_string(i);
            auto p = scatterCurvePositionAlg(*src, i, params);
            copy->transform.position[0] = p[0] + jitterRng() * params.jitter;
            copy->transform.position[1] = p[1] + jitterRng() * params.jitter;
            copy->transform.position[2] = p[2] + jitterRng() * params.jitter;
            insertIt = parentList->insert(insertIt, copy);
            ++insertIt;
            newObjs.push_back(copy);
        }
    }
    return newObjs;
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

// ── AI panel dialog lifecycle (STAB-0299/0300/0301) ───────────────────────────
//
// Mirrors the Reset / Apply to Scene / Save-to-Registry state transitions in
// MeshCraftApplication::drawAiPanel() (MeshCraftApplication_UiAi.cpp:264-380).
// Those transitions are plain field assignments, but they live directly
// inside `if (ImGui::Button(...))` blocks with no separable function today,
// so this mirror tracks presence/absence of the real fields with bools
// (the actual document/error/dialog content isn't what's under test —
// std::optional<Mc3Document> aiPendingDoc_ / std::string aiValidationError_
// / bool regSaveFromAi_ / bool regSaveDlgOpen_).

struct AiPanelStateAlg {
    bool        aiPendingDocSet    = false;  // aiPendingDoc_.has_value()
    bool        validationErrorSet = false;  // !aiValidationError_.empty()
    bool        regSaveFromAi      = false;
    bool        regSaveDlgOpen     = false;
    std::string regSaveDefId;                // regSaveDefId_
};

// Mirrors the "Reset" button body (MeshCraftApplication_UiAi.cpp:369-379):
// always clears the pending result and any validation error; additionally
// closes the registry save dialog, but ONLY if it was opened from this AI
// result (regSaveFromAi_) — a dialog opened independently is left alone.
inline void aiResetAlg(AiPanelStateAlg& st)
{
    st.aiPendingDocSet    = false;
    st.validationErrorSet = false;
    if (st.regSaveFromAi) {
        st.regSaveDlgOpen = false;
        st.regSaveFromAi  = false;
    }
}

// Mirrors the "Apply to Scene" button body (MeshCraftApplication_UiAi.cpp:
// 317-324): intentionally does NOT clear aiPendingDoc_, so "Save to
// Registry" stays available after applying.
inline void aiApplyToSceneAlg(AiPanelStateAlg& st)
{
    (void)st; // no field the real code touches is relevant to lifecycle state
}

// Mirrors the "Save to Registry…" button's Ai-tracking assignments
// (MeshCraftApplication_UiAi.cpp:350-361, the `if (regReady)` body): records
// which definition id to pre-fill and marks the save as AI-sourced. Note
// what this does NOT depend on: it doesn't check or set any "applied to
// scene" flag — the button's visibility gate (line 329) and this handler
// both reference `aiPendingDoc_` alone (STAB-0348: works without a prior
// Apply). `defId` mirrors `aiPendingDoc_->definitions.begin()->first`.
inline void aiSaveToRegistryClickAlg(AiPanelStateAlg& st, const std::string& defId)
{
    st.regSaveDefId  = defId;
    st.regSaveFromAi = true;
}

// Mirrors the definition-source ternary in the "Save Definition to
// Registry" dialog (MeshCraftApplication_UiRegistry.cpp:137-139): when the
// save was initiated from an AI result (regSaveFromAi_) AND that result is
// still present, list aiPendingDoc_'s definitions; otherwise list the scene
// document's (document_.definitions) — STAB-0347.
inline bool registrySaveUsesAiDefinitionsAlg(const AiPanelStateAlg& st)
{
    return st.regSaveFromAi && st.aiPendingDocSet;
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

// ── Merge scene (STAB-0271) ───────────────────────────────────────────────────
//
// Mirrors MeshCraftApplication::mergeSceneFromFile() (MeshCraftApplication_
// FileOps.cpp:255-283), minus pushUndo()/modified_/updateWindowTitle()/
// setStatusMsg(). Textures: skip on key collision (existing document wins).
// Materials: on key collision, suffix with _2, _3, ... until unique, and
// rename the copy's `name` field to match its new key. Objects: appended by
// sharing the same shared_ptr (matches the real code — safe because src is
// always freshly loaded from file and shares no ownership with dst).
// Returns the number of objects appended.

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

    int added = 0;
    for (const auto& obj : src.objects) {
        dst.objects.push_back(obj);
        ++added;
    }
    return added;
}

// ── Save As path resolution (STAB-0272) ───────────────────────────────────────
//
// Mirrors the path-normalization logic in the "Save As" dialog's Save button
// body (MeshCraftApplication_UiOverlays.cpp:1196-1198): a path already ending
// in ".mcb" is saved via the MCB writer as-is; any other path gets ".mc3.xml"
// appended unless it already contains that suffix. Returns {resolvedPath,
// isMcb}.

inline std::pair<std::string, bool> resolveSaveAsPathAlg(const std::string& rawPath)
{
    std::string path = rawPath;
    bool isMcb = path.size() >= 4 && path.substr(path.size() - 4) == ".mcb";
    if (!isMcb && path.find(".mc3.xml") == std::string::npos) path += ".mc3.xml";
    return {path, isMcb};
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

// ── Animation keyframe insertion (STAB-0284) ──────────────────────────────────
//
// Mirrors MeshCraftApplication::insertAnimKeyframes() (MeshCraftApplication_
// Anim.cpp:94-153), minus pushUndo()/modified_/evaluateAndPushAnimOverrides().
// For each requested property: finds the channel for (obj.name, prop) or
// creates one; evaluates the object's *current* transform/visibility field as
// the keyframe value (for TRS/Visible properties) or the channel's currently
// animated value at animTime (for every other property — there's no live
// "current value" on Mc3Object for those, e.g. material/deform channels);
// then either overwrites an existing keyframe within 0.001 of animTime, or
// inserts a new one and re-sorts the channel by time.

inline void insertAnimKeyframesAlg(
    Mc3::Mc3Action&                            action,
    const Mc3::Mc3Object&                      obj,
    const std::vector<Mc3::AnimatedProperty>&  props,
    float                                       animTime)
{
    for (auto prop : props) {
        int ci = -1;
        for (int i = 0; i < (int)action.channels.size(); ++i) {
            if (action.channels[i].targetObject == obj.name &&
                action.channels[i].property == prop) { ci = i; break; }
        }
        if (ci < 0) {
            Mc3::Mc3Channel ch;
            ch.targetObject = obj.name;
            ch.property     = prop;
            action.channels.push_back(std::move(ch));
            ci = static_cast<int>(action.channels.size()) - 1;
        }

        float value = 0.0f;
        switch (prop) {
        case Mc3::AnimatedProperty::PositionX: value = obj.transform.position[0]; break;
        case Mc3::AnimatedProperty::PositionY: value = obj.transform.position[1]; break;
        case Mc3::AnimatedProperty::PositionZ: value = obj.transform.position[2]; break;
        case Mc3::AnimatedProperty::RotationX: value = obj.transform.rotation[0]; break;
        case Mc3::AnimatedProperty::RotationY: value = obj.transform.rotation[1]; break;
        case Mc3::AnimatedProperty::RotationZ: value = obj.transform.rotation[2]; break;
        case Mc3::AnimatedProperty::ScaleX:    value = obj.transform.scale[0];    break;
        case Mc3::AnimatedProperty::ScaleY:    value = obj.transform.scale[1];    break;
        case Mc3::AnimatedProperty::ScaleZ:    value = obj.transform.scale[2];    break;
        case Mc3::AnimatedProperty::Visible:   value = obj.visible ? 1.0f : 0.0f; break;
        default:
            value = Mc3::evaluateChannel(action.channels[ci], animTime); break;
        }

        auto& ch = action.channels[ci];
        bool replaced = false;
        for (auto& kf : ch.keyframes) {
            if (std::abs(kf.time - animTime) < 0.001f) { kf.value = value; replaced = true; break; }
        }
        if (!replaced) {
            Mc3::Mc3Keyframe kf;
            kf.time = animTime; kf.value = value;
            ch.keyframes.push_back(kf);
            std::sort(ch.keyframes.begin(), ch.keyframes.end(),
                [](const Mc3::Mc3Keyframe& a, const Mc3::Mc3Keyframe& b){ return a.time < b.time; });
        }
    }
}

// ── Keybinding persistence format (STAB-0286) ─────────────────────────────────
//
// Mirrors the persistence format used by KeyBind::toString()/fromString() and
// loadKeybindings()/saveKeybindings() (MeshCraftApplication_Keybindings.cpp):
// each binding serializes as "id=[ctrl+][shift+][alt+]KEYNAME" (or an empty
// value when unbound), one "id=value" line per binding; loading re-parses the
// same tokens case-insensitively and only overwrites ids present in the file
// (an id absent from the file keeps its pre-load — i.e. default — value, the
// same merge behavior `initDefaultBindings()` + `loadKeybindings()` produce
// together in the real code). Uses its own small key-name table — distinct
// from the real `Keys::` enum, which lives in CNA and can't be included here
// — because what's under test is the tokenize/join *format*, not any
// particular `Keys::` integer value.

struct KeyBindAlg {
    bool ctrl{false}, shift{false}, alt{false};
    int  key{0};
};

namespace detail {
inline const std::vector<std::pair<int, const char*>>& keyNameTableAlg()
{
    static const std::vector<std::pair<int, const char*>> table = {
        {1, "A"}, {2, "S"}, {3, "Z"}, {4, "F1"}, {5, "Space"}, {6, "Delete"},
    };
    return table;
}
} // namespace detail

inline const char* keyNameAlg(int key)
{
    for (auto& [k, n] : detail::keyNameTableAlg()) if (k == key) return n;
    return "?";
}

inline int keyFromNameAlg(const std::string& n)
{
    std::string upper = n;
    for (auto& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (auto& [k, name] : detail::keyNameTableAlg()) {
        std::string en = name;
        for (auto& c : en) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (en == upper) return k;
    }
    return 0;
}

inline std::string keyBindToStringAlg(const KeyBindAlg& b)
{
    if (!b.key) return "";
    std::string s;
    if (b.ctrl)  s += "ctrl+";
    if (b.shift) s += "shift+";
    if (b.alt)   s += "alt+";
    s += keyNameAlg(b.key);
    return s;
}

inline KeyBindAlg keyBindFromStringAlg(const std::string& raw)
{
    KeyBindAlg b;
    if (raw.empty()) return b;
    std::vector<std::string> parts;
    std::string cur;
    for (char c : raw) {
        if (c == '+') { if (!cur.empty()) { parts.push_back(cur); cur.clear(); } }
        else cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    for (auto& p : parts) {
        std::string pl = p;
        for (auto& c : pl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if      (pl == "ctrl")  b.ctrl  = true;
        else if (pl == "shift") b.shift = true;
        else if (pl == "alt")   b.alt   = true;
        else                    b.key   = keyFromNameAlg(p);
    }
    return b;
}

inline void saveKeybindingsAlg(const std::filesystem::path& path,
                               const std::map<std::string, KeyBindAlg>& bindings)
{
    std::ofstream f(path);
    for (const auto& [id, bind] : bindings)
        f << id << "=" << keyBindToStringAlg(bind) << "\n";
}

inline void loadKeybindingsAlg(const std::filesystem::path& path,
                               std::map<std::string, KeyBindAlg>& bindings)
{
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string id  = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        bindings[id] = keyBindFromStringAlg(val);
    }
}

// ── Preferences persistence (STAB-0287) ───────────────────────────────────────
//
// Mirrors loadPrefs()/savePrefs() (MeshCraftApplication_FileOps.cpp:296-329):
// a flat "key=value" ini file for the six scalar preference fields. An
// unknown key or an unparseable value is silently skipped — one bad line
// must not prevent the rest of the file from loading — and a missing file
// leaves every field at its pre-load (caller-supplied default) value.
// applyTheme()'s ImGui side effect is intentionally not mirrored (rendering
// only, not data).

struct PrefsAlg {
    float autoSaveInterval{60.0f};
    float snapTranslate{1.0f};
    float snapRotate{15.0f};
    float snapScale{0.1f};
    float gridSpacing{1.0f};
    int   theme{0};
};

inline void savePrefsAlg(const std::filesystem::path& path, const PrefsAlg& p)
{
    std::ofstream f(path);
    if (!f) return;
    f << "autoSaveInterval=" << p.autoSaveInterval << "\n";
    f << "snapTranslate="    << p.snapTranslate    << "\n";
    f << "snapRotate="       << p.snapRotate       << "\n";
    f << "snapScale="        << p.snapScale        << "\n";
    f << "gridSpacing="      << p.gridSpacing      << "\n";
    f << "theme="            << p.theme            << "\n";
}

inline void loadPrefsAlg(const std::filesystem::path& path, PrefsAlg& p)
{
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        try {
            if      (key == "autoSaveInterval") p.autoSaveInterval = std::stof(val);
            else if (key == "snapTranslate")    p.snapTranslate    = std::stof(val);
            else if (key == "snapRotate")       p.snapRotate       = std::stof(val);
            else if (key == "snapScale")        p.snapScale        = std::stof(val);
            else if (key == "gridSpacing")      p.gridSpacing      = std::stof(val);
            else if (key == "theme")            p.theme            = std::stoi(val);
        } catch (...) {}
    }
}

// ── Macro save/load (STAB-0292) ───────────────────────────────────────────────
//
// Mirrors saveMacro()/loadMacro() (MeshCraftApplication_Macro.cpp:94-131):
// each step is one tab-separated line ("verb\targ1\targ2..."); loading skips
// blank lines. MacroStepAlg mirrors the real (CNA-coupled, declared inside
// MeshCraftApplication.hpp) MacroStep struct's fields exactly.

struct MacroStepAlg {
    std::string              verb;
    std::vector<std::string> args;
};

inline void saveMacroAlg(const std::filesystem::path& path,
                         const std::vector<MacroStepAlg>& steps)
{
    std::ofstream f(path);
    for (const auto& step : steps) {
        f << step.verb;
        for (const auto& arg : step.args) f << '\t' << arg;
        f << '\n';
    }
}

inline std::vector<MacroStepAlg> loadMacroAlg(const std::filesystem::path& path)
{
    std::vector<MacroStepAlg> steps;
    std::ifstream f(path);
    if (!f) return steps;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        MacroStepAlg step;
        std::istringstream ss(line);
        std::string tok;
        bool first = true;
        while (std::getline(ss, tok, '\t')) {
            if (first) { step.verb = tok; first = false; }
            else step.args.push_back(tok);
        }
        if (!step.verb.empty()) steps.push_back(std::move(step));
    }
    return steps;
}

// ── Hierarchy panel filter (STAB-0306) ────────────────────────────────────────
//
// Mirrors matchesType()/matchesFilter() and the filter-active flags
// (Scene/SceneHierarchyPanel.cpp:206-252): an object matches if it (or any
// descendant, recursively — so a matching child keeps its ancestors visible)
// passes every ACTIVE filter (AND mode, the default) or any active filter
// (OR mode, `filterOr_`); an inactive filter is never checked, and with no
// filter active at all everything matches. `hierarchyAnyFilterActiveAlg` is
// what the real drawHierarchy() loop must gate the skip-decision on — it
// used to gate on the text-search flag alone, which silently made a
// type/layer/tag/material-only filter (no search text typed) do nothing;
// fixed alongside this mirror (SceneHierarchyPanel.cpp:269/276 now check
// `anyFiltering`, matching this mirror's `hierarchyAnyFilterActiveAlg`).

struct HierarchyFilterAlg {
    std::string textLower;      // already-lowercased search text; empty = inactive
    int         typeFilter{0};  // 0 = any; see hierarchyMatchesTypeAlg
    std::string layerFilter;    // empty = inactive
    std::string tagFilter;      // empty = inactive
    std::string matFilter;      // empty = inactive
    bool        orMode{false};
};

inline bool hierarchyMatchesTypeAlg(int typeFilter, Mc3::ObjectType t)
{
    using OT = Mc3::ObjectType;
    switch (typeFilter) {
    case 1:
        return t == OT::Box || t == OT::Cube || t == OT::Sphere ||
               t == OT::Cylinder || t == OT::Cone || t == OT::Plane ||
               t == OT::Torus || t == OT::Capsule || t == OT::Disk ||
               t == OT::Grid || t == OT::IcoSphere;
    case 2: return t == OT::Mesh;
    case 3: return t == OT::Group || t == OT::Area;
    case 4: return t == OT::Instance;
    case 5: return t == OT::Union || t == OT::Difference || t == OT::Intersection;
    case 6: return t == OT::Extrude;
    default: return true;
    }
}

inline bool hierarchyAnyFilterActiveAlg(const HierarchyFilterAlg& f)
{
    return !f.textLower.empty() || f.typeFilter != 0 || !f.layerFilter.empty() ||
           !f.tagFilter.empty() || !f.matFilter.empty();
}

inline bool hierarchyFilterMatchesAlg(const HierarchyFilterAlg& f, const Mc3::Mc3Object& o)
{
    if (!hierarchyAnyFilterActiveAlg(f)) return true;

    bool filtering     = !f.textLower.empty();
    bool typeFiltering = (f.typeFilter != 0);
    bool layFiltering  = !f.layerFilter.empty();
    bool tagFiltering  = !f.tagFilter.empty();
    bool matFiltering  = !f.matFilter.empty();

    std::string nl = o.name.empty() ? o.id : o.name;
    for (auto& ch : nl) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    bool textOk = !filtering     || nl.find(f.textLower) != std::string::npos;
    bool typeOk = !typeFiltering || hierarchyMatchesTypeAlg(f.typeFilter, o.type);
    bool layOk  = !layFiltering  || (o.layer == f.layerFilter);
    bool tagOk  = !tagFiltering  || std::any_of(o.tags.begin(), o.tags.end(),
                                        [&](const std::string& t){ return t == f.tagFilter; });
    bool matOk  = !matFiltering  || (o.material == f.matFilter);
    bool selfMatch = f.orMode
        ? (   (filtering     && textOk)
           || (typeFiltering && typeOk)
           || (layFiltering  && layOk)
           || (tagFiltering  && tagOk)
           || (matFiltering  && matOk))
        : (textOk && typeOk && layOk && tagOk && matOk);
    if (selfMatch) return true;
    for (const auto& c : o.children) if (hierarchyFilterMatchesAlg(f, *c)) return true;
    return false;
}

// ── Material color resolution fallback (STAB-0304) ────────────────────────────
//
// Mirrors SceneRenderer::materialColor() (Renderer/SceneRenderer.cpp:
// 368-381), minus the CNA `Color` type (returns a plain clamped RGBA float
// array instead): an empty or unresolvable material id — e.g. the object's
// `material` attribute references a definition that was deleted, renamed, or
// never existed in a hand-edited/AI-generated file — falls back to a fixed
// default gray rather than crashing or leaving the object unrendered.

inline std::array<float,4> materialColorAlg(const std::string& matId, const Mc3::Mc3Document& doc)
{
    if (!matId.empty()) {
        auto it = doc.materials.find(matId);
        if (it != doc.materials.end()) {
            const auto& bc = it->second.baseColor;
            return { std::clamp(bc[0], 0.0f, 1.0f), std::clamp(bc[1], 0.0f, 1.0f),
                     std::clamp(bc[2], 0.0f, 1.0f), std::clamp(bc[3], 0.0f, 1.0f) };
        }
    }
    return { 180.0f/255.0f, 180.0f/255.0f, 180.0f/255.0f, 1.0f };
}

// ── Undo/redo stack depth cap (STAB-0481) ─────────────────────────────────────
//
// Mirrors the push-then-trim pattern duplicated at all three undo/redo stack
// mutation sites — pushUndo() (MeshCraftApplication_Commands.cpp:323-329),
// and the undo/redo key handlers' opposite-stack pushes
// (MeshCraftApplication_Keyboard.cpp:44-69): push the new snapshot, then if
// the stack now exceeds the cap, drop the OLDEST entry (index 0) — so the
// stack always holds at most `cap` entries, always the most recent ones.

template <class T>
inline void pushWithCapAlg(std::vector<T>& stack, T value, int cap)
{
    stack.push_back(std::move(value));
    if (static_cast<int>(stack.size()) > cap)
        stack.erase(stack.begin());
}

} // namespace MeshCraft
