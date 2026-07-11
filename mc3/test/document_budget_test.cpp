// SYS-W1-03: document-complexity budgets BEYOND AUD-059's total object
// count + total tessellation weight (input_budget_test.cpp covers those).
//
// Each sub-test constructs a document that exceeds exactly ONE new budget
// dimension while staying comfortably under every other budget (including
// the pre-existing kMaxTotalObjects/kMaxTotalTessellationWeight/
// kMaxTotalIncludes), then confirms the rejection specifically names that
// dimension -- proving the new check is what actually caught it, not an
// unrelated budget tripping first.
//
// Grown incrementally, one dimension per commit -- see this session's
// plan.md entry for SYS-W1-03 for exactly which dimensions are covered.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

static Mc3Document load(const std::string& xml) {
    auto path = std::filesystem::temp_directory_path() / "mc3_document_budget_test.mc3.xml";
    { std::ofstream f(path); f << xml; }
    Mc3Document doc = Mc3Document::loadFromFile(path);
    std::filesystem::remove(path);
    return doc;
}

// Loads `xml`, expecting it to throw. Returns the exception message (empty
// if it didn't throw, which the caller should treat as a failure).
static std::string loadExpectingThrow(const std::string& xml) {
    auto path = std::filesystem::temp_directory_path() / "mc3_document_budget_test.mc3.xml";
    { std::ofstream f(path); f << xml; }
    std::string what;
    try {
        Mc3Document::loadFromFile(path);
    } catch (const std::exception& e) {
        what = e.what();
    }
    std::filesystem::remove(path);
    return what;
}

// ---------------------------------------------------------------------------
// Materials: kMaxTotalMaterials = 20,000.
// ---------------------------------------------------------------------------
static void testMaterialBudget() {
    // Comfortably over the material budget, using trivial materials so no
    // other budget (objects, tessellation) is anywhere close to its ceiling.
    std::string xml = "<mc3 version=\"0.3\" model=\"mat-budget\">\n  <materials>\n";
    for (int i = 0; i < 20'001; ++i)
        xml += "    <material id=\"m" + std::to_string(i) + "\"/>\n";
    xml += "  </materials>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(), "20,001 materials exceeds the material budget and is rejected");
    check(what.find("material") != std::string::npos,
          "rejection names the material budget, not an unrelated failure: " + what);

    // A comfortably-under-budget document must still load fine.
    std::string okXml = "<mc3 version=\"0.3\" model=\"mat-ok\">\n  <materials>\n";
    for (int i = 0; i < 500; ++i)
        okXml += "    <material id=\"m" + std::to_string(i) + "\"/>\n";
    okXml += "  </materials>\n</mc3>\n";
    Mc3Document doc = load(okXml);
    check(doc.materials.size() == 500, "500 materials (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Textures: kMaxTotalTextures = 20,000 (regular + svg combined).
// ---------------------------------------------------------------------------
static void testTextureBudget() {
    std::string xml = "<mc3 version=\"0.3\" model=\"tex-budget\">\n  <textures>\n";
    for (int i = 0; i < 20'001; ++i)
        xml += "    <texture id=\"t" + std::to_string(i) + "\" uri=\"t" +
               std::to_string(i) + ".png\"/>\n";
    xml += "  </textures>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(), "20,001 textures exceeds the texture budget and is rejected");
    check(what.find("texture") != std::string::npos,
          "rejection names the texture budget, not an unrelated failure: " + what);

    std::string okXml = "<mc3 version=\"0.3\" model=\"tex-ok\">\n  <textures>\n";
    for (int i = 0; i < 500; ++i)
        okXml += "    <texture id=\"t" + std::to_string(i) + "\" uri=\"t" +
                 std::to_string(i) + ".png\"/>\n";
    okXml += "  </textures>\n</mc3>\n";
    Mc3Document doc = load(okXml);
    check(doc.textures.size() == 500, "500 textures (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Embeds: kMaxTotalEmbeds = 1,000 (count) and kMaxTotalEmbedBytes = 256MB
// (sum of every embed's base64Content.size(), independent of the existing
// per-embed 64MB kMaxEmbedBase64Length ceiling).
// ---------------------------------------------------------------------------
static void testEmbedCountBudget() {
    // 1,001 trivial (near-empty base64) embeds: exceeds the COUNT budget
    // while staying nowhere near the aggregate-bytes budget.
    std::string xml = "<mc3 version=\"0.3\" model=\"embed-count-budget\">\n  <embeds>\n";
    for (int i = 0; i < 1'001; ++i)
        xml += "    <embed id=\"e" + std::to_string(i) + "\" type=\"gltf\">AA==</embed>\n";
    xml += "  </embeds>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(), "1,001 trivial embeds exceeds the embed COUNT budget and is rejected");
    check(what.find("embed") != std::string::npos,
          "rejection names the embed budget, not an unrelated failure: " + what);

    std::string okXml = "<mc3 version=\"0.3\" model=\"embed-count-ok\">\n  <embeds>\n";
    for (int i = 0; i < 100; ++i)
        okXml += "    <embed id=\"e" + std::to_string(i) + "\" type=\"gltf\">AA==</embed>\n";
    okXml += "  </embeds>\n</mc3>\n";
    Mc3Document doc = load(okXml);
    check(doc.embeds.size() == 100, "100 trivial embeds (under budget) all load fine");
}

static void testEmbedAggregateBytesBudget() {
    // 5 embeds at 60MB of base64 each (each individually well under the
    // existing 64MB per-embed ceiling, and 5 is nowhere near the 1,000
    // embed COUNT budget) sum to 300MB, over the 256MB aggregate budget.
    const size_t perEmbed = 60ull * 1024ull * 1024ull;
    std::string bigBody(perEmbed, 'A');
    std::string xml = "<mc3 version=\"0.3\" model=\"embed-bytes-budget\">\n  <embeds>\n";
    for (int i = 0; i < 5; ++i)
        xml += "    <embed id=\"e" + std::to_string(i) + "\" type=\"gltf\">" + bigBody + "</embed>\n";
    xml += "  </embeds>\n</mc3>\n";
    bigBody.clear(); bigBody.shrink_to_fit();

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(),
          "5 embeds at 60MB each (300MB combined, each individually legal) "
          "exceeds the aggregate embed-bytes budget and is rejected");
    check(what.find("embed") != std::string::npos && what.find("byte") != std::string::npos,
          "rejection names the aggregate embed-bytes budget, not an unrelated failure: " + what);
}

// ---------------------------------------------------------------------------
// Actions: kMaxTotalActions = 10,000 -- each action here has no channels,
// so this stays nowhere near the channel/keyframe budgets.
// ---------------------------------------------------------------------------
static void testActionBudget() {
    std::string xml = "<mc3 version=\"0.3\" model=\"action-budget\">\n  <actions>\n";
    for (int i = 0; i < 10'001; ++i)
        xml += "    <action name=\"a" + std::to_string(i) + "\"/>\n";
    xml += "  </actions>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(), "10,001 empty actions exceeds the action budget and is rejected");
    check(what.find("action") != std::string::npos,
          "rejection names the action budget, not an unrelated failure: " + what);

    std::string okXml = "<mc3 version=\"0.3\" model=\"action-ok\">\n  <actions>\n";
    for (int i = 0; i < 500; ++i)
        okXml += "    <action name=\"a" + std::to_string(i) + "\"/>\n";
    okXml += "  </actions>\n</mc3>\n";
    Mc3Document doc = load(okXml);
    check(doc.actions.size() == 500, "500 actions (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Channels: kMaxTotalChannels = 200,000 -- a single action with many
// channels (no keyframes each), staying well under the action budget (1)
// and the keyframe budget (0 keyframes).
// ---------------------------------------------------------------------------
static void testChannelBudget() {
    std::string xml = "<mc3 version=\"0.3\" model=\"channel-budget\">\n  <actions>\n"
                       "    <action name=\"a\">\n";
    for (int i = 0; i < 200'001; ++i)
        xml += "      <channel target=\"obj\" property=\"position.x\"/>\n";
    xml += "    </action>\n  </actions>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(), "200,001 channels (1 action) exceeds the channel budget and is rejected");
    check(what.find("channel") != std::string::npos,
          "rejection names the channel budget, not an unrelated failure: " + what);

    std::string okXml = "<mc3 version=\"0.3\" model=\"channel-ok\">\n  <actions>\n"
                         "    <action name=\"a\">\n";
    for (int i = 0; i < 500; ++i)
        okXml += "      <channel target=\"obj\" property=\"position.x\"/>\n";
    okXml += "    </action>\n  </actions>\n</mc3>\n";
    Mc3Document doc = load(okXml);
    check(doc.actions.count("a") == 1 && doc.actions.at("a").channels.size() == 500,
          "500 channels (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Keyframes: kMaxTotalKeyframes = 2,000,000 -- a single channel with many
// keyframes, staying well under the action and channel budgets.
// ---------------------------------------------------------------------------
static void testKeyframeBudget() {
    std::string xml = "<mc3 version=\"0.3\" model=\"keyframe-budget\">\n  <actions>\n"
                       "    <action name=\"a\">\n"
                       "      <channel target=\"obj\" property=\"position.x\">\n";
    for (int i = 0; i < 2'000'001; ++i)
        xml += "        <keyframe time=\"" + std::to_string(i) + "\" value=\"0\"/>\n";
    xml += "      </channel>\n    </action>\n  </actions>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(),
          "2,000,001 keyframes (1 action, 1 channel) exceeds the keyframe budget and is rejected");
    check(what.find("keyframe") != std::string::npos,
          "rejection names the keyframe budget, not an unrelated failure: " + what);

    std::string okXml = "<mc3 version=\"0.3\" model=\"keyframe-ok\">\n  <actions>\n"
                         "    <action name=\"a\">\n"
                         "      <channel target=\"obj\" property=\"position.x\">\n";
    for (int i = 0; i < 1000; ++i)
        okXml += "        <keyframe time=\"" + std::to_string(i) + "\" value=\"0\"/>\n";
    okXml += "      </channel>\n    </action>\n  </actions>\n</mc3>\n";
    Mc3Document doc = load(okXml);
    check(doc.actions.count("a") == 1 &&
          doc.actions.at("a").channels.size() == 1 &&
          doc.actions.at("a").channels[0].keyframes.size() == 1000,
          "1000 keyframes (under budget) all load fine");
}

int main() {
    testMaterialBudget();
    testTextureBudget();
    testEmbedCountBudget();
    testEmbedAggregateBytesBudget();
    testActionBudget();
    testChannelBudget();
    testKeyframeBudget();

    if (failures == 0) { std::cout << "All document-budget tests passed.\n"; return 0; }
    std::cerr << failures << " document-budget test(s) failed.\n";
    return 1;
}
