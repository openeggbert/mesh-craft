// Mc3LoadPolicy test: untrusted content must not process <include>, and
// include-confinement must reject path traversal.
//
// The AI/import path (AiResponseAlgorithms.hpp parseXmlAlg) parses model output
// with Mc3LoadPolicy::untrusted(), which disables <include> so a hostile
// response cannot make the loader open and merge arbitrary local files.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3LoadPolicy.hpp>

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
    fs::path dir = fs::temp_directory_path() / "mc3_load_policy_test";
    fs::create_directories(dir);

    // A "secret" file whose definition would be merged if the include is honored.
    const char* secret =
        "<mc3 version=\"0.3\" model=\"secret\">\n"
        "  <definitions>\n"
        "    <definition id=\"leaked\"><box name=\"b\"/></definition>\n"
        "  </definitions>\n"
        "</mc3>\n";
    { std::ofstream f(dir / "secret.mc3.xml"); f << secret; }

    const std::string mainXml =
        "<mc3 version=\"0.3\" model=\"main\">\n"
        "  <include file=\"secret.mc3.xml\"/>\n"
        "  <objects><box name=\"visible\"/></objects>\n"
        "</mc3>\n";
    { std::ofstream f(dir / "main.mc3.xml"); f << mainXml; }

    // Trusted (default) file load: include IS processed, definition merged.
    {
        Mc3Document doc = Mc3Document::loadFromFile(dir / "main.mc3.xml");
        check(doc.definitions.count("leaked") == 1,
              "trusted loadFromFile processes <include> (definition merged)");
    }

    // Untrusted file load: include is skipped, definition NOT merged, but the
    // rest of the document still parses.
    {
        Mc3Document doc = Mc3Document::loadFromFile(dir / "main.mc3.xml",
                                                    Mc3LoadPolicy::untrusted());
        check(doc.definitions.count("leaked") == 0,
              "untrusted loadFromFile ignores <include> (no merge)");
        bool hasVisible = false;
        for (const auto& o : doc.objects) if (o && o->name == "visible") hasVisible = true;
        check(hasVisible, "untrusted load still parses the rest of the document");
    }

    // In-memory parse (the AI path) defaults to untrusted: include ignored.
    {
        Mc3Document doc = Mc3Document::loadFromString(mainXml, dir);
        check(doc.definitions.count("leaked") == 0,
              "loadFromString (untrusted default) ignores <include>");
    }

    // Confinement: an include that escapes the document root via `..` is
    // rejected when confineIncludesToRoot is set (with includes still enabled).
    {
        const std::string escapeXml =
            "<mc3 version=\"0.3\" model=\"escape\">\n"
            "  <include file=\"../secret.mc3.xml\"/>\n"
            "  <objects><box name=\"x\"/></objects>\n"
            "</mc3>\n";
        fs::create_directories(dir / "sub");
        { std::ofstream f(dir / "sub" / "escape.mc3.xml"); f << escapeXml; }

        Mc3LoadPolicy confined;           // includes enabled, but confined to root
        confined.allowIncludes = true;
        confined.confineIncludesToRoot = true;

        bool threw = false;
        try {
            Mc3Document::loadFromFile(dir / "sub" / "escape.mc3.xml", confined);
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "confined policy rejects a `..`-escaping <include>");
    }

    // Regression: doc.sourcePath (rootDir) is EMPTY when the document is opened
    // via a bare relative filename with no directory component -- the common
    // invocation when running from the scene's own directory. weakly_canonical
    // of an empty path returns an empty path rather than resolving to the
    // current working directory, so includePathWithinRoot() used to treat that
    // as "root is empty, relative-to-empty is empty, empty looks like an
    // escape" and wrongly rejected every same-directory include under a
    // confined policy. Reproduce by chdir'ing into `dir` and opening
    // "main.mc3.xml" (no directory prefix) directly.
    {
        fs::path cwd = fs::current_path();
        fs::current_path(dir);

        Mc3LoadPolicy confined;
        confined.allowIncludes = true;
        confined.confineIncludesToRoot = true;

        bool threw = false;
        std::string what;
        try {
            Mc3Document doc = Mc3Document::loadFromFile("main.mc3.xml", confined);
            check(doc.definitions.count("leaked") == 1,
                  "confined policy + bare relative filename still merges a "
                  "same-directory include (empty-basePath regression)");
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(!threw, std::string("confined policy does not reject a same-directory "
              "include when opened via a bare relative filename") +
              (threw ? (": " + what) : ""));

        fs::current_path(cwd);
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All load-policy tests passed.\n"; return 0; }
    std::cerr << failures << " load-policy test(s) failed.\n";
    return 1;
}
