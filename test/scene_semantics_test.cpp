// SYS-W3-02 -- CNA-free contracts shared by viewport and export consumers.

#include <MeshCraft/SceneSemanticsAlgorithms.hpp>

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

std::shared_ptr<Mc3Object> definition(const std::string& id, const std::string& material) {
    auto object = std::make_shared<Mc3Object>();
    object->id = id;
    object->name = id;
    object->type = ObjectType::Box;
    object->material = material;
    object->primitive = Mc3Primitive::box({1.0f, 1.0f, 1.0f});
    return object;
}

} // namespace

int main() {
    Mc3Transform transform;
    transform.position = {3.0f, -2.0f, 5.0f};
    transform.pivot = {0.5f, 1.0f, -0.25f};
    const auto pivoted = objectTransformSemanticsAlg(transform);
    check(pivoted.hasPivot &&
          pivoted.outerTranslation == std::array<float, 3>{3.5f, -1.0f, 4.75f} &&
          pivoted.childOriginOffset == std::array<float, 3>{-0.5f, -1.0f, 0.25f},
          "pivot semantics provide the same outer translation and inner origin offset");

    transform.pivot = {0.0f, 0.0f, 0.0f};
    const auto unpivoted = objectTransformSemanticsAlg(transform);
    check(!unpivoted.hasPivot &&
          unpivoted.outerTranslation == transform.position &&
          unpivoted.childOriginOffset == std::array<float, 3>{},
          "zero pivot preserves the ordinary object translation");

    Mc3Object direct;
    direct.material = "base";
    direct.materialOverride = "override";
    Mc3Object def;
    def.material = "definition-base";
    def.materialOverride = "definition-override";
    Mc3Object instance;
    check(effectiveObjectMaterialIdAlg(direct) == "override" &&
          effectiveInstanceMaterialIdAlg(instance, def) == "definition-override",
          "material override wins locally and definitions supply an empty instance fallback");
    instance.material = "instance-base";
    instance.materialOverride = "instance-override";
    check(effectiveInstanceMaterialIdAlg(instance, def) == "instance-override",
          "an instance's own override wins over the resolved definition material");

    check(!effectiveObjectVisibilityAlg(false) &&
          effectiveObjectVisibilityAlg(false, true) &&
          !effectiveObjectVisibilityAlg(true, false),
          "authored and animated visibility use one explicit precedence rule");

    direct.id = "canonical-id";
    direct.name = "fallback-name";
    check(stableObjectIdentityKeyAlg(direct) == "canonical-id",
          "stable identity prefers a canonical MC3 ID");
    direct.id.clear();
    check(stableObjectIdentityKeyAlg(direct) == "fallback-name",
          "stable identity uses a legacy name only when ID is absent");
    direct.name.clear();
    check(stableObjectIdentityKeyAlg(direct).empty(),
          "objects without ID or name do not receive process-local synthetic identities");

    std::map<std::string, std::shared_ptr<Mc3Object>> definitions;
    auto base = definition("tree.base", "bark");
    base->assetMetadata = Mc3AssetMetadata{};
    base->assetMetadata->lods = {
        {"near", "tree.near"}, {"mid", "tree.mid"}, {"far", "tree.far"}
    };
    definitions["tree.base"] = base;
    definitions["tree.near"] = definition("tree.near", "near-bark");
    definitions["tree.mid"] = definition("tree.mid", "mid-bark");
    definitions["tree.far"] = definition("tree.far", "far-bark");

    instance = Mc3Object{};
    instance.type = ObjectType::Instance;
    instance.id = "tree-placement";
    instance.definition = "tree.base";

    const AssetLodConfig viewportConfig{25.0f, 75.0f, 2.0f};
    const auto viewport = resolveInstanceSemanticsAlg(instance, definitions, 40.0f, viewportConfig);
    const auto exported = resolveDefaultInstanceSemanticsAlg(instance, definitions);
    const auto legacyExportSelection = resolveDefaultAssetLodForInstanceAlg(instance, definitions);
    check(viewport.lod.tier == AssetLodTier::Mid &&
          viewport.definition == definitions.at("tree.mid").get(),
          "viewport distance resolution binds the selected LOD definition pointer once");
    check(exported.lod.definitionId == "tree.near" &&
          exported.definition == definitions.at("tree.near").get() &&
          exported.lod.definitionId == legacyExportSelection.definitionId,
          "export default and the existing asset-LOD policy select the same near definition");

    instance.definition = "missing";
    const auto unresolved = resolveDefaultInstanceSemanticsAlg(instance, definitions);
    check(!unresolved.resolved() && unresolved.definition == nullptr &&
          unresolved.lod.reason.find("unresolved") != std::string::npos,
          "an unresolved definition remains explicit instead of returning a dangling pointer");

    if (failures == 0) std::cout << "All scene semantic tests passed.\n";
    return failures == 0 ? 0 : 1;
}
