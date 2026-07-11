#include "Mc3XmlParser.hpp"
#include "MathUtils.hpp"

#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Camera.hpp"
#include "MeshCraft/Mc3/Mc3Environment.hpp"
#include "MeshCraft/Mc3/Mc3Extrude.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"

#include <tinyxml2.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace tinyxml2;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mc3::Internal;

// ---------------------------------------------------------------------------
// Tiny helpers
// ---------------------------------------------------------------------------

static const char* attr(const XMLElement* el, const char* name, const char* def = "") {
    const char* v = el->Attribute(name);
    return v ? v : def;
}

// STAB-0080: a malformed value (e.g. "abc") previously threw std::invalid_argument
// straight out of std::stof/std::stoi, which failed the *entire* file load with an
// unhelpful "Failed to load file: stof" message instead of gracefully defaulting
// just this one attribute — unlike parseVec3/parseVec4, which are sscanf-based and
// already tolerate malformed input safely.
static float attrF(const XMLElement* el, const char* name, float def = 0.0f) {
    const char* v = el->Attribute(name);
    if (!v) return def;
    // finiteOr rejects "nan"/"inf" (which std::stof accepts without throwing);
    // the catch handles non-numeric junk like "abc".
    try { return Internal::finiteOr(std::stof(v), def); } catch (...) { return def; }
}

static int attrI(const XMLElement* el, const char* name, int def = 0) {
    const char* v = el->Attribute(name);
    if (!v) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

// Tessellation counts (segments / sides / subdivisions) come from untrusted
// input and directly drive geometry allocation, so they are clamped to a sane
// range. The upper bound is far above any legitimate mesh but stops a hostile
// <sphere segments="100000000"/> (which would request ~5e15 vertices) or
// <grid subdivisions_x="1000000"/> from exhausting memory.
static constexpr int kMaxTessellation = 4096;

static int attrCount(const XMLElement* el, const char* name, int def,
                     int minv, int maxv = kMaxTessellation) {
    int v = attrI(el, name, def);
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

// AUD-059: kMaxTessellation bounds any SINGLE field, but a document with many
// objects each individually under that cap can still sum to an enormous
// aggregate allocation (e.g. 100,000 <sphere segments="4096"/> objects -- each
// legal on its own -- request ~8.6e11 vertices combined). This tracks running
// totals across the WHOLE document being parsed and rejects before the
// corresponding generation would be attempted downstream (mc3togltf's
// buildPrimitive/buildExtrude), not after allocating.
//
// thread_local, not a parameter threaded through every parse* function,
// because parseObject/parseChildren/parsePrimitive/parseCrossSection/
// parseExtrude are a large, already-recursive call graph with no existing
// context-object plumbing; adding one is a much larger refactor than this
// fix's scope justifies. reset() is called once at the top of
// buildDocumentFromRoot(), so nested <include> parses (which reuse the same
// call graph) correctly charge against the SAME top-level document's budget,
// and a later unrelated parse (e.g. the next test in the same process) starts
// fresh.
struct DocumentBudget {
    long long totalObjects = 0;
    long long totalTessellationWeight = 0; // sum of every segments/sides/subdivisions value

    // Generous enough for any real scene (the largest checked-in stress
    // fixture sums to a few thousand) while still bounding the pathological
    // many-objects-near-the-per-field-cap case to a small multiple of that cap.
    static constexpr long long kMaxTotalObjects = 100'000;
    static constexpr long long kMaxTotalTessellationWeight = 500'000;

    void chargeObject() {
        if (++totalObjects > kMaxTotalObjects)
            throw std::runtime_error(
                "MC3: document exceeds the total object budget (" +
                std::to_string(kMaxTotalObjects) + ") -- rejected before allocating"
                " geometry for all of them");
    }
    void chargeTessellation(int weight) {
        totalTessellationWeight += weight;
        if (totalTessellationWeight > kMaxTotalTessellationWeight)
            throw std::runtime_error(
                "MC3: document's total tessellation complexity (sum of all "
                "segments/sides/subdivisions values, " +
                std::to_string(totalTessellationWeight) + ") exceeds the budget (" +
                std::to_string(kMaxTotalTessellationWeight) +
                ") -- rejected before allocating geometry for all of it");
    }
    void reset() { totalObjects = 0; totalTessellationWeight = 0; }
};
static thread_local DocumentBudget g_budget;

// Wraps attrCount() and also charges the result against the document-wide
// tessellation budget.
static int attrCountBudgeted(const XMLElement* el, const char* name, int def,
                             int minv, int maxv = kMaxTessellation) {
    int v = attrCount(el, name, def, minv, maxv);
    g_budget.chargeTessellation(v);
    return v;
}

// AUD-006b: forward-declared here so parseObject/parseTextures/parseSounds/
// parseMusic/parseEmbeds (all defined before validateResourcePathIfConfined's
// own definition, further down this file) can call it. Full documentation is
// at the definition site.
static void validateResourcePathIfConfined(const std::string& rawPath, const char* kind);

static bool attrB(const XMLElement* el, const char* name, bool def = false) {
    const char* v = el->Attribute(name);
    if (!v) return def;
    return parseBool(v);
}

static std::array<float,3> attrVec3(const XMLElement* el, const char* name,
                                     std::array<float,3> def = {0,0,0}) {
    const char* v = el->Attribute(name);
    return v ? parseVec3(v, def) : def;
}

static std::string childText(const XMLElement* el, const char* childName) {
    const XMLElement* c = el->FirstChildElement(childName);
    if (!c || !c->GetText()) return {};
    return c->GetText();
}

// ---------------------------------------------------------------------------
// Transform
// ---------------------------------------------------------------------------

static Mc3Transform parseTransform(const XMLElement* el) {
    Mc3Transform t;
    t.position = attrVec3(el, "position");
    t.rotation = attrVec3(el, "rotation");
    const char* sv = el->Attribute("scale");
    if (sv) {
        std::string s = sv;
        if (s.find(' ') == std::string::npos && s.find(',') == std::string::npos) {
            // STAB-0080: a malformed single-value scale (e.g. "abc") must not
            // throw uncaught and fail the whole file load.
            try { float f = Internal::finiteOr(std::stof(s), 1.0f); t.scale = {f, f, f}; }
            catch (...) { t.scale = {1, 1, 1}; }
        } else {
            t.scale = parseVec3(s, {1,1,1});
        }
    }
    t.pivot = attrVec3(el, "pivot");
    return t;
}

static std::optional<Mc3Deform> parseDeform(const XMLElement* el) {
    const XMLElement* d = el->FirstChildElement("deform");
    if (!d) return std::nullopt;
    Mc3Deform def;
    def.scale = attrVec3(d, "scale", {1,1,1});
    return def;
}

// ---------------------------------------------------------------------------
// Cross-section + path (extrude)
// ---------------------------------------------------------------------------

static Mc3CrossSection parseCrossSection(const XMLElement* el) {
    Mc3CrossSection cs;
    std::string t = attr(el, "type", "rect");
    if      (t == "rect")    cs.type = CrossSectionType::Rect;
    else if (t == "circle")  cs.type = CrossSectionType::Circle;
    else if (t == "polygon") cs.type = CrossSectionType::Polygon;
    else if (t == "star")    cs.type = CrossSectionType::Star;
    else                     cs.type = CrossSectionType::Custom;
    cs.width       = attrF(el, "width",        0.3f);
    cs.height      = attrF(el, "height",       0.3f);
    cs.radius      = attrF(el, "radius",       0.1f);
    cs.innerRadius = attrF(el, "inner_radius", 0.0f);
    cs.sides       = attrCountBudgeted(el, "sides",    6, 3);
    cs.segments    = attrCountBudgeted(el, "segments", 32, 1);
    for (const XMLElement* p = el->FirstChildElement("point"); p; p = p->NextSiblingElement("point")) {
        Mc3CrossSection::Point2D pt;
        pt.x = attrF(p, "x", 0);
        pt.y = attrF(p, "y", 0);
        cs.customPoints.push_back(pt);
    }
    return cs;
}

static Mc3ExtrudePath parsePath(const XMLElement* el) {
    Mc3ExtrudePath path;
    std::string t = attr(el, "type", "line");
    if      (t == "line")     path.type = ExtrudePathType::Line;
    else if (t == "arc")      path.type = ExtrudePathType::Arc;
    else if (t == "helix")    path.type = ExtrudePathType::Helix;
    else if (t == "polyline") path.type = ExtrudePathType::Polyline;
    else if (t == "bezier")   path.type = ExtrudePathType::Bezier;
    path.length      = attrF(el, "length",  1.0f);
    path.axis        = attr (el, "axis",   "y");
    path.arcRadius   = attrF(el, "radius", 1.0f);
    path.arcAngle    = attrF(el, "angle",  180.0f);
    path.helixRadius = attrF(el, "radius", 0.5f);
    path.helixHeight = attrF(el, "height", 2.0f);
    path.helixTurns  = attrF(el, "turns",  4.0f);
    for (const XMLElement* p = el->FirstChildElement("point"); p; p = p->NextSiblingElement("point")) {
        Mc3PathPoint pt;
        pt.position = {attrF(p,"x",0), attrF(p,"y",0), attrF(p,"z",0)};
        pt.controlIn = {attrF(p,"cx",0), attrF(p,"cy",0), attrF(p,"cz",0)};
        path.points.push_back(pt);
    }
    return path;
}

static std::optional<Mc3Extrude> parseExtrude(const XMLElement* el) {
    const XMLElement* cs = el->FirstChildElement("cross_section");
    const XMLElement* pt = el->FirstChildElement("path");
    if (!cs || !pt) {
        std::cerr << "Warning: <extrude> missing <cross_section> or <path>, skipped.\n";
        return std::nullopt;
    }
    Mc3Extrude ext;
    ext.crossSection = parseCrossSection(cs);
    ext.path         = parsePath(pt);
    ext.twist    = attrF(el, "twist",    0.0f);
    ext.segments = attrCountBudgeted(el, "segments", 32, 1);
    ext.smooth   = attrB(el, "smooth",   true);
    ext.caps     = attrB(el, "caps",     true);
    return ext;
}

// ---------------------------------------------------------------------------
// Primitive
// ---------------------------------------------------------------------------

static Mc3Primitive parsePrimitive(const XMLElement* el, ObjectType type) {
    Mc3Primitive p;
    switch (type) {
    case ObjectType::Box:      p.primitiveType = PrimitiveType::Box;      break;
    case ObjectType::Cube:     p.primitiveType = PrimitiveType::Cube;     break;
    case ObjectType::Sphere:   p.primitiveType = PrimitiveType::Sphere;   break;
    case ObjectType::Cylinder: p.primitiveType = PrimitiveType::Cylinder; break;
    case ObjectType::Cone:     p.primitiveType = PrimitiveType::Cone;     break;
    case ObjectType::Plane:    p.primitiveType = PrimitiveType::Plane;    break;
    case ObjectType::Torus:    p.primitiveType = PrimitiveType::Torus;    break;
    case ObjectType::Capsule:  p.primitiveType = PrimitiveType::Capsule;  break;
    case ObjectType::Disk:     p.primitiveType = PrimitiveType::Disk;     break;
    case ObjectType::Grid:      p.primitiveType = PrimitiveType::Grid;      break;
    case ObjectType::IcoSphere: p.primitiveType = PrimitiveType::IcoSphere; break;
    default: break;
    }
    if (const char* sv = el->Attribute("size")) {
        std::string s = sv;
        if (s.find(' ') == std::string::npos && s.find(',') == std::string::npos) {
            // STAB-0080: a malformed single-value size (e.g. "abc") must not
            // throw uncaught and fail the whole file load.
            try { float f = Internal::finiteOr(std::stof(s), 1.0f); p.size = {f, f, f}; }
            catch (...) { /* p.size keeps its default-constructed value */ }
        } else if (type == ObjectType::Plane) {
            // Plane size is vec2 (width × depth = X × Z). Legacy XMLs may have "W 0 D" (3 values).
            std::istringstream iss(s);
            float a = 1.0f, b = 1.0f, c = 0.0f;
            iss >> a >> b;
            if (iss >> c)
                p.size = {a, b, c};       // legacy 3-value "W 0 D" → size[0]=W, size[2]=D
            else
                p.size = {a, 1.0f, b};   // canonical vec2 "W D" → size[0]=W, size[2]=D
        } else {
            auto v = parseVec3(s);
            p.size = {v[0], v[1], v[2]};
        }
    }
    p.radius        = attrF(el, "radius",         0.5f);
    p.height        = attrF(el, "height",         1.0f);
    // IcoSphere default "segments" is 2; all other primitives default to 32.
    // The count is clamped to a safe upper bound so a hostile value can't drive
    // unbounded allocation. Note IcoSphere reuses the same 0..32-style scale as
    // Sphere: buildIcoSphere() internally maps it to min(4, segments/8)
    // subdivisions, so its actual triangle count is already bounded there — the
    // clamp here just stops the raw integer from being absurd.
    p.segments      = attrCountBudgeted(el, "segments", type == ObjectType::IcoSphere ? 2 : 32, 0);
    p.axis          = attr (el, "axis",           "y");
    p.majorRadius   = attrF(el, "major_radius",   0.35f);
    if (type == ObjectType::Disk) {
        // Disk uses inner_radius (0=solid); accept legacy minor_radius for compat.
        float ir = attrF(el, "inner_radius", -1.0f);
        if (ir < 0.0f) ir = attrF(el, "minor_radius", 0.0f);
        p.minorRadius = ir;
    } else {
        p.minorRadius = attrF(el, "minor_radius", 0.15f);
    }
    p.subdivisionsX = attrCountBudgeted(el, "subdivisions_x", 4, 1);
    p.subdivisionsZ = attrCountBudgeted(el, "subdivisions_z", 4, 1);
    return p;
}

// ---------------------------------------------------------------------------
// Object (forward declaration for recursive group parsing)
// ---------------------------------------------------------------------------

static std::shared_ptr<Mc3Object> parseObject(const XMLElement* el);

static void parseCommonObjectAttribs(const XMLElement* el, Mc3Object& obj) {
    obj.name      = attr(el, "name");
    obj.id        = attr(el, "id");
    obj.material  = attr(el, "material");
    obj.visible   = attrB(el, "visible", true);
    obj.collision = attr(el, "collision", "none");
    obj.layer     = attr(el, "layer");
    obj.isCutter  = attrB(el, "role_cutter", false) || (std::string(attr(el, "role")) == "cutter");
    obj.transform = parseTransform(el);
    obj.deform    = parseDeform(el);
    if (const char* tags = el->Attribute("tags")) {
        std::istringstream ss(tags);
        std::string token;
        while (ss >> token) obj.tags.push_back(token);
    }
    if (const XMLElement* uv = el->FirstChildElement("uv_mapping")) {
        Mc3UvMapping m;
        std::string proj = attr(uv, "projection", "planar");
        if      (proj == "box")    m.projection = UvProjection::Box;
        else if (proj == "sphere") m.projection = UvProjection::Sphere;
        m.scaleU   = attrF(uv, "scale_u",  1.0f);
        m.scaleV   = attrF(uv, "scale_v",  1.0f);
        m.offsetU  = attrF(uv, "offset_u", 0.0f);
        m.offsetV  = attrF(uv, "offset_v", 0.0f);
        m.rotation = attrF(uv, "rotation", 0.0f);
        obj.uvMapping = m;
    }
    if (const XMLElement* meta = el->FirstChildElement("metadata"))
        for (const XMLElement* p = meta->FirstChildElement("property"); p;
             p = p->NextSiblingElement("property"))
            if (const char* n = p->Attribute("name"))
                if (const char* v = p->Attribute("value"))
                    obj.metadata[n] = v;
}

static void parseChildren(const XMLElement* el, Mc3Object& obj) {
    for (const XMLElement* c = el->FirstChildElement(); c; c = c->NextSiblingElement()) {
        auto child = parseObject(c);
        if (child) obj.children.push_back(child);
    }
}

static std::shared_ptr<Mc3Object> parseObject(const XMLElement* el) {
    if (!el) return nullptr;
    std::string tag = el->Name();
    if (tag == "state" || tag == "deform") return nullptr; // handled by parent parsers
    g_budget.chargeObject();
    auto obj = std::make_shared<Mc3Object>();

    if (tag == "box") {
        obj->type = ObjectType::Box;
        obj->primitive = parsePrimitive(el, ObjectType::Box);
    } else if (tag == "cube") {
        obj->type = ObjectType::Cube;
        obj->primitive = parsePrimitive(el, ObjectType::Cube);
    } else if (tag == "sphere") {
        obj->type = ObjectType::Sphere;
        obj->primitive = parsePrimitive(el, ObjectType::Sphere);
    } else if (tag == "cylinder") {
        obj->type = ObjectType::Cylinder;
        obj->primitive = parsePrimitive(el, ObjectType::Cylinder);
    } else if (tag == "cone") {
        obj->type = ObjectType::Cone;
        obj->primitive = parsePrimitive(el, ObjectType::Cone);
    } else if (tag == "plane") {
        obj->type = ObjectType::Plane;
        obj->primitive = parsePrimitive(el, ObjectType::Plane);
    } else if (tag == "torus") {
        obj->type = ObjectType::Torus;
        obj->primitive = parsePrimitive(el, ObjectType::Torus);
    } else if (tag == "capsule") {
        obj->type = ObjectType::Capsule;
        obj->primitive = parsePrimitive(el, ObjectType::Capsule);
    } else if (tag == "disk") {
        obj->type = ObjectType::Disk;
        obj->primitive = parsePrimitive(el, ObjectType::Disk);
    } else if (tag == "grid") {
        obj->type = ObjectType::Grid;
        obj->primitive = parsePrimitive(el, ObjectType::Grid);
    } else if (tag == "icosphere") {
        obj->type = ObjectType::IcoSphere;
        obj->primitive = parsePrimitive(el, ObjectType::IcoSphere);
    } else if (tag == "mesh") {
        obj->type       = ObjectType::Mesh;
        obj->meshSource = attr(el, "src");
        if (obj->meshSource.empty())          // accept legacy source= attribute
            obj->meshSource = attr(el, "source");
        validateResourcePathIfConfined(obj->meshSource, "mesh source");
    } else if (tag == "extrude") {
        obj->type   = ObjectType::Extrude;
        obj->extrude = parseExtrude(el);
    } else if (tag == "group") {
        obj->type = ObjectType::Group;
    } else if (tag == "instance") {
        obj->type             = ObjectType::Instance;
        obj->definition       = attr(el, "definition");
        obj->materialOverride = attr(el, "material_override");
        // variants: space-separated list of definition IDs
        std::string varStr = attr(el, "variants");
        if (!varStr.empty()) {
            std::istringstream iss(varStr);
            std::string tok;
            while (iss >> tok) obj->variantDefinitions.push_back(tok);
        }
    } else if (tag == "union") {
        obj->type = ObjectType::Union;
        obj->csgOperation = Mc3CsgOperation{CsgType::Union};
    } else if (tag == "difference") {
        obj->type = ObjectType::Difference;
        obj->csgOperation = Mc3CsgOperation{CsgType::Difference};
    } else if (tag == "intersection") {
        obj->type = ObjectType::Intersection;
        obj->csgOperation = Mc3CsgOperation{CsgType::Intersection};
    } else if (tag == "area") {
        obj->type = ObjectType::Area;
        // STAB-0031: areaType's `size` attribute (mc3.xsd) was declared but
        // never actually parsed anywhere — parsePrimitive()'s generic size
        // handling captures it even though Area has no PrimitiveType case.
        if (el->Attribute("size")) obj->primitive = parsePrimitive(el, ObjectType::Area);
    } else {
        std::cerr << "Warning: unknown object type <" << tag << ">, skipped.\n";
        return nullptr;
    }

    parseCommonObjectAttribs(el, *obj);

    // Parse named states (<state id="open" position="..." .../>)
    for (const XMLElement* s = el->FirstChildElement("state"); s; s = s->NextSiblingElement("state")) {
        std::string stateId = attr(s, "id");
        if (stateId.empty()) continue;
        Mc3ObjectState st;
        if (s->Attribute("position")) st.position = attrVec3(s, "position");
        if (s->Attribute("rotation")) st.rotation = attrVec3(s, "rotation");
        if (s->Attribute("scale"))    st.scale    = attrVec3(s, "scale", {1,1,1});
        if (s->Attribute("visible"))  st.visible  = attrB(s, "visible", true);
        if (s->Attribute("material")) st.material = std::string(attr(s, "material"));
        obj->states[stateId] = st;
    }

    bool isGroup = (tag == "group" || tag == "union" || tag == "difference" || tag == "intersection" || tag == "area");
    if (isGroup) parseChildren(el, *obj);

    return obj;
}

// ---------------------------------------------------------------------------
// Top-level section parsers
// ---------------------------------------------------------------------------

static void parseEnvironment(const XMLElement* el, Mc3Document& doc) {
    Mc3Environment env;
    if (const XMLElement* bg = el->FirstChildElement("background"))
        env.backgroundColor = attrVec3(bg, "color");
    if (const XMLElement* bt = el->FirstChildElement("background_texture")) {
        if (const char* t = bt->GetText()) env.backgroundTexture = t;
    }
    if (const XMLElement* st = el->FirstChildElement("skybox_texture")) {
        if (const char* t = st->GetText()) env.skyboxTexture = t;
    }
    if (const XMLElement* fg = el->FirstChildElement("fog")) {
        Mc3Fog fog;
        fog.color   = attrVec3(fg, "color", {0.5f,0.5f,0.5f});
        fog.start   = attrF(fg, "start",   10.0f);
        fog.end     = attrF(fg, "end",     100.0f);
        fog.density = attrF(fg, "density", 0.01f);
        std::string mode = attr(fg, "mode", "linear");
        fog.mode = (mode == "exponential") ? FogMode::Exponential : FogMode::Linear;
        env.fog = fog;
    }
    doc.environment = env;
}

static void parseLights(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement(); c; c = c->NextSiblingElement()) {
        Mc3Light light;
        std::string t = c->Name();
        if (t == "ambient") {
            light.type       = LightType::Ambient;
            light.name       = attr(c, "name");
            light.color      = attrVec3(c, "color", {1,1,1});
            light.brightness = attrF(c, "brightness", 1.0f);
        } else if (t == "directional") {
            light.type       = LightType::Directional;
            light.name       = attr(c, "name");
            light.color      = attrVec3(c, "color", {1,1,1});
            light.brightness = attrF(c, "brightness", 1.0f);
            light.direction  = attrVec3(c, "direction", {0,-1,0});
            light.castShadows = attrB(c, "cast_shadows");
        } else if (t == "spot") {
            light.type       = LightType::Spot;
            light.name       = attr(c, "name");
            light.color      = attrVec3(c, "color", {1,1,1});
            light.brightness = attrF(c, "brightness", 1.0f);
            light.position   = attrVec3(c, "position");
            light.direction  = attrVec3(c, "direction", {0,-1,0});
            light.angle      = attrF(c, "angle",   45.0f);
            light.falloff    = attrF(c, "falloff", 0.0f);
            light.range      = attrF(c, "range",   0.0f);
            light.castShadows = attrB(c, "cast_shadows");
        } else if (t == "point") {
            light.type       = LightType::Point;
            light.name       = attr(c, "name");
            light.color      = attrVec3(c, "color", {1,1,1});
            light.brightness = attrF(c, "brightness", 1.0f);
            light.position   = attrVec3(c, "position");
            light.range      = attrF(c, "range", 0.0f);
            light.castShadows = attrB(c, "cast_shadows");
        } else {
            std::cerr << "Warning: unknown light type <" << t << ">, ignored.\n";
            continue;
        }
        doc.lights.push_back(light);
    }
}

static void parseCameras(const XMLElement* el, Mc3Document& doc) {
    // Only overrides the root-level default_camera (set before this runs)
    // when <cameras default="..."> is explicitly present.
    if (const char* d = el->Attribute("default")) doc.defaultCamera = d;
    for (const XMLElement* c = el->FirstChildElement("camera"); c;
         c = c->NextSiblingElement("camera")) {
        Mc3Camera cam;
        cam.name      = attr(c, "name");
        cam.position  = attrVec3(c, "position", {0,5,10});
        cam.target    = attrVec3(c, "target",   {0,0,0});
        cam.nearPlane = attrF(c, "near",  0.1f);
        cam.farPlane  = attrF(c, "far",  1000.0f);
        cam.fov       = attrF(c, "fov",   60.0f);
        cam.orthoSize = attrF(c, "size",  10.0f);
        cam.orthoAspect = attrF(c, "aspect", 1.0f);   // STAB-0695
        std::string t = attr(c, "type", "perspective");
        cam.type = (t == "orthographic") ? CameraType::Orthographic : CameraType::Perspective;
        if (const char* rot = c->Attribute("rotation"))
            cam.rotation = parseVec3(rot);
        doc.cameras.push_back(cam);
    }
    if (doc.defaultCamera.empty() && !doc.cameras.empty())
        doc.defaultCamera = doc.cameras.front().name;
}

static void parseTextures(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement("texture"); c;
         c = c->NextSiblingElement("texture")) {
        std::string id   = attr(c, "id");
        std::string type = attr(c, "type");
        if (type == "svg") {
            Mc3SvgTexture svg;
            svg.id  = id;
            svg.src = attr(c, "src");
            validateResourcePathIfConfined(svg.src, "SVG texture src");
            if (svg.src.empty()) {
                const char* text = c->GetText();
                if (text) svg.inlineContent = text;
            }
            if (!id.empty()) doc.svgTextures[id] = std::move(svg);
            continue;
        }
        Mc3Texture tex;
        tex.name       = attr(c, "name", id.c_str());
        tex.uri        = attr(c, "uri");
        validateResourcePathIfConfined(tex.uri, "texture uri");
        tex.wrapU      = attr(c, "wrap_u",      "repeat");
        tex.wrapV      = attr(c, "wrap_v",      "repeat");
        tex.filter     = attr(c, "filter",      "linear");
        tex.colorSpace = attr(c, "color_space", "srgb");
        tex.mipMaps    = attrB(c, "mip_maps",   true);
        doc.textures[id] = tex;
    }
}

static void parseMaterials(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement("material"); c;
         c = c->NextSiblingElement("material")) {
        Mc3Material mat;
        std::string id = attr(c, "id");
        mat.name        = id;
        mat.roughness   = attrF(c, "roughness",    0.5f);
        mat.metallic    = attrF(c, "metallic",     0.0f);
        mat.alphaMode   = attr (c, "alpha_mode",  "opaque");
        mat.doubleSided = attrB(c, "double_sided", false);
        std::string bcText = childText(c, "base_color");
        if (!bcText.empty()) {
            auto v = parseVec4(bcText, {0.8f,0.8f,0.8f,1.0f});
            mat.baseColor = {v[0], v[1], v[2], v[3]};
        }
        mat.baseColorTexture         = childText(c, "base_color_texture");
        mat.metallicRoughnessTexture = childText(c, "metallic_roughness_texture");
        mat.normalTexture            = childText(c, "normal_texture");
        mat.occlusionTexture         = childText(c, "occlusion_texture");
        mat.emissiveTexture          = childText(c, "emissive_texture");
        mat.normalScale       = attrF(c, "normal_scale",      1.0f);
        mat.occlusionStrength = attrF(c, "occlusion_strength", 1.0f);
        mat.alphaCutoff       = attrF(c, "alpha_cutoff",      0.5f);
        std::string ec = childText(c, "emissive_color");
        if (!ec.empty()) {
            auto v = parseVec3(ec);
            mat.emissiveColor = {v[0], v[1], v[2]};
        }
        doc.materials[id] = mat;
    }
}

static void parseStates(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* s = el->FirstChildElement("state"); s;
         s = s->NextSiblingElement("state")) {
        std::string name = attr(s, "name");
        if (name.empty()) continue;
        Mc3SceneState state;
        state.name = name;
        for (const XMLElement* ov = s->FirstChildElement("object-override"); ov;
             ov = ov->NextSiblingElement("object-override")) {
            std::string id = attr(ov, "id");
            if (id.empty()) continue;
            Mc3ObjectOverride ovr;
            ovr.id = id;
            if (ov->Attribute("visible"))  ovr.visible  = attrB(ov, "visible", true);
            if (ov->Attribute("position")) ovr.position = attrVec3(ov, "position");
            if (ov->Attribute("rotation")) ovr.rotation = attrVec3(ov, "rotation");
            if (ov->Attribute("material")) ovr.material = std::string(attr(ov, "material"));
            state.overrides.push_back(std::move(ovr));
        }
        doc.sceneStates[name] = std::move(state);
    }
}

static void parseTriggers(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* t = el->FirstChildElement("trigger"); t;
         t = t->NextSiblingElement("trigger")) {
        std::string id = attr(t, "id");
        if (id.empty()) continue;
        Mc3Trigger trig;
        trig.id = id;
        for (const XMLElement* s = t->FirstChildElement(); s;
             s = s->NextSiblingElement()) {
            std::string name = s->Name();
            Mc3TriggerStep step;
            if      (name == "play-action") step.type = TriggerStepType::PlayAction;
            else if (name == "play-sound")  step.type = TriggerStepType::PlaySound;
            else if (name == "run-script")  step.type = TriggerStepType::RunScript;
            else if (name == "play-music")  step.type = TriggerStepType::PlayMusic;
            else continue;
            step.ref = attr(s, "ref");
            trig.steps.push_back(std::move(step));
        }
        doc.triggers[id] = std::move(trig);
    }
}

static void parseSounds(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement("sound"); c;
         c = c->NextSiblingElement("sound")) {
        std::string id = attr(c, "id");
        if (id.empty()) continue;
        Mc3Sound snd;
        snd.id   = id;
        snd.src  = attr(c, "src");
        validateResourcePathIfConfined(snd.src, "sound src");
        snd.loop = attrB(c, "loop", false);
        doc.sounds[id] = std::move(snd);
    }
}

static void parseMusic(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement("track"); c;
         c = c->NextSiblingElement("track")) {
        std::string id = attr(c, "id");
        if (id.empty()) continue;
        Mc3Music mus;
        mus.id   = id;
        mus.src  = attr(c, "src");
        validateResourcePathIfConfined(mus.src, "music src");
        mus.loop = attrB(c, "loop", true);
        doc.musicTracks[id] = std::move(mus);
    }
}

static void parseScripts(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement("script"); c;
         c = c->NextSiblingElement("script")) {
        std::string id   = attr(c, "id");
        std::string type = attr(c, "type");
        if (id.empty()) continue;
        Mc3Script sc;
        sc.id   = id;
        sc.type = type;
        const char* text = c->GetText();
        if (text) sc.source = text;
        doc.scripts[id] = std::move(sc);
    }
}

static void parseEmbeds(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement("embed"); c;
         c = c->NextSiblingElement("embed")) {
        std::string id   = attr(c, "id");
        std::string type = attr(c, "type");
        if (type != "gltf" || id.empty()) continue;
        Mc3EmbedGltf em;
        em.id  = id;
        em.src = attr(c, "src");
        validateResourcePathIfConfined(em.src, "embed src");
        if (em.src.empty()) {
            const char* text = c->GetText();
            if (text) em.base64Content = text;
        }
        doc.embeds[id] = std::move(em);
    }
}

static void parseDefinitions(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement("definition"); c;
         c = c->NextSiblingElement("definition")) {
        std::string id = attr(c, "id");
        for (const XMLElement* child = c->FirstChildElement(); child;
             child = child->NextSiblingElement()) {
            auto obj = parseObject(child);
            if (obj) { doc.definitions[id] = obj; break; }
        }
    }
}

static void parseObjects(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement(); c; c = c->NextSiblingElement()) {
        auto obj = parseObject(c);
        if (obj) doc.objects.push_back(obj);
    }
}

static Interpolation parseInterpolation(const char* s) {
    if (!s) return Interpolation::Linear;
    std::string v = s;
    if (v == "step")   return Interpolation::Step;
    if (v == "cubic")  return Interpolation::CubicBezier;
    return Interpolation::Linear;
}

static void parseActions(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* ae = el->FirstChildElement("action"); ae;
         ae = ae->NextSiblingElement("action")) {
        Mc3Action action;
        action.name      = attr(ae, "name");
        action.duration  = attrF(ae, "duration", 1.0f);
        action.loop      = attrB(ae, "loop", false);
        action.autoplay  = attrB(ae, "autoplay", false);
        action.timeScale = attrF(ae, "time_scale", 1.0f); // STAB-0460
        if (action.name.empty()) continue;

        for (const XMLElement* ce = ae->FirstChildElement("channel"); ce;
             ce = ce->NextSiblingElement("channel")) {
            Mc3Channel ch;
            ch.targetObject = attr(ce, "target");
            auto prop = animatedPropertyFromName(attr(ce, "property", ""));
            if (!prop || ch.targetObject.empty()) continue;
            ch.property = *prop;

            for (const XMLElement* ke = ce->FirstChildElement("keyframe"); ke;
                 ke = ke->NextSiblingElement("keyframe")) {
                Mc3Keyframe kf;
                kf.time          = attrF(ke, "time",  0.0f);
                kf.value         = attrF(ke, "value", 0.0f);
                kf.interpolation = parseInterpolation(ke->Attribute("interp"));
                if (const XMLElement* hl = ke->FirstChildElement("handle_left")) {
                    kf.handleLeft.dt = attrF(hl, "dt", -0.1f);
                    kf.handleLeft.dv = attrF(hl, "dv",  0.0f);
                }
                if (const XMLElement* hr = ke->FirstChildElement("handle_right")) {
                    kf.handleRight.dt = attrF(hr, "dt", 0.1f);
                    kf.handleRight.dv = attrF(hr, "dv", 0.0f);
                }
                ch.keyframes.push_back(kf);
            }
            // Ensure keyframes are sorted by time. Stable so two keyframes at
            // the identical time keep their original (first-declared-wins)
            // relative order — plain std::sort doesn't guarantee this for
            // equal keys (STAB-0468).
            std::stable_sort(ch.keyframes.begin(), ch.keyframes.end(),
                [](const Mc3Keyframe& a, const Mc3Keyframe& b){ return a.time < b.time; });
            action.channels.push_back(std::move(ch));
        }
        doc.actions[action.name] = std::move(action);
    }
}

// ---------------------------------------------------------------------------
// Include processing
// ---------------------------------------------------------------------------

// Forward declarations (the actual functions are defined above).
static void parseTextures   (const XMLElement*, Mc3Document&);
static void parseMaterials  (const XMLElement*, Mc3Document&);
static void parseDefinitions(const XMLElement*, Mc3Document&);

static void processIncludes(const XMLElement* root, Mc3Document& doc,
                             const std::filesystem::path& selfPath,
                             std::set<std::filesystem::path>& inProgress,
                             std::set<std::filesystem::path>& processed,
                             bool recordIncludes,
                             const Mc3LoadPolicy& policy,
                             int depth);

// Returns true if `candidate` resolves inside `rootDir` (no `..`-escape, not an
// unrelated absolute path). Used to confine includes for untrusted content.
static bool includePathWithinRoot(const std::filesystem::path& candidate,
                                   const std::filesystem::path& rootDir) {
    // doc.sourcePath (rootDir) is `selfPath.parent_path()`, which is EMPTY when
    // the document was opened via a bare relative filename with no directory
    // component (e.g. `mc3togltf scene.mc3.xml out.glb` run from the scene's own
    // directory -- the common case). weakly_canonical("") returns an empty path
    // rather than resolving to the current working directory or erroring, so an
    // empty rootDir must be normalized to "." first; otherwise `r` stays empty,
    // relative(c, r) against an empty base returns empty too, and every
    // same-directory include would be wrongly rejected as "escaping the root".
    std::error_code ec;
    auto c = std::filesystem::weakly_canonical(candidate, ec);
    if (ec) return false;
    auto r = std::filesystem::weakly_canonical(rootDir.empty() ? std::filesystem::path(".") : rootDir, ec);
    if (ec) return false;
    auto rel = std::filesystem::relative(c, r, ec);
    if (ec || rel.empty()) return false;
    return rel.native().rfind("..", 0) != 0;  // does not start with ".."
}

// AUD-006b: current parse's resource-confinement state, set once at the top of
// buildDocumentFromRoot() (same thread_local pattern as g_budget above, for
// the same reason: parseObject/parseTextures/parseSounds/parseMusic/
// parseEmbeds have no existing context-object parameter to thread a policy
// through, and adding one is a larger refactor than this fix's scope
// justifies). Reset once per top-level parse, not per <include>, matching
// g_budget's reset discipline.
static thread_local bool g_confineResourcePaths = false;
static thread_local std::filesystem::path g_resourceRoot;

// Validates a texture/SVG/mesh/embed/sound/music `src`/`uri` field against the
// current parse's confinement policy. No-op when not confining (trusted
// documents keep full permissive behavior) or when `rawPath` is empty or a
// non-filesystem pseudo-path (`embed:`/`data:`). Throws a clear error naming
// `kind` when the path is absolute or escapes the document root.
static void validateResourcePathIfConfined(const std::string& rawPath, const char* kind) {
    if (!g_confineResourcePaths || rawPath.empty()) return;
    if (rawPath.rfind("embed:", 0) == 0 || rawPath.rfind("data:", 0) == 0) return;

    std::filesystem::path p(rawPath);
    if (p.is_absolute())
        throw std::runtime_error(
            std::string("MC3: ") + kind + " '" + rawPath +
            "' is an absolute path outside the document root; rejected under "
            "the untrusted-content load policy");

    if (!includePathWithinRoot(g_resourceRoot / p, g_resourceRoot))
        throw std::runtime_error(
            std::string("MC3: ") + kind + " '" + rawPath +
            "' escapes the document root; rejected under the untrusted-content "
            "load policy");
}

// Re-express a path that's relative to `fromDir` (the file that actually
// contains it) so it resolves correctly relative to `toDir` (doc.sourcePath,
// the *main* document's directory) instead — needed because everything in
// doc ends up resolved against a single doc.sourcePath, regardless of which
// included file a texture/mesh reference actually came from (STAB-0550).
// Leaves the path unchanged if either directory can't be resolved.
static std::string rebaseRelativePath(const std::string& relPath,
                                       const std::filesystem::path& fromDir,
                                       const std::filesystem::path& toDir) {
    std::error_code ec;
    auto abs = std::filesystem::weakly_canonical(fromDir / relPath, ec);
    if (ec) return relPath;
    auto rel = std::filesystem::relative(abs, toDir, ec);
    if (ec) return relPath;
    return rel.generic_string();
}

// Recursively rebases meshSource on every Mesh-type object in an included
// definition's subtree (skips "embed:<id>" references — those resolve
// through doc.embeds, not the filesystem).
static void rebaseDefinitionMeshSources(Mc3Object& obj,
                                         const std::filesystem::path& fromDir,
                                         const std::filesystem::path& toDir) {
    if (obj.type == ObjectType::Mesh && !obj.meshSource.empty() &&
        obj.meshSource.rfind("embed:", 0) != 0) {
        obj.meshSource = rebaseRelativePath(obj.meshSource, fromDir, toDir);
    }
    for (auto& child : obj.children)
        if (child) rebaseDefinitionMeshSources(*child, fromDir, toDir);
}

// Merge definitions/materials/textures from one included file into doc.
// Respects cycle detection: throws on cyclic includes, silently skips
// already-processed files (diamond-include deduplication).
static void mergeInclude(const std::filesystem::path& includePath,
                          Mc3Document& doc,
                          std::set<std::filesystem::path>& inProgress,
                          std::set<std::filesystem::path>& processed,
                          const Mc3LoadPolicy& policy,
                          int depth)
{
    std::filesystem::path canonical;
    try {
        canonical = std::filesystem::weakly_canonical(includePath);
    } catch (...) {
        canonical = std::filesystem::absolute(includePath);
    }

    if (inProgress.count(canonical))
        throw std::runtime_error("Cyclic <include> detected: " + includePath.string());

    if (processed.count(canonical))
        return;  // already merged via a different include path — skip silently

    XMLDocument xml;
    if (xml.LoadFile(includePath.string().c_str()) != XML_SUCCESS)
        throw std::runtime_error("Failed to load <include> file '" +
                                  includePath.string() + "': " + xml.ErrorStr());

    const XMLElement* root = xml.FirstChildElement("mc3");
    if (!root)
        throw std::runtime_error("No <mc3> root element in included file: " +
                                  includePath.string());

    inProgress.insert(canonical);

    // Recurse into nested includes first (do NOT record them in doc.includes)
    processIncludes(root, doc, includePath, inProgress, processed,
                    /*recordIncludes=*/false, policy, depth + 1);

    // Merge shared assets (NOT objects/lights/cameras/environment/actions —
    // those belong to the main scene only). AUDIT-0037: id collisions across
    // includes keep the existing last-write-wins behavior, but now log a
    // warning so authors can spot unintended overrides.
    if (const XMLElement* txs  = root->FirstChildElement("textures")) {
        for (const XMLElement* c = txs->FirstChildElement("texture"); c;
             c = c->NextSiblingElement("texture"))
            if (const char* id = c->Attribute("id"))
                if (doc.textures.count(id) || doc.svgTextures.count(id))
                    std::cerr << "Warning: <include file=\"" << includePath.string()
                              << "\"> texture id '" << id
                              << "' collides with an already-loaded texture; last-write-wins.\n";
        parseTextures(txs, doc);
        for (const XMLElement* c = txs->FirstChildElement("texture"); c;
             c = c->NextSiblingElement("texture"))
            if (const char* id = c->Attribute("id")) {
                doc.includedTextures.insert(id);
                // STAB-0550: rebase the texture's uri (or an external SVG
                // texture's src) so it still resolves correctly against
                // doc.sourcePath, not includePath's own directory (only
                // matters when they differ).
                auto texIt = doc.textures.find(id);
                if (texIt != doc.textures.end() && !texIt->second.uri.empty())
                    texIt->second.uri = rebaseRelativePath(
                        texIt->second.uri, includePath.parent_path(), doc.sourcePath);
                auto svgIt = doc.svgTextures.find(id);
                if (svgIt != doc.svgTextures.end() && !svgIt->second.src.empty())
                    svgIt->second.src = rebaseRelativePath(
                        svgIt->second.src, includePath.parent_path(), doc.sourcePath);
            }
    }
    if (const XMLElement* mats = root->FirstChildElement("materials")) {
        for (const XMLElement* c = mats->FirstChildElement("material"); c;
             c = c->NextSiblingElement("material"))
            if (const char* id = c->Attribute("id"))
                if (doc.materials.count(id))
                    std::cerr << "Warning: <include file=\"" << includePath.string()
                              << "\"> material id '" << id
                              << "' collides with an already-loaded material; last-write-wins.\n";
        parseMaterials(mats, doc);
        for (const XMLElement* c = mats->FirstChildElement("material"); c;
             c = c->NextSiblingElement("material"))
            if (const char* id = c->Attribute("id"))
                doc.includedMaterials.insert(id);
    }
    if (const XMLElement* defs = root->FirstChildElement("definitions")) {
        for (const XMLElement* c = defs->FirstChildElement("definition"); c;
             c = c->NextSiblingElement("definition"))
            if (const char* id = c->Attribute("id"))
                if (doc.definitions.count(id))
                    std::cerr << "Warning: <include file=\"" << includePath.string()
                              << "\"> definition id '" << id
                              << "' collides with an already-loaded definition; last-write-wins.\n";
        parseDefinitions(defs, doc);
        for (const XMLElement* c = defs->FirstChildElement("definition"); c;
             c = c->NextSiblingElement("definition"))
            if (const char* id = c->Attribute("id")) {
                doc.includedDefs.insert(id);
                // STAB-0550: rebase any OBJ meshSource inside this included
                // definition the same way, for the same reason.
                auto defIt = doc.definitions.find(id);
                if (defIt != doc.definitions.end() && defIt->second)
                    rebaseDefinitionMeshSources(*defIt->second, includePath.parent_path(),
                                                 doc.sourcePath);
            }
    }
    // STAB-0092: <embeds> was previously never merged from an included file
    // at all (only parsed from the main document's own top-level <embeds>),
    // so a <definition> merged from an include whose meshSource referenced
    // "embed:<id>" declared in that same included file would silently fail
    // to resolve. Mirrors the definitions-merge pattern above.
    if (const XMLElement* embs = root->FirstChildElement("embeds")) {
        for (const XMLElement* c = embs->FirstChildElement("embed"); c;
             c = c->NextSiblingElement("embed"))
            if (const char* id = c->Attribute("id"))
                if (doc.embeds.count(id))
                    std::cerr << "Warning: <include file=\"" << includePath.string()
                              << "\"> embed id '" << id
                              << "' collides with an already-loaded embed; last-write-wins.\n";
        parseEmbeds(embs, doc);
        for (const XMLElement* c = embs->FirstChildElement("embed"); c;
             c = c->NextSiblingElement("embed"))
            if (const char* id = c->Attribute("id")) {
                doc.includedEmbeds.insert(id);
                // STAB-0550-style rebase: an external embed's src is a path
                // relative to the included file's own directory.
                auto embIt = doc.embeds.find(id);
                if (embIt != doc.embeds.end() && !embIt->second.src.empty())
                    embIt->second.src = rebaseRelativePath(
                        embIt->second.src, includePath.parent_path(), doc.sourcePath);
            }
    }

    inProgress.erase(canonical);
    processed.insert(canonical);
}

static void processIncludes(const XMLElement* root, Mc3Document& doc,
                             const std::filesystem::path& selfPath,
                             std::set<std::filesystem::path>& inProgress,
                             std::set<std::filesystem::path>& processed,
                             bool recordIncludes,
                             const Mc3LoadPolicy& policy,
                             int depth)
{
    if (depth > policy.maxIncludeDepth)
        throw std::runtime_error(
            "<include> nesting exceeds the policy limit (" +
            std::to_string(policy.maxIncludeDepth) + ")");

    for (const XMLElement* inc = root->FirstChildElement("include"); inc;
         inc = inc->NextSiblingElement("include")) {
        const char* fileAttr = inc->Attribute("file");
        if (!fileAttr || !fileAttr[0]) continue;

        // Resolve relative to the file that contains the <include>
        std::filesystem::path includePath = selfPath.parent_path() / fileAttr;

        // Confinement: reject an include that escapes the document root
        // (absolute path or `..` traversal) when the policy demands it.
        if (policy.confineIncludesToRoot &&
            !includePathWithinRoot(includePath, doc.sourcePath)) {
            throw std::runtime_error(
                "<include file=\"" + std::string(fileAttr) +
                "\"> escapes the document root (rejected by load policy)");
        }

        // Only record at the top level (not when called recursively from mergeInclude)
        if (recordIncludes)
            doc.includes.push_back(fileAttr);

        mergeInclude(includePath, doc, inProgress, processed, policy, depth);
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

// Shared builder: everything after the XML is loaded (from a file or a string)
// and the <mc3> root is located. `selfPath` is the notional path of the document
// being parsed; its parent_path() is the base for relative includes/resources.
static Mc3Document buildDocumentFromRoot(const XMLElement* root,
                                         const std::filesystem::path& selfPath,
                                         const Mc3LoadPolicy& policy) {
    // AUD-059: reset the document-wide budget once per top-level parse (NOT
    // once per <include> -- mergeInclude() parses included files' definitions/
    // materials/textures directly via parseDefinitions() etc., without calling
    // back into buildDocumentFromRoot(), so an included file's objects/
    // primitives correctly accumulate against this same top-level budget
    // rather than resetting it and escaping the cap).
    g_budget.reset();

    // AUD-006b: set the resource-confinement state for this parse before any
    // texture/SVG/mesh/embed/sound/music field is read.
    g_confineResourcePaths = policy.confineResourcePathsToRoot;
    g_resourceRoot = selfPath.parent_path();

    Mc3Document doc;
    doc.sourcePath       = selfPath.parent_path();
    doc.version          = attr(root, "version", "0.3");
    doc.model            = attr(root, "model",   "unnamed");
    doc.unit             = attr(root, "unit",    "meter");
    doc.coordinateSystem = attr(root, "coordinate_system", "right_handed_y_up");
    doc.rotationUnits    = attr(root, "rotation_units",    "degrees");
    doc.eulerOrder       = attr(root, "euler_order",       "XYZ");
    // STAB-0653: root-level default_camera is an alternate spelling of
    // <cameras default="...">; the latter (parsed in parseCameras(), which
    // runs after this) takes priority if both are present.
    doc.defaultCamera    = attr(root, "default_camera");

    if (const XMLElement* meta = root->FirstChildElement("metadata"))
        for (const XMLElement* p = meta->FirstChildElement("property"); p;
             p = p->NextSiblingElement("property"))
            if (const char* n = p->Attribute("name"))
                if (const char* v = p->Attribute("value"))
                    doc.metadata[n] = v;

    if (const XMLElement* metaEl = root->FirstChildElement("meta"))
        for (const XMLElement* e = metaEl->FirstChildElement("metaentry"); e;
             e = e->NextSiblingElement("metaentry"))
            if (const char* k = e->Attribute("key"))
                if (const char* v = e->Attribute("value"))
                    doc.meta[k] = v;

    // Process <include> elements before any local sections so that included
    // definitions/materials/textures are available when the main file is parsed.
    // Untrusted content (AI output, imports) parses with allowIncludes=false so
    // it cannot open and merge arbitrary local files.
    if (policy.allowIncludes) {
        std::set<std::filesystem::path> inProgress, processed;
        try {
            inProgress.insert(std::filesystem::weakly_canonical(selfPath));
        } catch (...) {
            inProgress.insert(std::filesystem::absolute(selfPath));
        }
        processIncludes(root, doc, selfPath, inProgress, processed,
                        /*recordIncludes=*/true, policy, /*depth=*/0);
    } else if (root->FirstChildElement("include")) {
        std::cerr << "Note: <include> ignored (parsing under a no-include policy, "
                     "e.g. untrusted/AI content).\n";
    }

    if (const XMLElement* env  = root->FirstChildElement("environment"))  parseEnvironment(env,  doc);
    if (const XMLElement* lts  = root->FirstChildElement("lights"))       parseLights(lts,       doc);
    if (const XMLElement* cams = root->FirstChildElement("cameras"))      parseCameras(cams,     doc);

    if (const XMLElement* txs  = root->FirstChildElement("textures")) {
        parseTextures(txs, doc);
        // Task 1: erase local IDs so writer does not skip them as "included"
        for (const XMLElement* c = txs->FirstChildElement("texture"); c;
             c = c->NextSiblingElement("texture"))
            if (const char* id = c->Attribute("id")) doc.includedTextures.erase(id);
    }
    if (const XMLElement* mats = root->FirstChildElement("materials")) {
        parseMaterials(mats, doc);
        for (const XMLElement* c = mats->FirstChildElement("material"); c;
             c = c->NextSiblingElement("material"))
            if (const char* id = c->Attribute("id")) doc.includedMaterials.erase(id);
    }
    if (const XMLElement* defs = root->FirstChildElement("definitions")) {
        parseDefinitions(defs, doc);
        for (const XMLElement* c = defs->FirstChildElement("definition"); c;
             c = c->NextSiblingElement("definition"))
            if (const char* id = c->Attribute("id")) doc.includedDefs.erase(id);
    }
    if (const XMLElement* embs = root->FirstChildElement("embeds")) {
        parseEmbeds(embs, doc);
        for (const XMLElement* c = embs->FirstChildElement("embed"); c;
             c = c->NextSiblingElement("embed"))
            if (const char* id = c->Attribute("id")) doc.includedEmbeds.erase(id);
    }
    if (const XMLElement* scrs = root->FirstChildElement("scripts"))      parseScripts(scrs,     doc);
    if (const XMLElement* snds = root->FirstChildElement("sounds"))       parseSounds(snds,      doc);
    if (const XMLElement* mus  = root->FirstChildElement("music"))        parseMusic(mus,        doc);
    if (const XMLElement* trgs = root->FirstChildElement("triggers"))     parseTriggers(trgs,    doc);
    if (const XMLElement* sts  = root->FirstChildElement("states"))       parseStates(sts,       doc);
    if (const XMLElement* objs = root->FirstChildElement("objects"))      parseObjects(objs,     doc);
    if (const XMLElement* acts = root->FirstChildElement("actions"))      parseActions(acts,     doc);

    return doc;
}

Mc3Document Mc3XmlParser::parse(const std::filesystem::path& path,
                                const Mc3LoadPolicy& policy) {
    XMLDocument xml;
    if (xml.LoadFile(path.string().c_str()) != XML_SUCCESS)
        throw std::runtime_error("Failed to load XML: " + path.string() + ": " + xml.ErrorStr());
    const XMLElement* root = xml.FirstChildElement("mc3");
    if (!root)
        throw std::runtime_error("Root element <mc3> not found in " + path.string());
    return buildDocumentFromRoot(root, path, policy);
}

Mc3Document Mc3XmlParser::parseString(const std::string& xmlText,
                                      const std::filesystem::path& sourceDir,
                                      const Mc3LoadPolicy& policy) {
    XMLDocument xml;
    if (xml.Parse(xmlText.c_str(), xmlText.size()) != XML_SUCCESS)
        throw std::runtime_error(std::string("Failed to parse XML: ") + xml.ErrorStr());
    const XMLElement* root = xml.FirstChildElement("mc3");
    if (!root)
        throw std::runtime_error("Root element <mc3> not found in in-memory document");
    // Synthetic self-path so relative includes/resources resolve against sourceDir.
    return buildDocumentFromRoot(root, sourceDir / "in-memory.mc3.xml", policy);
}
