#include "Mc3XmlWriter.hpp"
#include <tinyxml2.h>
#include <stdexcept>
#include <cstdio>

using namespace tinyxml2;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mc3::Internal;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string vec3Str(const std::array<float,3>& v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g %.6g %.6g", v[0], v[1], v[2]);
    return buf;
}

static std::string vec4Str(const std::array<float,4>& v) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%.6g %.6g %.6g %.6g", v[0], v[1], v[2], v[3]);
    return buf;
}

static std::string fStr(float f) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", f);
    return buf;
}

static void setTransform(XMLElement* el, const Mc3Transform& t) {
    auto nonzero3 = [](const std::array<float,3>& v) {
        return v[0] != 0.0f || v[1] != 0.0f || v[2] != 0.0f;
    };
    auto notone3 = [](const std::array<float,3>& v) {
        return v[0] != 1.0f || v[1] != 1.0f || v[2] != 1.0f;
    };
    if (nonzero3(t.position)) el->SetAttribute("position", vec3Str(t.position).c_str());
    if (nonzero3(t.rotation)) el->SetAttribute("rotation", vec3Str(t.rotation).c_str());
    if (notone3(t.scale))     el->SetAttribute("scale",    vec3Str(t.scale).c_str());
    if (nonzero3(t.pivot))    el->SetAttribute("pivot",    vec3Str(t.pivot).c_str());
}

static void setCommonAttribs(XMLElement* el, const Mc3Object& obj) {
    if (!obj.name.empty())      el->SetAttribute("name",      obj.name.c_str());
    if (!obj.id.empty())        el->SetAttribute("id",        obj.id.c_str());
    if (!obj.material.empty())  el->SetAttribute("material",  obj.material.c_str());
    if (!obj.visible)           el->SetAttribute("visible",   "false");
    if (obj.collision != "none" && !obj.collision.empty())
                                el->SetAttribute("collision", obj.collision.c_str());
    if (obj.isCutter)           el->SetAttribute("role",      "cutter");
    if (!obj.tags.empty()) {
        std::string tags;
        for (const auto& t : obj.tags) { if (!tags.empty()) tags += ' '; tags += t; }
        el->SetAttribute("tags", tags.c_str());
    }
    setTransform(el, obj.transform);
}

// Forward declaration
static XMLElement* writeObject(XMLDocument& doc, const std::shared_ptr<Mc3Object>& obj);

static XMLElement* writeObject(XMLDocument& xmlDoc, const std::shared_ptr<Mc3Object>& obj) {
    if (!obj) return nullptr;

    const char* tag = "group";
    switch (obj->type) {
    case ObjectType::Box:          tag = "box";         break;
    case ObjectType::Cube:         tag = "cube";        break;
    case ObjectType::Sphere:       tag = "sphere";      break;
    case ObjectType::Cylinder:     tag = "cylinder";    break;
    case ObjectType::Cone:         tag = "cone";        break;
    case ObjectType::Plane:        tag = "plane";       break;
    case ObjectType::Mesh:         tag = "mesh";        break;
    case ObjectType::Extrude:      tag = "extrude";     break;
    case ObjectType::Group:        tag = "group";       break;
    case ObjectType::Instance:     tag = "instance";    break;
    case ObjectType::Union:        tag = "union";       break;
    case ObjectType::Difference:   tag = "difference";  break;
    case ObjectType::Intersection: tag = "intersection";break;
    case ObjectType::Area:         tag = "area";        break;
    }

    XMLElement* el = xmlDoc.NewElement(tag);
    setCommonAttribs(el, *obj);

    if (obj->primitive) {
        const auto& p = *obj->primitive;
        switch (obj->type) {
        case ObjectType::Box:
        case ObjectType::Cube:
            if (p.size[0] != 1.f || p.size[1] != 1.f || p.size[2] != 1.f)
                el->SetAttribute("size", vec3Str(p.size).c_str());
            break;
        case ObjectType::Sphere:
            if (p.radius != 0.5f) el->SetAttribute("radius", fStr(p.radius).c_str());
            if (p.segments != 32) el->SetAttribute("segments", p.segments);
            break;
        case ObjectType::Cylinder:
        case ObjectType::Cone:
            if (p.radius != 0.5f) el->SetAttribute("radius", fStr(p.radius).c_str());
            if (p.height != 1.0f) el->SetAttribute("height", fStr(p.height).c_str());
            if (p.segments != 32) el->SetAttribute("segments", p.segments);
            break;
        case ObjectType::Plane:
            el->SetAttribute("size", vec3Str(p.size).c_str());
            if (p.axis != "y") el->SetAttribute("axis", p.axis.c_str());
            break;
        default: break;
        }
    }

    if (obj->type == ObjectType::Mesh && !obj->meshSource.empty())
        el->SetAttribute("src", obj->meshSource.c_str());

    if (obj->type == ObjectType::Instance) {
        el->SetAttribute("def", obj->definition.c_str());
        if (!obj->materialOverride.empty())
            el->SetAttribute("material_override", obj->materialOverride.c_str());
    }

    for (const auto& child : obj->children) {
        XMLElement* ce = writeObject(xmlDoc, child);
        if (ce) el->InsertEndChild(ce);
    }
    return el;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

void Mc3XmlWriter::write(const Mc3Document& doc, const std::filesystem::path& path) {
    XMLDocument xml;
    xml.InsertFirstChild(xml.NewDeclaration());

    XMLElement* root = xml.NewElement("mc3");
    root->SetAttribute("version", doc.version.c_str());
    root->SetAttribute("model",   doc.model.c_str());
    if (doc.unit != "meter")
        root->SetAttribute("unit", doc.unit.c_str());
    xml.InsertEndChild(root);

    // Environment
    if (doc.environment) {
        const auto& env = *doc.environment;
        XMLElement* eEl = xml.NewElement("environment");
        XMLElement* bg = xml.NewElement("background");
        bg->SetAttribute("color", vec3Str(env.backgroundColor).c_str());
        eEl->InsertEndChild(bg);
        if (!env.backgroundTexture.empty()) {
            XMLElement* bt = xml.NewElement("background_texture");
            bt->SetText(env.backgroundTexture.c_str());
            eEl->InsertEndChild(bt);
        }
        if (env.fog) {
            XMLElement* fg = xml.NewElement("fog");
            fg->SetAttribute("mode",    env.fog->mode == FogMode::Exponential ? "exponential" : "linear");
            fg->SetAttribute("color",   vec3Str(env.fog->color).c_str());
            fg->SetAttribute("start",   fStr(env.fog->start).c_str());
            fg->SetAttribute("end",     fStr(env.fog->end).c_str());
            fg->SetAttribute("density", fStr(env.fog->density).c_str());
            eEl->InsertEndChild(fg);
        }
        root->InsertEndChild(eEl);
    }

    // Lights
    if (!doc.lights.empty()) {
        XMLElement* lEl = xml.NewElement("lights");
        for (const auto& l : doc.lights) {
            const char* tag = "directional";
            switch (l.type) {
            case LightType::Ambient:     tag = "ambient";     break;
            case LightType::Directional: tag = "directional"; break;
            case LightType::Spot:        tag = "spot";        break;
            case LightType::Point:       tag = "point";       break;
            }
            XMLElement* le = xml.NewElement(tag);
            if (!l.name.empty()) le->SetAttribute("name", l.name.c_str());
            le->SetAttribute("color",      vec3Str(l.color).c_str());
            le->SetAttribute("brightness", fStr(l.brightness).c_str());
            if (l.type == LightType::Directional || l.type == LightType::Spot)
                le->SetAttribute("direction", vec3Str(l.direction).c_str());
            if (l.type == LightType::Spot || l.type == LightType::Point)
                le->SetAttribute("position", vec3Str(l.position).c_str());
            if (l.type == LightType::Spot) {
                le->SetAttribute("angle",   fStr(l.angle).c_str());
                le->SetAttribute("falloff", fStr(l.falloff).c_str());
            }
            if (l.castShadows) le->SetAttribute("cast_shadows", "true");
            lEl->InsertEndChild(le);
        }
        root->InsertEndChild(lEl);
    }

    // Cameras
    if (!doc.cameras.empty()) {
        XMLElement* cEl = xml.NewElement("cameras");
        if (!doc.defaultCamera.empty())
            cEl->SetAttribute("default", doc.defaultCamera.c_str());
        for (const auto& cam : doc.cameras) {
            XMLElement* ce = xml.NewElement("camera");
            ce->SetAttribute("name",     cam.name.c_str());
            ce->SetAttribute("type",     cam.type == CameraType::Orthographic ? "orthographic" : "perspective");
            ce->SetAttribute("position", vec3Str(cam.position).c_str());
            ce->SetAttribute("target",   vec3Str(cam.target).c_str());
            ce->SetAttribute("fov",      fStr(cam.fov).c_str());
            ce->SetAttribute("near",     fStr(cam.nearPlane).c_str());
            ce->SetAttribute("far",      fStr(cam.farPlane).c_str());
            cEl->InsertEndChild(ce);
        }
        root->InsertEndChild(cEl);
    }

    // Materials
    if (!doc.materials.empty()) {
        XMLElement* mEl = xml.NewElement("materials");
        for (const auto& [id, mat] : doc.materials) {
            XMLElement* me = xml.NewElement("material");
            me->SetAttribute("id",        mat.name.c_str());
            me->SetAttribute("roughness", fStr(mat.roughness).c_str());
            me->SetAttribute("metallic",  fStr(mat.metallic).c_str());
            if (mat.doubleSided)            me->SetAttribute("double_sided", "true");
            if (mat.alphaMode != "opaque")  me->SetAttribute("alpha_mode", mat.alphaMode.c_str());
            XMLElement* bc = xml.NewElement("base_color");
            bc->SetText(vec4Str(mat.baseColor).c_str());
            me->InsertEndChild(bc);
            if (!mat.emissiveTexture.empty()) {
                auto add = [&](const char* tag, const std::string& val) {
                    XMLElement* t = xml.NewElement(tag); t->SetText(val.c_str()); me->InsertEndChild(t);
                };
                if (!mat.baseColorTexture.empty())         add("base_color_texture",          mat.baseColorTexture);
                if (!mat.normalTexture.empty())            add("normal_texture",               mat.normalTexture);
                if (!mat.metallicRoughnessTexture.empty()) add("metallic_roughness_texture",   mat.metallicRoughnessTexture);
                if (!mat.occlusionTexture.empty())         add("occlusion_texture",            mat.occlusionTexture);
                if (!mat.emissiveTexture.empty())          add("emissive_texture",             mat.emissiveTexture);
            }
            mEl->InsertEndChild(me);
        }
        root->InsertEndChild(mEl);
    }

    // Definitions
    if (!doc.definitions.empty()) {
        XMLElement* dEl = xml.NewElement("definitions");
        for (const auto& [id, obj] : doc.definitions) {
            XMLElement* de = xml.NewElement("definition");
            de->SetAttribute("id", id.c_str());
            XMLElement* oe = writeObject(xml, obj);
            if (oe) de->InsertEndChild(oe);
            dEl->InsertEndChild(de);
        }
        root->InsertEndChild(dEl);
    }

    // Objects
    if (!doc.objects.empty()) {
        XMLElement* oEl = xml.NewElement("objects");
        for (const auto& obj : doc.objects) {
            XMLElement* oe = writeObject(xml, obj);
            if (oe) oEl->InsertEndChild(oe);
        }
        root->InsertEndChild(oEl);
    }

    if (xml.SaveFile(path.string().c_str()) != XML_SUCCESS)
        throw std::runtime_error("Failed to save XML: " + path.string());
}
