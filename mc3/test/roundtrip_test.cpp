#include <MeshCraft/Mc3/Mc3Animation.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
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

int main(int argc, char* argv[]) {
    testVisible();
    testVisibleDefault();
    testDeform();
    testExtrudeArcCircle();
    testExtrudeHelixPolygon();
    testExtrudePolyline();
    testExtrudeBezier();
    testCsgDifferenceAndCutter();
    testGroupChildren();
    testAnimationLinear();
    testAnimationCubicBezier();
    testAnimationStep();
    testAnimationMultiAction();
    testAnimationEvaluate();

    if (argc >= 2)
        testFeaturesXmlLoads(argv[1]);

    std::cout << "\n" << (failures == 0 ? "All tests passed." : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures > 0 ? 1 : 0;
}
