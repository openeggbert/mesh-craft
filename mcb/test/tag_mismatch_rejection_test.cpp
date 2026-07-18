// AUD-071: two known keys, Mc3SceneState's `overrides` and Mc3Trigger's
// `steps`, were read with `if (k == "overrides" && tag == TAG_ARR) { ... }
// else { skipValue(in, tag); }` -- unlike every other known key in this
// reader (which uses expectTag(), throwing a clear "type mismatch" error
// on a tag/decoder mismatch, per AUD-015's own established policy), a
// corrupted/malicious file with the RIGHT key name but the WRONG tag byte
// silently fell into the unknown-key fallback and had the field dropped
// -- no error, no diagnostic, just a valid-looking document quietly
// missing its scene-state overrides or trigger steps.
//
// Proves the fix by patching the tag byte immediately following each
// key's exact byte pattern (located precisely, not guessed -- same
// technique as reserve_bomb_test.cpp) from TAG_ARR to TAG_STR, and
// asserting the load now throws a clear type-mismatch error instead of
// silently succeeding with the field missing.

#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3SceneState.hpp"
#include "MeshCraft/Mc3/Mc3Trigger.hpp"

#include <cstdint>
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

namespace {
constexpr uint8_t TAG_STR = 0x04;

// Locates `\x<len>key` (wKey()'s exact 1-byte-length-prefix + raw bytes
// format) and returns the byte offset of the TAG byte that immediately
// follows (key pattern + 1 byte for the TAG_ARR wKeyArr() itself wrote).
// Asserts the pattern appears exactly once, so patching it can't
// ambiguously hit the wrong occurrence.
size_t findTagOffset(std::string& bytes, const std::string& key, bool& ok) {
    const std::string pattern = std::string(1, static_cast<char>(key.size())) + key;
    size_t pos = bytes.find(pattern);
    ok = (pos != std::string::npos) && (bytes.find(pattern, pos + 1) == std::string::npos);
    return pos + pattern.size();
}
} // namespace

static void testOverridesTagMismatch() {
    Mc3Document doc;
    doc.model = "TagMismatchOverrides";
    Mc3SceneState state;
    state.name = "night";
    Mc3ObjectOverride ovr;
    ovr.id = "lamp";
    ovr.visible = false;
    state.overrides.push_back(ovr);
    doc.sceneStates[state.name] = state;

    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    std::string bytes = out.str();

    bool located = false;
    size_t tagOffset = findTagOffset(bytes, "overrides", located);
    CHECK(located, "located the 'overrides' key's exact byte pattern unambiguously");
    if (!located) return;
    CHECK(tagOffset < bytes.size(), "tag byte offset is within the stream");

    // Sanity: confirm it currently reads as TAG_ARR (0x08, per McbFormat.hpp)
    // before patching, proving the offset is correct.
    CHECK(static_cast<uint8_t>(bytes[tagOffset]) == 0x08,
          "the located byte is TAG_ARR before patching, confirming the offset is correct");

    bytes[tagOffset] = static_cast<char>(TAG_STR);

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
    CHECK(threw, "loading a file with 'overrides' key but a mismatched (TAG_STR) tag throws "
          "instead of silently dropping the field");
    CHECK(!threw || (what.find("overrides") != std::string::npos &&
                      what.find("type mismatch") != std::string::npos),
          "the rejection clearly names the field and the mismatch, not a generic failure: " + what);
}

static void testStepsTagMismatch() {
    Mc3Document doc;
    doc.model = "TagMismatchSteps";
    Mc3Trigger trig;
    trig.id = "door";
    trig.steps.push_back({TriggerStepType::PlaySound, "boom"});
    doc.triggers[trig.id] = trig;

    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    std::string bytes = out.str();

    bool located = false;
    size_t tagOffset = findTagOffset(bytes, "steps", located);
    CHECK(located, "located the 'steps' key's exact byte pattern unambiguously");
    if (!located) return;
    CHECK(tagOffset < bytes.size(), "tag byte offset is within the stream");
    CHECK(static_cast<uint8_t>(bytes[tagOffset]) == 0x08,
          "the located byte is TAG_ARR before patching, confirming the offset is correct");

    bytes[tagOffset] = static_cast<char>(TAG_STR);

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
    CHECK(threw, "loading a file with 'steps' key but a mismatched (TAG_STR) tag throws "
          "instead of silently dropping the field");
    CHECK(!threw || (what.find("steps") != std::string::npos &&
                      what.find("type mismatch") != std::string::npos),
          "the rejection clearly names the field and the mismatch, not a generic failure: " + what);
}

static void testLegitimateDocumentsStillLoad() {
    // The fix must not be a false-positive trap on well-formed files.
    Mc3Document doc;
    doc.model = "StillFine";
    Mc3SceneState state;
    state.name = "day";
    Mc3ObjectOverride ovr;
    ovr.id = "sun";
    ovr.visible = true;
    state.overrides.push_back(ovr);
    doc.sceneStates[state.name] = state;

    Mc3Trigger trig;
    trig.id = "gate";
    trig.steps.push_back({TriggerStepType::RunScript, "onOpen"});
    doc.triggers[trig.id] = trig;

    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    std::istringstream in(out.str(), std::ios::binary);
    bool threw = false;
    Mc3Document rt;
    try { rt = loadFromBinary(in); } catch (const std::exception&) { threw = true; }
    CHECK(!threw, "a legitimate document with both overrides and steps still loads cleanly");
    CHECK(!threw && rt.sceneStates.count("day") == 1 &&
          rt.sceneStates.at("day").overrides.size() == 1,
          "scene state overrides survive a correct round-trip");
    CHECK(!threw && rt.triggers.count("gate") == 1 &&
          rt.triggers.at("gate").steps.size() == 1,
          "trigger steps survive a correct round-trip");
}

int main() {
    testOverridesTagMismatch();
    testStepsTagMismatch();
    testLegitimateDocumentsStillLoad();

    if (failures == 0) { std::cout << "All tag-mismatch-rejection tests passed.\n"; return 0; }
    std::cerr << failures << " tag-mismatch-rejection test(s) FAILED.\n";
    return 1;
}
