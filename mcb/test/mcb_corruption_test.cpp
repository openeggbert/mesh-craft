// SYS-W1-04 / SYS-W6-02 — MCB corruption coverage beyond what
// mcb_roundtrip_test.cpp already exercises.
//
// mcb_roundtrip_test.cpp already covers a lot of MCB-specific corruption:
// tag-mismatch on known keys (AUD-015), out-of-range enum clamping
// (AUD-017), huge string/collection-count rejection, the compressed-flag
// reject path (AUD-019), the deeply-nested-children recursion guard, version
// range validation (AUDIT-0038), and single-point cases of a truncated file
// / all-zeros / a single byte (STAB-0133/0134/0135). Read fully before
// writing this file; the two sub-cases NOT covered there are:
//
//  1. TRUNCATED at *every* byte offset of a realistically rich, multi-
//     section document (STAB-0133's testTruncatedFile only tries ONE cut
//     point, on a minimal one-object document) -- proving EOF mid-record
//     never crashes regardless of exactly which field/tag/length/nesting
//     level it lands in.
//  2. Genuinely RANDOM byte noise (not the deterministic all-zeros / single-
//     null-byte structured edge cases STAB-0134/0135 already cover) across
//     a range of sizes, with a fixed seed for reproducibility.
//
// Code-level investigation (McbReader.cpp) going in: every primitive read
// (rU8/rU32/rF32/rRawStr/rKey) already checks the stream's read succeeded
// before using the bytes; string/collection lengths are capped by
// kMcbMaxStringLen/kMcbMaxCollectionCount BEFORE being used to
// allocate/reserve; skipValue()'s tag switch has an explicit `default:
// throw` for any tag byte that isn't one of the 10 defined TAG_* constants;
// and RecursionGuard<256> bounds nesting depth. The expectation going in was
// therefore that this reader is already memory-safe against arbitrary
// corruption and this file would be a regression-PROVING addition, not a
// bug hunt -- confirmed by the results below (all cases pass; verified
// additionally under an ASan+UBSan build with zero findings).
//
// AUD-066 refinement (found later, by a separate audit pass -- see
// reserve_bomb_test.cpp): "capped BEFORE being used to allocate/reserve"
// above is true but incomplete -- kMcbMaxCollectionCount alone (10M) still
// let a tiny file's claimed count drive a single up-front .reserve() of
// that FULL size, which is a large-but-bounded allocation, not a memory-
// safety hole (no OOB read/write, no crash) but still a real resource-
// exhaustion DoS vector this file's own truncation/fuzz sweeps did not
// happen to surface (they check crash-safety and rejection correctness,
// not allocation SIZE). Fixed separately by capping the up-front reserve
// itself (reserveHint()), not by lowering kMcbMaxCollectionCount.

#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3CsgOperation.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3EmbedGltf.hpp"
#include "MeshCraft/Mc3/Mc3Environment.hpp"
#include "MeshCraft/Mc3/Mc3Extrude.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Primitive.hpp"
#include "MeshCraft/Mc3/Mc3SceneState.hpp"
#include "MeshCraft/Mc3/Mc3Script.hpp"
#include "MeshCraft/Mc3/Mc3Sound.hpp"
#include "MeshCraft/Mc3/Mc3SvgTexture.hpp"
#include "MeshCraft/Mc3/Mc3Texture.hpp"
#include "MeshCraft/Mc3/Mc3Trigger.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mcb;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

// A deliberately "rich" document that touches most of readDocument()'s known
// keys (objects with nested children/primitive/extrude/csg, materials,
// textures, svgTextures, embeds, scripts, sounds, music, triggers,
// sceneStates, meta/metadata, includes, actions/channels/keyframes,
// environment+fog, lights, cameras) so a byte-offset truncation sweep lands
// mid-field/mid-tag/mid-length/mid-nesting at many different structural
// positions, not just within one or two fields.
static Mc3Document buildRichDocument() {
    Mc3Document doc;
    doc.model = "RichCorruptionFixture";
    doc.meta["author"] = "fuzz";
    doc.metadata["legacy"] = "value";
    doc.includes = {"lib.mc3.xml"};
    doc.includedMaterials.insert("stone");

    Mc3Environment env;
    env.backgroundColor = {0.1f, 0.2f, 0.3f};
    env.skyboxTexture = "sky.hdr";
    Mc3Fog fog; fog.mode = FogMode::Exponential; fog.start = 1.0f; fog.end = 10.0f;
    env.fog = fog;
    doc.environment = env;

    Mc3Light light;
    light.type = LightType::Directional; light.name = "sun";
    light.color = {1, 1, 1}; light.castShadows = true;
    doc.lights.push_back(light);

    Mc3Camera cam;
    cam.name = "main"; cam.position = {0, 5, 10}; cam.fov = 60.0f;
    doc.cameras.push_back(cam);

    Mc3Texture tex; tex.name = "wall"; tex.uri = "wall.png";
    doc.textures["wall"] = tex;

    Mc3SvgTexture svg; svg.id = "logo"; svg.inlineContent = "<svg/>";
    doc.svgTextures["logo"] = svg;

    Mc3EmbedGltf em; em.id = "prop"; em.base64Content = "Z2xURg==";
    doc.embeds["prop"] = em;

    Mc3Script sc; sc.id = "onStart"; sc.type = "lua"; sc.source = "print(1)";
    doc.scripts["onStart"] = sc;

    Mc3Sound snd; snd.id = "boom"; snd.src = "boom.ogg"; snd.loop = true;
    doc.sounds["boom"] = snd;

    Mc3Music mus; mus.id = "theme"; mus.src = "theme.ogg";
    doc.musicTracks["theme"] = mus;

    Mc3Trigger trig; trig.id = "door";
    trig.steps.push_back({TriggerStepType::PlaySound, "boom"});
    doc.triggers["door"] = trig;

    Mc3Material mat; mat.name = "Red"; mat.baseColor = {1, 0, 0, 1}; mat.roughness = 0.4f;
    doc.materials["Red"] = mat;

    Mc3SceneState state; state.name = "night";
    Mc3ObjectOverride ovr; ovr.id = "lamp"; ovr.visible = false;
    state.overrides.push_back(ovr);
    doc.sceneStates["night"] = state;

    // Root box with a nested child sphere (children array + nested TAG_OBJ).
    auto root = std::make_shared<Mc3Object>();
    root->id = "root"; root->name = "Root"; root->type = ObjectType::Box;
    root->primitive = Mc3Primitive{};
    root->primitive->size = {2, 2, 2};
    root->tags = {"a", "b"};
    root->metadata["k"] = "v";

    auto child = std::make_shared<Mc3Object>();
    child->id = "child"; child->name = "Child"; child->type = ObjectType::Sphere;
    child->primitive = Mc3Primitive{.primitiveType = PrimitiveType::Sphere, .radius = 0.5f};
    root->children.push_back(child);
    doc.objects.push_back(root);

    // CSG node with two children (one cutter).
    auto csgNode = std::make_shared<Mc3Object>();
    csgNode->id = "diff"; csgNode->type = ObjectType::Difference;
    csgNode->csgOperation = Mc3CsgOperation{.csgType = CsgType::Difference};
    auto base = std::make_shared<Mc3Object>();
    base->id = "base"; base->type = ObjectType::Box; base->primitive = Mc3Primitive{};
    auto cutter = std::make_shared<Mc3Object>();
    cutter->id = "cutter"; cutter->type = ObjectType::Sphere;
    cutter->primitive = Mc3Primitive{.primitiveType = PrimitiveType::Sphere};
    cutter->isCutter = true;
    csgNode->children.push_back(base);
    csgNode->children.push_back(cutter);
    doc.objects.push_back(csgNode);

    // Extrude object.
    auto ext = std::make_shared<Mc3Object>();
    ext->id = "ext"; ext->type = ObjectType::Extrude;
    Mc3Extrude ex;
    ex.crossSection.type = CrossSectionType::Polygon;
    ex.crossSection.sides = 6;
    ex.path.type = ExtrudePathType::Helix;
    ex.segments = 32;
    ext->extrude = ex;
    doc.objects.push_back(ext);

    // Action with a channel and several keyframes.
    Mc3Action action;
    action.name = "Walk"; action.duration = 2.0f; action.loop = true;
    Mc3Channel channel;
    channel.targetObject = "root"; channel.property = AnimatedProperty::PositionY;
    for (int i = 0; i < 5; ++i)
        channel.keyframes.push_back(Mc3Keyframe::linear(static_cast<float>(i) * 0.5f, static_cast<float>(i)));
    action.channels.push_back(channel);
    doc.actions["Walk"] = action;

    return doc;
}

static void testTruncationSweepDoesNotCrash() {
    Mc3Document doc = buildRichDocument();
    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    const std::string full = out.str();

    CHECK(full.size() > 500,
          "truncation sweep: rich fixture is large enough to give a meaningful sweep ("
          + std::to_string(full.size()) + " bytes)");

    // Every prefix length from 0 to full.size()-1: loadFromBinary() must
    // either succeed (returning some partial/valid document) or throw a
    // std::exception -- if this loop function returns at all, the process
    // did not crash (a native segfault/abort would kill the whole test
    // binary and ctest would report the test as failed/crashed, not as a
    // clean PASS/FAIL line).
    long long threwCount = 0, okCount = 0;
    auto start = std::chrono::steady_clock::now();
    for (size_t len = 0; len < full.size(); ++len) {
        std::istringstream in(full.substr(0, len), std::ios::binary);
        try {
            Mc3Document rt = loadFromBinary(in);
            (void)rt;
            ++okCount;
        } catch (const std::exception&) {
            ++threwCount;
        }
    }
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    CHECK(true, "truncation sweep: swept all " + std::to_string(full.size()) +
          " prefix lengths without a crash (" + std::to_string(threwCount) +
          " threw, " + std::to_string(okCount) + " parsed as a (necessarily "
          "still-partial/short) document) in " + std::to_string(elapsedMs) + "ms");
    // Truncation almost always removes required bytes a downstream read
    // needs, so the overwhelming majority of prefixes must throw. A near-100%
    // throw rate (as opposed to e.g. 50/50) is itself evidence the reader is
    // correctly bounds-checking rather than silently accepting garbage.
    CHECK(threwCount > okCount,
          "truncation sweep: the large majority of truncated prefixes are "
          "rejected with a clean exception, not silently accepted");
    CHECK(elapsedMs < 10000,
          "truncation sweep: completes promptly (no pathological slow path "
          "hiding behind a truncated length)");

    // The full, untruncated stream must still load successfully (sanity:
    // proves the sweep's byte source itself is a valid, complete document).
    {
        std::istringstream in(full, std::ios::binary);
        bool threw = false;
        Mc3Document rt;
        try { rt = loadFromBinary(in); } catch (const std::exception&) { threw = true; }
        CHECK(!threw, "truncation sweep: the full (untruncated) stream loads cleanly");
        CHECK(!threw && rt.model == "RichCorruptionFixture",
              "truncation sweep: the full stream round-trips the fixture's model name");
    }
}

static void testRandomByteFuzzDoesNotCrash() {
    // Fixed seed: reproducible across runs/machines, not a real fuzzer, just
    // a deterministic sample of "genuine noise" shapes distinct from the
    // structured all-zeros/single-null-byte cases mcb_roundtrip_test.cpp
    // already covers.
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> byteDist(0, 255);

    const std::vector<size_t> sizes = {0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
                                        1024, 4096, 16384};
    const int trialsPerSize = 25;

    long long threwCount = 0, okCount = 0, total = 0;
    auto start = std::chrono::steady_clock::now();
    for (size_t sz : sizes) {
        for (int t = 0; t < trialsPerSize; ++t) {
            std::string buf(sz, '\0');
            for (auto& ch : buf) ch = static_cast<char>(byteDist(rng));

            std::istringstream in(buf, std::ios::binary);
            try {
                Mc3Document rt = loadFromBinary(in);
                (void)rt;
                ++okCount;
            } catch (const std::exception&) {
                ++threwCount;
            }
            ++total;
        }
    }
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    CHECK(true, "random-byte fuzz: " + std::to_string(total) +
          " random buffers (sizes 0..16KB) processed without a crash (" +
          std::to_string(threwCount) + " threw, " + std::to_string(okCount) +
          " happened to parse) in " + std::to_string(elapsedMs) + "ms");
    // Genuine random noise essentially never starts with the 4-byte magic
    // "MCB\0" (probability ~1/2^32 per trial), so virtually every trial must
    // be rejected at the very first check in loadFromBinary().
    CHECK(threwCount == total,
          "random-byte fuzz: every random buffer is rejected (none of the " +
          std::to_string(total) + " trials happened to start with a valid "
          "MCB magic by chance, as expected)");
    CHECK(elapsedMs < 5000,
          "random-byte fuzz: completes promptly, no hang on any random input");
}

int main() {
    testTruncationSweepDoesNotCrash();
    testRandomByteFuzzDoesNotCrash();

    if (failures == 0)
        std::cout << "All MCB corruption tests passed.\n";
    else
        std::cerr << failures << " MCB corruption test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
