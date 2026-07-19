// AUD-068: Mc3JsonParser::parseString() used to entirely ignore its own
// Mc3LoadPolicy parameter (`const Mc3LoadPolicy& /*policy*/`) --
// confineResourcePathsToRoot was a silent no-op on the .mc3.json load
// path, so an untrusted document could name an absolute or `..`-escaping
// meshSource/texture uri/SVG src/embed src/sound src/music src and it
// would pass straight through unvalidated. Mirrors load_policy_test.cpp's
// (the XML equivalent) resource-confinement coverage for every one of the
// 6 fields.
//
// Deliberately NOT covered here (by design, not an oversight): the
// XML test's <include>-processing/confinement cases. .mc3.json's own
// `includes` field is parsed as an inert flat string list and never
// resolved/merged into another file's content anywhere in this codebase
// -- there is no local-file-inclusion vector on the JSON path to test.

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
    fs::path dir = fs::temp_directory_path() / "mc3_json_load_policy_test";
    fs::create_directories(dir);

    // AUD-006b-style coverage: untrusted content must not gain local-file
    // access merely by using a texture/mesh/SVG/sound/music/embed field.
    // Each case is deliberately a different field, wired in separately at
    // each parse site.
    auto checkFieldConfined = [&](const std::string& objectsJson, const char* label) {
        std::string js =
            "{\"version\":\"0.3\",\"model\":\"resource-escape\"," + objectsJson + "}";
        bool threw = false;
        std::string what;
        try {
            Mc3Document::loadFromJsonString(js, dir); // untrusted() default
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(threw, std::string("untrusted policy rejects an absolute ") + label + " path");
    };

    checkFieldConfined(
        R"("objects":[{"id":"m","name":"m","type":"mesh","meshSource":"/etc/passwd"}])",
        "mesh source");
    checkFieldConfined(
        R"("textures":[{"id":"t","uri":"/etc/passwd"}],)"
        R"("objects":[{"id":"b","name":"b","type":"box"}])",
        "texture uri");
    checkFieldConfined(
        R"("textures":[{"id":"s","type":"svg","src":"/etc/passwd"}],)"
        R"("objects":[{"id":"b","name":"b","type":"box"}])",
        "SVG texture src");
    checkFieldConfined(
        R"("sounds":[{"id":"snd","src":"/etc/passwd"}],)"
        R"("objects":[{"id":"b","name":"b","type":"box"}])",
        "sound src");
    checkFieldConfined(
        R"("music":[{"id":"trk","src":"/etc/passwd"}],)"
        R"("objects":[{"id":"b","name":"b","type":"box"}])",
        "music src");
    checkFieldConfined(
        R"("embeds":[{"id":"e","src":"/etc/passwd"}],)"
        R"("objects":[{"id":"b","name":"b","type":"box"}])",
        "embed src");

    // A `..`-escaping RELATIVE path must also be rejected, not just an
    // absolute one -- the same confinement covers both escape shapes.
    {
        std::string js =
            R"({"version":"0.3","model":"escape-rel",)"
            R"("objects":[{"id":"m","name":"m","type":"mesh","meshSource":"../../etc/passwd"}]})";
        bool threw = false;
        try {
            Mc3Document::loadFromJsonString(js, dir / "sub");
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "untrusted policy rejects a `..`-escaping relative mesh source");
    }

    // A trusted (permissive) load of the exact same absolute-path document
    // must NOT reject it -- confinement is opt-in via the policy, not a
    // universal restriction that would break a legitimate local scene
    // referencing a texture elsewhere on disk.
    {
        std::string js =
            R"({"version":"0.3","model":"trusted-abs",)"
            R"("textures":[{"id":"t","uri":"/etc/hostname"}],)"
            R"("objects":[{"id":"b","name":"b","type":"box"}]})";
        bool threw = false;
        try {
            Mc3Document::loadFromJsonString(js, dir, Mc3LoadPolicy::trusted());
        } catch (const std::exception&) {
            threw = true;
        }
        check(!threw, "trusted policy does not reject an absolute texture uri "
              "(confinement is opt-in, not universal)");
    }

    // `embed:<id>` is a pseudo-reference (resolved against doc.embeds, not
    // the filesystem) and must never be treated as an escaping path.
    {
        std::string js =
            R"({"version":"0.3","model":"embed-ref",)"
            R"("objects":[{"id":"m","name":"m","type":"mesh","meshSource":"embed:someId"}]})";
        bool threw = false;
        try {
            Mc3Document::loadFromJsonString(js, dir); // untrusted() default
        } catch (const std::exception&) {
            threw = true;
        }
        check(!threw, "untrusted policy does not reject an embed: pseudo-reference");
    }

    // Regression parity with load_policy_test.cpp's own empty-basePath fix:
    // an empty sourceDir (bare relative filename, no directory component)
    // must not make every relative path look like it "escapes" an empty
    // root, at least for a resource that actually exists at that location
    // (chdir'd into `dir`, matching the XML test's own precedent for this
    // exact regression).
    {
        fs::path cwd = fs::current_path();
        fs::current_path(dir);
        { std::ofstream f(dir / "model.obj"); f << "o Cube\n"; }

        std::string js =
            R"({"version":"0.3","model":"same-dir",)"
            R"("objects":[{"id":"m","name":"m","type":"mesh","meshSource":"model.obj"}]})";
        bool threw = false;
        std::string what;
        try {
            Mc3Document::loadFromJsonString(js, /*sourceDir=*/{});
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(!threw, std::string("untrusted policy + empty sourceDir does not reject a "
              "same-directory relative mesh source that actually exists on disk") +
              (threw ? (": " + what) : ""));

        fs::current_path(cwd);
    }

    // AUD-069: same empty-sourceDir scenario as immediately above, but for
    // a resource path that does NOT exist on disk at all -- deliberately
    // not creating dir/missing.obj. Previously the confinement check's
    // weakly_canonical()-based resolution only resolved a relative
    // candidate correctly when the referenced path actually existed;
    // a genuinely same-directory but non-existent relative reference under
    // an empty sourceDir was wrongly rejected as "escaping the root"
    // (fails safe -- overly strict, not a bypass -- but a real usability
    // gap). Fixed by making the candidate absolute before canonicalizing it
    // (see includePathWithinRoot() in both Mc3XmlParser.cpp and this file's
    // own copy), so resolution no longer depends on the target existing.
    {
        fs::path cwd = fs::current_path();
        fs::current_path(dir);

        std::string js =
            R"({"version":"0.3","model":"same-dir-missing",)"
            R"("objects":[{"id":"m","name":"m","type":"mesh","meshSource":"missing.obj"}]})";
        bool threw = false;
        std::string what;
        try {
            Mc3Document::loadFromJsonString(js, /*sourceDir=*/{});
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(!threw, std::string("AUD-069: untrusted policy + empty sourceDir does not "
              "reject a same-directory relative mesh source that does NOT exist on disk") +
              (threw ? (": " + what) : ""));

        fs::current_path(cwd);
    }

    // `doc.includes` stays populated as an inert flat string list under an
    // untrusted policy -- confirms this fix did not (and, per its own
    // scope, should not) touch that field's behavior.
    {
        std::string js =
            R"({"version":"0.3","model":"includes-passthrough",)"
            R"("includes":["lib.mc3.json"],)"
            R"("objects":[{"id":"b","name":"b","type":"box"}]})";
        Mc3Document doc = Mc3Document::loadFromJsonString(js, dir); // untrusted() default
        check(doc.includes.size() == 1 && doc.includes[0] == "lib.mc3.json",
              "untrusted policy leaves doc.includes as an unresolved passthrough list");
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All JSON load-policy tests passed.\n"; return 0; }
    std::cerr << failures << " JSON load-policy test(s) failed.\n";
    return 1;
}
