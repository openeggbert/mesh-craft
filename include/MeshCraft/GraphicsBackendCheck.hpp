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

// The renderer has no native-GL dependency, but a backend becomes launchable
// only after SYS-W8-05's real screenshot qualification. EASYGL is qualified
// now; the other CNA backends stay truthfully rejected until then.
inline bool isBackendSupportedAlg(const std::string& backend, bool /*allowOverride*/) {
    return backend == "EASYGL";
}

} // namespace MeshCraft
