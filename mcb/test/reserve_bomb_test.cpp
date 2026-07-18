// AUD-066: McbReader.cpp's per-collection count is validated against
// kMcbMaxCollectionCount (10M, rU32Bounded) BEFORE being used to .reserve()
// a vector -- but that only proves the claimed count is under the sanity
// ceiling, not that the stream actually contains that many elements. Every
// .reserve(n) call site used to reserve the FULL claimed count up front, so
// a small, corrupted/malicious file (valid header/fields up to one
// oversized-but-legal collection count, then EOF) could force a near-
// gigabyte transient allocation before a single element was read or
// validated -- e.g. a claimed 9,000,000-entry `lights` array (~100 bytes
// per Mc3Light) requests ~900MB, for a file that is itself only a few
// hundred bytes on disk.
//
// This proves the fix's actual effect empirically, the same way AUD-064's
// fix was verified by real before/after timing rather than by inspecting
// the code alone: a hand-crafted document with a legit `lights` array is
// serialized, its length-prefixed "lights" key + TAG_ARR + u32 count are
// located by exact byte pattern (not guessed), the count is patched to a
// huge-but-under-the-sanity-ceiling value, and the stream is truncated
// immediately after -- so the file stays tiny while claiming millions of
// elements.
//
// Measured metric: VIRTUAL memory footprint (/proc/self/status VmPeak),
// not RSS. Empirically confirmed while writing this test: a bare
// `vector<Mc3Light>::reserve(9000000)` jumps VmPeak from ~6MB to ~850MB
// but leaves VmRSS essentially flat (~3.5MB) -- reserve() only asks the
// allocator for address space, it never constructs/touches the elements,
// so Linux's lazy page commit means the pages are never physically
// resident and getrusage()'s RSS-based ru_maxrss would NOT have shown
// this bug at all. VmPeak is measured because it tracks the high-water
// mark even after the oversized allocation is freed by the exception
// unwinding the vector's destructor.

#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mcb;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

#ifdef __linux__
#define MCB_HAVE_VMPEAK 1
// Parses "VmPeak:\t   123456 kB" from /proc/self/status. Returns -1 if
// unavailable (never expected on Linux, but fail open rather than crash).
static long vmPeakKb() {
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long result = -1;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "VmPeak:", 7) == 0) {
            result = std::strtol(line + 7, nullptr, 10);
            break;
        }
    }
    std::fclose(f);
    return result;
}
#endif

int main() {
    // A minimal document whose only notable collection is `lights` (one
    // entry) -- keeps the "lights" key's byte pattern unambiguous in the
    // serialized stream.
    Mc3Document doc;
    doc.model = "ReserveBomb";
    doc.lights.push_back(Mc3Light::directional("sun"));

    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    std::string bytes = out.str();
    CHECK(!bytes.empty(), "baseline document serializes to a non-empty MCB stream");

    // wKey() writes a 1-byte length prefix then the raw key bytes (no null
    // terminator); wKeyArr() then writes TAG_ARR (1 byte) then the u32
    // count (4 bytes, little-endian, per wU32/McbWriter.cpp). So the exact
    // byte pattern "\x06lights" (length-prefix 0x06 + the 6 ASCII bytes)
    // is immediately followed by [TAG_ARR][count_lo][count_1][count_2][count_hi].
    const std::string keyPattern = std::string("\x06", 1) + "lights";
    size_t pos = bytes.find(keyPattern);
    CHECK(pos != std::string::npos, "located the 'lights' key's exact byte pattern in the stream");
    CHECK(pos != std::string::npos && bytes.find(keyPattern, pos + 1) == std::string::npos,
          "the 'lights' key pattern is unambiguous (appears exactly once)");
    if (pos == std::string::npos) {
        std::cerr << failures << " reserve-bomb test(s) FAILED (setup).\n";
        return 1;
    }

    const size_t countOffset = pos + keyPattern.size() + 1; // +1 for TAG_ARR
    CHECK(countOffset + 4 <= bytes.size(), "count field fits within the serialized stream");

    // Sanity: the count currently there must read back as 1 (one light),
    // proving countOffset was computed correctly before we start patching.
    uint32_t originalCount;
    std::memcpy(&originalCount, bytes.data() + countOffset, 4);
    CHECK(originalCount == 1, "the located count field reads back as 1, confirming the offset is correct");

    // Patch to a huge-but-legal (< kMcbMaxCollectionCount == 10,000,000)
    // claimed count, matching the exact class of value rU32Bounded's own
    // ceiling was designed to still accept.
    const uint32_t hostileCount = 9'000'000;
    std::memcpy(bytes.data() + countOffset, &hostileCount, 4);

    // Truncate immediately after the patched count -- no actual light data
    // follows, so the file stays tiny (a few hundred bytes) while claiming
    // millions of elements. This is the "small malicious/corrupted file"
    // shape, not a large one.
    bytes.resize(countOffset + 4);
    CHECK(bytes.size() < 1024,
          "the hostile file stays small (" + std::to_string(bytes.size()) +
          " bytes) despite claiming " + std::to_string(hostileCount) + " lights");

#ifdef MCB_HAVE_VMPEAK
    long vmPeakBefore = vmPeakKb();
#endif

    bool threw = false;
    std::string what;
    try {
        std::istringstream in(bytes, std::ios::binary);
        Mc3Document rt = loadFromBinary(in);
        (void)rt;
    } catch (const std::exception& e) {
        threw = true;
        what = e.what();
    }
    CHECK(threw, "loading the truncated hostile-count stream throws (no data follows the claimed "
          "9,000,000 lights) rather than reading garbage");
    CHECK(!threw || what.find("truncat") != std::string::npos ||
          what.find("MCB") != std::string::npos,
          "the rejection is a clean MCB/truncation error, not an unrelated failure: " + what);

#ifdef MCB_HAVE_VMPEAK
    long vmPeakAfter = vmPeakKb();
    CHECK(vmPeakBefore > 0 && vmPeakAfter > 0, "VmPeak readable from /proc/self/status");
    long deltaKb = vmPeakAfter - vmPeakBefore;
    // Before the fix, .reserve(9000000) on vector<Mc3Light> (~100 bytes/elem)
    // requested roughly 850MB of address space up front (empirically
    // measured, see the file header comment). After the fix, the up-front
    // reserve is capped to kMcbReserveHint (4096) regardless of the claimed
    // count, so the genuine allocation stays on the order of a few hundred
    // KB. 64MB leaves an enormous margin above any legitimate cost of
    // parsing a <1KB stream while still being more than an order of
    // magnitude below the ~850MB the unpatched code would have requested.
    const long kMaxAcceptableDeltaKb = 64 * 1024;
    CHECK(deltaKb < kMaxAcceptableDeltaKb,
          "VmPeak grew by only " + std::to_string(deltaKb) + "KB while rejecting a claimed "
          "9,000,000-element collection (< " + std::to_string(kMaxAcceptableDeltaKb) +
          "KB budget; an unbounded up-front reserve would have requested ~850MB)");
#else
    pass("VmPeak-based virtual-memory check skipped (Linux-only /proc/self/status; not "
         "available on this platform) -- the throws-cleanly assertions above still cover correctness");
#endif

    // A legitimate, non-hostile document must still load correctly --
    // the fix must not be a false-positive trap on real files.
    {
        std::ostringstream out2(std::ios::binary);
        Mc3Document ok;
        ok.model = "StillFine";
        for (int i = 0; i < 50; ++i)
            ok.lights.push_back(Mc3Light::point("l" + std::to_string(i), {0, 1, 0}));
        saveToBinary(ok, out2);
        std::istringstream in2(out2.str(), std::ios::binary);
        bool threw2 = false;
        Mc3Document rt2;
        try { rt2 = loadFromBinary(in2); } catch (const std::exception&) { threw2 = true; }
        CHECK(!threw2, "a legitimate 50-light document still loads without hitting the reserve hint");
        CHECK(!threw2 && rt2.lights.size() == 50,
              "all 50 lights are present after a successful load (reserve hint doesn't truncate real data)");
    }

    if (failures == 0) { std::cout << "All reserve-bomb tests passed.\n"; return 0; }
    std::cerr << failures << " reserve-bomb test(s) FAILED.\n";
    return 1;
}
