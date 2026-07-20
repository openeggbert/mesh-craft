// 2026-07-20 audit F1 (P0): McbReader.cpp used to read every tessellation-
// driving field (Mc3Primitive::segments/subdivisionsX/subdivisionsZ,
// Mc3CrossSection::sides/segments, Mc3Extrude::segments) via a bare rI32()
// with no clamp at all -- unlike Mc3XmlParser.cpp's kMaxTessellation=4096
// and Mc3JsonParser.cpp's clampTess(), both of which bound every one of
// these same fields. A hostile/corrupted .mcb file could therefore claim
// an arbitrarily large segment/subdivision count; downstream geometry
// builders (mc3togltf/src/MeshBuilder.cpp) trust these fields completely,
// so an unclamped value drives an allocation many orders of magnitude
// beyond anything a legitimate scene needs -- reachable directly from the
// editor's own Open-file path (MeshCraftApplication_FileOps.cpp calls
// Mcb::loadFromFile() on whatever the user opens).
//
// Byte-patches a legitimately-serialized MCB stream's tessellation field(s)
// to a huge value, the same technique reserve_bomb_test.cpp already
// established for a different field (a claimed collection count) --
// locates the exact key-byte-pattern, patches the 4-byte TAG_I32 value in
// place (no truncation needed here, unlike a collection-count bomb, since
// a single scalar field doesn't need trailing element data), then confirms
// the loaded document's field is clamped to <=4096 rather than reading
// back the hostile value.

#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Extrude.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mcb;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

// Patches every occurrence of `key`'s TAG_I32 value to `hostileValue`.
// Returns the number of occurrences patched.
static int patchAllI32Fields(std::string& bytes, const std::string& key, int32_t hostileValue) {
    const std::string keyPattern = std::string(1, static_cast<char>(key.size())) + key;
    int patched = 0;
    size_t pos = 0;
    while ((pos = bytes.find(keyPattern, pos)) != std::string::npos) {
        size_t valueOffset = pos + keyPattern.size() + 1; // +1 for the TAG_I32 byte
        if (valueOffset + 4 <= bytes.size()) {
            std::memcpy(&bytes[valueOffset], &hostileValue, 4);
            ++patched;
        }
        pos += keyPattern.size();
    }
    return patched;
}

int main() {
    // --- Sphere.segments (readPrimitive) ---
    // McbWriter's wIfI32() only writes a field when it differs from
    // Mc3Primitive's own default (segments=32, per Mc3Primitive.hpp) -- a
    // sparse-write optimization. Using 32 here would make the writer skip
    // the field entirely, so this test deliberately picks 48 instead.
    {
        Mc3Document doc;
        doc.model = "TessBombSphere";
        doc.addObject(Mc3Object::makeSphere("s", 1.0f, 48, ""));

        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        std::string bytes = out.str();

        int patched = patchAllI32Fields(bytes, "segments", 999'999'999);
        CHECK(patched == 1, "Sphere-only document: exactly one 'segments' field patched (unambiguous)");

        std::istringstream in(bytes, std::ios::binary);
        Mc3Document rt = loadFromBinary(in);
        CHECK(rt.objects.size() == 1 && rt.objects[0]->primitive.has_value(),
              "patched Sphere document still loads with its primitive intact");
        if (!rt.objects.empty() && rt.objects[0]->primitive) {
            CHECK(rt.objects[0]->primitive->segments <= 4096,
                  "Sphere.segments clamped to <=4096 (was patched to 999999999), got " +
                  std::to_string(rt.objects[0]->primitive->segments));
        }
    }

    // --- Grid.subdivisionsX / subdivisionsZ (readPrimitive) ---
    // Same sparse-write concern: Mc3Primitive's default subdivisionsX/Z is
    // 4/4, so this test uses 7/5 instead to guarantee the writer emits them.
    {
        Mc3Document doc;
        doc.model = "TessBombGrid";
        doc.addObject(Mc3Object::makeGrid("g", 7, 5, 1.0f, 1.0f, ""));

        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        std::string bytes = out.str();

        int patchedX = patchAllI32Fields(bytes, "subdivisionsX", 999'999'999);
        int patchedZ = patchAllI32Fields(bytes, "subdivisionsZ", 999'999'999);
        CHECK(patchedX == 1 && patchedZ == 1, "Grid-only document: subdivisionsX/Z each patched exactly once");

        std::istringstream in(bytes, std::ios::binary);
        Mc3Document rt = loadFromBinary(in);
        CHECK(rt.objects.size() == 1 && rt.objects[0]->primitive.has_value(),
              "patched Grid document still loads with its primitive intact");
        if (!rt.objects.empty() && rt.objects[0]->primitive) {
            CHECK(rt.objects[0]->primitive->subdivisionsX <= 4096,
                  "Grid.subdivisionsX clamped to <=4096, got " +
                  std::to_string(rt.objects[0]->primitive->subdivisionsX));
            CHECK(rt.objects[0]->primitive->subdivisionsZ <= 4096,
                  "Grid.subdivisionsZ clamped to <=4096, got " +
                  std::to_string(rt.objects[0]->primitive->subdivisionsZ));
        }
    }

    // --- Extrude crossSection.sides / crossSection.segments / extrude.segments
    // (readCrossSection + readExtrude) ---
    // Same sparse-write concern again: Mc3CrossSection's defaults are
    // sides=6/segments=32 and Mc3Extrude's default segments is also 32 --
    // all three deliberately set to different values here.
    {
        Mc3Document doc;
        doc.model = "TessBombExtrude";
        auto obj = std::make_shared<Mc3Object>();
        obj->type = ObjectType::Extrude;
        obj->id = obj->name = "e";
        Mc3Extrude ex;
        ex.crossSection.type = CrossSectionType::Polygon;
        ex.crossSection.sides = 8;
        ex.crossSection.segments = 40;
        ex.segments = 40; // path segments
        obj->extrude = ex;
        doc.objects.push_back(obj);

        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        std::string bytes = out.str();

        int patchedSides = patchAllI32Fields(bytes, "sides", 999'999'999);
        CHECK(patchedSides == 1, "crossSection.sides patched exactly once (unique key)");
        // "segments" appears twice in a single Extrude object: once as
        // crossSection.segments, once as the extrude's own path segments.
        // Both share the same clamp semantics (min 1, max 4096), so
        // patching every occurrence and confirming every loaded field is
        // clamped covers both without needing to disambiguate which
        // physical occurrence is which.
        int patchedSegments = patchAllI32Fields(bytes, "segments", 999'999'999);
        CHECK(patchedSegments == 2,
              "exactly two 'segments' fields patched (crossSection.segments + extrude.segments), got " +
              std::to_string(patchedSegments));

        std::istringstream in(bytes, std::ios::binary);
        Mc3Document rt = loadFromBinary(in);
        CHECK(rt.objects.size() == 1 && rt.objects[0]->extrude.has_value(),
              "patched Extrude document still loads with its extrude data intact");
        if (!rt.objects.empty() && rt.objects[0]->extrude) {
            const auto& rtEx = *rt.objects[0]->extrude;
            CHECK(rtEx.crossSection.sides <= 4096,
                  "crossSection.sides clamped to <=4096, got " + std::to_string(rtEx.crossSection.sides));
            CHECK(rtEx.crossSection.segments <= 4096,
                  "crossSection.segments clamped to <=4096, got " + std::to_string(rtEx.crossSection.segments));
            CHECK(rtEx.segments <= 4096,
                  "extrude.segments (path) clamped to <=4096, got " + std::to_string(rtEx.segments));
        }
    }

    // --- A legitimate, non-hostile document must still round-trip its
    // real values exactly -- the clamp must not be a false-positive trap. ---
    {
        Mc3Document doc;
        doc.model = "TessLegit";
        doc.addObject(Mc3Object::makeSphere("s", 1.0f, 24, ""));
        doc.addObject(Mc3Object::makeGrid("g", 8, 6, 2.0f, 2.0f, ""));

        std::ostringstream out(std::ios::binary);
        saveToBinary(doc, out);
        std::istringstream in(out.str(), std::ios::binary);
        Mc3Document rt = loadFromBinary(in);

        CHECK(rt.objects.size() == 2, "legitimate document round-trips both objects");
        bool sphereOk = false, gridOk = false;
        for (const auto& o : rt.objects) {
            if (o->primitive && o->primitive->primitiveType == PrimitiveType::Sphere)
                sphereOk = (o->primitive->segments == 24);
            if (o->primitive && o->primitive->primitiveType == PrimitiveType::Grid)
                gridOk = (o->primitive->subdivisionsX == 8 && o->primitive->subdivisionsZ == 6);
        }
        CHECK(sphereOk, "legitimate Sphere.segments=24 survives round-trip unchanged (not clamped away)");
        CHECK(gridOk, "legitimate Grid.subdivisions=8/6 survives round-trip unchanged (not clamped away)");
    }

    if (failures == 0) { std::cout << "All tessellation-clamp tests passed.\n"; return 0; }
    std::cerr << failures << " tessellation-clamp test(s) FAILED.\n";
    return 1;
}
