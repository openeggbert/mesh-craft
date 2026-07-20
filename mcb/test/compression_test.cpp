// SYS-W14-25 (2026-07-20): McbFormat.hpp's MCB_FLAG_COMPRESSED was reserved
// but never implemented -- McbWriter always wrote flags=0 (no compression),
// and McbReader threw "compressed format not yet supported" for any other
// value, making that branch permanently dead code against this codebase's
// own writer. Now McbWriter::saveToBinary(doc, out, /*compress=*/true)
// actually zlib-deflates the document payload, and McbReader transparently
// inflates it back.
//
// Covers: round-trip correctness (compressed load reproduces the same
// document as uncompressed), the flags byte actually reflecting compression,
// real size reduction on a repetitive document, and the zip-bomb defenses
// (claimed uncompressed/compressed size sanity ceilings, truncated payload,
// corrupted payload) -- same "hand-craft the exact byte pattern" idiom
// reserve_bomb_test.cpp/tag_mismatch_rejection_test.cpp already use for
// this reader, applied to the new compressed-header fields instead.

#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
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

// Little-endian u32, matching McbWriter.cpp's own wU32() -- used to
// hand-craft/patch header fields the same way reserve_bomb_test.cpp does.
void appendU32(std::string& s, uint32_t v) {
    s.push_back(static_cast<char>(v & 0xFF));
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
    s.push_back(static_cast<char>((v >> 16) & 0xFF));
    s.push_back(static_cast<char>((v >> 24) & 0xFF));
}

// A document with enough repetitive structure that real compression should
// meaningfully shrink it (many near-identical boxes), not just a couple of
// bytes that could be noise either way.
Mc3Document makeRepetitiveDocument() {
    Mc3Document doc;
    doc.model = "CompressionTestScene";
    for (int i = 0; i < 200; ++i) {
        auto box = std::make_shared<Mc3Object>();
        box->name = "RepeatedBox" + std::to_string(i);
        box->type = ObjectType::Box;
        box->primitive = Mc3Primitive::box({1.0f, 1.0f, 1.0f});
        box->transform.position = {static_cast<float>(i), 0.0f, 0.0f};
        doc.objects.push_back(box);
    }
    return doc;
}

} // namespace

int main() {
    // --- 1. Uncompressed (default) still writes flags=0, byte 5. ---
    {
        Mc3Document doc;
        doc.model = "Plain";
        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        const std::string bytes = out.str();
        CHECK(bytes.size() >= 8, "uncompressed: stream has at least the 8-byte header");
        CHECK(static_cast<uint8_t>(bytes[5]) == 0,
              "uncompressed (default compress=false): flags byte is 0");
    }

#ifndef MESHCRAFT_HAS_ZLIB
    std::cout << "SKIP: remaining MCB compression tests require MESHCRAFT_HAS_ZLIB "
                 "(this build was compiled without zlib available)\n";
#else
    // --- 2. Compressed writes MCB_FLAG_COMPRESSED at byte 5. ---
    {
        Mc3Document doc = makeRepetitiveDocument();
        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out, /*compress=*/true);
        const std::string bytes = out.str();
        CHECK(bytes.size() >= 8, "compressed: stream has at least the 8-byte header");
        CHECK(static_cast<uint8_t>(bytes[5]) == MCB_FLAG_COMPRESSED,
              "compressed (compress=true): flags byte is MCB_FLAG_COMPRESSED");
    }

    // --- 3. Round-trip correctness: compressed load reproduces the same
    //        document (spot-checked fields) as the uncompressed path. ---
    {
        Mc3Document doc = makeRepetitiveDocument();
        doc.lights.push_back(Mc3Light::directional("sun"));

        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out, /*compress=*/true);
        std::istringstream in(out.str(), std::ios::binary);
        Mc3Document loaded = loadFromBinary(in);

        CHECK(loaded.model == doc.model, "round-trip: model name survives compression");
        CHECK(loaded.objects.size() == doc.objects.size(),
              "round-trip: object count survives compression");
        CHECK(loaded.lights.size() == 1, "round-trip: lights survive compression");
        bool allNamesMatch = loaded.objects.size() == doc.objects.size();
        for (size_t i = 0; allNamesMatch && i < loaded.objects.size(); ++i)
            allNamesMatch = loaded.objects[i]->name == doc.objects[i]->name &&
                            loaded.objects[i]->transform.position == doc.objects[i]->transform.position;
        CHECK(allNamesMatch, "round-trip: every object's name+position survives compression");
    }

    // --- 4. Real size reduction on a repetitive document. ---
    {
        Mc3Document doc = makeRepetitiveDocument();
        std::ostringstream plain(std::ios::binary);
        saveToBinary(doc, plain, /*compress=*/false);
        std::ostringstream compressed(std::ios::binary);
        saveToBinary(doc, compressed, /*compress=*/true);

        CHECK(compressed.str().size() < plain.str().size(),
              "compression measurably shrinks a repetitive document (plain=" +
              std::to_string(plain.str().size()) + " bytes, compressed=" +
              std::to_string(compressed.str().size()) + " bytes)");
    }

    // --- 5. Zip-bomb defense: a claimed uncompressed size past the sanity
    //        ceiling is rejected before any large allocation/decompression
    //        is attempted, not discovered only after one is. ---
    {
        std::string bytes;
        bytes.append(MCB_MAGIC, 4);
        bytes.push_back(static_cast<char>(MCB_VERSION));
        bytes.push_back(static_cast<char>(MCB_FLAG_COMPRESSED));
        bytes.push_back('\0'); bytes.push_back('\0'); // reserved
        appendU32(bytes, 0xFFFFFFFFu); // claimed uncompressed size: ~4GB
        appendU32(bytes, 4);           // claimed compressed size (small, unreachable)
        bytes.append("\x00\x00\x00\x00", 4);

        std::istringstream in(bytes, std::ios::binary);
        bool threw = false;
        std::string what;
        try { loadFromBinary(in); }
        catch (const std::exception& e) { threw = true; what = e.what(); }
        CHECK(threw, "an absurd claimed uncompressed size is rejected");
        CHECK(what.find("sanity limit") != std::string::npos,
              "rejection names the sanity-limit reason (got: " + what + ")");
    }

    // --- 6. Zip-bomb defense: claimed compressed size past the sanity
    //        ceiling is also rejected up front. ---
    {
        std::string bytes;
        bytes.append(MCB_MAGIC, 4);
        bytes.push_back(static_cast<char>(MCB_VERSION));
        bytes.push_back(static_cast<char>(MCB_FLAG_COMPRESSED));
        bytes.push_back('\0'); bytes.push_back('\0'); // reserved
        appendU32(bytes, 1024);        // claimed uncompressed size: modest
        appendU32(bytes, 0xFFFFFFFFu); // claimed compressed size: ~4GB

        std::istringstream in(bytes, std::ios::binary);
        bool threw = false;
        std::string what;
        try { loadFromBinary(in); }
        catch (const std::exception& e) { threw = true; what = e.what(); }
        CHECK(threw, "an absurd claimed compressed size is rejected");
        CHECK(what.find("sanity limit") != std::string::npos,
              "rejection names the sanity-limit reason (got: " + what + ")");
    }

    // --- 7. Truncated compressed payload (claims more bytes than the
    //        stream actually contains) is rejected, not read past EOF. ---
    {
        Mc3Document doc = makeRepetitiveDocument();
        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out, /*compress=*/true);
        std::string bytes = out.str();
        std::string truncated = bytes.substr(0, bytes.size() - 10); // chop off the tail

        std::istringstream in(truncated, std::ios::binary);
        bool threw = false;
        try { loadFromBinary(in); }
        catch (const std::exception&) { threw = true; }
        CHECK(threw, "a truncated compressed payload is rejected, not read past EOF");
    }

    // --- 8. Corrupted compressed payload (valid-looking header/sizes, but
    //        the bytes themselves aren't valid deflate data) is rejected
    //        with a clear error, not a crash or silent garbage document. ---
    {
        Mc3Document doc = makeRepetitiveDocument();
        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out, /*compress=*/true);
        std::string bytes = out.str();
        // Flip a handful of bytes well inside the compressed payload
        // (past the 16-byte header) so the sizes still match but the
        // deflate stream itself is invalid.
        for (size_t i = 20; i < bytes.size() && i < 40; ++i)
            bytes[i] = static_cast<char>(~bytes[i]);

        std::istringstream in(bytes, std::ios::binary);
        bool threw = false;
        try { loadFromBinary(in); }
        catch (const std::exception&) { threw = true; }
        CHECK(threw, "a corrupted compressed payload is rejected with a clear error");
    }
#endif // MESHCRAFT_HAS_ZLIB

    if (failures == 0)
        std::cout << "All MCB compression (SYS-W14-25) tests passed.\n";
    else
        std::cerr << failures << " MCB compression (SYS-W14-25) test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
