// SYS-W11-05 — property-based (randomized, seeded) MC3 XML round-trip test.
//
// mc3_roundtrip_test.cpp already covers a lot of round-trip ground, but every
// case there is a fixed, hand-picked scenario -- one feature (or a small
// fixed combination) per test function, with fixed parameter values. This
// file instead builds a LARGE NUMBER of RANDOM documents (seeded RNG, so
// failures are reproducible) combining many features with random parameter
// values in a single pass, then round-trips each through the real codec path
// (Mc3Document::saveToFile -> Mc3Document::loadFromFile, i.e. Mc3XmlWriter
// then Mc3XmlParser) and asserts the reloaded document is semantically
// equivalent to the original -- a differential/property-based check ("does
// round-tripping silently corrupt data") rather than a crash/hang check
// ("fuzzing" in the narrower sense; that's covered separately by
// xml_mutation_fuzz_test.cpp in this same directory).
//
// See RandomMc3DocumentGenerator.hpp (this directory) for exactly what
// features the generator covers and why some (Extrude, Instance/definitions,
// SceneState/Trigger/Script/Sound/Music, SVG texture, embed) are
// deliberately excluded from THIS generator (they already have deep, fixed
// coverage elsewhere and their round-trip semantics are subtle enough that
// randomizing them here risks false-positive failures from the generator's
// own misunderstanding rather than genuine bugs).

#include "RandomMc3DocumentGenerator.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraftTest;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

static int tmpIdx = 0;
static std::filesystem::path tmpPath() {
    return std::filesystem::temp_directory_path() /
           ("mc3_random_rt_" + std::to_string(tmpIdx++) + ".mc3.xml");
}

int main() {
    // Fixed seed: reproducible across runs/machines, matching this session's
    // established convention (see mcb_corruption_test.cpp's random-byte fuzz).
    std::mt19937 rng(0x5EED1234u);

    const int kNumDocuments = 60;   // bounded so this stays a fast per-commit gate
    long long totalMismatches = 0;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < kNumDocuments; ++i) {
        int complexity = randInt(rng, 1, 8);
        Mc3Document doc = buildRandomDocument(rng, complexity);

        auto p = tmpPath();
        bool threwOnSave = false, threwOnLoad = false;
        Mc3Document reloaded;
        try {
            doc.saveToFile(p);
        } catch (const std::exception& e) {
            threwOnSave = true;
            std::cerr << "  (save threw: " << e.what() << ")\n";
        }
        if (!threwOnSave) {
            try {
                reloaded = Mc3Document::loadFromFile(p);
            } catch (const std::exception& e) {
                threwOnLoad = true;
                std::cerr << "  (load threw: " << e.what() << ")\n";
            }
        }
        std::filesystem::remove(p);

        CHECK(!threwOnSave, "random doc " + std::to_string(i) + " (complexity " +
              std::to_string(complexity) + "): saveToFile does not throw on a "
              "well-formed randomly-generated document");
        CHECK(!threwOnLoad, "random doc " + std::to_string(i) + ": loadFromFile "
              "does not throw reloading what was just saved");

        if (!threwOnSave && !threwOnLoad) {
            auto mismatches = compareDocuments(doc, reloaded);
            totalMismatches += static_cast<long long>(mismatches.size());
            CHECK(mismatches.empty(), "random doc " + std::to_string(i) +
                  ": semantically equivalent after XML round-trip");
            if (!mismatches.empty()) {
                for (const auto& m : mismatches) std::cerr << "    MISMATCH: " << m << "\n";
            }
        }
    }

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    CHECK(true, "random round-trip: " + std::to_string(kNumDocuments) +
          " random documents processed in " + std::to_string(elapsedMs) + "ms (" +
          std::to_string(totalMismatches) + " total field mismatches)");
    CHECK(elapsedMs < 30000, "random round-trip: completes promptly");

    // Also exercise the in-memory loadFromString() path (no temp file) for a
    // handful of the same random documents, via a second independent RNG
    // stream seeded from a different constant -- proves the string-based
    // entry point (the one an AI/import pipeline actually calls, per
    // Mc3Document::loadFromString's own doc comment) has the same
    // round-trip fidelity as the file-based path, not just a parallel
    // untested code path.
    {
        std::mt19937 rng2(0x5EED5678u);
        const int kNumStringDocs = 15;
        for (int i = 0; i < kNumStringDocs; ++i) {
            Mc3Document doc = buildRandomDocument(rng2, randInt(rng2, 1, 5));
            auto p = tmpPath();
            doc.saveToFile(p);
            std::ifstream in(p, std::ios::binary);
            std::string xml((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            in.close();
            std::filesystem::remove(p);

            bool threw = false;
            Mc3Document reloaded;
            try {
                reloaded = Mc3Document::loadFromString(xml, {}, Mc3LoadPolicy::trusted());
            } catch (const std::exception& e) {
                threw = true;
                std::cerr << "  (loadFromString threw: " << e.what() << ")\n";
            }
            CHECK(!threw, "random doc (string path) " + std::to_string(i) +
                  ": loadFromString does not throw on a well-formed document");
            if (!threw) {
                auto mismatches = compareDocuments(doc, reloaded);
                CHECK(mismatches.empty(), "random doc (string path) " + std::to_string(i) +
                      ": semantically equivalent via loadFromString round-trip");
                for (const auto& m : mismatches) std::cerr << "    MISMATCH: " << m << "\n";
            }
        }
    }

    if (failures == 0)
        std::cout << "All MC3 random round-trip tests passed.\n";
    else
        std::cerr << failures << " MC3 random round-trip test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
