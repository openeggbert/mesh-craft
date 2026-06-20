#include "Mc3XmlWriter.hpp"
#include <MeshCraft/Mc3/Mc3Animation.hpp>
#include <MeshCraft/Mc3/Mc3Extrude.hpp>
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
    if (!obj.layer.empty())     el->SetAttribute("layer",     obj.layer.c_str());
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
    case ObjectType::Torus:        tag = "torus";       break;
    case ObjectType::Capsule:      tag = "capsule";     break;
    case ObjectType::Disk:         tag = "disk";        break;
    case ObjectType::Grid:         tag = "grid";        break;
    case ObjectType::IcoSphere:    tag = "icosphere";   break;
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

    if (obj->deform) {
        XMLElement* de = xmlDoc.NewElement("deform");
        de->SetAttribute("scale", vec3Str(obj->deform->scale).c_str());
        el->InsertEndChild(de);
    }

    if (obj->uvMapping) {
        const auto& m = *obj->uvMapping;
        XMLElement* uve = xmlDoc.NewElement("uv_mapping");
        const char* projStr = "planar";
        if (m.projection == UvProjection::Box)    projStr = "box";
        if (m.projection == UvProjection::Sphere) projStr = "sphere";
        uve->SetAttribute("projection", projStr);
        if (m.scaleU   != 1.0f) uve->SetAttribute("scale_u",  m.scaleU);
        if (m.scaleV   != 1.0f) uve->SetAttribute("scale_v",  m.scaleV);
        if (m.offsetU  != 0.0f) uve->SetAttribute("offset_u", m.offsetU);
        if (m.offsetV  != 0.0f) uve->SetAttribute("offset_v", m.offsetV);
        if (m.rotation != 0.0f) uve->SetAttribute("rotation", m.rotation);
        el->InsertEndChild(uve);
    }

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
        case ObjectType::Plane: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.6g %.6g", p.size[0], p.size[2]);
            el->SetAttribute("size", buf);
            if (p.axis != "y") el->SetAttribute("axis", p.axis.c_str());
            break;
        }
        case ObjectType::Torus:
            if (p.majorRadius != 0.35f) el->SetAttribute("major_radius", fStr(p.majorRadius).c_str());
            if (p.minorRadius != 0.15f) el->SetAttribute("minor_radius", fStr(p.minorRadius).c_str());
            if (p.segments != 32)       el->SetAttribute("segments", p.segments);
            break;
        case ObjectType::Grid:
            el->SetAttribute("size", vec3Str(p.size).c_str());
            if (p.subdivisionsX != 4) el->SetAttribute("subdivisions_x", p.subdivisionsX);
            if (p.subdivisionsZ != 4) el->SetAttribute("subdivisions_z", p.subdivisionsZ);
            break;
        case ObjectType::IcoSphere:
            if (p.radius != 0.5f) el->SetAttribute("radius", fStr(p.radius).c_str());
            break;
        default: break;
        }
    }

    if (obj->extrude) {
        const auto& ex = *obj->extrude;
        if (ex.twist    != 0.0f) el->SetAttribute("twist",    fStr(ex.twist).c_str());
        if (ex.segments != 32)   el->SetAttribute("segments", ex.segments);
        if (!ex.smooth)          el->SetAttribute("smooth",   "false");
        if (!ex.caps)            el->SetAttribute("caps",     "false");

        // Cross-section
        const auto& cs = ex.crossSection;
        XMLElement* csEl = xmlDoc.NewElement("cross_section");
        const char* csTypeStr = "rect";
        switch (cs.type) {
        case CrossSectionType::Circle:  csTypeStr = "circle";  break;
        case CrossSectionType::Polygon: csTypeStr = "polygon"; break;
        case CrossSectionType::Custom:  csTypeStr = "custom";  break;
        case CrossSectionType::Star:    csTypeStr = "star";    break;
        default: break;
        }
        csEl->SetAttribute("type", csTypeStr);
        switch (cs.type) {
        case CrossSectionType::Rect:
            if (cs.width  != 0.3f) csEl->SetAttribute("width",  fStr(cs.width).c_str());
            if (cs.height != 0.3f) csEl->SetAttribute("height", fStr(cs.height).c_str());
            break;
        case CrossSectionType::Circle:
            if (cs.radius      != 0.1f) csEl->SetAttribute("radius",       fStr(cs.radius).c_str());
            if (cs.innerRadius != 0.0f) csEl->SetAttribute("inner_radius", fStr(cs.innerRadius).c_str());
            if (cs.segments    != 32)   csEl->SetAttribute("segments",     cs.segments);
            break;
        case CrossSectionType::Polygon:
            if (cs.radius      != 0.1f) csEl->SetAttribute("radius",       fStr(cs.radius).c_str());
            if (cs.innerRadius != 0.0f) csEl->SetAttribute("inner_radius", fStr(cs.innerRadius).c_str());
            if (cs.sides       != 6)    csEl->SetAttribute("sides",        cs.sides);
            break;
        case CrossSectionType::Star:
            if (cs.radius      != 0.1f) csEl->SetAttribute("radius",       fStr(cs.radius).c_str());
            if (cs.innerRadius != 0.0f) csEl->SetAttribute("inner_radius", fStr(cs.innerRadius).c_str());
            if (cs.sides       != 6)    csEl->SetAttribute("sides",        cs.sides);
            break;
        case CrossSectionType::Custom:
            for (const auto& pt : cs.customPoints) {
                XMLElement* pe = xmlDoc.NewElement("point");
                pe->SetAttribute("x", fStr(pt.x).c_str());
                pe->SetAttribute("y", fStr(pt.y).c_str());
                csEl->InsertEndChild(pe);
            }
            break;
        }
        el->InsertEndChild(csEl);

        // Path
        const auto& path = ex.path;
        XMLElement* pathEl = xmlDoc.NewElement("path");
        const char* pathTypeStr = "line";
        switch (path.type) {
        case ExtrudePathType::Arc:      pathTypeStr = "arc";      break;
        case ExtrudePathType::Helix:    pathTypeStr = "helix";    break;
        case ExtrudePathType::Polyline: pathTypeStr = "polyline"; break;
        case ExtrudePathType::Bezier:   pathTypeStr = "bezier";   break;
        default: break;
        }
        pathEl->SetAttribute("type", pathTypeStr);
        switch (path.type) {
        case ExtrudePathType::Line:
            if (path.length != 1.0f) pathEl->SetAttribute("length", fStr(path.length).c_str());
            if (path.axis   != "y")  pathEl->SetAttribute("axis",   path.axis.c_str());
            break;
        case ExtrudePathType::Arc:
            pathEl->SetAttribute("radius", fStr(path.arcRadius).c_str());
            pathEl->SetAttribute("angle",  fStr(path.arcAngle).c_str());
            break;
        case ExtrudePathType::Helix:
            pathEl->SetAttribute("radius", fStr(path.helixRadius).c_str());
            pathEl->SetAttribute("height", fStr(path.helixHeight).c_str());
            pathEl->SetAttribute("turns",  fStr(path.helixTurns).c_str());
            break;
        case ExtrudePathType::Polyline:
        case ExtrudePathType::Bezier:
            for (const auto& pt : path.points) {
                XMLElement* pe = xmlDoc.NewElement("point");
                pe->SetAttribute("x",  fStr(pt.position[0]).c_str());
                pe->SetAttribute("y",  fStr(pt.position[1]).c_str());
                pe->SetAttribute("z",  fStr(pt.position[2]).c_str());
                if (path.type == ExtrudePathType::Bezier) {
                    pe->SetAttribute("cx", fStr(pt.controlIn[0]).c_str());
                    pe->SetAttribute("cy", fStr(pt.controlIn[1]).c_str());
                    pe->SetAttribute("cz", fStr(pt.controlIn[2]).c_str());
                }
                pathEl->InsertEndChild(pe);
            }
            break;
        }
        el->InsertEndChild(pathEl);
    }

    if (obj->type == ObjectType::Mesh && !obj->meshSource.empty())
        el->SetAttribute("src", obj->meshSource.c_str());

    if (obj->type == ObjectType::Instance) {
        el->SetAttribute("definition", obj->definition.c_str());
        if (!obj->materialOverride.empty())
            el->SetAttribute("material_override", obj->materialOverride.c_str());
        if (!obj->variantDefinitions.empty()) {
            std::string varStr;
            for (const auto& v : obj->variantDefinitions) {
                if (!varStr.empty()) varStr += ' ';
                varStr += v;
            }
            el->SetAttribute("variants", varStr.c_str());
        }
    }

    // Named states
    for (const auto& [stateId, st] : obj->states) {
        XMLElement* se = xmlDoc.NewElement("state");
        se->SetAttribute("id", stateId.c_str());
        if (st.position) se->SetAttribute("position", vec3Str(*st.position).c_str());
        if (st.rotation) se->SetAttribute("rotation", vec3Str(*st.rotation).c_str());
        if (st.scale)    se->SetAttribute("scale",    vec3Str(*st.scale).c_str());
        if (st.visible.has_value() && !*st.visible) se->SetAttribute("visible", "false");
        if (st.material) se->SetAttribute("material", st.material->c_str());
        el->InsertEndChild(se);
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

    // Textures
    if (!doc.textures.empty()) {
        XMLElement* tEl = xml.NewElement("textures");
        for (const auto& [id, tex] : doc.textures) {
            XMLElement* te = xml.NewElement("texture");
            te->SetAttribute("id",  id.c_str());
            if (!tex.name.empty() && tex.name != id)
                te->SetAttribute("name", tex.name.c_str());
            if (!tex.uri.empty())
                te->SetAttribute("uri", tex.uri.c_str());
            if (tex.wrapU != "repeat")
                te->SetAttribute("wrap_u", tex.wrapU.c_str());
            if (tex.wrapV != "repeat")
                te->SetAttribute("wrap_v", tex.wrapV.c_str());
            if (tex.filter != "linear")
                te->SetAttribute("filter", tex.filter.c_str());
            if (tex.colorSpace != "srgb")
                te->SetAttribute("color_space", tex.colorSpace.c_str());
            tEl->InsertEndChild(te);
        }
        root->InsertEndChild(tEl);
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
            {
                auto add = [&](const char* tag, const std::string& val) {
                    if (val.empty()) return;
                    XMLElement* t = xml.NewElement(tag); t->SetText(val.c_str()); me->InsertEndChild(t);
                };
                add("base_color_texture",        mat.baseColorTexture);
                add("normal_texture",            mat.normalTexture);
                add("metallic_roughness_texture",mat.metallicRoughnessTexture);
                add("occlusion_texture",         mat.occlusionTexture);
                add("emissive_texture",          mat.emissiveTexture);
                if (mat.emissiveColor[0] != 0 || mat.emissiveColor[1] != 0 || mat.emissiveColor[2] != 0)
                    add("emissive_color", vec3Str(mat.emissiveColor));
                if (mat.alphaCutoff != 0.5f) me->SetAttribute("alpha_cutoff", fStr(mat.alphaCutoff).c_str());
                if (mat.normalScale != 1.0f) me->SetAttribute("normal_scale", fStr(mat.normalScale).c_str());
                if (mat.occlusionStrength != 1.0f) me->SetAttribute("occlusion_strength", fStr(mat.occlusionStrength).c_str());
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

    // Actions
    if (!doc.actions.empty()) {
        static const auto interpStr = [](Interpolation i) -> const char* {
            switch (i) {
            case Interpolation::Step:        return "step";
            case Interpolation::CubicBezier: return "cubic";
            default:                         return "linear";
            }
        };

        XMLElement* actsEl = xml.NewElement("actions");
        for (const auto& [name, act] : doc.actions) {
            XMLElement* ae = xml.NewElement("action");
            ae->SetAttribute("name",     act.name.c_str());
            ae->SetAttribute("duration", fStr(act.duration).c_str());
            if (act.loop) ae->SetAttribute("loop", "true");

            for (const auto& ch : act.channels) {
                XMLElement* ce = xml.NewElement("channel");
                ce->SetAttribute("target",   ch.targetObject.c_str());
                ce->SetAttribute("property", animatedPropertyName(ch.property));

                for (const auto& kf : ch.keyframes) {
                    XMLElement* ke = xml.NewElement("keyframe");
                    ke->SetAttribute("time",  fStr(kf.time).c_str());
                    ke->SetAttribute("value", fStr(kf.value).c_str());
                    if (kf.interpolation != Interpolation::Linear)
                        ke->SetAttribute("interp", interpStr(kf.interpolation));
                    if (kf.interpolation == Interpolation::CubicBezier) {
                        XMLElement* hl = xml.NewElement("handle_left");
                        hl->SetAttribute("dt", fStr(kf.handleLeft.dt).c_str());
                        hl->SetAttribute("dv", fStr(kf.handleLeft.dv).c_str());
                        ke->InsertEndChild(hl);
                        XMLElement* hr = xml.NewElement("handle_right");
                        hr->SetAttribute("dt", fStr(kf.handleRight.dt).c_str());
                        hr->SetAttribute("dv", fStr(kf.handleRight.dv).c_str());
                        ke->InsertEndChild(hr);
                    }
                    ce->InsertEndChild(ke);
                }
                ae->InsertEndChild(ce);
            }
            actsEl->InsertEndChild(ae);
        }
        root->InsertEndChild(actsEl);
    }

    if (xml.SaveFile(path.string().c_str()) != XML_SUCCESS)
        throw std::runtime_error("Failed to save XML: " + path.string());
}
