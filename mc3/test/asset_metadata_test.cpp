// R111 -- structured asset metadata (mesh_world_revival.md §6), attached
// via Mc3Object::assetMetadata. Proves the full field set round-trips
// through both Mc3Document::saveToFile/loadFromFile (XML) and
// saveToJsonFile/loadFromJsonFile (JSON) -- same one-AST-two-surfaces
// pattern as R109/R110, hand-compared field by field rather than via the
// shared RandomMc3DocumentGenerator/compareDocuments() harness (which
// doesn't know about this new field yet -- kept deliberately separate
// rather than touched, to keep this task's blast radius to new files
// only).

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include <filesystem>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

static int tmpIdx = 0;
static std::filesystem::path tmpPath(const char* ext) {
    return std::filesystem::temp_directory_path() /
           ("mc3_asset_metadata_test_" + std::to_string(tmpIdx++) + ext);
}

static Mc3AssetMetadata buildFullMetadata() {
    Mc3AssetMetadata am;
    am.category = "window";
    am.subcategory = "residential_double";
    am.semanticTags = {"residential", "double"};
    am.styleTags = {"classic", "central-europe"};
    am.regionTags = {"central-europe"};
    am.periodTags = {"20th-century"};
    am.nominalSize = {1.4f, 1.6f, 0.16f};
    am.boundsMin = {-0.7f, -0.8f, -0.08f};
    am.boundsMax = {0.7f, 0.8f, 0.08f};
    am.facing = "-Z";
    am.sockets = {{"wallAnchor", {0.f, 0.f, 0.f}}, {"handle", {0.38f, 1.0f, -0.08f}}};
    am.materialSlots = {"frame", "glass", "hardware"};
    am.collisionProxy = "box";
    am.clearanceVolume = {1.6f, 2.0f, 0.5f};
    am.lods = {{"near", "window.residential.double_03.lod0"}, {"far", "window.residential.double_03.lod2"}};
    am.instancingEligible = false;
    am.shadowPolicy = "cast_receive";
    am.maxVisibilityDistanceM = 250.0f;
    am.selectionWeight = 2.5f;
    am.license = "CC0";
    am.provenance = "hand-authored";
    am.sourceGeneratorOrHash = "lua.object.window.double_pane";
    am.semanticVersion = "1.2.0";
    return am;
}

static void expectMetadataEquals(const Mc3AssetMetadata& a, const Mc3AssetMetadata& b, const std::string& label) {
    CHECK(a.category == b.category, label + ": category matches");
    CHECK(a.subcategory == b.subcategory, label + ": subcategory matches");
    CHECK(a.semanticTags == b.semanticTags, label + ": semanticTags matches");
    CHECK(a.styleTags == b.styleTags, label + ": styleTags matches");
    CHECK(a.regionTags == b.regionTags, label + ": regionTags matches");
    CHECK(a.periodTags == b.periodTags, label + ": periodTags matches");
    CHECK(a.nominalSize == b.nominalSize, label + ": nominalSize matches");
    CHECK(a.boundsMin == b.boundsMin, label + ": boundsMin matches");
    CHECK(a.boundsMax == b.boundsMax, label + ": boundsMax matches");
    CHECK(a.facing == b.facing, label + ": facing matches");
    CHECK(a.sockets == b.sockets, label + ": sockets matches");
    CHECK(a.materialSlots == b.materialSlots, label + ": materialSlots matches");
    CHECK(a.collisionProxy == b.collisionProxy, label + ": collisionProxy matches");
    CHECK(a.clearanceVolume == b.clearanceVolume, label + ": clearanceVolume matches");
    CHECK(a.lods == b.lods, label + ": lods matches");
    CHECK(a.instancingEligible == b.instancingEligible, label + ": instancingEligible matches");
    CHECK(a.shadowPolicy == b.shadowPolicy, label + ": shadowPolicy matches");
    CHECK(a.maxVisibilityDistanceM == b.maxVisibilityDistanceM, label + ": maxVisibilityDistanceM matches");
    CHECK(a.selectionWeight == b.selectionWeight, label + ": selectionWeight matches");
    CHECK(a.license == b.license, label + ": license matches");
    CHECK(a.provenance == b.provenance, label + ": provenance matches");
    CHECK(a.sourceGeneratorOrHash == b.sourceGeneratorOrHash, label + ": sourceGeneratorOrHash matches");
    CHECK(a.semanticVersion == b.semanticVersion, label + ": semanticVersion matches");
}

int main() {
    Mc3Document doc;
    doc.model = "asset_metadata_fixture";
    auto window = std::make_shared<Mc3Object>();
    window->type = ObjectType::Box;
    window->primitive = Mc3Primitive::box({1.4f, 1.6f, 0.16f});
    window->assetMetadata = buildFullMetadata();
    doc.defineObject("window.residential.double_03", window);

    // A second definition with NO assetMetadata, to prove the optional
    // field stays genuinely absent (not defaulted-and-written) when unset.
    auto plain = std::make_shared<Mc3Object>();
    plain->type = ObjectType::Box;
    plain->primitive = Mc3Primitive::box({1.f, 1.f, 1.f});
    doc.defineObject("plain.box", plain);

    // --- XML round-trip ---
    {
        auto p = tmpPath(".mc3.xml");
        doc.saveToFile(p);
        Mc3Document reloaded = Mc3Document::loadFromFile(p);
        std::filesystem::remove(p);

        auto it = reloaded.definitions.find("window.residential.double_03");
        CHECK(it != reloaded.definitions.end(), "XML: window definition present after reload");
        if (it != reloaded.definitions.end()) {
            CHECK(it->second->assetMetadata.has_value(), "XML: assetMetadata present after reload");
            if (it->second->assetMetadata)
                expectMetadataEquals(*doc.definitions["window.residential.double_03"]->assetMetadata,
                                      *it->second->assetMetadata, "XML");
        }

        auto plainIt = reloaded.definitions.find("plain.box");
        CHECK(plainIt != reloaded.definitions.end(), "XML: plain definition present after reload");
        if (plainIt != reloaded.definitions.end())
            CHECK(!plainIt->second->assetMetadata.has_value(),
                  "XML: plain definition has NO assetMetadata after reload (optional field stays absent)");
    }

    // --- JSON round-trip ---
    {
        auto p = tmpPath(".mc3.json");
        doc.saveToJsonFile(p);
        Mc3Document reloaded = Mc3Document::loadFromJsonFile(p);
        std::filesystem::remove(p);

        auto it = reloaded.definitions.find("window.residential.double_03");
        CHECK(it != reloaded.definitions.end(), "JSON: window definition present after reload");
        if (it != reloaded.definitions.end()) {
            CHECK(it->second->assetMetadata.has_value(), "JSON: assetMetadata present after reload");
            if (it->second->assetMetadata)
                expectMetadataEquals(*doc.definitions["window.residential.double_03"]->assetMetadata,
                                      *it->second->assetMetadata, "JSON");
        }

        auto plainIt = reloaded.definitions.find("plain.box");
        CHECK(plainIt != reloaded.definitions.end(), "JSON: plain definition present after reload");
        if (plainIt != reloaded.definitions.end())
            CHECK(!plainIt->second->assetMetadata.has_value(),
                  "JSON: plain definition has NO assetMetadata after reload (optional field stays absent)");
    }

    if (failures == 0)
        std::cout << "All MC3 asset metadata (R111) tests passed.\n";
    else
        std::cerr << failures << " MC3 asset metadata (R111) test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
