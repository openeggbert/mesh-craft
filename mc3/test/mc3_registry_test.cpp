#include "MeshCraft/ModelRegistry.hpp"
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef MESHCRAFT_HAS_SQLITE3
#include <sqlite3.h>
#endif

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

        // STAB-0346: verify the inserted definition actually parsed correctly —
        // not just that *some* entry landed under `insertedId`, but that its
        // object tree matches what was originally serialized: a Group
        // containing one Box child with material "wood" (see makeDocWithDef()).
        const auto& insertedDef = scene.definitions.at(insertedId);
        CHECK(insertedDef->type == ObjectType::Group,
              "insertIntoScene: inserted definition parsed with the correct type (Group)");
        CHECK(insertedDef->children.size() == 1,
              "insertIntoScene: inserted definition parsed with the correct child count");
        if (!insertedDef->children.empty()) {
            const auto& insertedChild = insertedDef->children.front();
            CHECK(insertedChild->type == ObjectType::Box,
                  "insertIntoScene: inserted definition's child parsed with the correct type (Box)");
            CHECK(insertedChild->material == "wood",
                  "insertIntoScene: inserted definition's child kept its material reference");
        }
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

// STAB-0425: two independently-authored registry entries can plausibly reuse
// the same generic material name (e.g. "wood") for two *different* materials.
// insertIntoScene() must not let the second insert silently keep whichever
// "wood" the scene already has — the second entry's own material (and the
// inserted object's reference to it) must survive under a suffixed id.
static void testInsertMaterialNameCollision() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_matcollision_test.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    // Entry 1: "crate" definition using "wood" = brown.
    auto doc1 = makeDocWithDef();
    auto e1 = reg.entryFromDefinition(doc1, "crate", "G", "Crate", "", "", "", "");

    // Entry 2: a different "chair" definition that ALSO names its (different)
    // material "wood" = red.
    Mc3Document doc2;
    Mc3Material redWood;
    redWood.roughness = 0.2f;
    redWood.baseColor = {0.9f, 0.1f, 0.1f, 1.0f};
    doc2.materials["wood"] = redWood;
    auto chairBox = std::make_shared<Mc3Object>();
    chairBox->id = "cb1"; chairBox->type = ObjectType::Box;
    chairBox->primitive = Mc3Primitive{}; chairBox->material = "wood";
    auto chairDef = std::make_shared<Mc3Object>();
    chairDef->id = "chair"; chairDef->type = ObjectType::Group;
    chairDef->children.push_back(chairBox);
    doc2.definitions["chair"] = chairDef;
    auto e2 = reg.entryFromDefinition(doc2, "chair", "G", "Chair", "", "", "", "");

    Mc3Document scene;
    std::string id1, id2;
    try {
        id1 = reg.insertIntoScene(scene, e1);
        id2 = reg.insertIntoScene(scene, e2);
        CHECK(true, "material collision: both inserts did not throw");
    } catch (const std::exception& ex) {
        fail(std::string("material collision: threw: ") + ex.what());
        reg.close(); fs::remove(dbPath);
        return;
    }

    CHECK(scene.materials.count("wood") == 1,
          "material collision: original 'wood' (brown) still present");
    if (scene.materials.count("wood"))
        CHECK(scene.materials.at("wood").baseColor[0] < 0.7f,
              "material collision: original 'wood' unchanged (still brown, not red)");

    CHECK(scene.materials.count("wood_1") == 1,
          "material collision: entry 2's material renamed to 'wood_1'");
    if (scene.materials.count("wood_1"))
        CHECK(scene.materials.at("wood_1").baseColor[0] > 0.7f,
              "material collision: 'wood_1' has entry 2's actual (red) values");

    // The inserted 'chair' definition's box must reference the renamed
    // material, not silently keep pointing at the scene's pre-existing 'wood'.
    CHECK(scene.definitions.count(id2) == 1, "material collision: chair definition present");
    if (scene.definitions.count(id2)) {
        auto& def = scene.definitions.at(id2);
        CHECK(def && !def->children.empty(),
              "material collision: chair definition has its child");
        if (def && !def->children.empty())
            CHECK(def->children[0]->material == "wood_1",
                  "material collision: chair box's material reference remapped to 'wood_1'");
    }

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

// STAB-0065: a *corrupted* SQLite file — unlike testOpenFailureThrowsNamedError's
// directory-as-path case (which fails at sqlite3_open() itself), garbage bytes
// in an otherwise-valid file path pass sqlite3_open() (SQLite validates the
// file format lazily) and only fail once createSchema()'s CREATE TABLE
// actually executes — a different code path (open()'s createSchema() call,
// not the sqlite3_open() error branch), so it needs its own test.
static void testCorruptedDatabaseThrowsCleanly() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_corrupted.sqlite3";
    fs::remove(dbPath);

    {
        std::ofstream garbage(dbPath, std::ios::binary);
        garbage << "this is not a valid SQLite database file, just garbage bytes\0\xFF\xFE";
    }

    ModelRegistry reg;
    bool threw = false;
    std::string message;
    try {
        reg.open(dbPath);
    } catch (const std::exception& ex) {
        threw = true;
        message = ex.what();
    }
    CHECK(threw, "corrupted db: open() throws rather than crashing");
    CHECK(!message.empty(), "corrupted db: exception message is non-empty");
    CHECK(!reg.isOpen(), "corrupted db: registry stays closed after the failed open()");

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

// STAB-0061: exercises the actual ALTER TABLE ADD COLUMN path in
// createSchema() — writes a legacy-schema DB (pre-description/source
// columns, matching what a DB created before those columns existed would
// look like) directly via sqlite3, then confirms ModelRegistry::open()
// migrates it in place without throwing and without losing the pre-existing
// row.
static void testMigrationFromLegacySchema() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_legacy_migrate.sqlite3";
    fs::remove(dbPath);

    {
        sqlite3* raw = nullptr;
        CHECK(sqlite3_open(dbPath.string().c_str(), &raw) == SQLITE_OK,
              "legacy migration: raw sqlite3_open succeeds");
        const char* legacySchema =
            "CREATE TABLE models ("
            "  id      INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  grp     TEXT NOT NULL DEFAULT '',"
            "  name    TEXT NOT NULL,"
            "  variant TEXT NOT NULL DEFAULT '',"
            "  xml     TEXT NOT NULL,"
            "  tags    TEXT NOT NULL DEFAULT '',"
            "  created INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
            ");";
        char* errmsg = nullptr;
        sqlite3_exec(raw, legacySchema, nullptr, nullptr, &errmsg);
        CHECK(errmsg == nullptr, "legacy migration: legacy CREATE TABLE (no description/source) succeeds");
        if (errmsg) sqlite3_free(errmsg);

        sqlite3_exec(raw,
            "INSERT INTO models(grp,name,variant,xml,tags) "
            "VALUES ('', 'LegacyWidget', '', '<mc3/>', '');",
            nullptr, nullptr, &errmsg);
        CHECK(errmsg == nullptr, "legacy migration: insert into legacy schema succeeds");
        if (errmsg) sqlite3_free(errmsg);
        sqlite3_close(raw);
    }

    {
        ModelRegistry reg;
        reg.open(dbPath);  // createSchema() must ALTER TABLE ADD COLUMN here, not throw
        auto results = reg.search("");
        CHECK(results.size() == 1, "legacy migration: pre-existing row survives ALTER TABLE migration");
        if (!results.empty()) {
            CHECK(results[0].name == "LegacyWidget", "legacy migration: name preserved");
            CHECK(results[0].description.empty(), "legacy migration: description defaults to empty after ADD COLUMN");
            CHECK(results[0].source.empty(), "legacy migration: source defaults to empty after ADD COLUMN");
        }
        reg.close();
    }
    fs::remove(dbPath);
}

// ---------------------------------------------------------------------------
// STAB-0351 — thumbnails are explicitly unsupported (design placeholder only,
// see m1m2m3.md: "Not yet implemented: thumbnail column"). `Entry` having no
// `thumbnail` member is a compile-time fact (verified by reading
// ModelRegistry.hpp), not something a runtime test can assert without C++
// reflection — but the *schema* is a runtime fact this test CAN check
// directly: query the real `models` table via `PRAGMA table_info` and
// confirm no column named "thumbnail" exists, so a future accidental
// addition wouldn't silently drift from the documented design.
// ---------------------------------------------------------------------------

static void testNoThumbnailColumn() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_no_thumbnail.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath); // creates the schema

    sqlite3* raw = nullptr;
    CHECK(sqlite3_open_v2(dbPath.string().c_str(), &raw, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
          "no-thumbnail: can reopen the DB file directly for schema inspection");

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(raw, "PRAGMA table_info(models);", -1, &stmt, nullptr);
    bool hasThumbnailColumn = false;
    int  columnCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ++columnCount;
        const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)); // column 1 = name
        if (name && std::string(name) == "thumbnail") hasThumbnailColumn = true;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(raw);

    CHECK(columnCount > 0, "no-thumbnail: table_info returned the models table's columns");
    CHECK(!hasThumbnailColumn,
          "no-thumbnail: the 'models' table schema has no thumbnail column (design placeholder only)");

    reg.close();
    fs::remove(dbPath);
}

// ---------------------------------------------------------------------------
// STAB-0359 — large XML content (>1MB) round-trips through SQLite without
// truncation or corruption.
// ---------------------------------------------------------------------------

static void testLargeXmlContent() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_large_xml.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    // Build a >1MB XML-ish payload out of a repeating, distinctive pattern
    // so a truncation or byte-corruption bug would be easy to catch (as
    // opposed to a uniform filler that could truncate "successfully").
    std::string bigXml = "<mc3 version=\"0.3\"><definitions><definition id=\"big\">";
    std::string chunk;
    for (int i = 0; i < 100; ++i)
        chunk += "<box id=\"b" + std::to_string(i) + "\" size=\"1 1 1\"/>";
    while (bigXml.size() < 1024 * 1024) bigXml += chunk;
    bigXml += "</definition></definitions></mc3>";
    CHECK(bigXml.size() > 1024 * 1024, "large-xml: test payload is actually over 1MB");

    ModelRegistry::Entry e;
    e.name = "BigModel";
    e.xml  = bigXml;
    int64_t id = reg.save(e);
    CHECK(id > 0, "large-xml: entry with >1MB xml saved without error");

    auto results = reg.search("BigModel");
    CHECK(results.size() == 1, "large-xml: entry is found again by search");
    if (!results.empty()) {
        CHECK(results[0].xml.size() == bigXml.size(),
              "large-xml: retrieved xml is exactly the same length as what was saved");
        CHECK(results[0].xml == bigXml,
              "large-xml: retrieved xml is byte-for-byte identical (no corruption)");
    }

    reg.close();
    fs::remove(dbPath);
}

// ---------------------------------------------------------------------------
// STAB-0364 — special characters in name/tags save and search correctly
// (parameterized SQLite queries, not string concatenation, so this is
// expected to already work — this test locks it in explicitly).
// ---------------------------------------------------------------------------

static void testSpecialCharsInNameAndTags() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_special_chars.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    ModelRegistry::Entry e;
    e.name = "door (gothic)";
    e.tags = "arch medieval";
    e.xml  = "<mc3/>";
    int64_t id = reg.save(e);
    CHECK(id > 0, "special-chars: entry with parens/spaces in name saved without error");

    auto byName = reg.search("door (gothic)");
    CHECK(byName.size() == 1, "special-chars: exact name with parens found by search");
    if (!byName.empty())
        CHECK(byName[0].name == "door (gothic)",
              "special-chars: name with parens preserved exactly on retrieval");

    auto byTag = reg.search("medieval");
    CHECK(byTag.size() == 1, "special-chars: tag search still works alongside a parenthesized name");

    reg.close();
    fs::remove(dbPath);
}

// ---------------------------------------------------------------------------
// STAB-0368 — saving with an existing id updates that row in place rather
// than inserting a duplicate.
// ---------------------------------------------------------------------------

static void testUpdateExistingEntry() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_update.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    ModelRegistry::Entry e;
    e.name = "Lamp"; e.xml = "<mc3/>"; e.description = "Original description";
    int64_t id = reg.save(e);
    CHECK(id > 0, "update-entry: initial save succeeds");

    ModelRegistry::Entry updated;
    updated.id          = id;
    updated.name        = "Lamp";
    updated.xml         = "<mc3/>";
    updated.description = "Updated description";
    int64_t updatedId = reg.save(updated);
    CHECK(updatedId == id, "update-entry: saving with an existing id returns that same id");

    auto results = reg.search("");
    CHECK(results.size() == 1, "update-entry: still exactly 1 row — no duplicate inserted");
    if (!results.empty())
        CHECK(results[0].description == "Updated description",
              "update-entry: search returns the updated description, not the original");

    reg.close();
    fs::remove(dbPath);
}

// ---------------------------------------------------------------------------
// STAB-0370 — an empty variant field saves and retrieves correctly (not
// corrupted into NULL or some other sentinel).
// ---------------------------------------------------------------------------

static void testEmptyVariantField() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_empty_variant.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    ModelRegistry::Entry e;
    e.name = "GenericBox"; e.xml = "<mc3/>"; e.variant = "";
    int64_t id = reg.save(e);
    CHECK(id > 0, "empty-variant: entry with empty variant saved without error");

    auto results = reg.search("GenericBox");
    CHECK(results.size() == 1, "empty-variant: entry found by search");
    if (!results.empty())
        CHECK(results[0].variant.empty(),
              "empty-variant: retrieved variant is an empty string, not corrupted");

    reg.close();
    fs::remove(dbPath);
}

// ---------------------------------------------------------------------------
// STAB-0345 — entryFromDefinition's XML includes only the materials (and
// their textures) actually referenced by the definition being saved, not
// every material in the source scene document.
// ---------------------------------------------------------------------------

static void testEntryFromDefinitionOnlyIncludesReferencedMaterials() {
    namespace fs = std::filesystem;
    auto dbPath = fs::temp_directory_path() / "mc3_reg_scoped_materials.sqlite3";
    fs::remove(dbPath);

    ModelRegistry reg;
    reg.open(dbPath);

    auto doc = makeDocWithDef(); // "crate" definition references only "wood"
    Mc3Material unrelated;
    unrelated.baseColor = {0.1f, 0.1f, 0.9f, 1.0f};
    doc.materials["unrelated_material"] = unrelated; // present in the scene, NOT referenced by "crate"

    ModelRegistry::Entry e = reg.entryFromDefinition(doc, "crate", "G", "Crate", "", "", "", "");
    CHECK(e.xml.find("wood") != std::string::npos,
          "entryFromDef scoping: xml includes the referenced material 'wood'");
    CHECK(e.xml.find("unrelated_material") == std::string::npos,
          "entryFromDef scoping: xml does NOT include the unreferenced 'unrelated_material'");

    // Confirm at the parsed-document level too, not just a substring check.
    auto tmpPath = fs::temp_directory_path() / "mc3_reg_scoped_materials_check.mc3.xml";
    { std::ofstream f(tmpPath); f << e.xml; }
    Mc3Document parsed = Mc3Document::loadFromFile(tmpPath);
    CHECK(parsed.materials.size() == 1,
          "entryFromDef scoping: exactly 1 material round-trips through the saved entry");
    CHECK(parsed.materials.count("wood") == 1,
          "entryFromDef scoping: that material is 'wood'");
    fs::remove(tmpPath);

    reg.close();
    fs::remove(dbPath);
}

// ---------------------------------------------------------------------------
// STAB-0349 — the default registry DB path is ~/.meshcraft/modelregistry.sqlite3
// (or %USERPROFILE%\.meshcraft\modelregistry.sqlite3 on Windows).
// ---------------------------------------------------------------------------

static void testDefaultPathFormat() {
    auto path = ModelRegistry::defaultPath();
    CHECK(path.filename() == "modelregistry.sqlite3",
          "defaultPath: filename is 'modelregistry.sqlite3'");
    CHECK(path.parent_path().filename() == ".meshcraft",
          "defaultPath: parent directory is '.meshcraft'");
#ifndef _WIN32
    const char* home = std::getenv("HOME");
    if (home && *home) {
        CHECK(path == std::filesystem::path(home) / ".meshcraft" / "modelregistry.sqlite3",
              "defaultPath: full path is $HOME/.meshcraft/modelregistry.sqlite3");
    }
#endif
}

#endif // MESHCRAFT_HAS_SQLITE3

int main() {
#ifdef MESHCRAFT_HAS_SQLITE3
    testOpenClose();
    testSaveSearchRemove();
    testEntryFromDefinitionAndInsert();
    testInsertDuplicateDefId();
    testInsertMaterialNameCollision();
    testMigration();
    testMigrationFromLegacySchema();
    testCorruptedDatabaseThrowsCleanly();
    testUnavailableRegistryNoCrash();
    testOpenFailureThrowsNamedError();
    testSearchMatchesEachFieldIndependently();
    testSearchCaseInsensitiveByName();
    testSearchBySourceField();
    testNoThumbnailColumn();
    testLargeXmlContent();
    testSpecialCharsInNameAndTags();
    testUpdateExistingEntry();
    testEmptyVariantField();
    testEntryFromDefinitionOnlyIncludesReferencedMaterials();
    testDefaultPathFormat();
#else
    std::cout << "SKIP: ModelRegistry tests require MESHCRAFT_HAS_SQLITE3\n";
#endif
    std::cout << "\n" << (failures == 0 ? "All registry tests passed."
                                        : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures > 0 ? 1 : 0;
}
