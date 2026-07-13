// Mc3Document::validate() test (SYS-W1-01, pre-render/pre-export/save
// integration points).
//
// mc3_validation_test.cpp already proves the diagnostic surface for content
// that goes through Mc3XmlParser (a real load from disk). This test proves
// the gap that motivates validate() specifically: a document built
// PROGRAMMATICALLY via the Mc3Object::make*() factories (as a world
// generator or the editor's own builder-method call sites would) never goes
// through the parser's clamps at all, so a hostile/out-of-range value set
// directly on a struct field would otherwise reach export/save/render with
// zero diagnostics. validate() closes that gap by round-tripping the
// in-memory document through the same writer/parser the validating load
// overloads already use.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Validation.hpp>

#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

static const Mc3ValidationEntry* findEntry(const Mc3Validation& v, const std::string& field) {
    for (const auto& e : v.entries) if (e.field == field) return &e;
    return nullptr;
}

int main() {
    // 1. A document built entirely in-memory (never parsed from XML) with a
    // hostile tessellation value set directly on the struct -- something the
    // Mc3XmlParser clamp never sees -- must still produce a diagnostic entry
    // once validate() round-trips it.
    {
        Mc3Document doc;
        doc.model = "programmatic";
        auto huge = Mc3Object::makeSphere("huge", 1.0f, 100000000, "");
        doc.addObject(huge);

        Mc3Validation validation;
        doc.validate(validation);

        check(validation.hasWarnings(),
              "a programmatically-built out-of-range sphere is caught by validate()");
        const Mc3ValidationEntry* e = findEntry(validation, "segments");
        check(e != nullptr, "validate()'s entry names the 'segments' field");
        if (e) {
            check(e->objectId == "huge", "the entry identifies the offending object ('huge')");
            check(e->suggestedRepair.find("4096") != std::string::npos,
                  "the entry's suggestedRepair names the clamp target (4096): '" + e->suggestedRepair + "'");
        }

        // The original in-memory object is untouched by validate() -- it
        // only inspects a round-tripped COPY, matching its own doc comment
        // ("re-validates ... discarding the reparsed copy").
        check(huge->primitive && huge->primitive->segments == 100000000,
              "validate() does not mutate the live document it was called on");
    }

    // 2. A well-formed programmatic document must NOT produce any findings.
    {
        Mc3Document doc;
        doc.model = "ok";
        doc.addObject(Mc3Object::makeSphere("ok", 1.0f, 32, ""));

        Mc3Validation validation;
        doc.validate(validation);
        check(validation.empty(), "a well-formed programmatic document produces no findings");
    }

    if (failures == 0) {
        std::cout << "\nAll document_validate checks passed.\n";
        return 0;
    }
    std::cerr << "\n" << failures << " document_validate check(s) FAILED.\n";
    return 1;
}
