#pragma once
#include <array>
#include <string>

namespace MeshCraft::Mc3 {

enum class LightType { Ambient, Directional, Spot, Point };

struct Mc3Light {
    LightType type{LightType::Directional};
    std::string name;

    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
    float brightness{1.0f};

    // Directional / Spot
    std::array<float, 3> direction{0.0f, -1.0f, 0.0f};

    // Spot / Point
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    float range{0.0f}; // 0 = unlimited

    // Spot only
    float angle{45.0f};   // half-angle in degrees
    float falloff{0.0f};  // 0..1

    bool castShadows{false};
};

} // namespace MeshCraft::Mc3
