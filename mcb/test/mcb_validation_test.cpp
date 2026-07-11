// Mc3Validation test for the MCB reader (SYS-W1-01).
//
// mcb_roundtrip_test.cpp's testOutOfRangeEnumClampedToDefault (AUD-017) and
// its tag-mismatch tests (AUD-015) already prove the clamped/rejected VALUE
// is correct. This test proves the separate thing SYS-W1-01 adds: that a
// caller who passes an Mc3Validation& to McbReader's loadFromFile/
// loadFromBinary gets a structured diagnostic entry for each of those
// events, not just a silently-different result.

#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Primitive.hpp"
#include "MeshCraft/Mc3/Mc3Validation.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mcb;
namespace fs = std::filesystem;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

// Minimal hand-rolled byte writers, same as mcb_roundtrip_test.cpp's, needed
// to construct a byte-exact MCB stream with an out-of-range enum / mismatched
// tag -- something McbWriter itself would never emit.
static void rawU8(std::ostream& o, uint8_t v) { o.put(static_cast<char>(v)); }
static void rawU32(std::ostream& o, uint32_t v) {
    char b[4] = {
        static_cast<char>(v & 0xFF),
        static_cast<char>((v >> 8) & 0xFF),
        static_cast<char>((v >> 16) & 0xFF),
        static_cast<char>((v >> 24) & 0xFF),
    };
    o.write(b, 4);
}
static void rawStr(std::ostream& o, const std::string& s) {
    rawU32(o, static_cast<uint32_t>(s.size()));
    if (!s.empty()) o.write(s.data(), static_cast<std::streamsize>(s.size()));
}
static void rawKey(std::ostream& o, const std::string& k) {
    rawU8(o, static_cast<uint8_t>(k.size()));
    o.write(k.data(), static_cast<std::streamsize>(k.size()));
}
static void rawEnd(std::ostream& o) { rawU8(o, 0); }

static const Mc3ValidationEntry* findEntry(const Mc3Validation& v, const std::string& field) {
    for (const auto& e : v.entries) if (e.field == field) return &e;
    return nullptr;
}

int main() {
    // 1. AUD-017: an out-of-range enum value (correctly tagged TAG_I32, just
    // a nonsense int) must produce a WARNING entry naming the 'type' field,
    // in addition to the already-proven clamp-to-0 behavior.
    {
        std::ostringstream out(std::ios::binary);
        out.write(MCB_MAGIC, 4);
        rawU8(out, MCB_VERSION);
        rawU8(out, 0);
        rawU8(out, 0); rawU8(out, 0);
        rawU8(out, TAG_OBJ);
        rawKey(out, "objects"); rawU8(out, TAG_ARR); rawU32(out, 1);
        rawU8(out, TAG_OBJ);
            rawKey(out, "name"); rawU8(out, TAG_STR); rawStr(out, "Obj");
            rawKey(out, "type"); rawU8(out, TAG_I32); rawU32(out, 9999);
        rawEnd(out);
        rawEnd(out);

        std::istringstream in(out.str(), std::ios::binary);
        Mc3Validation validation;
        Mc3Document doc = loadFromBinary(in, validation);

        CHECK(doc.objects.size() == 1 && doc.objects[0]->type == ObjectType::Box,
              "value-level behavior unchanged: out-of-range type still clamps to Box");
        CHECK(validation.hasWarnings(), "an out-of-range enum produces at least one warning");
        const Mc3ValidationEntry* e = findEntry(validation, "type");
        CHECK(e != nullptr, "a validation entry names the 'type' field");
        if (e) {
            CHECK(e->severity == Mc3ValidationSeverity::Warning,
                  "the enum clamp is a warning, not an error");
            CHECK(e->objectId == "Obj",
                  "the entry's objectId identifies the offending object ('Obj'), read from "
                  "the earlier 'name' field in the same object");
            CHECK(e->suggestedRepair.find("enumerator 0") != std::string::npos,
                  "the entry's suggestedRepair names the clamp target: '" + e->suggestedRepair + "'");
        }
    }

    // 2. AUD-015: a type-tag mismatch on a known key must produce an ERROR
    // entry, and the SAME validation object must still receive it even
    // though loadFromBinary throws (additive side-channel, not a
    // replacement for the exception).
    {
        std::ostringstream out(std::ios::binary);
        out.write(MCB_MAGIC, 4);
        rawU8(out, MCB_VERSION);
        rawU8(out, 0);
        rawU8(out, 0); rawU8(out, 0);
        rawU8(out, TAG_OBJ);
        rawKey(out, "objects"); rawU8(out, TAG_ARR); rawU32(out, 1);
        rawU8(out, TAG_OBJ);
            rawKey(out, "name"); rawU8(out, TAG_STR); rawStr(out, "Bad");
            // "visible" is normally TAG_BOOL -- tag it TAG_STR instead.
            rawKey(out, "visible"); rawU8(out, TAG_STR); rawStr(out, "true");
        rawEnd(out);
        rawEnd(out);

        std::istringstream in(out.str(), std::ios::binary);
        Mc3Validation validation;
        bool threw = false;
        try {
            loadFromBinary(in, validation);
        } catch (const std::exception&) {
            threw = true;
        }
        CHECK(threw, "a type-tag mismatch still throws (unchanged contract)");
        CHECK(validation.hasErrors(),
              "the same validation object received an error entry despite the throw");
        const Mc3ValidationEntry* e = findEntry(validation, "visible");
        CHECK(e != nullptr, "the error entry names the 'visible' field");
        if (e) {
            CHECK(e->severity == Mc3ValidationSeverity::Error,
                  "a type-tag mismatch is an ERROR, not a warning");
            CHECK(e->objectId == "Bad", "the entry's objectId identifies the offending object");
        }
    }

    // 3. A clean, legitimately-written document (round-tripped through the
    // real McbWriter) must produce zero validation entries.
    {
        Mc3Document doc;
        doc.model = "clean";
        auto obj = std::make_shared<Mc3Object>();
        obj->type = ObjectType::Sphere;
        obj->name = "s";
        obj->id = "s1";
        Mc3Primitive prim;
        prim.primitiveType = PrimitiveType::Sphere;
        prim.segments = 32;
        obj->primitive = prim;
        doc.objects.push_back(obj);

        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        std::istringstream in(out.str(), std::ios::binary);

        Mc3Validation validation;
        Mc3Document rt = loadFromBinary(in, validation);
        CHECK(rt.objects.size() == 1, "clean document round-trips through Mcb");
        CHECK(validation.empty(), "a clean, legitimately-written document produces zero "
              "validation entries");
    }

    // 4. loadFromFile's validation-capturing overload works the same way as
    // loadFromBinary's, and the plain (no-validation) overloads are
    // unaffected (no regression).
    {
        Mc3Document doc;
        doc.model = "file-test";
        fs::path p = fs::temp_directory_path() / "mcb_validation_test.mcb";
        std::ofstream f(p, std::ios::binary);
        saveToBinary(doc, f);
        f.close();

        Mc3Validation validation;
        Mc3Document rt = loadFromFile(p, validation);
        CHECK(rt.model == "file-test", "loadFromFile(path, validation) still loads correctly");
        CHECK(validation.empty(), "a clean file produces zero validation entries via loadFromFile");

        Mc3Document rtPlain = loadFromFile(p); // no validation argument at all
        CHECK(rtPlain.model == "file-test",
              "loadFromFile(path) without a validation argument still behaves as before");

        std::error_code ec;
        fs::remove(p, ec);
    }

    if (failures == 0) { std::cout << "All Mcb Mc3Validation tests passed.\n"; return 0; }
    std::cerr << failures << " Mcb Mc3Validation test(s) failed.\n";
    return 1;
}
