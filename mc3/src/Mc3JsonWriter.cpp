#include "Mc3JsonWriter.hpp"
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Extrude.hpp>
#include <MeshCraft/Mc3/Mc3SceneState.hpp>
#include <MeshCraft/Mc3/Mc3Trigger.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::ordered_json;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mc3::Internal;

// ---------------------------------------------------------------------------
// Helpers -- everything below produces genuine semantic JSON (arrays for
// vectors, nested objects for structured fields), never a mechanical mirror
// of the XML attribute layout. See mesh_world_revival.md §4.2/§4.3.
// ---------------------------------------------------------------------------

namespace {

json vec3(const std::array<float, 3>& v) { return json::array({v[0], v[1], v[2]}); }
json vec4(const std::array<float, 4>& v) { return json::array({v[0], v[1], v[2], v[3]}); }

json transformJson(const Mc3Transform& t) {
    json j = json::object();
    j["position"] = vec3(t.position);
    j["rotation"] = vec3(t.rotation);
    j["scale"]    = vec3(t.scale);
    j["pivot"]    = vec3(t.pivot);
    return j;
}

const char* objectTypeName(ObjectType t) {
    switch (t) {
    case ObjectType::Box:          return "box";
    case ObjectType::Cube:         return "cube";
    case ObjectType::Sphere:       return "sphere";
    case ObjectType::Cylinder:     return "cylinder";
    case ObjectType::Cone:         return "cone";
    case ObjectType::Plane:        return "plane";
    case ObjectType::Torus:        return "torus";
    case ObjectType::Capsule:      return "capsule";
    case ObjectType::Disk:         return "disk";
    case ObjectType::Grid:         return "grid";
    case ObjectType::IcoSphere:    return "icosphere";
    case ObjectType::Mesh:         return "mesh";
    case ObjectType::Extrude:      return "extrude";
    case ObjectType::Group:        return "group";
    case ObjectType::Instance:     return "instance";
    case ObjectType::Union:        return "union";
    case ObjectType::Difference:   return "difference";
    case ObjectType::Intersection: return "intersection";
    case ObjectType::Area:         return "area";
    }
    return "group";
}

const char* primitiveTypeName(PrimitiveType t) {
    switch (t) {
    case PrimitiveType::Box:       return "box";
    case PrimitiveType::Cube:      return "cube";
    case PrimitiveType::Sphere:    return "sphere";
    case PrimitiveType::Cylinder:  return "cylinder";
    case PrimitiveType::Cone:      return "cone";
    case PrimitiveType::Plane:     return "plane";
    case PrimitiveType::Torus:     return "torus";
    case PrimitiveType::Capsule:   return "capsule";
    case PrimitiveType::Disk:      return "disk";
    case PrimitiveType::Grid:      return "grid";
    case PrimitiveType::IcoSphere: return "icosphere";
    }
    return "box";
}

json primitiveJson(const Mc3Primitive& p) {
    json j = json::object();
    j["primitiveType"]  = primitiveTypeName(p.primitiveType);
    j["size"]          = vec3(p.size);
    j["radius"]        = p.radius;
    j["height"]        = p.height;
    j["segments"]      = p.segments;
    j["axis"]          = p.axis;
    j["majorRadius"]   = p.majorRadius;
    j["minorRadius"]   = p.minorRadius;
    j["subdivisionsX"] = p.subdivisionsX;
    j["subdivisionsZ"] = p.subdivisionsZ;
    return j;
}

json crossSectionJson(const Mc3CrossSection& cs) {
    json j = json::object();
    switch (cs.type) {
    case CrossSectionType::Rect:    j["type"] = "rect";    break;
    case CrossSectionType::Circle:  j["type"] = "circle";  break;
    case CrossSectionType::Polygon: j["type"] = "polygon"; break;
    case CrossSectionType::Custom:  j["type"] = "custom";  break;
    case CrossSectionType::Star:    j["type"] = "star";    break;
    }
    j["width"]       = cs.width;
    j["height"]      = cs.height;
    j["radius"]      = cs.radius;
    j["innerRadius"] = cs.innerRadius;
    j["sides"]       = cs.sides;
    j["segments"]    = cs.segments;
    if (!cs.customPoints.empty()) {
        json pts = json::array();
        for (const auto& pt : cs.customPoints)
            pts.push_back(json::array({pt.x, pt.y}));
        j["customPoints"] = std::move(pts);
    }
    return j;
}

json pathJson(const Mc3ExtrudePath& path) {
    json j = json::object();
    switch (path.type) {
    case ExtrudePathType::Line:     j["type"] = "line";     break;
    case ExtrudePathType::Arc:      j["type"] = "arc";      break;
    case ExtrudePathType::Helix:    j["type"] = "helix";    break;
    case ExtrudePathType::Polyline: j["type"] = "polyline"; break;
    case ExtrudePathType::Bezier:   j["type"] = "bezier";   break;
    }
    j["length"]      = path.length;
    j["axis"]        = path.axis;
    j["arcRadius"]   = path.arcRadius;
    j["arcAngle"]    = path.arcAngle;
    j["helixRadius"] = path.helixRadius;
    j["helixHeight"] = path.helixHeight;
    j["helixTurns"]  = path.helixTurns;
    if (!path.points.empty()) {
        json pts = json::array();
        for (const auto& pt : path.points) {
            json pe = json::object();
            pe["position"]  = vec3(pt.position);
            pe["controlIn"] = vec3(pt.controlIn);
            pts.push_back(std::move(pe));
        }
        j["points"] = std::move(pts);
    }
    return j;
}

json extrudeJson(const Mc3Extrude& ex) {
    json j = json::object();
    j["crossSection"] = crossSectionJson(ex.crossSection);
    j["path"]         = pathJson(ex.path);
    j["twist"]        = ex.twist;
    j["segments"]     = ex.segments;
    j["smooth"]       = ex.smooth;
    j["caps"]         = ex.caps;
    return j;
}

json uvMappingJson(const Mc3UvMapping& m) {
    json j = json::object();
    switch (m.projection) {
    case UvProjection::Planar: j["projection"] = "planar"; break;
    case UvProjection::Box:    j["projection"] = "box";    break;
    case UvProjection::Sphere: j["projection"] = "sphere"; break;
    }
    j["scaleU"]   = m.scaleU;
    j["scaleV"]   = m.scaleV;
    j["offsetU"]  = m.offsetU;
    j["offsetV"]  = m.offsetV;
    j["rotation"] = m.rotation;
    return j;
}

json objectJson(const std::shared_ptr<Mc3Object>& obj) {
    json j = json::object();
    if (!obj) return j;

    j["type"] = objectTypeName(obj->type);
    if (!obj->id.empty())       j["id"] = obj->id;
    if (!obj->name.empty())     j["name"] = obj->name;
    j["transform"] = transformJson(obj->transform);
    if (!obj->material.empty()) j["material"] = obj->material;
    if (!obj->visible)          j["visible"] = false;
    if (obj->collision != "none" && !obj->collision.empty())
                                 j["collision"] = obj->collision;
    if (!obj->layer.empty())    j["layer"] = obj->layer;
    if (!obj->tags.empty())     j["tags"] = obj->tags;
    if (obj->isCutter)          j["role"] = "cutter";

    if (obj->deform)   j["deform"]    = json{{"scale", vec3(obj->deform->scale)}};
    if (obj->uvMapping) j["uvMapping"] = uvMappingJson(*obj->uvMapping);
    if (obj->primitive) j["primitive"] = primitiveJson(*obj->primitive);
    if (obj->extrude)   j["extrude"]   = extrudeJson(*obj->extrude);
    if (obj->csgOperation) {
        const char* csgTypeStr = "union";
        switch (obj->csgOperation->csgType) {
        case CsgType::Union:        csgTypeStr = "union";        break;
        case CsgType::Difference:   csgTypeStr = "difference";   break;
        case CsgType::Intersection: csgTypeStr = "intersection"; break;
        }
        j["csgOperation"] = json{{"csgType", csgTypeStr}};
    }

    if (obj->type == ObjectType::Mesh && !obj->meshSource.empty())
        j["meshSource"] = obj->meshSource;

    if (obj->type == ObjectType::Instance) {
        j["definition"] = obj->definition;
        if (!obj->materialOverride.empty()) j["materialOverride"] = obj->materialOverride;
        if (!obj->variantDefinitions.empty()) j["variants"] = obj->variantDefinitions;
    }

    if (!obj->metadata.empty()) {
        json m = json::object();
        for (const auto& [k, v] : obj->metadata) m[k] = v;
        j["metadata"] = std::move(m);
    }

    if (!obj->states.empty()) {
        json states = json::object();
        for (const auto& [stateId, st] : obj->states) {
            json se = json::object();
            if (st.position) se["position"] = vec3(*st.position);
            if (st.rotation) se["rotation"] = vec3(*st.rotation);
            if (st.scale)    se["scale"]    = vec3(*st.scale);
            if (st.visible)  se["visible"]  = *st.visible;
            if (st.material) se["material"] = *st.material;
            states[stateId] = std::move(se);
        }
        j["states"] = std::move(states);
    }

    if (!obj->children.empty()) {
        json ch = json::array();
        for (const auto& c : obj->children) ch.push_back(objectJson(c));
        j["children"] = std::move(ch);
    }

    return j;
}

const char* interpName(Interpolation i) {
    switch (i) {
    case Interpolation::Step:        return "step";
    case Interpolation::CubicBezier: return "cubic";
    default:                         return "linear";
    }
}

} // namespace

std::string Mc3JsonWriter::toString(const Mc3Document& doc) {
    json j = json::object();
    j["format"]           = "mc3";
    j["version"]          = doc.version;
    j["model"]             = doc.model;
    j["unit"]              = doc.unit;
    j["coordinateSystem"]  = doc.coordinateSystem;
    j["rotationUnits"]     = doc.rotationUnits;
    j["eulerOrder"]        = doc.eulerOrder;

    if (!doc.includes.empty()) j["includes"] = doc.includes;

    if (!doc.metadata.empty()) {
        json m = json::object();
        for (const auto& [k, v] : doc.metadata) m[k] = v;
        j["metadata"] = std::move(m);
    }
    if (!doc.meta.empty()) {
        json m = json::object();
        for (const auto& [k, v] : doc.meta) m[k] = v;
        j["meta"] = std::move(m);
    }

    if (doc.environment) {
        const auto& env = *doc.environment;
        json e = json::object();
        e["backgroundColor"] = vec3(env.backgroundColor);
        if (!env.backgroundTexture.empty()) e["backgroundTexture"] = env.backgroundTexture;
        if (!env.skyboxTexture.empty())     e["skyboxTexture"]     = env.skyboxTexture;
        if (env.fog) {
            json f = json::object();
            f["mode"]    = env.fog->mode == FogMode::Exponential ? "exponential" : "linear";
            f["color"]   = vec3(env.fog->color);
            f["start"]   = env.fog->start;
            f["end"]     = env.fog->end;
            f["density"] = env.fog->density;
            e["fog"] = std::move(f);
        }
        j["environment"] = std::move(e);
    }

    if (!doc.lights.empty()) {
        json arr = json::array();
        for (const auto& l : doc.lights) {
            json le = json::object();
            switch (l.type) {
            case LightType::Ambient:     le["type"] = "ambient";     break;
            case LightType::Directional: le["type"] = "directional"; break;
            case LightType::Spot:        le["type"] = "spot";        break;
            case LightType::Point:       le["type"] = "point";       break;
            }
            if (!l.name.empty()) le["name"] = l.name;
            le["color"]      = vec3(l.color);
            le["brightness"] = l.brightness;
            if (l.type == LightType::Directional || l.type == LightType::Spot)
                le["direction"] = vec3(l.direction);
            if (l.type == LightType::Spot || l.type == LightType::Point)
                le["position"] = vec3(l.position);
            if (l.type == LightType::Spot) {
                le["angle"]   = l.angle;
                le["falloff"] = l.falloff;
            }
            le["range"] = l.range;
            if (l.castShadows) le["castShadows"] = true;
            arr.push_back(std::move(le));
        }
        j["lights"] = std::move(arr);
    }

    if (!doc.cameras.empty()) {
        json c = json::object();
        if (!doc.defaultCamera.empty()) c["default"] = doc.defaultCamera;
        json list = json::array();
        for (const auto& cam : doc.cameras) {
            json ce = json::object();
            ce["name"]     = cam.name;
            ce["type"]     = cam.type == CameraType::Orthographic ? "orthographic" : "perspective";
            ce["position"] = vec3(cam.position);
            ce["target"]   = vec3(cam.target);
            ce["fov"]      = cam.fov;
            ce["orthoSize"]   = cam.orthoSize;
            ce["orthoAspect"] = cam.orthoAspect;
            ce["near"]     = cam.nearPlane;
            ce["far"]      = cam.farPlane;
            if (cam.rotation) ce["rotation"] = vec3(*cam.rotation);
            list.push_back(std::move(ce));
        }
        c["list"] = std::move(list);
        j["cameras"] = std::move(c);
    }

    if (!doc.textures.empty() || !doc.svgTextures.empty()) {
        json arr = json::array();
        for (const auto& [id, tex] : doc.textures) {
            if (doc.includedTextures.count(id)) continue;
            json te = json::object();
            te["id"]         = id;
            te["type"]       = "bitmap";
            if (!tex.name.empty()) te["name"] = tex.name;
            te["uri"]         = tex.uri;
            te["wrapU"]       = tex.wrapU;
            te["wrapV"]       = tex.wrapV;
            te["filter"]      = tex.filter;
            te["colorSpace"]  = tex.colorSpace;
            te["mipMaps"]     = tex.mipMaps;
            arr.push_back(std::move(te));
        }
        for (const auto& [id, svg] : doc.svgTextures) {
            if (doc.includedTextures.count(id)) continue;
            json te = json::object();
            te["id"]   = id;
            te["type"] = "svg";
            if (!svg.src.empty())            te["src"] = svg.src;
            if (!svg.inlineContent.empty())  te["inlineContent"] = svg.inlineContent;
            arr.push_back(std::move(te));
        }
        j["textures"] = std::move(arr);
    }

    if (!doc.materials.empty()) {
        json arr = json::array();
        for (const auto& [id, mat] : doc.materials) {
            if (doc.includedMaterials.count(id)) continue;
            json me = json::object();
            me["id"]                       = id;
            me["baseColor"]                = vec4(mat.baseColor);
            me["roughness"]                = mat.roughness;
            me["metallic"]                 = mat.metallic;
            me["normalScale"]              = mat.normalScale;
            me["occlusionStrength"]        = mat.occlusionStrength;
            me["emissiveColor"]             = vec3(mat.emissiveColor);
            me["alphaMode"]                = mat.alphaMode;
            me["alphaCutoff"]              = mat.alphaCutoff;
            me["doubleSided"]              = mat.doubleSided;
            if (!mat.baseColorTexture.empty())         me["baseColorTexture"] = mat.baseColorTexture;
            if (!mat.normalTexture.empty())            me["normalTexture"] = mat.normalTexture;
            if (!mat.emissiveTexture.empty())          me["emissiveTexture"] = mat.emissiveTexture;
            if (!mat.metallicRoughnessTexture.empty()) me["metallicRoughnessTexture"] = mat.metallicRoughnessTexture;
            if (!mat.occlusionTexture.empty())         me["occlusionTexture"] = mat.occlusionTexture;
            arr.push_back(std::move(me));
        }
        j["materials"] = std::move(arr);
    }

    if (!doc.embeds.empty()) {
        json arr = json::array();
        for (const auto& [id, em] : doc.embeds) {
            if (doc.includedEmbeds.count(id)) continue;
            json ee = json::object();
            ee["id"] = id;
            if (!em.src.empty())           ee["src"] = em.src;
            if (!em.base64Content.empty()) ee["base64Content"] = em.base64Content;
            arr.push_back(std::move(ee));
        }
        j["embeds"] = std::move(arr);
    }

    if (!doc.scripts.empty()) {
        json arr = json::array();
        for (const auto& [id, sc] : doc.scripts) {
            json se = json::object();
            se["id"]     = id;
            se["scriptType"] = sc.type;
            se["source"] = sc.source;
            arr.push_back(std::move(se));
        }
        j["scripts"] = std::move(arr);
    }

    if (!doc.sounds.empty()) {
        json arr = json::array();
        for (const auto& [id, snd] : doc.sounds) {
            json se = json::object();
            se["id"]   = id;
            se["src"]  = snd.src;
            se["loop"] = snd.loop;
            arr.push_back(std::move(se));
        }
        j["sounds"] = std::move(arr);
    }

    if (!doc.musicTracks.empty()) {
        json arr = json::array();
        for (const auto& [id, mus] : doc.musicTracks) {
            json te = json::object();
            te["id"]   = id;
            te["src"]  = mus.src;
            te["loop"] = mus.loop;
            arr.push_back(std::move(te));
        }
        j["music"] = std::move(arr);
    }

    if (!doc.triggers.empty()) {
        json arr = json::array();
        for (const auto& [id, trig] : doc.triggers) {
            json te = json::object();
            te["id"] = id;
            json steps = json::array();
            for (const auto& step : trig.steps) {
                json se = json::object();
                switch (step.type) {
                case TriggerStepType::PlayAction: se["type"] = "playAction"; break;
                case TriggerStepType::PlaySound:  se["type"] = "playSound";  break;
                case TriggerStepType::RunScript:  se["type"] = "runScript";  break;
                case TriggerStepType::PlayMusic:  se["type"] = "playMusic";  break;
                }
                se["ref"] = step.ref;
                steps.push_back(std::move(se));
            }
            te["steps"] = std::move(steps);
            arr.push_back(std::move(te));
        }
        j["triggers"] = std::move(arr);
    }

    if (!doc.sceneStates.empty()) {
        json arr = json::array();
        for (const auto& [name, state] : doc.sceneStates) {
            json se = json::object();
            se["name"] = name;
            json overrides = json::array();
            for (const auto& ovr : state.overrides) {
                json oe = json::object();
                oe["id"] = ovr.id;
                if (ovr.visible)  oe["visible"]  = *ovr.visible;
                if (ovr.position) oe["position"] = vec3(*ovr.position);
                if (ovr.rotation) oe["rotation"] = vec3(*ovr.rotation);
                if (ovr.material) oe["material"] = *ovr.material;
                overrides.push_back(std::move(oe));
            }
            se["overrides"] = std::move(overrides);
            arr.push_back(std::move(se));
        }
        j["states"] = std::move(arr);
    }

    if (!doc.definitions.empty()) {
        json arr = json::array();
        for (const auto& [id, obj] : doc.definitions) {
            if (doc.includedDefs.count(id)) continue;
            json de = json::object();
            de["id"]     = id;
            de["object"] = objectJson(obj);
            arr.push_back(std::move(de));
        }
        j["definitions"] = std::move(arr);
    }

    if (!doc.objects.empty()) {
        json arr = json::array();
        for (const auto& obj : doc.objects) arr.push_back(objectJson(obj));
        j["objects"] = std::move(arr);
    }

    if (!doc.actions.empty()) {
        json arr = json::array();
        for (const auto& [name, act] : doc.actions) {
            json ae = json::object();
            ae["name"]      = name;
            ae["duration"]  = act.duration;
            ae["loop"]      = act.loop;
            ae["autoplay"]  = act.autoplay;
            ae["timeScale"] = act.timeScale;
            json channels = json::array();
            for (const auto& ch : act.channels) {
                json ce = json::object();
                ce["target"]   = ch.targetObject;
                ce["property"] = animatedPropertyName(ch.property);
                json kfs = json::array();
                for (const auto& kf : ch.keyframes) {
                    json ke = json::object();
                    ke["time"]  = kf.time;
                    ke["value"] = kf.value;
                    ke["interp"] = interpName(kf.interpolation);
                    if (kf.interpolation == Interpolation::CubicBezier) {
                        ke["handleLeft"]  = json::array({kf.handleLeft.dt, kf.handleLeft.dv});
                        ke["handleRight"] = json::array({kf.handleRight.dt, kf.handleRight.dv});
                    }
                    kfs.push_back(std::move(ke));
                }
                ce["keyframes"] = std::move(kfs);
                channels.push_back(std::move(ce));
            }
            ae["channels"] = std::move(channels);
            arr.push_back(std::move(ae));
        }
        j["actions"] = std::move(arr);
    }

    return j.dump(2);
}

void Mc3JsonWriter::write(const Mc3Document& doc, const std::filesystem::path& path) {
    const std::string content = toString(doc);

    // Same write-to-temp-then-rename pattern as Mc3XmlWriter::write() so a
    // crash/disk-full mid-write can never leave a truncated file at `path`.
    std::filesystem::path tmpPath = path;
    tmpPath += ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Failed to save JSON: " + path.string());
        out << content;
        if (!out) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);
            throw std::runtime_error("Failed to save JSON: " + path.string());
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        throw std::runtime_error("Failed to finalize JSON save (rename): " + path.string());
    }
}
