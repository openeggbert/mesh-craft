// R110 -- .mc3lib.xml/.mc3lib.json reusable-definition-library file type
// (mesh_world_revival.md §7/§8). A library is stored as an ordinary
// Mc3Document (same AST, same XML/JSON writers/parsers as a scene/model
// file) with `library` (namespace + semver version + sha256 content hash)
// set. This proves:
//
//   1. sha256Hex() matches the well-known FIPS 180-4 test vectors (empty
//      string, "abc") -- the hand-rolled implementation is correct before
//      trusting it for anything else.
//   2. saveToLibraryFile/saveToLibraryJsonFile throw if `library` is unset
//      (a library file must always have an identity a resolver can
//      reference it by).
//   3. `library` (namespace/version/hash) round-trips through both the XML
//      and JSON surfaces.
//   4. computeLibraryContentHash() is stable for unchanged content and
//      changes when a definition's content changes -- and never includes
//      the `library` block itself (no self-reference).

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>
#include <MeshCraft/Mc3/Mc3Sha256.hpp>

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
           ("mc3lib_test_" + std::to_string(tmpIdx++) + ext);
}

static std::shared_ptr<Mc3Object> makeBox(float size) {
    auto box = std::make_shared<Mc3Object>();
    box->type = ObjectType::Box;
    box->primitive = Mc3Primitive::box({size, size, size});
    return box;
}

int main() {
    // --- 1. sha256Hex() correctness against FIPS 180-4 test vectors. ---
    CHECK(sha256Hex("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "sha256Hex(\"\") matches the empty-string test vector");
    CHECK(sha256Hex("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sha256Hex(\"abc\") matches the FIPS 180-4 test vector");

    // --- 2. saveToLibraryFile/saveToLibraryJsonFile require `library`. ---
    {
        Mc3Document doc;
        doc.model = "unnamed";
        bool threwXml = false, threwJson = false;
        try { doc.saveToLibraryFile(tmpPath(".mc3lib.xml")); } catch (const std::invalid_argument&) { threwXml = true; }
        try { doc.saveToLibraryJsonFile(tmpPath(".mc3lib.json")); } catch (const std::invalid_argument&) { threwJson = true; }
        CHECK(threwXml, "saveToLibraryFile throws when `library` is unset");
        CHECK(threwJson, "saveToLibraryJsonFile throws when `library` is unset");
    }

    // --- 3. `library` round-trips through XML and JSON. ---
    {
        Mc3Document doc;
        doc.model = "city-core";
        doc.library = Mc3LibraryInfo{"city-core", "3.2.1", ""};
        doc.defineObject("window.residential.double_04", makeBox(1.0f));
        doc.library->contentHash = "sha256:" + doc.computeLibraryContentHash();

        auto xmlPath = tmpPath(".mc3lib.xml");
        doc.saveToLibraryFile(xmlPath);
        Mc3Document fromXml = Mc3Document::loadFromLibraryFile(xmlPath);
        std::filesystem::remove(xmlPath);
        CHECK(fromXml.library.has_value(), "mc3lib.xml round-trip: `library` present");
        if (fromXml.library) {
            CHECK(fromXml.library->libraryNamespace == "city-core", "mc3lib.xml round-trip: namespace matches");
            CHECK(fromXml.library->version == "3.2.1", "mc3lib.xml round-trip: version matches");
            CHECK(fromXml.library->contentHash == doc.library->contentHash, "mc3lib.xml round-trip: contentHash matches");
        }

        auto jsonPath = tmpPath(".mc3lib.json");
        doc.saveToLibraryJsonFile(jsonPath);
        Mc3Document fromJson = Mc3Document::loadFromLibraryJsonFile(jsonPath);
        std::filesystem::remove(jsonPath);
        CHECK(fromJson.library.has_value(), "mc3lib.json round-trip: `library` present");
        if (fromJson.library) {
            CHECK(fromJson.library->libraryNamespace == "city-core", "mc3lib.json round-trip: namespace matches");
            CHECK(fromJson.library->version == "3.2.1", "mc3lib.json round-trip: version matches");
            CHECK(fromJson.library->contentHash == doc.library->contentHash, "mc3lib.json round-trip: contentHash matches");
        }
    }

    // --- 4. computeLibraryContentHash(): stable, sensitive to content,
    //        excludes the `library` block itself. ---
    {
        Mc3Document a;
        a.model = "kit";
        a.library = Mc3LibraryInfo{"kit", "1.0.0", ""};
        a.defineObject("door.simple", makeBox(1.0f));
        const std::string hashA1 = a.computeLibraryContentHash();
        const std::string hashA2 = a.computeLibraryContentHash();
        CHECK(hashA1 == hashA2, "computeLibraryContentHash() is stable for unchanged content");

        Mc3Document b = a;
        b.library->version = "2.0.0";  // change library metadata, not content
        CHECK(b.computeLibraryContentHash() == hashA1,
              "computeLibraryContentHash() does not depend on `library` itself (no self-reference)");

        Mc3Document c = a;
        c.defineObject("door.simple", makeBox(2.0f));  // change actual content
        CHECK(c.computeLibraryContentHash() != hashA1,
              "computeLibraryContentHash() changes when definition content changes");
    }

    if (failures == 0)
        std::cout << "All MC3 library (.mc3lib) tests passed.\n";
    else
        std::cerr << failures << " MC3 library (.mc3lib) test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
