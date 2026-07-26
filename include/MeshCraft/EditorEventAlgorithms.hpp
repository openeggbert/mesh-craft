#pragma once
// Pure editor EVENT algorithms (AI panel dialog lifecycle, animation
// keyframe insertion, animation override evaluation/blending) — no CNA /
// ImGui / SDL / OpenGL dependencies.
//
// SYS-W3-05: split out of the former monolithic EditorAlgorithms.hpp so
// consumers that only need event/animation helpers don't have to pull in
// selection, command-mutation, transform-drag, persistence, and preferences
// algorithms they never call. insertAnimKeyframesAlg() seeds a new
// keyframe from an object's live value via the shared
// resolveObjectPropertyValueAlg(), so this depends on
// EditorUtilityAlgorithms.hpp.

#include <MeshCraft/EditorUtilityAlgorithms.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MeshCraft {

// ── AI panel dialog lifecycle (STAB-0299/0300/0301) ───────────────────────────
//
// Mirrors the Reset / Apply to Scene / Save-to-Registry state transitions in
// drawAiPanel() and RegistryWorkspace. The ImGui code remains separate, so
// this small model tracks only the presence/absence of the actual state
// (aiPendingDoc_, aiValidationError_, RegistryWorkspace's AI save source and
// save-dialog flag), not document or widget contents.

struct AiPanelStateAlg {
    bool        aiPendingDocSet    = false;  // aiPendingDoc_.has_value()
    bool        validationErrorSet = false;  // !aiValidationError_.empty()
    bool        registrySaveFromAi  = false;
    bool        registrySaveDlgOpen = false;
    std::string registrySaveDefId;
};

// Mirrors the "Reset" button body (MeshCraftApplication_UiAi.cpp:369-379):
// always clears the pending result and any validation error; additionally
// closes the registry save dialog, but ONLY if it was opened from this AI
// result — a dialog opened independently is left alone.
inline void aiResetAlg(AiPanelStateAlg& st)
{
    st.aiPendingDocSet    = false;
    st.validationErrorSet = false;
    if (st.registrySaveFromAi) {
        st.registrySaveDlgOpen = false;
        st.registrySaveFromAi  = false;
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
// (RegistryWorkspace::prepareAiSave()): records which definition id to
// pre-fill and marks the save as AI-sourced. Note
// what this does NOT depend on: it doesn't check or set any "applied to
// scene" flag — the button's visibility gate (line 329) and this handler
// both reference `aiPendingDoc_` alone (STAB-0348: works without a prior
// Apply). `defId` mirrors `aiPendingDoc_->definitions.begin()->first`.
inline void aiSaveToRegistryClickAlg(AiPanelStateAlg& st, const std::string& defId)
{
    st.registrySaveDefId  = defId;
    st.registrySaveFromAi = true;
}

// Mirrors the definition-source ternary in the "Save Definition to
// Registry" dialog (MeshCraftApplication_UiRegistry.cpp:137-139): when the
// save was initiated from an AI result AND that result is still present, list
// aiPendingDoc_'s definitions; otherwise list the scene document's
// definitions — STAB-0347.
inline bool registrySaveUsesAiDefinitionsAlg(const AiPanelStateAlg& st)
{
    return st.registrySaveFromAi && st.aiPendingDocSet;
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

} // namespace MeshCraft
