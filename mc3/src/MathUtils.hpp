#pragma once
#include <array>
#include <cmath>
#include <numbers>
#include <string>

namespace MeshCraft::Mc3::Internal {

// Reject non-finite floats (NaN, +/-Inf) from untrusted input.
//
// std::stof and std::sscanf("%f") both accept the textual forms "nan", "inf"
// and "-inf" and return the corresponding non-finite value WITHOUT throwing, so
// a scene attribute like width="nan" or z="inf" used to flow straight into the
// geometry pipeline — poisoning bounds, normals and CSG, and rendering as
// garbage or NaN-propagated crashes. Every numeric parse in the MC3 reader
// funnels through this so a non-finite value is replaced by the caller's
// default instead of entering the model.
inline float finiteOr(float v, float def) {
    return std::isfinite(v) ? v : def;
}

inline std::array<float, 3> parseVec3(const std::string& s, std::array<float,3> def = {0,0,0}) {
    std::array<float,3> out = def;
    int n = std::sscanf(s.c_str(), "%f %f %f", &out[0], &out[1], &out[2]);
    if (n < 3) std::sscanf(s.c_str(), "%f,%f,%f", &out[0], &out[1], &out[2]);
    for (int i = 0; i < 3; ++i) out[i] = finiteOr(out[i], def[i]);
    return out;
}

inline std::array<float, 4> parseVec4(const std::string& s, std::array<float,4> def = {0,0,0,1}) {
    std::array<float,4> out = def;
    int n = std::sscanf(s.c_str(), "%f %f %f %f", &out[0], &out[1], &out[2], &out[3]);
    if (n < 4) std::sscanf(s.c_str(), "%f,%f,%f,%f", &out[0], &out[1], &out[2], &out[3]);
    for (int i = 0; i < 4; ++i) out[i] = finiteOr(out[i], def[i]);
    return out;
}

inline bool parseBool(const std::string& s) {
    return s == "true" || s == "1" || s == "yes";
}

} // namespace MeshCraft::Mc3::Internal
