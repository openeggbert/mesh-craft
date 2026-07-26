#pragma once

// Small CNA-free scene-semantic helpers shared by parity-sensitive consumers.
// Keep graphics representation (XNA matrices, glTF nodes, Manifold matrices)
// at their respective boundaries; this header owns only the MC3 decisions that
// must mean the same thing in all of them.

#include <MeshCraft/AssetLodAlgorithms.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace MeshCraft {

// The local transform's pivot convention is shared by the editor's SRT
// matrix and glTF's outer-node/inner-origin-node representation.
struct ObjectTransformSemantics {
    std::array<float, 3> outerTranslation{}; // position + pivot
    std::array<float, 3> childOriginOffset{}; // -pivot
    bool hasPivot{false};
};

[[nodiscard]] inline ObjectTransformSemantics objectTransformSemanticsAlg(
    const Mc3::Mc3Transform& transform)
{
    const auto& pivot = transform.pivot;
    return {{transform.position[0] + pivot[0],
             transform.position[1] + pivot[1],
             transform.position[2] + pivot[2]},
            {-pivot[0], -pivot[1], -pivot[2]},
            pivot[0] != 0.0f || pivot[1] != 0.0f || pivot[2] != 0.0f};
}

// `materialOverride` always has precedence over the ordinary material field.
// An Instance's own effective material, when present, takes precedence over
// the effective material of the selected definition.
[[nodiscard]] inline std::string_view effectiveObjectMaterialIdAlg(
    const Mc3::Mc3Object& object)
{
    return !object.materialOverride.empty() ? std::string_view(object.materialOverride)
                                            : std::string_view(object.material);
}

[[nodiscard]] inline std::string_view effectiveInstanceMaterialIdAlg(
    const Mc3::Mc3Object& instance, const Mc3::Mc3Object& definition)
{
    const std::string_view instanceMaterial = effectiveObjectMaterialIdAlg(instance);
    return !instanceMaterial.empty() ? instanceMaterial : effectiveObjectMaterialIdAlg(definition);
}

[[nodiscard]] inline bool effectiveObjectVisibilityAlg(
    bool authoredVisible, std::optional<bool> visibilityOverride = std::nullopt)
{
    return visibilityOverride.value_or(authoredVisible);
}

// A useful stable key for renderer state maps and diagnostics.  MC3 IDs are
// canonical; names are only the safe fallback for legacy programmatic scenes
// that omitted IDs. Empty remains empty rather than inventing a pointer-based
// identity that would diverge across editor/exporter processes.
[[nodiscard]] inline std::string stableObjectIdentityKeyAlg(const Mc3::Mc3Object& object)
{
    if (!object.id.empty()) return object.id;
    return object.name;
}

struct ResolvedInstanceSemantics {
    AssetLodSelection lod;
    const Mc3::Mc3Object* definition{nullptr};

    [[nodiscard]] bool resolved() const { return !lod.culled && definition != nullptr; }
};

[[nodiscard]] inline const Mc3::Mc3Object* resolvedDefinitionForAssetLodAlg(
    const AssetLodSelection& selection,
    const std::map<std::string, std::shared_ptr<Mc3::Mc3Object>>& definitions)
{
    if (selection.culled) return nullptr;
    const auto found = definitions.find(selection.definitionId);
    return found != definitions.end() && found->second ? found->second.get() : nullptr;
}

// Resolves the stable variant and selected asset LOD exactly once, then binds
// that selected definition pointer. A caller can choose distance-aware viewport
// policy or call the default wrapper used by export.
[[nodiscard]] inline ResolvedInstanceSemantics resolveInstanceSemanticsAlg(
    const Mc3::Mc3Object& instance,
    const std::map<std::string, std::shared_ptr<Mc3::Mc3Object>>& definitions,
    float distanceM, AssetLodConfig config,
    std::optional<AssetLodTier> previousTier = std::nullopt)
{
    ResolvedInstanceSemantics result;
    result.lod = resolveAssetLodForInstanceAlg(instance, definitions, distanceM,
                                                config, previousTier);
    result.definition = resolvedDefinitionForAssetLodAlg(result.lod, definitions);
    return result;
}

[[nodiscard]] inline ResolvedInstanceSemantics resolveDefaultInstanceSemanticsAlg(
    const Mc3::Mc3Object& instance,
    const std::map<std::string, std::shared_ptr<Mc3::Mc3Object>>& definitions)
{
    return resolveInstanceSemanticsAlg(instance, definitions, 0.0f, AssetLodConfig{});
}

} // namespace MeshCraft
