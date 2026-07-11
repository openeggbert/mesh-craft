// SYS-W1-02: documented per-field numeric-range test for the MC3 XML reader.
//
// finite_input_test.cpp already proves NaN/Inf are sanitized away
// (finiteOr); this file proves a DIFFERENT class of bug: a value that is
// perfectly finite but still outside its field's documented valid domain
// (e.g. fov="600", roughness="-3", radius="-5") is clamped to something sane
// -- not silently accepted as garbage that breaks downstream projection
// math, PBR shading, or geometry generation. See MC3_FORMAT.md for the
// human-readable range documentation and Mc3XmlParser.cpp (search
// "SYS-W1-02") for the enforcement + named constants.
//
// Grown incrementally, one domain (camera/material/geometry/environment/
// animation/transform) per commit -- see this session's plan.md entry for
// SYS-W1-02 for exactly which domains/fields are covered.

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

static Mc3Document load(const char* xml) {
    auto path = std::filesystem::temp_directory_path() / "mc3_numeric_range_test.mc3.xml";
    { std::ofstream f(path); f << xml; }
    Mc3Document doc = Mc3Document::loadFromFile(path);
    std::filesystem::remove(path);
    return doc;
}

// ---------------------------------------------------------------------------
// Camera: fov in (0, 180) degrees, near > 0, far > near, aspect > 0.
// ---------------------------------------------------------------------------
static void testCameraRanges() {
    Mc3Document doc = load(
        "<mc3 version=\"0.3\" model=\"cam\">\n"
        "  <cameras default=\"bad\">\n"
        "    <camera name=\"bad\" fov=\"600\" near=\"-1\" far=\"0.5\" aspect=\"-2\"/>\n"
        "    <camera name=\"zero_fov\" fov=\"0\" near=\"0\" far=\"1\" aspect=\"0\"/>\n"
        "    <camera name=\"inverted\" fov=\"60\" near=\"10\" far=\"5\" aspect=\"1\"/>\n"
        "    <camera name=\"ok\" fov=\"75\" near=\"0.5\" far=\"200\" aspect=\"1.777\"/>\n"
        "  </cameras>\n"
        "</mc3>\n");

    check(doc.cameras.size() == 4, "4 cameras parsed");
    auto find = [&](const std::string& n) -> const Mc3Camera* {
        for (const auto& c : doc.cameras) if (c.name == n) return &c;
        return nullptr;
    };

    if (const Mc3Camera* bad = find("bad")) {
        check(bad->fov > 0.0f && bad->fov < 180.0f,
              "fov=600 clamped into (0, 180): got " + std::to_string(bad->fov));
        check(bad->nearPlane > 0.0f, "near=-1 clamped to > 0: got " + std::to_string(bad->nearPlane));
        check(bad->orthoAspect > 0.0f,
              "aspect=-2 clamped to > 0: got " + std::to_string(bad->orthoAspect));
    } else {
        check(false, "camera 'bad' found");
    }

    if (const Mc3Camera* zf = find("zero_fov")) {
        check(zf->fov > 0.0f && zf->fov < 180.0f, "fov=0 clamped away from the degenerate edge");
        check(zf->nearPlane > 0.0f, "near=0 clamped to > 0");
        check(zf->farPlane > zf->nearPlane, "far=1 still exceeds clamped near");
        check(zf->orthoAspect > 0.0f, "aspect=0 clamped to > 0");
    } else {
        check(false, "camera 'zero_fov' found");
    }

    if (const Mc3Camera* inv = find("inverted")) {
        // near=10 needs no floor-clamp on its own (it's already positive),
        // so this specifically exercises the cross-field far>near margin
        // clamp, independent of the near-floor clamp exercised above.
        check(inv->nearPlane == 10.0f, "near=10 (already valid) preserved unchanged");
        check(inv->farPlane > inv->nearPlane,
              "far=5 (<= near=10) clamped to exceed near: far=" + std::to_string(inv->farPlane));
    } else {
        check(false, "camera 'inverted' found");
    }

    if (const Mc3Camera* ok = find("ok")) {
        check(ok->fov == 75.0f, "legitimate fov=75 preserved unchanged");
        check(ok->nearPlane == 0.5f, "legitimate near=0.5 preserved unchanged");
        check(ok->farPlane == 200.0f, "legitimate far=200 preserved unchanged");
        check(ok->orthoAspect == 1.777f, "legitimate aspect=1.777 preserved unchanged");
    } else {
        check(false, "camera 'ok' found");
    }
}

// ---------------------------------------------------------------------------
// Material: roughness/metallic/occlusion_strength/alpha_cutoff and
// base_color's alpha (opacity) channel all in [0, 1] (glTF PBR convention).
// ---------------------------------------------------------------------------
static void testMaterialRanges() {
    Mc3Document doc = load(
        "<mc3 version=\"0.3\" model=\"mat\">\n"
        "  <materials>\n"
        "    <material id=\"bad\" roughness=\"-3\" metallic=\"5\"\n"
        "              occlusion_strength=\"2\" alpha_cutoff=\"-1\">\n"
        "      <base_color>0.5 0.5 0.5 2.0</base_color>\n"
        "    </material>\n"
        "    <material id=\"ok\" roughness=\"0.2\" metallic=\"0.8\"\n"
        "              occlusion_strength=\"0.6\" alpha_cutoff=\"0.5\">\n"
        "      <base_color>0.8 0.1 0.1 0.9</base_color>\n"
        "    </material>\n"
        "  </materials>\n"
        "</mc3>\n");

    check(doc.materials.size() == 2, "2 materials parsed");

    auto it = doc.materials.find("bad");
    check(it != doc.materials.end(), "material 'bad' found");
    if (it != doc.materials.end()) {
        const auto& m = it->second;
        check(m.roughness >= 0.0f && m.roughness <= 1.0f,
              "roughness=-3 clamped into [0,1]: got " + std::to_string(m.roughness));
        check(m.metallic >= 0.0f && m.metallic <= 1.0f,
              "metallic=5 clamped into [0,1]: got " + std::to_string(m.metallic));
        check(m.occlusionStrength >= 0.0f && m.occlusionStrength <= 1.0f,
              "occlusion_strength=2 clamped into [0,1]: got " + std::to_string(m.occlusionStrength));
        check(m.alphaCutoff >= 0.0f && m.alphaCutoff <= 1.0f,
              "alpha_cutoff=-1 clamped into [0,1]: got " + std::to_string(m.alphaCutoff));
        check(m.baseColor[3] >= 0.0f && m.baseColor[3] <= 1.0f,
              "base_color alpha=2.0 clamped into [0,1]: got " + std::to_string(m.baseColor[3]));
        check(m.baseColor[0] == 0.5f, "base_color RGB left untouched (not an opacity field)");
    }

    it = doc.materials.find("ok");
    check(it != doc.materials.end(), "material 'ok' found");
    if (it != doc.materials.end()) {
        const auto& m = it->second;
        check(m.roughness == 0.2f, "legitimate roughness=0.2 preserved unchanged");
        check(m.metallic == 0.8f, "legitimate metallic=0.8 preserved unchanged");
        check(m.occlusionStrength == 0.6f, "legitimate occlusion_strength=0.6 preserved unchanged");
        check(m.alphaCutoff == 0.5f, "legitimate alpha_cutoff=0.5 preserved unchanged");
        check(m.baseColor[3] == 0.9f, "legitimate base_color alpha=0.9 preserved unchanged");
    }
}

// ---------------------------------------------------------------------------
// Geometry: primitive radius/height/size/major_radius/minor_radius,
// cross-section width/height/radius/inner_radius, and extrude path
// length/radius/height must all reject negative values (clamped to 0).
// ---------------------------------------------------------------------------
static void testGeometryRanges() {
    Mc3Document doc = load(
        "<mc3 version=\"0.3\" model=\"geom\">\n"
        "  <objects>\n"
        "    <box name=\"bad_box\" size=\"-2 -3 4\"/>\n"
        "    <sphere name=\"bad_sphere\" radius=\"-5\"/>\n"
        "    <cylinder name=\"bad_cyl\" radius=\"-1\" height=\"-2\"/>\n"
        "    <torus name=\"bad_torus\" major_radius=\"-1\" minor_radius=\"-0.5\"/>\n"
        "    <disk name=\"bad_disk\" radius=\"2\" inner_radius=\"-3\"/>\n"
        "    <sphere name=\"ok_sphere\" radius=\"2.5\"/>\n"
        "    <extrude name=\"bad_extrude\">\n"
        "      <cross_section type=\"rect\" width=\"-1\" height=\"-2\" radius=\"-3\" inner_radius=\"-4\"/>\n"
        "      <path type=\"helix\" length=\"-1\" radius=\"-2\" height=\"-3\"/>\n"
        "    </extrude>\n"
        "  </objects>\n"
        "</mc3>\n");

    auto find = [&](const std::string& n) -> const Mc3Object* {
        for (const auto& o : doc.objects) if (o && o->name == n) return o.get();
        return nullptr;
    };

    if (const Mc3Object* o = find("bad_box"); o && o->primitive) {
        check(o->primitive->size[0] == 0.0f && o->primitive->size[1] == 0.0f,
              "box size=-2,-3,4: negative components clamped to 0");
        check(o->primitive->size[2] == 4.0f, "box size=-2,-3,4: positive component preserved");
    } else check(false, "bad_box parsed with a primitive");

    if (const Mc3Object* o = find("bad_sphere"); o && o->primitive) {
        check(o->primitive->radius == 0.0f, "sphere radius=-5 clamped to 0");
    } else check(false, "bad_sphere parsed with a primitive");

    if (const Mc3Object* o = find("bad_cyl"); o && o->primitive) {
        check(o->primitive->radius == 0.0f, "cylinder radius=-1 clamped to 0");
        check(o->primitive->height == 0.0f, "cylinder height=-2 clamped to 0");
    } else check(false, "bad_cyl parsed with a primitive");

    if (const Mc3Object* o = find("bad_torus"); o && o->primitive) {
        check(o->primitive->majorRadius == 0.0f, "torus major_radius=-1 clamped to 0");
        check(o->primitive->minorRadius == 0.0f, "torus minor_radius=-0.5 clamped to 0");
    } else check(false, "bad_torus parsed with a primitive");

    if (const Mc3Object* o = find("bad_disk"); o && o->primitive) {
        check(o->primitive->minorRadius == 0.0f,
              "disk inner_radius=-3 clamped/defaulted to 0 (not left negative)");
    } else check(false, "bad_disk parsed with a primitive");

    if (const Mc3Object* o = find("ok_sphere"); o && o->primitive) {
        check(o->primitive->radius == 2.5f, "legitimate sphere radius=2.5 preserved unchanged");
    } else check(false, "ok_sphere parsed with a primitive");

    if (const Mc3Object* o = find("bad_extrude"); o && o->extrude) {
        const auto& cs = o->extrude->crossSection;
        check(cs.width == 0.0f && cs.height == 0.0f && cs.radius == 0.0f && cs.innerRadius == 0.0f,
              "extrude cross_section: all negative width/height/radius/inner_radius clamped to 0");
        const auto& path = o->extrude->path;
        check(path.length == 0.0f, "extrude path length=-1 clamped to 0");
        check(path.helixHeight == 0.0f, "extrude helix path height=-3 clamped to 0");
        // helix reuses the shared "radius" attribute for helixRadius.
        check(path.helixRadius == 0.0f, "extrude helix path radius=-2 clamped to 0");
    } else check(false, "bad_extrude parsed with an extrude");
}

int main() {
    testCameraRanges();
    testMaterialRanges();
    testGeometryRanges();

    if (failures == 0) { std::cout << "All numeric-range tests passed.\n"; return 0; }
    std::cerr << failures << " numeric-range test(s) failed.\n";
    return 1;
}
