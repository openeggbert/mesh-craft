#pragma once

// CNA-free material-range partitioning for a Manifold CSG result. The glTF
// exporter uses the same two authoring rules: an explicit root material owns
// the whole result; otherwise each output triangle keeps its source material.

#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace MeshCraft {

struct CsgMaterialRangeAlg {
    std::string materialId;
    std::vector<uint32_t> indices;
};

[[nodiscard]] inline std::vector<CsgMaterialRangeAlg> csgMaterialRangesAlg(
    const std::vector<uint32_t>& indices,
    const std::vector<std::string>& triangleMaterials,
    std::string_view rootMaterialId)
{
    if (indices.empty() || indices.size() % 3 != 0) return {};

    const size_t triangleCount = indices.size() / 3;
    // This mirrors addCsgMeshDataToGltf(): an explicit root material, or an
    // unusable relation vector, deliberately collapses the result to one
    // range. An empty ID means the default unmaterialed viewport appearance.
    if (!rootMaterialId.empty() || triangleMaterials.size() != triangleCount)
        return {{std::string(rootMaterialId), indices}};

    std::vector<CsgMaterialRangeAlg> ranges;
    for (size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const std::string& materialId = triangleMaterials[triangle];
        auto range = ranges.begin();
        while (range != ranges.end() && range->materialId != materialId) ++range;
        if (range == ranges.end()) {
            ranges.push_back({materialId, {}});
            range = std::prev(ranges.end());
        }
        const size_t index = triangle * 3;
        range->indices.insert(range->indices.end(), indices.begin() + index, indices.begin() + index + 3);
    }
    return ranges;
}

} // namespace MeshCraft
