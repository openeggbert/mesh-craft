// SYS-W11-05 — genuine libFuzzer corpus-driven harness over Mc3XmlParser.
//
// This is the real deep-fuzzing complement to xml_mutation_fuzz_test.cpp's
// fast, bounded, CTest-registered seeded-mutation harness. It is NOT part of
// the default per-commit build: it requires Clang specifically (libFuzzer is
// a Clang/LLVM feature; the default cmake-build-debug tree in this
// environment uses GCC) and is intended for longer, occasional/manual or
// CI-scheduled runs, not a fast per-commit gate.
//
// Build (standalone mc3, so this never touches ../cna or the rest of the
// MeshCraft tree -- mc3/ is independently configurable, matching the CI
// matrix in .github_/workflows/ci.yml which already builds mc3 standalone):
//
//   cmake -S mc3 -B /tmp/mc3-fuzz-build -G Ninja \
//         -DCMAKE_CXX_COMPILER=clang++ -DMESHCRAFT_FUZZ=ON
//   cmake --build /tmp/mc3-fuzz-build --target mc3_xml_libfuzzer
//   /tmp/mc3-fuzz-build/mc3_xml_libfuzzer -max_total_time=60 -seed=1
//
// (add a corpus directory argument to seed/persist findings across runs;
// omitted here since none is checked in -- see the option() help text in
// mc3/CMakeLists.txt for why this stays opt-in rather than committing a
// corpus directory).

#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Mirrors what an AI-generated/imported/pasted document goes through:
    // the untrusted() policy, no filesystem access, in-memory text only.
    std::string xml(reinterpret_cast<const char*>(data), size);
    try {
        MeshCraft::Mc3::Mc3Document doc =
            MeshCraft::Mc3::Mc3Document::loadFromString(xml, {}, MeshCraft::Mc3::Mc3LoadPolicy::untrusted());
        (void)doc;
    } catch (const std::exception&) {
        // Expected outcome for the overwhelming majority of fuzzer-generated
        // inputs -- a clean, typed rejection is success, not a finding.
    }
    return 0;
}
