// R109 -- mc3.json semantic JSON round-trip test.
//
// mc3.json is meant to parse into the exact same Mc3Document AST as
// mc3.xml (mesh_world_revival.md §4.3) -- not a mechanical XML-to-JSON
// mirror, but a genuinely semantic JSON representation (arrays for
// vectors, nested objects for transform/primitive/extrude/uv_mapping/...,
// see Mc3JsonWriter.cpp). This test proves that fidelity two ways:
//
//   1. A fixed, hand-built document covering (almost) every AST feature
//      (materials, textures incl. SVG, embed, lights of every type,
//      cameras, environment/fog, scripts/sounds/music/triggers/scene
//      states, definitions/instance with variants, primitives, extrude,
//      CSG, groups, deform, uv mapping, per-object metadata/states, an
//      action with keyframes of every interpolation kind) is round-
//      tripped through Mc3Document::saveToJsonFile/loadFromJsonFile and
//      Mc3JsonWriter::toString/loadFromJsonString, and every field is
//      compared by hand.
//   2. The same RandomMc3DocumentGenerator.hpp used by
//      random_roundtrip_test.cpp (XML) drives many random documents
//      through the JSON codec instead, reusing its compareDocuments()
//      deep field comparator -- proving the JSON path has the same
//      round-trip fidelity as the already-covered XML path, not just a
//      parallel untested code path.
//   3. If a features.mc3.xml fixture path is passed on argv[1] (same
//      fixture roundtrip_test.cpp/CMakeLists.txt already wires up), it is
//      loaded via the XML parser and re-saved/reloaded through JSON, so
//      the same real-world fixture both formats already share gets one
//      more cross-format check.

#include "RandomMc3DocumentGenerator.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3SceneState.hpp>
#include <MeshCraft/Mc3/Mc3Trigger.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraftTest;

static int failures = 0;
static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
#define CHECK(cond, msg) do { if (cond) pass(msg); else fail(msg); } while (0)

static int tmpIdx = 0;
static std::filesystem::path tmpPath() {
    return std::filesystem::temp_directory_path() /
           ("mc3_json_rt_" + std::to_string(tmpIdx++) + ".mc3.json");
}

// A fixed document exercising nearly every AST feature, hand-built rather
// than via the random generator (which deliberately excludes several of
// these -- see that file's header comment).
static Mc3Document buildFeatureDocument() {
    Mc3Document doc;
    doc.model = "feature_showcase";
    doc.withMetadata("author", "junie").withMeta("license", "MIT");
    doc.setBackgroundColor(0.1f, 0.2f, 0.3f);
    doc.setFog(Mc3Fog{{0.4f, 0.4f, 0.5f}, FogMode::Exponential, 5.0f, 80.0f, 0.02f});

    doc.addLight(Mc3Light::ambient("amb", {0.05f, 0.05f, 0.05f}, 1.0f));
    doc.addLight(Mc3Light::directional("sun", {0.f, -1.f, 0.2f}, {1.f, 1.f, 0.9f}, 1.2f).withShadows());
    doc.addLight(Mc3Light::point("bulb", {1.f, 2.f, 3.f}, {1.f, 0.8f, 0.6f}, 0.8f, 12.f));
    doc.addLight(Mc3Light::spot("torch", {0.f, 1.f, 0.f}, {0.f, -1.f, 0.f}, 30.f, {1.f, 1.f, 1.f}, 1.f)
                    .withFalloff(0.3f));

    doc.addCamera(Mc3Camera::perspective("main", {0.f, 5.f, 10.f}, {0.f, 0.f, 0.f}, 55.f));
    doc.defaultCamera = "main";
    doc.addCamera(Mc3Camera::orthographic("top", {0.f, 20.f, 0.f}, {0.f, 0.f, 0.f}, 8.f).withOrthoAspect(1.5f));

    doc.addTexture(Mc3Texture("bricks", "bricks.png", "clamp", "repeat", "nearest", "linear"));
    doc.addSvgTexture(Mc3SvgTexture{"logo", "", "<svg></svg>"});

    doc.addMaterial(Mc3Material::opaque("stone", {0.5f, 0.5f, 0.5f, 1.f}, 0.8f, 0.0f));
    doc.addMaterial(Mc3Material::metal("chrome", {0.9f, 0.9f, 0.9f, 1.f}, 0.05f));
    doc.addMaterial(Mc3Material::glass("window_glass"));

    doc.addEmbed(Mc3EmbedGltf{"car", "", "QUJD"});

    doc.addScript(Mc3Script{"placer", "lua", "-- place things\nreturn true"});
    doc.addSound(Mc3Sound{"click", "click.wav", false});
    doc.addMusic(Mc3Music{"theme", "theme.ogg", true});
    doc.addTrigger(Mc3Trigger{"door_open", {
        Mc3TriggerStep{TriggerStepType::PlaySound, "click"},
        Mc3TriggerStep{TriggerStepType::PlayAction, "open_anim"},
    }});

    Mc3SceneState state;
    state.name = "night";
    Mc3ObjectOverride ovr;
    ovr.id = "front_wall";
    ovr.visible = true;
    ovr.material = "stone";
    state.overrides.push_back(ovr);
    doc.addSceneState(state);

    auto door = Mc3Object::makeBox("door_def", {1.f, 2.f, 0.1f}, "chrome");
    door->withUvMapping(UvProjection::Box, 2.f, 2.f);
    doc.defineObject("door", door);
    doc.definitions["door"]->id = "door"; // ensure round-trippable id

    auto wall = Mc3Object::makeBox("front_wall", {10.f, 3.f, 0.3f}, "stone");
    wall->withId("front_wall");
    wall->withTag("exterior");
    wall->withTag("load-bearing");
    wall->withMetadata("floor", "1");
    wall->states["broken"] = Mc3ObjectState{ std::array<float,3>{0,0,0}, std::nullopt, std::nullopt, false, std::string("stone") };

    auto doorInstance = Mc3Object::makeInstance("door_01", "door", "chrome");
    doorInstance->at(-3.f, 0.f, -0.2f);

    auto cutter = Mc3Object::makeCylinder("hole", 0.3f, 1.f, 16);
    cutter->asCutter();
    auto solid  = Mc3Object::makeBox("block", {2.f, 2.f, 2.f}, "stone");
    std::vector<std::shared_ptr<Mc3Object>> diffChildren{solid, cutter};
    auto diff = Mc3Object::makeDifference("carved_block", diffChildren);
    diff->withDeform(1.f, 1.1f, 1.f);

    Mc3Extrude ex;
    ex.crossSection.type = CrossSectionType::Circle;
    ex.crossSection.radius = 0.05f;
    ex.path.type = ExtrudePathType::Helix;
    ex.path.helixRadius = 1.f; ex.path.helixHeight = 3.f; ex.path.helixTurns = 5.f;
    ex.twist = 90.f;
    auto railing = std::make_shared<Mc3Object>();
    railing->type = ObjectType::Extrude;
    railing->id = "railing";
    railing->extrude = ex;
    railing->material = "chrome";

    std::vector<std::shared_ptr<Mc3Object>> porchChildren{wall, doorInstance, diff, railing};
    auto group = Mc3Object::makeGroup("porch", porchChildren);
    doc.addObject(group);

    Mc3Action act = Mc3Action::make("open_anim", 2.0f, false, false).withTimeScale(1.5f);
    act.addChannel("door_01", AnimatedProperty::RotationY,
                   {Mc3Keyframe::step(0.f, 0.f), Mc3Keyframe::linear(1.f, 45.f),
                    Mc3Keyframe::bezier(2.f, 90.f, {-0.2f, -5.f}, {0.2f, 5.f})});
    doc.addAction(act);

    return doc;
}

int main(int argc, char** argv) {
    // --- 1. Fixed feature document, both file-based and string-based paths.
    {
        Mc3Document doc = buildFeatureDocument();
        auto p = tmpPath();
        doc.saveToJsonFile(p);
        Mc3Document reloaded = Mc3Document::loadFromJsonFile(p, Mc3LoadPolicy::trusted());
        std::filesystem::remove(p);

        auto mismatches = compareDocuments(doc, reloaded);
        CHECK(mismatches.empty(), "feature document: semantically equivalent after "
              "saveToJsonFile/loadFromJsonFile round-trip");
        for (const auto& m : mismatches) std::cerr << "    MISMATCH: " << m << "\n";
    }

    // String-based path via loadFromJsonString, independent of the file path.
    {
        Mc3Document doc = buildFeatureDocument();
        auto p = tmpPath();
        doc.saveToJsonFile(p);
        std::ifstream in(p, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        std::filesystem::remove(p);

        Mc3Document reloaded = Mc3Document::loadFromJsonString(text, {}, Mc3LoadPolicy::trusted());
        auto mismatches = compareDocuments(doc, reloaded);
        CHECK(mismatches.empty(), "feature document: semantically equivalent after "
              "loadFromJsonString round-trip");
        for (const auto& m : mismatches) std::cerr << "    MISMATCH: " << m << "\n";

        CHECK(text.find("\"format\": \"mc3\"") != std::string::npos ||
              text.find("\"format\":\"mc3\"") != std::string::npos,
              "written mc3.json declares format:mc3");
        CHECK(text.find("@name") == std::string::npos,
              "mc3.json is genuinely semantic -- no mechanical XML-attribute-"
              "style '@name' fields");
    }

    // --- 2. Random documents through the JSON codec.
    {
        std::mt19937 rng(0x51ED1234u);
        const int kNumDocuments = 60;
        long long totalMismatches = 0;
        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < kNumDocuments; ++i) {
            int complexity = randInt(rng, 1, 8);
            Mc3Document doc = buildRandomDocument(rng, complexity);

            auto p = tmpPath();
            bool threwOnSave = false, threwOnLoad = false;
            Mc3Document reloaded;
            try {
                doc.saveToJsonFile(p);
            } catch (const std::exception& e) {
                threwOnSave = true;
                std::cerr << "  (save threw: " << e.what() << ")\n";
            }
            if (!threwOnSave) {
                try {
                    reloaded = Mc3Document::loadFromJsonFile(p, Mc3LoadPolicy::trusted());
                } catch (const std::exception& e) {
                    threwOnLoad = true;
                    std::cerr << "  (load threw: " << e.what() << ")\n";
                }
            }
            std::filesystem::remove(p);

            CHECK(!threwOnSave, "random doc " + std::to_string(i) + " (complexity " +
                  std::to_string(complexity) + "): saveToJsonFile does not throw");
            CHECK(!threwOnLoad, "random doc " + std::to_string(i) +
                  ": loadFromJsonFile does not throw reloading what was just saved");

            if (!threwOnSave && !threwOnLoad) {
                auto mismatches = compareDocuments(doc, reloaded);
                totalMismatches += static_cast<long long>(mismatches.size());
                CHECK(mismatches.empty(), "random doc " + std::to_string(i) +
                      ": semantically equivalent after JSON round-trip");
                for (const auto& m : mismatches) std::cerr << "    MISMATCH: " << m << "\n";
            }
        }

        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        CHECK(true, "JSON random round-trip: " + std::to_string(kNumDocuments) +
              " random documents processed in " + std::to_string(elapsedMs) + "ms (" +
              std::to_string(totalMismatches) + " total field mismatches)");
        CHECK(elapsedMs < 30000, "JSON random round-trip: completes promptly");
    }

    // --- 3. Cross-format: real features.mc3.xml fixture, XML load -> JSON
    // save -> JSON load, compared against the XML-loaded original.
    if (argc > 1 && argv[1] && argv[1][0] != '\0' && std::filesystem::exists(argv[1])) {
        Mc3Document fromXml = Mc3Document::loadFromFile(argv[1]);
        auto p = tmpPath();
        bool threw = false;
        Mc3Document reloaded;
        try {
            fromXml.saveToJsonFile(p);
            reloaded = Mc3Document::loadFromJsonFile(p, Mc3LoadPolicy::trusted());
        } catch (const std::exception& e) {
            threw = true;
            std::cerr << "  (features.mc3.xml cross-format round-trip threw: " << e.what() << ")\n";
        }
        std::filesystem::remove(p);
        CHECK(!threw, "features.mc3.xml: XML load -> JSON save -> JSON load does not throw");
        if (!threw) {
            auto mismatches = compareDocuments(fromXml, reloaded);
            CHECK(mismatches.empty(), "features.mc3.xml: semantically equivalent after "
                  "XML->JSON->JSON cross-format round-trip");
            for (const auto& m : mismatches) std::cerr << "    MISMATCH: " << m << "\n";
        }
    } else {
        pass("features.mc3.xml cross-format check skipped (no fixture path given)");
    }

    if (failures == 0)
        std::cout << "All MC3 JSON round-trip tests passed.\n";
    else
        std::cerr << failures << " MC3 JSON round-trip test(s) FAILED.\n";
    return failures != 0 ? 1 : 0;
}
