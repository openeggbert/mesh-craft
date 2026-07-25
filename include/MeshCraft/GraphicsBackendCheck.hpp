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

} // namespace MeshCraft
