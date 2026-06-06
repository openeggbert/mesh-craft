#pragma once
#include <array>
#include <cmath>
#include <numbers>
#include <string>

namespace MeshCraft::Mc3::Internal {

inline std::array<float, 3> parseVec3(const std::string& s, std::array<float,3> def = {0,0,0}) {
    std::array<float,3> out = def;
    int n = std::sscanf(s.c_str(), "%f %f %f", &out[0], &out[1], &out[2]);
    if (n < 3) std::sscanf(s.c_str(), "%f,%f,%f", &out[0], &out[1], &out[2]);
    return out;
}

inline std::array<float, 4> parseVec4(const std::string& s, std::array<float,4> def = {0,0,0,1}) {
    std::array<float,4> out = def;
    int n = std::sscanf(s.c_str(), "%f %f %f %f", &out[0], &out[1], &out[2], &out[3]);
    if (n < 4) std::sscanf(s.c_str(), "%f,%f,%f,%f", &out[0], &out[1], &out[2], &out[3]);
    return out;
}

inline bool parseBool(const std::string& s) {
    return s == "true" || s == "1" || s == "yes";
}

} // namespace MeshCraft::Mc3::Internal
