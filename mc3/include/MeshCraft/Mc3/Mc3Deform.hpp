#pragma once
#include <array>

namespace MeshCraft::Mc3 {

// Geometry-level non-uniform scale applied before the scene transform.
// Does not propagate to children — use Mc3Transform::scale for that.
struct Mc3Deform {
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
};

} // namespace MeshCraft::Mc3
