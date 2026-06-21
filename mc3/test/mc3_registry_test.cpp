#include "MeshCraft/ModelRegistry.hpp"
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <filesystem>
#include <iostream>
#include <string>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while(0)

#ifdef MESHCRAFT_HAS_SQLITE3

static Mc3Document makeDocWithDef() {
    Mc3Document doc;
    doc.version = "0.3";
    doc.model   = "TestScene";

    Mc3Material mat;
    mat.roughness = 0.6f;
    mat.baseColor = {0.6f, 0.4f, 0.2f, 1.0f};
    doc.materials["wood"] = mat;

    auto box       = std::make_shared<Mc3Object>();
    box->id        = "b1";
    box->type      = ObjectType::Box;
    box->primitive = Mc3Primitive{};
    box->material  = "wood";

    auto def = std::make_shared<Mc3Object>();
    def->id   = "crate";
    def->type = ObjectType::Group;
    def->children.push_back(box);
    doc.definitions["crate"] = def;

    return doc;
}

static void testOpenClose() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_test.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    CHECK(!reg.isOpen(), "registry: not open before open()");
    reg.open(dbPath);
    CHECK(reg.isOpen(), "registry: open after open()");
    reg.close();
    CHECK(!reg.isOpen(), "registry: closed after close()");
    fs::remove(dbPath);
}

static void testSaveSearchRemove() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_test2.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    ModelRegistry::Entry e;
    e.group       = "Furniture";
    e.name        = "Chair";
    e.variant     = "wooden";
    e.xml         = "<mc3/>";
    e.tags        = "seating indoor";
    e.description = "A wooden chair";
    e.source      = "handmade";
    int64_t savedId = reg.save(e);
    CHECK(savedId > 0, "registry: save returns positive id");

    auto results = reg.search("");
    CHECK(results.size() == 1, "registry: search all returns 1 entry");
    if (!results.empty()) {
        CHECK(results[0].name        == "Chair",       "registry: name preserved");
        CHECK(results[0].group       == "Furniture",   "registry: group preserved");
        CHECK(results[0].variant     == "wooden",      "registry: variant preserved");
        CHECK(results[0].description == "A wooden chair", "registry: description preserved");
        CHECK(results[0].source      == "handmade",    "registry: source preserved");
        CHECK(results[0].tags        == "seating indoor", "registry: tags preserved");
    }

    auto found = reg.search("Chair");
    CHECK(found.size() == 1, "registry: search by name works");

    auto notFound = reg.search("xxxx_notexist");
    CHECK(notFound.empty(), "registry: search non-existent returns empty");

    reg.remove(savedId);
    auto afterRemove = reg.search("");
    CHECK(afterRemove.empty(), "registry: entry gone after remove");

    reg.close();
    fs::remove(dbPath);
}

static void testEntryFromDefinitionAndInsert() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_test3.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    auto doc = makeDocWithDef();
    ModelRegistry::Entry e;
    try {
        e = reg.entryFromDefinition(doc, "crate", "Containers", "Crate", "", "box container",
                                    "A simple wooden crate", "handmade");
        CHECK(true, "entryFromDefinition: did not throw");
    } catch (const std::exception& ex) {
        fail(std::string("entryFromDefinition threw: ") + ex.what());
        reg.close(); fs::remove(dbPath);
        return;
    }

    CHECK(e.group       == "Containers",              "entryFromDef: group");
    CHECK(e.name        == "Crate",                   "entryFromDef: name");
    CHECK(e.description == "A simple wooden crate",   "entryFromDef: description");
    CHECK(e.source      == "handmade",                "entryFromDef: source");
    CHECK(!e.xml.empty(),                             "entryFromDef: xml non-empty");
    CHECK(e.xml.find("crate") != std::string::npos,  "entryFromDef: xml contains 'crate'");

    int64_t id = reg.save(e);
    CHECK(id > 0, "entryFromDef: saved to DB");

    // Insert into a fresh scene
    Mc3Document scene;
    try {
        auto insertedId = reg.insertIntoScene(scene, e);
        CHECK(!insertedId.empty(),                        "insertIntoScene: returned id");
        CHECK(scene.definitions.count(insertedId) == 1,  "insertIntoScene: definition present");
        CHECK(scene.materials.count("wood") == 1,        "insertIntoScene: material merged");
    } catch (const std::exception& ex) {
        fail(std::string("insertIntoScene threw: ") + ex.what());
    }

    reg.close();
    fs::remove(dbPath);
}

static void testMigration() {
    // Opening the same DB twice should not fail (migration runs safely on existing columns)
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_migrate.sqlite3";
    fs::remove(dbPath);

    {
        ModelRegistry reg;
        reg.open(dbPath);
        ModelRegistry::Entry e;
        e.name = "Widget"; e.xml = "<mc3/>"; e.source = "test";
        reg.save(e);
        reg.close();
    }
    {
        ModelRegistry reg;
        reg.open(dbPath);  // must not throw even though columns already exist
        auto results = reg.search("");
        CHECK(results.size() == 1, "migration: entry survives DB re-open");
        reg.close();
    }
    fs::remove(dbPath);
}

#endif // MESHCRAFT_HAS_SQLITE3

int main() {
#ifdef MESHCRAFT_HAS_SQLITE3
    testOpenClose();
    testSaveSearchRemove();
    testEntryFromDefinitionAndInsert();
    testMigration();
#else
    std::cout << "SKIP: ModelRegistry tests require MESHCRAFT_HAS_SQLITE3\n";
#endif
    std::cout << "\n" << (failures == 0 ? "All registry tests passed."
                                        : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures > 0 ? 1 : 0;
}
