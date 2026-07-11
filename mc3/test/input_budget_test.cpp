// Input-budget test for the MC3 XML reader.
//
// Tessellation counts (segments / sides / subdivisions) come from untrusted
// input and drive geometry allocation. A hostile <sphere segments="100000000"/>
// would request ~5e15 vertices. The parser clamps these to a safe upper bound
// (kMaxTessellation == 4096); this test asserts nothing above the cap survives
// and that legitimate values are preserved unchanged.

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

static const Mc3Object* findByName(const Mc3Document& doc, const std::string& n) {
    for (const auto& o : doc.objects) if (o && o->name == n) return o.get();
    return nullptr;
}

static Mc3Document load(const char* xml) {
    auto path = std::filesystem::temp_directory_path() / "mc3_input_budget_test.mc3.xml";
    { std::ofstream f(path); f << xml; }
    Mc3Document doc = Mc3Document::loadFromFile(path);
    std::filesystem::remove(path);
    return doc;
}

int main() {
    const int kMaxTess = 4096;  // must match Mc3XmlParser.cpp kMaxTessellation

    // Hostile counts must be clamped, not passed through.
    {
        Mc3Document doc = load(
            "<mc3 version=\"0.3\" model=\"budget\">\n"
            "  <objects>\n"
            "    <sphere name=\"huge\" radius=\"1\" segments=\"100000000\"/>\n"
            "    <grid name=\"grid\" subdivisions_x=\"999999\" subdivisions_z=\"888888\"/>\n"
            "    <sphere name=\"neg\" radius=\"1\" segments=\"-5\"/>\n"
            "  </objects>\n"
            "</mc3>\n");

        const Mc3Object* huge = findByName(doc, "huge");
        check(huge && huge->primitive, "huge sphere parsed");
        if (huge && huge->primitive)
            check(huge->primitive->segments <= kMaxTess,
                  "segments=100000000 clamped to <= " + std::to_string(kMaxTess));

        const Mc3Object* grid = findByName(doc, "grid");
        check(grid && grid->primitive, "grid parsed");
        if (grid && grid->primitive) {
            check(grid->primitive->subdivisionsX <= kMaxTess,
                  "subdivisions_x clamped to <= " + std::to_string(kMaxTess));
            check(grid->primitive->subdivisionsZ <= kMaxTess,
                  "subdivisions_z clamped to <= " + std::to_string(kMaxTess));
        }

        const Mc3Object* neg = findByName(doc, "neg");
        check(neg && neg->primitive, "neg sphere parsed");
        if (neg && neg->primitive)
            check(neg->primitive->segments >= 0,
                  "negative segments clamped to >= 0");
    }

    // Legitimate values must survive unchanged.
    {
        Mc3Document doc = load(
            "<mc3 version=\"0.3\" model=\"ok\">\n"
            "  <objects>\n"
            "    <sphere name=\"ok\" radius=\"1\" segments=\"48\"/>\n"
            "  </objects>\n"
            "</mc3>\n");
        const Mc3Object* ok = findByName(doc, "ok");
        check(ok && ok->primitive, "ok sphere parsed");
        if (ok && ok->primitive)
            check(ok->primitive->segments == 48, "legitimate segments=48 preserved");
    }

    if (failures == 0) { std::cout << "All input-budget tests passed.\n"; return 0; }
    std::cerr << failures << " input-budget test(s) failed.\n";
    return 1;
}
