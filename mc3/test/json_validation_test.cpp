// SYS-W1-08: Mc3JsonParser's new Mc3Validation-capturing load path.
// mc3_validation_test.cpp already proves the XML side; this mirrors it
// differentially for JSON so the two surfaces are proven to report an
// analogous diagnostic for the same underlying condition, not just that the
// JSON side reports *something*.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3LoadPolicy.hpp>
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

int main() {
    fs::path dir = fs::temp_directory_path() / "mc3_json_validation_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // 1. Tessellation clamp, differentially compared against the XML side
    // for the exact same value: both surfaces must report a "segments"
    // warning whose suggestedRepair names the same clamp target.
    {
        const std::string json = R"({
            "model": "clamp_test",
            "objects": [
                {"id": "huge", "type": "sphere", "primitive": {"segments": 100000000}}
            ]
        })";
        Mc3Validation jsonValidation;
        Mc3Document::loadFromJsonString(json, {}, Mc3LoadPolicy::trusted(), jsonValidation);
        const Mc3ValidationEntry* jsonEntry = findEntry(jsonValidation, "segments");
        check(jsonEntry != nullptr, "JSON: out-of-range segments produces a 'segments' entry");
        if (jsonEntry) {
            check(jsonEntry->severity == Mc3ValidationSeverity::Warning,
                  "JSON: segments clamp is a Warning, not an Error");
            check(jsonEntry->objectId == "huge",
                  "JSON: entry identifies the offending object ('huge')");
            check(jsonEntry->suggestedRepair.find("4096") != std::string::npos,
                  "JSON: suggestedRepair names the clamp target (4096): '" +
                  jsonEntry->suggestedRepair + "'");
        }

        const std::string xml =
            "<mc3 version=\"0.3\" model=\"clamp_test\">\n"
            "  <objects><sphere id=\"huge\" segments=\"100000000\"/></objects>\n"
            "</mc3>\n";
        Mc3Validation xmlValidation;
        Mc3Document::loadFromString(xml, {}, Mc3LoadPolicy::trusted(), xmlValidation);
        const Mc3ValidationEntry* xmlEntry = findEntry(xmlValidation, "segments");
        check(xmlEntry != nullptr, "XML: out-of-range segments produces a 'segments' entry (baseline)");
        if (xmlEntry && jsonEntry)
            check(xmlEntry->suggestedRepair == jsonEntry->suggestedRepair,
                  "XML and JSON clamp the same value to the same repair: '" +
                  xmlEntry->suggestedRepair + "' == '" + jsonEntry->suggestedRepair + "'");
    }

    // 2. Invalid enum: an unrecognized object "type" is defaulted to group,
    // not silently accepted -- differs from XML (which drops the object
    // entirely), but the point of this task is diagnostic parity, not
    // behavioral parity, so only the "something was reported" half is
    // asserted here.
    {
        const std::string json = R"({
            "model": "unknown_type_test",
            "objects": [ {"id": "mystery", "type": "not_a_real_type"} ]
        })";
        Mc3Validation validation;
        Mc3Document doc = Mc3Document::loadFromJsonString(json, {}, Mc3LoadPolicy::trusted(), validation);
        const Mc3ValidationEntry* e = findEntry(validation, "type");
        check(e != nullptr, "JSON: unrecognized object type produces a 'type' entry");
        if (e) {
            check(e->severity == Mc3ValidationSeverity::Warning,
                  "JSON: unrecognized type is a Warning (defaulted, not rejected)");
            check(e->message.find("not_a_real_type") != std::string::npos,
                  "JSON: message names the offending raw type string");
        }
        check(doc.objects.size() == 1 && doc.objects[0] && doc.objects[0]->type == ObjectType::Group,
              "JSON: the object is still created, defaulted to Group");
    }

    // 3. Excessive limits: the document-wide object budget, hit via file
    // load so the "document '<path>'" source tag is exercised too.
    {
        std::string json = R"({"model":"budget_test","objects":[)";
        for (int i = 0; i < 100'001; ++i) {
            if (i) json += ",";
            json += R"({"type":"group"})";
        }
        json += "]}";
        fs::path p = dir / "budget.mc3.json";
        { std::ofstream f(p, std::ios::binary); f << json; }

        Mc3Validation validation;
        bool threw = false;
        try {
            Mc3Document::loadFromJsonFile(p, Mc3LoadPolicy::trusted(), validation);
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "JSON: exceeding the total object budget throws");
        const Mc3ValidationEntry* e = findEntry(validation, "objects");
        check(e != nullptr, "JSON: the object-budget rejection is captured before the throw");
        if (e) check(e->severity == Mc3ValidationSeverity::Error,
                     "JSON: object-budget rejection is an Error");
    }

    // 4. Hard rejection: an untrusted document's absolute meshSource path is
    // rejected, and the rejection is captured before the throw propagates.
    {
        const std::string json = R"({
            "model": "confinement_test",
            "objects": [ {"id": "escapee", "type": "mesh", "meshSource": "/etc/passwd"} ]
        })";
        Mc3Validation validation;
        bool threw = false;
        try {
            Mc3Document::loadFromJsonString(json, dir, Mc3LoadPolicy::untrusted(), validation);
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "JSON: an absolute meshSource under untrusted() throws");
        const Mc3ValidationEntry* e = findEntry(validation, "mesh source");
        check(e != nullptr, "JSON: the confinement rejection is captured before the throw");
        if (e) {
            check(e->severity == Mc3ValidationSeverity::Error,
                  "JSON: confinement rejection is an Error");
            check(e->objectId == "escapee",
                  "JSON: entry identifies the offending object ('escapee')");
        }
    }

    // 5. Library identity: .mc3lib.json's validation-capturing overload.
    {
        const std::string json = R"({
            "model": "lib_test",
            "library": {"namespace": "test.lib", "version": "1.0"},
            "objects": [ {"id": "big", "type": "sphere", "primitive": {"segments": 999999}} ]
        })";
        fs::path p = dir / "test.mc3lib.json";
        { std::ofstream f(p, std::ios::binary); f << json; }

        Mc3Validation validation;
        Mc3Document doc = Mc3Document::loadFromLibraryJsonFile(p, validation);
        check(doc.library.has_value(), "JSON library load still populates library identity");
        check(findEntry(validation, "segments") != nullptr,
              "JSON library load's validation overload still captures parser clamps");
    }

    // 6. Clean valid document: no entries at all.
    {
        const std::string json = R"({
            "model": "clean_test",
            "objects": [ {"id": "ok", "type": "sphere", "primitive": {"segments": 32}} ]
        })";
        Mc3Validation validation;
        Mc3Document::loadFromJsonString(json, {}, Mc3LoadPolicy::trusted(), validation);
        check(validation.empty(), "JSON: a clean valid document produces zero validation entries");
    }

    // 7. No regression: omitting the validation argument still clamps
    // correctly (the additive side-channel is opt-in, not required).
    {
        const std::string json = R"({
            "model": "no_validation_arg_test",
            "objects": [ {"id": "huge2", "type": "sphere", "primitive": {"segments": 999999}} ]
        })";
        Mc3Document doc = Mc3Document::loadFromJsonString(json); // no validation arg at all
        check(doc.objects.size() == 1 && doc.objects[0] && doc.objects[0]->primitive &&
              doc.objects[0]->primitive->segments == 4096,
              "JSON: loadFromJsonString without a validation argument still clamps correctly");
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All JSON validation tests passed.\n"; return 0; }
    std::cerr << failures << " JSON validation test(s) failed.\n";
    return 1;
}
