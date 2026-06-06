#pragma once
#include <array>
#include <optional>
#include <string>

namespace MeshCraft::Mc3 {

enum class CameraType { Perspective, Orthographic };

struct Mc3Camera {
    std::string name;
    CameraType  type{CameraType::Perspective};

    std::array<float, 3> position{0.0f, 5.0f, 10.0f};
    std::array<float, 3> target{0.0f, 0.0f, 0.0f};
    std::optional<std::array<float, 3>> rotation; // alternative to target

    float nearPlane{0.1f};
    float farPlane{1000.0f};
    float fov{60.0f};        // perspective: vertical field of view in degrees
    float orthoSize{10.0f};  // orthographic: half-height in world units
};

} // namespace MeshCraft::Mc3
