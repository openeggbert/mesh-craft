// SYS-W1-04 — invalid UTF-8 byte sequences in MC3 XML content.
//
// Investigation: tinyxml2 (vendored, v10.0.0) does not validate UTF-8
// well-formedness of attribute values or element text at all -- it treats
// document content as an opaque byte stream and only interprets the ASCII
// structural characters ('<','>','&','"','\'') plus numeric character
// references (&#NNN; / &#xHH;, via ConvertUTF32ToUTF8). The one UTF-8-aware
// helper in tinyxml2 (IsUTF8Continuation) exists only so IsWhiteSpace()
// doesn't misinterpret a continuation byte (0x80-0xBF, negative as a signed
// char) as a control character; it does not reject or repair anything.
// Mc3XmlParser.cpp itself does no UTF-8 validation either (attrF/attr just
// forward tinyxml2's char* verbatim into std::string fields).
//
// Empirically confirmed (see plan.md SYS-W1-04 investigation notes): a lone
// continuation byte (0x80) inside a name="..." attribute and a truncated
// 3-byte sequence (0xE2 0x82 with no 3rd byte, at the very end of an
// element's text content) both load successfully with NO exception, and the
// invalid bytes are preserved byte-for-byte, unmodified, in the resulting
// Mc3Object::name / Mc3Script::source strings -- garbage-in-garbage-out, not
// crash/UB and not further corruption. This test locks in that safe
// pass-through behavior as a regression guard (distinct from AUD-009's
// appendUtf8CodePoint/readHex4 fix in AiAssistant.cpp, which handles a
// DIFFERENT code path -- decoding \u-escaped JSON from an AI provider
// response, not raw bytes in an MC3 XML document).

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

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
    fs::path dir = fs::temp_directory_path() / "mc3_invalid_utf8_test";
    fs::create_directories(dir);

    // Case 1: a lone continuation byte (0x80, no lead byte) inside an
    // attribute value -- must not crash or throw, and the byte sequence
    // must survive unchanged.
    {
        std::string xml = "<mc3 version=\"0.3\" model=\"badutf8\">\n  <objects>\n";
        xml += std::string("    <box name=\"bad_\x80_name\" id=\"b1\"/>\n");
        xml += "  </objects>\n</mc3>\n";

        bool threw = false;
        std::string what;
        Mc3Document doc;
        fs::path path = dir / "lone_continuation.mc3.xml";
        { std::ofstream f(path, std::ios::binary); f << xml; }
        try { doc = Mc3Document::loadFromFile(path); }
        catch (const std::exception& e) { threw = true; what = e.what(); }
        fs::remove(path);

        check(!threw, std::string("lone continuation byte: load does not throw (got: ") + what + ")");
        check(!threw && doc.objects.size() == 1, "lone continuation byte: object parsed");
        if (!threw && doc.objects.size() == 1) {
            const std::string expected("bad_\x80_name");
            check(doc.objects[0]->name == expected,
                  "lone continuation byte: name preserved byte-for-byte (no crash, no mangling)");
        }
    }

    // Case 2: a truncated multi-byte sequence (0xE2 0x82 -- the first two
    // bytes of a 3-byte encoding, missing the 3rd) right at the end of an
    // element's text content (the EOF-adjacent case explicitly called out
    // as the risky one).
    {
        std::string xml = "<mc3 version=\"0.3\" model=\"badutf8b\">\n";
        xml += std::string("  <scripts><script id=\"s1\" type=\"lua\">trunc_\xE2\x82</script></scripts>\n");
        xml += "  <objects><box name=\"b\"/></objects>\n</mc3>\n";

        bool threw = false;
        std::string what;
        Mc3Document doc;
        fs::path path = dir / "truncated_seq.mc3.xml";
        { std::ofstream f(path, std::ios::binary); f << xml; }
        try { doc = Mc3Document::loadFromFile(path); }
        catch (const std::exception& e) { threw = true; what = e.what(); }
        fs::remove(path);

        check(!threw, std::string("truncated multi-byte sequence: load does not throw (got: ") + what + ")");
        check(!threw && doc.scripts.count("s1") == 1,
              "truncated multi-byte sequence: script entry parsed");
        if (!threw && doc.scripts.count("s1")) {
            const std::string expected("trunc_\xE2\x82");
            check(doc.scripts["s1"].source == expected,
                  "truncated multi-byte sequence: source preserved byte-for-byte at EOF, "
                  "not read out of bounds looking for a continuation byte that never comes");
        }
    }

    // Case 3: an overlong encoding of NUL (0xC0 0x80 -- invalid per the UTF-8
    // spec, which forbids overlong forms) inside an attribute value.
    {
        std::string xml = "<mc3 version=\"0.3\" model=\"badutf8c\">\n  <objects>\n";
        xml += std::string("    <box name=\"over_\xC0\x80_long\" id=\"b1\"/>\n");
        xml += "  </objects>\n</mc3>\n";

        bool threw = false;
        std::string what;
        Mc3Document doc;
        fs::path path = dir / "overlong.mc3.xml";
        { std::ofstream f(path, std::ios::binary); f << xml; }
        try { doc = Mc3Document::loadFromFile(path); }
        catch (const std::exception& e) { threw = true; what = e.what(); }
        fs::remove(path);

        check(!threw, std::string("overlong encoding: load does not throw (got: ") + what + ")");
        check(!threw && doc.objects.size() == 1, "overlong encoding: object parsed");
        if (!threw && doc.objects.size() == 1) {
            const std::string expected("over_\xC0\x80_long");
            check(doc.objects[0]->name == expected,
                  "overlong encoding: name preserved byte-for-byte");
        }
    }

    fs::remove_all(dir);

    if (failures == 0) { std::cout << "All invalid-UTF-8 tests passed.\n"; return 0; }
    std::cerr << failures << " invalid-UTF-8 test(s) failed.\n";
    return 1;
}
