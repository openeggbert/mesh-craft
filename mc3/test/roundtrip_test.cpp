#include <MeshCraft/Mc3/Mc3Animation.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3EmbedGltf.hpp>
#include <MeshCraft/Mc3/Mc3Music.hpp>
#include <MeshCraft/Mc3/Mc3SceneState.hpp>
#include <MeshCraft/Mc3/Mc3Script.hpp>
#include <MeshCraft/Mc3/Mc3Sound.hpp>
#include <MeshCraft/Mc3/Mc3Trigger.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static int tmpIdx   = 0;

static void pass(const std::string& msg) { std::cout << "PASS: " << msg << "\n"; }
static void fail(const std::string& msg) { std::cerr << "FAIL: " << msg << "\n"; ++failures; }

#define CHECK(cond, msg)  do { if (cond) pass(msg); else fail(msg); } while(0)
#define CHECKF(a, b, msg) CHECK(std::abs((a)-(b)) < 1e-5f, msg)

static std::filesystem::path tmpPath() {
    return std::filesystem::temp_directory_path() /
           ("mc3_rt_" + std::to_string(tmpIdx++) + ".mc3.xml");
}

static Mc3Document roundtrip(Mc3Document doc) {
    auto p = tmpPath();
    doc.saveToFile(p);
    auto loaded = Mc3Document::loadFromFile(p);
    std::filesystem::remove(p);
    return loaded;
}

// Round-trips through an explicit path instead of the auto-generated
// ASCII-only tmpPath() — lets STAB-0555/0556 exercise UTF-8 filenames and
// paths containing spaces.
static Mc3Document roundtripAt(Mc3Document doc, const std::filesystem::path& p) {
    doc.saveToFile(p);
    auto loaded = Mc3Document::loadFromFile(p);
    std::filesystem::remove(p);
    return loaded;
}

// ---------------------------------------------------------------------------

static void testVisible() {
    Mc3Document doc;
    auto obj          = std::make_shared<Mc3Object>();
    obj->id           = "box1";
    obj->name         = "HiddenBox";
    obj->type         = ObjectType::Box;
    obj->primitive    = Mc3Primitive{};
    obj->visible      = false;
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "visible: object present after round-trip");
    if (!rt.objects.empty())
        CHECK(rt.objects[0]->visible == false, "visible: false preserved");
}

static void testVisibleDefault() {
    Mc3Document doc;
    auto obj       = std::make_shared<Mc3Object>();
    obj->id        = "box2";
    obj->type      = ObjectType::Box;
    obj->primitive = Mc3Primitive{};
    // visible defaults to true — should still round-trip as true
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    if (!rt.objects.empty())
        CHECK(rt.objects[0]->visible == true, "visible: true default preserved");
}

static void testDeform() {
    Mc3Document doc;
    auto obj       = std::make_shared<Mc3Object>();
    obj->id        = "sph1";
    obj->type      = ObjectType::Sphere;
    obj->primitive = Mc3Primitive{.primitiveType=PrimitiveType::Sphere, .radius=0.5f};
    obj->deform    = Mc3Deform{.scale={2.0f, 3.0f, 4.0f}};
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "deform: object present");
    if (!rt.objects.empty()) {
        CHECK(rt.objects[0]->deform.has_value(), "deform: optional present");
        if (rt.objects[0]->deform) {
            CHECKF(rt.objects[0]->deform->scale[0], 2.0f, "deform: scale.x");
            CHECKF(rt.objects[0]->deform->scale[1], 3.0f, "deform: scale.y");
            CHECKF(rt.objects[0]->deform->scale[2], 4.0f, "deform: scale.z");
        }
    }
}

// STAB-0436: material colors are written as plain decimal floats
// (Mc3XmlWriter.cpp's vec4Str/vec3Str, "%.6g" == 6 significant digits), never
// quantized to uint8 anywhere in the mc3<->glTF pipeline. Confirms a value
// that isn't a round decimal (205/255, plausible from an 8-bit color picker)
// survives within the row's own 1e-5 tolerance despite the 6-sig-fig
// serialization, and that a simple value round-trips exactly.
static void testMaterialColorPrecisionRoundtrip() {
    Mc3Document doc;
    Mc3Material mat;
    mat.name = "precise";
    mat.baseColor = {0.8f, 0.3f, 0.1f, 1.0f};
    mat.emissiveColor = {205.0f/255.0f, 128.0f/255.0f, 64.0f/255.0f};
    doc.materials["precise"] = mat;

    auto obj = std::make_shared<Mc3Object>();
    obj->id = "o1"; obj->type = ObjectType::Box;
    obj->primitive = Mc3Primitive{}; obj->material = "precise";
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(rt.materials.count("precise") == 1, "color precision: material present");
    if (!rt.materials.count("precise")) return;
    const auto& m = rt.materials.at("precise");
    CHECKF(m.baseColor[0], 0.8f, "color precision: base_color.r round decimal");
    CHECKF(m.baseColor[1], 0.3f, "color precision: base_color.g round decimal");
    CHECKF(m.baseColor[2], 0.1f, "color precision: base_color.b round decimal");
    CHECKF(m.emissiveColor[0], 205.0f/255.0f, "color precision: emissive.r (8-bit-derived value, within 1e-5)");
    CHECKF(m.emissiveColor[1], 128.0f/255.0f, "color precision: emissive.g (8-bit-derived value, within 1e-5)");
    CHECKF(m.emissiveColor[2], 64.0f/255.0f,  "color precision: emissive.b (8-bit-derived value, within 1e-5)");
}

// STAB-0437: a <material> with no id attribute parses with id="" (attr()'s
// default). Confirmed by reading GltfExporter.cpp's buildNode(): it guards
// `if (!matName.empty())` before ever looking up doc.materials by name, so an
// object with no material assigned (obj->material == "") never accidentally
// resolves to an unnamed material — the empty string means "no material",
// never "the material named empty string". Two materials that both omit id
// collide on the same "" key: no crash, silent last-write-wins (the same
// policy STAB-0424 already confirmed for genuine id collisions).
static void testUnnamedMaterialHandledGracefully() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="UnnamedMaterialTest">
  <materials>
    <material roughness="0.2"><base_color>0.1 0.1 0.1 1.0</base_color></material>
    <material roughness="0.9"><base_color>0.9 0.9 0.9 1.0</base_color></material>
  </materials>
  <objects>
    <box name="NoMaterialBox" size="1 1 1"/>
  </objects>
</mc3>)";
    }
    try {
        auto doc = Mc3Document::loadFromFile(xmlPath);
        std::filesystem::remove(xmlPath);

        CHECK(doc.materials.count("") == 1,
              "unnamed material: no crash, exactly one '' entry (last-write-wins on collision)");
        if (doc.materials.count("")) {
            CHECKF(doc.materials.at("").roughness, 0.9f,
                   "unnamed material: second (last) unnamed material's value survived, first silently dropped");
        }

        CHECK(!doc.objects.empty(), "unnamed material: materialless object still loaded");
        if (!doc.objects.empty())
            CHECK(doc.objects[0]->material.empty(),
                  "unnamed material: materialless object's material field stays empty, "
                  "not accidentally resolved to the unnamed material");
    } catch (const std::exception& e) {
        std::filesystem::remove(xmlPath);
        fail(std::string("unnamed material: threw unexpectedly: ") + e.what());
    }
}

// STAB-0468: two keyframes at the identical time must not crash (division by
// zero in linear interpolation, guarded; cubic bezier uses bisection, so it's
// inherently safe) and must have a well-defined, documented tie-break policy
// (first-declared wins) rather than whatever an unstable sort happens to do.
static void testDuplicateKeyframeTimePolicy() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="DuplicateKeyframeTimeTest">
  <objects>
    <box name="Box1" size="1 1 1"/>
  </objects>
  <actions>
    <action name="Snap" duration="1.0">
      <channel target="Box1" property="position.x">
        <keyframe time="0.5" value="1.0" interp="linear"/>
        <keyframe time="0.5" value="9.0" interp="linear"/>
        <keyframe time="1.0" value="2.0" interp="linear"/>
      </channel>
    </action>
  </actions>
</mc3>)";
    }
    try {
        auto doc = Mc3Document::loadFromFile(xmlPath);
        std::filesystem::remove(xmlPath);

        CHECK(doc.actions.count("Snap") == 1, "duplicate kf time: action present");
        if (!doc.actions.count("Snap")) return;
        const auto& ch = doc.actions.at("Snap").channels.at(0);
        CHECK(ch.keyframes.size() == 3, "duplicate kf time: all 3 keyframes kept (no dedup)");
        if (ch.keyframes.size() == 3) {
            // First-declared-wins: the kf with value=1.0 (declared first in the
            // XML) must sort before the kf with value=9.0 (declared second),
            // since std::stable_sort preserves original order for equal keys.
            CHECKF(ch.keyframes[0].time,  0.5f, "duplicate kf time: kf[0].time");
            CHECKF(ch.keyframes[0].value, 1.0f, "duplicate kf time: kf[0] is the first-declared duplicate");
            CHECKF(ch.keyframes[1].time,  0.5f, "duplicate kf time: kf[1].time");
            CHECKF(ch.keyframes[1].value, 9.0f, "duplicate kf time: kf[1] is the second-declared duplicate");
        }

        // evaluateChannel must not crash/NaN at or around the duplicate time.
        float v = evaluateChannel(ch, 0.5f);
        CHECK(std::isfinite(v), "duplicate kf time: evaluateChannel(0.5) is finite, not NaN/Inf");
        float v2 = evaluateChannel(ch, 0.75f);
        CHECK(std::isfinite(v2), "duplicate kf time: evaluateChannel(0.75) is finite, not NaN/Inf");
    } catch (const std::exception& e) {
        std::filesystem::remove(xmlPath);
        fail(std::string("duplicate kf time: threw unexpectedly: ") + e.what());
    }
}

// STAB-0432: Mc3UvMapping (projection/scale/offset/rotation) had no roundtrip
// test at all before this.
static void testUvMappingRoundtrip() {
    Mc3Document doc;
    auto obj       = std::make_shared<Mc3Object>();
    obj->id        = "box1";
    obj->type      = ObjectType::Box;
    obj->primitive = Mc3Primitive{.primitiveType=PrimitiveType::Cube, .size={1,1,1}};
    Mc3UvMapping uv;
    uv.projection = UvProjection::Box;
    uv.scaleU     = 2.0f;
    uv.scaleV     = 0.5f;
    uv.offsetU    = 0.25f;
    uv.offsetV    = 0.75f;
    uv.rotation   = 45.0f;
    obj->uvMapping = uv;
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "uv mapping: object present");
    if (rt.objects.empty()) return;
    CHECK(rt.objects[0]->uvMapping.has_value(), "uv mapping: optional present");
    if (!rt.objects[0]->uvMapping) return;
    const auto& m = *rt.objects[0]->uvMapping;
    CHECK(m.projection == UvProjection::Box, "uv mapping: projection==Box");
    CHECKF(m.scaleU,   2.0f,  "uv mapping: scale_u");
    CHECKF(m.scaleV,   0.5f,  "uv mapping: scale_v");
    CHECKF(m.offsetU,  0.25f, "uv mapping: offset_u");
    CHECKF(m.offsetV,  0.75f, "uv mapping: offset_v");
    CHECKF(m.rotation, 45.0f, "uv mapping: rotation");
}

static void testExtrudeArcCircle() {
    Mc3Document doc;
    auto obj    = std::make_shared<Mc3Object>();
    obj->id     = "ext1";
    obj->type   = ObjectType::Extrude;
    Mc3Extrude ex;
    ex.crossSection.type    = CrossSectionType::Circle;
    ex.crossSection.radius  = 0.12f;
    ex.crossSection.segments = 10;
    ex.path.type            = ExtrudePathType::Arc;
    ex.path.arcRadius       = 2.5f;
    ex.path.arcAngle        = 135.0f;
    ex.twist                = 45.0f;
    ex.segments             = 20;
    ex.caps                 = false;
    obj->extrude            = ex;
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "extrude arc: object present");
    if (rt.objects.empty()) return;
    const auto& o = rt.objects[0];
    CHECK(o->extrude.has_value(), "extrude arc: optional present");
    if (!o->extrude) return;
    const auto& rx = o->extrude.value();
    CHECK(rx.crossSection.type == CrossSectionType::Circle,   "extrude arc: cs.type==Circle");
    CHECKF(rx.crossSection.radius, 0.12f,                     "extrude arc: cs.radius");
    CHECK(rx.crossSection.segments == 10,                     "extrude arc: cs.segments");
    CHECK(rx.path.type == ExtrudePathType::Arc,               "extrude arc: path.type==Arc");
    CHECKF(rx.path.arcRadius, 2.5f,                           "extrude arc: path.arcRadius");
    CHECKF(rx.path.arcAngle, 135.0f,                          "extrude arc: path.arcAngle");
    CHECKF(rx.twist, 45.0f,                                   "extrude arc: twist");
    CHECK(rx.segments == 20,                                   "extrude arc: segments");
    CHECK(rx.caps == false,                                    "extrude arc: caps==false");
}

static void testExtrudeHelixPolygon() {
    Mc3Document doc;
    auto obj = std::make_shared<Mc3Object>();
    obj->id  = "ext2";
    obj->type = ObjectType::Extrude;
    Mc3Extrude ex;
    ex.crossSection.type    = CrossSectionType::Polygon;
    ex.crossSection.radius  = 0.08f;
    ex.crossSection.sides   = 6;
    ex.path.type            = ExtrudePathType::Helix;
    ex.path.helixRadius     = 0.5f;
    ex.path.helixHeight     = 3.0f;
    ex.path.helixTurns      = 4.0f;
    ex.segments             = 64;
    ex.caps                 = true;
    obj->extrude            = ex;
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    if (rt.objects.empty() || !rt.objects[0]->extrude) { fail("extrude helix: missing"); return; }
    const auto& rx = rt.objects[0]->extrude.value();
    CHECK(rx.crossSection.type == CrossSectionType::Polygon,  "extrude helix: cs.type==Polygon");
    CHECK(rx.crossSection.sides == 6,                         "extrude helix: cs.sides");
    CHECK(rx.path.type == ExtrudePathType::Helix,             "extrude helix: path.type==Helix");
    CHECKF(rx.path.helixRadius, 0.5f,                         "extrude helix: helixRadius");
    CHECKF(rx.path.helixHeight, 3.0f,                         "extrude helix: helixHeight");
    CHECKF(rx.path.helixTurns, 4.0f,                          "extrude helix: helixTurns");
    CHECK(rx.segments == 64,                                   "extrude helix: segments");
    CHECK(rx.caps == true,                                     "extrude helix: caps==true");
}

static void testExtrudePolyline() {
    Mc3Document doc;
    auto obj = std::make_shared<Mc3Object>();
    obj->id   = "ext3";
    obj->type = ObjectType::Extrude;
    Mc3Extrude ex;
    ex.crossSection.type   = CrossSectionType::Rect;
    ex.crossSection.width  = 0.2f;
    ex.crossSection.height = 0.1f;
    ex.path.type           = ExtrudePathType::Polyline;
    ex.path.points         = {
        Mc3PathPoint{{0,0,0}},
        Mc3PathPoint{{0,1,0}},
        Mc3PathPoint{{1,2,0}},
    };
    ex.segments = 8;
    obj->extrude = ex;
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    if (rt.objects.empty() || !rt.objects[0]->extrude) { fail("extrude polyline: missing"); return; }
    const auto& rx = rt.objects[0]->extrude.value();
    CHECK(rx.path.type == ExtrudePathType::Polyline,  "extrude polyline: path.type");
    CHECK(rx.path.points.size() == 3,                 "extrude polyline: point count");
    if (rx.path.points.size() == 3) {
        CHECKF(rx.path.points[1].position[1], 1.0f,  "extrude polyline: point[1].y");
        CHECKF(rx.path.points[2].position[0], 1.0f,  "extrude polyline: point[2].x");
    }
    CHECKF(rx.crossSection.width,  0.2f,  "extrude polyline: cs.width");
    CHECKF(rx.crossSection.height, 0.1f,  "extrude polyline: cs.height");
}

static void testExtrudeBezier() {
    Mc3Document doc;
    auto obj  = std::make_shared<Mc3Object>();
    obj->id   = "ext4";
    obj->type = ObjectType::Extrude;
    Mc3Extrude ex;
    ex.crossSection.type   = CrossSectionType::Circle;
    ex.crossSection.radius = 0.05f;
    ex.path.type           = ExtrudePathType::Bezier;
    ex.path.points         = {
        Mc3PathPoint{{0,0,0}, {0,0.5f,0}},
        Mc3PathPoint{{2,2,0}, {0,0.5f,0}},
    };
    ex.segments = 12;
    obj->extrude = ex;
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    if (rt.objects.empty() || !rt.objects[0]->extrude) { fail("extrude bezier: missing"); return; }
    const auto& rx = rt.objects[0]->extrude.value();
    CHECK(rx.path.type == ExtrudePathType::Bezier,   "extrude bezier: path.type");
    CHECK(rx.path.points.size() == 2,                "extrude bezier: point count");
    if (rx.path.points.size() == 2) {
        CHECKF(rx.path.points[0].controlIn[1], 0.5f, "extrude bezier: controlIn[0].y");
        CHECKF(rx.path.points[1].position[0], 2.0f,  "extrude bezier: point[1].x");
    }
}

static void testCsgDifferenceAndCutter() {
    Mc3Document doc;
    auto csgNode         = std::make_shared<Mc3Object>();
    csgNode->id          = "diff1";
    csgNode->type        = ObjectType::Difference;
    csgNode->csgOperation = Mc3CsgOperation{.csgType = CsgType::Difference};

    auto base            = std::make_shared<Mc3Object>();
    base->id             = "base1";
    base->type           = ObjectType::Box;
    base->primitive      = Mc3Primitive{};
    base->isCutter       = false;

    auto cutter          = std::make_shared<Mc3Object>();
    cutter->id           = "cut1";
    cutter->type         = ObjectType::Sphere;
    cutter->primitive    = Mc3Primitive{.primitiveType=PrimitiveType::Sphere};
    cutter->isCutter     = true;

    csgNode->children.push_back(base);
    csgNode->children.push_back(cutter);
    doc.objects.push_back(csgNode);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "csg: node present");
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

static void testGroupChildren() {
    Mc3Document doc;
    auto grp    = std::make_shared<Mc3Object>();
    grp->id     = "grp1";
    grp->type   = ObjectType::Group;
    for (int i = 0; i < 3; ++i) {
        auto child    = std::make_shared<Mc3Object>();
        child->id     = "child" + std::to_string(i);
        child->name   = "Child" + std::to_string(i);
        child->type   = ObjectType::Box;
        child->primitive = Mc3Primitive{};
        grp->children.push_back(child);
    }
    doc.objects.push_back(grp);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "group: node present");
    if (rt.objects.empty()) return;
    CHECK(rt.objects[0]->type == ObjectType::Group, "group: type==Group");
    CHECK(rt.objects[0]->children.size() == 3, "group: child count==3");
    if (rt.objects[0]->children.size() == 3)
        CHECK(rt.objects[0]->children[2]->name == "Child2", "group: child[2].name");
}

// STAB-0031: Union/Intersection (Difference already covered by
// testCsgDifferenceAndCutter above), Instance, and Area — object types not
// exercised by testAllPrimitiveTypes() (primitives only) or elsewhere.
static void testUnionAndIntersectionRoundtrip() {
    for (auto [type, csgType, label] : {
             std::tuple{ObjectType::Union, CsgType::Union, "union"},
             std::tuple{ObjectType::Intersection, CsgType::Intersection, "intersection"}}) {
        Mc3Document doc;
        auto node = std::make_shared<Mc3Object>();
        node->id = "csg1"; node->type = type;
        node->csgOperation = Mc3CsgOperation{.csgType = csgType};
        for (const char* childId : {"a", "b"}) {
            auto child = std::make_shared<Mc3Object>();
            child->id = childId; child->type = ObjectType::Box;
            child->primitive = Mc3Primitive{};
            node->children.push_back(child);
        }
        doc.objects.push_back(node);

        auto rt = roundtrip(doc);
        CHECK(!rt.objects.empty(), std::string(label) + ": node present");
        if (rt.objects.empty()) continue;
        CHECK(rt.objects[0]->type == type, std::string(label) + ": type preserved");
        CHECK(rt.objects[0]->csgOperation.has_value() &&
              rt.objects[0]->csgOperation->csgType == csgType,
              std::string(label) + ": csgType preserved");
        CHECK(rt.objects[0]->children.size() == 2, std::string(label) + ": child count==2");
    }
}

static void testInstanceRoundtrip() {
    Mc3Document doc;
    auto def = std::make_shared<Mc3Object>();
    def->id = "chairDef"; def->type = ObjectType::Box;
    def->primitive = Mc3Primitive{};
    doc.definitions["chairDef"] = def;

    auto mat = Mc3Material("Fabric", {0.5f, 0.1f, 0.1f, 1.0f});
    doc.materials["Fabric"] = mat;

    auto inst = std::make_shared<Mc3Object>();
    inst->id = "inst1"; inst->name = "Chair1"; inst->type = ObjectType::Instance;
    inst->definition = "chairDef";
    inst->materialOverride = "Fabric";
    doc.objects.push_back(inst);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "instance: node present");
    if (rt.objects.empty()) return;
    CHECK(rt.objects[0]->type == ObjectType::Instance, "instance: type preserved");
    CHECK(rt.objects[0]->definition == "chairDef", "instance: definition IDREF preserved");
    CHECK(rt.objects[0]->materialOverride == "Fabric", "instance: material_override preserved");
    CHECK(rt.definitions.count("chairDef") == 1, "instance: referenced definition survives");
}

static void testAreaRoundtrip() {
    Mc3Document doc;
    auto area = std::make_shared<Mc3Object>();
    area->id = "area1"; area->name = "TriggerZone"; area->type = ObjectType::Area;
    area->primitive = Mc3Primitive{.size = {4.0f, 2.0f, 4.0f}};
    doc.objects.push_back(area);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "area: node present");
    if (rt.objects.empty()) return;
    CHECK(rt.objects[0]->type == ObjectType::Area, "area: type preserved");
    CHECK(rt.objects[0]->primitive.has_value(), "area: primitive/size present");
    if (rt.objects[0]->primitive)
        CHECKF(rt.objects[0]->primitive->size[0], 4.0f, "area: size.x preserved");
}

// ---------------------------------------------------------------------------
// Animation tests
// ---------------------------------------------------------------------------

static void testAnimationLinear() {
    Mc3Document doc;
    Mc3Action action;
    action.name     = "Spin";
    action.duration = 3.0f;
    action.loop     = true;

    Mc3Channel ch;
    ch.targetObject = "Sphere1";
    ch.property     = AnimatedProperty::RotationY;
    ch.keyframes    = {
        {0.0f,   0.0f, Interpolation::Linear},
        {3.0f, 360.0f, Interpolation::Linear},
    };
    action.channels.push_back(ch);
    doc.actions["Spin"] = std::move(action);

    auto rt = roundtrip(doc);
    CHECK(rt.actions.size() == 1, "anim linear: action count==1");
    if (!rt.actions.count("Spin")) { fail("anim linear: action 'Spin' missing"); return; }
    const auto& a = rt.actions.at("Spin");
    CHECK(a.name == "Spin",     "anim linear: action.name");
    CHECKF(a.duration, 3.0f,   "anim linear: action.duration");
    CHECK(a.loop == true,       "anim linear: action.loop");
    CHECK(a.channels.size() == 1, "anim linear: channel count");
    if (a.channels.empty()) return;
    const auto& c = a.channels[0];
    CHECK(c.targetObject == "Sphere1",               "anim linear: channel.target");
    CHECK(c.property == AnimatedProperty::RotationY, "anim linear: channel.property");
    CHECK(c.keyframes.size() == 2,                   "anim linear: keyframe count");
    if (c.keyframes.size() == 2) {
        CHECKF(c.keyframes[0].time,   0.0f,   "anim linear: kf[0].time");
        CHECKF(c.keyframes[0].value,  0.0f,   "anim linear: kf[0].value");
        CHECK(c.keyframes[0].interpolation == Interpolation::Linear, "anim linear: kf[0].interp");
        CHECKF(c.keyframes[1].time,   3.0f,   "anim linear: kf[1].time");
        CHECKF(c.keyframes[1].value, 360.0f,  "anim linear: kf[1].value");
    }
}

static void testAnimationCubicBezier() {
    Mc3Document doc;
    Mc3Action action;
    action.name     = "Bounce";
    action.duration = 2.0f;
    action.loop     = true;

    Mc3Channel ch;
    ch.targetObject = "Box1";
    ch.property     = AnimatedProperty::PositionY;
    ch.keyframes    = {
        {0.0f, 0.0f, Interpolation::CubicBezier, {-0.2f, 0.0f}, { 0.2f,  2.0f}},
        {1.0f, 3.0f, Interpolation::CubicBezier, {-0.2f, 2.0f}, { 0.2f, -2.0f}},
    };
    action.channels.push_back(ch);
    doc.actions["Bounce"] = std::move(action);

    auto rt = roundtrip(doc);
    if (!rt.actions.count("Bounce")) { fail("anim cubic: action missing"); return; }
    const auto& a = rt.actions.at("Bounce");
    CHECK(a.channels.size() == 1, "anim cubic: channel count");
    if (a.channels.empty()) return;
    const auto& c = a.channels[0];
    CHECK(c.keyframes.size() == 2, "anim cubic: keyframe count");
    if (c.keyframes.size() < 2) return;
    const auto& k0 = c.keyframes[0];
    CHECK(k0.interpolation == Interpolation::CubicBezier, "anim cubic: kf[0].interp==CubicBezier");
    CHECKF(k0.handleLeft.dt,  -0.2f, "anim cubic: kf[0].handleLeft.dt");
    CHECKF(k0.handleLeft.dv,   0.0f, "anim cubic: kf[0].handleLeft.dv");
    CHECKF(k0.handleRight.dt,  0.2f, "anim cubic: kf[0].handleRight.dt");
    CHECKF(k0.handleRight.dv,  2.0f, "anim cubic: kf[0].handleRight.dv");
    const auto& k1 = c.keyframes[1];
    CHECKF(k1.handleLeft.dt,  -0.2f, "anim cubic: kf[1].handleLeft.dt");
    CHECKF(k1.handleRight.dv, -2.0f, "anim cubic: kf[1].handleRight.dv");
}

static void testAnimationStep() {
    Mc3Document doc;
    Mc3Action action;
    action.name     = "Flash";
    action.duration = 2.0f;
    action.loop     = true;

    Mc3Channel ch;
    ch.targetObject = "Plane1";
    ch.property     = AnimatedProperty::Visible;
    ch.keyframes    = {
        {0.0f, 1.0f, Interpolation::Step},
        {0.5f, 0.0f, Interpolation::Step},
        {1.0f, 1.0f, Interpolation::Step},
    };
    action.channels.push_back(ch);
    doc.actions["Flash"] = std::move(action);

    auto rt = roundtrip(doc);
    if (!rt.actions.count("Flash")) { fail("anim step: action missing"); return; }
    const auto& c = rt.actions.at("Flash").channels;
    CHECK(c.size() == 1, "anim step: channel count");
    if (c.empty()) return;
    CHECK(c[0].property == AnimatedProperty::Visible, "anim step: property==Visible");
    CHECK(c[0].keyframes.size() == 3, "anim step: keyframe count");
    if (c[0].keyframes.size() == 3) {
        CHECK(c[0].keyframes[0].interpolation == Interpolation::Step, "anim step: kf[0].interp==Step");
        CHECKF(c[0].keyframes[1].time,  0.5f, "anim step: kf[1].time");
        CHECKF(c[0].keyframes[1].value, 0.0f, "anim step: kf[1].value");
        CHECK(c[0].keyframes[2].interpolation == Interpolation::Step, "anim step: kf[2].interp==Step");
    }
}

static void testAnimationMultiAction() {
    Mc3Document doc;

    Mc3Action a1;
    a1.name = "Walk"; a1.duration = 1.0f;
    Mc3Channel c1; c1.targetObject = "Leg"; c1.property = AnimatedProperty::RotationX;
    c1.keyframes = {{0.0f, -30.0f, Interpolation::Linear}, {0.5f, 30.0f, Interpolation::Linear}};
    a1.channels.push_back(c1);
    doc.actions["Walk"] = std::move(a1);

    Mc3Action a2;
    a2.name = "Jump"; a2.duration = 0.5f;
    Mc3Channel c2; c2.targetObject = "Body"; c2.property = AnimatedProperty::PositionY;
    c2.keyframes = {{0.0f, 0.0f, Interpolation::Linear}, {0.25f, 2.0f, Interpolation::Linear}};
    a2.channels.push_back(c2);
    doc.actions["Jump"] = std::move(a2);

    auto rt = roundtrip(doc);
    CHECK(rt.actions.size() == 2,        "anim multi: action count==2");
    CHECK(rt.actions.count("Walk") == 1, "anim multi: 'Walk' present");
    CHECK(rt.actions.count("Jump") == 1, "anim multi: 'Jump' present");
    if (rt.actions.count("Walk"))
        CHECKF(rt.actions.at("Walk").duration, 1.0f, "anim multi: Walk.duration");
    if (rt.actions.count("Jump")) {
        CHECKF(rt.actions.at("Jump").duration, 0.5f, "anim multi: Jump.duration");
        const auto& jch = rt.actions.at("Jump").channels;
        CHECK(jch.size() == 1, "anim multi: Jump channel count");
        if (!jch.empty())
            CHECK(jch[0].targetObject == "Body", "anim multi: Jump channel.target");
    }
}

// STAB-0443: no prior test used any AnimatedProperty::Scale* channel.
static void testAnimationScaleRoundtrip() {
    Mc3Document doc;
    Mc3Action action;
    action.name     = "GrowShrink";
    action.duration = 1.5f;

    Mc3Channel ch;
    ch.targetObject = "Balloon";
    ch.property     = AnimatedProperty::ScaleX;
    ch.keyframes    = {
        Mc3Keyframe::linear(0.0f, 1.0f),
        Mc3Keyframe::linear(1.5f, 3.0f),
    };
    action.channels.push_back(ch);
    doc.actions["GrowShrink"] = std::move(action);

    auto rt = roundtrip(doc);
    CHECK(rt.actions.count("GrowShrink") == 1, "anim scale: action present");
    if (!rt.actions.count("GrowShrink")) return;
    const auto& a = rt.actions.at("GrowShrink");
    CHECK(a.channels.size() == 1, "anim scale: channel count");
    if (a.channels.empty()) return;
    const auto& c = a.channels[0];
    CHECK(c.targetObject == "Balloon", "anim scale: channel.target");
    CHECK(c.property == AnimatedProperty::ScaleX, "anim scale: channel.property==ScaleX");
    CHECK(c.keyframes.size() == 2, "anim scale: keyframe count");
    if (c.keyframes.size() == 2) {
        CHECKF(c.keyframes[0].value, 1.0f, "anim scale: kf[0].value");
        CHECKF(c.keyframes[1].value, 3.0f, "anim scale: kf[1].value");
    }
}

// STAB-0445: no prior test used any AnimatedProperty::Deform* channel.
static void testAnimationDeformRoundtrip() {
    Mc3Document doc;
    Mc3Action action;
    action.name     = "Squash";
    action.duration = 1.0f;

    Mc3Channel ch;
    ch.targetObject = "Ball";
    ch.property     = AnimatedProperty::DeformX;
    ch.keyframes    = {
        Mc3Keyframe::linear(0.0f, 1.0f),
        Mc3Keyframe::linear(1.0f, 1.8f),
    };
    action.channels.push_back(ch);
    doc.actions["Squash"] = std::move(action);

    auto rt = roundtrip(doc);
    CHECK(rt.actions.count("Squash") == 1, "anim deform: action present");
    if (!rt.actions.count("Squash")) return;
    const auto& a = rt.actions.at("Squash");
    CHECK(a.channels.size() == 1, "anim deform: channel count");
    if (a.channels.empty()) return;
    const auto& c = a.channels[0];
    CHECK(c.targetObject == "Ball", "anim deform: channel.target");
    CHECK(c.property == AnimatedProperty::DeformX, "anim deform: channel.property==DeformX");
    CHECK(c.keyframes.size() == 2, "anim deform: keyframe count");
    if (c.keyframes.size() == 2) {
        CHECKF(c.keyframes[0].value, 1.0f, "anim deform: kf[0].value");
        CHECKF(c.keyframes[1].value, 1.8f, "anim deform: kf[1].value");
    }
}

static void testAnimationEvaluate() {
    // Linear 0→1 over 1 second
    Mc3Channel lin;
    lin.targetObject = "Obj";
    lin.property     = AnimatedProperty::PositionX;
    lin.keyframes    = {
        {0.0f, 0.0f, Interpolation::Linear},
        {1.0f, 1.0f, Interpolation::Linear},
    };
    CHECKF(evaluateChannel(lin, 0.0f),  0.0f, "eval linear: t=0");
    CHECKF(evaluateChannel(lin, 0.5f),  0.5f, "eval linear: t=0.5");
    CHECKF(evaluateChannel(lin, 1.0f),  1.0f, "eval linear: t=1");
    CHECKF(evaluateChannel(lin, -1.0f), 0.0f, "eval linear: before start clamps");
    CHECKF(evaluateChannel(lin, 2.0f),  1.0f, "eval linear: after end clamps");

    // Step: value holds until next keyframe
    Mc3Channel step;
    step.targetObject = "Obj2";
    step.property     = AnimatedProperty::Visible;
    step.keyframes    = {
        {0.0f, 1.0f, Interpolation::Step},
        {0.5f, 0.0f, Interpolation::Step},
    };
    CHECKF(evaluateChannel(step, 0.0f),  1.0f, "eval step: t=0");
    CHECKF(evaluateChannel(step, 0.49f), 1.0f, "eval step: just before 0.5");
    CHECKF(evaluateChannel(step, 0.5f),  0.0f, "eval step: t=0.5");
    CHECKF(evaluateChannel(step, 1.0f),  0.0f, "eval step: after end clamps");
}

// ---------------------------------------------------------------------------
// STAB-0430 — material color/roughness/metallic/emissive animation roundtrip
// ---------------------------------------------------------------------------

// AnimatedProperty::MaterialBaseColorR/G/B/A, MaterialRoughness/Metallic, and
// MaterialEmissiveR/G/B are all defined and have a complete animatedPropertyName()/
// animatedPropertyFromName() string mapping (Mc3Animation.cpp), and the
// writer/parser both call through it (Mc3XmlWriter.cpp:676,
// Mc3XmlParser.cpp:659) — but no test had ever exercised a material property
// channel specifically, only position/rotation/scale/deform/visible.
static void testMaterialColorAnimationRoundtrip() {
    Mc3Document doc;
    Mc3Action action;
    action.name     = "Pulse";
    action.duration  = 2.0f;
    action.loop      = true;

    auto makeChannel = [](AnimatedProperty prop, float v0, float v1) {
        Mc3Channel ch;
        ch.targetObject = "GlowCube";
        ch.property     = prop;
        ch.keyframes    = {
            Mc3Keyframe::linear(0.0f, v0),
            Mc3Keyframe::linear(2.0f, v1),
        };
        return ch;
    };
    action.channels.push_back(makeChannel(AnimatedProperty::MaterialBaseColorR, 0.2f, 0.9f));
    action.channels.push_back(makeChannel(AnimatedProperty::MaterialBaseColorG, 0.3f, 0.1f));
    action.channels.push_back(makeChannel(AnimatedProperty::MaterialBaseColorB, 0.4f, 0.0f));
    action.channels.push_back(makeChannel(AnimatedProperty::MaterialRoughness,  0.8f, 0.1f));
    action.channels.push_back(makeChannel(AnimatedProperty::MaterialMetallic,  0.0f, 1.0f));
    action.channels.push_back(makeChannel(AnimatedProperty::MaterialEmissiveR, 0.0f, 1.0f));
    doc.actions["Pulse"] = std::move(action);

    auto rt = roundtrip(doc);
    CHECK(rt.actions.count("Pulse") == 1, "material anim: action 'Pulse' present");
    if (!rt.actions.count("Pulse")) return;
    const auto& a = rt.actions.at("Pulse");
    CHECK(a.channels.size() == 6, "material anim: 6 channels present");
    if (a.channels.size() != 6) return;

    static const AnimatedProperty expectedProps[6] = {
        AnimatedProperty::MaterialBaseColorR, AnimatedProperty::MaterialBaseColorG,
        AnimatedProperty::MaterialBaseColorB, AnimatedProperty::MaterialRoughness,
        AnimatedProperty::MaterialMetallic,   AnimatedProperty::MaterialEmissiveR,
    };
    static const char* propNames[6] = {
        "baseColorR", "baseColorG", "baseColorB", "roughness", "metallic", "emissiveR",
    };
    static const float v0s[6] = {0.2f, 0.3f, 0.4f, 0.8f, 0.0f, 0.0f};
    static const float v1s[6] = {0.9f, 0.1f, 0.0f, 0.1f, 1.0f, 1.0f};

    for (int i = 0; i < 6; ++i) {
        const auto& c = a.channels[i];
        CHECK(c.targetObject == "GlowCube",
              std::string("material anim: ") + propNames[i] + " channel.target");
        CHECK(c.property == expectedProps[i],
              std::string("material anim: ") + propNames[i] + " channel.property survived roundtrip");
        CHECK(c.keyframes.size() == 2,
              std::string("material anim: ") + propNames[i] + " keyframe count");
        if (c.keyframes.size() == 2) {
            CHECKF(c.keyframes[0].value, v0s[i],
                   std::string("material anim: ") + propNames[i] + " kf[0].value");
            CHECKF(c.keyframes[1].value, v1s[i],
                   std::string("material anim: ") + propNames[i] + " kf[1].value");
        }
    }
}

// ---------------------------------------------------------------------------
// Include override / nested include / cycle tests
// ---------------------------------------------------------------------------

static void testIncludeOverride(const std::string& featuresXmlPath) {
    auto xmlDir   = std::filesystem::path(featuresXmlPath).parent_path();
    auto sceneFile = xmlDir / "include_override_scene.mc3.xml";
    if (!std::filesystem::exists(sceneFile)) {
        std::cout << "SKIP: include_override_scene.mc3.xml not found\n";
        return;
    }
    try {
        auto doc = Mc3Document::loadFromFile(sceneFile);
        // The local override (stone = red) must win over the library (stone = grey)
        CHECK(doc.materials.count("stone") == 1, "override: stone material present");
        if (doc.materials.count("stone")) {
            const auto& stone = doc.materials.at("stone");
            CHECKF(stone.baseColor[0], 0.9f, "override: stone.r==0.9 (local red, not library grey)");
            CHECKF(stone.baseColor[1], 0.1f, "override: stone.g==0.1");
        }
        // After roundtrip the saved file must NOT skip the local override
        auto savedPath = xmlDir / "override_rt_tmp.mc3.xml";
        doc.saveToFile(savedPath);
        {
            std::ifstream f(savedPath);
            std::string saved((std::istreambuf_iterator<char>(f)), {});
            // The local stone definition must appear in the saved file
            CHECK(saved.find("id=\"stone\"") != std::string::npos,
                  "override rt: local stone definition written to saved file");
        }
        auto rt = Mc3Document::loadFromFile(savedPath);
        std::filesystem::remove(savedPath);
        CHECK(rt.materials.count("stone") == 1, "override rt: stone present after reload");
        if (rt.materials.count("stone"))
            CHECKF(rt.materials.at("stone").baseColor[0], 0.9f,
                   "override rt: red override preserved after reload");
    } catch (const std::exception& e) {
        fail(std::string("include override test threw: ") + e.what());
    }
}

static void testIncludeNested(const std::string& featuresXmlPath) {
    auto xmlDir    = std::filesystem::path(featuresXmlPath).parent_path();
    auto sceneFile = xmlDir / "include_nested_scene.mc3.xml";
    if (!std::filesystem::exists(sceneFile)) {
        std::cout << "SKIP: include_nested_scene.mc3.xml not found\n";
        return;
    }
    try {
        auto doc = Mc3Document::loadFromFile(sceneFile);
        // lib_a includes lib_b, so widget (from lib_b) must be accessible
        CHECK(doc.definitions.count("widget") == 1, "nested include: 'widget' from lib_b accessible");
        CHECK(doc.materials.count("wood") == 1,     "nested include: 'wood' from lib_a accessible");
        // doc.includes must only list lib_a (not lib_b — that's nested)
        CHECK(doc.includes.size() == 1, "nested include: only lib_a in doc.includes (not lib_b)");
        if (!doc.includes.empty())
            CHECK(doc.includes[0] == "include_lib_a.mc3.xml",
                  "nested include: doc.includes[0]==\"include_lib_a.mc3.xml\"");

        // --- Roundtrip: save to the same directory so relative paths resolve ---
        auto savedPath = xmlDir / "nested_rt_tmp.mc3.xml";
        doc.saveToFile(savedPath);
        {
            std::ifstream f(savedPath);
            std::string saved((std::istreambuf_iterator<char>(f)), {});
            // The saved file must re-emit the lib_a include
            CHECK(saved.find("include_lib_a.mc3.xml") != std::string::npos,
                  "nested rt: lib_a include present in saved file");
            // The saved file must NOT directly reference lib_b (that's nested inside lib_a)
            CHECK(saved.find("include_lib_b.mc3.xml") == std::string::npos,
                  "nested rt: lib_b NOT directly referenced in saved file");
            // The definitions from lib_b must also not be inlined
            CHECK(saved.find("id=\"widget\"") == std::string::npos,
                  "nested rt: widget definition NOT inlined in saved file");
        }

        // Reload: assets from both lib_a and lib_b must still be accessible
        auto rt = Mc3Document::loadFromFile(savedPath);
        std::filesystem::remove(savedPath);

        CHECK(rt.definitions.count("widget") == 1,
              "nested rt: 'widget' accessible after reload");
        CHECK(rt.materials.count("wood") == 1,
              "nested rt: 'wood' accessible after reload");
        CHECK(rt.includes.size() == 1,
              "nested rt: includes list has exactly one entry after reload");
        if (!rt.includes.empty())
            CHECK(rt.includes[0] == "include_lib_a.mc3.xml",
                  "nested rt: doc.includes[0] still lib_a after reload");
    } catch (const std::exception& e) {
        fail(std::string("include nested test threw: ") + e.what());
    }
}

static void testIncludeCycle(const std::string& featuresXmlPath) {
    auto xmlDir    = std::filesystem::path(featuresXmlPath).parent_path();
    auto cycleFile = xmlDir / "include_cycle_a.mc3.xml";
    if (!std::filesystem::exists(cycleFile)) {
        std::cout << "SKIP: include_cycle_a.mc3.xml not found\n";
        return;
    }
    bool threw = false;
    try {
        Mc3Document::loadFromFile(cycleFile);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw, "include cycle: loading cyclic includes must throw");
}

// ---------------------------------------------------------------------------
// Include tests
// ---------------------------------------------------------------------------

static void testInclude(const std::string& featuresXmlPath) {
    auto xmlDir    = std::filesystem::path(featuresXmlPath).parent_path();
    auto sceneFile = xmlDir / "scene_with_include.mc3.xml";
    auto libFile   = xmlDir / "mc3_library.mc3.xml";

    if (!std::filesystem::exists(sceneFile) || !std::filesystem::exists(libFile)) {
        std::cout << "SKIP: scene_with_include.mc3.xml or mc3_library.mc3.xml not found\n";
        return;
    }

    try {
        // --- Load ---
        auto doc = Mc3Document::loadFromFile(sceneFile);

        CHECK(doc.definitions.count("pillar") == 1, "include: 'pillar' definition loaded from library");
        CHECK(doc.definitions.count("crate")  == 1, "include: 'crate' definition loaded from library");
        CHECK(doc.materials.count("stone")    == 1, "include: 'stone' material loaded from library");
        CHECK(doc.materials.count("wood")     == 1, "include: 'wood' material loaded from library");

        CHECK(!doc.includes.empty(),                        "include: includes list non-empty");
        CHECK(doc.includes[0] == "mc3_library.mc3.xml",    "include: relative path recorded");

        CHECK(doc.includedDefs.count("pillar") == 1,       "include: 'pillar' tracked as included def");
        CHECK(doc.includedDefs.count("crate")  == 1,       "include: 'crate' tracked as included def");
        CHECK(doc.includedMaterials.count("stone") == 1,   "include: 'stone' tracked as included mat");
        CHECK(doc.includedMaterials.count("wood")  == 1,   "include: 'wood' tracked as included mat");

        // Scene objects from the main file
        CHECK(doc.objects.size() >= 4, "include: scene objects present (4 expected)");

        // --- Roundtrip: save to same directory so relative include path resolves ---
        auto savedPath = xmlDir / "scene_include_rt_tmp.mc3.xml";
        doc.saveToFile(savedPath);

        // Saved file must contain <include> and must NOT inline the library content
        {
            std::ifstream f(savedPath);
            std::string saved((std::istreambuf_iterator<char>(f)), {});
            CHECK(saved.find("<include") != std::string::npos,
                  "include rt: <include> element present in saved file");
            CHECK(saved.find("id=\"pillar\"") == std::string::npos,
                  "include rt: included definitions not inlined in saved file");
            CHECK(saved.find("id=\"stone\"") == std::string::npos,
                  "include rt: included materials not inlined in saved file");
        }

        // Reload: definitions should still be accessible via the re-emitted <include>
        auto rt = Mc3Document::loadFromFile(savedPath);
        std::filesystem::remove(savedPath);

        CHECK(rt.definitions.count("pillar") == 1, "include rt: 'pillar' accessible after reload");
        CHECK(rt.materials.count("stone")    == 1, "include rt: 'stone' accessible after reload");
        CHECK(rt.objects.size() >= 4,              "include rt: scene objects preserved");
        CHECK(!rt.includes.empty(),                "include rt: includes list preserved");

    } catch (const std::exception& e) {
        fail(std::string("include test threw: ") + e.what());
    }
}

// ---------------------------------------------------------------------------
// Include across directories (STAB-0550): a texture path or OBJ meshSource
// that's relative to the *included* file's own directory must still resolve
// correctly once merged into a main document that lives in a *different*
// directory. testInclude() above can't catch this because its fixtures
// (scene_with_include.mc3.xml + mc3_library.mc3.xml) happen to share the
// same directory, where doc.sourcePath and the include's own directory are
// identical by coincidence.
// ---------------------------------------------------------------------------

static void testIncludeAcrossDirectoriesRebasesRelativePaths() {
    auto root = std::filesystem::temp_directory_path() /
                ("mc3_include_dirs_" + std::to_string(tmpIdx++));
    auto assetsDir = root / "assets";
    auto scenesDir = root / "scenes";
    std::error_code ec;
    std::filesystem::create_directories(assetsDir / "textures", ec);
    std::filesystem::create_directories(scenesDir, ec);

    // The actual texture file the library's relative path should resolve to.
    auto realTexturePath = assetsDir / "textures" / "tree.png";
    { std::ofstream f(realTexturePath); f << "fake png bytes"; }

    auto libPath = assetsDir / "lib.mc3.xml";
    {
        std::ofstream f(libPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="Lib">
  <textures>
    <texture id="treeTex" uri="textures/tree.png"/>
  </textures>
  <definitions>
    <definition id="tree">
      <mesh name="Tree" src="models/tree.obj"/>
    </definition>
  </definitions>
</mc3>
)";
    }

    auto scenePath = scenesDir / "scene.mc3.xml";
    {
        std::ofstream f(scenePath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="Scene">
  <include file="../assets/lib.mc3.xml"/>
  <objects>
    <instance name="Tree1" definition="tree" position="0 0 0"/>
  </objects>
</mc3>
)";
    }

    try {
        auto doc = Mc3Document::loadFromFile(scenePath);

        CHECK(doc.textures.count("treeTex") == 1, "include across dirs: included texture is present");
        if (doc.textures.count("treeTex")) {
            auto resolved = std::filesystem::weakly_canonical(
                doc.sourcePath / doc.textures.at("treeTex").uri, ec);
            auto expected = std::filesystem::weakly_canonical(realTexturePath, ec);
            CHECK(resolved == expected,
                  "include across dirs: rebased texture uri resolves to the library's real "
                  "file (" + resolved.string() + " == " + expected.string() + ")");
        }

        CHECK(doc.definitions.count("tree") == 1, "include across dirs: included definition is present");
        if (doc.definitions.count("tree") && doc.definitions.at("tree")) {
            const auto& defObj = doc.definitions.at("tree");
            auto resolvedMesh = std::filesystem::weakly_canonical(
                doc.sourcePath / defObj->meshSource, ec);
            auto expectedMesh = std::filesystem::weakly_canonical(
                assetsDir / "models" / "tree.obj", ec);
            CHECK(resolvedMesh == expectedMesh,
                  "include across dirs: rebased definition meshSource resolves relative to "
                  "the library's own directory (" + resolvedMesh.string() + " == " +
                  expectedMesh.string() + ")");
        }
    } catch (const std::exception& e) {
        fail(std::string("include across directories test threw: ") + e.what());
    }

    std::filesystem::remove_all(root, ec);
}

// STAB-0091: an SVG texture (<texture type="svg">, sharing doc.textures'
// includedTextures id-tracking) merged from an <include> file must NOT be
// re-inlined into the main document's own saved file — same skip-set
// contract as regular textures/materials/definitions. Found and fixed a
// real bug here: the writer's svgTextures loop was missing the
// includedTextures check that the adjacent doc.textures loop already had.
static void testIncludedSvgTextureNotDuplicatedOnSave() {
    auto root = std::filesystem::temp_directory_path() /
                ("mc3_include_svg_" + std::to_string(tmpIdx++));
    std::error_code ec;
    std::filesystem::create_directories(root, ec);

    auto libPath = root / "lib.mc3.xml";
    {
        std::ofstream f(libPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="Lib">
  <textures>
    <texture id="logo" type="svg"><![CDATA[<svg></svg>]]></texture>
  </textures>
</mc3>
)";
    }

    auto scenePath = root / "scene.mc3.xml";
    {
        std::ofstream f(scenePath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="Scene">
  <include file="lib.mc3.xml"/>
  <objects/>
</mc3>
)";
    }

    try {
        auto doc = Mc3Document::loadFromFile(scenePath);
        CHECK(doc.svgTextures.count("logo") == 1, "included svg texture: present after load");
        CHECK(doc.includedTextures.count("logo") == 1, "included svg texture: tracked in includedTextures");

        auto savedPath = root / "scene_saved.mc3.xml";
        doc.saveToFile(savedPath);
        std::ifstream f(savedPath);
        std::string saved((std::istreambuf_iterator<char>(f)), {});
        CHECK(saved.find("<include") != std::string::npos,
              "included svg texture: <include> re-emitted in saved file");
        CHECK(saved.find("id=\"logo\"") == std::string::npos,
              "included svg texture: NOT re-inlined into the saved file (bug fixed this session)");
    } catch (const std::exception& e) {
        fail(std::string("included svg texture test threw: ") + e.what());
    }

    std::filesystem::remove_all(root, ec);
}

// STAB-0088: an <include file="..."/> whose filename contains spaces must
// resolve and load correctly — includePath is a std::filesystem::path built
// via selfPath.parent_path() / fileAttr (Mc3XmlParser.cpp), same
// space-tolerant path-joining already confirmed for top-level file paths
// (STAB-0556).
static void testIncludePathWithSpaces() {
    auto root = std::filesystem::temp_directory_path() /
                ("mc3_include_spaces_" + std::to_string(tmpIdx++));
    std::error_code ec;
    std::filesystem::create_directories(root, ec);

    auto libPath = root / "my shared library.mc3.xml";
    {
        std::ofstream f(libPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="Lib">
  <materials>
    <material id="paint"><base_color>0.2 0.5 0.8 1</base_color></material>
  </materials>
</mc3>
)";
    }

    auto scenePath = root / "scene.mc3.xml";
    {
        std::ofstream f(scenePath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="Scene">
  <include file="my shared library.mc3.xml"/>
  <objects/>
</mc3>
)";
    }

    try {
        auto doc = Mc3Document::loadFromFile(scenePath);
        CHECK(doc.materials.count("paint") == 1,
              "include path with spaces: material from included file is present");
    } catch (const std::exception& e) {
        fail(std::string("include path with spaces test threw: ") + e.what());
    }

    std::filesystem::remove_all(root, ec);
}

// STAB-0089: <include file="doesnotexist.mc3.xml"/> must throw a clear error
// naming the missing file (Mc3XmlParser.cpp's mergeInclude() explicitly
// throws std::runtime_error("Failed to load <include> file '<path>': ...")),
// not crash or silently produce an incomplete document.
static void testIncludeNonexistentFileClearError() {
    auto root = std::filesystem::temp_directory_path() /
                ("mc3_include_missing_" + std::to_string(tmpIdx++));
    std::error_code ec;
    std::filesystem::create_directories(root, ec);

    auto scenePath = root / "scene.mc3.xml";
    {
        std::ofstream f(scenePath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="Scene">
  <include file="does_not_exist.mc3.xml"/>
  <objects/>
</mc3>
)";
    }

    bool threw = false;
    std::string message;
    try {
        Mc3Document::loadFromFile(scenePath);
    } catch (const std::exception& e) {
        threw = true;
        message = e.what();
    }
    std::filesystem::remove_all(root, ec);

    CHECK(threw, "nonexistent include: throws (not silently incomplete)");
    CHECK(message.find("does_not_exist.mc3.xml") != std::string::npos,
          "nonexistent include: error message names the missing file");
}

// STAB-0109: malformed XML (e.g. an unclosed tag) must fail cleanly via
// Mc3XmlParser::parse()'s existing tinyxml2-error check
// (`throw std::runtime_error("Failed to load XML: <path>: <tinyxml2 error>")`),
// not crash or hang.
static void testMalformedXmlParseErrorIsClean() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <box name="Unclosed")" "\n"  // missing closing '>' / '/>'
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    bool threw = false;
    std::string message;
    try {
        Mc3Document::loadFromFile(xmlPath);
    } catch (const std::exception& e) {
        threw = true;
        message = e.what();
    }
    std::filesystem::remove(xmlPath);

    CHECK(threw, "malformed xml: throws cleanly (not a crash/hang)");
    CHECK(!message.empty(), "malformed xml: exception message is non-empty");
}

// STAB-0111: the writer emits the document's own `version` attribute back
// verbatim (Mc3XmlWriter.cpp: `root->SetAttribute("version", doc.version...)`),
// not a hardcoded constant.
static void testWriterEmitsVersionCorrectly() {
    Mc3Document doc;
    doc.version = "0.3";
    auto obj = std::make_shared<Mc3Object>();
    obj->id = "b1"; obj->type = ObjectType::Box; obj->primitive = Mc3Primitive{};
    doc.objects.push_back(obj);

    auto p = tmpPath();
    doc.saveToFile(p);
    std::ifstream f(p);
    std::string text((std::istreambuf_iterator<char>(f)), {});

    CHECK(text.find(R"(version="0.3")") != std::string::npos,
          "writer version: saved file's <mc3> root has version=\"0.3\"");

    auto rt = Mc3Document::loadFromFile(p);
    std::filesystem::remove(p);
    CHECK(rt.version == "0.3", "writer version: roundtrips back as \"0.3\"");
}

// STAB-0114: the writer preserves the insertion order of doc.objects (a
// plain std::vector) in the output XML — objects are not reordered by id,
// name, or type.
static void testWriterPreservesObjectInsertionOrder() {
    Mc3Document doc;
    for (const char* name : {"Zebra", "Apple", "Mango", "Banana"}) {
        auto obj = std::make_shared<Mc3Object>();
        obj->id = name; obj->name = name; obj->type = ObjectType::Box;
        obj->primitive = Mc3Primitive{};
        doc.objects.push_back(obj);
    }

    auto rt = roundtrip(doc);
    CHECK(rt.objects.size() == 4, "insertion order: all 4 objects present");
    if (rt.objects.size() == 4) {
        CHECK(rt.objects[0]->name == "Zebra",  "insertion order: [0]==Zebra (not alphabetical)");
        CHECK(rt.objects[1]->name == "Apple",  "insertion order: [1]==Apple");
        CHECK(rt.objects[2]->name == "Mango",  "insertion order: [2]==Mango");
        CHECK(rt.objects[3]->name == "Banana", "insertion order: [3]==Banana");
    }
}

// ---------------------------------------------------------------------------

static void testFeaturesXmlLoads(const std::string& path) {
    try {
        auto doc = Mc3Document::loadFromFile(path);
        CHECK(!doc.objects.empty(), "features.mc3.xml: objects not empty");
        CHECK(!doc.lights.empty(),  "features.mc3.xml: lights not empty");
        // Find the HiddenBox and verify visible==false
        std::function<std::shared_ptr<Mc3Object>(
            const std::vector<std::shared_ptr<Mc3Object>>&, const std::string&)> find;
        find = [&](const auto& list, const std::string& name) -> std::shared_ptr<Mc3Object> {
            for (const auto& o : list) {
                if (o->name == name) return o;
                if (auto f = find(o->children, name)) return f;
            }
            return nullptr;
        };
        auto hidden = find(doc.objects, "HiddenBox");
        CHECK(hidden != nullptr, "features.mc3.xml: HiddenBox found");
        if (hidden) CHECK(hidden->visible == false, "features.mc3.xml: HiddenBox.visible==false");
        auto arch = find(doc.objects, "Arch");
        CHECK(arch != nullptr, "features.mc3.xml: Arch extrude found");
        if (arch && arch->extrude)
            CHECK(arch->extrude->path.type == ExtrudePathType::Arc,
                  "features.mc3.xml: Arch path.type==Arc");

        // Animation checks
        CHECK(!doc.actions.empty(), "features.mc3.xml: actions not empty");
        CHECK(doc.actions.count("DemoSpin") == 1, "features.mc3.xml: DemoSpin action present");
        if (doc.actions.count("DemoSpin")) {
            const auto& a = doc.actions.at("DemoSpin");
            CHECKF(a.duration, 4.0f, "features.mc3.xml: DemoSpin.duration==4.0");
            CHECK(a.loop == true, "features.mc3.xml: DemoSpin.loop==true");
            CHECK(a.channels.size() == 2, "features.mc3.xml: DemoSpin has 2 channels");
            // First channel: PivotBox rotation.y, linear, 2 keyframes
            if (!a.channels.empty()) {
                const auto& c0 = a.channels[0];
                CHECK(c0.targetObject == "PivotBox", "features.mc3.xml: ch[0].target==PivotBox");
                CHECK(c0.property == AnimatedProperty::RotationY, "features.mc3.xml: ch[0].property==RotationY");
                CHECK(c0.keyframes.size() == 2, "features.mc3.xml: ch[0] has 2 keyframes");
                if (c0.keyframes.size() == 2) {
                    CHECK(c0.keyframes[0].interpolation == Interpolation::Linear,
                          "features.mc3.xml: ch[0].kf[0].interp==Linear");
                    CHECKF(c0.keyframes[1].value, 360.0f, "features.mc3.xml: ch[0].kf[1].value==360");
                }
            }
            // Second channel: Spring position.y, cubic, 3 keyframes with handles
            if (a.channels.size() >= 2) {
                const auto& c1 = a.channels[1];
                CHECK(c1.targetObject == "Spring", "features.mc3.xml: ch[1].target==Spring");
                CHECK(c1.property == AnimatedProperty::PositionY, "features.mc3.xml: ch[1].property==PositionY");
                CHECK(c1.keyframes.size() == 3, "features.mc3.xml: ch[1] has 3 keyframes");
                if (!c1.keyframes.empty()) {
                    CHECK(c1.keyframes[0].interpolation == Interpolation::CubicBezier,
                          "features.mc3.xml: ch[1].kf[0].interp==CubicBezier");
                    CHECKF(c1.keyframes[0].handleRight.dt, 0.5f,
                           "features.mc3.xml: ch[1].kf[0].handleRight.dt==0.5");
                }
            }
        }
    } catch (const std::exception& e) {
        fail(std::string("features.mc3.xml threw: ") + e.what());
    }
}

// ---------------------------------------------------------------------------
// Disk inner-radius roundtrip tests (Task 1 regression lock)
// ---------------------------------------------------------------------------

static void testDiskRingRoundtrip() {
    Mc3Document doc;
    auto obj = std::make_shared<Mc3Object>();
    obj->id   = "disk1";
    obj->name = "RingDisk";
    obj->type = ObjectType::Disk;
    Mc3Primitive p;
    p.primitiveType = PrimitiveType::Disk;
    p.radius        = 0.5f;
    p.minorRadius   = 0.25f;  // inner radius
    p.segments      = 16;
    obj->primitive  = p;
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "disk ring: object present");
    if (rt.objects.empty()) return;
    const auto& o = rt.objects[0];
    CHECK(o->primitive.has_value(), "disk ring: primitive present");
    if (!o->primitive) return;
    CHECK(o->primitive->primitiveType == PrimitiveType::Disk, "disk ring: type==Disk");
    CHECKF(o->primitive->radius,      0.5f,  "disk ring: radius==0.5");
    CHECKF(o->primitive->minorRadius, 0.25f, "disk ring: minorRadius (inner_radius)==0.25");
    CHECK(o->primitive->segments == 16,      "disk ring: segments==16");
}

static void testDiskSolidRoundtrip() {
    Mc3Document doc;
    auto obj = std::make_shared<Mc3Object>();
    obj->id   = "disk2";
    obj->name = "SolidDisk";
    obj->type = ObjectType::Disk;
    Mc3Primitive p;
    p.primitiveType = PrimitiveType::Disk;
    p.radius        = 0.4f;
    p.minorRadius   = 0.0f;  // solid — inner_radius must not be written
    p.segments      = 24;
    obj->primitive  = p;
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "disk solid: object present");
    if (rt.objects.empty()) return;
    const auto& o = rt.objects[0];
    CHECK(o->primitive.has_value(), "disk solid: primitive present");
    if (!o->primitive) return;
    CHECK(o->primitive->primitiveType == PrimitiveType::Disk, "disk solid: type==Disk");
    CHECKF(o->primitive->radius,      0.4f, "disk solid: radius==0.4");
    CHECKF(o->primitive->minorRadius, 0.0f, "disk solid: minorRadius==0.0 (solid)");
}

static void testDiskLegacyMinorRadius() {
    // Legacy XML uses minor_radius="..." for disk; parser must accept it and
    // treat it as inner_radius. On save, the canonical inner_radius attr must be used.
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="LegacyDisk">
  <objects>
    <disk name="LegacyRing" radius="0.5" minor_radius="0.2" segments="32"/>
  </objects>
</mc3>)";
    }
    try {
        auto doc = Mc3Document::loadFromFile(xmlPath);
        std::filesystem::remove(xmlPath);

        CHECK(!doc.objects.empty(),           "disk legacy: object loaded");
        if (doc.objects.empty()) return;
        const auto& o = doc.objects[0];
        CHECK(o->primitive.has_value(),       "disk legacy: primitive present");
        if (!o->primitive) return;
        CHECKF(o->primitive->minorRadius, 0.2f,
               "disk legacy: minor_radius='0.2' parsed as inner radius");

        // Roundtrip: saved XML must use inner_radius; loading it back must preserve value.
        auto rt = roundtrip(doc);
        CHECK(!rt.objects.empty(), "disk legacy rt: object present");
        if (rt.objects.empty()) return;
        const auto& ro = rt.objects[0];
        if (ro->primitive)
            CHECKF(ro->primitive->minorRadius, 0.2f,
                   "disk legacy rt: inner_radius preserved through save/load");
    } catch (const std::exception& e) {
        std::filesystem::remove(xmlPath);
        fail(std::string("disk legacy: exception: ") + e.what());
    }
}

// ---------------------------------------------------------------------------
// STAB-0429 — material with every field populated must survive roundtrip
// ---------------------------------------------------------------------------

static void testMaterialAllFieldsRoundtrip() {
    Mc3Document doc;

    Mc3Texture bcTex;  bcTex.name = "bc_tex";  bcTex.uri = "textures/bc.png";
    Mc3Texture nTex;   nTex.name = "n_tex";    nTex.uri = "textures/n.png";
    Mc3Texture mrTex;  mrTex.name = "mr_tex";  mrTex.uri = "textures/mr.png";
    Mc3Texture aoTex;  aoTex.name = "ao_tex";  aoTex.uri = "textures/ao.png";
    Mc3Texture emTex;  emTex.name = "em_tex";  emTex.uri = "textures/em.png";
    doc.textures["bc_tex"] = bcTex;
    doc.textures["n_tex"]  = nTex;
    doc.textures["mr_tex"] = mrTex;
    doc.textures["ao_tex"] = aoTex;
    doc.textures["em_tex"] = emTex;

    Mc3Material mat;
    mat.name                     = "kitchen_sink";
    mat.baseColor                = {0.25f, 0.5f, 0.75f, 0.6f};
    mat.baseColorTexture         = "bc_tex";
    mat.normalTexture            = "n_tex";
    mat.normalScale              = 1.5f;
    mat.metallicRoughnessTexture = "mr_tex";
    mat.occlusionTexture         = "ao_tex";
    mat.occlusionStrength        = 0.8f;
    mat.emissiveTexture          = "em_tex";
    mat.emissiveColor            = {0.9f, 0.4f, 0.1f};
    mat.roughness                = 0.35f;
    mat.metallic                 = 0.65f;
    mat.alphaMode                = "mask";
    mat.alphaCutoff              = 0.42f;
    mat.doubleSided              = true;
    doc.materials["kitchen_sink"] = mat;

    auto obj = std::make_shared<Mc3Object>();
    obj->id = "obj1"; obj->name = "AllFieldsBox"; obj->type = ObjectType::Box;
    obj->primitive = Mc3Primitive{};
    obj->material  = "kitchen_sink";
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(rt.materials.count("kitchen_sink") == 1, "material all-fields: material present after roundtrip");
    if (!rt.materials.count("kitchen_sink")) return;
    const auto& m = rt.materials.at("kitchen_sink");

    CHECKF(m.baseColor[0], 0.25f, "material all-fields: baseColor.r");
    CHECKF(m.baseColor[1], 0.5f,  "material all-fields: baseColor.g");
    CHECKF(m.baseColor[2], 0.75f, "material all-fields: baseColor.b");
    CHECKF(m.baseColor[3], 0.6f,  "material all-fields: baseColor.a");
    CHECK(m.baseColorTexture == "bc_tex",         "material all-fields: baseColorTexture");
    CHECK(m.normalTexture == "n_tex",             "material all-fields: normalTexture");
    CHECKF(m.normalScale, 1.5f,                   "material all-fields: normalScale");
    CHECK(m.metallicRoughnessTexture == "mr_tex", "material all-fields: metallicRoughnessTexture");
    CHECK(m.occlusionTexture == "ao_tex",         "material all-fields: occlusionTexture");
    CHECKF(m.occlusionStrength, 0.8f,             "material all-fields: occlusionStrength");
    CHECK(m.emissiveTexture == "em_tex",          "material all-fields: emissiveTexture");
    CHECKF(m.emissiveColor[0], 0.9f, "material all-fields: emissiveColor.r");
    CHECKF(m.emissiveColor[1], 0.4f, "material all-fields: emissiveColor.g");
    CHECKF(m.emissiveColor[2], 0.1f, "material all-fields: emissiveColor.b");
    CHECKF(m.roughness, 0.35f, "material all-fields: roughness");
    CHECKF(m.metallic,  0.65f, "material all-fields: metallic");
    CHECK(m.alphaMode == "mask", "material all-fields: alphaMode");
    CHECKF(m.alphaCutoff, 0.42f, "material all-fields: alphaCutoff");
    CHECK(m.doubleSided == true, "material all-fields: doubleSided");

    CHECK(!rt.objects.empty() && rt.objects[0]->material == "kitchen_sink",
          "material all-fields: object's material reference preserved");

    for (const char* texId : {"bc_tex", "n_tex", "mr_tex", "ao_tex", "em_tex"})
        CHECK(rt.textures.count(texId) == 1,
              std::string("material all-fields: texture '") + texId + "' present after roundtrip");
}

// ---------------------------------------------------------------------------
// SVG texture roundtrip
// ---------------------------------------------------------------------------

static void testEmbedGltf() {
    // External GLB
    {
        Mc3Document doc;
        Mc3EmbedGltf em;
        em.id  = "car";
        em.src = "models/car.glb";
        doc.addEmbed(em);

        auto rt = roundtrip(doc);
        CHECK(rt.embeds.count("car") == 1,       "embed external: present after roundtrip");
        if (rt.embeds.count("car")) {
            CHECK(rt.embeds["car"].src == "models/car.glb", "embed external: src preserved");
            CHECK(rt.embeds["car"].base64Content.empty(),    "embed external: no base64");
            CHECK(rt.embeds["car"].isExternal(),             "embed external: isExternal()");
        }
    }
    // Inline base64
    {
        Mc3Document doc;
        Mc3EmbedGltf em;
        em.id            = "icon_mesh";
        em.base64Content = "Z2xURgIAAAA="; // fake base64 GLB header
        doc.addEmbed(em);

        auto rt = roundtrip(doc);
        CHECK(rt.embeds.count("icon_mesh") == 1,     "embed inline: present after roundtrip");
        if (rt.embeds.count("icon_mesh")) {
            CHECK(rt.embeds["icon_mesh"].src.empty(),        "embed inline: src empty");
            CHECK(!rt.embeds["icon_mesh"].base64Content.empty(), "embed inline: base64 preserved");
            CHECK(rt.embeds["icon_mesh"].isInline(),         "embed inline: isInline()");
        }
    }
    // Mesh object references embed via "embed:<id>"
    {
        Mc3Document doc;
        Mc3EmbedGltf em;
        em.id  = "tree";
        em.src = "tree.glb";
        doc.addEmbed(em);
        auto obj = std::make_shared<Mc3Object>();
        obj->id         = "tree_obj";
        obj->type       = ObjectType::Mesh;
        obj->meshSource = "embed:tree";
        doc.objects.push_back(obj);

        auto rt = roundtrip(doc);
        CHECK(rt.embeds.count("tree") == 1,               "embed+mesh: embed present");
        CHECK(!rt.objects.empty(),                         "embed+mesh: object present");
        if (!rt.objects.empty())
            CHECK(rt.objects[0]->meshSource == "embed:tree", "embed+mesh: meshSource roundtrips");
    }
    // Mesh object references a plain imported OBJ file path (STAB-0533) —
    // distinct from the "embed:<id>" special case above; both share the
    // same <mesh src="..."/> attribute (Mc3XmlWriter.cpp:269-270).
    {
        Mc3Document doc;
        auto obj = std::make_shared<Mc3Object>();
        obj->id         = "chair_obj";
        obj->type       = ObjectType::Mesh;
        obj->meshSource = "models/chair.obj";
        doc.objects.push_back(obj);

        auto rt = roundtrip(doc);
        CHECK(!rt.objects.empty(), "obj import: object present");
        if (!rt.objects.empty())
            CHECK(rt.objects[0]->meshSource == "models/chair.obj", "obj import: meshSource path roundtrips");
    }
    // Coexistence with textures
    {
        Mc3Document doc;
        doc.addTexture(Mc3Texture{"diffuse", "tex/d.png"});
        doc.addEmbed(Mc3EmbedGltf{"prop", "prop.glb", {}});

        auto rt = roundtrip(doc);
        CHECK(rt.textures.count("diffuse") == 1, "embed+tex: texture preserved");
        CHECK(rt.embeds.count("prop")      == 1, "embed+tex: embed preserved");
    }
}

static void testMeta() {
    // Single entry
    {
        Mc3Document doc;
        doc.withMeta("author", "Robert");

        auto rt = roundtrip(doc);
        CHECK(rt.meta.count("author") == 1,        "meta single: present");
        if (rt.meta.count("author"))
            CHECK(rt.meta["author"] == "Robert",   "meta single: value");
    }
    // Multiple entries
    {
        Mc3Document doc;
        doc.withMeta("author",  "Robert")
           .withMeta("version", "1.0")
           .withMeta("license", "MIT");

        auto rt = roundtrip(doc);
        CHECK(rt.meta.size() == 3,                 "meta multi: count");
        CHECK(rt.meta["author"]  == "Robert",      "meta multi: author");
        CHECK(rt.meta["version"] == "1.0",         "meta multi: version");
        CHECK(rt.meta["license"] == "MIT",         "meta multi: license");
    }
    // Distinct from legacy metadata
    {
        Mc3Document doc;
        doc.withMetadata("old_key", "old_value");
        doc.withMeta("new_key", "new_value");

        auto rt = roundtrip(doc);
        CHECK(rt.metadata.count("old_key") == 1,   "meta vs legacy: metadata preserved");
        CHECK(rt.meta.count("new_key")     == 1,   "meta vs legacy: meta preserved");
        CHECK(rt.meta.count("old_key")     == 0,   "meta vs legacy: no cross-contamination");
    }
    // Coexistence with scene states and sounds
    {
        Mc3Document doc;
        doc.withMeta("scene", "dungeon");
        doc.addSound(Mc3Sound{"click", "click.wav", false});
        Mc3SceneState st; st.name = "dark";
        Mc3ObjectOverride ovr; ovr.id = "torch"; ovr.visible = false; st.overrides.push_back(ovr);
        doc.addSceneState(st);

        auto rt = roundtrip(doc);
        CHECK(rt.meta.count("scene")        == 1, "meta+state+snd: meta");
        CHECK(rt.sounds.count("click")       == 1, "meta+state+snd: sound");
        CHECK(rt.sceneStates.count("dark")   == 1, "meta+state+snd: state");
    }
}

static void testSceneState() {
    // Single override — visible
    {
        Mc3Document doc;
        Mc3SceneState st;
        st.name = "night";
        Mc3ObjectOverride ovr;
        ovr.id      = "lamp";
        ovr.visible = false;
        st.overrides.push_back(ovr);
        doc.addSceneState(st);

        auto rt = roundtrip(doc);
        CHECK(rt.sceneStates.count("night") == 1,      "state visible: present");
        if (rt.sceneStates.count("night")) {
            const auto& s = rt.sceneStates["night"];
            CHECK(s.overrides.size() == 1,             "state visible: override count");
            if (!s.overrides.empty()) {
                CHECK(s.overrides[0].id == "lamp",     "state visible: id");
                CHECK(s.overrides[0].visible.has_value(), "state visible: has_value");
                CHECK(s.overrides[0].visible == false, "state visible: value false");
            }
        }
    }
    // Position + rotation override
    {
        Mc3Document doc;
        Mc3SceneState st;
        st.name = "displaced";
        Mc3ObjectOverride ovr;
        ovr.id       = "chair";
        ovr.position = std::array<float,3>{1.0f, 2.0f, 3.0f};
        ovr.rotation = std::array<float,3>{0.0f, 45.0f, 0.0f};
        st.overrides.push_back(ovr);
        doc.addSceneState(st);

        auto rt = roundtrip(doc);
        CHECK(rt.sceneStates.count("displaced") == 1, "state pos/rot: present");
        if (rt.sceneStates.count("displaced")) {
            const auto& s = rt.sceneStates["displaced"];
            if (!s.overrides.empty()) {
                CHECK(s.overrides[0].position.has_value(), "state pos: has_value");
                CHECK(s.overrides[0].rotation.has_value(), "state rot: has_value");
                if (s.overrides[0].position)
                    CHECKF((*s.overrides[0].position)[1], 2.0f, "state pos: y=2");
                if (s.overrides[0].rotation)
                    CHECKF((*s.overrides[0].rotation)[1], 45.0f, "state rot: y=45");
            }
        }
    }
    // Material override
    {
        Mc3Document doc;
        Mc3SceneState st;
        st.name = "dark";
        Mc3ObjectOverride ovr;
        ovr.id       = "wall";
        ovr.material = "dark_stone";
        st.overrides.push_back(ovr);
        doc.addSceneState(st);

        auto rt = roundtrip(doc);
        CHECK(rt.sceneStates.count("dark") == 1,       "state material: present");
        if (rt.sceneStates.count("dark")) {
            const auto& s = rt.sceneStates["dark"];
            if (!s.overrides.empty()) {
                CHECK(s.overrides[0].material.has_value(),        "state material: has_value");
                CHECK(s.overrides[0].material == "dark_stone",    "state material: value");
                CHECK(!s.overrides[0].visible.has_value(),        "state material: visible unset");
            }
        }
    }
    // Multiple states
    {
        Mc3Document doc;
        Mc3SceneState day; day.name = "day";
        Mc3ObjectOverride o1; o1.id = "lamp"; o1.visible = false; day.overrides.push_back(o1);
        Mc3SceneState night; night.name = "night";
        Mc3ObjectOverride o2; o2.id = "lamp"; o2.visible = true; night.overrides.push_back(o2);
        doc.addSceneState(day);
        doc.addSceneState(night);

        auto rt = roundtrip(doc);
        CHECK(rt.sceneStates.count("day")   == 1,      "states multi: day present");
        CHECK(rt.sceneStates.count("night") == 1,      "states multi: night present");
    }
    // Coexistence with triggers and sounds
    {
        Mc3Document doc;
        doc.addSound(Mc3Sound{"click", "click.wav", false});
        Mc3Trigger trig; trig.id = "toggle";
        trig.steps.push_back({TriggerStepType::PlaySound, "click"});
        doc.addTrigger(trig);
        Mc3SceneState st; st.name = "active";
        Mc3ObjectOverride ovr; ovr.id = "btn"; ovr.visible = true; st.overrides.push_back(ovr);
        doc.addSceneState(st);

        auto rt = roundtrip(doc);
        CHECK(rt.sounds.count("click")       == 1, "state+trig+snd: sound");
        CHECK(rt.triggers.count("toggle")    == 1, "state+trig+snd: trigger");
        CHECK(rt.sceneStates.count("active") == 1, "state+trig+snd: state");
    }
    // STAB-0105: all four override fields set together on one override —
    // confirms setting one field doesn't clobber the others in the same
    // <object-override> element.
    {
        Mc3Document doc;
        Mc3SceneState st; st.name = "everything";
        Mc3ObjectOverride ovr;
        ovr.id       = "prop";
        ovr.visible  = false;
        ovr.position = std::array<float,3>{1.0f, 2.0f, 3.0f};
        ovr.rotation = std::array<float,3>{0.0f, 90.0f, 0.0f};
        ovr.material = "rusty";
        st.overrides.push_back(ovr);
        doc.addSceneState(st);

        auto rt = roundtrip(doc);
        CHECK(rt.sceneStates.count("everything") == 1, "state all-fields: present");
        if (rt.sceneStates.count("everything") && !rt.sceneStates["everything"].overrides.empty()) {
            const auto& o = rt.sceneStates["everything"].overrides[0];
            CHECK(o.visible.has_value() && *o.visible == false,        "state all-fields: visible survives");
            CHECK(o.position.has_value(),                              "state all-fields: position survives");
            CHECK(o.rotation.has_value(),                              "state all-fields: rotation survives");
            CHECK(o.material.has_value() && *o.material == "rusty",    "state all-fields: material survives");
            if (o.position) CHECKF((*o.position)[0], 1.0f, "state all-fields: position.x");
            if (o.rotation) CHECKF((*o.rotation)[1], 90.0f, "state all-fields: rotation.y");
        }
    }
    // STAB-0106: an override with only `visible` set must not write
    // position/rotation/material attributes at all (genuinely absent from
    // the XML text, not just unset after reload).
    {
        Mc3Document doc;
        Mc3SceneState st; st.name = "onlyvisible";
        Mc3ObjectOverride ovr; ovr.id = "prop2"; ovr.visible = true;
        st.overrides.push_back(ovr);
        doc.addSceneState(st);

        auto p = tmpPath();
        doc.saveToFile(p);
        std::ifstream f(p);
        std::string text((std::istreambuf_iterator<char>(f)), {});
        std::filesystem::remove(p);

        CHECK(text.find("position") == std::string::npos, "state only-visible: no position attribute written");
        CHECK(text.find("rotation") == std::string::npos, "state only-visible: no rotation attribute written");
        CHECK(text.find("material") == std::string::npos, "state only-visible: no material attribute written");
        CHECK(text.find("visible") != std::string::npos,  "state only-visible: visible attribute IS written");
    }
}

static void testTrigger() {
    // Single step
    {
        Mc3Document doc;
        Mc3Trigger trig;
        trig.id = "on_click";
        trig.steps.push_back({TriggerStepType::PlaySound, "click"});
        doc.addTrigger(trig);

        auto rt = roundtrip(doc);
        CHECK(rt.triggers.count("on_click") == 1,      "trigger single: present");
        if (rt.triggers.count("on_click")) {
            const auto& t = rt.triggers["on_click"];
            CHECK(t.steps.size() == 1,                 "trigger single: step count");
            if (!t.steps.empty()) {
                CHECK(t.steps[0].type == TriggerStepType::PlaySound, "trigger single: type");
                CHECK(t.steps[0].ref  == "click",                    "trigger single: ref");
            }
        }
    }
    // Multi-step — order preserved
    {
        Mc3Document doc;
        Mc3Trigger trig;
        trig.id = "intro";
        trig.steps.push_back({TriggerStepType::PlayAction, "walk_anim"});
        trig.steps.push_back({TriggerStepType::PlaySound,  "click"});
        trig.steps.push_back({TriggerStepType::RunScript,  "on_start"});
        trig.steps.push_back({TriggerStepType::PlayMusic,  "ambient"});
        doc.addTrigger(trig);

        auto rt = roundtrip(doc);
        CHECK(rt.triggers.count("intro") == 1,         "trigger multi: present");
        if (rt.triggers.count("intro")) {
            const auto& t = rt.triggers["intro"];
            CHECK(t.steps.size() == 4,                 "trigger multi: step count");
            if (t.steps.size() == 4) {
                CHECK(t.steps[0].type == TriggerStepType::PlayAction, "trigger multi: step0 type");
                CHECK(t.steps[0].ref  == "walk_anim",                 "trigger multi: step0 ref");
                CHECK(t.steps[1].type == TriggerStepType::PlaySound,  "trigger multi: step1 type");
                CHECK(t.steps[2].type == TriggerStepType::RunScript,  "trigger multi: step2 type");
                CHECK(t.steps[3].type == TriggerStepType::PlayMusic,  "trigger multi: step3 type");
                CHECK(t.steps[3].ref  == "ambient",                   "trigger multi: step3 ref");
            }
        }
    }
    // Empty trigger (no steps)
    {
        Mc3Document doc;
        Mc3Trigger trig;
        trig.id = "empty";
        doc.addTrigger(trig);

        auto rt = roundtrip(doc);
        CHECK(rt.triggers.count("empty") == 1,         "trigger empty: present");
        if (rt.triggers.count("empty"))
            CHECK(rt.triggers["empty"].steps.empty(),  "trigger empty: no steps");
    }
    // Multiple triggers
    {
        Mc3Document doc;
        Mc3Trigger t1; t1.id = "t1"; t1.steps.push_back({TriggerStepType::PlaySound, "s1"});
        Mc3Trigger t2; t2.id = "t2"; t2.steps.push_back({TriggerStepType::PlayMusic, "m1"});
        doc.addTrigger(t1);
        doc.addTrigger(t2);

        auto rt = roundtrip(doc);
        CHECK(rt.triggers.count("t1") == 1,            "triggers multi: t1 present");
        CHECK(rt.triggers.count("t2") == 1,            "triggers multi: t2 present");
    }
    // Coexistence with sounds and music
    {
        Mc3Document doc;
        doc.addSound(Mc3Sound{"click", "click.wav", false});
        doc.addMusic(Mc3Music{"ambient", "ambient.ogg", true});
        Mc3Trigger trig;
        trig.id = "start";
        trig.steps.push_back({TriggerStepType::PlaySound, "click"});
        trig.steps.push_back({TriggerStepType::PlayMusic, "ambient"});
        doc.addTrigger(trig);

        auto rt = roundtrip(doc);
        CHECK(rt.sounds.count("click")    == 1, "trigger+sound+music: sound preserved");
        CHECK(rt.musicTracks.count("ambient") == 1, "trigger+sound+music: music preserved");
        CHECK(rt.triggers.count("start")  == 1, "trigger+sound+music: trigger preserved");
        if (rt.triggers.count("start"))
            CHECK(rt.triggers["start"].steps.size() == 2, "trigger+sound+music: step count");
    }
}

static void testSoundMusic() {
    // Sound with default loop (false)
    {
        Mc3Document doc;
        doc.addSound(Mc3Sound{"click", "sounds/click.wav", false});

        auto rt = roundtrip(doc);
        CHECK(rt.sounds.count("click") == 1,          "sound default: present");
        if (rt.sounds.count("click")) {
            CHECK(rt.sounds["click"].src  == "sounds/click.wav", "sound default: src preserved");
            CHECK(rt.sounds["click"].loop == false,               "sound default: loop=false");
        }
    }
    // Sound with loop=true
    {
        Mc3Document doc;
        doc.addSound(Mc3Sound{"wind", "sounds/wind.ogg", true});

        auto rt = roundtrip(doc);
        CHECK(rt.sounds.count("wind") == 1,           "sound loop: present");
        if (rt.sounds.count("wind"))
            CHECK(rt.sounds["wind"].loop == true,      "sound loop: loop=true preserved");
    }
    // Music track with default loop (true)
    {
        Mc3Document doc;
        doc.addMusic(Mc3Music{"ambient", "music/ambient.ogg", true});

        auto rt = roundtrip(doc);
        CHECK(rt.musicTracks.count("ambient") == 1,   "music default: present");
        if (rt.musicTracks.count("ambient")) {
            CHECK(rt.musicTracks["ambient"].src  == "music/ambient.ogg", "music default: src preserved");
            CHECK(rt.musicTracks["ambient"].loop == true,                 "music default: loop=true");
        }
    }
    // Music track with loop=false
    {
        Mc3Document doc;
        doc.addMusic(Mc3Music{"intro", "music/intro.ogg", false});

        auto rt = roundtrip(doc);
        CHECK(rt.musicTracks.count("intro") == 1,     "music no-loop: present");
        if (rt.musicTracks.count("intro"))
            CHECK(rt.musicTracks["intro"].loop == false, "music no-loop: loop=false preserved");
    }
    // Multiple sounds and tracks
    {
        Mc3Document doc;
        doc.addSound(Mc3Sound{"click",    "click.wav",    false});
        doc.addSound(Mc3Sound{"explosion","boom.wav",     false});
        doc.addMusic(Mc3Music{"ambient",  "ambient.ogg",  true});
        doc.addMusic(Mc3Music{"battle",   "battle.ogg",   true});

        auto rt = roundtrip(doc);
        CHECK(rt.sounds.count("click")     == 1, "multi: click present");
        CHECK(rt.sounds.count("explosion") == 1, "multi: explosion present");
        CHECK(rt.musicTracks.count("ambient") == 1, "multi: ambient present");
        CHECK(rt.musicTracks.count("battle")  == 1, "multi: battle present");
    }
    // Coexistence with scripts and objects
    {
        Mc3Document doc;
        Mc3Script sc; sc.id = "on_start"; sc.type = "lua"; sc.source = "play('click')";
        doc.addScript(sc);
        doc.addSound(Mc3Sound{"click", "click.wav", false});
        doc.addMusic(Mc3Music{"bg", "bg.ogg", true});

        auto rt = roundtrip(doc);
        CHECK(rt.scripts.count("on_start") == 1, "sound+music+script: script preserved");
        CHECK(rt.sounds.count("click")     == 1, "sound+music+script: sound preserved");
        CHECK(rt.musicTracks.count("bg")   == 1, "sound+music+script: music preserved");
    }
    // STAB-0099/0101: the *default* loop value for each type must be
    // genuinely omitted from the written XML text (not just round-trip to
    // the same value, which a test only checking the loaded value back
    // wouldn't distinguish from "always explicitly written").
    {
        Mc3Document doc;
        doc.addSound(Mc3Sound{"click", "click.wav", false});  // false is sound's default
        doc.addMusic(Mc3Music{"bg",    "bg.ogg",    true});   // true is music's default
        auto p = tmpPath();
        doc.saveToFile(p);
        std::ifstream f(p);
        std::string text((std::istreambuf_iterator<char>(f)), {});
        std::filesystem::remove(p);

        CHECK(text.find("loop") == std::string::npos,
              "sound/music defaults: loop attribute genuinely omitted from XML text, not just "
              "round-tripping to the same value");
    }
}

static void testScript() {
    // Inline Lua source
    {
        Mc3Document doc;
        Mc3Script sc;
        sc.id     = "on_start";
        sc.type   = "lua";
        sc.source = "print(\"hello world\")";
        doc.addScript(sc);

        auto rt = roundtrip(doc);
        CHECK(rt.scripts.count("on_start") == 1,      "script inline: present after roundtrip");
        if (rt.scripts.count("on_start")) {
            CHECK(rt.scripts["on_start"].type   == "lua",              "script inline: type preserved");
            CHECK(rt.scripts["on_start"].source == "print(\"hello world\")", "script inline: source preserved");
            CHECK(rt.scripts["on_start"].hasSource(),                  "script inline: hasSource()");
        }
    }
    // Multi-line source preserved
    {
        Mc3Document doc;
        Mc3Script sc;
        sc.id     = "tick";
        sc.type   = "lua";
        sc.source = "local t = 0\nfunction update(dt)\n  t = t + dt\nend";
        doc.addScript(sc);

        auto rt = roundtrip(doc);
        CHECK(rt.scripts.count("tick") == 1,           "script multiline: present");
        if (rt.scripts.count("tick"))
            CHECK(rt.scripts["tick"].source == sc.source, "script multiline: source preserved");
    }
    // Empty source
    {
        Mc3Document doc;
        Mc3Script sc;
        sc.id   = "empty";
        sc.type = "lua";
        doc.addScript(sc);

        auto rt = roundtrip(doc);
        CHECK(rt.scripts.count("empty") == 1,          "script empty: present");
        if (rt.scripts.count("empty"))
            CHECK(!rt.scripts["empty"].hasSource(),    "script empty: hasSource() false");
    }
    // Coexistence with objects and embeds
    {
        Mc3Document doc;
        doc.addEmbed(Mc3EmbedGltf{"prop", "prop.glb", {}});
        Mc3Script sc;
        sc.id     = "main";
        sc.type   = "lua";
        sc.source = "return 42";
        doc.addScript(sc);
        auto obj = std::make_shared<Mc3Object>();
        obj->id   = "cube1";
        obj->type = ObjectType::Cube;
        doc.objects.push_back(obj);

        auto rt = roundtrip(doc);
        CHECK(rt.embeds.count("prop")   == 1, "script+embed+obj: embed preserved");
        CHECK(rt.scripts.count("main")  == 1, "script+embed+obj: script preserved");
        CHECK(!rt.objects.empty(),             "script+embed+obj: object preserved");
    }
}

static void testSvgTexture() {
    // External SVG
    {
        Mc3Document doc;
        Mc3SvgTexture svg;
        svg.id  = "logo";
        svg.src = "images/logo.svg";
        doc.addSvgTexture(svg);

        auto rt = roundtrip(doc);
        CHECK(rt.svgTextures.count("logo") == 1,  "svg tex external: present after roundtrip");
        if (rt.svgTextures.count("logo")) {
            CHECK(rt.svgTextures["logo"].src == "images/logo.svg", "svg tex external: src preserved");
            CHECK(rt.svgTextures["logo"].inlineContent.empty(),    "svg tex external: no inline content");
            CHECK(rt.svgTextures["logo"].isExternal(),             "svg tex external: isExternal()");
        }
    }
    // Inline SVG
    {
        Mc3Document doc;
        Mc3SvgTexture svg;
        svg.id            = "icon";
        svg.inlineContent = "<svg xmlns=\"http://www.w3.org/2000/svg\"><circle r=\"10\"/></svg>";
        doc.addSvgTexture(svg);

        auto rt = roundtrip(doc);
        CHECK(rt.svgTextures.count("icon") == 1, "svg tex inline: present after roundtrip");
        if (rt.svgTextures.count("icon")) {
            CHECK(rt.svgTextures["icon"].src.empty(),       "svg tex inline: src empty");
            CHECK(!rt.svgTextures["icon"].inlineContent.empty(), "svg tex inline: content preserved");
            CHECK(rt.svgTextures["icon"].isInline(),        "svg tex inline: isInline()");
        }
    }
    // Coexistence with regular bitmap textures
    {
        Mc3Document doc;
        doc.addTexture(Mc3Texture{"diffuse", "tex/color.png"});
        Mc3SvgTexture svg;
        svg.id  = "badge";
        svg.src = "badge.svg";
        doc.addSvgTexture(svg);

        auto rt = roundtrip(doc);
        CHECK(rt.textures.count("diffuse") == 1,    "svg+bitmap: bitmap texture preserved");
        CHECK(rt.svgTextures.count("badge") == 1,   "svg+bitmap: svg texture preserved");
    }
}

// ---------------------------------------------------------------------------
// Texture display name (Mc3Texture::name) surviving a roundtrip when it
// differs from the map key / XML id — found while auditing mc3.xsd for
// writer/parser drift: the editor's "rename texture" UI
// (MeshCraftApplication_UiLeftPanel.cpp) sets tex.name independently of the
// map key, Mc3XmlWriter writes it as a `name` attribute when it differs
// from `id`, but Mc3XmlParser previously always reset tex.name = id on
// load, silently discarding the rename on every save/reload. Fixed to read
// the `name` attribute back (falling back to id when absent, same as
// before for files that never set it).
// ---------------------------------------------------------------------------

static void testTextureNameDiffersFromId() {
    Mc3Document doc;
    doc.addTexture(Mc3Texture{"tex1", "foo.png"});
    // Simulate the editor's rename-texture UI: map key ("tex1", becomes the
    // XML id) stays fixed, only the display name changes.
    doc.textures["tex1"].name = "Diffuse Texture";

    auto rt = roundtrip(doc);
    CHECK(rt.textures.count("tex1") == 1, "texture rename rt: entry present under original id");
    if (rt.textures.count("tex1")) {
        CHECK(rt.textures.at("tex1").name == "Diffuse Texture",
              "texture rename rt: display name survives (was silently reset to id before the fix)");
    }

    // Negative control: when name was never changed (matches id), no `name`
    // attribute is written at all, and the parser's fallback still yields name==id.
    Mc3Document doc2;
    doc2.addTexture(Mc3Texture{"tex2", "bar.png"});
    auto rt2 = roundtrip(doc2);
    CHECK(rt2.textures.count("tex2") == 1 && rt2.textures.at("tex2").name == "tex2",
          "texture rename rt: unrenamed texture still defaults name to id");
}

// ---------------------------------------------------------------------------
// STAB-0070 — Disk with inner_radius="0.3" loaded from XML
// ---------------------------------------------------------------------------

static void testDiskInnerRadius() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <disk name="Ring" radius="1.0" inner_radius="0.3" segments="32"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    auto doc = Mc3Document::loadFromFile(xmlPath);
    std::filesystem::remove(xmlPath);

    CHECK(!doc.objects.empty(), "disk inner_radius: object present");
    if (!doc.objects.empty() && doc.objects[0]->primitive) {
        const auto& pr = *doc.objects[0]->primitive;
        CHECK(pr.primitiveType == PrimitiveType::Disk, "disk inner_radius: type==Disk");
        CHECKF(pr.radius,      1.0f, "disk inner_radius: radius==1.0");
        CHECKF(pr.minorRadius, 0.3f, "disk inner_radius: inner_radius==0.3 survives");
        CHECK(pr.segments == 32,     "disk inner_radius: segments==32");
    }

    // Roundtrip through writer+parser must preserve inner_radius
    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "disk inner_radius rt: object present");
    if (!rt.objects.empty() && rt.objects[0]->primitive)
        CHECKF(rt.objects[0]->primitive->minorRadius, 0.3f,
               "disk inner_radius rt: inner_radius==0.3 after roundtrip");
}

// ---------------------------------------------------------------------------
// STAB-0030 — All primitive types survive roundtrip (primitiveType preserved)
// ---------------------------------------------------------------------------

static void testAllPrimitiveTypes() {
    // Each row: XML tag, expected PrimitiveType
    struct Case { const char* tag; PrimitiveType expected; };
    static const Case cases[] = {
        { "box",       PrimitiveType::Box       },
        { "sphere",    PrimitiveType::Sphere     },
        { "cylinder",  PrimitiveType::Cylinder   },
        { "cone",      PrimitiveType::Cone       },
        { "torus",     PrimitiveType::Torus      },
        { "capsule",   PrimitiveType::Capsule    },
        { "disk",      PrimitiveType::Disk       },
        { "grid",      PrimitiveType::Grid       },
        { "icosphere", PrimitiveType::IcoSphere  },
        { "plane",     PrimitiveType::Plane      },
    };

    for (const auto& c : cases) {
        Mc3Document doc;
        auto xmlPath = tmpPath();
        {
            std::ofstream f(xmlPath);
            f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
              << R"(<mc3 version="0.3">)" "\n"
              << "  <objects>\n"
              << "    <" << c.tag << " name=\"obj\"/>\n"
              << "  </objects>\n"
              << R"(</mc3>)" "\n";
        }
        try {
            doc = Mc3Document::loadFromFile(xmlPath);
        } catch (const std::exception& e) {
            std::filesystem::remove(xmlPath);
            fail(std::string("all_primitives: load threw for ") + c.tag + ": " + e.what());
            continue;
        }
        std::filesystem::remove(xmlPath);

        std::string label = std::string("all_primitives[") + c.tag + "]";
        CHECK(!doc.objects.empty(), label + ": object present");
        if (doc.objects.empty() || !doc.objects[0]->primitive) continue;
        CHECK(doc.objects[0]->primitive->primitiveType == c.expected,
              label + ": primitiveType correct after parse");

        auto rt = roundtrip(doc);
        CHECK(!rt.objects.empty(), label + " rt: object present");
        if (!rt.objects.empty() && rt.objects[0]->primitive)
            CHECK(rt.objects[0]->primitive->primitiveType == c.expected,
                  label + ": primitiveType correct after roundtrip");
    }
}

// ---------------------------------------------------------------------------
// STAB-0075 — Unknown top-level element is silently skipped (no throw)
// ---------------------------------------------------------------------------

static void testUnknownTopLevelElement() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <unknowntag foo="bar"/>)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <box name="Known" size="1 1 1"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    bool threw = false;
    Mc3Document doc;
    try {
        doc = Mc3Document::loadFromFile(xmlPath);
    } catch (...) {
        threw = true;
    }
    std::filesystem::remove(xmlPath);

    CHECK(!threw,                   "unknown tag: no exception thrown");
    CHECK(doc.objects.size() == 1,  "unknown tag: known objects still parsed");
}

// STAB-0076: an unknown *attribute* on a known element must be ignored, not
// throw or corrupt the known attributes on the same element.
static void testUnknownAttributeOnKnownElement() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <box name="Known" id="b1" unknownAttr="whatever" size="2 2 2"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    bool threw = false;
    Mc3Document doc;
    try {
        doc = Mc3Document::loadFromFile(xmlPath);
    } catch (...) {
        threw = true;
    }
    std::filesystem::remove(xmlPath);

    CHECK(!threw, "unknown attribute: no exception thrown");
    CHECK(doc.objects.size() == 1, "unknown attribute: object still parsed");
    if (!doc.objects.empty()) {
        CHECK(doc.objects[0]->name == "Known", "unknown attribute: known attribute (name) unaffected");
        if (doc.objects[0]->primitive)
            CHECKF(doc.objects[0]->primitive->size[0], 2.0f, "unknown attribute: known attribute (size) unaffected");
    }
}

// STAB-0079/0080: malformed numeric attributes must not crash or throw
// uncaught — parseVec3/parseVec4 are sscanf-based (already tolerant by
// construction); attrF()/attrI() and the two inline single-value size/scale
// parsers in Mc3XmlParser.cpp previously used raw std::stof/std::stoi with
// no try/catch, which threw std::invalid_argument straight out and failed
// the *entire* file load with an unhelpful "Failed to load file: stof"
// message (found and fixed this session).
static void testMalformedNumericAttributesDoNotCrash() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <sphere name="BadRadius" radius="abc"/>)" "\n"
          << R"(    <box name="BadScale" scale="notanumber"/>)" "\n"
          << R"(    <box name="BadSize" size="notanumber"/>)" "\n"
          << R"(    <box name="PartialVec3" position="1 2"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    bool threw = false;
    Mc3Document doc;
    try {
        doc = Mc3Document::loadFromFile(xmlPath);
    } catch (...) {
        threw = true;
    }
    std::filesystem::remove(xmlPath);

    CHECK(!threw, "malformed numeric attrs: no exception thrown, whole file still loads");
    CHECK(doc.objects.size() == 4, "malformed numeric attrs: all 4 objects still parsed");
    if (doc.objects.size() == 4) {
        CHECK(doc.objects[0]->primitive.has_value(), "malformed radius: primitive still present");
        if (doc.objects[0]->primitive)
            CHECKF(doc.objects[0]->primitive->radius, 0.5f, "malformed radius: falls back to default 0.5");

        CHECKF(doc.objects[1]->transform.scale[0], 1.0f, "malformed scale: falls back to default 1.0");

        if (doc.objects[2]->primitive)
            CHECKF(doc.objects[2]->primitive->size[0], 1.0f, "malformed size: falls back to default 1.0");

        // "1 2" (only 2 of 3 vec3 components) must not crash; third
        // component stays at its default (0) rather than reading garbage.
        CHECKF(doc.objects[3]->transform.position[0], 1.0f, "partial vec3: position.x parsed");
        CHECKF(doc.objects[3]->transform.position[1], 2.0f, "partial vec3: position.y parsed");
        CHECKF(doc.objects[3]->transform.position[2], 0.0f, "partial vec3: position.z defaults to 0");
    }
}

// STAB-0081: an object with no `id` attribute must not crash. The row's
// original acceptance criterion assumed it should be *skipped*, but
// mc3.xsd's own objectAttrs group declares `id` as plain `xs:string` with
// no `use="required"` (unlike e.g. instanceType's `definition`, which is
// `use="required"`) — id is genuinely optional by design for ordinary
// scene objects. The actual, schema-consistent policy confirmed here:
// the object is still parsed and added with an empty id, not dropped.
static void testMissingIdDoesNotCrash() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <box name="NoId"/>)" "\n"
          << R"(    <box name="HasId" id="b1"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    bool threw = false;
    Mc3Document doc;
    try {
        doc = Mc3Document::loadFromFile(xmlPath);
    } catch (...) {
        threw = true;
    }
    std::filesystem::remove(xmlPath);

    CHECK(!threw, "missing id: no exception thrown");
    CHECK(doc.objects.size() == 2, "missing id: both objects still parsed (id is genuinely optional per mc3.xsd)");
    if (doc.objects.size() == 2) {
        CHECK(doc.objects[0]->id.empty(), "missing id: NoId object kept with an empty id, not dropped");
        CHECK(doc.objects[1]->id == "b1", "missing id: sibling object's id unaffected");
    }
}

// STAB-0077: duplicate object `id` policy. doc.objects is a plain
// std::vector<shared_ptr<Mc3Object>> (not id-keyed), so parsing never
// deduplicates or overrides on `id` collision — both objects simply coexist
// as independent entries. Documenting and testing the real behavior, since
// "id" here is an optional identifier (STAB-0081), not a map key.
static void testDuplicateObjectIdBothCoexist() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <box name="First" id="dup"/>)" "\n"
          << R"(    <sphere name="Second" id="dup"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    Mc3Document doc = Mc3Document::loadFromFile(xmlPath);
    std::filesystem::remove(xmlPath);

    CHECK(doc.objects.size() == 2, "duplicate object id: both objects survive (no dedup/override)");
    if (doc.objects.size() == 2) {
        CHECK(doc.objects[0]->id == "dup" && doc.objects[1]->id == "dup",
              "duplicate object id: both entries keep the same id");
        CHECK(doc.objects[0]->name == "First" && doc.objects[1]->name == "Second",
              "duplicate object id: distinguishable by name; neither was overwritten");
    }
}

// STAB-0078: duplicate material `id` policy. doc.materials IS a
// std::map<std::string, Mc3Material> (id-keyed, `doc.materials[id] = mat`),
// so — unlike objects above — a duplicate material id genuinely overwrites:
// the later <material> element in document order wins.
static void testDuplicateMaterialIdLastWins() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <materials>)" "\n"
          << R"(    <material id="dup" roughness="0.2"><base_color>1 0 0 1</base_color></material>)" "\n"
          << R"(    <material id="dup" roughness="0.9"><base_color>0 1 0 1</base_color></material>)" "\n"
          << R"(  </materials>)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <box name="Box1" id="b1" material="dup"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    Mc3Document doc = Mc3Document::loadFromFile(xmlPath);
    std::filesystem::remove(xmlPath);

    CHECK(doc.materials.count("dup") == 1, "duplicate material id: exactly one entry (map semantics)");
    if (doc.materials.count("dup")) {
        const auto& m = doc.materials.at("dup");
        CHECKF(m.roughness, 0.9f, "duplicate material id: second <material> (roughness=0.9) wins");
        CHECKF(m.baseColor[1], 1.0f, "duplicate material id: second <material>'s base_color (green) wins");
    }
}

// ---------------------------------------------------------------------------
// STAB-0074 — Grid with explicit subdivisions_x and subdivisions_z
// ---------------------------------------------------------------------------

static void testGridSubdivisions() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <grid name="Floor" size="10 10" subdivisions_x="5" subdivisions_z="8"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    auto doc = Mc3Document::loadFromFile(xmlPath);
    std::filesystem::remove(xmlPath);

    CHECK(!doc.objects.empty(), "grid: object present");
    if (!doc.objects.empty() && doc.objects[0]->primitive) {
        const auto& pr = *doc.objects[0]->primitive;
        CHECK(pr.primitiveType == PrimitiveType::Grid, "grid: type==Grid");
        CHECK(pr.subdivisionsX == 5, "grid: subdivisions_x==5 survives parse");
        CHECK(pr.subdivisionsZ == 8, "grid: subdivisions_z==8 survives parse");
    }

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "grid rt: object present");
    if (!rt.objects.empty() && rt.objects[0]->primitive) {
        CHECK(rt.objects[0]->primitive->subdivisionsX == 5, "grid rt: subdivisions_x survives roundtrip");
        CHECK(rt.objects[0]->primitive->subdivisionsZ == 8, "grid rt: subdivisions_z survives roundtrip");
    }
}

// ---------------------------------------------------------------------------
// STAB-0073 — IcoSphere with explicit segments (default is 2, not 32)
// ---------------------------------------------------------------------------

static void testIcoSphereSegments() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <icosphere name="HiRes" radius="0.6" segments="3"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    auto doc = Mc3Document::loadFromFile(xmlPath);
    std::filesystem::remove(xmlPath);

    CHECK(!doc.objects.empty(), "icosphere: object present");
    if (!doc.objects.empty() && doc.objects[0]->primitive) {
        const auto& pr = *doc.objects[0]->primitive;
        CHECK(pr.primitiveType == PrimitiveType::IcoSphere, "icosphere: type==IcoSphere");
        CHECKF(pr.radius, 0.6f,  "icosphere: radius==0.6 survives parse");
        CHECK(pr.segments == 3,  "icosphere: segments==3 survives parse");
    }

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "icosphere rt: object present");
    if (!rt.objects.empty() && rt.objects[0]->primitive) {
        CHECKF(rt.objects[0]->primitive->radius,  0.6f, "icosphere rt: radius survives roundtrip");
        CHECK(rt.objects[0]->primitive->segments == 3,  "icosphere rt: segments==3 survives roundtrip");
    }
}

// ---------------------------------------------------------------------------
// STAB-0072 — Capsule with explicit radius and height
// ---------------------------------------------------------------------------

static void testCapsuleRadiusHeight() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <capsule name="Pill" radius="0.4" height="1.5" segments="24"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    auto doc = Mc3Document::loadFromFile(xmlPath);
    std::filesystem::remove(xmlPath);

    CHECK(!doc.objects.empty(), "capsule: object present");
    if (!doc.objects.empty() && doc.objects[0]->primitive) {
        const auto& pr = *doc.objects[0]->primitive;
        CHECK(pr.primitiveType == PrimitiveType::Capsule, "capsule: type==Capsule");
        CHECKF(pr.radius,  0.4f, "capsule: radius==0.4 survives parse");
        CHECKF(pr.height,  1.5f, "capsule: height==1.5 survives parse");
        CHECK(pr.segments == 24, "capsule: segments==24");
    }

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "capsule rt: object present");
    if (!rt.objects.empty() && rt.objects[0]->primitive) {
        CHECKF(rt.objects[0]->primitive->radius, 0.4f, "capsule rt: radius survives roundtrip");
        CHECKF(rt.objects[0]->primitive->height, 1.5f, "capsule rt: height survives roundtrip");
    }
}

// ---------------------------------------------------------------------------
// STAB-0071 — Torus with non-default minor_radius
// ---------------------------------------------------------------------------

static void testTorusMinorRadius() {
    auto xmlPath = tmpPath();
    {
        std::ofstream f(xmlPath);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"
          << R"(<mc3 version="0.3">)" "\n"
          << R"(  <objects>)" "\n"
          << R"(    <torus name="Ring" major_radius="0.5" minor_radius="0.2" segments="24"/>)" "\n"
          << R"(  </objects>)" "\n"
          << R"(</mc3>)" "\n";
    }
    auto doc = Mc3Document::loadFromFile(xmlPath);
    std::filesystem::remove(xmlPath);

    CHECK(!doc.objects.empty(), "torus: object present");
    if (!doc.objects.empty() && doc.objects[0]->primitive) {
        const auto& pr = *doc.objects[0]->primitive;
        CHECK(pr.primitiveType == PrimitiveType::Torus, "torus: type==Torus");
        CHECKF(pr.majorRadius, 0.5f, "torus: major_radius==0.5");
        CHECKF(pr.minorRadius, 0.2f, "torus: minor_radius==0.2 survives parse");
        CHECK(pr.segments == 24,     "torus: segments==24");
    }

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "torus rt: object present");
    if (!rt.objects.empty() && rt.objects[0]->primitive) {
        CHECKF(rt.objects[0]->primitive->majorRadius, 0.5f, "torus rt: major_radius survives roundtrip");
        CHECKF(rt.objects[0]->primitive->minorRadius, 0.2f, "torus rt: minor_radius survives roundtrip");
    }
}

// ---------------------------------------------------------------------------
// STAB-0068 — plane size "8 8" (vec2) roundtrip
// ---------------------------------------------------------------------------

static void testPlaneSizeVec2() {
    Mc3Document doc;
    auto obj          = std::make_shared<Mc3Object>();
    obj->id           = "floor";
    obj->type         = ObjectType::Plane;
    obj->primitive    = Mc3Primitive{};
    obj->primitive->primitiveType = PrimitiveType::Plane;
    // vec2: width=8, depth=8 → stored as {8, 1, 8}
    obj->primitive->size = {8.0f, 1.0f, 8.0f};
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "plane vec2: object present");
    if (!rt.objects.empty() && rt.objects[0]->primitive) {
        const auto& sz = rt.objects[0]->primitive->size;
        CHECKF(sz[0], 8.0f, "plane vec2: size[0] == 8");
        CHECKF(sz[1], 1.0f, "plane vec2: size[1] == 1 (height layer)");
        CHECKF(sz[2], 8.0f, "plane vec2: size[2] == 8");
    }
}

// ---------------------------------------------------------------------------
// STAB-0069 — plane size "8 0 8" (legacy vec3) parsed and re-emitted as vec2
// ---------------------------------------------------------------------------

static void testPlaneSizeLegacyVec3() {
    // Simulate a legacy XML that uses "W 0 D" format.
    // After roundtrip the writer emits "W D" and the parser reads it as {W, 1, D}.
    auto p = tmpPath();
    {
        // Write the legacy format by hand — bypass writer
        std::ofstream f(p);
        f << R"(<?xml version="1.0" encoding="UTF-8"?>)"  "\n"
          << R"(<mc3 version="0.3">)"                    "\n"
          << R"(  <objects>)"                             "\n"
          << R"(    <plane name="OldFloor" size="8 0 8"/>)" "\n"
          << R"(  </objects>)"                            "\n"
          << R"(</mc3>)"                                  "\n";
    }
    auto doc  = Mc3Document::loadFromFile(p);
    std::filesystem::remove(p);

    CHECK(!doc.objects.empty(), "plane legacy: object present");
    if (!doc.objects.empty() && doc.objects[0]->primitive) {
        const auto& sz = doc.objects[0]->primitive->size;
        CHECKF(sz[0], 8.0f, "plane legacy: size[0] == 8");
        // Legacy "8 0 8" keeps size[1]=0 (no 1.0 injection for 3-value form)
        CHECKF(sz[2], 8.0f, "plane legacy: size[2] == 8");

        // After a second roundtrip through the writer the value normalises
        auto rt2 = roundtrip(doc);
        if (!rt2.objects.empty() && rt2.objects[0]->primitive) {
            const auto& sz2 = rt2.objects[0]->primitive->size;
            CHECKF(sz2[0], 8.0f, "plane legacy rt2: size[0] == 8");
            CHECKF(sz2[2], 8.0f, "plane legacy rt2: size[2] == 8");
        }
    }
}

// ---------------------------------------------------------------------------
// STAB-0555 — UTF-8 filename: the scene file's own path (not just its
// contents) contains non-ASCII characters.
// ---------------------------------------------------------------------------

static void testUtf8FilenameRoundtrip() {
    auto p = std::filesystem::temp_directory_path() /
             std::filesystem::path(u8"mc3_rt_čeština_日本.mc3.xml");

    Mc3Document doc;
    auto obj       = std::make_shared<Mc3Object>();
    obj->id        = "box1";
    obj->name      = "Box";
    obj->type      = ObjectType::Box;
    obj->primitive = Mc3Primitive{};
    doc.objects.push_back(obj);

    auto rt = roundtripAt(doc, p);
    CHECK(!rt.objects.empty(), "utf8 filename: object present after round-trip");
    if (!rt.objects.empty())
        CHECK(rt.objects[0]->id == "box1", "utf8 filename: object id preserved");
}

// ---------------------------------------------------------------------------
// STAB-0556 — path with spaces: both the parent directory and filename
// contain spaces.
// ---------------------------------------------------------------------------

static void testPathWithSpacesRoundtrip() {
    auto dir = std::filesystem::temp_directory_path() / "my scene files";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto p = dir / "house with spaces.mc3.xml";

    Mc3Document doc;
    auto obj       = std::make_shared<Mc3Object>();
    obj->id        = "box2";
    obj->name      = "Box";
    obj->type      = ObjectType::Box;
    obj->primitive = Mc3Primitive{};
    doc.objects.push_back(obj);

    auto rt = roundtripAt(doc, p);
    std::filesystem::remove_all(dir, ec);
    CHECK(!rt.objects.empty(), "path with spaces: object present after round-trip");
    if (!rt.objects.empty())
        CHECK(rt.objects[0]->id == "box2", "path with spaces: object id preserved");
}

// ---------------------------------------------------------------------------
// STAB-0049 — golden-file test: Mc3XmlWriter's exact byte output for a fixed
// document is compared against a committed reference file. Mc3XmlWriter has
// no time/random/machine-dependent state (verified: no time()/rand()/chrono
// calls anywhere in Mc3XmlWriter.cpp), so this output is fully deterministic
// — an unintentional formatting change (attribute order, float precision,
// whitespace) in the writer will show up here as a byte diff, even if every
// field-level roundtrip assertion elsewhere still passes.
// ---------------------------------------------------------------------------

static void testGoldenFileBasicScene() {
    Mc3Document doc;
    doc.materials["Wood"] = Mc3Material("Wood", {0.6f, 0.4f, 0.2f, 1.0f}, 0.7f, 0.0f);

    auto box = std::make_shared<Mc3Object>();
    box->id = "box1"; box->name = "Crate"; box->type = ObjectType::Box;
    box->primitive = Mc3Primitive{.primitiveType = PrimitiveType::Box, .size = {1.0f, 1.0f, 1.0f}};
    box->material = "Wood";
    box->transform.position = {2.0f, 0.5f, -1.0f};
    doc.objects.push_back(box);

    auto sphere = std::make_shared<Mc3Object>();
    sphere->id = "sphere1"; sphere->name = "Ball"; sphere->type = ObjectType::Sphere;
    sphere->primitive = Mc3Primitive{.primitiveType = PrimitiveType::Sphere, .radius = 0.75f};
    doc.objects.push_back(sphere);

    doc.lights.push_back(Mc3Light::directional("Sun", {0.3f, -1.0f, -0.2f}, {1.0f, 0.95f, 0.85f}, 1.2f));

    auto p = tmpPath();
    doc.saveToFile(p);
    std::ifstream produced(p);
    std::string producedContent((std::istreambuf_iterator<char>(produced)),
                                 std::istreambuf_iterator<char>());
    std::filesystem::remove(p);

    auto goldenPath = std::filesystem::path(__FILE__).parent_path() / "golden" / "basic_scene.mc3.xml";
    std::ifstream golden(goldenPath);
    CHECK(golden.good(), "golden file: basic_scene.mc3.xml exists and opens");
    if (!golden.good()) return;
    std::string goldenContent((std::istreambuf_iterator<char>(golden)),
                               std::istreambuf_iterator<char>());

    CHECK(producedContent == goldenContent,
          "golden file: Mc3XmlWriter output byte-for-byte matches committed golden/basic_scene.mc3.xml");
    if (producedContent != goldenContent) {
        std::cerr << "--- produced ---\n" << producedContent
                  << "--- golden ---\n" << goldenContent << "\n";
    }
}

// ---------------------------------------------------------------------------
// STAB-0557 — non-ASCII object name: name/id containing non-ASCII characters
// survives an ordinary (ASCII-path) round-trip unchanged.
// ---------------------------------------------------------------------------

static void testNonAsciiObjectNameRoundtrip() {
    Mc3Document doc;
    auto obj       = std::make_shared<Mc3Object>();
    const std::string nonAsciiName = "Slon_člověk";
    obj->id        = "obj1";
    obj->name      = nonAsciiName;
    obj->type      = ObjectType::Box;
    obj->primitive = Mc3Primitive{};
    doc.objects.push_back(obj);

    auto rt = roundtrip(doc);
    CHECK(!rt.objects.empty(), "non-ascii name: object present after round-trip");
    if (!rt.objects.empty())
        CHECK(rt.objects[0]->name == nonAsciiName,
              "non-ascii name: UTF-8 name preserved exactly");
}

// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    testVisible();
    testVisibleDefault();
    testDeform();
    testMaterialColorPrecisionRoundtrip();
    testUnnamedMaterialHandledGracefully();
    testDuplicateKeyframeTimePolicy();
    testUvMappingRoundtrip();
    testExtrudeArcCircle();
    testExtrudeHelixPolygon();
    testExtrudePolyline();
    testExtrudeBezier();
    testCsgDifferenceAndCutter();
    testGroupChildren();
    testUnionAndIntersectionRoundtrip();
    testInstanceRoundtrip();
    testAreaRoundtrip();
    testDiskRingRoundtrip();
    testDiskSolidRoundtrip();
    testDiskLegacyMinorRadius();
    testMaterialAllFieldsRoundtrip();
    testAnimationLinear();
    testAnimationCubicBezier();
    testAnimationStep();
    testAnimationMultiAction();
    testAnimationScaleRoundtrip();
    testAnimationDeformRoundtrip();
    testAnimationEvaluate();
    testMaterialColorAnimationRoundtrip();
    testEmbedGltf();
    testSvgTexture();
    testTextureNameDiffersFromId();
    testScript();
    testSoundMusic();
    testTrigger();
    testSceneState();
    testMeta();
    testAllPrimitiveTypes();
    testUnknownTopLevelElement();
    testUnknownAttributeOnKnownElement();
    testMalformedNumericAttributesDoNotCrash();
    testMissingIdDoesNotCrash();
    testDuplicateObjectIdBothCoexist();
    testDuplicateMaterialIdLastWins();
    testGridSubdivisions();
    testIcoSphereSegments();
    testCapsuleRadiusHeight();
    testDiskInnerRadius();
    testTorusMinorRadius();
    testPlaneSizeVec2();
    testPlaneSizeLegacyVec3();
    testIncludeAcrossDirectoriesRebasesRelativePaths();
    testIncludedSvgTextureNotDuplicatedOnSave();
    testIncludePathWithSpaces();
    testIncludeNonexistentFileClearError();
    testMalformedXmlParseErrorIsClean();
    testWriterEmitsVersionCorrectly();
    testWriterPreservesObjectInsertionOrder();
    testUtf8FilenameRoundtrip();
    testPathWithSpacesRoundtrip();
    testNonAsciiObjectNameRoundtrip();
    testGoldenFileBasicScene();

    if (argc >= 2) {
        testFeaturesXmlLoads(argv[1]);
        testInclude(argv[1]);
        testIncludeOverride(argv[1]);
        testIncludeNested(argv[1]);
        testIncludeCycle(argv[1]);
    }

    std::cout << "\n" << (failures == 0 ? "All tests passed." : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures > 0 ? 1 : 0;
}
