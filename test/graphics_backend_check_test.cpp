// Unit test for MeshCraft::isBackendSupportedAlg (AUD-039b, Gate C).
//
// The editor's ImGui UI only renders under EASYGL; every other CNA backend
// (SDL_RENDERER/BGFX/VULKAN) compiles but the UI never draws. Prior to this
// fix, selecting a non-EASYGL backend only produced a configure-time CMake
// warning that a headless/CI configure or a GUI CMake frontend could easily
// miss -- the binary still built and launched a broken window. This test
// covers the pure decision logic; a full end-to-end verification (actually
// building under SDL_RENDERER and confirming the binary refuses to launch)
// was performed manually -- see plan.md AUD-039b's status note for the exact
// commands and observed output, since a full alternate-backend rebuild is too
// slow to run as a routine CTest.

#include "MeshCraft/GraphicsBackendCheck.hpp"

#include <iostream>
#include <string>

using MeshCraft::isBackendSupportedAlg;

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

    for (const char* backend : {"SDL_RENDERER", "BGFX", "VULKAN"}) {
        check(!isBackendSupportedAlg(backend, /*allowOverride=*/false),
              std::string(backend) + " is rejected by default (no MESH_CRAFT_ALLOW_UNSUPPORTED_BACKEND)");
        check(isBackendSupportedAlg(backend, /*allowOverride=*/true),
              std::string(backend) + " is allowed when the override is explicitly set");
    }

    if (failures == 0) { std::cout << "All graphics-backend-check tests passed.\n"; return 0; }
    std::cerr << failures << " graphics-backend-check test(s) failed.\n";
    return 1;
}
