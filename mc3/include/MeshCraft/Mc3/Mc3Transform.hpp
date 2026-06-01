#pragma once

#include <array>

namespace MeshCraft::Mc3 {

// Local transform of an MC3 object.
// Rotation is stored as extrinsic XYZ Euler angles in degrees.
struct Mc3Transform {
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 3> rotation{0.0f, 0.0f, 0.0f}; // pitch, yaw, roll (XYZ extrinsic)
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
    std::array<float, 3> pivot{0.0f, 0.0f, 0.0f};
};

} // namespace MeshCraft::Mc3
