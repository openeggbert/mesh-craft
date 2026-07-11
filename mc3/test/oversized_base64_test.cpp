// SYS-W1-04 — oversized base64 (inline <embed> body).
//
// Investigation: no first-party code anywhere decodes Mc3EmbedGltf::
// base64Content today (SYS-W14-05 "safe embed: mesh/resource support" is
// still [DEFERRED]) -- but the raw base64 text is fully materialized as a
// std::string at PARSE time regardless of whether anything later decodes
// it, and Mc3XmlParser.cpp had no size ceiling on that field at all.
// Empirically confirmed before this fix: a 20MB inline <embed> body loaded
// with no error in ~120ms, and nothing stops that from being 2GB instead --
// a straightforward memory-exhaustion DoS from untrusted input, the same
// class of bug AUD-005/AUD-059 already fixed for tessellation counts. The
// fix adds a parse-time sanity ceiling (kMaxEmbedBase64Length, 64MB of
// base64 text) mirroring the MCB reader's existing kMcbMaxStringLen
// philosophy: no legitimate embedded prop/mesh needs anywhere near that
// much inline text.

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <chrono>
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

static Mc3Document loadXml(const fs::path& dir, const std::string& name, const std::string& xml) {
    fs::path path = dir / name;
    { std::ofstream f(path, std::ios::binary); f << xml; }
    Mc3Document doc = Mc3Document::loadFromFile(path);
    fs::remove(path);
    return doc;
}

int main() {
    fs::path dir = fs::temp_directory_path() / "mc3_oversized_base64_test";
    fs::create_directories(dir);

    // A small, legitimate inline embed must load fine and preserve content
    // exactly (this is the non-hostile baseline the new ceiling must not break).
    {
        std::string xml =
            "<mc3 version=\"0.3\" model=\"small\">\n"
            "  <embeds><embed type=\"gltf\" id=\"e\">Z2xURgIAAAA=</embed></embeds>\n"
            "  <objects><box name=\"b\"/></objects>\n"
            "</mc3>\n";
        Mc3Document doc = loadXml(dir, "small.mc3.xml", xml);
        check(doc.embeds.count("e") == 1, "small embed: entry present");
        if (doc.embeds.count("e"))
            check(doc.embeds["e"].base64Content == "Z2xURgIAAAA=",
                  "small embed: base64Content preserved exactly");
    }

    // A base64 body well past the 64MB ceiling must be rejected at parse
    // time, quickly, with a clear error -- not silently accepted into memory.
    {
        const size_t oversized = 70ull * 1024ull * 1024ull; // 70MB > 64MB ceiling
        std::string huge(oversized, 'A');
        std::string xml =
            "<mc3 version=\"0.3\" model=\"huge\">\n"
            "  <embeds><embed type=\"gltf\" id=\"e\">" + huge + "</embed></embeds>\n"
            "  <objects><box name=\"b\"/></objects>\n"
            "</mc3>\n";
        huge.clear(); huge.shrink_to_fit(); // don't hold two 70MB copies longer than needed

        fs::path path = dir / "huge.mc3.xml";
        { std::ofstream f(path, std::ios::binary); f << xml; }
        xml.clear(); xml.shrink_to_fit();

        auto t0 = std::chrono::steady_clock::now();
        bool threw = false;
        std::string what;
        Mc3Document doc;
        try {
            doc = Mc3Document::loadFromFile(path);
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        fs::remove(path);

        check(threw, "oversized embed: a 70MB inline base64 body is rejected, not "
                     "silently accepted");
        check(what.find("base64Content") != std::string::npos &&
              what.find("sanity limit") != std::string::npos,
              std::string("oversized embed: error names the field and the sanity "
              "limit (got: ") + what + ")");
        check(elapsedMs < 5000,
              "oversized embed: rejected promptly, not after minutes of "
              "processing the oversized payload");
        check(doc.embeds.empty(), "oversized embed: rejected before being stored in doc.embeds");
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All oversized-base64 tests passed.\n"; return 0; }
    std::cerr << failures << " oversized-base64 test(s) failed.\n";
    return 1;
}
