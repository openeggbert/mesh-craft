#pragma once
#include <array>
#include <cmath>
#include <numbers>
#include <string>

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

// STAB-0691: quaternion for a single axis-angle rotation (radians).
inline std::array<double, 4> quatFromAxisAngleRad(char axis, double angleRad) {
    double s = std::sin(angleRad * 0.5), c = std::cos(angleRad * 0.5);
    switch (axis) {
        case 'X': return {s, 0.0, 0.0, c};
        case 'Y': return {0.0, s, 0.0, c};
        case 'Z': return {0.0, 0.0, s, c};
        default:  return {0.0, 0.0, 0.0, 1.0};
    }
}

// Hamilton product a*b, q=[x,y,z,w].
inline std::array<double, 4> quatMul(const std::array<double,4>& a, const std::array<double,4>& b) {
    return {
        a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
        a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
        a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
        a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]
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
    const double toRad = rotationIsRadians ? 1.0 : (std::numbers::pi / 180.0);
    std::array<double, 4> qAxis[3] = {
        quatFromAxisAngleRad('X', x * toRad),
        quatFromAxisAngleRad('Y', y * toRad),
        quatFromAxisAngleRad('Z', z * toRad),
    };
    auto axisIdx = [](char c) { return c == 'X' ? 0 : (c == 'Y' ? 1 : 2); };
    const std::string& ord = order.size() == 3 ? order : "XYZ";
    return quatMul(quatMul(qAxis[axisIdx(ord[2])], qAxis[axisIdx(ord[1])]), qAxis[axisIdx(ord[0])]);
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
