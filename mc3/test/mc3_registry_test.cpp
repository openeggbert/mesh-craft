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

    // Search by group
    auto byGroup = reg.search("Furniture");
    CHECK(byGroup.size() == 1, "registry: search by group works");

    // Search by tags
    auto byTag = reg.search("seating");
    CHECK(byTag.size() == 1, "registry: search by tag 'seating' works");

    auto byTag2 = reg.search("indoor");
    CHECK(byTag2.size() == 1, "registry: search by tag 'indoor' works");

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

static void testInsertDuplicateDefId() {
    // Two registry entries with the same definition id "crate" inserted into one scene
    // must get unique ids: "crate" and "crate_1".
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_dup_test.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    // First entry — crate with wood material
    auto doc1 = makeDocWithDef();
    auto e1 = reg.entryFromDefinition(doc1, "crate", "G", "Crate1", "", "", "", "");

    // Second entry — a different crate (different material) but same def id
    Mc3Document doc2;
    Mc3Material mat2;
    mat2.baseColor = {0.8f, 0.1f, 0.1f, 1.0f};
    doc2.materials["steel"] = mat2;
    auto box2 = std::make_shared<Mc3Object>(); box2->id="b2"; box2->type=ObjectType::Box;
    box2->primitive=Mc3Primitive{}; box2->material="steel";
    auto def2 = std::make_shared<Mc3Object>(); def2->id="crate"; def2->type=ObjectType::Group;
    def2->children.push_back(box2);
    doc2.definitions["crate"] = def2;
    auto e2 = reg.entryFromDefinition(doc2, "crate", "G", "Crate2", "", "", "", "");

    Mc3Document scene;
    std::string id1, id2;
    try {
        id1 = reg.insertIntoScene(scene, e1);
        id2 = reg.insertIntoScene(scene, e2);
        CHECK(true, "duplicate def: both inserts did not throw");
    } catch (const std::exception& ex) {
        fail(std::string("duplicate def: threw: ") + ex.what());
        reg.close(); fs::remove(dbPath);
        return;
    }

    CHECK(id1 != id2,                               "duplicate def: ids are distinct");
    CHECK(scene.definitions.count(id1) == 1,        "duplicate def: first def present");
    CHECK(scene.definitions.count(id2) == 1,        "duplicate def: second def present");
    // The first id must be "crate", the second must have a suffix
    CHECK(id1 == "crate",                           "duplicate def: first id is 'crate'");
    CHECK(id2 == "crate_1",                         "duplicate def: second id is 'crate_1'");

    // Materials from both entries must be merged into the scene
    CHECK(scene.materials.count("wood") == 1,       "duplicate def: wood material merged");
    CHECK(scene.materials.count("steel") == 1,      "duplicate def: steel material merged");

    reg.close();
    fs::remove(dbPath);
}

// ---------------------------------------------------------------------------
// STAB-0340/0341/0342 — registry edge cases
// ---------------------------------------------------------------------------

// STAB-0340: an unavailable registry (never opened, or opened+closed) must
// never crash — every public method guards on `db_ == nullptr` and returns a
// safe default. This is the exact same code path a no-SQLite3 stub build
// exercises (see the `#ifndef MESHCRAFT_HAS_SQLITE3` stubs above in
// ModelRegistry.cpp, which return the same defaults unconditionally); the UI
// side (MeshCraftApplication_UiRegistry.cpp:17-27) checks `isOpen()` and shows
// "Model Registry is not available in this build" rather than proceeding —
// verified by code inspection since driving the real ImGui panel needs a
// CNA context.
static void testUnavailableRegistryNoCrash() {
    ModelRegistry reg; // never opened
    CHECK(!reg.isOpen(), "unavailable registry: isOpen() false before any open()");
    CHECK(reg.search("anything").empty(),
          "unavailable registry: search() returns empty, no crash");

    ModelRegistry::Entry e;
    e.name = "X"; e.xml = "<mc3/>";
    CHECK(reg.save(e) == -1, "unavailable registry: save() returns -1, no crash");

    reg.remove(123); // must not crash
    CHECK(true, "unavailable registry: remove() does not crash");

    // Same guarantee after open() + close() (not just before the first open()).
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_unavailable_after_close.sqlite3";
    fs::remove(dbPath);
    reg.open(dbPath);
    reg.close();
    CHECK(!reg.isOpen(), "unavailable registry: isOpen() false after close()");
    CHECK(reg.search("anything").empty(),
          "unavailable registry: search() safe after close()");
    fs::remove(dbPath);
}

// STAB-0341: opening a path that can't be a SQLite DB file (a directory)
// throws std::runtime_error with a named, non-empty message, and leaves the
// registry closed — matching what MeshCraftApplication_UiRegistry.cpp's
// catch block reports via setStatusMsg(std::string("Registry: ") + ex.what()).
static void testOpenFailureThrowsNamedError() {
    namespace fs = std::filesystem;
    auto badPath = fs::temp_directory_path() / "mc3_reg_open_fail_dir";
    fs::remove_all(badPath);
    fs::create_directory(badPath);

    ModelRegistry reg;
    bool threw = false;
    std::string message;
    try {
        reg.open(badPath); // a directory can't be opened as a SQLite file
    } catch (const std::exception& ex) {
        threw = true;
        message = ex.what();
    }
    CHECK(threw, "registry open on an invalid path throws");
    CHECK(!message.empty(), "registry open failure message is non-empty");
    CHECK(message.find("ModelRegistry open failed") != std::string::npos,
          "registry open failure message identifies the failing operation");
    CHECK(!reg.isOpen(), "registry stays closed after a failed open()");

    fs::remove_all(badPath);
}

// STAB-0342: search matches each of group/name/tags/description independently.
// Each entry below has a marker unique to exactly one field, so a false
// match (e.g. searching group text also hitting name) would be caught.
static void testSearchMatchesEachFieldIndependently() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_search_fields.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    ModelRegistry::Entry e;
    e.group       = "GroupMarkerZZZ";
    e.name        = "NameMarkerZZZ";
    e.variant     = "";
    e.xml         = "<mc3/>";
    e.tags        = "TagMarkerZZZ";
    e.description = "DescMarkerZZZ";
    e.source      = "SourceMarkerZZZ";
    int64_t id = reg.save(e);
    CHECK(id > 0, "search-fields: entry saved");

    CHECK(reg.search("GroupMarkerZZZ").size() == 1, "search: matches by group");
    CHECK(reg.search("NameMarkerZZZ").size()  == 1, "search: matches by name");
    CHECK(reg.search("TagMarkerZZZ").size()   == 1, "search: matches by tags");
    CHECK(reg.search("DescMarkerZZZ").size()  == 1, "search: matches by description");
    CHECK(reg.search("SourceMarkerZZZ").size() == 1, "search: matches by source");

    // Case-insensitivity (search SQL uses lower() on both sides).
    CHECK(reg.search("descmarkerzzz").size() == 1,
          "search: matches by description case-insensitively");

    // A marker that doesn't appear in any field must not match.
    CHECK(reg.search("NoSuchMarkerAtAll").empty(),
          "search: no match for a marker absent from every field");

    reg.close();
    fs::remove(dbPath);
}

// STAB-0343: exact scenario from plan.md — saving "Chair" and searching the
// lowercase "chair" still finds it.
static void testSearchCaseInsensitiveByName() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_search_ci_name.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    ModelRegistry::Entry e;
    e.name = "Chair"; e.xml = "<mc3/>";
    CHECK(reg.save(e) > 0, "search-ci-name: entry saved");

    CHECK(reg.search("chair").size() == 1, "search-ci-name: lowercase 'chair' finds 'Chair'");
    CHECK(reg.search("CHAIR").size() == 1, "search-ci-name: uppercase 'CHAIR' finds 'Chair'");

    reg.close();
    fs::remove(dbPath);
}

// STAB-0344: searching by source returns only entries with a matching
// source, not entries that merely share a name/group/tag/description.
// Found during STAB-0342 investigation that `source` was entirely absent
// from the search SQL (and from the search box's hint text) — fixed as
// part of this task rather than left undone, per user direction.
static void testSearchBySourceField() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_search_source.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    ModelRegistry::Entry aiEntry;
    aiEntry.group = "Furniture"; aiEntry.name = "AiChair"; aiEntry.xml = "<mc3/>";
    aiEntry.source = "ai_generated";
    CHECK(reg.save(aiEntry) > 0, "search-by-source: ai entry saved");

    ModelRegistry::Entry handEntry;
    handEntry.group = "Furniture"; handEntry.name = "HandChair"; handEntry.xml = "<mc3/>";
    handEntry.source = "handmade";
    CHECK(reg.save(handEntry) > 0, "search-by-source: handmade entry saved");

    auto aiResults = reg.search("ai_generated");
    CHECK(aiResults.size() == 1, "search-by-source: 'ai_generated' returns exactly 1 entry");
    if (!aiResults.empty())
        CHECK(aiResults[0].name == "AiChair",
              "search-by-source: 'ai_generated' returns only the AI-sourced entry");

    auto handResults = reg.search("handmade");
    CHECK(handResults.size() == 1, "search-by-source: 'handmade' returns exactly 1 entry");
    if (!handResults.empty())
        CHECK(handResults[0].name == "HandChair",
              "search-by-source: 'handmade' returns only the handmade entry");

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
    testInsertDuplicateDefId();
    testMigration();
    testUnavailableRegistryNoCrash();
    testOpenFailureThrowsNamedError();
    testSearchMatchesEachFieldIndependently();
    testSearchCaseInsensitiveByName();
    testSearchBySourceField();
#else
    std::cout << "SKIP: ModelRegistry tests require MESHCRAFT_HAS_SQLITE3\n";
#endif
    std::cout << "\n" << (failures == 0 ? "All registry tests passed."
                                        : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures > 0 ? 1 : 0;
}
