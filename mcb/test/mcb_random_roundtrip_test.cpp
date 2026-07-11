// SYS-W11-05 — property-based (randomized, seeded) MCB binary round-trip
// test: the McbReader/McbWriter differential half of this task.
//
// mcb_roundtrip_test.cpp already covers a lot of round-trip ground with
// fixed, hand-picked scenarios. mcb_corruption_test.cpp covers "does the
// reader crash on garbage" (truncation sweep + random-byte fuzz). Neither
// answers "does a random VALID document survive MC3 -> MCB -> MC3 with its
// data intact" across a large, randomized sample -- that's what this file
// adds, reusing the SAME seeded random-document generator/comparator that
// mc3/test/random_roundtrip_test.cpp uses for the XML side (see
// RandomMc3DocumentGenerator.hpp's header comment for exactly what feature
// set it covers and why some features are deliberately excluded).
//
// Sharing the generator between mc3/test/ and mcb/test/ instead of
// duplicating it keeps the two round-trip properties (XML fidelity, MCB
// fidelity) checked against the literal same random inputs, which also
// means: if a future mismatch shows up in ONE of these but not the other,
// that itself is a meaningful signal about which codec has the bug.

#include "RandomMc3DocumentGenerator.hpp"

#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mcb;
using namespace MeshCraftTest;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

int main() {
    // Different fixed seed than mc3's random_roundtrip_test.cpp on purpose
    // (independent random document samples for the two codecs; both are
    // still fully reproducible given their own fixed seed).
    std::mt19937 rng(0xB1AB1Au);

    const int kNumDocuments = 60; // bounded so this stays a fast per-commit gate
    long long totalMismatches = 0;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < kNumDocuments; ++i) {
        int complexity = randInt(rng, 1, 8);
        Mc3Document doc = buildRandomDocument(rng, complexity);

        bool threwOnSave = false, threwOnLoad = false;
        Mc3Document reloaded;
        std::string bytes;
        try {
            std::ostringstream out(std::ios::binary);
            saveToBinary(doc, out);
            bytes = out.str();
        } catch (const std::exception& e) {
            threwOnSave = true;
            std::cerr << "  (saveToBinary threw: " << e.what() << ")\n";
        }
        if (!threwOnSave) {
            try {
                std::istringstream in(bytes, std::ios::binary);
                reloaded = loadFromBinary(in);
            } catch (const std::exception& e) {
                threwOnLoad = true;
                std::cerr << "  (loadFromBinary threw: " << e.what() << ")\n";
            }
        }

        CHECK(!threwOnSave, "random doc " + std::to_string(i) + " (complexity " +
              std::to_string(complexity) + "): saveToBinary does not throw on a "
              "well-formed randomly-generated document");
        CHECK(!threwOnLoad, "random doc " + std::to_string(i) +
              ": loadFromBinary does not throw reloading what was just saved");

        if (!threwOnSave && !threwOnLoad) {
            auto mismatches = compareDocuments(doc, reloaded);
            totalMismatches += static_cast<long long>(mismatches.size());
            CHECK(mismatches.empty(), "random doc " + std::to_string(i) +
                  ": semantically equivalent after MCB round-trip");
            for (const auto& m : mismatches) std::cerr << "    MISMATCH: " << m << "\n";
        }
    }

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    CHECK(true, "random MCB round-trip: " + std::to_string(kNumDocuments) +
          " random documents processed in " + std::to_string(elapsedMs) + "ms (" +
          std::to_string(totalMismatches) + " total field mismatches)");
    CHECK(elapsedMs < 30000, "random MCB round-trip: completes promptly");

    if (failures == 0)
        std::cout << "All MCB random round-trip tests passed.\n";
    else
        std::cerr << failures << " MCB random round-trip test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
