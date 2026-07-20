// 2026-07-20 audit F3: McbReader.cpp/McbWriter.cpp had NO Mc3LoadPolicy
// integration at all -- loadFromFile()/loadFromBinary() took no policy
// parameter, so meshSource/texture uri/SVG src/embed src/sound src/music
// src were read completely unconfined, structurally unable to reject an
// absolute or `..`-escaping path the way Mc3XmlParser.cpp/Mc3JsonParser.cpp
// already do (mc3/test/load_policy_test.cpp, mc3/test/json_load_policy_test.cpp).
// Mirrors both of those test files' own coverage, adapted for MCB.

#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mcb;
namespace fs = std::filesystem;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

int main() {
    fs::path dir = fs::temp_directory_path() / "mcb_load_policy_test";
    fs::create_directories(dir);

    // --- Absolute mesh source: untrusted policy rejects, trusted does not ---
    {
        Mc3Document doc;
        doc.model = "abs-mesh";
        doc.objects.push_back(Mc3Object::makeMesh("m", "/etc/passwd"));

        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        std::string bytes = out.str();

        bool threwUntrusted = false;
        std::string what;
        try {
            std::istringstream in(bytes, std::ios::binary);
            loadFromBinary(in, Mc3LoadPolicy::untrusted());
        } catch (const std::exception& e) {
            threwUntrusted = true;
            what = e.what();
        }
        check(threwUntrusted, "untrusted policy rejects an absolute mesh source path");
        check(!threwUntrusted || what.find("absolute") != std::string::npos,
              "the rejection message names it as an absolute-path issue: " + what);

        bool threwTrusted = false;
        try {
            std::istringstream in(bytes, std::ios::binary);
            loadFromBinary(in); // default (trusted) policy
        } catch (const std::exception&) {
            threwTrusted = true;
        }
        check(!threwTrusted,
              "trusted (default) policy does not reject an absolute mesh source "
              "(confinement is opt-in, not universal)");
    }

    // --- `..`-escaping texture uri: untrusted policy rejects ---
    {
        Mc3Document doc;
        doc.model = "escape-tex";
        Mc3Texture tex;
        tex.name = "t";
        tex.uri  = "../../secret.png";
        doc.textures["t"] = tex;
        auto obj = Mc3Object::makeBox("b");
        obj->material = "";
        doc.objects.push_back(obj);

        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        std::string bytes = out.str();

        bool threw = false;
        std::string what;
        try {
            std::istringstream in(bytes, std::ios::binary);
            loadFromBinary(in, Mc3LoadPolicy::untrusted());
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(threw, "untrusted policy rejects a `..`-escaping texture uri");
        check(!threw || what.find("escapes") != std::string::npos,
              "the rejection message names it as escaping the root: " + what);
    }

    // --- Same-directory mesh source, bare relative filename (empty
    // sourceDir), target does NOT exist on disk: must NOT be rejected
    // (mirrors AUD-069's fix -- confirms McbReader's fresh
    // includePathWithinRoot() copy got that fix too, not just the original
    // bug it was mirrored from). ---
    {
        Mc3Document doc;
        doc.model = "same-dir-missing";
        doc.objects.push_back(Mc3Object::makeMesh("m", "missing.obj"));
        saveToFile(doc, dir / "scene.mcb");

        fs::path cwd = fs::current_path();
        fs::current_path(dir);

        bool threw = false;
        std::string what;
        try {
            loadFromFile("scene.mcb", Mc3LoadPolicy::untrusted());
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(!threw, std::string("untrusted policy + empty sourceDir does not reject a "
              "same-directory mesh source that does NOT exist on disk") +
              (threw ? (": " + what) : ""));

        fs::current_path(cwd);
    }

    // --- `embed:<id>` pseudo-reference must never be treated as an escaping path ---
    {
        Mc3Document doc;
        doc.model = "embed-ref";
        doc.objects.push_back(Mc3Object::makeMesh("m", "embed:someId"));

        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        std::string bytes = out.str();

        bool threw = false;
        try {
            std::istringstream in(bytes, std::ios::binary);
            loadFromBinary(in, Mc3LoadPolicy::untrusted());
        } catch (const std::exception&) {
            threw = true;
        }
        check(!threw, "untrusted policy does not reject an embed: pseudo-reference");
    }

    // --- A legitimate, same-directory resource that DOES exist must still
    // load fine under untrusted+confined policy. ---
    {
        { std::ofstream f(dir / "model.obj"); f << "o Cube\n"; }

        Mc3Document doc;
        doc.model = "same-dir-exists";
        doc.objects.push_back(Mc3Object::makeMesh("m", "model.obj"));
        saveToFile(doc, dir / "scene2.mcb");

        fs::path cwd = fs::current_path();
        fs::current_path(dir);

        bool threw = false;
        std::string what;
        try {
            loadFromFile("scene2.mcb", Mc3LoadPolicy::untrusted());
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(!threw, std::string("untrusted policy does not reject a same-directory "
              "mesh source that actually exists on disk") + (threw ? (": " + what) : ""));

        fs::current_path(cwd);
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All MCB load-policy tests passed.\n"; return 0; }
    std::cerr << failures << " MCB load-policy test(s) failed.\n";
    return 1;
}
