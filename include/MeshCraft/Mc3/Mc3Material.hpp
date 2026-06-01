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

    float roughness{0.5f};
    float metallic{0.0f};
    std::array<float, 3> emissiveColor{0.0f, 0.0f, 0.0f};

    std::string alphaMode{"opaque"}; // opaque | mask | blend
    bool doubleSided{false};
};

} // namespace MeshCraft::Mc3
