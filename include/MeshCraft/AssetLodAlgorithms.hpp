#pragma once

// SYS-W14-29 -- CNA-free asset-definition LOD selection.  This deliberately
// lives beside, rather than inside, Renderer/PrimitiveTessellationAlg.hpp:
// metadata selects a different authored definition for an Instance, whereas
// the renderer's older LOD still changes tessellation of one primitive mesh.

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MeshCraft {

enum class AssetLodTier { Near, Mid, Far };

struct AssetLodConfig {
    // In document units (normally metres). The chosen defaults intentionally
    // do not match the procedural tessellation thresholds (10/40): these are
    // author-facing policy values for swapping entire reusable definitions.
    float midDistanceM{25.0f};
    float farDistanceM{75.0f};
    float hysteresisM{2.0f};
};

struct AssetLodSelection {
    std::string baseDefinitionId;
    std::string definitionId;
    AssetLodTier tier{AssetLodTier::Near};
    bool culled{false};
    // Intended for the selected-instance debug panel and diagnostics. It is
    // deliberately a description of the safe fallback when a target is bad.
    std::string reason;
};

inline constexpr std::string_view assetLodTierNameAlg(AssetLodTier tier) {
    switch (tier) {
    case AssetLodTier::Near: return "near";
    case AssetLodTier::Mid:  return "mid";
    case AssetLodTier::Far:  return "far";
    }
    return "near";
}

// FNV-1a is specified here instead of std::hash: the latter is allowed to
// vary between standard-library implementations, while an Instance's ID must
// choose the same variant in the viewport, exporters and another machine.
inline std::uint64_t stableAssetIdentityHashAlg(std::string_view value) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char c : value) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

inline AssetLodConfig normaliseAssetLodConfigAlg(AssetLodConfig config) {
    if (!std::isfinite(config.midDistanceM) || config.midDistanceM < 0.0f)
        config.midDistanceM = 0.0f;
    if (!std::isfinite(config.farDistanceM) || config.farDistanceM <= config.midDistanceM)
        config.farDistanceM = config.midDistanceM + 0.001f;
    if (!std::isfinite(config.hysteresisM) || config.hysteresisM < 0.0f)
        config.hysteresisM = 0.0f;
    return config;
}

inline AssetLodTier selectAssetLodTierAlg(float distanceM, AssetLodConfig config,
                                          std::optional<AssetLodTier> previous = std::nullopt) {
    config = normaliseAssetLodConfigAlg(config);
    if (!std::isfinite(distanceM) || distanceM < 0.0f) distanceM = 0.0f;

    const auto direct = [&] {
        return distanceM >= config.farDistanceM ? AssetLodTier::Far
             : distanceM >= config.midDistanceM ? AssetLodTier::Mid
                                                : AssetLodTier::Near;
    };
    if (!previous) return direct();

    const float enterMid = config.midDistanceM + config.hysteresisM;
    const float leaveMid = std::max(0.0f, config.midDistanceM - config.hysteresisM);
    const float enterFar = config.farDistanceM + config.hysteresisM;
    const float leaveFar = std::max(0.0f, config.farDistanceM - config.hysteresisM);
    switch (*previous) {
    case AssetLodTier::Near:
        return distanceM >= enterFar ? AssetLodTier::Far
             : distanceM >= enterMid ? AssetLodTier::Mid
                                     : AssetLodTier::Near;
    case AssetLodTier::Mid:
        return distanceM < leaveMid ? AssetLodTier::Near
             : distanceM >= enterFar ? AssetLodTier::Far
                                     : AssetLodTier::Mid;
    case AssetLodTier::Far:
        return distanceM < leaveMid ? AssetLodTier::Near
             : distanceM < leaveFar ? AssetLodTier::Mid
                                    : AssetLodTier::Far;
    }
    return direct();
}

inline std::string lowercaseAssetLodKeyAlg(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

inline std::string assetLodTargetForTierAlg(const Mc3::Mc3AssetMetadata& metadata,
                                            AssetLodTier requestedTier,
                                            AssetLodTier& resolvedTier) {
    // A finer tier is a safe fallback when an asset did not author every
    // level. Do not fall back from Near to a coarser mesh: an export default
    // must never silently lose authored detail.
    std::array<AssetLodTier, 3> candidates{AssetLodTier::Near,
                                            AssetLodTier::Near,
                                            AssetLodTier::Near};
    int candidateCount = 1;
    if (requestedTier == AssetLodTier::Mid) {
        candidates = {AssetLodTier::Mid, AssetLodTier::Near, AssetLodTier::Near};
        candidateCount = 2;
    } else if (requestedTier == AssetLodTier::Far) {
        candidates = {AssetLodTier::Far, AssetLodTier::Mid, AssetLodTier::Near};
        candidateCount = 3;
    }
    for (int i = 0; i < candidateCount; ++i) {
        const auto wanted = assetLodTierNameAlg(candidates[i]);
        for (const auto& [key, definitionId] : metadata.lods) {
            if (lowercaseAssetLodKeyAlg(key) == wanted && !definitionId.empty()) {
                resolvedTier = candidates[i];
                return definitionId;
            }
        }
    }
    resolvedTier = requestedTier;
    return {};
}

// Resolves an Instance's stable variant first, then applies the chosen
// definition's metadata. The returned definition always exists when culled is
// false, except for an invalid base reference where callers may show their
// existing placeholder. An invalid LOD target safely falls back to the base.
inline AssetLodSelection resolveAssetLodForInstanceAlg(
    const Mc3::Mc3Object& instance,
    const std::map<std::string, std::shared_ptr<Mc3::Mc3Object>>& definitions,
    float distanceM, AssetLodConfig config,
    std::optional<AssetLodTier> previousTier = std::nullopt)
{
    AssetLodSelection selection;
    selection.baseDefinitionId = instance.resolvedInstanceDefinitionKey();
    selection.definitionId = selection.baseDefinitionId;
    selection.tier = selectAssetLodTierAlg(distanceM, config, previousTier);

    const auto baseIt = definitions.find(selection.baseDefinitionId);
    if (baseIt == definitions.end() || !baseIt->second) {
        selection.reason = "base definition is unresolved";
        return selection;
    }
    const Mc3::Mc3Object& base = *baseIt->second;
    if (!base.assetMetadata) {
        selection.reason = "no asset metadata; using base definition";
        return selection;
    }

    const float maxVisibility = base.assetMetadata->maxVisibilityDistanceM;
    if (std::isfinite(maxVisibility) && maxVisibility > 0.0f && distanceM > maxVisibility) {
        selection.culled = true;
        selection.reason = "culled: max visibility distance exceeded";
        return selection;
    }

    AssetLodTier metadataTier = selection.tier;
    const std::string target = assetLodTargetForTierAlg(*base.assetMetadata,
                                                         selection.tier, metadataTier);
    if (target.empty()) {
        selection.reason = "no authored " + std::string(assetLodTierNameAlg(selection.tier)) +
                           " LOD; using base definition";
        return selection;
    }
    const auto targetIt = definitions.find(target);
    if (targetIt == definitions.end() || !targetIt->second) {
        selection.reason = "authored " + std::string(assetLodTierNameAlg(metadataTier)) +
                           " LOD target is unresolved; using base definition";
        return selection;
    }
    selection.definitionId = target;
    selection.reason = metadataTier == selection.tier
        ? "authored " + std::string(assetLodTierNameAlg(selection.tier)) + " LOD"
        : "fallback " + std::string(assetLodTierNameAlg(metadataTier)) + " LOD";
    return selection;
}

// glTF has no camera-distance context. Export intentionally selects the
// explicit Near/default tier; it does not bake viewport culling or its
// procedural tessellation LOD into a portable asset.
inline AssetLodSelection resolveDefaultAssetLodForInstanceAlg(
    const Mc3::Mc3Object& instance,
    const std::map<std::string, std::shared_ptr<Mc3::Mc3Object>>& definitions)
{
    return resolveAssetLodForInstanceAlg(instance, definitions, 0.0f, AssetLodConfig{});
}

} // namespace MeshCraft
