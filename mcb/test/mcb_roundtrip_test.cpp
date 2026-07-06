#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mc3/Mc3CsgOperation.hpp"
#include "MeshCraft/Mc3/Mc3Deform.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3EmbedGltf.hpp"
#include "MeshCraft/Mc3/Mc3Extrude.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Primitive.hpp"
#include "MeshCraft/Mc3/Mc3Music.hpp"
#include "MeshCraft/Mc3/Mc3SceneState.hpp"
#include "MeshCraft/Mc3/Mc3Script.hpp"
#include "MeshCraft/Mc3/Mc3Sound.hpp"
#include "MeshCraft/Mc3/Mc3SvgTexture.hpp"
#include "MeshCraft/Mc3/Mc3Trigger.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mcb;

static int failures = 0;

static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }

#define CHECK(cond, msg)  do { if (cond) pass(msg); else fail(msg); } while(0)
#define CHECKF(a, b, msg) CHECK(std::abs((a)-(b)) < 1e-5f, msg)

static Mc3Document roundtrip(const Mc3Document& doc) {
    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    std::istringstream in(out.str(), std::ios::binary);
    return loadFromBinary(in);
}

// ---------------------------------------------------------------------------
// STAB-0121 / STAB-0122 — basic scene roundtrip
// ---------------------------------------------------------------------------

static void testSmoke() {
    // Empty document: verify MCB magic survives
    Mc3Document doc;
    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    const std::string bytes = out.str();
    CHECK(bytes.size() >= 8,        "smoke: MCB stream has at least 8 bytes");
    CHECK(bytes[0] == 'M',          "smoke: magic[0] == 'M'");
    CHECK(bytes[1] == 'C',          "smoke: magic[1] == 'C'");
    CHECK(bytes[2] == 'B',          "smoke: magic[2] == 'B'");
    CHECK(bytes[3] == '\0',         "smoke: magic[3] == NUL");
    CHECK(static_cast<uint8_t>(bytes[4]) == MCB_VERSION, "smoke: version == MCB_VERSION");
    CHECK(bytes[5] == '\0',         "smoke: flags == 0 (no compression)");
}

static void testBasicScene() {
    Mc3Document doc;
    doc.model = "BasicModel";

    auto obj       = std::make_shared<Mc3Object>();
    obj->id        = "cube1";
    obj->name      = "MyCube";
    obj->type      = ObjectType::Box;
    obj->primitive = Mc3Primitive{};
    obj->primitive->size = {2.0f, 2.0f, 2.0f};
    doc.objects.push_back(obj);

    Mc3Material mat;
    mat.name      = "RedMat";
    mat.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
    mat.roughness = 0.3f;
    doc.materials["RedMat"] = mat;

    auto rt = roundtrip(doc);

    CHECK(rt.model == "BasicModel",        "basic: model survives");
    CHECK(rt.objects.size() == 1,          "basic: 1 object survives");
    if (!rt.objects.empty()) {
        CHECK(rt.objects[0]->id   == "cube1",  "basic: object id survives");
        CHECK(rt.objects[0]->name == "MyCube", "basic: object name survives");
        CHECK(rt.objects[0]->primitive.has_value(), "basic: primitive present");
        if (rt.objects[0]->primitive)
            CHECKF(rt.objects[0]->primitive->size[0], 2.0f, "basic: primitive size.x survives");
    }
    CHECK(rt.materials.count("RedMat") == 1, "basic: material survives");
    if (rt.materials.count("RedMat")) {
        CHECKF(rt.materials["RedMat"].roughness, 0.3f, "basic: material roughness survives");
        CHECKF(rt.materials["RedMat"].baseColor[0], 1.0f, "basic: material baseColor.r survives");
    }
}

// ---------------------------------------------------------------------------
// STAB-0123 — N1 svgTextures map
// ---------------------------------------------------------------------------

static void testSvgTexture() {
    Mc3Document doc;
    Mc3SvgTexture svg;
    svg.id            = "logo";
    svg.inlineContent = "<svg><circle r='10'/></svg>";
    doc.svgTextures["logo"] = svg;

    auto rt = roundtrip(doc);

    CHECK(rt.svgTextures.count("logo") == 1, "svg: entry survives");
    if (rt.svgTextures.count("logo")) {
        CHECK(rt.svgTextures["logo"].inlineContent == svg.inlineContent,
              "svg: inlineContent survives");
        CHECK(rt.svgTextures["logo"].src.empty(), "svg: src empty for inline");
    }
}

// ---------------------------------------------------------------------------
// STAB-0124 — N2 embeds map
// ---------------------------------------------------------------------------

static void testEmbed() {
    Mc3Document doc;
    Mc3EmbedGltf em;
    em.id            = "tree";
    em.base64Content = "Z2xURg==";  // fake base64
    doc.embeds["tree"] = em;

    auto rt = roundtrip(doc);

    CHECK(rt.embeds.count("tree") == 1, "embed: entry survives");
    if (rt.embeds.count("tree")) {
        CHECK(rt.embeds["tree"].base64Content == em.base64Content, "embed: base64Content survives");
        CHECK(rt.embeds["tree"].src.empty(), "embed: src empty for inline");
    }
}

// ---------------------------------------------------------------------------
// STAB-0125 — N3 scripts map
// ---------------------------------------------------------------------------

static void testScript() {
    Mc3Document doc;
    Mc3Script sc;
    sc.id     = "onStart";
    sc.type   = "lua";
    sc.source = "print('hello')";
    doc.scripts["onStart"] = sc;

    auto rt = roundtrip(doc);

    CHECK(rt.scripts.count("onStart") == 1, "script: entry survives");
    if (rt.scripts.count("onStart")) {
        CHECK(rt.scripts["onStart"].type   == "lua",            "script: type survives");
        CHECK(rt.scripts["onStart"].source == "print('hello')", "script: source survives");
    }
}

// ---------------------------------------------------------------------------
// STAB-0126 — N4 sounds map
// ---------------------------------------------------------------------------

static void testSound() {
    Mc3Document doc;
    Mc3Sound snd;
    snd.id   = "explosion";
    snd.src  = "sounds/boom.ogg";
    snd.loop = true;
    doc.sounds["explosion"] = snd;

    auto rt = roundtrip(doc);

    CHECK(rt.sounds.count("explosion") == 1, "sound: entry survives");
    if (rt.sounds.count("explosion")) {
        CHECK(rt.sounds["explosion"].src  == "sounds/boom.ogg", "sound: src survives");
        CHECK(rt.sounds["explosion"].loop == true,              "sound: loop=true survives");
    }
}

// ---------------------------------------------------------------------------
// STAB-0127 — N4 musicTracks map
// ---------------------------------------------------------------------------

static void testMusic() {
    Mc3Document doc;
    Mc3Music mus;
    mus.id   = "theme";
    mus.src  = "music/theme.ogg";
    mus.loop = false;
    doc.musicTracks["theme"] = mus;

    auto rt = roundtrip(doc);

    CHECK(rt.musicTracks.count("theme") == 1, "music: entry survives");
    if (rt.musicTracks.count("theme")) {
        CHECK(rt.musicTracks["theme"].src  == "music/theme.ogg", "music: src survives");
        CHECK(rt.musicTracks["theme"].loop == false,             "music: loop=false survives");
    }
}

// ---------------------------------------------------------------------------
// STAB-0128 — N5 triggers map with all step types
// ---------------------------------------------------------------------------

static void testTrigger() {
    Mc3Document doc;
    Mc3Trigger trig;
    trig.id = "door_open";
    trig.steps.push_back({TriggerStepType::PlayAction, "anim_open"});
    trig.steps.push_back({TriggerStepType::PlaySound,  "creak"});
    trig.steps.push_back({TriggerStepType::RunScript,  "onOpen"});
    trig.steps.push_back({TriggerStepType::PlayMusic,  "theme"});
    doc.triggers["door_open"] = trig;

    auto rt = roundtrip(doc);

    CHECK(rt.triggers.count("door_open") == 1, "trigger: entry survives");
    if (rt.triggers.count("door_open")) {
        const auto& t = rt.triggers["door_open"];
        CHECK(t.steps.size() == 4, "trigger: 4 steps survive");
        if (t.steps.size() == 4) {
            CHECK(t.steps[0].type == TriggerStepType::PlayAction && t.steps[0].ref == "anim_open", "trigger: step[0] PlayAction");
            CHECK(t.steps[1].type == TriggerStepType::PlaySound  && t.steps[1].ref == "creak",     "trigger: step[1] PlaySound");
            CHECK(t.steps[2].type == TriggerStepType::RunScript  && t.steps[2].ref == "onOpen",    "trigger: step[2] RunScript");
            CHECK(t.steps[3].type == TriggerStepType::PlayMusic  && t.steps[3].ref == "theme",     "trigger: step[3] PlayMusic");
        }
    }
}

// ---------------------------------------------------------------------------
// STAB-0129 — N6 sceneStates map with all override fields
// ---------------------------------------------------------------------------

static void testSceneState() {
    Mc3Document doc;
    Mc3SceneState state;
    state.name = "night";

    Mc3ObjectOverride ovr;
    ovr.id       = "lamp1";
    ovr.visible  = false;
    ovr.position = {1.0f, 2.0f, 3.0f};
    ovr.rotation = {0.0f, 90.0f, 0.0f};
    ovr.material = "NightMat";
    state.overrides.push_back(ovr);
    doc.sceneStates["night"] = state;

    auto rt = roundtrip(doc);

    CHECK(rt.sceneStates.count("night") == 1, "state: entry survives");
    if (rt.sceneStates.count("night")) {
        const auto& s = rt.sceneStates["night"];
        CHECK(s.overrides.size() == 1, "state: 1 override survives");
        if (!s.overrides.empty()) {
            const auto& o = s.overrides[0];
            CHECK(o.id == "lamp1",                     "state: override.id survives");
            CHECK(o.visible.has_value() && !*o.visible, "state: visible=false survives");
            CHECK(o.position.has_value(),               "state: position present");
            if (o.position) CHECKF((*o.position)[1], 2.0f, "state: position.y survives");
            CHECK(o.rotation.has_value(),               "state: rotation present");
            if (o.rotation) CHECKF((*o.rotation)[1], 90.0f, "state: rotation.y survives");
            CHECK(o.material.has_value() && *o.material == "NightMat", "state: material survives");
        }
    }
}

// ---------------------------------------------------------------------------
// STAB-0130 — N7 meta map
// ---------------------------------------------------------------------------

static void testMeta() {
    Mc3Document doc;
    doc.meta["author"] = "Test Author";
    doc.meta["license"] = "MIT";

    auto rt = roundtrip(doc);

    CHECK(rt.meta.count("author")  == 1, "meta: author key survives");
    CHECK(rt.meta.count("license") == 1, "meta: license key survives");
    if (rt.meta.count("author"))  CHECK(rt.meta["author"]  == "Test Author", "meta: author value survives");
    if (rt.meta.count("license")) CHECK(rt.meta["license"] == "MIT",         "meta: license value survives");
}

// ---------------------------------------------------------------------------
// STAB-0131 — doc.includes (and its include skip-sets) survive an MCB
// roundtrip. Without this, converting an XML scene that uses <include> to
// MCB and back would silently inline everything that used to live in the
// included library file into the main scene.
// ---------------------------------------------------------------------------

static void testIncludesList() {
    Mc3Document doc;
    doc.includes = {"shared_lib.mc3.xml", "materials.mc3.xml"};
    doc.includedDefs.insert("crate");
    doc.includedMaterials.insert("stone");
    doc.includedTextures.insert("tex1");

    auto rt = roundtrip(doc);

    CHECK(rt.includes.size() == 2, "includes: list size survives");
    if (rt.includes.size() == 2) {
        CHECK(rt.includes[0] == "shared_lib.mc3.xml", "includes: [0] survives, order preserved");
        CHECK(rt.includes[1] == "materials.mc3.xml",  "includes: [1] survives, order preserved");
    }
    CHECK(rt.includedDefs.count("crate") == 1,        "includes: includedDefs skip-set survives");
    CHECK(rt.includedMaterials.count("stone") == 1,   "includes: includedMaterials skip-set survives");
    CHECK(rt.includedTextures.count("tex1") == 1,      "includes: includedTextures skip-set survives");
}

// STAB-0144 — doc.metadata (the legacy <metadata><property> map, distinct
// from the newer doc.meta N7 map) survives an MCB roundtrip.
static void testLegacyMetadataMap() {
    Mc3Document doc;
    doc.metadata["old_key"] = "old_value";
    doc.metadata["another"] = "value2";

    auto rt = roundtrip(doc);

    CHECK(rt.metadata.count("old_key") == 1, "legacy metadata: old_key survives");
    CHECK(rt.metadata.count("another") == 1, "legacy metadata: another survives");
    if (rt.metadata.count("old_key")) CHECK(rt.metadata["old_key"] == "old_value", "legacy metadata: value survives");
    // Confirm doc.meta and doc.metadata don't cross-contaminate through MCB either.
    CHECK(rt.meta.empty(), "legacy metadata: doc.meta stays empty (no cross-contamination)");
}

// ---------------------------------------------------------------------------
// STAB-0141/0142/0143 — CSG, extrude, and deform were never exercised by any
// MCB test at all, despite McbWriter/McbReader having full support
// (writeCsgOp/writeExtrude/writeDeform).
// ---------------------------------------------------------------------------

static void testCsgOperationRoundtrip() {
    Mc3Document doc;
    auto csgNode = std::make_shared<Mc3Object>();
    csgNode->id = "diff1";
    csgNode->type = ObjectType::Difference;
    csgNode->csgOperation = Mc3CsgOperation{.csgType = CsgType::Difference};

    auto base = std::make_shared<Mc3Object>();
    base->id = "base1"; base->type = ObjectType::Box;
    base->primitive = Mc3Primitive{}; base->isCutter = false;

    auto cutter = std::make_shared<Mc3Object>();
    cutter->id = "cut1"; cutter->type = ObjectType::Sphere;
    cutter->primitive = Mc3Primitive{.primitiveType = PrimitiveType::Sphere};
    cutter->isCutter = true;

    csgNode->children.push_back(base);
    csgNode->children.push_back(cutter);
    doc.objects.push_back(csgNode);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "csg: node present after MCB roundtrip");
    if (rt.objects.empty()) return;
    const auto& node = rt.objects[0];
    CHECK(node->type == ObjectType::Difference, "csg: type==Difference");
    CHECK(node->csgOperation.has_value(), "csg: csgOperation present");
    if (node->csgOperation)
        CHECK(node->csgOperation->csgType == CsgType::Difference, "csg: csgType==Difference");
    CHECK(node->children.size() == 2, "csg: child count");
    if (node->children.size() == 2) {
        CHECK(node->children[0]->isCutter == false, "csg: base.isCutter==false");
        CHECK(node->children[1]->isCutter == true,  "csg: cutter.isCutter==true");
    }
}

static void testExtrudeRoundtrip() {
    Mc3Document doc;
    auto obj = std::make_shared<Mc3Object>();
    obj->id = "ext1"; obj->type = ObjectType::Extrude;
    Mc3Extrude ex;
    ex.crossSection.type   = CrossSectionType::Polygon;
    ex.crossSection.radius = 0.08f;
    ex.crossSection.sides  = 6;
    ex.path.type        = ExtrudePathType::Helix;
    ex.path.helixRadius = 0.5f;
    ex.path.helixHeight = 3.0f;
    ex.path.helixTurns  = 4.0f;
    ex.segments = 64;
    ex.caps     = true;
    obj->extrude = ex;
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "extrude: node present after MCB roundtrip");
    if (rt.objects.empty() || !rt.objects[0]->extrude) { fail("extrude: extrude data missing"); return; }
    const auto& rx = rt.objects[0]->extrude.value();
    CHECK(rx.crossSection.type == CrossSectionType::Polygon, "extrude: cs.type==Polygon");
    CHECK(rx.crossSection.sides == 6,                        "extrude: cs.sides");
    CHECKF(rx.crossSection.radius, 0.08f,                    "extrude: cs.radius");
    CHECK(rx.path.type == ExtrudePathType::Helix,             "extrude: path.type==Helix");
    CHECKF(rx.path.helixRadius, 0.5f,                         "extrude: path.helixRadius");
    CHECKF(rx.path.helixHeight, 3.0f,                         "extrude: path.helixHeight");
    CHECKF(rx.path.helixTurns, 4.0f,                          "extrude: path.helixTurns");
    CHECK(rx.segments == 64,                                  "extrude: segments");
    CHECK(rx.caps == true,                                    "extrude: caps");
}

static void testDeformRoundtrip() {
    Mc3Document doc;
    auto obj = std::make_shared<Mc3Object>();
    obj->id = "sph1"; obj->type = ObjectType::Sphere;
    obj->primitive = Mc3Primitive{.primitiveType = PrimitiveType::Sphere, .radius = 0.5f};
    obj->deform = Mc3Deform{.scale = {2.0f, 3.0f, 4.0f}};
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "deform: object present after MCB roundtrip");
    if (rt.objects.empty()) return;
    CHECK(rt.objects[0]->deform.has_value(), "deform: optional present");
    if (rt.objects[0]->deform) {
        CHECKF(rt.objects[0]->deform->scale[0], 2.0f, "deform: scale.x");
        CHECKF(rt.objects[0]->deform->scale[1], 3.0f, "deform: scale.y");
        CHECKF(rt.objects[0]->deform->scale[2], 4.0f, "deform: scale.z");
    }
}

// ---------------------------------------------------------------------------
// STAB-0132 / STAB-0145 — unknown-key skipping (forward compatibility)
// ---------------------------------------------------------------------------

// Minimal hand-rolled writers for constructing a byte-exact MCB stream that
// includes keys McbWriter itself would never emit — the only way to actually
// exercise McbReader's skipValue()/skipObject() unknown-key path.
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

static void testUnknownKeySkipping() {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, 0);                  // flags
    rawU8(out, 0); rawU8(out, 0);   // reserved
    rawU8(out, TAG_OBJ);            // root object

    // Known scalar field before any unknown key.
    rawKey(out, "model"); rawU8(out, TAG_STR); rawStr(out, "BeforeUnknown");

    // STAB-0132: a single unrecognized scalar key, as a future writer might add.
    rawKey(out, "xyz_future"); rawU8(out, TAG_I32); rawU32(out, 12345);

    // STAB-0145: an unrecognized key whose value is an entire new nested
    // TAG_OBJ *section* (simulates a future format version adding a whole
    // new document-level feature, not just one field).
    rawKey(out, "futureSection"); rawU8(out, TAG_OBJ);
    rawKey(out, "nestedUnknown"); rawU8(out, TAG_STR); rawStr(out, "ignored");
    rawEnd(out); // end futureSection

    // An unrecognized key holding a TAG_ARR of mixed known-tag elements.
    rawKey(out, "futureArray"); rawU8(out, TAG_ARR); rawU32(out, 2);
    rawU8(out, TAG_STR); rawStr(out, "a");
    rawU8(out, TAG_F32);
    { float f = 1.5f; uint32_t u; std::memcpy(&u, &f, 4); rawU32(out, u); }

    // Known scalar field after all the unknown keys — proves the reader's
    // cursor position is correct once skipValue() returns.
    rawKey(out, "unit"); rawU8(out, TAG_STR); rawStr(out, "AfterUnknown");

    rawEnd(out); // end root object

    std::istringstream in(out.str(), std::ios::binary);
    Mc3Document doc;
    bool threw = false;
    try {
        doc = loadFromBinary(in);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(!threw, "unknown-key: loadFromBinary does not throw on unrecognized keys/sections");
    CHECK(doc.model == "BeforeUnknown", "unknown-key: field before unknown key parses correctly");
    CHECK(doc.unit == "AfterUnknown",   "unknown-key: field after unknown scalar/object/array keys still parses (forward-compat)");
}

// ---------------------------------------------------------------------------
// STAB-0133 / STAB-0134 / STAB-0135 — malformed-input error handling
// ---------------------------------------------------------------------------

static void testTruncatedFile() {
    Mc3Document doc;
    doc.model = "TruncationTarget";
    auto obj = std::make_shared<Mc3Object>();
    obj->id = "cube1"; obj->name = "MyCube"; obj->type = ObjectType::Box;
    obj->primitive = Mc3Primitive{}; obj->primitive->size = {2.0f, 2.0f, 2.0f};
    doc.objects.push_back(obj);

    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    const std::string full = out.str();
    CHECK(full.size() > 50, "truncated: fixture is long enough to truncate meaningfully");

    const std::string truncated = full.substr(0, 50);
    std::istringstream in(truncated, std::ios::binary);
    bool threw = false;
    try {
        Mc3Document rt = loadFromBinary(in);
        (void)rt;
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw, "truncated: loadFromBinary() throws a clean error rather than crashing on a file truncated mid-document");
}

static void testAllZerosInput() {
    const std::string zeros(100, '\0');
    std::istringstream in(zeros, std::ios::binary);
    bool threw = false;
    try {
        Mc3Document rt = loadFromBinary(in);
        (void)rt;
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw, "all-zeros: loadFromBinary() throws a clean error on a corrupted (all-zero) header rather than crashing");
}

static void testSingleByteInput() {
    const std::string oneByte(1, '\0');
    std::istringstream in(oneByte, std::ios::binary);
    bool threw = false;
    try {
        Mc3Document rt = loadFromBinary(in);
        (void)rt;
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw, "single-byte: loadFromBinary() throws a clean error on a single-byte input rather than crashing");
}

// ---------------------------------------------------------------------------

int main() {
    testSmoke();
    testBasicScene();
    testSvgTexture();
    testEmbed();
    testScript();
    testSound();
    testMusic();
    testTrigger();
    testSceneState();
    testMeta();
    testIncludesList();
    testLegacyMetadataMap();
    testCsgOperationRoundtrip();
    testExtrudeRoundtrip();
    testDeformRoundtrip();
    testUnknownKeySkipping();
    testTruncatedFile();
    testAllZerosInput();
    testSingleByteInput();

    if (failures == 0)
        std::cout << "All MCB roundtrip tests passed.\n";
    else
        std::cerr << failures << " MCB roundtrip test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
