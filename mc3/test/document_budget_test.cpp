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

// ---------------------------------------------------------------------------
// Children-per-node: kMaxChildrenPerNode = 20,000 -- a single <group> with
// 20,001 direct children stays at ~20,002 TOTAL objects (group + children),
// nowhere near kMaxTotalObjects (100,000), so only the new per-node breadth
// check can catch it.
// ---------------------------------------------------------------------------
static void testChildrenPerNodeBudget() {
    // parsePrimitive() unconditionally charges segments/subdivisions_x/
    // subdivisions_z against the tessellation-weight budget regardless of
    // primitive type (even a <box>, which doesn't semantically use any of
    // them) -- explicit minimal values here (segments=0, subdivisions=1
    // each, the lowest attrCountBudgeted allows) keep 20,001 children's
    // combined tessellation weight (~40,002) far under the 500,000 budget,
    // so it's genuinely the children-per-node check that fires, not
    // tessellation.
    std::string xml = "<mc3 version=\"0.3\" model=\"children-budget\">\n"
                       "  <objects>\n    <group name=\"wide\">\n";
    for (int i = 0; i < 20'001; ++i)
        xml += "      <box name=\"c" + std::to_string(i) +
               "\" segments=\"0\" subdivisions_x=\"1\" subdivisions_z=\"1\"/>\n";
    xml += "    </group>\n  </objects>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(),
          "20,001 direct children of one group (total objects ~20,002, well "
          "under the 100,000 total-object budget) is rejected");
    check(what.find("children") != std::string::npos,
          "rejection names the children-per-node budget specifically, not "
          "total-objects or another budget: " + what);
    check(what.find("object budget") == std::string::npos,
          "rejection is NOT the total-object budget (would prove the wrong "
          "check caught it): " + what);

    std::string okXml = "<mc3 version=\"0.3\" model=\"children-ok\">\n"
                         "  <objects>\n    <group name=\"wide\">\n";
    for (int i = 0; i < 500; ++i)
        okXml += "      <box name=\"c" + std::to_string(i) + "\"/>\n";
    okXml += "    </group>\n  </objects>\n</mc3>\n";
    Mc3Document doc = load(okXml);
    check(doc.objects.size() == 1 && doc.objects[0] && doc.objects[0]->children.size() == 500,
          "500 direct children (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Recursion depth: mc3's own parseObject/parseChildren recursion has no
// dedicated depth guard (unlike MCB's RecursionGuard<256>, McbReader.cpp),
// but empirically this is ALREADY safe -- tinyxml2 itself caps element
// nesting at TINYXML2_MAX_ELEMENT_DEPTH=500 (tinyxml2.h) and rejects deeper
// XML with XML_ELEMENT_DEPTH_EXCEEDED before Mc3XmlParser's own recursion is
// ever reached, so the C++ call stack can never recurse past ~500 levels
// (trivially safe; the underlying `std::vector`-of-shared_ptr children
// representation doesn't blow the stack at that depth). This test proves
// that empirically: a few-thousand-level-deep <group> nesting must be
// rejected with a clear error (not crash the process).
// ---------------------------------------------------------------------------
static void testRecursionDepthAlreadyBounded() {
    const int N = 2000; // well past tinyxml2's own 500-level cap
    std::string xml = "<mc3 version=\"0.3\" model=\"deep\">\n  <objects>\n";
    for (int i = 0; i < N; ++i) xml += "<group name=\"g" + std::to_string(i) + "\">";
    xml += "<box name=\"leaf\"/>";
    for (int i = 0; i < N; ++i) xml += "</group>";
    xml += "\n  </objects>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(),
          std::to_string(N) + "-level-deep nesting is rejected (by tinyxml2's own "
          "element-depth cap), not a crash");
}

int main() {
    testMaterialBudget();
    testTextureBudget();
    testEmbedCountBudget();
    testEmbedAggregateBytesBudget();
    testActionBudget();
    testChannelBudget();
    testKeyframeBudget();
    testChildrenPerNodeBudget();
    testRecursionDepthAlreadyBounded();

    if (failures == 0) { std::cout << "All document-budget tests passed.\n"; return 0; }
    std::cerr << failures << " document-budget test(s) failed.\n";
    return 1;
}
