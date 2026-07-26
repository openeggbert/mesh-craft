#pragma once

// CNA-free punctual-light rules shared by the live ShaderEffect setup and
// focused tests. MC3 brightness remains an authored unitless multiplier; the
// glTF exporter performs its own required photometric-unit conversion.

#include <algorithm>
#include <cmath>
#include <numbers>

namespace MeshCraft {

// Eight lights keep the live fragment shader bounded on GLES while covering
// ordinary authored scenes. Lights are taken in document order.
inline constexpr int kViewportPunctualLightLimit = 8;

struct SpotConeParametersAlg {
    float innerCos{1.0f};
    float outerCos{1.0f};
};

// A source-GLSL shader can only be selected when the backend supports the
// CNA ShaderEffect contract, compilation succeeded, and there is an authored
// point/spot light worth representing. Every other case deliberately uses
// the established BasicEffect + gizmo fallback.
[[nodiscard]] inline bool pointSpotShaderEnabledAlg(bool sourceShadersSupported,
                                                     bool shaderIsValid,
                                                     int punctualLightCount)
{
    return sourceShadersSupported && shaderIsValid && punctualLightCount > 0;
}

// MC3's range is a hard authored limit (zero means unlimited). Within that
// limit, this is ordinary inverse-square attenuation with a small denominator
// floor that prevents a light exactly on a surface from producing infinity.
[[nodiscard]] inline float punctualRangeAttenuationAlg(float distance, float range)
{
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    if (std::isfinite(range) && range > 0.0f && distance >= range) return 0.0f;
    return 1.0f / std::max(distance * distance, 0.01f);
}

// MC3 spot angle is the outer half-angle in degrees. falloff interpolates
// from a hard edge (0) to an inner cone at zero degrees (1), matching the
// exporter’s `inner = outer * (1 - falloff)` KHR_lights_punctual mapping.
[[nodiscard]] inline SpotConeParametersAlg spotConeParametersAlg(float halfAngleDegrees,
                                                                   float falloff)
{
    const float safeAngle = std::clamp(
        std::isfinite(halfAngleDegrees) ? halfAngleDegrees : 0.0f, 0.0f, 89.9f);
    const float safeFalloff = std::clamp(std::isfinite(falloff) ? falloff : 0.0f, 0.0f, 1.0f);
    const float outerRadians = safeAngle * std::numbers::pi_v<float> / 180.0f;
    const float innerRadians = outerRadians * (1.0f - safeFalloff);
    return {std::cos(innerRadians), std::cos(outerRadians)};
}

// Smoothly transitions between the outer and inner cones. The equality case
// is the documented hard-edge falloff=0 mode and avoids smoothstep's equal
// endpoints being undefined in GLSL.
[[nodiscard]] inline float spotConeAttenuationAlg(float directionCosine,
                                                   SpotConeParametersAlg cone)
{
    if (!std::isfinite(directionCosine) || directionCosine <= cone.outerCos) return 0.0f;
    if (cone.innerCos <= cone.outerCos + 1e-5f) return 1.0f;
    const float t = std::clamp((directionCosine - cone.outerCos) /
                               (cone.innerCos - cone.outerCos), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace MeshCraft
