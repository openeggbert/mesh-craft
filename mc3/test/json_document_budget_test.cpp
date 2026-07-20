// 2026-07-20 audit F4: Mc3JsonParser.cpp had per-field tessellation clamps
// (json_input_budget_test.cpp) but none of Mc3XmlParser.cpp's document-wide
// running-total budgets (mc3/test/document_budget_test.cpp) -- a document
// with many objects/materials/textures/etc. each individually legal could
// still sum to an enormous aggregate allocation, and the entire input file
// was read into memory unconditionally before any size check at all.
// Mirrors document_budget_test.cpp's own coverage, translated to
// mc3.json syntax (skipping totalIncludes/kMaxTotalIncludes and
// kMaxChildrenPerNode, neither of which apply here -- JSON has no
// <include>-equivalent merge concept, and the children-per-node breadth
// cap is a separate, XML-only check the audit didn't flag as missing).

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <algorithm>
#include <cstdint>
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

static std::string loadExpectingThrow(const std::string& json) {
    std::string what;
    try {
        Mc3Document::loadFromJsonString(json);
    } catch (const std::exception& e) {
        what = e.what();
    }
    return what;
}

// ---------------------------------------------------------------------------
// Materials: kMaxTotalMaterials = 20,000.
// ---------------------------------------------------------------------------
static void testMaterialBudget() {
    std::string js = R"({"version":"0.3","model":"mat-budget","materials":[)";
    for (int i = 0; i < 20'001; ++i) {
        if (i) js += ",";
        js += R"({"id":"m)" + std::to_string(i) + R"("})";
    }
    js += "]}";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(), "20,001 materials exceeds the material budget and is rejected");
    check(what.find("material") != std::string::npos,
          "rejection names the material budget, not an unrelated failure: " + what);

    std::string okJs = R"({"version":"0.3","model":"mat-ok","materials":[)";
    for (int i = 0; i < 500; ++i) {
        if (i) okJs += ",";
        okJs += R"({"id":"m)" + std::to_string(i) + R"("})";
    }
    okJs += "]}";
    Mc3Document doc = Mc3Document::loadFromJsonString(okJs);
    check(doc.materials.size() == 500, "500 materials (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Textures: kMaxTotalTextures = 20,000.
// ---------------------------------------------------------------------------
static void testTextureBudget() {
    std::string js = R"({"version":"0.3","model":"tex-budget","textures":[)";
    for (int i = 0; i < 20'001; ++i) {
        if (i) js += ",";
        js += R"({"id":"t)" + std::to_string(i) + R"(","uri":"t)" + std::to_string(i) + R"(.png"})";
    }
    js += "]}";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(), "20,001 textures exceeds the texture budget and is rejected");
    check(what.find("texture") != std::string::npos,
          "rejection names the texture budget, not an unrelated failure: " + what);

    std::string okJs = R"({"version":"0.3","model":"tex-ok","textures":[)";
    for (int i = 0; i < 500; ++i) {
        if (i) okJs += ",";
        okJs += R"({"id":"t)" + std::to_string(i) + R"(","uri":"t)" + std::to_string(i) + R"(.png"})";
    }
    okJs += "]}";
    Mc3Document doc = Mc3Document::loadFromJsonString(okJs);
    check(doc.textures.size() == 500, "500 textures (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Embeds: kMaxTotalEmbeds = 1,000 (count) and kMaxTotalEmbedBytes = 256MB.
// ---------------------------------------------------------------------------
static void testEmbedCountBudget() {
    std::string js = R"({"version":"0.3","model":"embed-count-budget","embeds":[)";
    for (int i = 0; i < 1'001; ++i) {
        if (i) js += ",";
        js += R"({"id":"e)" + std::to_string(i) + R"(","base64Content":"AA=="})";
    }
    js += "]}";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(), "1,001 trivial embeds exceeds the embed COUNT budget and is rejected");
    check(what.find("embed") != std::string::npos,
          "rejection names the embed budget, not an unrelated failure: " + what);

    std::string okJs = R"({"version":"0.3","model":"embed-count-ok","embeds":[)";
    for (int i = 0; i < 100; ++i) {
        if (i) okJs += ",";
        okJs += R"({"id":"e)" + std::to_string(i) + R"(","base64Content":"AA=="})";
    }
    okJs += "]}";
    Mc3Document doc = Mc3Document::loadFromJsonString(okJs);
    check(doc.embeds.size() == 100, "100 trivial embeds (under budget) all load fine");
}

static void testEmbedAggregateBytesBudget() {
    const size_t perEmbed = 60ull * 1024ull * 1024ull;
    std::string bigBody(perEmbed, 'A');
    std::string js = R"({"version":"0.3","model":"embed-bytes-budget","embeds":[)";
    for (int i = 0; i < 5; ++i) {
        if (i) js += ",";
        js += R"({"id":"e)" + std::to_string(i) + R"(","base64Content":")" + bigBody + R"("})";
    }
    js += "]}";
    bigBody.clear(); bigBody.shrink_to_fit();

    std::string what = loadExpectingThrow(js);
    check(!what.empty(),
          "5 embeds at 60MB each (300MB combined, each individually legal) "
          "exceeds the aggregate embed-bytes budget and is rejected");
    check(what.find("embed") != std::string::npos && what.find("byte") != std::string::npos,
          "rejection names the aggregate embed-bytes budget, not an unrelated failure: " + what);
}

// ---------------------------------------------------------------------------
// Actions: kMaxTotalActions = 10,000.
// ---------------------------------------------------------------------------
static void testActionBudget() {
    std::string js = R"({"version":"0.3","model":"action-budget","actions":[)";
    for (int i = 0; i < 10'001; ++i) {
        if (i) js += ",";
        js += R"({"name":"a)" + std::to_string(i) + R"("})";
    }
    js += "]}";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(), "10,001 empty actions exceeds the action budget and is rejected");
    check(what.find("action") != std::string::npos,
          "rejection names the action budget, not an unrelated failure: " + what);

    std::string okJs = R"({"version":"0.3","model":"action-ok","actions":[)";
    for (int i = 0; i < 500; ++i) {
        if (i) okJs += ",";
        okJs += R"({"name":"a)" + std::to_string(i) + R"("})";
    }
    okJs += "]}";
    Mc3Document doc = Mc3Document::loadFromJsonString(okJs);
    check(doc.actions.size() == 500, "500 actions (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Channels: kMaxTotalChannels = 200,000 -- a single action with many channels.
// ---------------------------------------------------------------------------
static void testChannelBudget() {
    std::string js = R"({"version":"0.3","model":"channel-budget","actions":[{"name":"a","channels":[)";
    for (int i = 0; i < 200'001; ++i) {
        if (i) js += ",";
        js += R"({"target":"obj","property":"position.x"})";
    }
    js += "]}]}";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(), "200,001 channels (1 action) exceeds the channel budget and is rejected");
    check(what.find("channel") != std::string::npos,
          "rejection names the channel budget, not an unrelated failure: " + what);

    std::string okJs = R"({"version":"0.3","model":"channel-ok","actions":[{"name":"a","channels":[)";
    for (int i = 0; i < 500; ++i) {
        if (i) okJs += ",";
        okJs += R"({"target":"obj","property":"position.x"})";
    }
    okJs += "]}]}";
    Mc3Document doc = Mc3Document::loadFromJsonString(okJs);
    check(doc.actions.count("a") == 1 && doc.actions.at("a").channels.size() == 500,
          "500 channels (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Keyframes: kMaxTotalKeyframes = 2,000,000 -- a single channel with many keyframes.
// ---------------------------------------------------------------------------
static void testKeyframeBudget() {
    std::string js = R"({"version":"0.3","model":"keyframe-budget","actions":[{"name":"a","channels":[)"
                      R"({"target":"obj","property":"position.x","keyframes":[)";
    for (int i = 0; i < 2'000'001; ++i) {
        if (i) js += ",";
        js += R"({"time":)" + std::to_string(i) + R"(,"value":0})";
    }
    js += "]}]}]}";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(),
          "2,000,001 keyframes (1 action, 1 channel) exceeds the keyframe budget and is rejected");
    check(what.find("keyframe") != std::string::npos,
          "rejection names the keyframe budget, not an unrelated failure: " + what);

    std::string okJs = R"({"version":"0.3","model":"keyframe-ok","actions":[{"name":"a","channels":[)"
                        R"({"target":"obj","property":"position.x","keyframes":[)";
    for (int i = 0; i < 1000; ++i) {
        if (i) okJs += ",";
        okJs += R"({"time":)" + std::to_string(i) + R"(,"value":0})";
    }
    okJs += "]}]}]}";
    Mc3Document doc = Mc3Document::loadFromJsonString(okJs);
    check(doc.actions.count("a") == 1 &&
          doc.actions.at("a").channels.size() == 1 &&
          doc.actions.at("a").channels[0].keyframes.size() == 1000,
          "1000 keyframes (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Definitions: kMaxTotalDefinitions = 20,000.
// ---------------------------------------------------------------------------
static void testDefinitionBudget() {
    std::string js = R"({"version":"0.3","model":"def-budget","definitions":[)";
    for (int i = 0; i < 20'001; ++i) {
        if (i) js += ",";
        js += R"({"id":"d)" + std::to_string(i) + R"(","object":{"type":"box"}})";
    }
    js += "]}";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(),
          "20,001 definitions (total objects ~20,001, well under the "
          "100,000 total-object budget) is rejected");
    check(what.find("definition") != std::string::npos,
          "rejection names the definition budget specifically, not "
          "total-objects or another budget: " + what);
    check(what.find("object budget") == std::string::npos,
          "rejection is NOT the total-object budget: " + what);

    std::string okJs = R"({"version":"0.3","model":"def-ok","definitions":[)";
    for (int i = 0; i < 500; ++i) {
        if (i) okJs += ",";
        okJs += R"({"id":"d)" + std::to_string(i) + R"(","object":{"type":"box"}})";
    }
    okJs += "]}";
    Mc3Document doc = Mc3Document::loadFromJsonString(okJs);
    check(doc.definitions.size() == 500, "500 definitions (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Total object count: kMaxTotalObjects = 100,000.
// ---------------------------------------------------------------------------
static void testTotalObjectBudget() {
    std::string js = R"({"version":"0.3","model":"obj-budget","objects":[)";
    for (int i = 0; i < 100'001; ++i) {
        if (i) js += ",";
        js += R"({"id":"o)" + std::to_string(i) + R"(","type":"box"})";
    }
    js += "]}";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(), "100,001 objects exceeds the total-object budget and is rejected");
    check(what.find("object budget") != std::string::npos,
          "rejection names the total-object budget specifically: " + what);
}

// ---------------------------------------------------------------------------
// Max document bytes: kMaxDocumentBytes = 512MB -- checked against the raw
// input size BEFORE json::parse() even attempts to parse it.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// 2026-07-20 audit F6: crossSection.customPoints / path.points had no
// count cap at all, mirroring Mc3XmlParser.cpp's own equivalent fix.
// ---------------------------------------------------------------------------
static void testCrossSectionPointBudget() {
    std::string js = R"({"version":"0.3","model":"cs-point-budget","objects":[)"
                      R"({"id":"e","type":"extrude","extrude":{"segments":1,)"
                      R"("crossSection":{"type":"custom","customPoints":[)";
    for (int i = 0; i < 4097; ++i) {
        if (i) js += ",";
        js += "[" + std::to_string(i) + ",0]";
    }
    js += R"(]},"path":{"type":"line"}}}]})";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(), "4097 crossSection.customPoints entries exceeds the budget and is rejected");
    check(what.find("customPoints") != std::string::npos || what.find("Points") != std::string::npos,
          "rejection names the point-count budget, not an unrelated failure: " + what);

    std::string okJs = R"({"version":"0.3","model":"cs-point-ok","objects":[)"
                        R"({"id":"e","type":"extrude","extrude":{"segments":1,)"
                        R"("crossSection":{"type":"custom","customPoints":[)";
    for (int i = 0; i < 50; ++i) {
        if (i) okJs += ",";
        okJs += "[" + std::to_string(i) + ",0]";
    }
    okJs += R"(]},"path":{"type":"line"}}}]})";
    Mc3Document doc = Mc3Document::loadFromJsonString(okJs);
    check(doc.objects.size() == 1 && doc.objects[0]->extrude &&
          doc.objects[0]->extrude->crossSection.customPoints.size() == 50,
          "50 crossSection points (under budget) all load fine");
}

static void testPathPointBudget() {
    std::string js = R"({"version":"0.3","model":"path-point-budget","objects":[)"
                      R"({"id":"e","type":"extrude","extrude":{"segments":1,)"
                      R"("crossSection":{"type":"rect"},"path":{"type":"bezier","points":[)";
    for (int i = 0; i < 4097; ++i) {
        if (i) js += ",";
        js += R"({"position":[0,)" + std::to_string(i) + R"(,0]})";
    }
    js += "]}}}]}";

    std::string what = loadExpectingThrow(js);
    check(!what.empty(), "4097 path.points entries exceeds the budget and is rejected");
    check(what.find("points") != std::string::npos,
          "rejection names the point-count budget, not an unrelated failure: " + what);

    std::string okJs = R"({"version":"0.3","model":"path-point-ok","objects":[)"
                        R"({"id":"e","type":"extrude","extrude":{"segments":1,)"
                        R"("crossSection":{"type":"rect"},"path":{"type":"bezier","points":[)";
    for (int i = 0; i < 50; ++i) {
        if (i) okJs += ",";
        okJs += R"({"position":[0,)" + std::to_string(i) + R"(,0]})";
    }
    okJs += "]}}}]}";
    Mc3Document doc = Mc3Document::loadFromJsonString(okJs);
    check(doc.objects.size() == 1 && doc.objects[0]->extrude &&
          doc.objects[0]->extrude->path.points.size() == 50,
          "50 path points (under budget) all load fine");
}

static void testMaxDocumentBytesBudget() {
    auto path = std::filesystem::temp_directory_path() / "mc3_json_document_budget_bytes_test.mc3.json";
    {
        std::ofstream f(path, std::ios::binary);
        const size_t chunkSize = 16ull * 1024ull * 1024ull;
        std::string chunk(chunkSize, 'A');
        const uintmax_t target = 512ull * 1024ull * 1024ull + 1024ull; // just over the ceiling
        uintmax_t written = 0;
        while (written < target) {
            size_t toWrite = static_cast<size_t>(std::min<uintmax_t>(chunkSize, target - written));
            f.write(chunk.data(), static_cast<std::streamsize>(toWrite));
            written += toWrite;
        }
    }

    std::string what;
    try {
        Mc3Document::loadFromJsonFile(path);
    } catch (const std::exception& e) {
        what = e.what();
    }
    std::filesystem::remove(path);

    check(!what.empty(), "a 512MB+ document file is rejected before parsing begins");
    check(what.find("bytes") != std::string::npos,
          "rejection names the document-byte-size budget, not an unrelated failure: " + what);
}

int main() {
    testMaterialBudget();
    testTextureBudget();
    testEmbedCountBudget();
    testEmbedAggregateBytesBudget();
    testActionBudget();
    testChannelBudget();
    testKeyframeBudget();
    testDefinitionBudget();
    testTotalObjectBudget();
    testCrossSectionPointBudget();
    testPathPointBudget();
    testMaxDocumentBytesBudget();

    if (failures == 0) { std::cout << "All JSON document-budget tests passed.\n"; return 0; }
    std::cerr << failures << " JSON document-budget test(s) failed.\n";
    return 1;
}
