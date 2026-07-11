// Mc3Validation test (SYS-W1-01).
//
// input_budget_test.cpp already proves the clamped VALUE is correct (e.g.
// segments="100000000" ends up <= kMaxTessellation). This test proves the
// separate thing SYS-W1-01 actually adds: that the DIAGNOSTIC SURFACE itself
// is populated -- a caller who passes an Mc3Validation& gets a structured
// entry (severity, source path, object identity, field, message, suggested
// repair) for every clamp/default/rejection, not just a silently-different
// numeric result.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3LoadPolicy.hpp>
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

static const Mc3ValidationEntry* findEntry(const Mc3Validation& v, const std::string& field) {
    for (const auto& e : v.entries) if (e.field == field) return &e;
    return nullptr;
}

static fs::path writeFile(const fs::path& dir, const std::string& name, const char* xml) {
    fs::create_directories(dir);
    fs::path p = dir / name;
    { std::ofstream f(p); f << xml; }
    return p;
}

int main() {
    fs::path dir = fs::temp_directory_path() / "mc3_validation_test";
    fs::create_directories(dir);

    // 1. A hostile tessellation value must produce a WARNING entry naming the
    // field and describing the clamp -- not just a silently-clamped value.
    {
        fs::path p = writeFile(dir, "clamp.mc3.xml",
            "<mc3 version=\"0.3\" model=\"clamp\">\n"
            "  <objects>\n"
            "    <sphere id=\"huge\" name=\"huge\" radius=\"1\" segments=\"100000000\"/>\n"
            "  </objects>\n"
            "</mc3>\n");

        Mc3Validation validation;
        Mc3Document doc = Mc3Document::loadFromFile(p, Mc3LoadPolicy::trusted(), validation);

        const Mc3Object* huge = nullptr;
        for (const auto& o : doc.objects) if (o && o->name == "huge") huge = o.get();
        check(huge && huge->primitive && huge->primitive->segments <= 4096,
              "segments=100000000 still clamped to <= 4096 (value-level behavior unchanged)");

        check(validation.hasWarnings(), "clamped segments produces at least one warning entry");
        const Mc3ValidationEntry* e = findEntry(validation, "segments");
        check(e != nullptr, "a validation entry names the 'segments' field");
        if (e) {
            check(e->severity == Mc3ValidationSeverity::Warning,
                  "the segments clamp is reported as a warning, not an error");
            check(e->objectId == "huge", "the entry's objectId identifies the offending object ('huge')");
            check(e->suggestedRepair.find("4096") != std::string::npos,
                  "the entry's suggestedRepair names the clamp target (4096): '" + e->suggestedRepair + "'");
            check(!e->sourcePath.empty(), "the entry names a non-empty source path");
        }

        // A document with no clamp-worthy content must NOT produce warnings.
        fs::path pOk = writeFile(dir, "ok.mc3.xml",
            "<mc3 version=\"0.3\" model=\"ok\">\n"
            "  <objects><sphere name=\"ok\" radius=\"1\" segments=\"32\"/></objects>\n"
            "</mc3>\n");
        Mc3Validation validationOk;
        Mc3Document::loadFromFile(pOk, Mc3LoadPolicy::trusted(), validationOk);
        check(validationOk.empty(), "a clean, legitimate document produces zero validation entries");
    }

    // 2. A malformed (non-numeric) float attribute must produce a warning,
    // not just a silently-defaulted value.
    {
        fs::path p = writeFile(dir, "malformed.mc3.xml",
            "<mc3 version=\"0.3\" model=\"malformed\">\n"
            "  <objects>\n"
            "    <sphere id=\"bad\" name=\"bad\" radius=\"not-a-number\"/>\n"
            "  </objects>\n"
            "</mc3>\n");
        Mc3Validation validation;
        Mc3Document::loadFromFile(p, Mc3LoadPolicy::trusted(), validation);

        const Mc3ValidationEntry* e = findEntry(validation, "radius");
        check(e != nullptr, "a malformed 'radius' value produces an entry naming that field");
        if (e) {
            check(e->severity == Mc3ValidationSeverity::Warning,
                  "malformed numeric input is a warning (recoverable), not an error");
            check(e->message.find("not-a-number") != std::string::npos,
                  "the entry's message includes the offending raw value: '" + e->message + "'");
        }
    }

    // 3. A document-wide budget rejection (AUD-059's DocumentBudget) must
    // populate an ERROR entry in the SAME Mc3Validation the caller passed in,
    // even though the load ALSO still throws (the exception-based contract is
    // unchanged; the validation entry is an additive side-channel populated
    // before the throw, not a replacement for it).
    {
        std::string xml = "<mc3 version=\"0.3\" model=\"budget-many\">\n  <objects>\n";
        for (int i = 0; i < 200; ++i)
            xml += "    <sphere name=\"s" + std::to_string(i) + "\" radius=\"1\" segments=\"4096\"/>\n";
        xml += "  </objects>\n</mc3>\n";
        fs::path p = writeFile(dir, "budget.mc3.xml", xml.c_str());

        Mc3Validation validation;
        bool threw = false;
        try {
            Mc3Document::loadFromFile(p, Mc3LoadPolicy::trusted(), validation);
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "the document-wide tessellation budget still throws (unchanged contract)");
        check(validation.hasErrors(),
              "the SAME validation object also received an error entry, despite the throw "
              "unwinding past the code that populated it");
        const Mc3ValidationEntry* e = findEntry(validation, "tessellation");
        check(e != nullptr, "the error entry names the 'tessellation' field");
        if (e) check(e->severity == Mc3ValidationSeverity::Error,
                     "the budget-overflow entry is an ERROR, not a warning");
    }

    // 4. Include-merge id collisions (AUDIT-0037) must be surfaced as
    // warnings naming the colliding include file as the source path. Two
    // SEPARATE includes (not the main doc's own local <materials>, which
    // parses AFTER includes and takes a different, non-collision-checked
    // code path) must each declare the same material id to trigger this.
    {
        writeFile(dir, "shared_a.mc3.xml",
            "<mc3 version=\"0.3\" model=\"shared_a\">\n"
            "  <materials><material id=\"dup\" roughness=\"0.5\"/></materials>\n"
            "</mc3>\n");
        writeFile(dir, "shared_b.mc3.xml",
            "<mc3 version=\"0.3\" model=\"shared_b\">\n"
            "  <materials><material id=\"dup\" roughness=\"0.9\"/></materials>\n"
            "</mc3>\n");
        fs::path mainPath = writeFile(dir, "collide_main.mc3.xml",
            "<mc3 version=\"0.3\" model=\"collide\">\n"
            "  <include file=\"shared_a.mc3.xml\"/>\n"
            "  <include file=\"shared_b.mc3.xml\"/>\n"
            "  <objects><box name=\"b\"/></objects>\n"
            "</mc3>\n");

        Mc3Validation validation;
        Mc3Document::loadFromFile(mainPath, Mc3LoadPolicy::trusted(), validation);

        bool found = false;
        for (const auto& e : validation.entries) {
            if (e.field == "id" && e.message.find("dup") != std::string::npos) {
                found = true;
                check(e.severity == Mc3ValidationSeverity::Warning,
                      "material id collision is a warning");
                check(e.sourcePath.find("shared_b.mc3.xml") != std::string::npos,
                      "the collision entry's sourcePath names the second (colliding) include: '" +
                      e.sourcePath + "'");
            }
        }
        check(found, "an include-merge material id collision produces a validation entry");
    }

    // 5. Without a validation argument, behavior must be identical to before
    // this feature existed (no new required parameter, no behavior change).
    {
        fs::path p = writeFile(dir, "no_validation.mc3.xml",
            "<mc3 version=\"0.3\" model=\"plain\">\n"
            "  <objects><sphere name=\"s\" segments=\"999999999\"/></objects>\n"
            "</mc3>\n");
        Mc3Document doc = Mc3Document::loadFromFile(p); // no validation arg at all
        const Mc3Object* s = nullptr;
        for (const auto& o : doc.objects) if (o && o->name == "s") s = o.get();
        check(s && s->primitive && s->primitive->segments <= 4096,
              "loadFromFile without a validation argument still clamps correctly (no regression)");
    }

    if (failures == 0) { std::cout << "All Mc3Validation tests passed.\n"; return 0; }
    std::cerr << failures << " Mc3Validation test(s) failed.\n";
    return 1;
}
