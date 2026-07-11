// Hostile-numeric-input test for the MC3 XML reader.
//
// std::stof and sscanf("%f") both accept the textual forms "nan"/"inf"/"-inf"
// and return non-finite values WITHOUT throwing, so before finiteOr() a scene
// attribute like width="nan" or z="inf" flowed straight into the geometry
// pipeline. This test loads a document whose numeric attributes are all
// non-finite (or wildly out of range) and asserts that nothing non-finite
// survives into the parsed model.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <cmath>
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

static bool finite3(const std::array<float, 3>& v) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

// Non-finite anywhere in an object's transform/primitive subtree?
static void checkObjectFinite(const Mc3Object& o, const std::string& where) {
    check(finite3(o.transform.position), where + ".position finite");
    check(finite3(o.transform.rotation), where + ".rotation finite");
    check(finite3(o.transform.scale),    where + ".scale finite");
    if (o.primitive) {
        const auto& p = *o.primitive;
        check(std::isfinite(p.radius),      where + ".radius finite");
        check(std::isfinite(p.height),      where + ".height finite");
        check(std::isfinite(p.majorRadius), where + ".majorRadius finite");
        check(std::isfinite(p.minorRadius), where + ".minorRadius finite");
        check(finite3(p.size),              where + ".size finite");
    }
    for (const auto& c : o.children)
        if (c) checkObjectFinite(*c, where + "/child");
}

int main() {
    const char* xml =
        "<mc3 version=\"0.3\" model=\"hostile\">\n"
        "  <objects>\n"
        "    <box name=\"b\" position=\"nan inf -inf\" rotation=\"nan,nan,nan\"\n"
        "         scale=\"inf\" size=\"nan\"/>\n"
        "    <sphere name=\"s\" position=\"0 inf 0\" radius=\"nan\" segments=\"32\">\n"
        "      <torus name=\"t\" major_radius=\"inf\" minor_radius=\"nan\"/>\n"
        "    </sphere>\n"
        "    <cylinder name=\"c\" radius=\"1e999\" height=\"-inf\"/>\n"
        "  </objects>\n"
        "</mc3>\n";

    auto path = std::filesystem::temp_directory_path() / "mc3_finite_input_test.mc3.xml";
    { std::ofstream f(path); f << xml; }

    Mc3Document doc;
    bool loaded = true;
    try {
        doc = Mc3Document::loadFromFile(path);
    } catch (const std::exception& e) {
        loaded = false;
        std::cerr << "load threw: " << e.what() << "\n";
    }
    std::filesystem::remove(path);

    check(loaded, "hostile document loads without throwing");
    check(!doc.objects.empty(), "objects were parsed");
    for (const auto& o : doc.objects)
        if (o) checkObjectFinite(*o, o->name.empty() ? "<obj>" : o->name);

    if (failures == 0) { std::cout << "All finite-input tests passed.\n"; return 0; }
    std::cerr << failures << " finite-input test(s) failed.\n";
    return 1;
}
