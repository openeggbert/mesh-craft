#pragma once

#include <array>
#include <string>

namespace MeshCraft::Mc3 {

struct Mc3Material {
    std::string name;

    std::array<float, 4> baseColor{0.8f, 0.8f, 0.8f, 1.0f};
    std::string baseColorTexture;
    std::string normalTexture;
    std::string emissiveTexture;

    // Combined metallic-roughness texture (ORM: R=occlusion, G=roughness, B=metallic)
    std::string metallicRoughnessTexture;
    std::string occlusionTexture;

    float roughness{0.5f};
    float metallic{0.0f};
    float normalScale{1.0f};        // multiplier for the normal map
    float occlusionStrength{1.0f};  // 0 = no occlusion, 1 = full

    std::array<float, 3> emissiveColor{0.0f, 0.0f, 0.0f};

    std::string alphaMode{"opaque"}; // opaque | mask | blend
    float alphaCutoff{0.5f};         // used when alphaMode == "mask"
    bool doubleSided{false};
};

} // namespace MeshCraft::Mc3
