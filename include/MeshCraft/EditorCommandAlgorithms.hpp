#pragma once
// Pure editor COMMAND algorithms (object-tree mutation: rename, duplicate,
// group/ungroup, convert-to-definition, break-instance, delete reference
// cleanup, undo/redo cap, coordinate/rotation normalization) — no CNA /
// ImGui / SDL / OpenGL dependencies.
//
// SYS-W3-05: split out of the former monolithic EditorAlgorithms.hpp so
// consumers that only need document-mutation helpers (e.g.
// MeshCraftApplication_Commands.cpp) don't have to pull in selection,
// transform-drag, persistence, event, preferences, and utility algorithms
// they never call.

#include <MeshCraft/Editor/ObjectTypeName.hpp>
#include <MeshCraft/CoordinateSystemAlgorithms.hpp>
#include <MeshCraft/RotationConventionAlgorithms.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
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
            const auto forward = rotateDirectionAlg(
                {0.0f, 0.0f, -1.0f}, rotationMatrix3Alg(
                    *camera.rotation, document.rotationUnits, document.eulerOrder));
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
    conversionRoot->transform.rotation[0] =
        degreesInRotationUnitsAlg(-90.0f, document.rotationUnits);
    conversionRoot->children = std::move(document.objects);
    document.objects = {std::move(conversionRoot)};
    document.coordinateSystem = std::string(kRightHandedYUpCoordinateSystem);
    return true;
}

// Rotation conventions are document-wide.  A document can be made portable to
// older degrees/XYZ-only consumers by baking each *static* rotation into that
// convention.  Rotating Euler animation channels cannot be converted
// losslessly to three independently interpolated XYZ scalar channels (the
// conversion is nonlinear), so refuse that case rather than silently changing
// animation.  The live editor itself honours those animated conventions; this
// helper only powers the explicit normalization command.
inline bool hasAnimatedRotationAlg(const Mc3::Mc3Document& document)
{
    using AP = Mc3::AnimatedProperty;
    for (const auto& [_, action] : document.actions) {
        for (const auto& channel : action.channels) {
            if (channel.property == AP::RotationX || channel.property == AP::RotationY ||
                channel.property == AP::RotationZ)
                return true;
        }
    }
    return false;
}

inline bool normalizeRotationConventionToDegreesXYZAlg(Mc3::Mc3Document& document)
{
    if (document.rotationUnits == "degrees" &&
        normalisedEulerOrderAlg(document.eulerOrder) == "XYZ")
        return false;
    if (hasAnimatedRotationAlg(document)) return false;

    const auto normalize = [&](std::array<float, 3>& rotation) {
        rotation = rotationAsDegreesXYZAlg(
            rotation, document.rotationUnits, document.eulerOrder);
    };
    std::unordered_set<const Mc3::Mc3Object*> visited;
    std::function<void(const std::shared_ptr<Mc3::Mc3Object>&, int)> visit;
    visit = [&](const std::shared_ptr<Mc3::Mc3Object>& object, int depth) {
        if (!object || depth > 256 || !visited.insert(object.get()).second) return;
        normalize(object->transform.rotation);
        for (auto& [_, state] : object->states)
            if (state.rotation) normalize(*state.rotation);
        for (const auto& child : object->children) visit(child, depth + 1);
    };
    for (const auto& object : document.objects) visit(object, 0);
    for (const auto& [_, definition] : document.definitions) visit(definition, 0);
    for (auto& camera : document.cameras)
        if (camera.rotation) normalize(*camera.rotation);

    document.rotationUnits = "degrees";
    document.eulerOrder = "XYZ";
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

} // namespace MeshCraft
