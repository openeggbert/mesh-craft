// SYS-W11-05 — genuine libFuzzer corpus-driven harness over McbReader.
//
// Real deep-fuzzing complement to mcb_corruption_test.cpp's fast, bounded,
// CTest-registered truncation-sweep/random-byte harness. Not part of the
// default per-commit build -- requires Clang (libFuzzer is a Clang/LLVM
// feature; the default cmake-build-debug tree uses GCC). Intended for
// longer, occasional/manual or CI-scheduled runs.
//
// Build (standalone mcb -- mcb/ is independently configurable, matching the
// CI matrix in .github_/workflows/ci.yml which already builds mcb
// standalone; this never touches ../cna or the rest of the MeshCraft tree):
//
//   cmake -S mcb -B /tmp/mcb-fuzz-build -G Ninja \
//         -DCMAKE_CXX_COMPILER=clang++ -DMESHCRAFT_FUZZ=ON
//   cmake --build /tmp/mcb-fuzz-build --target mcb_libfuzzer
//   /tmp/mcb-fuzz-build/mcb_libfuzzer -max_total_time=60 -seed=1

#include "MeshCraft/Mcb/McbReader.hpp"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string bytes(reinterpret_cast<const char*>(data), size);
    std::istringstream in(bytes, std::ios::binary);
    try {
        MeshCraft::Mc3::Mc3Document doc = MeshCraft::Mcb::loadFromBinary(in);
        (void)doc;
    } catch (const std::exception&) {
        // Expected outcome for the overwhelming majority of fuzzer-generated
        // inputs -- a clean, typed rejection is success, not a finding.
    }
    return 0;
}
