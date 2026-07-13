// GltfExporter::validation test (SYS-W1-01, pre-export integration point).
//
// mc3_validation_test.cpp (mc3/) already proves the diagnostic surface for
// content that goes through Mc3XmlParser. This test proves the gap that
// motivates pre-export validation specifically: a document built
// PROGRAMMATICALLY (as mc3togltf's own callers could, or as a world
// generator using Mc3Object::make*() would) never goes through the parser's
// clamps, so a hostile value set directly on a struct field would otherwise
// reach export with zero diagnostics.

#include "GltfExporter.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Validation.hpp>

#include <filesystem>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;
using namespace mc3togltf;
namespace fs = std::filesystem;

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
    fs::path outPath = fs::temp_directory_path() / "mc3togltf_pre_export_validation_test.glb";

    // 1. A programmatically-built document with an out-of-range tessellation
    // value must still export successfully (validate() is diagnostic-only,
    // never a gate) AND populate `validation` with the same clamp entry a
    // real load would have produced. Deliberately NOT an extreme value like
    // 1e8: unlike mc3/'s document_validate_test (XML round-trip only, never
    // builds geometry), exportDocument() here goes on to actually
    // TESSELLATE the still-unclamped live `doc` (validate() only inspects a
    // round-tripped copy, per its own doc comment) -- segments feeds
    // buildSphere()'s O(segments^2) vertex count (MeshBuilder.cpp), so 1e8
    // would attempt a many-petabyte allocation. 5000 is safely past the
    // documented 4096 ceiling (still clamped, still reported) while staying
    // in the same real-allocation ballpark as the ceiling itself.
    {
        Mc3Document doc;
        doc.model = "programmatic";
        doc.addObject(Mc3Object::makeSphere("huge", 1.0f, 5000, ""));

        GltfExporter exporter;
        exporter.exportDocument(doc, outPath, OutputFormat::GLB);

        check(fs::exists(outPath), "export succeeds despite the hostile value (diagnostic-only)");
        check(!exporter.validation.empty(),
              "GltfExporter::validation is populated for a programmatically-built document");
        const Mc3ValidationEntry* e = findEntry(exporter.validation, "segments");
        check(e != nullptr, "the pre-export validation entry names the 'segments' field");
        if (e) {
            check(e->objectId == "huge", "the entry identifies the offending object ('huge')");
            check(e->suggestedRepair.find("4096") != std::string::npos,
                  "the entry's suggestedRepair names the clamp target (4096)");
        }
        std::error_code ec;
        fs::remove(outPath, ec);
    }

    // 2. A well-formed document must export with an EMPTY validation result.
    {
        Mc3Document doc;
        doc.model = "ok";
        doc.addObject(Mc3Object::makeSphere("ok", 1.0f, 32, ""));

        GltfExporter exporter;
        exporter.exportDocument(doc, outPath, OutputFormat::GLB);

        check(exporter.validation.empty(),
              "a well-formed programmatic document produces no pre-export validation findings");
        std::error_code ec;
        fs::remove(outPath, ec);
    }

    if (failures == 0) {
        std::cout << "\nAll pre_export_validation checks passed.\n";
        return 0;
    }
    std::cerr << "\n" << failures << " pre_export_validation check(s) FAILED.\n";
    return 1;
}
