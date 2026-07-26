#pragma once

// CNA-free UV projection and transform rules for ordinary MC3 geometry.
// CSG owns a separate generated-mesh path because its positions are already
// baked by Manifold; do not apply these rules to that cache a second time.

#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace MeshCraft {

using UvPositionAlg = std::array<float, 3>;
using UvCoordinateAlg = std::array<float, 2>;

[[nodiscard]] inline UvCoordinateAlg transformUvCoordinateAlg(
    UvCoordinateAlg coordinate, const Mc3::Mc3UvMapping& mapping)
{
    const float radians = mapping.rotation * std::numbers::pi_v<float> / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float scaledU = coordinate[0] * mapping.scaleU;
    const float scaledV = coordinate[1] * mapping.scaleV;
    return {scaledU * cosine - scaledV * sine + mapping.offsetU,
            scaledU * sine + scaledV * cosine + mapping.offsetV};
}

[[nodiscard]] inline UvPositionAlg uvBoundsCenterAlg(
    const std::vector<UvPositionAlg>& positions)
{
    if (positions.empty()) return {0.0f, 0.0f, 0.0f};
    UvPositionAlg minimum = positions.front();
    UvPositionAlg maximum = positions.front();
    for (const UvPositionAlg& position : positions) {
        for (int axis = 0; axis < 3; ++axis) {
            minimum[axis] = std::min(minimum[axis], position[axis]);
            maximum[axis] = std::max(maximum[axis], position[axis]);
        }
    }
    return {(minimum[0] + maximum[0]) * 0.5f,
            (minimum[1] + maximum[1]) * 0.5f,
            (minimum[2] + maximum[2]) * 0.5f};
}

// Matches MeshData::applyBoxProjectionUv(): use an authored normal when it
// exists, otherwise use the direction away from the local bounds centre.
[[nodiscard]] inline UvCoordinateAlg boxProjectedUvAlg(
    const UvPositionAlg& position, const UvPositionAlg& direction)
{
    const float absX = std::fabs(direction[0]);
    const float absY = std::fabs(direction[1]);
    const float absZ = std::fabs(direction[2]);
    if (absX >= absY && absX >= absZ) return {position[2], position[1]};
    if (absY >= absX && absY >= absZ) return {position[0], position[2]};
    return {position[0], position[1]};
}

[[nodiscard]] inline UvCoordinateAlg sphereProjectedUvAlg(
    const UvPositionAlg& position, const UvPositionAlg& center)
{
    const float dx = position[0] - center[0];
    const float dy = position[1] - center[1];
    const float dz = position[2] - center[2];
    const float radius = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (radius < 1e-8f) return {0.5f, 0.5f};
    const float u = 0.5f + std::atan2(dz, dx) /
                            (2.0f * std::numbers::pi_v<float>);
    const float v = 0.5f - std::asin(std::clamp(dy / radius, -1.0f, 1.0f)) /
                            std::numbers::pi_v<float>;
    return {u, v};
}

// Returns UVs for an ordinary (non-CSG) mesh. Planar mapping intentionally
// keeps the mesh's authored/default unwrap, while Box and Sphere regenerate
// it from local positions before all mappings use scale -> rotate -> offset.
[[nodiscard]] inline std::vector<UvCoordinateAlg> mapObjectUvsAlg(
    const std::vector<UvPositionAlg>& positions,
    const std::vector<UvPositionAlg>& normals,
    const std::vector<UvCoordinateAlg>& defaultCoordinates,
    const Mc3::Mc3UvMapping& mapping)
{
    std::vector<UvCoordinateAlg> result(positions.size(), {0.0f, 0.0f});
    const bool hasNormals = normals.size() == positions.size();
    const UvPositionAlg center = uvBoundsCenterAlg(positions);
    for (size_t index = 0; index < positions.size(); ++index) {
        UvCoordinateAlg coordinate = index < defaultCoordinates.size()
            ? defaultCoordinates[index] : UvCoordinateAlg{0.0f, 0.0f};
        if (mapping.projection == Mc3::UvProjection::Box) {
            const UvPositionAlg direction = hasNormals ? normals[index] : UvPositionAlg{
                positions[index][0] - center[0], positions[index][1] - center[1],
                positions[index][2] - center[2]};
            coordinate = boxProjectedUvAlg(positions[index], direction);
        } else if (mapping.projection == Mc3::UvProjection::Sphere) {
            coordinate = sphereProjectedUvAlg(positions[index], center);
        }
        result[index] = transformUvCoordinateAlg(coordinate, mapping);
    }
    return result;
}

} // namespace MeshCraft
