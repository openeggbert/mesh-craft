#pragma once

// AUD-039b (Gate C): pure decision logic for whether the MeshCraft editor GUI
// should be allowed to proceed under a given CNA graphics backend. Separated
// from src/MeshCraft/main.cpp (which owns the env-var lookup and error
// messages) so it is headlessly unit-testable without a full alternate-
// backend rebuild -- see test/graphics_backend_check_test.cpp.
//
// The editor's ImGui UI is hard-wired to OpenGL/GLES3 (see
// src/MeshCraft/MeshCraftApplication.cpp's ImGui_ImplOpenGL3_Init /
// ImGui_ImplSDL3_InitForOpenGL) and only actually renders under EASYGL.

#include <string>

namespace MeshCraft {

// Returns true if `backend` should be allowed to proceed (EASYGL always is;
// any other backend only if `allowOverride` is set, e.g. from
// MESH_CRAFT_ALLOW_UNSUPPORTED_BACKEND).
inline bool isBackendSupportedAlg(const std::string& backend, bool allowOverride) {
    if (backend == "EASYGL") return true;
    return allowOverride;
}

} // namespace MeshCraft
