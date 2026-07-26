// SYS-W14-33 -- bounded point/spot preview decisions and attenuation rules.

#include <MeshCraft/GraphicsBackendCheck.hpp>
#include <MeshCraft/PointSpotLightingAlgorithms.hpp>

#include <cmath>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) std::cout << "PASS: " << message << '\n';
    else { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

bool near(float actual, float expected) {
    return std::fabs(actual - expected) < 1e-5f;
}

} // namespace

int main() {
    using namespace MeshCraft;

    check(pointSpotShaderEnabledAlg(true, true, 1),
          "EASYGL-style source ShaderEffect path activates for authored punctual lights");
    check(!pointSpotShaderEnabledAlg(false, true, 1),
          "unsupported backend keeps the labelled BasicEffect/gizmo fallback");
    check(!pointSpotShaderEnabledAlg(true, false, 1),
          "failed ShaderEffect compilation keeps the fallback");
    check(!pointSpotShaderEnabledAlg(true, true, 0),
          "scenes without point or spot lights retain the BasicEffect path");
    check(!supportsTextShaderEffectsAlg("VULKAN"),
          "Vulkan does not claim the source-GLSL point/spot preview contract");

    check(near(punctualRangeAttenuationAlg(2.0f, 0.0f), 0.25f),
          "unlimited point light uses inverse-square attenuation");
    check(punctualRangeAttenuationAlg(1.0f, 5.0f) >
              punctualRangeAttenuationAlg(2.0f, 5.0f),
          "point light attenuates monotonically with distance inside range");
    check(near(punctualRangeAttenuationAlg(5.0f, 5.0f), 0.0f) &&
              near(punctualRangeAttenuationAlg(6.0f, 5.0f), 0.0f),
          "point light contributes nothing at or beyond its authored range");

    const SpotConeParametersAlg softCone = spotConeParametersAlg(30.0f, 0.5f);
    check(near(spotConeAttenuationAlg(1.0f, softCone), 1.0f),
          "spotlight axis is fully illuminated");
    check(near(spotConeAttenuationAlg(softCone.outerCos, softCone), 0.0f) &&
              near(spotConeAttenuationAlg(softCone.outerCos - 0.01f, softCone), 0.0f),
          "spotlight outer-cone boundary rejects outside fragments");
    const float middleCos = (softCone.innerCos + softCone.outerCos) * 0.5f;
    const float middle = spotConeAttenuationAlg(middleCos, softCone);
    check(middle > 0.0f && middle < 1.0f,
          "spotlight falloff smoothly interpolates between inner and outer cone");

    const SpotConeParametersAlg hardCone = spotConeParametersAlg(30.0f, 0.0f);
    check(near(hardCone.innerCos, hardCone.outerCos) &&
              near(spotConeAttenuationAlg(1.0f, hardCone), 1.0f),
          "falloff=0 retains the documented hard spotlight edge");

    if (failures == 0) std::cout << "All point/spot lighting algorithm tests passed.\n";
    return failures == 0 ? 0 : 1;
}
