#pragma once

// AUD-039b (Gate C): pure decision logic for whether the MeshCraft editor GUI
// should be allowed to proceed under a given CNA graphics backend. Separated
// from src/MeshCraft/main.cpp (which owns the env-var lookup and error
// messages) so it is headlessly unit-testable without a full alternate-
// backend rebuild -- see test/graphics_backend_check_test.cpp.
//
// The editor's ImGui renderer uses CNA rather than a native graphics API.

#include <string>

namespace MeshCraft {

// EASYGL is fully qualified. Vulkan is explicitly enabled for the current
// manual SYS-W8-05 qualification run; the remaining CNA backends stay closed
// until they receive their own real editor screenshot qualification.
inline bool isBackendSupportedAlg(const std::string& backend, bool /*allowOverride*/) {
    return backend == "EASYGL" || backend == "VULKAN";
}

// MeshCraft's optional post-processes and material-preview currently supply
// source GLSL to CNA ShaderEffect.  EASYGL compiles that source, but CNA's
// Vulkan backend currently accepts only a narrow precompiled-SPIR-V
// SpriteBatch path; it does not yet expose the named-uniform and 3D-pipeline
// contract these effects require.  Keep this decision separate from the
// editor gate: Vulkan can render the CNA editor and ImGui, just not these
// optional source-shader features.
inline bool supportsTextShaderEffectsAlg(const std::string& backend) {
    return backend == "EASYGL";
}

// The selected backend is embedded privately into the MeshCraft executable by
// CMake.  The fallback keeps this header useful for standalone algorithm
// tests and non-CMake consumers, where the established default is EASYGL.
inline bool supportsTextShaderEffects() {
#ifdef MESH_CRAFT_GRAPHICS_BACKEND_STR
    return supportsTextShaderEffectsAlg(MESH_CRAFT_GRAPHICS_BACKEND_STR);
#else
    return true;
#endif
}

} // namespace MeshCraft
