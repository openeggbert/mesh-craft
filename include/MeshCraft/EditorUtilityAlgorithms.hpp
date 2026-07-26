#pragma once
// Pure editor UTILITY algorithms (live object-property resolution for
// animation keyframing, material color resolution fallback) — no CNA /
// ImGui / SDL / OpenGL dependencies.
//
// SYS-W3-05: split out of the former monolithic EditorAlgorithms.hpp. These
// two small, self-contained lookups are shared by production code and
// EditorEventAlgorithms.hpp's insertAnimKeyframesAlg(), so they live in
// their own header rather than under Commands/Event to avoid an artificial
// direction of dependency between those two.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace MeshCraft {

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

} // namespace MeshCraft
