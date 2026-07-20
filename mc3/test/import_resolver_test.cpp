// R101 -- Mc3ImportResolver: resolves a document's <imports>/"imports"
// into a namespace-qualified map of definitions, loading .mc3lib files
// from search directories. Covers: basic resolution, missing-dependency
// error, content-hash match/mismatch, cycle detection (direct and via a
// nested import), and that a nested import's own definitions are NOT
// merged into the top-level caller's map. Also covers R102's own
// composite-object scenario: resolveAndMergeInto() makes an `<instance
// definition="namespace:id">` resolvable through the EXISTING
// doc.definitions[...] lookup every instance consumer already uses, with
// zero consumer-side changes.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3ImportResolver.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>

#include <filesystem>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

namespace {

std::filesystem::path testDir() {
    auto dir = std::filesystem::temp_directory_path() / "mc3_import_resolver_test";
    std::filesystem::create_directories(dir);
    return dir;
}

std::shared_ptr<Mc3Object> makeBox(float size) {
    auto box = std::make_shared<Mc3Object>();
    box->type = ObjectType::Box;
    box->primitive = Mc3Primitive::box({size, size, size});
    return box;
}

// Writes a library named `name`@`version` with one definition `defId`, to
// testDir()/"<name>-<version>.mc3lib.xml", optionally with its own
// `imports`. Returns the real content hash written into `library->contentHash`.
std::string writeLibrary(const std::string& name, const std::string& version,
                          const std::string& defId,
                          const std::vector<Mc3Import>& ownImports = {}) {
    Mc3Document doc;
    doc.model = name;
    doc.library = Mc3LibraryInfo{name, version, ""};
    doc.imports = ownImports;
    doc.defineObject(defId, makeBox(1.0f));
    doc.library->contentHash = "sha256:" + doc.computeLibraryContentHash();
    doc.saveToLibraryFile(testDir() / (name + "-" + version + ".mc3lib.xml"));
    return doc.library->contentHash;
}

} // namespace

int main() {
    // --- 1. Basic resolution: one direct import, one definition. ---
    {
        writeLibrary("basic-lib", "1.0.0", "door.simple");

        Mc3Document doc;
        doc.model = "scene";
        doc.imports.push_back(Mc3Import{"door_lib", "mc3lib://basic-lib@1.0.0", ""});

        Mc3ImportResolver resolver({testDir()});
        const auto resolved = resolver.resolve(doc);

        CHECK(resolved.count("door_lib:door.simple") == 1,
              "basic resolution: definition present under local alias");
        CHECK(resolved.size() == 1, "basic resolution: exactly one definition resolved");
    }

    // --- 2. Missing dependency: source doesn't resolve to any file. ---
    {
        Mc3Document doc;
        doc.model = "scene";
        doc.imports.push_back(Mc3Import{"missing", "mc3lib://does-not-exist@9.9.9", ""});

        Mc3ImportResolver resolver({testDir()});
        bool threw = false;
        try { resolver.resolve(doc); }
        catch (const std::runtime_error&) { threw = true; }
        CHECK(threw, "missing dependency throws std::runtime_error");
    }

    // --- 3. Content hash match succeeds, mismatch throws. ---
    {
        const std::string realHash = writeLibrary("hashed-lib", "2.0.0", "window.simple");

        Mc3Document okDoc;
        okDoc.model = "scene";
        okDoc.imports.push_back(Mc3Import{"w", "mc3lib://hashed-lib@2.0.0", realHash});
        Mc3ImportResolver resolver({testDir()});
        bool okThrew = false;
        try { resolver.resolve(okDoc); } catch (const std::runtime_error&) { okThrew = true; }
        CHECK(!okThrew, "matching content hash does not throw");

        Mc3Document badDoc;
        badDoc.model = "scene";
        badDoc.imports.push_back(Mc3Import{"w", "mc3lib://hashed-lib@2.0.0",
                                            "sha256:0000000000000000000000000000000000000000000000000000000000000"});
        bool badThrew = false;
        try { resolver.resolve(badDoc); } catch (const std::runtime_error&) { badThrew = true; }
        CHECK(badThrew, "mismatched content hash throws");
    }

    // --- 4. Nested import: A imports B, B imports C. resolve(A) has B's
    //        definitions under A's own alias, but NOT C's. ---
    {
        writeLibrary("lib-c", "1.0.0", "leaf.thing");
        writeLibrary("lib-b", "1.0.0", "mid.thing", {Mc3Import{"c_alias", "mc3lib://lib-c@1.0.0", ""}});
        writeLibrary("lib-a", "1.0.0", "top.thing", {});

        Mc3Document doc;
        doc.model = "scene";
        doc.imports.push_back(Mc3Import{"b", "mc3lib://lib-b@1.0.0", ""});

        Mc3ImportResolver resolver({testDir()});
        const auto resolved = resolver.resolve(doc);

        CHECK(resolved.count("b:mid.thing") == 1, "nested: direct import's own definition present");
        CHECK(resolved.count("c_alias:leaf.thing") == 0,
              "nested: grandchild import's definition NOT merged into top-level map");
    }

    // --- 5. Missing nested dependency is still caught (validated recursively). ---
    {
        writeLibrary("lib-with-missing-nested", "1.0.0", "thing",
                     {Mc3Import{"gone", "mc3lib://truly-missing@1.0.0", ""}});

        Mc3Document doc;
        doc.model = "scene";
        doc.imports.push_back(Mc3Import{"x", "mc3lib://lib-with-missing-nested@1.0.0", ""});

        Mc3ImportResolver resolver({testDir()});
        bool threw = false;
        try { resolver.resolve(doc); } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw, "missing nested dependency is caught during recursive validation");
    }

    // --- 6. Cycle detection: A imports B, B imports A. ---
    {
        // lib-cycle-a and lib-cycle-b import each other -- write B first
        // referencing A, then A referencing B (order doesn't matter, both
        // must exist on disk before resolve() walks the graph).
        writeLibrary("lib-cycle-b", "1.0.0", "b.thing",
                     {Mc3Import{"a", "mc3lib://lib-cycle-a@1.0.0", ""}});
        writeLibrary("lib-cycle-a", "1.0.0", "a.thing",
                     {Mc3Import{"b", "mc3lib://lib-cycle-b@1.0.0", ""}});

        Mc3Document doc;
        doc.model = "scene";
        doc.imports.push_back(Mc3Import{"a", "mc3lib://lib-cycle-a@1.0.0", ""});

        Mc3ImportResolver resolver({testDir()});
        bool threw = false;
        std::string message;
        try { resolver.resolve(doc); }
        catch (const std::runtime_error& e) { threw = true; message = e.what(); }
        CHECK(threw, "import cycle is detected");
        CHECK(message.find("cycle") != std::string::npos, "cycle error message mentions 'cycle'");
    }

    // --- 7. R102: resolveAndMergeInto() makes a composite object's
    //        imported-part instance resolvable through the ordinary
    //        doc.definitions[...] lookup every existing instance consumer
    //        already uses (SceneRenderer, mc3togltf, CSG evaluation, ...) --
    //        a house imports a door part and places one instance of it. ---
    {
        writeLibrary("door-parts", "1.0.0", "door.simple");

        Mc3Document house;
        house.model = "house";
        house.imports.push_back(Mc3Import{"door_lib", "mc3lib://door-parts@1.0.0", ""});

        auto houseShell = std::make_shared<Mc3Object>();
        houseShell->type = ObjectType::Group;
        houseShell->addChild(Mc3Object::makeInstance("front_door", "door_lib:door.simple"));
        house.defineObject("house.simple", houseShell);

        CHECK(house.definitions.count("door_lib:door.simple") == 0,
              "R102: imported definition absent before resolveAndMergeInto()");

        Mc3ImportResolver resolver({testDir()});
        resolver.resolveAndMergeInto(house);

        CHECK(house.definitions.count("door_lib:door.simple") == 1,
              "R102: imported definition present after resolveAndMergeInto()");

        const auto& doorInstance = house.definitions["house.simple"]->children[0];
        CHECK(doorInstance->resolvedInstanceDefinitionKey() == "door_lib:door.simple",
              "R102: instance's own resolved key matches the imported definition's merged key");
        CHECK(house.definitions.count(doorInstance->resolvedInstanceDefinitionKey()) == 1,
              "R102: instance's resolved key is a real, resolvable entry in doc.definitions");
    }

    // --- 8 (F18, 2026-07-20 audit): a long CHAIN of DISTINCT libraries (no
    //     repeats -- not a cycle) must be rejected once it exceeds the
    //     depth cap, with a catchable exception, not left to recurse until
    //     a native stack overflow. Every other recursive parse path in this
    //     codebase already has an equivalent cap (Mc3LoadPolicy's own
    //     maxIncludeDepth for <include>, McbReader's RecursionGuard<256>,
    //     GltfExporter's kMaxNodeDepth, CsgEvaluator's CSG_MAX_DEPTH) --
    //     this was the one recursive descent left with cycle detection but
    //     no depth bound. ---
    {
        // chain0 -> chain1 -> chain2 -> ... -> chain19: 20 distinct
        // libraries, comfortably past the 16-deep cap, zero repeats (so
        // this must NOT trip cycle detection instead -- a different error).
        const int kChainLen = 20;
        for (int i = kChainLen - 1; i >= 0; --i) {
            std::vector<Mc3Import> ownImports;
            if (i + 1 < kChainLen)
                ownImports.push_back(Mc3Import{
                    "next", "mc3lib://depthchain" + std::to_string(i + 1) + "@1.0.0", ""});
            writeLibrary("depthchain" + std::to_string(i), "1.0.0",
                         "thing" + std::to_string(i), ownImports);
        }

        Mc3Document doc;
        doc.model = "scene";
        doc.imports.push_back(Mc3Import{"d0", "mc3lib://depthchain0@1.0.0", ""});

        Mc3ImportResolver resolver({testDir()});
        bool threw = false;
        std::string message;
        try { resolver.resolve(doc); }
        catch (const std::runtime_error& e) { threw = true; message = e.what(); }
        CHECK(threw, "F18: an import chain of 20 distinct libraries (past the depth cap) throws");
        CHECK(message.find("cycle") == std::string::npos,
              "F18: rejected for exceeding the depth limit, NOT misreported as a cycle "
              "(got: " + message + ")");
        CHECK(message.find("depth") != std::string::npos,
              "F18: error message names the actual reason (depth limit)");
    }

    // --- 9 (F18): a chain WITHIN the depth cap must still resolve fine --
    //     confirms the cap doesn't reject legitimate, moderately-deep
    //     compositions (the whole point of a generous, not tight, limit). ---
    {
        const int kChainLen = 5;
        for (int i = kChainLen - 1; i >= 0; --i) {
            std::vector<Mc3Import> ownImports;
            if (i + 1 < kChainLen)
                ownImports.push_back(Mc3Import{
                    "next", "mc3lib://shortchain" + std::to_string(i + 1) + "@1.0.0", ""});
            writeLibrary("shortchain" + std::to_string(i), "1.0.0",
                         "thing" + std::to_string(i), ownImports);
        }

        Mc3Document doc;
        doc.model = "scene";
        doc.imports.push_back(Mc3Import{"s0", "mc3lib://shortchain0@1.0.0", ""});

        Mc3ImportResolver resolver({testDir()});
        bool threw = false;
        try { resolver.resolve(doc); } catch (const std::runtime_error&) { threw = true; }
        CHECK(!threw, "F18: a 5-deep import chain (well within the cap) resolves without error");
    }

    if (failures == 0)
        std::cout << "All MC3 import resolver (R101/R102) tests passed.\n";
    else
        std::cerr << failures << " MC3 import resolver (R101/R102) test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
