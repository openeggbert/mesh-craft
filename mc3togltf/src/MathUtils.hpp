#pragma once
#include <MeshCraft/RotationConventionAlgorithms.hpp>
#include <array>
#include <cmath>
#include <numbers>
#include <string>

namespace mc3togltf {

// Euler XYZ extrinsic (pitch, yaw, roll — degrees) → quaternion [x, y, z, w]
// Extrinsic XYZ: R = Rz * Ry * Rx  →  q = qz * qy * qx
inline std::array<double, 4> eulerXYZToQuat(float pitchDeg, float yawDeg, float rollDeg) {
    const auto quaternion = MeshCraft::rotationQuaternionAlg(
        {pitchDeg, yawDeg, rollDeg}, "degrees", "XYZ");
    return {
        quaternion.x, quaternion.y, quaternion.z, quaternion.w
    };
}

// STAB-0691: general Euler-to-quaternion honoring doc.rotationUnits (degrees
// vs radians) and doc.eulerOrder (any of the 6 axis-order permutations, e.g.
// "ZYX"), instead of eulerXYZToQuat()'s hardcoded degrees+XYZ assumption.
// order[0] is applied first (innermost), order[2] last (outermost):
// R = R[order[2]] * R[order[1]] * R[order[0]], matching eulerXYZToQuat's own
// "Extrinsic XYZ: R = Rz*Ry*Rx" convention when order=="XYZ" (verified
// numerically identical to eulerXYZToQuat for that case).
inline std::array<double, 4> eulerToQuat(float x, float y, float z,
                                          bool rotationIsRadians,
                                          const std::string& order) {
    const auto quaternion = MeshCraft::rotationQuaternionAlg(
        {x, y, z}, rotationIsRadians ? "radians" : "degrees", order);
    return {quaternion.x, quaternion.y, quaternion.z, quaternion.w};
}

// Decompose a "look-at" direction into pitch/yaw Euler angles (degrees)
inline std::array<double, 4> directionToQuat(float dx, float dy, float dz) {
    // Normalize
    float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len < 1e-6f) return {0.0, 0.0, 0.0, 1.0};
    dx /= len; dy /= len; dz /= len;

    // Camera/light default forward is -Z (glTF convention).
    // R_Y(yaw) * R_X(pitch) maps -Z to (dx,dy,dz)  →  yaw=atan2(-dx,-dz), pitch=asin(dy)
    float yaw   = std::atan2(-dx, -dz);
    float pitch = std::asin(dy);

    const float toDeg = 180.0f / std::numbers::pi_v<float>;
    return eulerXYZToQuat(pitch * toDeg, yaw * toDeg, 0.0f);
}

// Parse "x y z" or "x y z w" string
inline std::array<float, 3> parseVec3(const std::string& s, std::array<float,3> def = {0,0,0}) {
    std::array<float,3> out = def;
    int n = std::sscanf(s.c_str(), "%f %f %f", &out[0], &out[1], &out[2]);
    if (n < 3) n = std::sscanf(s.c_str(), "%f,%f,%f", &out[0], &out[1], &out[2]);
    return out;
}

inline std::array<float, 4> parseVec4(const std::string& s, std::array<float,4> def = {0,0,0,1}) {
    std::array<float,4> out = def;
    int n = std::sscanf(s.c_str(), "%f %f %f %f", &out[0], &out[1], &out[2], &out[3]);
    if (n < 4) n = std::sscanf(s.c_str(), "%f,%f,%f,%f", &out[0], &out[1], &out[2], &out[3]);
    return out;
}

inline std::array<float, 2> parseVec2(const std::string& s, std::array<float,2> def = {0,0}) {
    std::array<float,2> out = def;
    int n = std::sscanf(s.c_str(), "%f %f", &out[0], &out[1]);
    if (n < 2) n = std::sscanf(s.c_str(), "%f,%f", &out[0], &out[1]);
    return out;
}

inline bool parseBool(const std::string& s) {
    return s == "true" || s == "1" || s == "yes";
}

} // namespace mc3togltf
