#include "Mc3XmlWriter.hpp"
#include <MeshCraft/Mc3/Mc3Animation.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Extrude.hpp>
#include <MeshCraft/Mc3/Mc3SceneState.hpp>
#include <MeshCraft/Mc3/Mc3Trigger.hpp>
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
    if (!obj.scriptId.empty())  el->SetAttribute("script",    obj.scriptId.c_str());
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

    // SYS-W1-06: Mc3Object::children has no built-in cycle protection (only
    // XML parsing structurally can't produce one -- a document built/
    // mutated via the C++ API could). 256 matches mc3togltf/src/
    // GltfExporter.cpp's kMaxNodeDepth precedent.
    static thread_local int depth = 0;
    struct DepthGuard {
        DepthGuard() {
            if (++depth > 256) {
                --depth;
                throw std::runtime_error(
                    "Mc3XmlWriter::writeObject: object nesting exceeds 256 levels "
                    "(cyclic Mc3Object::children graph?)");
            }
        }
        ~DepthGuard() { --depth; }
        DepthGuard(const DepthGuard&) = delete;
    } depthGuard;

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

    // STAB-0656: mc3.xsd only declares <uv_mapping> for primitive/mesh/
    // extrude complexTypes, not for group/union/difference/intersection/
    // instance/area -- writing it for those would produce schema-invalid
    // XML even though obj->uvMapping isn't reachable there via the editor
    // UI today.
    const bool uvMappingAllowedForType =
        obj->type != ObjectType::Group        && obj->type != ObjectType::Union &&
        obj->type != ObjectType::Difference   && obj->type != ObjectType::Intersection &&
        obj->type != ObjectType::Instance     && obj->type != ObjectType::Area;
    if (obj->uvMapping && uvMappingAllowedForType) {
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
            if (p.radius   != 0.5f) el->SetAttribute("radius",   fStr(p.radius).c_str());
            if (p.segments != 2)    el->SetAttribute("segments", p.segments);
            break;
        case ObjectType::Capsule:
            if (p.radius   != 0.5f) el->SetAttribute("radius",   fStr(p.radius).c_str());
            if (p.height   != 1.0f) el->SetAttribute("height",   fStr(p.height).c_str());
            if (p.segments != 32)   el->SetAttribute("segments", p.segments);
            if (p.axis != "y")      el->SetAttribute("axis",     p.axis.c_str());
            break;
        case ObjectType::Disk:
            if (p.radius      != 0.5f) el->SetAttribute("radius",       fStr(p.radius).c_str());
            if (p.minorRadius >  0.0f) el->SetAttribute("inner_radius", fStr(p.minorRadius).c_str());
            if (p.segments    != 32)   el->SetAttribute("segments",     p.segments);
            if (p.axis != "y")         el->SetAttribute("axis",         p.axis.c_str());
            break;
        case ObjectType::Area:
            // STAB-0031: areaType's `size` attribute (mc3.xsd) was declared
            // but never actually written anywhere.
            el->SetAttribute("size", vec3Str(p.size).c_str());
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

    // Object metadata
    if (!obj->metadata.empty()) {
        XMLElement* metaEl = xmlDoc.NewElement("metadata");
        for (const auto& [k, v] : obj->metadata) {
            XMLElement* pe = xmlDoc.NewElement("property");
            pe->SetAttribute("name",  k.c_str());
            pe->SetAttribute("value", v.c_str());
            metaEl->InsertEndChild(pe);
        }
        el->InsertEndChild(metaEl);
    }

    // R111 -- structured asset metadata (mesh_world_revival.md §6).
    if (obj->assetMetadata) {
        const auto& am = *obj->assetMetadata;
        XMLElement* ame = xmlDoc.NewElement("assetMetadata");
        if (!am.category.empty())    ame->SetAttribute("category", am.category.c_str());
        if (!am.subcategory.empty()) ame->SetAttribute("subcategory", am.subcategory.c_str());
        if (!am.facing.empty())      ame->SetAttribute("facing", am.facing.c_str());
        if (!am.collisionProxy.empty()) ame->SetAttribute("collision_proxy", am.collisionProxy.c_str());
        if (!am.shadowPolicy.empty())   ame->SetAttribute("shadow_policy", am.shadowPolicy.c_str());
        if (!am.license.empty())       ame->SetAttribute("license", am.license.c_str());
        if (!am.provenance.empty())    ame->SetAttribute("provenance", am.provenance.c_str());
        if (!am.sourceGeneratorOrHash.empty()) ame->SetAttribute("source", am.sourceGeneratorOrHash.c_str());
        if (!am.semanticVersion.empty())       ame->SetAttribute("version", am.semanticVersion.c_str());
        if (!am.instancingEligible) ame->SetAttribute("instancing_eligible", "false");
        if (am.maxVisibilityDistanceM != 0.f) ame->SetAttribute("max_visibility_distance", am.maxVisibilityDistanceM);
        if (am.selectionWeight != 1.f)        ame->SetAttribute("selection_weight", am.selectionWeight);
        if (am.nominalSize != std::array<float,3>{0.f,0.f,0.f})
            ame->SetAttribute("nominal_size", vec3Str(am.nominalSize).c_str());
        if (am.boundsMin != std::array<float,3>{0.f,0.f,0.f} ||
            am.boundsMax != std::array<float,3>{0.f,0.f,0.f}) {
            ame->SetAttribute("bounds_min", vec3Str(am.boundsMin).c_str());
            ame->SetAttribute("bounds_max", vec3Str(am.boundsMax).c_str());
        }
        if (am.clearanceVolume != std::array<float,3>{0.f,0.f,0.f})
            ame->SetAttribute("clearance_volume", vec3Str(am.clearanceVolume).c_str());

        auto writeTagList = [&](const char* tag, const std::vector<std::string>& tags) {
            if (tags.empty()) return;
            XMLElement* te = xmlDoc.NewElement(tag);
            for (const auto& t : tags) {
                XMLElement* ie = xmlDoc.NewElement("tag");
                ie->SetAttribute("value", t.c_str());
                te->InsertEndChild(ie);
            }
            ame->InsertEndChild(te);
        };
        writeTagList("semanticTags", am.semanticTags);
        writeTagList("styleTags",    am.styleTags);
        writeTagList("regionTags",   am.regionTags);
        writeTagList("periodTags",   am.periodTags);
        writeTagList("materialSlots", am.materialSlots);

        if (!am.sockets.empty()) {
            XMLElement* se = xmlDoc.NewElement("sockets");
            for (const auto& [name, pos] : am.sockets) {
                XMLElement* pe = xmlDoc.NewElement("socket");
                pe->SetAttribute("name", name.c_str());
                pe->SetAttribute("position", vec3Str(pos).c_str());
                se->InsertEndChild(pe);
            }
            ame->InsertEndChild(se);
        }
        if (!am.lods.empty()) {
            XMLElement* le = xmlDoc.NewElement("lods");
            for (const auto& [tier, defId] : am.lods) {
                XMLElement* te = xmlDoc.NewElement("lod");
                te->SetAttribute("tier", tier.c_str());
                te->SetAttribute("definition", defId.c_str());
                le->InsertEndChild(te);
            }
            ame->InsertEndChild(le);
        }

        el->InsertEndChild(ame);
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
    if (doc.coordinateSystem != "right_handed_y_up")
        root->SetAttribute("coordinate_system", doc.coordinateSystem.c_str());
    if (doc.rotationUnits != "degrees")
        root->SetAttribute("rotation_units", doc.rotationUnits.c_str());
    if (doc.eulerOrder != "XYZ")
        root->SetAttribute("euler_order", doc.eulerOrder.c_str());
    xml.InsertEndChild(root);

    // R110 -- library identity (.mc3lib.xml only; absent on ordinary
    // scene/model documents).
    if (doc.library) {
        XMLElement* libEl = xml.NewElement("library");
        libEl->SetAttribute("namespace", doc.library->libraryNamespace.c_str());
        libEl->SetAttribute("version",   doc.library->version.c_str());
        if (!doc.library->contentHash.empty())
            libEl->SetAttribute("hash", doc.library->contentHash.c_str());
        root->InsertEndChild(libEl);
    }

    // R101 -- library imports (see Mc3Import's own doc comment).
    if (!doc.imports.empty()) {
        XMLElement* importsEl = xml.NewElement("imports");
        for (const auto& imp : doc.imports) {
            XMLElement* impEl = xml.NewElement("import");
            impEl->SetAttribute("namespace", imp.importNamespace.c_str());
            impEl->SetAttribute("source",    imp.source.c_str());
            if (!imp.hash.empty()) impEl->SetAttribute("hash", imp.hash.c_str());
            importsEl->InsertEndChild(impEl);
        }
        root->InsertEndChild(importsEl);
    }

    // Include references (written before all other sections so they appear at
    // the top and can be processed first on the next load).
    for (const auto& inc : doc.includes) {
        XMLElement* incEl = xml.NewElement("include");
        incEl->SetAttribute("file", inc.c_str());
        root->InsertEndChild(incEl);
    }

    // Root metadata (legacy <metadata><property name="…" value="…"/>)
    if (!doc.metadata.empty()) {
        XMLElement* metaEl = xml.NewElement("metadata");
        for (const auto& [k, v] : doc.metadata) {
            XMLElement* pe = xml.NewElement("property");
            pe->SetAttribute("name",  k.c_str());
            pe->SetAttribute("value", v.c_str());
            metaEl->InsertEndChild(pe);
        }
        root->InsertEndChild(metaEl);
    }

    // New-style meta (<meta><metaentry key="…" value="…"/>)
    if (!doc.meta.empty()) {
        XMLElement* mEl = xml.NewElement("meta");
        for (const auto& [k, v] : doc.meta) {
            XMLElement* ee = xml.NewElement("metaentry");
            ee->SetAttribute("key",   k.c_str());
            ee->SetAttribute("value", v.c_str());
            mEl->InsertEndChild(ee);
        }
        root->InsertEndChild(mEl);
    }

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
        if (!env.skyboxTexture.empty()) {
            XMLElement* st = xml.NewElement("skybox_texture");
            st->SetText(env.skyboxTexture.c_str());
            eEl->InsertEndChild(st);
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
            if ((l.type == LightType::Spot || l.type == LightType::Point) && l.range != 0.f)
                le->SetAttribute("range", fStr(l.range).c_str());
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
            if (cam.type == CameraType::Orthographic) {
                ce->SetAttribute("size", fStr(cam.orthoSize).c_str());
                if (cam.orthoAspect != 1.0f)   // STAB-0695: only write non-default
                    ce->SetAttribute("aspect", fStr(cam.orthoAspect).c_str());
            }
            ce->SetAttribute("near",     fStr(cam.nearPlane).c_str());
            ce->SetAttribute("far",      fStr(cam.farPlane).c_str());
            if (cam.rotation)
                ce->SetAttribute("rotation", vec3Str(*cam.rotation).c_str());
            cEl->InsertEndChild(ce);
        }
        root->InsertEndChild(cEl);
    }

    // Textures (bitmap + SVG) — skip entries that came from <include> files
    if (!doc.textures.empty() || !doc.svgTextures.empty()) {
        XMLElement* tEl = xml.NewElement("textures");
        for (const auto& [id, tex] : doc.textures) {
            if (doc.includedTextures.count(id)) continue;
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
            if (!tex.mipMaps)
                te->SetAttribute("mip_maps", false);
            tEl->InsertEndChild(te);
        }
        for (const auto& [id, svg] : doc.svgTextures) {
            // STAB-0091: unlike the doc.textures loop above, this one was
            // missing the includedTextures skip check — an SVG texture
            // merged from an <include> file (mergeInclude() tracks it in
            // the same includedTextures set, since <texture type="svg">
            // shares the id namespace with regular textures) was silently
            // re-inlined into the main file on every save instead of
            // staying only in the included file.
            if (doc.includedTextures.count(id)) continue;
            XMLElement* te = xml.NewElement("texture");
            te->SetAttribute("id",   id.c_str());
            te->SetAttribute("type", "svg");
            if (!svg.src.empty()) {
                te->SetAttribute("src", svg.src.c_str());
            } else if (!svg.inlineContent.empty()) {
                XMLText* t = xml.NewText(svg.inlineContent.c_str());
                t->SetCData(true);
                te->InsertEndChild(t);
            }
            tEl->InsertEndChild(te);
        }
        root->InsertEndChild(tEl);
    }

    // Materials — skip entries that came from <include> files
    if (!doc.materials.empty()) {
        XMLElement* mEl = xml.NewElement("materials");
        for (const auto& [id, mat] : doc.materials) {
            if (doc.includedMaterials.count(id)) continue;
            XMLElement* me = xml.NewElement("material");
            me->SetAttribute("id",        id.c_str());
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

    // Embedded GLTF assets — skip entries that came from <include> files
    if (!doc.embeds.empty()) {
        XMLElement* eEl = xml.NewElement("embeds");
        for (const auto& [id, em] : doc.embeds) {
            if (doc.includedEmbeds.count(id)) continue;
            XMLElement* ee = xml.NewElement("embed");
            ee->SetAttribute("type", "gltf");
            ee->SetAttribute("id",   id.c_str());
            if (!em.src.empty()) {
                ee->SetAttribute("src", em.src.c_str());
            } else if (!em.base64Content.empty()) {
                XMLText* t = xml.NewText(em.base64Content.c_str());
                t->SetCData(true);
                ee->InsertEndChild(t);
            }
            eEl->InsertEndChild(ee);
        }
        root->InsertEndChild(eEl);
    }

    // Scripts
    if (!doc.scripts.empty()) {
        XMLElement* sEl = xml.NewElement("scripts");
        for (const auto& [id, sc] : doc.scripts) {
            XMLElement* se = xml.NewElement("script");
            se->SetAttribute("id",   id.c_str());
            se->SetAttribute("type", sc.type.c_str());
            if (!sc.source.empty()) {
                XMLText* t = xml.NewText(sc.source.c_str());
                t->SetCData(true);
                se->InsertEndChild(t);
            }
            sEl->InsertEndChild(se);
        }
        root->InsertEndChild(sEl);
    }

    // Sounds
    if (!doc.sounds.empty()) {
        XMLElement* snEl = xml.NewElement("sounds");
        for (const auto& [id, snd] : doc.sounds) {
            XMLElement* se = xml.NewElement("sound");
            se->SetAttribute("id",  id.c_str());
            se->SetAttribute("src", snd.src.c_str());
            if (snd.loop) se->SetAttribute("loop", "true");
            snEl->InsertEndChild(se);
        }
        root->InsertEndChild(snEl);
    }

    // Music tracks
    if (!doc.musicTracks.empty()) {
        XMLElement* mEl = xml.NewElement("music");
        for (const auto& [id, mus] : doc.musicTracks) {
            XMLElement* te = xml.NewElement("track");
            te->SetAttribute("id",  id.c_str());
            te->SetAttribute("src", mus.src.c_str());
            if (!mus.loop) te->SetAttribute("loop", "false");
            mEl->InsertEndChild(te);
        }
        root->InsertEndChild(mEl);
    }

    // Triggers
    if (!doc.triggers.empty()) {
        XMLElement* trEl = xml.NewElement("triggers");
        for (const auto& [id, trig] : doc.triggers) {
            XMLElement* te = xml.NewElement("trigger");
            te->SetAttribute("id", id.c_str());
            for (const auto& step : trig.steps) {
                const char* tag = nullptr;
                switch (step.type) {
                case TriggerStepType::PlayAction: tag = "play-action"; break;
                case TriggerStepType::PlaySound:  tag = "play-sound";  break;
                case TriggerStepType::RunScript:  tag = "run-script";  break;
                case TriggerStepType::PlayMusic:  tag = "play-music";  break;
                }
                if (!tag) continue;
                XMLElement* se = xml.NewElement(tag);
                se->SetAttribute("ref", step.ref.c_str());
                te->InsertEndChild(se);
            }
            trEl->InsertEndChild(te);
        }
        root->InsertEndChild(trEl);
    }

    // Scene states
    if (!doc.sceneStates.empty()) {
        XMLElement* stEl = xml.NewElement("states");
        for (const auto& [name, state] : doc.sceneStates) {
            XMLElement* se = xml.NewElement("state");
            se->SetAttribute("name", name.c_str());
            for (const auto& ovr : state.overrides) {
                XMLElement* oe = xml.NewElement("object-override");
                oe->SetAttribute("id", ovr.id.c_str());
                if (ovr.visible.has_value())  oe->SetAttribute("visible",  *ovr.visible ? "true" : "false");
                if (ovr.position.has_value()) oe->SetAttribute("position", vec3Str(*ovr.position).c_str());
                if (ovr.rotation.has_value()) oe->SetAttribute("rotation", vec3Str(*ovr.rotation).c_str());
                if (ovr.material.has_value()) oe->SetAttribute("material", ovr.material->c_str());
                se->InsertEndChild(oe);
            }
            stEl->InsertEndChild(se);
        }
        root->InsertEndChild(stEl);
    }

    // Definitions — skip entries that came from <include> files
    if (!doc.definitions.empty()) {
        XMLElement* dEl = xml.NewElement("definitions");
        for (const auto& [id, obj] : doc.definitions) {
            if (doc.includedDefs.count(id)) continue;
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
            if (act.loop)     ae->SetAttribute("loop",     "true");
            if (act.autoplay) ae->SetAttribute("autoplay", "true");
            if (act.timeScale != 1.0f) // STAB-0460: only write non-default
                ae->SetAttribute("time_scale", fStr(act.timeScale).c_str());

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

    // AUDIT-0019: write to a sibling temp file and rename over the real
    // destination only after a fully successful write, so a crash/disk-full/
    // permission failure mid-write can never leave a truncated or corrupt
    // file at `path` (std::filesystem::rename is atomic within the same
    // filesystem, which a sibling file in the same directory always is).
    std::filesystem::path tmpPath = path;
    tmpPath += ".tmp";
    if (xml.SaveFile(tmpPath.string().c_str()) != XML_SUCCESS) {
        std::error_code ec;
        std::filesystem::remove(tmpPath, ec);
        throw std::runtime_error("Failed to save XML: " + path.string());
    }
    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        throw std::runtime_error("Failed to finalize XML save (rename): " + path.string());
    }
}
