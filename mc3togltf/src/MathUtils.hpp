#pragma once
#include <array>
#include <cmath>
#include <numbers>

namespace mc3togltf {

// Euler XYZ extrinsic (pitch, yaw, roll — degrees) → quaternion [x, y, z, w]
// Extrinsic XYZ: R = Rz * Ry * Rx  →  q = qz * qy * qx
inline std::array<double, 4> eulerXYZToQuat(float pitchDeg, float yawDeg, float rollDeg) {
    const double r = std::numbers::pi / 180.0;
    double cx = std::cos(pitchDeg * r * 0.5), sx = std::sin(pitchDeg * r * 0.5);
    double cy = std::cos(yawDeg   * r * 0.5), sy = std::sin(yawDeg   * r * 0.5);
    double cz = std::cos(rollDeg  * r * 0.5), sz = std::sin(rollDeg  * r * 0.5);

    // q = qZ * qY * qX  (Hamilton product, q=[x,y,z,w])
    return {
        cz*cy*sx - sz*sy*cx,   // x
        cz*sy*cx + sz*cy*sx,   // y
       -cz*sy*sx + sz*cy*cx,   // z
        cz*cy*cx + sz*sy*sx    // w
    };
}

// Decompose a "look-at" direction into pitch/yaw Euler angles (degrees)
inline std::array<double, 4> directionToQuat(float dx, float dy, float dz) {
    // Normalize
    float len = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (len < 1e-6f) return {0.0, 0.0, 0.0, 1.0};
    dx /= len; dy /= len; dz /= len;

    float yaw   = std::atan2(dx, dz);
    float pitch = std::asin(-dy);

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
