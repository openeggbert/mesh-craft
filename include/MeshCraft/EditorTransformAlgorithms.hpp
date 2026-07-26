#pragma once
// Pure editor TRANSFORM-drag algorithms (align, scatter-along-curve,
// proportional-editing falloff, vertex snap, rotation-drag snap, group
// scale, camera view presets) — no CNA / ImGui / SDL / OpenGL dependencies.
//
// SYS-W3-05: split out of the former monolithic EditorAlgorithms.hpp so
// consumers that only need transform-drag helpers don't have to pull in
// selection, persistence, event, preferences, and utility algorithms they
// never call. scatterAlongCurveAlg() is the one function here that mutates
// the object tree (inserting new copies), so it depends on
// EditorCommandAlgorithms.hpp for findParentListAlg()/deepCopyObjectAlg().

#include <MeshCraft/EditorCommandAlgorithms.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <numbers>
#include <set>
#include <vector>

namespace MeshCraft {

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

} // namespace MeshCraft
