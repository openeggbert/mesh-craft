#pragma once
#include <array>
#include <optional>
#include <string>

namespace MeshCraft::Mc3 {

enum class FogMode { Linear, Exponential };

struct Mc3Fog {
    std::array<float, 3> color{0.5f, 0.5f, 0.5f};
    FogMode mode{FogMode::Linear};
    float start{10.0f};
    float end{100.0f};
    float density{0.01f};
};

struct Mc3Environment {
    std::array<float, 3> backgroundColor{0.0f, 0.0f, 0.0f};
    std::string backgroundTexture;   // flat 2D background (I1)
    std::string skyboxTexture;       // equirectangular panorama (I2)
    std::optional<Mc3Fog> fog;
};

} // namespace MeshCraft::Mc3
