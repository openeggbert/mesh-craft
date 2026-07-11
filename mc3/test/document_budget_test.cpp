// SYS-W1-03: document-complexity budgets BEYOND AUD-059's total object
// count + total tessellation weight (input_budget_test.cpp covers those).
//
// Each sub-test constructs a document that exceeds exactly ONE new budget
// dimension while staying comfortably under every other budget (including
// the pre-existing kMaxTotalObjects/kMaxTotalTessellationWeight/
// kMaxTotalIncludes), then confirms the rejection specifically names that
// dimension -- proving the new check is what actually caught it, not an
// unrelated budget tripping first.
//
// Grown incrementally, one dimension per commit -- see this session's
// plan.md entry for SYS-W1-03 for exactly which dimensions are covered.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

static Mc3Document load(const std::string& xml) {
    auto path = std::filesystem::temp_directory_path() / "mc3_document_budget_test.mc3.xml";
    { std::ofstream f(path); f << xml; }
    Mc3Document doc = Mc3Document::loadFromFile(path);
    std::filesystem::remove(path);
    return doc;
}

// Loads `xml`, expecting it to throw. Returns the exception message (empty
// if it didn't throw, which the caller should treat as a failure).
static std::string loadExpectingThrow(const std::string& xml) {
    auto path = std::filesystem::temp_directory_path() / "mc3_document_budget_test.mc3.xml";
    { std::ofstream f(path); f << xml; }
    std::string what;
    try {
        Mc3Document::loadFromFile(path);
    } catch (const std::exception& e) {
        what = e.what();
    }
    std::filesystem::remove(path);
    return what;
}

// ---------------------------------------------------------------------------
// Materials: kMaxTotalMaterials = 20,000.
// ---------------------------------------------------------------------------
static void testMaterialBudget() {
    // Comfortably over the material budget, using trivial materials so no
    // other budget (objects, tessellation) is anywhere close to its ceiling.
    std::string xml = "<mc3 version=\"0.3\" model=\"mat-budget\">\n  <materials>\n";
    for (int i = 0; i < 20'001; ++i)
        xml += "    <material id=\"m" + std::to_string(i) + "\"/>\n";
    xml += "  </materials>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(), "20,001 materials exceeds the material budget and is rejected");
    check(what.find("material") != std::string::npos,
          "rejection names the material budget, not an unrelated failure: " + what);

    // A comfortably-under-budget document must still load fine.
    std::string okXml = "<mc3 version=\"0.3\" model=\"mat-ok\">\n  <materials>\n";
    for (int i = 0; i < 500; ++i)
        okXml += "    <material id=\"m" + std::to_string(i) + "\"/>\n";
    okXml += "  </materials>\n</mc3>\n";
    Mc3Document doc = load(okXml);
    check(doc.materials.size() == 500, "500 materials (under budget) all load fine");
}

// ---------------------------------------------------------------------------
// Textures: kMaxTotalTextures = 20,000 (regular + svg combined).
// ---------------------------------------------------------------------------
static void testTextureBudget() {
    std::string xml = "<mc3 version=\"0.3\" model=\"tex-budget\">\n  <textures>\n";
    for (int i = 0; i < 20'001; ++i)
        xml += "    <texture id=\"t" + std::to_string(i) + "\" uri=\"t" +
               std::to_string(i) + ".png\"/>\n";
    xml += "  </textures>\n</mc3>\n";

    std::string what = loadExpectingThrow(xml);
    check(!what.empty(), "20,001 textures exceeds the texture budget and is rejected");
    check(what.find("texture") != std::string::npos,
          "rejection names the texture budget, not an unrelated failure: " + what);

    std::string okXml = "<mc3 version=\"0.3\" model=\"tex-ok\">\n  <textures>\n";
    for (int i = 0; i < 500; ++i)
        okXml += "    <texture id=\"t" + std::to_string(i) + "\" uri=\"t" +
                 std::to_string(i) + ".png\"/>\n";
    okXml += "  </textures>\n</mc3>\n";
    Mc3Document doc = load(okXml);
    check(doc.textures.size() == 500, "500 textures (under budget) all load fine");
}

int main() {
    testMaterialBudget();
    testTextureBudget();

    if (failures == 0) { std::cout << "All document-budget tests passed.\n"; return 0; }
    std::cerr << failures << " document-budget test(s) failed.\n";
    return 1;
}
