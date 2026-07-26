// SYS-W14-29 -- CNA-free contract tests for authored Instance definition LOD.

#include <MeshCraft/AssetLodAlgorithms.hpp>

#include <iostream>
#include <memory>
#include <string>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

namespace {

int failures = 0;
void check(bool condition, const std::string& message) {
    if (condition) std::cout << "PASS: " << message << "\n";
    else { std::cerr << "FAIL: " << message << "\n"; ++failures; }
}

std::shared_ptr<Mc3Object> definition(const std::string& id) {
    auto object = std::make_shared<Mc3Object>();
    object->id = id;
    object->name = id;
    object->type = ObjectType::Box;
    object->primitive = Mc3Primitive::box({1.0f, 1.0f, 1.0f});
    return object;
}

} // namespace

int main() {
    std::map<std::string, std::shared_ptr<Mc3Object>> definitions;
    auto base = definition("tree.base");
    base->assetMetadata = Mc3AssetMetadata{};
    base->assetMetadata->lods = {
        {"NeAr", "tree.near"}, {"mid", "tree.mid"}, {"far", "tree.far"}
    };
    base->assetMetadata->maxVisibilityDistanceM = 100.0f;
    definitions["tree.base"] = base;
    definitions["tree.near"] = definition("tree.near");
    definitions["tree.mid"] = definition("tree.mid");
    definitions["tree.far"] = definition("tree.far");

    Mc3Object instance;
    instance.type = ObjectType::Instance;
    instance.id = "tree-placement-17";
    instance.definition = "tree.base";

    const AssetLodConfig config{25.0f, 75.0f, 2.0f};
    const auto near = resolveAssetLodForInstanceAlg(instance, definitions, 5.0f, config);
    const auto mid = resolveAssetLodForInstanceAlg(instance, definitions, 40.0f, config);
    const auto far = resolveAssetLodForInstanceAlg(instance, definitions, 90.0f, config);
    check(!near.culled && near.tier == AssetLodTier::Near && near.definitionId == "tree.near",
          "near distance resolves the case-insensitive authored near definition");
    check(!mid.culled && mid.tier == AssetLodTier::Mid && mid.definitionId == "tree.mid",
          "mid distance resolves the authored mid definition");
    check(!far.culled && far.tier == AssetLodTier::Far && far.definitionId == "tree.far",
          "far distance resolves the authored far definition");

    const auto culled = resolveAssetLodForInstanceAlg(instance, definitions, 100.1f, config);
    check(culled.culled && culled.reason.find("max visibility") != std::string::npos,
          "maxVisibilityDistanceM culls the instance with an actionable reason");

    const AssetLodConfig hysteresisConfig{10.0f, 40.0f, 2.0f};
    check(selectAssetLodTierAlg(11.0f, hysteresisConfig, AssetLodTier::Near) == AssetLodTier::Near &&
          selectAssetLodTierAlg(12.0f, hysteresisConfig, AssetLodTier::Near) == AssetLodTier::Mid &&
          selectAssetLodTierAlg(9.0f, hysteresisConfig, AssetLodTier::Mid) == AssetLodTier::Mid &&
          selectAssetLodTierAlg(7.0f, hysteresisConfig, AssetLodTier::Mid) == AssetLodTier::Near,
          "hysteresis avoids tier thrashing until an exit boundary is crossed");

    base->assetMetadata->lods.erase("mid");
    const auto midFallback = resolveAssetLodForInstanceAlg(instance, definitions, 40.0f, config);
    check(midFallback.definitionId == "tree.near" &&
          midFallback.reason.find("fallback near") != std::string::npos,
          "a missing mid tier falls back to the finer authored near definition");

    base->assetMetadata->lods["far"] = "tree.missing";
    const auto missingTarget = resolveAssetLodForInstanceAlg(instance, definitions, 90.0f, config);
    check(!missingTarget.culled && missingTarget.definitionId == "tree.base" &&
          missingTarget.reason.find("unresolved") != std::string::npos,
          "an unresolved authored LOD target safely falls back to the base definition");

    instance.variantDefinitions = {"tree.base", "tree.near", "tree.mid"};
    const std::string firstVariant = instance.resolvedInstanceDefinitionKey();
    const std::string secondVariant = instance.resolvedInstanceDefinitionKey();
    const auto variantSelection = resolveDefaultAssetLodForInstanceAlg(instance, definitions);
    const std::string expectedVariant = instance.variantDefinitions[
        stableAssetIdentityHashAlg(instance.id) % instance.variantDefinitions.size()];
    check(firstVariant == secondVariant && firstVariant == expectedVariant &&
          variantSelection.baseDefinitionId == expectedVariant,
          "variant selection is stable from the instance identity and shared by asset LOD resolution");

    const auto normalised = normaliseAssetLodConfigAlg({-1.0f, -5.0f, -3.0f});
    check(normalised.midDistanceM == 0.0f && normalised.farDistanceM > normalised.midDistanceM &&
          normalised.hysteresisM == 0.0f,
          "invalid LOD thresholds are normalised into a safe ordered configuration");

    if (failures == 0) std::cout << "All asset LOD tests passed.\n";
    return failures == 0 ? 0 : 1;
}
