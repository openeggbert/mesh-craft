// SYS-W1-04 — include bombs (depth and fan-out; cycles are already covered
// by mc3_roundtrip_test's include_cycle_a/b.mc3.xml fixture, so not
// duplicated here).
//
// Investigation found two of the three sub-hazards already bounded:
//  - Cycles (A includes B includes A): already rejected via the inProgress
//    canonical-path set in Mc3XmlParser.cpp's mergeInclude() ("Cyclic
//    <include> detected"). Covered by mc3_roundtrip.
//  - Depth (a long linear A->B->C->...->Z chain): already bounded by
//    Mc3LoadPolicy::maxIncludeDepth (default 16 for trusted(), 0 -- i.e. no
//    includes at all -- for untrusted()), enforced in processIncludes().
//    Exercised below for completeness (this specific hazard had no test).
//  - Fan-out (one document directly <include>-ing thousands of distinct,
//    non-cyclic sibling files): NOT bounded prior to this task. Empirically
//    confirmed: 1500 trivial sibling includes merged successfully with no
//    error at all. Fixed by adding a document-wide include-count budget
//    (DocumentBudget::chargeInclude / kMaxTotalIncludes = 1000) mirroring
//    the existing per-document object/tessellation budgets (AUD-059).
//    Exercised below.

#include <MeshCraft/Mc3/Mc3Document.hpp>

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

// Writes a linear include chain file0 -> file1 -> ... -> file{depth-1}, each
// file including the next, the last one containing a distinctive material id
// so a caller can confirm the whole chain was actually merged.
static void writeLinearChain(const fs::path& dir, int depth) {
    for (int i = 0; i < depth; ++i) {
        fs::path p = dir / ("chain" + std::to_string(i) + ".mc3.xml");
        std::ofstream f(p);
        f << "<mc3 version=\"0.3\" model=\"chain" << i << "\">\n";
        if (i + 1 < depth)
            f << "  <include file=\"chain" << (i + 1) << ".mc3.xml\"/>\n";
        else
            f << "  <materials><material id=\"deepest\"/></materials>\n";
        f << "</mc3>\n";
    }
}

int main() {
    fs::path dir = fs::temp_directory_path() / "mc3_include_bomb_test";
    fs::create_directories(dir);

    // --- Depth: within the default policy's maxIncludeDepth (16) must load fine. ---
    {
        fs::path sub = dir / "shallow";
        fs::create_directories(sub);
        const int depth = 10; // comfortably under the default cap of 16
        writeLinearChain(sub, depth);

        bool threw = false;
        std::string what;
        Mc3Document doc;
        try {
            doc = Mc3Document::loadFromFile(sub / "chain0.mc3.xml");
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(!threw, std::string("shallow chain (depth=") + std::to_string(depth) +
              "): loads without throwing (got: " + what + ")");
        check(!threw && doc.materials.count("deepest") == 1,
              "shallow chain: the deepest included material is actually merged "
              "(proves the whole chain was followed, not just accepted trivially)");
        fs::remove_all(sub);
    }

    // --- Depth: a chain deeper than maxIncludeDepth (16) must be rejected
    // cleanly, not recurse unbounded or overflow the stack. ---
    {
        fs::path sub = dir / "deep";
        fs::create_directories(sub);
        const int depth = 40; // well past the default cap of 16
        writeLinearChain(sub, depth);

        bool threw = false;
        std::string what;
        Mc3Document doc;
        try {
            doc = Mc3Document::loadFromFile(sub / "chain0.mc3.xml");
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(threw, "deep chain (depth=40): exceeding the policy's include-depth "
              "cap is rejected, not followed unbounded");
        check(what.find("nesting") != std::string::npos ||
              what.find("depth") != std::string::npos,
              std::string("deep chain: error names the depth/nesting limit (got: ") + what + ")");
        fs::remove_all(sub);
    }

    // --- Fan-out: a modest number of distinct sibling includes (well under
    // the 1000 budget) must load fine, all merged. ---
    {
        fs::path sub = dir / "fanout_ok";
        fs::create_directories(sub);
        const int n = 50;
        std::string mainXml = "<mc3 version=\"0.3\" model=\"fanout_ok\">\n";
        for (int i = 0; i < n; ++i) {
            fs::path p = sub / ("f" + std::to_string(i) + ".mc3.xml");
            std::ofstream f(p);
            f << "<mc3 version=\"0.3\" model=\"f" << i << "\">\n"
              << "  <materials><material id=\"m" << i << "\"/></materials>\n"
              << "</mc3>\n";
            mainXml += "  <include file=\"f" + std::to_string(i) + ".mc3.xml\"/>\n";
        }
        mainXml += "</mc3>\n";
        fs::path mainPath = sub / "main.mc3.xml";
        { std::ofstream f(mainPath); f << mainXml; }

        bool threw = false;
        Mc3Document doc;
        try {
            doc = Mc3Document::loadFromFile(mainPath);
        } catch (const std::exception&) {
            threw = true;
        }
        check(!threw, "fan-out (n=50, under budget): loads without throwing");
        check(!threw && doc.materials.size() == static_cast<size_t>(n),
              "fan-out (n=50): all 50 distinct included materials are merged");
        fs::remove_all(sub);
    }

    // --- Fan-out: exceeding the 1000-include budget must be rejected
    // cleanly (not silently accepted, not a slow unbounded merge). ---
    {
        fs::path sub = dir / "fanout_bomb";
        fs::create_directories(sub);
        const int n = 1001; // just over DocumentBudget::kMaxTotalIncludes (1000)
        std::string mainXml = "<mc3 version=\"0.3\" model=\"fanout_bomb\">\n";
        for (int i = 0; i < n; ++i) {
            fs::path p = sub / ("g" + std::to_string(i) + ".mc3.xml");
            std::ofstream f(p);
            f << "<mc3 version=\"0.3\" model=\"g" << i << "\">\n"
              << "  <materials><material id=\"gm" << i << "\"/></materials>\n"
              << "</mc3>\n";
            mainXml += "  <include file=\"g" + std::to_string(i) + ".mc3.xml\"/>\n";
        }
        mainXml += "</mc3>\n";
        fs::path mainPath = sub / "main.mc3.xml";
        { std::ofstream f(mainPath); f << mainXml; }

        bool threw = false;
        std::string what;
        Mc3Document doc;
        try {
            doc = Mc3Document::loadFromFile(mainPath);
        } catch (const std::exception& e) {
            threw = true;
            what = e.what();
        }
        check(threw, "fan-out bomb (n=1001): exceeding the include-count budget is rejected");
        check(what.find("include") != std::string::npos &&
              what.find("budget") != std::string::npos,
              std::string("fan-out bomb: error names the include budget (got: ") + what + ")");
        fs::remove_all(sub);
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All include-bomb tests passed.\n"; return 0; }
    std::cerr << failures << " include-bomb test(s) failed.\n";
    return 1;
}
