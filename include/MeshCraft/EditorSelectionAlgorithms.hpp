#pragma once
// Pure editor SELECTION algorithms (select-children, lock-aware selection
// dry-run, viewport ray-cast picking, hierarchy panel filtering) — no CNA /
// ImGui / SDL / OpenGL dependencies.
//
// SYS-W3-05: split out of the former monolithic EditorAlgorithms.hpp so
// consumers that only need selection helpers don't have to pull in
// command-mutation, transform-drag, persistence, event, preferences, and
// utility algorithms they never call.

#include <MeshCraft/RotationConventionAlgorithms.hpp>
#include <MeshCraft/Editor/SelectionManager.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MeshCraft {

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

// ── Viewport ray-cast picking (STAB-0503) ────────────────────────────────────
//
// Mirrors the ray-cast object picking in handleMouseInput()
// (MeshCraftApplication_Mouse.cpp). Any visible object gets a default
// 0.5-unit half-extent AABB (scaled by its transform scale); primitive
// shapes (Box/Sphere/Cylinder/Cone/Plane) use their actual dimensions
// instead. Bounds are transformed through the complete parent chain using the
// document's rotation convention and pivot semantics. This must NOT be gated
// on `obj.primitive` being set — an
// earlier version of the real code only tested primitive-typed objects,
// silently making every Instance/Mesh/Group/Extrude/CSG object unclickable
// in the viewport (found and fixed by STAB-0503), inconsistent with
// box-select, which already selects by screen position regardless of type.

struct ObjectAffineTransformAlg {
    RotationMatrix3Alg linear{{{1.0f, 0.0f, 0.0f},
                                {0.0f, 1.0f, 0.0f},
                                {0.0f, 0.0f, 1.0f}}};
    std::array<float, 3> translation{0.0f, 0.0f, 0.0f};
};

inline std::array<float, 3> transformPointAlg(
    const std::array<float, 3>& point, const ObjectAffineTransformAlg& transform)
{
    const auto transformed = rotateDirectionAlg(point, transform.linear);
    return {transformed[0] + transform.translation[0],
            transformed[1] + transform.translation[1],
            transformed[2] + transform.translation[2]};
}

inline ObjectAffineTransformAlg localObjectTransformAlg(
    const Mc3::Mc3Transform& transform, std::string_view rotationUnits,
    std::string_view eulerOrder)
{
    ObjectAffineTransformAlg result;
    const auto rotation = rotationMatrix3Alg(transform.rotation, rotationUnits, eulerOrder);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            result.linear[row][col] = transform.scale[row] * rotation[row][col];
    const std::array<float, 3> childOrigin{-transform.pivot[0], -transform.pivot[1],
                                           -transform.pivot[2]};
    const auto pivotOffset = rotateDirectionAlg(childOrigin, result.linear);
    result.translation = {pivotOffset[0] + transform.position[0] + transform.pivot[0],
                          pivotOffset[1] + transform.position[1] + transform.pivot[1],
                          pivotOffset[2] + transform.position[2] + transform.pivot[2]};
    return result;
}

inline ObjectAffineTransformAlg composeObjectTransformsAlg(
    const ObjectAffineTransformAlg& local, const ObjectAffineTransformAlg& parent)
{
    ObjectAffineTransformAlg result;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            result.linear[row][col] = local.linear[row][0] * parent.linear[0][col] +
                                      local.linear[row][1] * parent.linear[1][col] +
                                      local.linear[row][2] * parent.linear[2][col];
        }
    }
    const auto inheritedTranslation = rotateDirectionAlg(local.translation, parent.linear);
    result.translation = {inheritedTranslation[0] + parent.translation[0],
                          inheritedTranslation[1] + parent.translation[1],
                          inheritedTranslation[2] + parent.translation[2]};
    return result;
}

inline void objectAABBAlg(const Mc3::Mc3Object& obj,
                           const ObjectAffineTransformAlg& worldTransform,
                           std::array<float,3>& bMin, std::array<float,3>& bMax)
{
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
    bMin = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
    bMax = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()};
    for (const float x : {-hx, hx}) for (const float y : {-hy, hy}) for (const float z : {-hz, hz}) {
        const auto corner = transformPointAlg({x, y, z}, worldTransform);
        for (int axis = 0; axis < 3; ++axis) {
            bMin[axis] = std::min(bMin[axis], corner[axis]);
            bMax[axis] = std::max(bMax[axis], corner[axis]);
        }
    }
}

inline void objectAABBAlg(const Mc3::Mc3Object& obj,
                           std::array<float,3>& bMin, std::array<float,3>& bMax,
                           std::string_view rotationUnits = "degrees",
                           std::string_view eulerOrder = "XYZ")
{
    objectAABBAlg(obj, localObjectTransformAlg(obj.transform, rotationUnits, eulerOrder),
                  bMin, bMax);
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
    const std::array<float,3>& rayOrig, const std::array<float,3>& rayDir,
    std::string_view rotationUnits = "degrees", std::string_view eulerOrder = "XYZ")
{
    float bestT = 1e30f;
    std::shared_ptr<Mc3::Mc3Object> bestObj;

    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&,
                       const ObjectAffineTransformAlg&, int)> testList;
    testList = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
                   const ObjectAffineTransformAlg& parentTransform, int depth) {
        if (depth > 256) return;
        for (const auto& obj : list) {
            if (!obj || !obj->visible) continue;
            const auto worldTransform = composeObjectTransformsAlg(
                localObjectTransformAlg(obj->transform, rotationUnits, eulerOrder), parentTransform);
            std::array<float,3> bMin, bMax;
            objectAABBAlg(*obj, worldTransform, bMin, bMax);
            float tHit = 0.0f;
            if (rayAABBIntersectAlg(rayOrig, rayDir, bMin, bMax, tHit) && tHit < bestT) {
                bestT = tHit; bestObj = obj;
            }
            if (!obj->children.empty()) testList(obj->children, worldTransform, depth + 1);
        }
    };
    testList(rootObjects, {}, 0);
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

} // namespace MeshCraft
