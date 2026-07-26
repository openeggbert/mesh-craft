#pragma once

// Coordinate-system conversion shared by the editor and exporters.
//
// MC3 stores authored values in either right-handed Y-up (the editor's
// native space) or right-handed Z-up.  Keep this header CNA-free so every
// consumer can use the same conversion instead of reimplementing an
// incompatible axis swap.  Z-up is converted to native Y-up by a -90 degree
// rotation around X: (x, y, z) -> (x, z, -y).

#include <array>
#include <string_view>

namespace MeshCraft {

inline constexpr std::string_view kRightHandedYUpCoordinateSystem =
    "right_handed_y_up";
inline constexpr std::string_view kRightHandedZUpCoordinateSystem =
    "right_handed_z_up";

[[nodiscard]] inline constexpr bool usesRightHandedZUpAlg(
    std::string_view coordinateSystem)
{
    return coordinateSystem == kRightHandedZUpCoordinateSystem;
}

// Convert a point or direction authored in document coordinates into the
// editor/glTF's right-handed Y-up space.  The operation is a pure rotation,
// therefore it is valid for both points and directions.
[[nodiscard]] inline constexpr std::array<float, 3> coordinateToYUpAlg(
    std::string_view coordinateSystem, std::array<float, 3> value)
{
    return usesRightHandedZUpAlg(coordinateSystem)
        ? std::array<float, 3>{value[0], value[2], -value[1]}
        : value;
}

// Inverse of coordinateToYUpAlg(), used by editor interaction paths: the
// viewport camera/ray is native Y-up, while picking still evaluates authored
// object bounds in the document's declared coordinate system.
[[nodiscard]] inline constexpr std::array<float, 3> coordinateFromYUpAlg(
    std::string_view coordinateSystem, std::array<float, 3> value)
{
    return usesRightHandedZUpAlg(coordinateSystem)
        ? std::array<float, 3>{value[0], -value[2], value[1]}
        : value;
}

} // namespace MeshCraft
