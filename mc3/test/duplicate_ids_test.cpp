// SYS-W1-04 — duplicate object IDs.
//
// Investigation (see plan.md SYS-W1-04 for the full writeup): mc3's own
// parser/document model never does any ID-KEYED lookup. `obj.id` is parsed
// (Mc3XmlParser.cpp parseCommonObjectAttribs) and carried through as opaque
// data; objects are always appended to a plain std::vector
// (doc.objects.push_back), never inserted into an id-keyed map. Animation
// channels resolve their target by NAME (Mc3Channel::targetObject, resolved
// via flatFindByName in the editor), not by id. So two objects sharing the
// same id="..." in a single document's own <objects> block do NOT collide,
// get silently overwritten, or corrupt anything at the mc3 library level --
// both are preserved, in document order, and both round-trip through
// save/reload unchanged.
//
// This is deliberately a *proving* test, not a policy fix: the editor
// application layer (src/MeshCraft/MeshCraftApplication_Commands.cpp
// flatFindById, and the object-lock feature's lockedIds_ set keyed by
// obj->id) DOES do first-match/set-keyed lookups on `id`, so a duplicate id
// there is ambiguous (locking one of two same-id objects locks both,
// flatFindById returns whichever occurs first). Deciding whether the mc3
// parser should treat a duplicate id as a hard parse error or auto-rename
// duplicates is a product/UX decision (would change behavior for any
// existing authored content that happens to reuse an id) that this
// stabilization pass is not authorized to make unilaterally -- see plan.md
// SYS-W1-04's status note. This test documents and locks in the current,
// safe (non-crashing, non-data-losing) library-level behavior so a future
// change to that policy is a deliberate, visible diff here.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Validation.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

int main() {
    fs::path dir = fs::temp_directory_path() / "mc3_duplicate_ids_test";
    fs::create_directories(dir);

    const char* xml =
        "<mc3 version=\"0.3\" model=\"dupid\">\n"
        "  <objects>\n"
        "    <box name=\"first\" id=\"dup\"/>\n"
        "    <sphere name=\"second\" id=\"dup\"/>\n"
        "    <cylinder name=\"third\" id=\"unique\"/>\n"
        "  </objects>\n"
        "</mc3>\n";
    fs::path path = dir / "dupid.mc3.xml";
    { std::ofstream f(path); f << xml; }

    bool threw = false;
    std::string what;
    Mc3Document doc;
    try {
        doc = Mc3Document::loadFromFile(path);
    } catch (const std::exception& e) {
        threw = true;
        what = e.what();
    }

    check(!threw, std::string("duplicate ids: loading does not throw (got: ") + what + ")");
    check(doc.objects.size() == 3, "duplicate ids: all 3 objects survive parsing (no silent drop)");

    if (doc.objects.size() == 3) {
        check(doc.objects[0]->name == "first" && doc.objects[0]->id == "dup",
              "duplicate ids: first 'dup' object preserved in document order");
        check(doc.objects[1]->name == "second" && doc.objects[1]->id == "dup",
              "duplicate ids: second 'dup' object preserved distinctly (not overwritten "
              "by the first, not merged into it)");
        check(doc.objects[2]->name == "third" && doc.objects[2]->id == "unique",
              "duplicate ids: an unrelated unique-id object after the duplicates is "
              "unaffected");
    }

    // Round-trip: the writer must not deduplicate/collapse the two same-id
    // objects either -- both must still be present after save + reload.
    fs::path savedPath = dir / "dupid_saved.mc3.xml";
    doc.saveToFile(savedPath);

    bool reloadThrew = false;
    Mc3Document reloaded;
    try {
        reloaded = Mc3Document::loadFromFile(savedPath);
    } catch (const std::exception&) {
        reloadThrew = true;
    }
    check(!reloadThrew, "duplicate ids: saved document with duplicate ids reloads without throwing");
    check(!reloadThrew && reloaded.objects.size() == 3,
          "duplicate ids: save->reload preserves all 3 objects (writer does not "
          "dedupe by id)");
    if (!reloadThrew && reloaded.objects.size() == 3) {
        int dupCount = 0;
        for (const auto& o : reloaded.objects) if (o->id == "dup") ++dupCount;
        check(dupCount == 2,
              "duplicate ids: both 'dup'-id objects survive a full save->reload cycle");
    }

    // SYS-W1-04 (human-authorized decision, 2026-07-17): duplicate ids stay a
    // permissive parse (proven above) but now surface a warning-level
    // Mc3Validation diagnostic, since the mc3-library-level safety this test
    // proves does NOT extend to the editor application layer's id-keyed
    // lookups (flatFindById, lockedIds_) -- see checkDuplicateObjectIds() in
    // Mc3XmlParser.cpp.
    {
        Mc3Validation validation;
        Mc3Document validated = Mc3Document::loadFromFile(path, Mc3LoadPolicy::trusted(), validation);
        check(validated.objects.size() == 3,
              "duplicate ids + validation: loading with a validation sink still "
              "succeeds and preserves all 3 objects");

        int dupWarnings = 0;
        for (const auto& e : validation.entries)
            if (e.severity == Mc3ValidationSeverity::Warning && e.objectId == "dup")
                ++dupWarnings;
        check(dupWarnings == 1,
              "duplicate ids + validation: exactly one warning-level diagnostic for "
              "id 'dup' (not one per duplicate object)");
        check(validation.hasWarnings() && !validation.hasErrors(),
              "duplicate ids + validation: duplicate ids are a warning, never an "
              "error -- parsing must not be rejected");

        // The unique-id object must not spuriously trigger a diagnostic.
        for (const auto& e : validation.entries)
            check(e.objectId != "unique",
                  "duplicate ids + validation: the non-duplicated 'unique' id gets "
                  "no diagnostic");
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All duplicate-id tests passed.\n"; return 0; }
    std::cerr << failures << " duplicate-id test(s) failed.\n";
    return 1;
}
