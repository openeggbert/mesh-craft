// SYS-W11-05 — seeded mutation-based fuzz harness over Mc3XmlParser (via
// Mc3Document::loadFromString), the primary genuine gap this task exists to
// fill: mcb_corruption_test.cpp already does byte-offset truncation sweeps
// and random-byte fuzzing for the MCB *binary* reader, but before this file
// the MC3 *XML* parser had only targeted/hostile-value tests (finite_input_
// test.cpp, invalid_utf8_test.cpp, oversized_base64_test.cpp, ...), never a
// broad randomized/mutation fuzz pass.
//
// Two complementary techniques, both seeded (std::mt19937 with a fixed seed)
// so any failure is reproducible, matching this session's established
// convention (see mcb_corruption_test.cpp's "seeded random-byte fuzz"):
//
//  1. Pure random byte noise fed directly to loadFromString() as if it were
//     XML text (the mc3 analogue of mcb_corruption_test.cpp's
//     testRandomByteFuzzDoesNotCrash) -- cheap, but rarely gets past
//     tinyxml2's very first well-formedness check, so it mostly exercises
//     the "reject malformed XML cleanly" path.
//  2. AFL-style byte-level mutation ("havoc") of a small corpus of KNOWN-
//     VALID mc3.xml documents (built via the same random document generator
//     random_roundtrip_test.cpp uses, so the corpus itself is diverse) --
//     this is the technique that actually reaches deep parser code paths,
//     because a mutated-but-still-mostly-well-formed document exercises
//     attribute/element-specific parsing logic that pure noise essentially
//     never reaches (tinyxml2 rejects malformed markup before MeshCraft's
//     own per-field parsing code ever runs).
//
// This is a CTest-registered per-commit gate: bounded iteration count and
// wall-clock time (a few seconds), not an overnight fuzz campaign. It is a
// deliberate, honest substitute for a genuine libFuzzer/AFL corpus-driven
// harness in the DEFAULT build -- see mc3/test/fuzz/mc3_xml_libfuzzer.cpp
// (built only when MESHCRAFT_FUZZ=ON and the compiler is Clang) for the
// real corpus-driven equivalent, which this environment's toolchain DOES
// support (Debian clang 19.1.7 with -fsanitize=fuzzer) but which isn't part
// of the default per-commit build because it requires Clang specifically
// (the default cmake-build-debug tree uses GCC) and is meant for longer,
// occasional runs rather than a fast per-commit gate.
//
// Every trial asserts only ONE thing: loadFromString() either returns a
// document or throws a std::exception. If this test binary itself survives
// to print PASS/FAIL lines, no trial crashed/hung the process (a real
// segfault/abort/UBSan trap would kill the whole binary, and ctest would
// report the test as crashed, not FAILED).

#include "RandomMc3DocumentGenerator.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

using namespace MeshCraft::Mc3;
using namespace MeshCraftTest;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

static int tmpIdx = 0;
static std::string renderToXmlString(const Mc3Document& doc) {
    auto p = std::filesystem::temp_directory_path() /
             ("mc3_fuzz_seed_" + std::to_string(tmpIdx++) + ".mc3.xml");
    doc.saveToFile(p);
    std::ifstream in(p, std::ios::binary);
    std::string xml((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::filesystem::remove(p);
    return xml;
}

// Applies one random byte-level mutation to `s` in place. AFL-"havoc"-style:
// each call flips a bit, replaces/inserts/deletes a byte, or duplicates a
// chunk -- small, local, structure-agnostic edits that are cheap to apply
// many times per trial.
static void mutateOnce(std::string& s, std::mt19937& rng) {
    if (s.empty()) {
        s.push_back(static_cast<char>(std::uniform_int_distribution<int>(0, 255)(rng)));
        return;
    }
    std::uniform_int_distribution<int> opDist(0, 6);
    std::uniform_int_distribution<size_t> posDist(0, s.size() - 1);
    switch (opDist(rng)) {
        case 0: { // bit flip
            size_t pos = posDist(rng);
            int bit = std::uniform_int_distribution<int>(0, 7)(rng);
            s[pos] = static_cast<char>(static_cast<unsigned char>(s[pos]) ^ (1u << bit));
            break;
        }
        case 1: { // byte replace with random value
            s[posDist(rng)] = static_cast<char>(std::uniform_int_distribution<int>(0, 255)(rng));
            break;
        }
        case 2: { // byte insert
            size_t pos = posDist(rng);
            s.insert(s.begin() + static_cast<long>(pos),
                      static_cast<char>(std::uniform_int_distribution<int>(0, 255)(rng)));
            break;
        }
        case 3: { // byte delete
            size_t pos = posDist(rng);
            s.erase(s.begin() + static_cast<long>(pos));
            break;
        }
        case 4: { // insert an XML-special character (more likely to perturb parsing structure)
            static const char kSpecial[] = {'<', '>', '&', '"', '\'', '/', '='};
            size_t pos = posDist(rng);
            char c = kSpecial[std::uniform_int_distribution<size_t>(0, sizeof(kSpecial) - 1)(rng)];
            s.insert(s.begin() + static_cast<long>(pos), c);
            break;
        }
        case 5: { // duplicate a small chunk (grows the input, tests unbounded-ish structures)
            size_t pos = posDist(rng);
            size_t len = std::min(s.size() - pos, static_cast<size_t>(std::uniform_int_distribution<int>(1, 16)(rng)));
            std::string chunk = s.substr(pos, len);
            s.insert(pos, chunk);
            break;
        }
        default: { // truncate at a random point
            size_t pos = posDist(rng);
            s.resize(pos);
            break;
        }
    }
}

int main() {
    // Fixed seed: reproducible across runs/machines (this session's
    // convention -- see mcb_corruption_test.cpp).
    std::mt19937 rng(0xFACADE42u);

    // --- Technique 1: pure random byte noise as "XML" ----------------------
    {
        std::uniform_int_distribution<int> byteDist(0, 255);
        const std::vector<size_t> sizes = {0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 4096};
        const int trialsPerSize = 15;
        long long threwCount = 0, okCount = 0, total = 0;
        auto start = std::chrono::steady_clock::now();
        for (size_t sz : sizes) {
            for (int t = 0; t < trialsPerSize; ++t) {
                std::string buf(sz, '\0');
                for (auto& ch : buf) ch = static_cast<char>(byteDist(rng));
                try {
                    Mc3Document doc = Mc3Document::loadFromString(buf, {}, Mc3LoadPolicy::untrusted());
                    (void)doc;
                    ++okCount;
                } catch (const std::exception&) {
                    ++threwCount;
                }
                ++total;
            }
        }
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        CHECK(true, "random-byte-as-xml fuzz: " + std::to_string(total) +
              " buffers processed without a crash (" + std::to_string(threwCount) +
              " threw, " + std::to_string(okCount) + " happened to parse) in " +
              std::to_string(elapsedMs) + "ms");
        CHECK(threwCount == total, "random-byte-as-xml fuzz: every random buffer is "
              "rejected as malformed XML (none happened to be well-formed by chance)");
        CHECK(elapsedMs < 5000, "random-byte-as-xml fuzz: completes promptly");
    }

    // --- Technique 2: mutation ("havoc") fuzzing of a valid-document corpus ---
    {
        std::vector<std::string> corpus;
        for (int complexity : {1, 3, 6}) {
            Mc3Document doc = buildRandomDocument(rng, complexity);
            corpus.push_back(renderToXmlString(doc));
        }
        // A minimal hand-written seed too, so the corpus isn't ONLY
        // generator output (independent of any generator bias/blind spot).
        corpus.push_back(
            "<?xml version=\"1.0\"?>\n"
            "<mc3 version=\"0.3\" model=\"seed\">\n"
            "  <object id=\"a\" type=\"box\"><size x=\"1\" y=\"1\" z=\"1\"/></object>\n"
            "  <material name=\"m\" baseColor=\"1 0 0 1\" roughness=\"0.5\"/>\n"
            "</mc3>\n");

        CHECK(!corpus.empty() && corpus[0].size() > 50,
              "mutation fuzz: seed corpus is non-trivial (" + std::to_string(corpus.size()) +
              " seeds, first is " + std::to_string(corpus[0].size()) + " bytes)");

        std::uniform_int_distribution<size_t> corpusPick(0, corpus.size() - 1);
        std::uniform_int_distribution<int> mutationCount(1, 8);

        const int kTrials = 2500; // bounded: this is a per-commit gate, not an overnight campaign
        long long threwCount = 0, okCount = 0;
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kTrials; ++i) {
            std::string mutated = corpus[corpusPick(rng)];
            int nMutations = mutationCount(rng);
            for (int m = 0; m < nMutations; ++m) mutateOnce(mutated, rng);

            try {
                Mc3Document doc = Mc3Document::loadFromString(mutated, {}, Mc3LoadPolicy::untrusted());
                (void)doc;
                ++okCount;
            } catch (const std::exception&) {
                ++threwCount;
            }
        }
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        CHECK(true, "mutation fuzz: " + std::to_string(kTrials) +
              " mutated documents processed without a crash (" + std::to_string(threwCount) +
              " threw, " + std::to_string(okCount) + " still parsed) in " +
              std::to_string(elapsedMs) + "ms");
        // Unlike pure random noise, small mutations of a valid document
        // frequently stay well-formed enough to parse (e.g. a bit-flip
        // inside a numeric attribute, or a duplicated whitespace run) -- so,
        // unlike the random-byte case, a mix of throw/ok is EXPECTED and
        // healthy here; only the "no crash" property (implicit in reaching
        // this line at all) and a sane runtime are asserted.
        CHECK(okCount > 0, "mutation fuzz: at least some mutated documents remain "
              "well-formed enough to parse (proves mutations are reaching real "
              "parsing code, not just tripping tinyxml2's outermost well-formedness check)");
        CHECK(elapsedMs < 20000, "mutation fuzz: completes promptly (no pathological "
              "slow path hiding behind a mutated attribute/structure)");
    }

    if (failures == 0)
        std::cout << "All MC3 XML mutation fuzz tests passed.\n";
    else
        std::cerr << failures << " MC3 XML mutation fuzz test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
