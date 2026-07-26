#pragma once
// Pure editor command algorithms — no CNA / ImGui / SDL / OpenGL dependencies.
// Included by MeshCraftApplication_Commands.cpp and editor_commands_test.cpp.

#include <MeshCraft/Editor/ObjectTypeName.hpp>
#include <MeshCraft/CoordinateSystemAlgorithms.hpp>
#include <MeshCraft/Editor/SelectionManager.hpp>
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
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MeshCraft {

// Convert a document in-place to the editor's native right-handed Y-up
// convention without silently changing it merely on load.  Object trees are
// preserved under one explicit -90 degree X-rotation group, which avoids
// lossy Euler decomposition and keeps animations/instances/CSG semantics
// intact. Scene-level values that are not children (lights and cameras) are
// converted directly. A rotation-authored camera is made target-authored:
// the editor uses only its forward vector for this representation, so this
// preserves the actual view direction while removing an otherwise lossy
// Euler basis conversion.
inline bool normalizeCoordinateSystemToYUpAlg(Mc3::Mc3Document& document)
{
    if (!usesRightHandedZUpAlg(document.coordinateSystem)) return false;

    for (auto& light : document.lights) {
        if (light.type == Mc3::LightType::Directional || light.type == Mc3::LightType::Spot)
            light.direction = coordinateToYUpAlg(document.coordinateSystem, light.direction);
        if (light.type == Mc3::LightType::Point || light.type == Mc3::LightType::Spot)
            light.position = coordinateToYUpAlg(document.coordinateSystem, light.position);
    }

    for (auto& camera : document.cameras) {
        if (camera.rotation.has_value()) {
            // Roll does not affect an MC3 camera's look direction. This
            // matches SceneRenderer::cameraForwardFromRotation()'s purpose
            // without pulling CNA math into this header-only algorithm.
            constexpr float radiansPerDegree = std::numbers::pi_v<float> / 180.0f;
            const float pitch = (*camera.rotation)[0] * radiansPerDegree;
            const float yaw   = (*camera.rotation)[1] * radiansPerDegree;
            const std::array<float, 3> forward{
                std::sin(yaw) * std::cos(pitch),
                std::sin(pitch),
                -std::cos(yaw) * std::cos(pitch)};
            camera.target = {camera.position[0] + forward[0],
                             camera.position[1] + forward[1],
                             camera.position[2] + forward[2]};
            camera.rotation.reset();
        }
        camera.position = coordinateToYUpAlg(document.coordinateSystem, camera.position);
        camera.target   = coordinateToYUpAlg(document.coordinateSystem, camera.target);
    }

    auto conversionRoot = std::make_shared<Mc3::Mc3Object>();
    conversionRoot->type = Mc3::ObjectType::Group;
    conversionRoot->name = "Coordinate system normalized to Y-up";
    conversionRoot->transform.rotation[0] = -90.0f;
    conversionRoot->children = std::move(document.objects);
    document.objects = {std::move(conversionRoot)};
    document.coordinateSystem = std::string(kRightHandedYUpCoordinateSystem);
    return true;
}

// ── Object type name ──────────────────────────────────────────────────────────

// Thin alias for the canonical mapping in MeshCraft/Editor/ObjectTypeName.hpp.
// Kept so existing *Alg call sites read consistently; both resolve to the one
// exhaustive definition, so they can no longer drift apart.
inline const char* objectTypeNameAlg(Mc3::ObjectType t) { return objectTypeName(t); }

// ── Deep copy ─────────────────────────────────────────────────────────────────

// SYS-W1-05: same cycle-protection rationale as deepCopyObj() in
// MeshCraftPrivate.hpp (see its own comment) -- this is the more heavily
// used of the two, called directly from ordinary user actions (Duplicate,
// Group, Convert to Definition, Break Instance, macro playback), so an
// unguarded cycle here crashes on a single click, not just on undo.
inline std::shared_ptr<Mc3::Mc3Object> deepCopyObjectAlg(const Mc3::Mc3Object& src)
{
    static thread_local int depth = 0;
    struct DepthGuard {
        DepthGuard() {
            if (++depth > 256) {
                --depth;
                throw std::runtime_error(
                    "deepCopyObjectAlg: object nesting exceeds 256 levels (cyclic "
                    "Mc3Object::children graph?)");
            }
        }
        ~DepthGuard() { --depth; }
        DepthGuard(const DepthGuard&) = delete;
    } guard;

    auto copy = std::make_shared<Mc3::Mc3Object>(src);
    copy->children.clear();
    for (const auto& child : src.children)
        copy->children.push_back(deepCopyObjectAlg(*child));
    return copy;
}

// ── Tree helpers ──────────────────────────────────────────────────────────────

// SYS-W1-06: same cycle-protection rationale as deepCopyObjectAlg's own
// comment above -- a cyclic Mc3Object::children graph (only possible via
// C++-API misuse, never from XML parsing) would otherwise recurse forever
// here too whenever `target` isn't found in the cyclic portion.
inline std::vector<std::shared_ptr<Mc3::Mc3Object>>*
findParentListAlg(std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
                  const Mc3::Mc3Object* target)
{
    static thread_local int depth = 0;
    struct DepthGuard {
        DepthGuard() {
            if (++depth > 256) {
                --depth;
                throw std::runtime_error(
                    "findParentListAlg: object nesting exceeds 256 levels (cyclic "
                    "Mc3Object::children graph?)");
            }
        }
        ~DepthGuard() { --depth; }
        DepthGuard(const DepthGuard&) = delete;
    } guard;

    for (auto& obj : list) {
        if (obj.get() == target) return &list;
        if (!obj->children.empty()) {
            auto* found = findParentListAlg(obj->children, target);
            if (found) return found;
        }
    }
    return nullptr;
}

// Removes target from list (searching recursively into children) wherever
// it appears. (AUD-033: this used to note "mirrors removeFromList() in
// MeshCraftPrivate.hpp" -- that byte-identical duplicate was deleted;
// this is now the only implementation.)
// SYS-W1-06: same cycle-protection rationale as findParentListAlg above.
inline void removeFromListAlg(std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
                              const Mc3::Mc3Object* target)
{
    static thread_local int depth = 0;
    struct DepthGuard {
        DepthGuard() {
            if (++depth > 256) {
                --depth;
                throw std::runtime_error(
                    "removeFromListAlg: object nesting exceeds 256 levels (cyclic "
                    "Mc3Object::children graph?)");
            }
        }
        ~DepthGuard() { --depth; }
        DepthGuard(const DepthGuard&) = delete;
    } guard;

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
    // AUD-034: was missing here (though present in the near-identical
    // exportSubtreeAsTemplate, MeshCraftApplication_Commands.cpp), so
    // Convert-to-Definition on an object assigned to a named layer silently
    // moved the resulting Instance to the default layer.
    inst->layer      = src->layer;

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
// e.g. `ObjectLockState`). `assignedThisWalk` additionally guards against two
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
    auto defIt = doc.definitions.find(inst->resolvedInstanceDefinitionKey());
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

// ── Proportional editing falloff (H1 / STAB-0489) ─────────────────────────────
//
// Mirrors the Gaussian-falloff nearby-object influence applied during a
// Move-tool drag when propEditEnabled_ is on (the applyFalloff lambda in
// MeshCraftApplication_Mouse.cpp's handleMouseInput()): every unselected,
// unlocked object within propEditRadius_ of the selection's center gets
// nudged by (deltaX,deltaY,deltaZ) scaled by a Gaussian weight, sigma chosen
// so the weight is ~5% at the radius edge. deltaX/Y/Z are the already-
// resolved world-space move delta (delta*axis), since axis resolution is
// mouse/gizmo-driven and out of scope for a CNA-free mirror.

inline float proportionalFalloffWeightAlg(float distSq, float radius)
{
    if (radius <= 0.0f) return 0.0f;
    float r2 = radius * radius;
    if (distSq >= r2) return 0.0f;
    float sigSq = r2 / 9.0f; // reaches ~5% at radius edge
    return std::exp(-distSq / (2.0f * sigSq));
}

// Returns the number of non-selected, non-locked objects actually nudged.
// No-op (returns 0) if `selected` is empty or radius <= 0.
inline int applyProportionalFalloffAlg(
    std::vector<std::shared_ptr<Mc3::Mc3Object>>&       rootObjects,
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const std::set<std::string>&                        lockedIds,
    float deltaX, float deltaY, float deltaZ,
    float radius)
{
    if (selected.empty() || radius <= 0.0f) return 0;

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    for (const auto& s : selected) {
        cx += s->transform.position[0];
        cy += s->transform.position[1];
        cz += s->transform.position[2];
    }
    float n = static_cast<float>(selected.size());
    cx /= n; cy /= n; cz /= n;

    std::set<const Mc3::Mc3Object*> selPtrs;
    for (const auto& s : selected) selPtrs.insert(s.get());

    int affected = 0;
    std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> walk;
    walk = [&](std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
        for (auto& obj : list) {
            if (!selPtrs.count(obj.get()) && !lockedIds.count(obj->id)) {
                float ddx = obj->transform.position[0] - cx;
                float ddy = obj->transform.position[1] - cy;
                float ddz = obj->transform.position[2] - cz;
                float weight = proportionalFalloffWeightAlg(ddx*ddx + ddy*ddy + ddz*ddz, radius);
                if (weight > 0.0f) {
                    obj->transform.position[0] += deltaX * weight;
                    obj->transform.position[1] += deltaY * weight;
                    obj->transform.position[2] += deltaZ * weight;
                    ++affected;
                }
            }
            walk(obj->children);
        }
    };
    walk(rootObjects);
    return affected;
}

// ── Vertex snap on Shift+drag (STAB-0490) ─────────────────────────────────────
//
// Mirrors the "Vertex snap (Shift)" block in handleMouseInput()
// (MeshCraftApplication_Mouse.cpp): finds the nearest unselected object's
// position to (refX,refY,refZ) within `threshold`; if found, offsets every
// selected, unlocked object by the delta from the reference point to that
// nearest position. Returns true iff a snap target was found and applied
// (false leaves every object untouched).

inline bool vertexSnapToNearestAlg(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& rootObjects,
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const std::set<std::string>&                        lockedIds,
    float refX, float refY, float refZ,
    float threshold)
{
    if (selected.empty()) return false;

    std::set<const Mc3::Mc3Object*> selPtrs;
    for (const auto& s : selected) selPtrs.insert(s.get());

    float bestDist = threshold;
    float bestX = refX, bestY = refY, bestZ = refZ;
    bool  found = false;

    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> findNearest;
    findNearest = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
        for (const auto& obj : list) {
            if (!selPtrs.count(obj.get())) {
                float ddx = obj->transform.position[0] - refX;
                float ddy = obj->transform.position[1] - refY;
                float ddz = obj->transform.position[2] - refZ;
                float d = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
                if (d < bestDist) {
                    bestDist = d;
                    bestX = obj->transform.position[0];
                    bestY = obj->transform.position[1];
                    bestZ = obj->transform.position[2];
                    found = true;
                }
            }
            findNearest(obj->children);
        }
    };
    findNearest(rootObjects);
    if (!found) return false;

    float offX = bestX - refX, offY = bestY - refY, offZ = bestZ - refZ;
    for (const auto& s : selected) {
        if (lockedIds.count(s->id)) continue;
        s->transform.position[0] += offX;
        s->transform.position[1] += offY;
        s->transform.position[2] += offZ;
    }
    return true;
}

// ── Rotation drag + angle snap (STAB-0491) ────────────────────────────────────
//
// Mirrors the rotate-gizmo drag loop in handleMouseInput() (rotation
// branch): adds `delta` degrees to axis `axIdx` of every selected, unlocked
// object's rotation, then rounds to the nearest `snapIncrement` if
// `shouldSnap` is true — the real code resolves `shouldSnap` as
// `snapEnabled_ || ctrlHeld` (a momentary Ctrl override on top of the
// persistent Snap-to-grid toggle, rotation-only — Move/Scale drags only
// check `snapEnabled_`, confirmed intentional by the matching "Ctrl or snap
// grid" status-indicator condition in MeshCraftApplication_UiOverlays.cpp).
// Returns the number of objects actually rotated (locked ones skipped).
inline int applyRotationDragAlg(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const std::set<std::string>&                        lockedIds,
    int axIdx, float delta, bool shouldSnap, float snapIncrement)
{
    int rotated = 0;
    for (const auto& s : selected) {
        if (lockedIds.count(s->id)) continue;
        float& r = s->transform.rotation[axIdx];
        r += delta;
        if (shouldSnap)
            r = std::round(r / snapIncrement) * snapIncrement;
        ++rotated;
    }
    return rotated;
}

// ── Select Children (STAB-0492) ───────────────────────────────────────────────
//
// Mirrors selectChildren() (MeshCraftApplication_Commands.cpp): flattens
// every descendant of the given children list at any depth (not just direct
// children), in pre-order (parent before its own children).
inline std::vector<std::shared_ptr<Mc3::Mc3Object>> flattenDescendantsAlg(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& children)
{
    std::vector<std::shared_ptr<Mc3::Mc3Object>> out;
    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> addAll;
    addAll = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
        for (const auto& c : list) {
            out.push_back(c);
            addAll(c->children);
        }
    };
    addAll(children);
    return out;
}

// ── Lock-aware selection dry-run (NEXT.md task, 2026-07-19) ───────────────────
//
// Several selection commands in MeshCraftApplication_Commands.cpp
// (deleteSelected, dropSelectedToGroundPlane, groupScaleSelected,
// randomizeTransformSelected, resetPivot) skip locked objects inside their
// own mutation loop, but must decide BEFORE calling pushUndo() whether
// there is anything to do at all -- if the selection is empty or every
// targeted object is locked, nothing would actually mutate, so
// pushUndo()/modified_=true must be skipped too. Mirrors
// findReplaceNames()'s existing dry-run-then-commit pattern (via
// countFindReplaceMatches() above): compute the dry-run result first,
// bail out early if it says there's nothing to do.
inline bool anySelectedUnlockedAlg(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const std::set<std::string>&                        lockedIds)
{
    for (const auto& o : selected)
        if (!lockedIds.count(o->id)) return true;
    return false;
}

// ── Group Scale (H14 / STAB-0495) ─────────────────────────────────────────────
//
// Mirrors groupScaleSelected() (MeshCraftApplication_Commands.cpp): scales
// every selected, unlocked object's own transform.scale by `factor`, and
// moves its position away from (or toward, for factor<1) the group's
// centroid (the average position of every selected object, including locked
// ones — only the actual move/scale application skips locked objects) by
// that same factor. E.g. objects at (0,0,0) and (2,0,0) with factor=2 move
// to (-1,0,0) and (3,0,0) (centroid 1,0,0, each pushed twice as far from it).
// Returns the number of objects actually scaled (locked ones skipped).
inline int groupScaleAlg(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selected,
    const std::set<std::string>&                        lockedIds,
    float factor)
{
    if (selected.empty()) return 0;

    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    for (const auto& o : selected) {
        cx += o->transform.position[0];
        cy += o->transform.position[1];
        cz += o->transform.position[2];
    }
    float n = static_cast<float>(selected.size());
    cx /= n; cy /= n; cz /= n;

    int scaled = 0;
    for (const auto& o : selected) {
        if (lockedIds.count(o->id)) continue;
        o->transform.position[0] = cx + (o->transform.position[0] - cx) * factor;
        o->transform.position[1] = cy + (o->transform.position[1] - cy) * factor;
        o->transform.position[2] = cz + (o->transform.position[2] - cz) * factor;
        o->transform.scale[0] *= factor;
        o->transform.scale[1] *= factor;
        o->transform.scale[2] *= factor;
        ++scaled;
    }
    return scaled;
}

// ── Viewport ray-cast picking (STAB-0503) ────────────────────────────────────
//
// Mirrors the ray-cast object picking in handleMouseInput()
// (MeshCraftApplication_Mouse.cpp). Any visible object gets a default
// 0.5-unit half-extent AABB (scaled by its transform scale); primitive
// shapes (Box/Sphere/Cylinder/Cone/Plane) use their actual dimensions
// instead. This must NOT be gated on `obj.primitive` being set — an
// earlier version of the real code only tested primitive-typed objects,
// silently making every Instance/Mesh/Group/Extrude/CSG object unclickable
// in the viewport (found and fixed by STAB-0503), inconsistent with
// box-select, which already selects by screen position regardless of type.

inline void objectAABBAlg(const Mc3::Mc3Object& obj,
                           std::array<float,3>& bMin, std::array<float,3>& bMax)
{
    const auto& t = obj.transform;
    float opx = t.position[0], opy = t.position[1], opz = t.position[2];
    float osx = t.scale[0],    osy = t.scale[1],    osz = t.scale[2];
    float hx = 0.5f, hy = 0.5f, hz = 0.5f;
    if (obj.primitive) {
        const auto& p = *obj.primitive;
        switch (p.primitiveType) {
        case Mc3::PrimitiveType::Box: case Mc3::PrimitiveType::Cube:
            hx = p.size[0]*0.5f; hy = p.size[1]*0.5f; hz = p.size[2]*0.5f; break;
        case Mc3::PrimitiveType::Sphere:
            hx = hy = hz = p.radius; break;
        case Mc3::PrimitiveType::Cylinder: case Mc3::PrimitiveType::Cone:
            hx = hz = p.radius; hy = p.height*0.5f; break;
        case Mc3::PrimitiveType::Plane:
            hx = p.size[0]*0.5f; hy = 0.05f; hz = p.size[1]*0.5f; break;
        default: break;
        }
    }
    hx *= std::abs(osx); hy *= std::abs(osy); hz *= std::abs(osz);
    bMin = {opx-hx, opy-hy, opz-hz};
    bMax = {opx+hx, opy+hy, opz+hz};
}

inline bool rayAABBIntersectAlg(
    const std::array<float,3>& rayOrig, const std::array<float,3>& rayDir,
    const std::array<float,3>& bMin, const std::array<float,3>& bMax,
    float& tHit)
{
    float tNear = 0.0f, tFar = 1e30f;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(rayDir[i]) < 1e-9f) {
            if (rayOrig[i] < bMin[i] || rayOrig[i] > bMax[i]) return false;
        } else {
            float t1 = (bMin[i] - rayOrig[i]) / rayDir[i];
            float t2 = (bMax[i] - rayOrig[i]) / rayDir[i];
            if (t1 > t2) std::swap(t1, t2);
            tNear = std::max(tNear, t1);
            tFar  = std::min(tFar,  t2);
            if (tNear > tFar) return false;
        }
    }
    tHit = tNear;
    return tNear >= 0.0f;
}

// Recursively finds the closest visible object (any type) whose AABB the
// ray hits, or nullptr if none. Matches the "closest wins" semantics of the
// real click-to-select handler.
inline std::shared_ptr<Mc3::Mc3Object> pickObjectByRayAlg(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& rootObjects,
    const std::array<float,3>& rayOrig, const std::array<float,3>& rayDir)
{
    float bestT = 1e30f;
    std::shared_ptr<Mc3::Mc3Object> bestObj;

    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> testList;
    testList = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
        for (const auto& obj : list) {
            if (!obj || !obj->visible) continue;
            std::array<float,3> bMin, bMax;
            objectAABBAlg(*obj, bMin, bMax);
            float tHit = 0.0f;
            if (rayAABBIntersectAlg(rayOrig, rayDir, bMin, bMax, tHit) && tHit < bestT) {
                bestT = tHit; bestObj = obj;
            }
            if (!obj->children.empty()) testList(obj->children);
        }
    };
    testList(rootObjects);
    return bestObj;
}

// Resolves what a viewport click should do to the current selection, given
// the ray-cast pick result and whether Ctrl was held. Single source of
// truth for the exact sequence in handleMouseInput() right after its
// pickObjectByRayAlg() call (STAB-0504): without Ctrl, a fresh click always
// replaces the selection (clearing it even on a miss); Ctrl held makes a
// hit additive and a miss a no-op, so Ctrl-clicking empty space never loses
// the existing selection.
inline void resolveClickSelectionAlg(
    Editor::SelectionManager& selection,
    const std::shared_ptr<Mc3::Mc3Object>& picked,
    bool ctrlHeld)
{
    if (!ctrlHeld) selection.clear();
    if (picked) selection.select(picked);
}

// ── Camera view presets (STAB-0506) ──────────────────────────────────────────
//
// Single source of truth for the view-preset button row
// (Application/UI/CameraPresetOverlay.cpp): each preset sets EditorCamera's yaw/
// pitch (or triggers a full reset for "Persp"). cameraOrbitPositionAlg()
// mirrors EditorCamera::position()'s exact spherical-to-Cartesian formula
// (include/MeshCraft/Editor/EditorCamera.hpp — that method just converts
// this function's std::array<float,3> to/from its CNA Vector3 type), so the
// presets' resulting camera position can be verified headlessly: Front
// (yaw=0, pitch=0) puts the camera on the target's +Z axis looking toward
// -Z; Right (yaw=90°) puts it on +X; Top uses a near-90° pitch (1.47 rad,
// not exactly 90°) to dodge the gimbal singularity viewMatrix() already
// guards against for |pitch| > 1.47.

struct CameraPresetAlg { const char* label; float yaw; float pitch; bool reset; };

inline const std::array<CameraPresetAlg, 4>& cameraPresetsAlg()
{
    static const std::array<CameraPresetAlg, 4> kPresets = {{
        { "Front", 0.0f,     0.0f,  false },
        { "Top",   0.0f,     1.47f, false },
        { "Right", 1.5708f,  0.0f,  false },
        { "Persp", 0.0f,     0.0f,  true  },
    }};
    return kPresets;
}

inline std::array<float,3> cameraOrbitPositionAlg(
    float yaw, float pitch, float distance, const std::array<float,3>& target)
{
    float cosP = std::cos(pitch);
    return {
        target[0] + distance * cosP * std::sin(yaw),
        target[1] + distance * std::sin(pitch),
        target[2] + distance * cosP * std::cos(yaw)
    };
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

// ── Live object-property resolution (STAB-0715) ───────────────────────────────
//
// Reads an object's REAL current value for a given AnimatedProperty — used to
// seed a newly-created channel's first keyframe. This is the single source of
// truth shared by production (MeshCraftApplication_Anim.cpp) and the headless
// insertAnimKeyframesAlg mirror below, so the two can no longer diverge (they
// previously did: the mirror returned 0.0 for every Material/Deform property
// while production returned the object's live value — DeformX/Y/Z 1.0,
// MaterialBaseColor 0.8, alpha 1.0, roughness 0.5, etc.).
inline float resolveObjectPropertyValueAlg(const Mc3::Mc3Document& doc,
                                           const Mc3::Mc3Object& obj,
                                           Mc3::AnimatedProperty prop)
{
    using AP = Mc3::AnimatedProperty;
    switch (prop) {
    case AP::PositionX: return obj.transform.position[0];
    case AP::PositionY: return obj.transform.position[1];
    case AP::PositionZ: return obj.transform.position[2];
    case AP::RotationX: return obj.transform.rotation[0];
    case AP::RotationY: return obj.transform.rotation[1];
    case AP::RotationZ: return obj.transform.rotation[2];
    case AP::ScaleX:    return obj.transform.scale[0];
    case AP::ScaleY:    return obj.transform.scale[1];
    case AP::ScaleZ:    return obj.transform.scale[2];
    case AP::Visible:   return obj.visible ? 1.0f : 0.0f;
    case AP::DeformX:   return obj.deform.has_value() ? obj.deform->scale[0] : 1.0f;
    case AP::DeformY:   return obj.deform.has_value() ? obj.deform->scale[1] : 1.0f;
    case AP::DeformZ:   return obj.deform.has_value() ? obj.deform->scale[2] : 1.0f;
    case AP::MaterialBaseColorR:
    case AP::MaterialBaseColorG:
    case AP::MaterialBaseColorB:
    case AP::MaterialBaseColorA:
    case AP::MaterialRoughness:
    case AP::MaterialMetallic:
    case AP::MaterialEmissiveR:
    case AP::MaterialEmissiveG:
    case AP::MaterialEmissiveB: {
        const std::string& matName = !obj.materialOverride.empty() ? obj.materialOverride : obj.material;
        auto it = matName.empty() ? doc.materials.end() : doc.materials.find(matName);
        if (it == doc.materials.end()) {
            switch (prop) {
            case AP::MaterialBaseColorR: case AP::MaterialBaseColorG: case AP::MaterialBaseColorB: return 0.8f;
            case AP::MaterialBaseColorA: return 1.0f;
            case AP::MaterialRoughness:  return 0.5f;
            default: return 0.0f; // metallic, emissive r/g/b
            }
        }
        const auto& mat = it->second;
        switch (prop) {
        case AP::MaterialBaseColorR: return mat.baseColor[0];
        case AP::MaterialBaseColorG: return mat.baseColor[1];
        case AP::MaterialBaseColorB: return mat.baseColor[2];
        case AP::MaterialBaseColorA: return mat.baseColor[3];
        case AP::MaterialRoughness:  return mat.roughness;
        case AP::MaterialMetallic:   return mat.metallic;
        case AP::MaterialEmissiveR:  return mat.emissiveColor[0];
        case AP::MaterialEmissiveG:  return mat.emissiveColor[1];
        case AP::MaterialEmissiveB:  return mat.emissiveColor[2];
        default: return 0.0f; // unreachable, silences -Wswitch
        }
    }
    }
    return 0.0f; // unreachable, silences -Wswitch
}

// ── Animation keyframe insertion (STAB-0284) ──────────────────────────────────
//
// Mirrors MeshCraftApplication::insertAnimKeyframes(), minus pushUndo()/
// modified_/evaluateAndPushAnimOverrides(). For each requested property: finds
// or creates the channel for (obj.name, prop); seeds the keyframe with the
// object's live value via the shared resolveObjectPropertyValueAlg() (same
// resolution production uses); then overwrites an existing keyframe within
// 0.001 of animTime or inserts a new one and re-sorts the channel by time.

inline void insertAnimKeyframesAlg(
    const Mc3::Mc3Document&                    doc,
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

        float value = resolveObjectPropertyValueAlg(doc, obj, prop);

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

// ── Animation override evaluation (SYS-W3-01 Phase 5) ─────────────────────────
//
// Mirrors MeshCraftApplication::evaluateAndPushAnimOverrides()'s core
// computation, minus the stale-currentActionName_/animPlaying_ clearing
// (a stateful side effect on MeshCraftApplication itself, not part of this
// pure computation) and the final SceneRenderer::setAnimOverrides() push.
//
// AnimOverrideAlg is a field-for-field CNA-free mirror of
// Renderer::AnimOverride (MeshCraft/Renderer/SceneRenderer.hpp) -- that
// header cannot be included from here without pulling in CNA-coupled
// dependencies (BasicEffect/GraphicsDevice/VertexBuffer/etc.) needed by
// its OTHER, unrelated structs. The caller converts one to the other,
// matching MacroStep/MacroStepAlg's established duplication-for-
// testability idiom.
struct AnimOverrideAlg {
    std::optional<std::array<float,3>> position;
    std::optional<std::array<float,3>> rotation;
    std::optional<std::array<float,3>> scale;
    std::optional<bool>                visible;

    std::optional<std::array<float,4>> baseColor;
    std::optional<float>               roughness;
    std::optional<float>               metallic;
    std::optional<std::array<float,3>> emissive;

    std::optional<std::array<float,3>> deformScale;
};

// Computes the per-object override map for `action` at `animTime`: a first
// pass seeds each target object's override from its current document state
// (transform + material + deform), then a second pass overwrites whichever
// specific property each channel animates with Mc3::evaluateChannel()'s
// result at animTime. findObjectByName resolves a channel's targetObject
// (mirrors MeshCraftApplication::flatFindByName()'s exact semantics) --
// passed in rather than baked in, since this function has no document
// access of its own beyond what's given per call. An object that can't be
// resolved (stale/renamed target) is silently skipped, matching the
// pre-extraction production behavior exactly.
inline std::unordered_map<std::string, AnimOverrideAlg> computeAnimOverridesAlg(
    const Mc3::Mc3Action&                                    action,
    float                                                     animTime,
    const std::map<std::string, Mc3::Mc3Material>&           materials,
    const std::function<Mc3::Mc3Object*(const std::string&)>& findObjectByName)
{
    std::unordered_map<std::string, AnimOverrideAlg> overrides;

    // First pass: seed each target object's override from its current
    // document-state (transform + material).
    for (const auto& ch : action.channels) {
        if (ch.targetObject.empty() || overrides.count(ch.targetObject)) continue;
        Mc3::Mc3Object* obj = findObjectByName(ch.targetObject);
        if (!obj) continue;
        auto& ov = overrides[ch.targetObject];
        ov.position = obj->transform.position;
        ov.rotation = obj->transform.rotation;
        ov.scale    = obj->transform.scale;
        ov.visible  = obj->visible;
        if (!obj->material.empty()) {
            auto matIt = materials.find(obj->material);
            if (matIt != materials.end()) {
                const auto& m = matIt->second;
                ov.baseColor = m.baseColor;
                ov.roughness = m.roughness;
                ov.metallic  = m.metallic;
                ov.emissive  = std::array<float,3>{m.emissiveColor[0], m.emissiveColor[1], m.emissiveColor[2]};
            }
        }
        ov.deformScale = obj->deform
            ? obj->deform->scale
            : std::array<float,3>{1.0f, 1.0f, 1.0f};
    }

    // Second pass: apply each channel's evaluated value at animTime.
    using AP = Mc3::AnimatedProperty;
    for (const auto& ch : action.channels) {
        auto it = overrides.find(ch.targetObject);
        if (it == overrides.end()) continue;
        float v = Mc3::evaluateChannel(ch, animTime);
        auto& ov = it->second;
        switch (ch.property) {
        case AP::PositionX: (*ov.position)[0] = v; break;
        case AP::PositionY: (*ov.position)[1] = v; break;
        case AP::PositionZ: (*ov.position)[2] = v; break;
        case AP::RotationX: (*ov.rotation)[0] = v; break;
        case AP::RotationY: (*ov.rotation)[1] = v; break;
        case AP::RotationZ: (*ov.rotation)[2] = v; break;
        case AP::ScaleX:    (*ov.scale)[0]    = v; break;
        case AP::ScaleY:    (*ov.scale)[1]    = v; break;
        case AP::ScaleZ:    (*ov.scale)[2]    = v; break;
        case AP::Visible:   ov.visible        = (v >= 0.5f); break;
        case AP::MaterialBaseColorR: if (ov.baseColor) (*ov.baseColor)[0] = v; break;
        case AP::MaterialBaseColorG: if (ov.baseColor) (*ov.baseColor)[1] = v; break;
        case AP::MaterialBaseColorB: if (ov.baseColor) (*ov.baseColor)[2] = v; break;
        case AP::MaterialBaseColorA: if (ov.baseColor) (*ov.baseColor)[3] = v; break;
        case AP::MaterialRoughness:  ov.roughness = v; break;
        case AP::MaterialMetallic:   ov.metallic  = v; break;
        case AP::MaterialEmissiveR:  if (ov.emissive) (*ov.emissive)[0] = v; break;
        case AP::MaterialEmissiveG:  if (ov.emissive) (*ov.emissive)[1] = v; break;
        case AP::MaterialEmissiveB:  if (ov.emissive) (*ov.emissive)[2] = v; break;
        case AP::DeformX: if (ov.deformScale) (*ov.deformScale)[0] = v; break;
        case AP::DeformY: if (ov.deformScale) (*ov.deformScale)[1] = v; break;
        case AP::DeformZ: if (ov.deformScale) (*ov.deformScale)[2] = v; break;
        default: break;
        }
    }

    return overrides;
}

// Cross-fades two evaluations of the same action (normally two named clip
// ranges). Numeric transforms/material values interpolate; visibility changes
// at the midpoint so it never becomes an invented fractional state. A target
// present on only one side is retained until/after the midpoint respectively.
inline std::unordered_map<std::string, AnimOverrideAlg> blendAnimOverridesAlg(
    const std::unordered_map<std::string, AnimOverrideAlg>& from,
    const std::unordered_map<std::string, AnimOverrideAlg>& to,
    float amount)
{
    const float t = std::clamp(amount, 0.0f, 1.0f);
    auto blendFloat = [t](const std::optional<float>& a, const std::optional<float>& b) {
        if (a && b) return std::optional<float>{std::lerp(*a, *b, t)};
        return t < 0.5f ? a : b;
    };
    auto blendBool = [t](const std::optional<bool>& a, const std::optional<bool>& b) {
        if (a && b) return std::optional<bool>{t < 0.5f ? *a : *b};
        return t < 0.5f ? a : b;
    };
    auto blendVec3 = [t](const std::optional<std::array<float, 3>>& a,
                         const std::optional<std::array<float, 3>>& b) {
        if (!a || !b) return t < 0.5f ? a : b;
        std::array<float, 3> result{};
        for (int i = 0; i < 3; ++i) result[i] = std::lerp((*a)[i], (*b)[i], t);
        return std::optional<std::array<float, 3>>{result};
    };
    auto blendVec4 = [t](const std::optional<std::array<float, 4>>& a,
                         const std::optional<std::array<float, 4>>& b) {
        if (!a || !b) return t < 0.5f ? a : b;
        std::array<float, 4> result{};
        for (int i = 0; i < 4; ++i) result[i] = std::lerp((*a)[i], (*b)[i], t);
        return std::optional<std::array<float, 4>>{result};
    };

    std::unordered_map<std::string, AnimOverrideAlg> result;
    for (const auto& [target, before] : from) {
        auto afterIt = to.find(target);
        if (afterIt == to.end()) {
            if (t < 0.5f) result.emplace(target, before);
            continue;
        }
        const auto& after = afterIt->second;
        AnimOverrideAlg blended;
        blended.position = blendVec3(before.position, after.position);
        blended.rotation = blendVec3(before.rotation, after.rotation);
        blended.scale = blendVec3(before.scale, after.scale);
        blended.visible = blendBool(before.visible, after.visible);
        blended.baseColor = blendVec4(before.baseColor, after.baseColor);
        blended.roughness = blendFloat(before.roughness, after.roughness);
        blended.metallic = blendFloat(before.metallic, after.metallic);
        blended.emissive = blendVec3(before.emissive, after.emissive);
        blended.deformScale = blendVec3(before.deformScale, after.deformScale);
        result.emplace(target, std::move(blended));
    }
    if (t >= 0.5f) {
        for (const auto& [target, after] : to)
            if (!from.count(target)) result.emplace(target, after);
    }
    return result;
}

// ── Keybinding persistence format (STAB-0286) ─────────────────────────────────
//
// Mirrors the persistence format used by Editor::KeyBind::toString()/
// fromString() and Editor::KeybindingManager::load()/save() (SYS-W3-01:
// relocated from MeshCraftApplication_Keybindings.cpp into
// MeshCraft/Editor/KeybindingManager.{hpp,cpp}): each binding serializes as
// "id=[ctrl+][shift+][alt+]KEYNAME" (or an empty value when unbound), one
// "id=value" line per binding; loading re-parses the same tokens
// case-insensitively and only overwrites ids present in the file (an id
// absent from the file keeps its pre-load — i.e. default — value, the same
// merge behavior `KeybindingManager::initDefaults()` + `load()` produce
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

// AUD-031: snapTranslate/snapScale defaults here had drifted from
// MeshCraftApplication's real member-initializer defaults (1.0f/0.1f here
// vs the real 0.5f/0.25f) -- exactly the kind of silent divergence this
// finding warns two hand-synced copies are prone to. Corrected to match.
struct PrefsAlg {
    float autoSaveInterval{60.0f};
    float snapTranslate{0.5f};
    float snapRotate{15.0f};
    float snapScale{0.25f};
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
            // AUD-032: clamped to the same bounds production loadPrefs()
            // (MeshCraftApplication_FileOps.cpp) uses -- the widest range
            // any slider UI for this value allows -- so a hand-edited
            // prefs.ini can't set a value neither slider could ever reach,
            // e.g. 0 or negative snapScale. Values here must be kept in
            // sync with loadPrefs() by hand; there is no shared constant.
            if      (key == "autoSaveInterval") p.autoSaveInterval = std::clamp(std::stof(val), 0.0f, 300.0f);
            else if (key == "snapTranslate")    p.snapTranslate    = std::clamp(std::stof(val), 0.01f, 100.0f);
            else if (key == "snapRotate")       p.snapRotate       = std::clamp(std::stof(val), 1.0f, 180.0f);
            else if (key == "snapScale")        p.snapScale        = std::clamp(std::stof(val), 0.01f, 10.0f);
            else if (key == "gridSpacing")      p.gridSpacing      = std::clamp(std::stof(val), 0.1f, 10.0f);
            else if (key == "theme")            p.theme            = std::clamp(std::stoi(val), 0, 2);
        } catch (...) {}
    }
}

// ── Macro save/load (STAB-0292) ───────────────────────────────────────────────
//
// Mirrors Editor::MacroRecorder::save()/load() (SYS-W3-01 Phase 3,
// src/MeshCraft/Editor/MacroRecorder.cpp): each step is one tab-separated
// line ("verb\targ1\targ2..."); loading skips blank lines. MacroStepAlg
// mirrors Editor::MacroRecorder::Step's fields exactly -- kept as a
// separate type since these Alg functions are unit-tested standalone
// without linking MacroRecorder itself.

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

// ── Texture registration from a raw file path (F9, 2026-07-20 audit) ─────────
//
// Mirrors MeshCraftApplication::registerTextureFromPath(). A material's
// texture-slot fields (baseColorTexture etc.) are doc.textures KEYS,
// resolved by both the live renderer (SceneRenderer.cpp) and the glTF
// exporter (GltfExporter.cpp) via doc.textures.find(...) -- never a raw
// file path. Browse ("..." button) and drag-drop both used to write the
// OS-provided path directly into the material field, which that lookup
// then silently fails to resolve (texture treated as absent). This
// registers (or reuses, if `path` already matches an existing entry's uri)
// a doc.textures entry and returns its id, so the slot field always holds
// something the renderer/exporter can actually resolve.

inline std::string registerTextureFromPathAlg(const std::string& path, Mc3::Mc3Document& doc)
{
    for (auto& [id, tex] : doc.textures)
        if (tex.uri == path) return id;

    std::string base = std::filesystem::path(path).stem().string();
    if (base.empty()) base = "tex";
    std::string key = base;
    int n = 1;
    while (doc.textures.count(key))
        key = base + "_" + std::to_string(n++);

    Mc3::Mc3Texture tex;
    tex.name = key;
    tex.uri  = path;
    doc.textures[key] = tex;
    return key;
}

// ── Delete reference-integrity cleanup (F10, 2026-07-20 audit) ───────────────
//
// Rename already rewrites every live reference to a resource's OLD id over
// to its NEW one (the Mat/Defs tabs' own fixRefs lambdas in
// MeshCraftApplication_UiLeftPanel.cpp, ModelRegistry.cpp's
// remapMaterialRefs()). Delete had no equivalent: erasing a shared
// resource left every object/material/trigger that referenced it pointing
// at a now-nonexistent id, with no warning and no cleanup -- silently
// broken material/texture/mesh assignments and dead trigger steps that
// would only surface later (a validation warning, a blank material in the
// renderer, glTF export dropping the reference).
//
// Each function below returns how many references it cleared, so the
// caller can report it in the status message. Trigger steps are REMOVED
// entirely rather than left with a cleared ref -- a PlaySound/PlayMusic/
// RunScript step with an empty ref does nothing useful, unlike a cleared
// Mc3Object::material (an object with no material is a normal, valid
// state -- it just renders with the default fallback color, same as
// materialColorAlg above already handles).

inline int clearMaterialRefsInObjectAlg(Mc3::Mc3Object& obj, const std::string& deletedId)
{
    int n = 0;
    if (obj.material == deletedId)         { obj.material.clear();         ++n; }
    if (obj.materialOverride == deletedId) { obj.materialOverride.clear(); ++n; }
    for (auto& [stateName, state] : obj.states)
        if (state.material && *state.material == deletedId) { state.material.reset(); ++n; }
    for (auto& child : obj.children)
        if (child) n += clearMaterialRefsInObjectAlg(*child, deletedId);
    return n;
}

inline int clearMaterialReferencesAlg(Mc3::Mc3Document& doc, const std::string& deletedId)
{
    int n = 0;
    for (auto& obj : doc.objects)
        if (obj) n += clearMaterialRefsInObjectAlg(*obj, deletedId);
    for (auto& [defId, defObj] : doc.definitions)
        if (defObj) n += clearMaterialRefsInObjectAlg(*defObj, deletedId);
    for (auto& [stateName, state] : doc.sceneStates)
        for (auto& ovr : state.overrides)
            if (ovr.material && *ovr.material == deletedId) { ovr.material.reset(); ++n; }
    return n;
}

inline int clearDefinitionRefsInObjectAlg(Mc3::Mc3Object& obj, const std::string& deletedId)
{
    int n = 0;
    if (obj.type == Mc3::ObjectType::Instance && obj.definition == deletedId) {
        obj.definition.clear();
        ++n;
    }
    auto& variants = obj.variantDefinitions;
    auto vEnd = std::remove(variants.begin(), variants.end(), deletedId);
    n += static_cast<int>(std::distance(vEnd, variants.end()));
    variants.erase(vEnd, variants.end());
    if (obj.assetMetadata) {
        auto& lods = obj.assetMetadata->lods;
        for (auto it = lods.begin(); it != lods.end(); ) {
            if (it->second == deletedId) { it = lods.erase(it); ++n; }
            else ++it;
        }
    }
    for (auto& child : obj.children)
        if (child) n += clearDefinitionRefsInObjectAlg(*child, deletedId);
    return n;
}

inline int clearDefinitionReferencesAlg(Mc3::Mc3Document& doc, const std::string& deletedId)
{
    int n = 0;
    for (auto& obj : doc.objects)
        if (obj) n += clearDefinitionRefsInObjectAlg(*obj, deletedId);
    for (auto& [defId, defObj] : doc.definitions)
        if (defObj) n += clearDefinitionRefsInObjectAlg(*defObj, deletedId);
    return n;
}

inline int clearTextureReferencesAlg(Mc3::Mc3Document& doc, const std::string& deletedId)
{
    int n = 0;
    for (auto& [matId, mat] : doc.materials) {
        if (mat.baseColorTexture == deletedId)         { mat.baseColorTexture.clear();         ++n; }
        if (mat.normalTexture == deletedId)            { mat.normalTexture.clear();            ++n; }
        if (mat.emissiveTexture == deletedId)          { mat.emissiveTexture.clear();          ++n; }
        if (mat.metallicRoughnessTexture == deletedId) { mat.metallicRoughnessTexture.clear(); ++n; }
        if (mat.occlusionTexture == deletedId)         { mat.occlusionTexture.clear();         ++n; }
    }
    return n;
}

inline int clearMeshSourceRefsInObjectAlg(Mc3::Mc3Object& obj, const std::string& meshSourceValue)
{
    int n = 0;
    if (obj.meshSource == meshSourceValue) { obj.meshSource.clear(); ++n; }
    for (auto& child : obj.children)
        if (child) n += clearMeshSourceRefsInObjectAlg(*child, meshSourceValue);
    return n;
}

// A Mesh object's src references an embed via the "embed:<id>" scheme
// (see Mc3XmlParser.cpp's mesh tag handling) -- not the bare id.
inline int clearEmbedReferencesAlg(Mc3::Mc3Document& doc, const std::string& deletedId)
{
    std::string embedRef = "embed:" + deletedId;
    int n = 0;
    for (auto& obj : doc.objects)
        if (obj) n += clearMeshSourceRefsInObjectAlg(*obj, embedRef);
    for (auto& [defId, defObj] : doc.definitions)
        if (defObj) n += clearMeshSourceRefsInObjectAlg(*defObj, embedRef);
    return n;
}

inline int clearScriptRefsInObjectAlg(Mc3::Mc3Object& obj, const std::string& deletedId)
{
    int n = 0;
    if (obj.scriptId == deletedId) { obj.scriptId.clear(); ++n; }
    for (auto& child : obj.children)
        if (child) n += clearScriptRefsInObjectAlg(*child, deletedId);
    return n;
}

inline int removeTriggerStepsReferencingAlg(Mc3::Mc3Document& doc, Mc3::TriggerStepType type,
                                             const std::string& deletedId)
{
    int n = 0;
    for (auto& [trigId, trig] : doc.triggers) {
        auto stepsEnd = std::remove_if(trig.steps.begin(), trig.steps.end(),
            [&](const Mc3::Mc3TriggerStep& s) { return s.type == type && s.ref == deletedId; });
        n += static_cast<int>(std::distance(stepsEnd, trig.steps.end()));
        trig.steps.erase(stepsEnd, trig.steps.end());
    }
    return n;
}

inline int clearScriptReferencesAlg(Mc3::Mc3Document& doc, const std::string& deletedId)
{
    int n = 0;
    for (auto& obj : doc.objects)
        if (obj) n += clearScriptRefsInObjectAlg(*obj, deletedId);
    for (auto& [defId, defObj] : doc.definitions)
        if (defObj) n += clearScriptRefsInObjectAlg(*defObj, deletedId);
    n += removeTriggerStepsReferencingAlg(doc, Mc3::TriggerStepType::RunScript, deletedId);
    return n;
}

inline int clearSoundReferencesAlg(Mc3::Mc3Document& doc, const std::string& deletedId)
{
    return removeTriggerStepsReferencingAlg(doc, Mc3::TriggerStepType::PlaySound, deletedId);
}

inline int clearMusicReferencesAlg(Mc3::Mc3Document& doc, const std::string& deletedId)
{
    return removeTriggerStepsReferencingAlg(doc, Mc3::TriggerStepType::PlayMusic, deletedId);
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
