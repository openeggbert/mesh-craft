// SYS-W14-28 -- CNA-free coverage for the reusable-library authoring flow:
// named definition creation, publication through the dedicated .mc3lib I/O,
// import health, collision reporting, and identity validation.

#include <MeshCraft/LibraryWorkflowAlgorithms.hpp>

#include <filesystem>
#include <iostream>
#include <string>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

namespace {

std::filesystem::path testDir() {
    const auto dir = std::filesystem::temp_directory_path() / "meshcraft_library_workflow_test";
    std::filesystem::create_directories(dir);
    return dir;
}

std::shared_ptr<Mc3Object> makeBox(const std::string& id, const std::string& material = {}) {
    auto object = std::make_shared<Mc3Object>();
    object->id = id;
    object->name = id;
    object->type = ObjectType::Box;
    object->primitive = Mc3Primitive::box({1.0f, 1.0f, 1.0f});
    object->material = material;
    return object;
}

} // namespace

int main() {
    Mc3Document scene;
    scene.model = "Workshop";
    Mc3Material oak;
    oak.name = "oak";
    scene.materials[oak.name] = oak;
    auto chair = makeBox("chair_placed", "oak");
    chair->name = "Oak chair";
    chair->layer = "furniture";
    chair->transform.position = {2.0f, 0.0f, -1.0f};
    scene.objects.push_back(chair);

    // Named creation keeps the placement but turns its reusable source into
    // an identity-transform definition, which is what a library publisher
    // needs rather than the old anonymous def_N conversion.
    auto instance = createNamedDefinitionFromSelectionAlg(scene, chair, "chair.oak");
    const std::array<float, 3> originalPosition{2.0f, 0.0f, -1.0f};
    const std::array<float, 3> origin{0.0f, 0.0f, 0.0f};
    CHECK(scene.definitions.count("chair.oak") == 1,
          "named definition creation: stores the requested definition id");
    CHECK(scene.objects.size() == 1 && scene.objects.front() == instance,
          "named definition creation: replaces the placed object in place");
    CHECK(instance->type == ObjectType::Instance && instance->definition == "chair.oak",
          "named definition creation: replacement is an Instance targeting the definition");
    CHECK(instance->transform.position == originalPosition,
          "named definition creation: placed transform is preserved on the Instance");
    CHECK(scene.definitions["chair.oak"]->transform.position == origin,
          "named definition creation: reusable definition is local-space identity");
    CHECK(scene.definitions["chair.oak"]->material == "oak",
          "named definition creation: referenced material stays on the reusable definition");

    bool duplicateRejected = false;
    try { (void)createNamedDefinitionFromSelectionAlg(scene, instance, "chair.oak"); }
    catch (const std::invalid_argument&) { duplicateRejected = true; }
    CHECK(duplicateRejected, "named definition creation: duplicate ids are rejected instead of overwritten");

    bool badNameRejected = false;
    try { (void)createNamedDefinitionFromSelectionAlg(scene, instance, "bad id"); }
    catch (const std::invalid_argument&) { badNameRejected = true; }
    CHECK(badNameRejected, "named definition creation: invalid ids are rejected before mutation");

    // Publishing builds the small self-contained library document and goes
    // through the dedicated XML and JSON library save/load APIs.
    Mc3Document published = publishDefinitionAsLibraryAlg(scene, "chair.oak", "furniture", "1.2.3");
    CHECK(published.library.has_value(), "publish: output has a required library identity");
    CHECK(published.library && published.library->contentHash.rfind("sha256:", 0) == 0,
          "publish: output carries a freshly computed content hash");
    CHECK(published.definitions.count("chair.oak") == 1 && published.materials.count("oak") == 1,
          "publish: output keeps the definition and its referenced material together");

    const auto xmlPath = testDir() / "furniture-1.2.3.mc3lib.xml";
    const auto jsonPath = testDir() / "furniture-1.2.3.mc3lib.json";
    published.saveToLibraryFile(xmlPath);
    published.saveToLibraryJsonFile(jsonPath);
    const Mc3Document xmlRoundTrip = Mc3Document::loadFromLibraryFile(xmlPath);
    const Mc3Document jsonRoundTrip = Mc3Document::loadFromLibraryJsonFile(jsonPath);
    CHECK(xmlRoundTrip.library && xmlRoundTrip.library->libraryNamespace == "furniture" &&
          xmlRoundTrip.library->version == "1.2.3",
          "publish: XML library round-trip retains namespace and version");
    CHECK(jsonRoundTrip.library && jsonRoundTrip.library->contentHash == published.library->contentHash,
          "publish: JSON library round-trip retains the published hash");

    // The editor refresh layer retains transient imports in memory for
    // placement but marks them external, so a normal scene writer will not
    // accidentally bake a library dependency into the scene.
    Mc3Document consumer;
    consumer.model = "Room";
    consumer.sourcePath = testDir();
    consumer.imports.push_back(Mc3Import{"furn", "mc3lib://furniture@1.2.3", ""});
    std::set<std::string> importedKeys;
    const auto health = refreshImportedDefinitionsAlg(consumer, {testDir()}, importedKeys);
    CHECK(health.imports.size() == 1 && health.imports.front().resolvedPath == xmlPath,
          "import health: records the exact resolved library path");
    CHECK(health.imports.front().libraryNamespace == "furniture" &&
          health.imports.front().libraryVersion == "1.2.3" &&
          health.imports.front().definitionCount == 1,
          "import health: exposes declared identity and direct definition count");
    CHECK(consumer.definitions.count("furn:chair.oak") == 1 &&
          consumer.includedDefs.count("furn:chair.oak") == 1 &&
          importedKeys.count("furn:chair.oak") == 1,
          "import refresh: resolved definition is usable but marked external for saving");

    // A local value with the imported key must not silently lose to a
    // resolver refresh. This is intentionally an actionable error, not the
    // resolver's old invisible last-write-wins merge behaviour.
    Mc3Document collision;
    collision.sourcePath = testDir();
    collision.imports.push_back(Mc3Import{"furn", "mc3lib://furniture@1.2.3", ""});
    collision.definitions["furn:chair.oak"] = makeBox("local_override");
    bool collisionRejected = false;
    try { std::set<std::string> keys; (void)refreshImportedDefinitionsAlg(collision, {testDir()}, keys); }
    catch (const std::runtime_error& e) {
        collisionRejected = std::string(e.what()).find("Definition collision") != std::string::npos;
    }
    CHECK(collisionRejected && collision.definitions.count("furn:chair.oak") == 1,
          "import health: local/imported definition collisions are surfaced without overwriting local data");

    // Filename matching alone is insufficient: the library identity inside
    // the file must agree with the mc3lib URI that requested it.
    Mc3Document impostor = published;
    impostor.library->libraryNamespace = "different-library";
    impostor.library->contentHash = "sha256:" + impostor.computeLibraryContentHash();
    impostor.saveToLibraryFile(testDir() / "claimed-1.0.0.mc3lib.xml");
    Mc3Document identityConsumer;
    identityConsumer.imports.push_back(Mc3Import{"claimed", "mc3lib://claimed@1.0.0", ""});
    bool identityRejected = false;
    try { (void)Mc3ImportResolver({testDir()}).resolveWithHealth(identityConsumer); }
    catch (const std::runtime_error& e) {
        identityRejected = std::string(e.what()).find("identity mismatch") != std::string::npos;
    }
    CHECK(identityRejected, "import health: a filename/library identity mismatch is rejected clearly");

    if (failures == 0)
        std::cout << "All library workflow tests passed.\n";
    else
        std::cerr << failures << " library workflow test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
