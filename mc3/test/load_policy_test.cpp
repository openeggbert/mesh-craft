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

    // AUD-006b: untrusted content must not gain local-file access merely by
    // using a texture/mesh/SVG/sound/music/embed path instead of <include> --
    // the same root-escape confinement now applies to every filesystem-
    // reference field, not just <include>. Each case is deliberately a
    // DIFFERENT field, since the check is wired in separately at each parse
    // site (there is no single shared code path that would make fixing one
    // automatically fix the rest).
    auto checkFieldConfined = [&](const std::string& fieldXml, const char* label) {
        std::string xml =
            "<mc3 version=\"0.3\" model=\"resource-escape\">\n" + fieldXml + "</mc3>\n";
        bool threw = false;
        std::string what;
        try {
            Mc3Document::loadFromString(xml, dir); // untrusted() default
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(threw, std::string("untrusted policy rejects an absolute ") + label + " path");
    };

    checkFieldConfined(
        "  <objects><mesh name=\"m\" src=\"/etc/passwd\"/></objects>\n",
        "mesh src");
    checkFieldConfined(
        "  <textures><texture id=\"t\" uri=\"/etc/passwd\"/></textures>\n"
        "  <objects><box name=\"b\"/></objects>\n",
        "texture uri");
    checkFieldConfined(
        "  <textures><texture id=\"s\" type=\"svg\" src=\"/etc/passwd\"/></textures>\n"
        "  <objects><box name=\"b\"/></objects>\n",
        "SVG texture src");
    checkFieldConfined(
        "  <sounds><sound id=\"snd\" src=\"/etc/passwd\"/></sounds>\n"
        "  <objects><box name=\"b\"/></objects>\n",
        "sound src");
    checkFieldConfined(
        "  <music><track id=\"trk\" src=\"/etc/passwd\"/></music>\n"
        "  <objects><box name=\"b\"/></objects>\n",
        "music src");
    checkFieldConfined(
        "  <embeds><embed id=\"e\" type=\"gltf\" src=\"/etc/passwd\"/></embeds>\n"
        "  <objects><box name=\"b\"/></objects>\n",
        "embed src");

    // A trusted (permissive) load of the exact same absolute-path documents
    // must NOT reject them -- confinement is opt-in via the policy, not a
    // universal restriction that would break a legitimate local scene
    // referencing a texture elsewhere on disk.
    {
        std::string xml =
            "<mc3 version=\"0.3\" model=\"trusted-abs\">\n"
            "  <textures><texture id=\"t\" uri=\"/etc/hostname\"/></textures>\n"
            "  <objects><box name=\"b\"/></objects>\n"
            "</mc3>\n";
        bool threw = false;
        try {
            Mc3Document::loadFromString(xml, dir, Mc3LoadPolicy::trusted());
        } catch (const std::exception&) {
            threw = true;
        }
        check(!threw, "trusted policy does not reject an absolute texture uri "
              "(confinement is opt-in, not universal)");
    }

    // `embed:<id>` is a pseudo-reference (resolved against doc.embeds, not the
    // filesystem) and must never be treated as an escaping path.
    {
        std::string xml =
            "<mc3 version=\"0.3\" model=\"embed-ref\">\n"
            "  <objects><mesh name=\"m\" src=\"embed:someId\"/></objects>\n"
            "</mc3>\n";
        bool threw = false;
        try {
            Mc3Document::loadFromString(xml, dir); // untrusted() default
        } catch (const std::exception&) {
            threw = true;
        }
        check(!threw, "untrusted policy does not reject an embed: pseudo-reference");
    }

    // AUD-069: same empty-basePath scenario as the earlier <include>
    // regression, but for a resource path (mesh src) under
    // confineResourcePathsToRoot (not confineIncludesToRoot) that does NOT
    // exist on disk -- deliberately not creating dir/model.obj at all.
    {
        const std::string missingXml =
            "<mc3 version=\"0.3\" model=\"missing-resource\">\n"
            "  <objects><mesh name=\"m\" src=\"model.obj\"/></objects>\n"
            "</mc3>\n";
        { std::ofstream f(dir / "missing_resource.mc3.xml"); f << missingXml; }

        fs::path cwd = fs::current_path();
        fs::current_path(dir);

        Mc3LoadPolicy confined;
        confined.confineResourcePathsToRoot = true;

        bool threw = false;
        std::string what;
        try {
            Mc3Document::loadFromFile("missing_resource.mc3.xml", confined);
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(!threw, std::string("AUD-069: confined policy does not reject a "
              "same-directory mesh src referencing a file that doesn't exist "
              "on disk, when opened via a bare relative filename") +
              (threw ? (": " + what) : ""));

        fs::current_path(cwd);
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All load-policy tests passed.\n"; return 0; }
    std::cerr << failures << " load-policy test(s) failed.\n";
    return 1;
}
