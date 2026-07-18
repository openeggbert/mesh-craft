// AUD-070: Mc3XmlWriter.cpp wrote SVG inlineContent, script source, and
// embed base64Content into a CDATA section unconditionally
// (SetCData(true)), with no scan for an embedded "]]>" -- a single CDATA
// section cannot contain its own closing delimiter. Content containing
// "]]>" followed by attacker-chosen text (e.g. an AI-generated <script>
// with that sequence in its source, or a pasted SVG whose own markup
// contains a CDATA section) closed the CDATA early and let whatever
// followed be parsed as literal XML markup on the next load -- a
// document-structure injection, not just garbled content: extra elements
// could appear as real children of the document, and the original string
// would be silently truncated on round-trip.
//
// Proves the fix by round-tripping a document whose script source and SVG
// inline content each contain "]]>" followed by a fake, distinctively-
// named injected element, and asserting (a) the string comes back byte-
// for-byte identical, and (b) no such element exists anywhere in the
// reloaded document tree -- either failure would mean the "]]>" broke out
// of the CDATA section.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3LoadPolicy.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/Mc3/Mc3Script.hpp>
#include <MeshCraft/Mc3/Mc3SvgTexture.hpp>

#include <filesystem>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

// Recursively confirms no object anywhere in the tree (or its id/name/type
// string fields -- not applicable here, only object presence matters) was
// spuriously created with this name, proving no injected element became a
// real document node.
static bool anyObjectNamed(const std::vector<std::shared_ptr<Mc3Object>>& objs,
                            const std::string& name) {
    for (const auto& o : objs) {
        if (!o) continue;
        if (o->name == name || o->id == name) return true;
        if (anyObjectNamed(o->children, name)) return true;
    }
    return false;
}

int main() {
    fs::path dir = fs::temp_directory_path() / "mc3_cdata_injection_test";
    fs::create_directories(dir);
    fs::path path = dir / "cdata_injection.mc3.xml";

    const std::string injectedMarker = "injected_by_cdata_attack";
    const std::string scriptSource =
        "local x = 1]]><object id=\"" + injectedMarker + "\" name=\"" +
        injectedMarker + "\" type=\"box\"/><more_evil>]]>tail";
    const std::string svgContent =
        "<svg]]><object id=\"svg_" + injectedMarker + "\" name=\"svg_" +
        injectedMarker + "\" type=\"box\"/></svg>";

    {
        Mc3Document doc;
        doc.model = "CdataInjectionTest";

        Mc3Script sc;
        sc.id = "s1";
        sc.type = "lua";
        sc.source = scriptSource;
        doc.scripts[sc.id] = sc;

        Mc3SvgTexture svg;
        svg.id = "svg1";
        svg.inlineContent = svgContent;
        doc.svgTextures[svg.id] = svg;

        auto root = std::make_shared<Mc3Object>();
        root->id = "root"; root->name = "Root"; root->type = ObjectType::Box;
        doc.objects.push_back(root);

        doc.saveToFile(path);
    }

    {
        Mc3Document doc = Mc3Document::loadFromFile(path, Mc3LoadPolicy::trusted());

        check(doc.scripts.count("s1") == 1, "script s1 survives round-trip");
        if (doc.scripts.count("s1")) {
            check(doc.scripts.at("s1").source == scriptSource,
                  "script source round-trips byte-for-byte identical, including "
                  "the embedded ]]> (proves the CDATA was not broken out of)");
        }

        check(doc.svgTextures.count("svg1") == 1, "SVG texture svg1 survives round-trip");
        if (doc.svgTextures.count("svg1")) {
            check(doc.svgTextures.at("svg1").inlineContent == svgContent,
                  "SVG inline content round-trips byte-for-byte identical, "
                  "including the embedded ]]>");
        }

        check(!anyObjectNamed(doc.objects, injectedMarker),
              "no spurious object was created from the script's embedded ]]> content "
              "(the injected <object .../> text stayed literal, not real XML)");
        check(!anyObjectNamed(doc.objects, "svg_" + injectedMarker),
              "no spurious object was created from the SVG's embedded ]]> content");

        check(doc.objects.size() == 1 && doc.objects[0]->name == "Root",
              "exactly the one legitimate Root object exists -- no extra "
              "top-level nodes were injected by either ]]> payload");
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All CDATA-injection tests passed.\n"; return 0; }
    std::cerr << failures << " CDATA-injection test(s) failed.\n";
    return 1;
}
