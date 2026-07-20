// 2026-07-20 audit finding #1 (P0): McbReader.cpp had no document-wide
// aggregate budget -- Mc3XmlParser.cpp/Mc3JsonParser.cpp both defend
// against a document with many cheap-but-numerous elements (not just one
// oversized field) via a DocumentBudget, but McbReader.cpp never got the
// same protection. kMcbMaxCollectionCount (10,000,000) bounds a SINGLE
// array/map's claimed count, but not the SUM across the whole document --
// and unlike reserve_bomb_test.cpp's "huge claimed count, truncated/no
// data" scenario, this is exploitable with REAL, cheap, present data: each
// "objects" array entry can be encoded as just 2 bytes (TAG_OBJ + a
// zero-length key immediately ending readObject()), so a file well under
// 1MB can claim and actually deliver enough entries to force construction
// of far more heap objects than any legitimate scene would ever need.
//
// This test constructs a MINIMAL amount of real (not truncated/missing)
// data that crosses the new kMaxTotalObjects budget (100,000) by exactly
// one, proving the aggregate cap -- not just the per-array cap -- is what
// rejects it. Also covers the tessellation-weight and material budgets
// (chargeTessellation/chargeMaterial), and confirms a legitimate small
// document still loads fine (no false-positive at ordinary scale).

#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Primitive.hpp"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mcb;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

namespace {

void appendU32(std::string& s, uint32_t v) {
    s.push_back(static_cast<char>(v & 0xFF));
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
    s.push_back(static_cast<char>((v >> 16) & 0xFF));
    s.push_back(static_cast<char>((v >> 24) & 0xFF));
}

// TAG_OBJ field keys (McbReader.cpp's rKey()): 1-byte length prefix,
// terminated by a zero-length-key sentinel (no count is written up front).
void appendKey(std::string& s, const std::string& k) {
    s.push_back(static_cast<char>(k.size()));
    s += k;
}

// TAG_MAP entry keys and TAG_STR values (McbReader.cpp's rRawStr()): 4-byte
// little-endian length prefix -- a DIFFERENT encoding from appendKey()
// above, used wherever the reader itself calls rRawStr() for a key (map
// entries) rather than rKey() (object fields).
void appendRawStr(std::string& s, const std::string& v) {
    appendU32(s, static_cast<uint32_t>(v.size()));
    s += v;
}

// Builds a minimal, otherwise-valid MCB stream: header + a document object
// whose sole field is "objects", an array of `count` REAL minimal object
// entries (each just TAG_OBJ + a zero-length-key sentinel -- 2 bytes,
// parses to an empty Mc3Object with no further fields).
std::string buildManyMinimalObjectsStream(uint32_t count) {
    std::string bytes;
    bytes.append(MCB_MAGIC, 4);
    bytes.push_back(static_cast<char>(MCB_VERSION));
    bytes.push_back(0); // flags: uncompressed
    bytes.push_back(0); bytes.push_back(0); // reserved
    bytes.push_back(static_cast<char>(TAG_OBJ)); // root document object

    appendKey(bytes, "objects");
    bytes.push_back(static_cast<char>(TAG_ARR));
    appendU32(bytes, count);
    for (uint32_t i = 0; i < count; ++i) {
        bytes.push_back(static_cast<char>(TAG_OBJ));
        bytes.push_back(0); // zero-length key -> readObject() ends immediately
    }
    bytes.push_back(0); // end root document object
    return bytes;
}

} // namespace

int main() {
    // --- 1. A document with real, present, cheap data crossing the object
    //        budget by exactly one entry is rejected -- not a false claim
    //        with missing data (reserve_bomb_test.cpp's scenario), but
    //        actual bytes the old per-collection-only cap would have
    //        accepted in full. ---
    {
        // kMaxTotalObjects is 100,000 (McbReader.cpp, mirrors
        // Mc3XmlParser.cpp's DocumentBudget) -- one past that must throw.
        const uint32_t kOverBudget = 100'001;
        std::string bytes = buildManyMinimalObjectsStream(kOverBudget);
        CHECK(bytes.size() < 300 * 1024,
              "the hostile-but-real-data file stays small (" +
              std::to_string(bytes.size()) + " bytes) despite " +
              std::to_string(kOverBudget) + " real object entries");

        std::istringstream in(bytes, std::ios::binary);
        bool threw = false;
        std::string what;
        try { loadFromBinary(in); }
        catch (const std::exception& e) { threw = true; what = e.what(); }
        CHECK(threw, "a document with " + std::to_string(kOverBudget) +
              " real (present, not truncated) minimal objects is rejected");
        CHECK(what.find("object budget") != std::string::npos,
              "rejection names the object budget specifically (got: " + what + ")");
    }

    // --- 2. Exactly at the budget ceiling (not one over) still loads --
    //        the fix must not be an off-by-one false-positive trap. ---
    {
        const uint32_t kAtBudget = 100'000;
        std::string bytes = buildManyMinimalObjectsStream(kAtBudget);
        std::istringstream in(bytes, std::ios::binary);
        bool threw = false;
        Mc3Document doc;
        try { doc = loadFromBinary(in); }
        catch (const std::exception&) { threw = true; }
        CHECK(!threw, "exactly kMaxTotalObjects (100,000) real objects still loads successfully");
        CHECK(!threw && doc.objects.size() == kAtBudget,
              "all " + std::to_string(kAtBudget) + " objects present after a successful load");
    }

    // --- 3. Tessellation-weight aggregate budget (chargeTessellation, now
    //        wired through all 6 of this reader's tessellation-driving
    //        fields via clampTessBudgeted) rejects many objects each with
    //        a legal-but-nonzero per-field tessellation value, once their
    //        SUM crosses kMaxTotalTessellationWeight (500,000) -- distinct
    //        from clampTess's existing per-field 4096 ceiling, which each
    //        individual value here stays well under. ---
    {
        Mc3Document doc;
        doc.model = "TessellationBudgetTest";
        // 200 spheres at segments=3000 each (under the 4096 per-field cap,
        // but 200*3000 = 600,000 > the 500,000 aggregate budget).
        for (int i = 0; i < 200; ++i) {
            auto sphere = std::make_shared<Mc3Object>();
            sphere->name = "Sphere" + std::to_string(i);
            sphere->type = ObjectType::Sphere;
            sphere->primitive = Mc3Primitive::sphere(1.0f, 3000);
            doc.objects.push_back(sphere);
        }
        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);

        std::istringstream in(out.str(), std::ios::binary);
        bool threw = false;
        std::string what;
        try { loadFromBinary(in); }
        catch (const std::exception& e) { threw = true; what = e.what(); }
        CHECK(threw, "a document whose summed tessellation weight (600,000) exceeds the "
              "500,000 aggregate budget is rejected, despite each individual segments "
              "value (3000) staying under the per-field 4096 cap");
        CHECK(what.find("tessellation") != std::string::npos,
              "rejection names the tessellation budget (got: " + what + ")");
    }

    // --- 4. Material budget (chargeMaterial, a map-based collection --
    //        proves the fix applies to doc.materials, not just doc.objects). ---
    {
        std::string bytes;
        bytes.append(MCB_MAGIC, 4);
        bytes.push_back(static_cast<char>(MCB_VERSION));
        bytes.push_back(0);
        bytes.push_back(0); bytes.push_back(0);
        bytes.push_back(static_cast<char>(TAG_OBJ));

        appendKey(bytes, "materials");
        bytes.push_back(static_cast<char>(TAG_MAP));
        const uint32_t kOverBudget = 20'001; // kMaxTotalMaterials is 20,000
        appendU32(bytes, kOverBudget);
        for (uint32_t i = 0; i < kOverBudget; ++i) {
            std::string key = "m" + std::to_string(i);
            appendRawStr(bytes, key); // TAG_MAP entry key -> rRawStr(), not rKey()
            bytes.push_back(static_cast<char>(TAG_OBJ));
            bytes.push_back(0); // empty material, ends immediately
        }
        bytes.push_back(0); // end root document object

        std::istringstream in(bytes, std::ios::binary);
        bool threw = false;
        std::string what;
        try { loadFromBinary(in); }
        catch (const std::exception& e) { threw = true; what = e.what(); }
        CHECK(threw, "a document with " + std::to_string(kOverBudget) +
              " real materials is rejected by the aggregate material budget");
        CHECK(what.find("material budget") != std::string::npos,
              "rejection names the material budget (got: " + what + ")");
    }

    // --- 5. An ordinary, legitimate document (far below every budget) is
    //        completely unaffected -- no false-positive at normal scale. ---
    {
        Mc3Document doc;
        doc.model = "OrdinaryScene";
        for (int i = 0; i < 25; ++i) {
            auto box = std::make_shared<Mc3Object>();
            box->name = "Box" + std::to_string(i);
            box->type = ObjectType::Box;
            box->primitive = Mc3Primitive::box({1, 1, 1});
            doc.objects.push_back(box);
        }
        doc.materials["mat_a"] = Mc3Material{};
        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        std::istringstream in(out.str(), std::ios::binary);
        bool threw = false;
        Mc3Document loaded;
        try { loaded = loadFromBinary(in); } catch (const std::exception&) { threw = true; }
        CHECK(!threw, "an ordinary 25-object, 1-material document still loads without error");
        CHECK(!threw && loaded.objects.size() == 25 && loaded.materials.size() == 1,
              "all objects/materials present after a successful load");
    }

    if (failures == 0)
        std::cout << "All MCB document-budget (2026-07-20 audit finding #1) tests passed.\n";
    else
        std::cerr << failures << " MCB document-budget test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
