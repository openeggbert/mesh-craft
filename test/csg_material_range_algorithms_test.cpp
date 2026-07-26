// SYS-W14-34 -- CSG preview material partitioning and cache invalidation.

#include <MeshCraft/CsgMaterialRangeAlgorithms.hpp>
#include <MeshCraft/Renderer/CsgCacheAlg.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) std::cout << "PASS: " << message << '\n';
    else { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

} // namespace

int main() {
    using namespace MeshCraft;
    using namespace MeshCraft::Mc3;

    const std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0, 4, 5, 6};
    const std::vector<std::string> materials{"red", "blue", "red"};
    const auto childRanges = csgMaterialRangesAlg(indices, materials, "");
    check(childRanges.size() == 2 && childRanges[0].materialId == "red" &&
              childRanges[0].indices.size() == 6 && childRanges[1].materialId == "blue" &&
              childRanges[1].indices.size() == 3,
          "unmaterialed CSG root retains every child-material triangle range");

    const auto rootRange = csgMaterialRangesAlg(indices, materials, "root");
    check(rootRange.size() == 1 && rootRange[0].materialId == "root" &&
              rootRange[0].indices == indices,
          "explicit CSG root material overrides all child material ranges");

    const auto invalidRelationRange = csgMaterialRangesAlg(indices, {"red"}, "");
    check(invalidRelationRange.size() == 1 && invalidRelationRange[0].materialId.empty() &&
              invalidRelationRange[0].indices == indices,
          "invalid Manifold relation data safely uses one unmaterialed range");

    Mc3Document doc;
    auto root = std::make_shared<Mc3Object>();
    root->id = "root";
    root->type = ObjectType::Union;
    auto child = std::make_shared<Mc3Object>();
    child->id = "child";
    child->type = ObjectType::Box;
    child->primitive = Mc3Primitive{};
    child->material = "red";
    root->children.push_back(child);
    const std::size_t childMaterialHash = csgSubtreeHashAlg(*root, doc);
    child->material = "blue";
    check(childMaterialHash != csgSubtreeHashAlg(*root, doc),
          "changing a CSG child material invalidates the preview cache key");

    const std::size_t rootMaterialHash = csgSubtreeHashAlg(*root, doc);
    root->materialOverride = "root";
    check(rootMaterialHash != csgSubtreeHashAlg(*root, doc),
          "changing a CSG root override invalidates the preview cache key");

    if (failures == 0) std::cout << "All CSG material-range algorithm tests passed.\n";
    return failures == 0 ? 0 : 1;
}
