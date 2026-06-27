#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3EmbedGltf.hpp"
#include "MeshCraft/Mc3/Mc3Music.hpp"
#include "MeshCraft/Mc3/Mc3SceneState.hpp"
#include "MeshCraft/Mc3/Mc3Script.hpp"
#include "MeshCraft/Mc3/Mc3Sound.hpp"
#include "MeshCraft/Mc3/Mc3SvgTexture.hpp"
#include "MeshCraft/Mc3/Mc3Trigger.hpp"

#include <cmath>
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

    if (failures == 0)
        std::cout << "All MCB roundtrip tests passed.\n";
    else
        std::cerr << failures << " MCB roundtrip test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
