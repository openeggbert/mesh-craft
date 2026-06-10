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

    if (argc >= 2)
        testFeaturesXmlLoads(argv[1]);

    std::cout << "\n" << (failures == 0 ? "All tests passed." : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures > 0 ? 1 : 0;
}
