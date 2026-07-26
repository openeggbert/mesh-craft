#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string_view>

namespace MeshCraft {

// CNA-free interpretation of MC3's document-level rotation convention.
// Angles always remain associated with X/Y/Z components; eulerOrder controls
// the application sequence. For order XYZ the resulting extrinsic rotation is
// Rz * Ry * Rx, exactly matching the historical degrees/XYZ exporter path.
struct RotationQuaternionAlg {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};
};

using RotationMatrix3Alg = std::array<std::array<float, 3>, 3>;

[[nodiscard]] inline bool rotationUsesRadiansAlg(std::string_view units)
{
    return units == "radians";
}

[[nodiscard]] inline std::string_view normalisedEulerOrderAlg(std::string_view order)
{
    if (order.size() != 3) return "XYZ";
    bool x = false, y = false, z = false;
    for (const char axis : order) {
        if (axis == 'X') x = true;
        else if (axis == 'Y') y = true;
        else if (axis == 'Z') z = true;
        else return "XYZ";
    }
    return x && y && z ? order : "XYZ";
}

[[nodiscard]] inline RotationQuaternionAlg rotationQuaternionMultiplyAlg(
    const RotationQuaternionAlg& a, const RotationQuaternionAlg& b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

[[nodiscard]] inline RotationQuaternionAlg rotationQuaternionForAxisAlg(char axis, float radians)
{
    const float sine = std::sin(radians * 0.5f);
    const float cosine = std::cos(radians * 0.5f);
    switch (axis) {
    case 'X': return {sine, 0.0f, 0.0f, cosine};
    case 'Y': return {0.0f, sine, 0.0f, cosine};
    case 'Z': return {0.0f, 0.0f, sine, cosine};
    default:  return {};
    }
}

[[nodiscard]] inline int rotationAxisIndexAlg(char axis)
{
    return axis == 'X' ? 0 : axis == 'Y' ? 1 : 2;
}

[[nodiscard]] inline RotationQuaternionAlg rotationQuaternionAlg(
    const std::array<float, 3>& rotation, std::string_view units,
    std::string_view order)
{
    const float toRadians = rotationUsesRadiansAlg(units)
        ? 1.0f : std::numbers::pi_v<float> / 180.0f;
    const std::array<RotationQuaternionAlg, 3> axisQuaternions{
        rotationQuaternionForAxisAlg('X', rotation[0] * toRadians),
        rotationQuaternionForAxisAlg('Y', rotation[1] * toRadians),
        rotationQuaternionForAxisAlg('Z', rotation[2] * toRadians),
    };
    const std::string_view effectiveOrder = normalisedEulerOrderAlg(order);
    return rotationQuaternionMultiplyAlg(
        rotationQuaternionMultiplyAlg(axisQuaternions[rotationAxisIndexAlg(effectiveOrder[2])],
                                      axisQuaternions[rotationAxisIndexAlg(effectiveOrder[1])]),
        axisQuaternions[rotationAxisIndexAlg(effectiveOrder[0])]);
}

[[nodiscard]] inline RotationMatrix3Alg rotationMatrix3Alg(const RotationQuaternionAlg& q)
{
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float xw = q.x * q.w, yw = q.y * q.w, zw = q.z * q.w;
    // Row-vector matrix, matching CNA's Matrix::CreateFromQuaternion().
    return {{{
        1.0f - 2.0f * (yy + zz), 2.0f * (xy + zw),        2.0f * (xz - yw),
    }, {
        2.0f * (xy - zw),        1.0f - 2.0f * (zz + xx), 2.0f * (yz + xw),
    }, {
        2.0f * (xz + yw),        2.0f * (yz - xw),        1.0f - 2.0f * (yy + xx),
    }}};
}

[[nodiscard]] inline RotationMatrix3Alg rotationMatrix3Alg(
    const std::array<float, 3>& rotation, std::string_view units,
    std::string_view order)
{
    return rotationMatrix3Alg(rotationQuaternionAlg(rotation, units, order));
}

[[nodiscard]] inline std::array<float, 3> rotateDirectionAlg(
    const std::array<float, 3>& direction, const RotationMatrix3Alg& matrix)
{
    return {
        direction[0] * matrix[0][0] + direction[1] * matrix[1][0] + direction[2] * matrix[2][0],
        direction[0] * matrix[0][1] + direction[1] * matrix[1][1] + direction[2] * matrix[2][1],
        direction[0] * matrix[0][2] + direction[1] * matrix[1][2] + direction[2] * matrix[2][2],
    };
}

[[nodiscard]] inline std::array<float, 3> rotationAsDegreesXYZAlg(
    const std::array<float, 3>& rotation, std::string_view units,
    std::string_view order)
{
    const auto q = rotationQuaternionAlg(rotation, units, order);
    // Inverse of the qZ * qY * qX convention above. Do this in quaternion
    // space rather than through a framework matrix so normalisation stays
    // CNA-free and follows the same convention as mc3togltf exactly.
    const float x = std::atan2(2.0f * (q.w * q.x + q.y * q.z),
                               1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    const float y = std::asin(std::clamp(2.0f * (q.w * q.y - q.z * q.x),
                                         -1.0f, 1.0f));
    const float z = std::atan2(2.0f * (q.w * q.z + q.x * q.y),
                               1.0f - 2.0f * (q.y * q.y + q.z * q.z));
    const float toDegrees = 180.0f / std::numbers::pi_v<float>;
    return {x * toDegrees, y * toDegrees, z * toDegrees};
}

[[nodiscard]] inline float degreesInRotationUnitsAlg(float degrees, std::string_view units)
{
    return rotationUsesRadiansAlg(units)
        ? degrees * (std::numbers::pi_v<float> / 180.0f) : degrees;
}

[[nodiscard]] inline const char* rotationUnitLabelAlg(std::string_view units)
{
    return rotationUsesRadiansAlg(units) ? "rad" : "°";
}

} // namespace MeshCraft
