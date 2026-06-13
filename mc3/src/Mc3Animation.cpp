#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include <algorithm>
#include <cmath>

namespace MeshCraft::Mc3 {

const char* animatedPropertyName(AnimatedProperty p) {
    switch (p) {
    case AnimatedProperty::PositionX:          return "position.x";
    case AnimatedProperty::PositionY:          return "position.y";
    case AnimatedProperty::PositionZ:          return "position.z";
    case AnimatedProperty::RotationX:          return "rotation.x";
    case AnimatedProperty::RotationY:          return "rotation.y";
    case AnimatedProperty::RotationZ:          return "rotation.z";
    case AnimatedProperty::ScaleX:             return "scale.x";
    case AnimatedProperty::ScaleY:             return "scale.y";
    case AnimatedProperty::ScaleZ:             return "scale.z";
    case AnimatedProperty::Visible:            return "visible";
    case AnimatedProperty::DeformX:            return "deform.x";
    case AnimatedProperty::DeformY:            return "deform.y";
    case AnimatedProperty::DeformZ:            return "deform.z";
    case AnimatedProperty::MaterialBaseColorR: return "material.baseColor.r";
    case AnimatedProperty::MaterialBaseColorG: return "material.baseColor.g";
    case AnimatedProperty::MaterialBaseColorB: return "material.baseColor.b";
    case AnimatedProperty::MaterialBaseColorA: return "material.baseColor.a";
    case AnimatedProperty::MaterialRoughness:  return "material.roughness";
    case AnimatedProperty::MaterialMetallic:   return "material.metallic";
    case AnimatedProperty::MaterialEmissiveR:  return "material.emissive.r";
    case AnimatedProperty::MaterialEmissiveG:  return "material.emissive.g";
    case AnimatedProperty::MaterialEmissiveB:  return "material.emissive.b";
    }
    return "unknown";
}

std::optional<AnimatedProperty> animatedPropertyFromName(const std::string& n) {
    if (n == "position.x")           return AnimatedProperty::PositionX;
    if (n == "position.y")           return AnimatedProperty::PositionY;
    if (n == "position.z")           return AnimatedProperty::PositionZ;
    if (n == "rotation.x")           return AnimatedProperty::RotationX;
    if (n == "rotation.y")           return AnimatedProperty::RotationY;
    if (n == "rotation.z")           return AnimatedProperty::RotationZ;
    if (n == "scale.x")              return AnimatedProperty::ScaleX;
    if (n == "scale.y")              return AnimatedProperty::ScaleY;
    if (n == "scale.z")              return AnimatedProperty::ScaleZ;
    if (n == "visible")              return AnimatedProperty::Visible;
    if (n == "deform.x")             return AnimatedProperty::DeformX;
    if (n == "deform.y")             return AnimatedProperty::DeformY;
    if (n == "deform.z")             return AnimatedProperty::DeformZ;
    if (n == "material.baseColor.r") return AnimatedProperty::MaterialBaseColorR;
    if (n == "material.baseColor.g") return AnimatedProperty::MaterialBaseColorG;
    if (n == "material.baseColor.b") return AnimatedProperty::MaterialBaseColorB;
    if (n == "material.baseColor.a") return AnimatedProperty::MaterialBaseColorA;
    if (n == "material.roughness")   return AnimatedProperty::MaterialRoughness;
    if (n == "material.metallic")    return AnimatedProperty::MaterialMetallic;
    if (n == "material.emissive.r")  return AnimatedProperty::MaterialEmissiveR;
    if (n == "material.emissive.g")  return AnimatedProperty::MaterialEmissiveG;
    if (n == "material.emissive.b")  return AnimatedProperty::MaterialEmissiveB;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Interpolation
// ---------------------------------------------------------------------------

static float cubicBez(float t, float p0, float p1, float p2, float p3) {
    float u = 1.0f - t;
    return u*u*u*p0 + 3.0f*u*u*t*p1 + 3.0f*u*t*t*p2 + t*t*t*p3;
}

// Binary-search for the bezier parameter u such that bezier_x(u) == targetX.
static float solveBezierU(float targetX, float x0, float x1, float x2, float x3) {
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < 20; ++i) {
        float mid = (lo + hi) * 0.5f;
        (cubicBez(mid, x0, x1, x2, x3) < targetX ? lo : hi) = mid;
    }
    return (lo + hi) * 0.5f;
}

float evaluateChannel(const Mc3Channel& ch, float time) {
    const auto& kfs = ch.keyframes;
    if (kfs.empty())              return 0.0f;
    if (kfs.size() == 1)          return kfs.front().value;
    if (time <= kfs.front().time) return kfs.front().value;
    if (time >= kfs.back().time)  return kfs.back().value;

    // Find the segment [k0, k1] that brackets time
    auto it = std::upper_bound(kfs.begin(), kfs.end(), time,
        [](float t, const Mc3Keyframe& k){ return t < k.time; });
    const Mc3Keyframe& k1 = *it;
    const Mc3Keyframe& k0 = *(it - 1);

    switch (k0.interpolation) {
    case Interpolation::Step:
        return k0.value;

    case Interpolation::Linear: {
        float span = k1.time - k0.time;
        if (span <= 0.0f) return k0.value;
        float t = (time - k0.time) / span;
        return k0.value + t * (k1.value - k0.value);
    }

    case Interpolation::CubicBezier: {
        float tx0 = k0.time,                    tx1 = k0.time + k0.handleRight.dt;
        float tx2 = k1.time + k1.handleLeft.dt, tx3 = k1.time;
        float vy0 = k0.value,                    vy1 = k0.value + k0.handleRight.dv;
        float vy2 = k1.value + k1.handleLeft.dv, vy3 = k1.value;
        float u = solveBezierU(time, tx0, tx1, tx2, tx3);
        return cubicBez(u, vy0, vy1, vy2, vy3);
    }
    }
    return k0.value;
}

} // namespace MeshCraft::Mc3
