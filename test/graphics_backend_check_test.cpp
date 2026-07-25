// Unit test for MeshCraft::isBackendSupportedAlg (AUD-039b, Gate C).
//
// The CNA ImGui renderer has no native-GL dependency, but only EASYGL has
// passed the required real editor screenshot qualification so far.

#include "MeshCraft/GraphicsBackendCheck.hpp"

#include <iostream>
#include <string>

using MeshCraft::isBackendSupportedAlg;
using MeshCraft::supportsTextShaderEffectsAlg;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

int main() {
    check(isBackendSupportedAlg("EASYGL", /*allowOverride=*/false),
          "EASYGL is always supported, no override needed");
    check(isBackendSupportedAlg("EASYGL", /*allowOverride=*/true),
          "EASYGL is supported even with override set (no-op)");
    check(isBackendSupportedAlg("VULKAN", /*allowOverride=*/false),
          "VULKAN is enabled for the explicit manual qualification run");
    check(isBackendSupportedAlg("VULKAN", /*allowOverride=*/true),
          "VULKAN does not require the legacy override argument");

    for (const char* backend : {"SDL_RENDERER", "BGFX", "WEBGPU"}) {
        check(!isBackendSupportedAlg(backend, /*allowOverride=*/false),
              std::string(backend) + " stays gated until screenshot qualification");
        check(!isBackendSupportedAlg(backend, /*allowOverride=*/true),
              std::string(backend) + " cannot bypass qualification with the legacy override argument");
    }
    check(!isBackendSupportedAlg("UNKNOWN", false), "unknown backend is rejected");

    check(supportsTextShaderEffectsAlg("EASYGL"),
          "EASYGL supports MeshCraft's source-GLSL ShaderEffects");
    for (const char* backend : {"VULKAN", "SDL_RENDERER", "BGFX", "WEBGPU"}) {
        check(!supportsTextShaderEffectsAlg(backend),
              std::string(backend) + " does not claim unsupported source-GLSL ShaderEffects");
    }

    if (failures == 0) { std::cout << "All graphics-backend-check tests passed.\n"; return 0; }
    std::cerr << failures << " graphics-backend-check test(s) failed.\n";
    return 1;
}
