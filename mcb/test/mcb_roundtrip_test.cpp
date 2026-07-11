#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
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

#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
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
// STAB-0658 — doc.rotationUnits/eulerOrder were entirely missing from MCB;
// a document authored with non-default rotation conventions would silently
// revert to degrees/XYZ (reinterpreting every object's rotation) on a
// roundtrip.
// ---------------------------------------------------------------------------

static void testDocumentRotationConvention() {
    Mc3Document doc;
    doc.rotationUnits = "radians";
    doc.eulerOrder    = "ZYX";

    auto rt = roundtrip(doc);
    CHECK(rt.rotationUnits == "radians", "rotation convention: rotationUnits survives (was silently dropped)");
    CHECK(rt.eulerOrder    == "ZYX",     "rotation convention: eulerOrder survives (was silently dropped)");
}

// ---------------------------------------------------------------------------
// STAB-0659 — Mc3Environment::skyboxTexture (I2, equirectangular panorama)
// was entirely missing from MCB; its sibling backgroundTexture (I1) WAS
// covered, which previously masked "environment roundtrips fine" as true
// when it wasn't for this field. No prior mcb test covered Mc3Environment
// at all, so this also covers backgroundColor/backgroundTexture/fog.
// ---------------------------------------------------------------------------

static void testEnvironmentAllFields() {
    Mc3Document doc;
    Mc3Environment env;
    env.backgroundColor   = {0.1f, 0.2f, 0.3f};
    env.backgroundTexture = "textures/bg.png";
    env.skyboxTexture     = "textures/sky_panorama.hdr";
    Mc3Fog fog;
    fog.color   = {0.5f, 0.6f, 0.7f};
    fog.mode    = FogMode::Exponential;
    fog.start   = 5.0f;
    fog.end     = 50.0f;
    fog.density = 0.02f;
    env.fog = fog;
    doc.environment = env;

    auto rt = roundtrip(doc);
    CHECK(rt.environment.has_value(), "environment: present after roundtrip");
    if (!rt.environment) return;
    CHECKF(rt.environment->backgroundColor[2], 0.3f, "environment: backgroundColor.b survives");
    CHECK(rt.environment->backgroundTexture == "textures/bg.png", "environment: backgroundTexture survives");
    CHECK(rt.environment->skyboxTexture == "textures/sky_panorama.hdr",
          "environment: skyboxTexture survives (was silently dropped)");
    CHECK(rt.environment->fog.has_value(), "environment: fog present");
    if (rt.environment->fog) {
        CHECK(rt.environment->fog->mode == FogMode::Exponential, "environment: fog.mode survives");
        CHECKF(rt.environment->fog->start, 5.0f, "environment: fog.start survives");
        CHECKF(rt.environment->fog->density, 0.02f, "environment: fog.density survives");
    }
}

// ---------------------------------------------------------------------------
// STAB-0660 — Mc3Texture::mipMaps was entirely missing from MCB, the same
// bug class STAB-0654 just fixed on the XML side. No prior mcb test covered
// Mc3Texture at all, so this also covers wrapU/wrapV/filter/colorSpace.
// ---------------------------------------------------------------------------

static void testTextureAllFields() {
    Mc3Document doc;
    Mc3Texture tex;
    tex.name       = "Wall";
    tex.uri        = "textures/wall.png";
    tex.wrapU      = "clamp";
    tex.wrapV      = "mirror";
    tex.filter     = "nearest";
    tex.colorSpace = "linear";
    tex.mipMaps    = false;
    doc.textures["wall_tex"] = tex;

    auto rt = roundtrip(doc);
    CHECK(rt.textures.count("wall_tex") == 1, "texture: present after roundtrip");
    if (!rt.textures.count("wall_tex")) return;
    const auto& t = rt.textures.at("wall_tex");
    CHECK(t.name       == "Wall",              "texture: name survives");
    CHECK(t.uri        == "textures/wall.png", "texture: uri survives");
    CHECK(t.wrapU      == "clamp",              "texture: wrapU survives");
    CHECK(t.wrapV      == "mirror",             "texture: wrapV survives");
    CHECK(t.filter     == "nearest",            "texture: filter survives");
    CHECK(t.colorSpace == "linear",             "texture: colorSpace survives");
    CHECK(t.mipMaps    == false,                "texture: mipMaps=false survives (was silently dropped)");

    // Default (true) must round-trip too, without needing to be written.
    Mc3Document doc2;
    doc2.textures["tex2"] = Mc3Texture{"tex2", "bar.png"};
    auto rt2 = roundtrip(doc2);
    CHECK(rt2.textures.count("tex2") == 1 && rt2.textures.at("tex2").mipMaps == true,
          "texture: mipMaps default (true) survives roundtrip");
}

// ---------------------------------------------------------------------------
// STAB-0661 — per-object Mc3Object::metadata (opaque key/value pass-through,
// mirrors <metadata> in the XSD) was entirely missing from MCB. Distinct
// from document-level doc.metadata/doc.meta, which ARE covered (see
// testMeta above) — this asymmetry made the per-object gap easy to miss.
// ---------------------------------------------------------------------------

static void testObjectMetadata() {
    Mc3Document doc;
    auto obj  = std::make_shared<Mc3Object>();
    obj->id   = "obj1";
    obj->type = ObjectType::Box;
    obj->primitive = Mc3Primitive{};
    obj->metadata["source_format"] = "FBX";
    obj->metadata["original_id"]   = "12345";
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "object metadata: object present after roundtrip");
    if (rt.objects.empty()) return;
    const auto& m = rt.objects[0]->metadata;
    CHECK(m.size() == 2, "object metadata: both entries survive (was silently dropped)");
    CHECK(m.count("source_format") == 1 && m.at("source_format") == "FBX",
          "object metadata: source_format value survives");
    CHECK(m.count("original_id") == 1 && m.at("original_id") == "12345",
          "object metadata: original_id value survives");
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
    doc.includedEmbeds.insert("prop");

    auto rt = roundtrip(doc);

    CHECK(rt.includes.size() == 2, "includes: list size survives");
    if (rt.includes.size() == 2) {
        CHECK(rt.includes[0] == "shared_lib.mc3.xml", "includes: [0] survives, order preserved");
        CHECK(rt.includes[1] == "materials.mc3.xml",  "includes: [1] survives, order preserved");
    }
    CHECK(rt.includedDefs.count("crate") == 1,        "includes: includedDefs skip-set survives");
    CHECK(rt.includedMaterials.count("stone") == 1,   "includes: includedMaterials skip-set survives");
    CHECK(rt.includedTextures.count("tex1") == 1,      "includes: includedTextures skip-set survives");
    CHECK(rt.includedEmbeds.count("prop") == 1,        "includes: includedEmbeds skip-set survives");
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
// STAB-0148 — UTF-8 strings survive MCB roundtrip
// ---------------------------------------------------------------------------

static void testUtf8StringRoundtrip() {
    Mc3Document doc;
    doc.model = "\xC5\xBDlu\xC5\xA5ou\xC4\x8Dk\xC3\xBD k\xC5\xAF\xC5\x88"; // "Žluťoučký kůň"

    auto obj       = std::make_shared<Mc3Object>();
    const std::string nonAsciiName = "Slon_\xC4\x8Dlov\xC4\x9Bk"; // "Slon_člověk"
    const std::string emojiId      = "box_\xF0\x9F\x9A\x80";      // "box_🚀" (4-byte UTF-8 codepoint)
    obj->id        = emojiId;
    obj->name      = nonAsciiName;
    obj->type      = ObjectType::Box;
    obj->primitive = Mc3Primitive{};
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(rt.model == doc.model, "utf-8: multi-byte doc.model string survives MCB roundtrip");
    CHECK(!rt.objects.empty(), "utf-8: object present after MCB roundtrip");
    if (!rt.objects.empty()) {
        CHECK(rt.objects[0]->name == nonAsciiName, "utf-8: 2-byte-codepoint object name preserved exactly");
        CHECK(rt.objects[0]->id   == emojiId,      "utf-8: 4-byte-codepoint (emoji) object id preserved exactly");
    }
}

// ---------------------------------------------------------------------------
// STAB-0140 — McbWriter determinism (same input -> same bytes)
// ---------------------------------------------------------------------------

static void testWriterDeterminism() {
    Mc3Document doc;
    doc.model = "DeterminismTarget";
    doc.meta["author"]  = "Alice";
    doc.meta["license"] = "MIT";
    doc.metadata["legacyKey"] = "legacyValue";
    doc.includedDefs.insert("defA");
    doc.includedDefs.insert("defB");

    Mc3Material mat1; mat1.name = "Mat1"; mat1.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};
    Mc3Material mat2; mat2.name = "Mat2"; mat2.baseColor = {0.0f, 1.0f, 0.0f, 1.0f};
    doc.materials["Mat1"] = mat1;
    doc.materials["Mat2"] = mat2;

    for (int i = 0; i < 5; ++i) {
        auto obj = std::make_shared<Mc3Object>();
        obj->id   = "obj" + std::to_string(i);
        obj->name = "Object " + std::to_string(i);
        obj->type = ObjectType::Box;
        obj->primitive = Mc3Primitive{};
        obj->tags = {"tagA", "tagB"};
        doc.objects.push_back(obj);
    }

    std::ostringstream out1(std::ios::binary);
    std::ostringstream out2(std::ios::binary);
    saveToBinary(doc, out1);
    saveToBinary(doc, out2);

    CHECK(out1.str() == out2.str(),
          "determinism: two writes of the identical document produce byte-identical output "
          "(all Mc3Document maps/sets are std::map/std::set, so iteration order is key-sorted, not insertion- or pointer-order-dependent)");
}

// ---------------------------------------------------------------------------
// STAB-0150 — endianness: MCB's wire format is a fixed little-endian
// encoding regardless of host byte order
// ---------------------------------------------------------------------------

static void testEndiannessLittleEndian() {
    // 3.5f's IEEE-754 bit pattern is the well-known constant 0x40600000
    // (sign=0, exponent=128, mantissa=0x600000). McbWriter's wU32()/wF32()
    // build the wire bytes via explicit bit shifts (v & 0xFF, >>8, >>16,
    // >>24), not a raw multi-byte memcpy of an integer — so the byte order
    // written to disk is fixed and does not depend on the host's native
    // endianness. If that holds, the 4 bytes for 3.5f must appear in the
    // stream as 00 00 60 40, which this test verifies empirically against
    // a hand-computed expected sequence (not derived from the host's own
    // float layout).
    Mc3Document doc;
    auto obj = std::make_shared<Mc3Object>();
    obj->id = "endian_probe"; obj->type = ObjectType::Sphere;
    obj->primitive = Mc3Primitive{};
    obj->primitive->radius = 3.5f; // default is 0.5f, so this is guaranteed to be written

    doc.objects.push_back(obj);

    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    const std::string bytes = out.str();

    const unsigned char expected[4] = {0x00, 0x00, 0x60, 0x40};
    bool found = false;
    for (size_t i = 0; i + 4 <= bytes.size() && !found; ++i) {
        if (static_cast<unsigned char>(bytes[i])   == expected[0] &&
            static_cast<unsigned char>(bytes[i+1]) == expected[1] &&
            static_cast<unsigned char>(bytes[i+2]) == expected[2] &&
            static_cast<unsigned char>(bytes[i+3]) == expected[3]) {
            found = true;
        }
    }
    CHECK(found, "endianness: 3.5f's bytes appear in the wire format in little-endian order (00 00 60 40)");
}

// ---------------------------------------------------------------------------
// STAB-0137 — MCB writes all animation keyframes
// ---------------------------------------------------------------------------

static void testActionAnimationRoundtrip() {
    Mc3Document doc;
    Mc3Action action;
    action.name     = "Walk";
    action.duration = 2.5f;
    action.loop      = true;
    action.autoplay  = true; // real bug found & fixed this task: MCB never wrote/read this field at all
    action.timeScale = 2.0f; // STAB-0460

    Mc3Channel channel;
    channel.targetObject = "hero";
    channel.property     = AnimatedProperty::PositionY;
    for (int i = 0; i < 10; ++i) {
        const float t = static_cast<float>(i) * 0.25f;
        const float v = static_cast<float>(i);
        Mc3Keyframe kf;
        if      (i % 3 == 0) kf = Mc3Keyframe::step(t, v);
        else if (i % 3 == 1) kf = Mc3Keyframe::linear(t, v);
        else                 kf = Mc3Keyframe::bezier(t, v, {-0.2f, 0.1f}, {0.3f, -0.15f});
        channel.keyframes.push_back(kf);
    }
    action.channels.push_back(channel);
    doc.actions["Walk"] = action;

    auto rt = roundtrip(doc);
    CHECK(rt.actions.count("Walk") == 1, "action: 'Walk' survives MCB roundtrip");
    if (rt.actions.count("Walk") != 1) return;

    const auto& ract = rt.actions.at("Walk");
    CHECKF(ract.duration, action.duration, "action: duration survives");
    CHECK(ract.loop     == action.loop,     "action: loop survives");
    CHECK(ract.autoplay == action.autoplay, "action: autoplay survives (previously silently dropped by MCB)");
    CHECKF(ract.timeScale, action.timeScale, "action: timeScale survives (STAB-0460)");
    CHECK(ract.channels.size() == 1, "action: channel count survives");
    if (ract.channels.empty()) return;

    const auto& rch = ract.channels[0];
    CHECK(rch.targetObject == channel.targetObject, "action: channel targetObject survives");
    CHECK(rch.property     == channel.property,     "action: channel property survives");
    CHECK(rch.keyframes.size() == channel.keyframes.size(), "action: all 10 keyframes survive (none dropped)");
    for (size_t i = 0; i < rch.keyframes.size() && i < channel.keyframes.size(); ++i) {
        const auto& orig = channel.keyframes[i];
        const auto& got  = rch.keyframes[i];
        const std::string tag = "keyframe[" + std::to_string(i) + "]";
        CHECKF(got.time,          orig.time,          "action: " + tag + ".time survives");
        CHECKF(got.value,         orig.value,         "action: " + tag + ".value survives");
        CHECK(got.interpolation == orig.interpolation, "action: " + tag + ".interpolation survives");
        if (orig.interpolation == Interpolation::CubicBezier) {
            CHECKF(got.handleLeft.dt,  orig.handleLeft.dt,  "action: " + tag + ".handleLeft.dt survives");
            CHECKF(got.handleLeft.dv,  orig.handleLeft.dv,  "action: " + tag + ".handleLeft.dv survives");
            CHECKF(got.handleRight.dt, orig.handleRight.dt, "action: " + tag + ".handleRight.dt survives");
            CHECKF(got.handleRight.dv, orig.handleRight.dv, "action: " + tag + ".handleRight.dv survives");
        }
    }
}

// ---------------------------------------------------------------------------
// STAB-0138 — very large string values (>64KB) do not crash the writer
// ---------------------------------------------------------------------------

static void testLargeStringRoundtrip() {
    Mc3Document doc;
    std::string bigSource(100 * 1024, 'x'); // 100KB
    // Sprinkle distinguishable content at the start/end so truncation would be detectable.
    bigSource.replace(0, 6, "START:");
    bigSource.replace(bigSource.size() - 4, 4, ":END");

    Mc3Script script;
    script.id     = "bigScript";
    script.type   = "lua";
    script.source = bigSource;
    doc.scripts["bigScript"] = script;

    auto rt = roundtrip(doc);
    CHECK(rt.scripts.count("bigScript") == 1, "large-string: 100KB script survives MCB roundtrip");
    if (rt.scripts.count("bigScript") != 1) return;
    const auto& rs = rt.scripts.at("bigScript");
    CHECK(rs.source.size() == bigSource.size(), "large-string: 100KB source length is not truncated");
    CHECK(rs.source == bigSource, "large-string: 100KB source content is byte-identical (not just length)");
}

// ---------------------------------------------------------------------------
// STAB-0139 — MCB binary file size vs. equivalent XML
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Regression tests for a fresh bug sweep this session: McbReader had no
// recursion-depth limit (readObject/skipValue), and rRawStr allocated a
// claimed string length before validating it against the stream at all.
// Both were confirmed to be real, not theoretical: a ~20,000-level-deep
// <children> chain segfaulted the process outright (not a catchable
// exception), and a 23-byte crafted file claiming a ~4GB string forced
// ~4.1GB of committed memory and 1.66s of CPU before finally throwing.
// ---------------------------------------------------------------------------

// Writes one Mc3Object's serialized field-list body (the caller has already
// written the TAG_OBJ tag byte that precedes this). depth==0 writes an empty
// object; otherwise writes a single "children" entry and recurses.
static void writeNestedChildObjectBody(std::ostream& o, int depth) {
    if (depth <= 0) {
        rawEnd(o);
        return;
    }
    rawKey(o, "children");
    rawU8(o, TAG_ARR);
    rawU32(o, 1);
    rawU8(o, TAG_OBJ);
    writeNestedChildObjectBody(o, depth - 1);
    rawEnd(o);
}

static void testDeeplyNestedChildrenDoesNotCrash() {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, 0);                  // flags
    rawU8(out, 0); rawU8(out, 0);   // reserved
    rawU8(out, TAG_OBJ);            // root document object

    rawKey(out, "objects");
    rawU8(out, TAG_ARR);
    rawU32(out, 1);
    rawU8(out, TAG_OBJ);
    // 8x the reader's 256-level guard -- comfortably exceeds it without
    // needing anywhere near the ~20,000 levels that segfaulted the
    // unguarded reader.
    writeNestedChildObjectBody(out, 2000);

    rawEnd(out); // end root document object

    std::istringstream in(out.str(), std::ios::binary);
    bool threw = false;
    std::string errMsg;
    try {
        Mc3Document rt = loadFromBinary(in);
        (void)rt;
    } catch (const std::exception& e) {
        threw = true;
        errMsg = e.what();
    }
    CHECK(threw, "deeply-nested children: loadFromBinary() throws a clean "
                 "error instead of a native stack-overflow crash");
    CHECK(errMsg.find("nesting depth") != std::string::npos,
          "deeply-nested children: the error specifically names the "
          "recursion-depth guard, not some other failure");
}

// ---------------------------------------------------------------------------
// AUDIT-0038 — version validation now checks a range
// (MCB_MIN_SUPPORTED_VERSION..MCB_VERSION), not exact equality, to make
// room for the chained-upgrade mechanism. Both boundaries must still
// reject a header they don't cover.
// ---------------------------------------------------------------------------

static bool loadThrowsContaining(uint8_t version, const std::string& needle) {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, version);
    rawU8(out, 0);                  // flags
    rawU8(out, 0); rawU8(out, 0);   // reserved
    rawU8(out, TAG_OBJ);            // root document object
    rawEnd(out);

    std::istringstream in(out.str(), std::ios::binary);
    try {
        Mc3Document rt = loadFromBinary(in);
        (void)rt;
        return false;
    } catch (const std::exception& e) {
        return std::string(e.what()).find(needle) != std::string::npos;
    }
}

static void testVersionBelowMinSupportedRejected() {
    CHECK(MCB_MIN_SUPPORTED_VERSION > 0,
          "version bounds: MCB_MIN_SUPPORTED_VERSION > 0 (0 is guaranteed below it)");
    CHECK(loadThrowsContaining(0, "unsupported version"),
          "version: below MCB_MIN_SUPPORTED_VERSION is rejected (AUDIT-0038)");
}

static void testVersionAboveCurrentRejected() {
    CHECK(loadThrowsContaining(static_cast<uint8_t>(MCB_VERSION + 1), "unsupported version"),
          "version: above MCB_VERSION (a newer format this reader doesn't "
          "know) is rejected, not silently misparsed (AUDIT-0038)");
}

// AUD-019: MCB_FORMAT.md documents that a file with MCB_FLAG_COMPRESSED set
// is "rejected with a clear error, not silently misread" -- a guarantee
// that had never actually been exercised by a test (only that the WRITER
// never sets the bit, via testSmoke's flags==0 check). Hand-write an
// otherwise-valid, empty-document header with that bit set and confirm the
// reader takes the documented reject path.
static void testCompressedFlagRejected() {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, MCB_FLAG_COMPRESSED); // flags
    rawU8(out, 0); rawU8(out, 0);    // reserved
    rawU8(out, TAG_OBJ);             // root document object
    rawEnd(out);

    std::istringstream in(out.str(), std::ios::binary);
    bool threw = false;
    std::string what;
    try {
        Mc3Document rt = loadFromBinary(in);
        (void)rt;
    } catch (const std::exception& e) {
        threw = true;
        what = e.what();
    }
    CHECK(threw, "compressed flag: a file with MCB_FLAG_COMPRESSED set throws, "
          "instead of being silently misread as an uncompressed payload");
    CHECK(what.find("compressed format not yet supported") != std::string::npos,
          "compressed flag: the error message names the documented reason "
          "(got: " + what + ")");
}

static void testHugeStringLengthRejectedCleanly() {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, 0);
    rawU8(out, 0); rawU8(out, 0);
    rawU8(out, TAG_OBJ);             // root document object

    rawKey(out, "model");
    rawU8(out, TAG_STR);
    rawU32(out, 0xFFFFFFF0u);        // claims a ~4GB string length
    // Deliberately no actual string bytes follow -- the point is that the
    // reader must reject the claimed length before trying to honor it.

    rawEnd(out);

    std::istringstream in(out.str(), std::ios::binary);
    auto start = std::chrono::steady_clock::now();
    bool threw = false;
    std::string errMsg;
    try {
        Mc3Document rt = loadFromBinary(in);
        (void)rt;
    } catch (const std::exception& e) {
        threw = true;
        errMsg = e.what();
    }
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    CHECK(threw, "huge string length: loadFromBinary() rejects an absurd "
                 "claimed string length instead of allocating it");
    CHECK(errMsg.find("sanity limit") != std::string::npos,
          "huge string length: the error specifically names the sanity-limit guard");
    CHECK(elapsedMs < 1000,
          "huge string length: rejected in well under a second, not after a "
          "multi-second multi-gigabyte memory commit");
}

static void testHugeCollectionCountRejectedCleanly() {
    // AUDIT-0024..0034: every count-prefixed collection field (not just
    // strings) must reject an absurd claimed count before it's used to
    // .reserve() a vector. "lights" is one of the .reserve(n) call sites.
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, 0);
    rawU8(out, 0); rawU8(out, 0);
    rawU8(out, TAG_OBJ);             // root document object

    rawKey(out, "lights");
    rawU8(out, TAG_ARR);
    rawU32(out, 0xFFFFFFF0u);        // claims ~4 billion lights
    // Deliberately no actual entries follow -- the point is that the reader
    // must reject the claimed count before trying to reserve() for it.

    rawEnd(out);

    std::istringstream in(out.str(), std::ios::binary);
    auto start = std::chrono::steady_clock::now();
    bool threw = false;
    std::string errMsg;
    try {
        Mc3Document rt = loadFromBinary(in);
        (void)rt;
    } catch (const std::exception& e) {
        threw = true;
        errMsg = e.what();
    }
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    CHECK(threw, "huge collection count: loadFromBinary() rejects an absurd "
                 "claimed collection count instead of reserving for it");
    CHECK(errMsg.find("sanity limit") != std::string::npos,
          "huge collection count: the error specifically names the sanity-limit guard");
    CHECK(elapsedMs < 1000,
          "huge collection count: rejected in well under a second, not after a "
          "multi-second multi-gigabyte memory commit");
}

static void testFileSizeSmallerThanXml() {
    const auto xmlPath = std::filesystem::path(__FILE__).parent_path() / ".." / ".." / "test" / "house.mc3.xml";
    std::error_code ec;
    const auto xmlSize = std::filesystem::file_size(xmlPath, ec);
    CHECK(!ec, "file-size: test/house.mc3.xml exists and is readable");
    if (ec) return;

    Mc3Document doc = Mc3Document::loadFromFile(xmlPath);
    std::ostringstream out(std::ios::binary);
    saveToBinary(doc, out);
    const auto mcbSize = out.str().size();

    std::cout << "INFO: house.mc3.xml = " << xmlSize << " bytes, house.mcb = " << mcbSize
              << " bytes (" << (100.0 * static_cast<double>(mcbSize) / static_cast<double>(xmlSize))
              << "% of XML size)\n";
    CHECK(mcbSize < xmlSize,
          "file-size: MCB encoding of house.mc3.xml is smaller than its XML source");
}

// ---------------------------------------------------------------------------
// AUD-015 — known-key tag/type validation
// ---------------------------------------------------------------------------

// A corrupt file whose known field carries a tag that doesn't match what the
// reader expects for that key (e.g. "visible" -- normally TAG_BOOL -- tagged
// as TAG_STR instead) must be rejected with a clear type-mismatch error at
// the field, not silently decoded as the wrong type / desynced downstream.
static void testKnownKeyTagMismatchRejected() {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, 0);                  // flags
    rawU8(out, 0); rawU8(out, 0);   // reserved
    rawU8(out, TAG_OBJ);            // root object

    rawKey(out, "model"); rawU8(out, TAG_STR); rawStr(out, "TagMismatch");
    rawKey(out, "objects"); rawU8(out, TAG_ARR); rawU32(out, 1);
    rawU8(out, TAG_OBJ);
        rawKey(out, "name"); rawU8(out, TAG_STR); rawStr(out, "Obj");
        // "visible" is normally TAG_BOOL (1 byte). Tag it TAG_STR instead --
        // the reader must reject this at the field, not read the following
        // length-prefixed bytes as if they were a bool.
        rawKey(out, "visible"); rawU8(out, TAG_STR); rawStr(out, "true");
    rawEnd(out); // end object
    rawEnd(out); // end root object

    std::istringstream in(out.str(), std::ios::binary);
    bool threw = false;
    std::string what;
    try {
        Mc3Document doc = loadFromBinary(in);
        (void)doc;
    } catch (const std::exception& e) {
        threw = true;
        what = e.what();
    }
    CHECK(threw, "tag-mismatch: loadFromBinary rejects 'visible' tagged as TAG_STR instead of TAG_BOOL");
    CHECK(what.find("visible") != std::string::npos,
          "tag-mismatch: error message names the offending key ('visible')");
}

// A file where every known field's tag genuinely matches must still load
// correctly -- the new check must not be a false-positive trap on valid input.
static void testKnownKeyTagMatchStillLoads() {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, 0);
    rawU8(out, 0); rawU8(out, 0);
    rawU8(out, TAG_OBJ);

    rawKey(out, "objects"); rawU8(out, TAG_ARR); rawU32(out, 1);
    rawU8(out, TAG_OBJ);
        rawKey(out, "name");    rawU8(out, TAG_STR);  rawStr(out, "Obj");
        rawKey(out, "visible"); rawU8(out, TAG_BOOL); rawU8(out, 1);
    rawEnd(out);
    rawEnd(out);

    std::istringstream in(out.str(), std::ios::binary);
    bool threw = false;
    Mc3Document doc;
    try {
        doc = loadFromBinary(in);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(!threw, "tag-match: correctly-tagged 'visible' field loads without error");
    CHECK(!threw && doc.objects.size() == 1 && doc.objects[0]->visible == true,
          "tag-match: 'visible' value decodes correctly (true)");
}

// ---------------------------------------------------------------------------
// AUD-017 — enum fields clamped to a valid enumerator on out-of-range input
// ---------------------------------------------------------------------------

// A corrupt/hostile file can claim any int32 for an enum-typed field (the
// tag itself is correctly TAG_I32 -- this isn't a type-mismatch case like
// AUD-015 above, the VALUE is simply out of the enum's real range). Reading
// it must not store an unhandled enumerator value for a downstream switch
// (mesh generation, glTF export) to mishandle -- it must clamp to
// enumerator 0, the same forward-compat philosophy already used for
// unknown keys.
static void testOutOfRangeEnumClampedToDefault() {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, 0);                  // flags
    rawU8(out, 0); rawU8(out, 0);   // reserved
    rawU8(out, TAG_OBJ);            // root object

    rawKey(out, "objects"); rawU8(out, TAG_ARR); rawU32(out, 1);
    rawU8(out, TAG_OBJ);
        rawKey(out, "name"); rawU8(out, TAG_STR); rawStr(out, "Obj");
        // ObjectType has 19 real enumerators (0..18) -- 9999 is nonsense.
        rawKey(out, "type"); rawU8(out, TAG_I32); rawU32(out, 9999);
    rawEnd(out); // end object
    rawEnd(out); // end root object

    std::istringstream in(out.str(), std::ios::binary);
    bool threw = false;
    Mc3Document doc;
    try {
        doc = loadFromBinary(in);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(!threw, "out-of-range enum: a nonsense 'type' int (correctly tagged "
          "TAG_I32, just out of range) does not reject the whole file");
    CHECK(!threw && doc.objects.size() == 1 &&
          doc.objects[0]->type == ObjectType::Box,
          "out-of-range enum: an out-of-range 'type' value is clamped to "
          "enumerator 0 (Box, Mc3Object's own default), not stored as an "
          "unhandled 9999");
}

// AUD-015 (full rollout): proves the tag-validation extension beyond
// readObject actually has teeth on a nested read* function, not just the
// one function the original partial fix covered. "transform" -> "position"
// is normally TAG_VEC3; tag it TAG_STR instead.
static void testNestedReadFunctionTagMismatchRejected() {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, 0);
    rawU8(out, 0); rawU8(out, 0);
    rawU8(out, TAG_OBJ);

    rawKey(out, "objects"); rawU8(out, TAG_ARR); rawU32(out, 1);
    rawU8(out, TAG_OBJ);
        rawKey(out, "name"); rawU8(out, TAG_STR); rawStr(out, "Obj");
        rawKey(out, "transform"); rawU8(out, TAG_OBJ);
            rawKey(out, "position"); rawU8(out, TAG_STR); rawStr(out, "not a vec3");
        rawEnd(out); // end transform
    rawEnd(out); // end object
    rawEnd(out); // end root object

    std::istringstream in(out.str(), std::ios::binary);
    bool threw = false;
    std::string what;
    try {
        Mc3Document doc = loadFromBinary(in);
        (void)doc;
    } catch (const std::exception& e) {
        threw = true;
        what = e.what();
    }
    CHECK(threw, "nested tag-mismatch: readTransform rejects 'position' "
          "tagged as TAG_STR instead of TAG_VEC3 (proves the AUD-015 "
          "rollout covers nested read* functions, not just readObject)");
    CHECK(what.find("position") != std::string::npos,
          "nested tag-mismatch: error message names the offending key ('position')");
}

// AUD-017 follow-up: the original enum-clamp pass covered 9 enum sites but
// missed 2 (ExtrudePathType in readPath, UvProjection in readUvMapping) --
// found while extending AUD-015's tag validation to those same functions.
// Both are nested 2+ levels deep (object -> extrude -> path -> type; object
// -> uvMapping -> projection), unlike the top-level "type" field the
// original AUD-017 test already covers.
static void testNestedOutOfRangeEnumsClampedToDefault() {
    std::ostringstream out(std::ios::binary);
    out.write(MCB_MAGIC, 4);
    rawU8(out, MCB_VERSION);
    rawU8(out, 0);
    rawU8(out, 0); rawU8(out, 0);
    rawU8(out, TAG_OBJ);

    rawKey(out, "objects"); rawU8(out, TAG_ARR); rawU32(out, 1);
    rawU8(out, TAG_OBJ);
        rawKey(out, "name"); rawU8(out, TAG_STR); rawStr(out, "Obj");
        rawKey(out, "extrude"); rawU8(out, TAG_OBJ);
            rawKey(out, "path"); rawU8(out, TAG_OBJ);
                // ExtrudePathType has 5 real enumerators (0..4) -- 777 is nonsense.
                rawKey(out, "type"); rawU8(out, TAG_I32); rawU32(out, 777);
            rawEnd(out); // end path
        rawEnd(out); // end extrude
        rawKey(out, "uvMapping"); rawU8(out, TAG_OBJ);
            // UvProjection has 3 real enumerators (0..2) -- 888 is nonsense.
            rawKey(out, "projection"); rawU8(out, TAG_I32); rawU32(out, 888);
        rawEnd(out); // end uvMapping
    rawEnd(out); // end object
    rawEnd(out); // end root object

    std::istringstream in(out.str(), std::ios::binary);
    bool threw = false;
    Mc3Document doc;
    try {
        doc = loadFromBinary(in);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(!threw, "nested out-of-range enums: nonsense ExtrudePathType/UvProjection "
          "ints do not reject the whole file");
    if (!threw) {
        CHECK(doc.objects.size() == 1 && doc.objects[0]->extrude.has_value() &&
              doc.objects[0]->extrude->path.type == ExtrudePathType::Line,
              "nested out-of-range enums: extrude.path.type=777 clamped to "
              "enumerator 0 (Line), not stored as unhandled 777");
        CHECK(doc.objects[0]->uvMapping.has_value() &&
              doc.objects[0]->uvMapping->projection == UvProjection::Planar,
              "nested out-of-range enums: uvMapping.projection=888 clamped to "
              "enumerator 0 (Planar), not stored as unhandled 888");
    }
}

// ---------------------------------------------------------------------------

int main() {
    testSmoke();
    testBasicScene();
    testDocumentRotationConvention();
    testEnvironmentAllFields();
    testTextureAllFields();
    testObjectMetadata();
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
    testUtf8StringRoundtrip();
    testWriterDeterminism();
    testEndiannessLittleEndian();
    testActionAnimationRoundtrip();
    testLargeStringRoundtrip();
    testDeeplyNestedChildrenDoesNotCrash();
    testVersionBelowMinSupportedRejected();
    testVersionAboveCurrentRejected();
    testCompressedFlagRejected();
    testHugeStringLengthRejectedCleanly();
    testHugeCollectionCountRejectedCleanly();
    testFileSizeSmallerThanXml();
    testKnownKeyTagMismatchRejected();
    testKnownKeyTagMatchStillLoads();
    testOutOfRangeEnumClampedToDefault();
    testNestedReadFunctionTagMismatchRejected();
    testNestedOutOfRangeEnumsClampedToDefault();

    if (failures == 0)
        std::cout << "All MCB roundtrip tests passed.\n";
    else
        std::cerr << failures << " MCB roundtrip test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
