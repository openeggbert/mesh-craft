#include "Mc3JsonParser.hpp"
#include <MeshCraft/Mc3/Mc3Extrude.hpp>
#include <MeshCraft/Mc3/Mc3SceneState.hpp>
#include <MeshCraft/Mc3/Mc3Trigger.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mc3::Internal;

namespace {

std::array<float,3> toVec3(const json& j, std::array<float,3> def = {0.f,0.f,0.f}) {
    if (!j.is_array() || j.size() < 3) return def;
    return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
}

std::array<float,4> toVec4(const json& j, std::array<float,4> def = {0.f,0.f,0.f,1.f}) {
    if (!j.is_array() || j.size() < 4) return def;
    return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>()};
}

Mc3Transform toTransform(const json& j) {
    Mc3Transform t;
    if (!j.is_object()) return t;
    if (j.contains("position")) t.position = toVec3(j["position"]);
    if (j.contains("rotation")) t.rotation = toVec3(j["rotation"]);
    if (j.contains("scale"))    t.scale    = toVec3(j["scale"], {1.f,1.f,1.f});
    if (j.contains("pivot"))    t.pivot    = toVec3(j["pivot"]);
    return t;
}

ObjectType objectTypeFromName(const std::string& s) {
    if (s == "box")          return ObjectType::Box;
    if (s == "cube")         return ObjectType::Cube;
    if (s == "sphere")       return ObjectType::Sphere;
    if (s == "cylinder")     return ObjectType::Cylinder;
    if (s == "cone")         return ObjectType::Cone;
    if (s == "plane")        return ObjectType::Plane;
    if (s == "torus")        return ObjectType::Torus;
    if (s == "capsule")      return ObjectType::Capsule;
    if (s == "disk")         return ObjectType::Disk;
    if (s == "grid")         return ObjectType::Grid;
    if (s == "icosphere")    return ObjectType::IcoSphere;
    if (s == "mesh")         return ObjectType::Mesh;
    if (s == "extrude")      return ObjectType::Extrude;
    if (s == "instance")     return ObjectType::Instance;
    if (s == "union")        return ObjectType::Union;
    if (s == "difference")   return ObjectType::Difference;
    if (s == "intersection") return ObjectType::Intersection;
    if (s == "area")         return ObjectType::Area;
    return ObjectType::Group;
}

PrimitiveType primitiveTypeFromName(const std::string& s) {
    if (s == "cube")      return PrimitiveType::Cube;
    if (s == "sphere")    return PrimitiveType::Sphere;
    if (s == "cylinder")  return PrimitiveType::Cylinder;
    if (s == "cone")      return PrimitiveType::Cone;
    if (s == "plane")     return PrimitiveType::Plane;
    if (s == "torus")     return PrimitiveType::Torus;
    if (s == "capsule")   return PrimitiveType::Capsule;
    if (s == "disk")      return PrimitiveType::Disk;
    if (s == "grid")      return PrimitiveType::Grid;
    if (s == "icosphere") return PrimitiveType::IcoSphere;
    return PrimitiveType::Box;
}

Mc3Primitive toPrimitive(const json& j) {
    Mc3Primitive p;
    if (!j.is_object()) return p;
    if (j.contains("primitiveType")) p.primitiveType = primitiveTypeFromName(j["primitiveType"].get<std::string>());
    if (j.contains("size"))          p.size          = toVec3(j["size"], {1.f,1.f,1.f});
    if (j.contains("radius"))        p.radius        = j["radius"].get<float>();
    if (j.contains("height"))        p.height        = j["height"].get<float>();
    if (j.contains("segments"))      p.segments      = j["segments"].get<int>();
    if (j.contains("axis"))          p.axis          = j["axis"].get<std::string>();
    if (j.contains("majorRadius"))   p.majorRadius   = j["majorRadius"].get<float>();
    if (j.contains("minorRadius"))   p.minorRadius   = j["minorRadius"].get<float>();
    if (j.contains("subdivisionsX")) p.subdivisionsX = j["subdivisionsX"].get<int>();
    if (j.contains("subdivisionsZ")) p.subdivisionsZ = j["subdivisionsZ"].get<int>();
    return p;
}

Mc3CrossSection toCrossSection(const json& j) {
    Mc3CrossSection cs;
    if (!j.is_object()) return cs;
    const std::string t = j.value("type", "rect");
    if (t == "circle")  cs.type = CrossSectionType::Circle;
    else if (t == "polygon") cs.type = CrossSectionType::Polygon;
    else if (t == "custom")  cs.type = CrossSectionType::Custom;
    else if (t == "star")    cs.type = CrossSectionType::Star;
    else cs.type = CrossSectionType::Rect;
    if (j.contains("width"))       cs.width       = j["width"].get<float>();
    if (j.contains("height"))      cs.height      = j["height"].get<float>();
    if (j.contains("radius"))      cs.radius      = j["radius"].get<float>();
    if (j.contains("innerRadius")) cs.innerRadius = j["innerRadius"].get<float>();
    if (j.contains("sides"))       cs.sides       = j["sides"].get<int>();
    if (j.contains("segments"))    cs.segments    = j["segments"].get<int>();
    if (j.contains("customPoints")) {
        for (const auto& pt : j["customPoints"])
            cs.customPoints.push_back({pt.at(0).get<float>(), pt.at(1).get<float>()});
    }
    return cs;
}

Mc3ExtrudePath toPath(const json& j) {
    Mc3ExtrudePath path;
    if (!j.is_object()) return path;
    const std::string t = j.value("type", "line");
    if (t == "arc")      path.type = ExtrudePathType::Arc;
    else if (t == "helix")    path.type = ExtrudePathType::Helix;
    else if (t == "polyline") path.type = ExtrudePathType::Polyline;
    else if (t == "bezier")   path.type = ExtrudePathType::Bezier;
    else path.type = ExtrudePathType::Line;
    if (j.contains("length"))      path.length      = j["length"].get<float>();
    if (j.contains("axis"))        path.axis        = j["axis"].get<std::string>();
    if (j.contains("arcRadius"))   path.arcRadius   = j["arcRadius"].get<float>();
    if (j.contains("arcAngle"))    path.arcAngle    = j["arcAngle"].get<float>();
    if (j.contains("helixRadius")) path.helixRadius = j["helixRadius"].get<float>();
    if (j.contains("helixHeight")) path.helixHeight = j["helixHeight"].get<float>();
    if (j.contains("helixTurns"))  path.helixTurns  = j["helixTurns"].get<float>();
    if (j.contains("points")) {
        for (const auto& pe : j["points"]) {
            Mc3PathPoint pt;
            if (pe.contains("position"))  pt.position  = toVec3(pe["position"]);
            if (pe.contains("controlIn")) pt.controlIn = toVec3(pe["controlIn"]);
            path.points.push_back(pt);
        }
    }
    return path;
}

Mc3Extrude toExtrude(const json& j) {
    Mc3Extrude ex;
    if (!j.is_object()) return ex;
    if (j.contains("crossSection")) ex.crossSection = toCrossSection(j["crossSection"]);
    if (j.contains("path"))         ex.path         = toPath(j["path"]);
    if (j.contains("twist"))        ex.twist        = j["twist"].get<float>();
    if (j.contains("segments"))     ex.segments     = j["segments"].get<int>();
    if (j.contains("smooth"))       ex.smooth       = j["smooth"].get<bool>();
    if (j.contains("caps"))         ex.caps         = j["caps"].get<bool>();
    return ex;
}

Mc3UvMapping toUvMapping(const json& j) {
    Mc3UvMapping m;
    const std::string p = j.value("projection", "planar");
    if (p == "box")    m.projection = UvProjection::Box;
    else if (p == "sphere") m.projection = UvProjection::Sphere;
    else m.projection = UvProjection::Planar;
    if (j.contains("scaleU"))   m.scaleU   = j["scaleU"].get<float>();
    if (j.contains("scaleV"))   m.scaleV   = j["scaleV"].get<float>();
    if (j.contains("offsetU"))  m.offsetU  = j["offsetU"].get<float>();
    if (j.contains("offsetV"))  m.offsetV  = j["offsetV"].get<float>();
    if (j.contains("rotation")) m.rotation = j["rotation"].get<float>();
    return m;
}

std::shared_ptr<Mc3Object> toObject(const json& j) {
    auto obj = std::make_shared<Mc3Object>();
    if (!j.is_object()) return obj;

    obj->type = objectTypeFromName(j.value("type", "group"));
    obj->id       = j.value("id", "");
    obj->name     = j.value("name", "");
    if (j.contains("transform")) obj->transform = toTransform(j["transform"]);
    obj->material = j.value("material", "");
    obj->visible  = j.value("visible", true);
    obj->collision = j.value("collision", "none");
    obj->layer    = j.value("layer", "");
    if (j.contains("tags"))
        obj->tags = j["tags"].get<std::vector<std::string>>();
    obj->isCutter = j.value("role", "") == "cutter";

    if (j.contains("deform"))
        obj->deform = Mc3Deform{ toVec3(j["deform"].value("scale", json::array({1.f,1.f,1.f})), {1.f,1.f,1.f}) };
    if (j.contains("uvMapping")) obj->uvMapping = toUvMapping(j["uvMapping"]);
    if (j.contains("primitive")) obj->primitive = toPrimitive(j["primitive"]);
    if (j.contains("extrude"))   obj->extrude   = toExtrude(j["extrude"]);
    if (j.contains("csgOperation")) {
        const std::string t = j["csgOperation"].value("csgType", "union");
        Mc3CsgOperation csg;
        if (t == "difference")        csg.csgType = CsgType::Difference;
        else if (t == "intersection") csg.csgType = CsgType::Intersection;
        else                           csg.csgType = CsgType::Union;
        obj->csgOperation = csg;
    }

    if (j.contains("meshSource")) obj->meshSource = j["meshSource"].get<std::string>();

    if (obj->type == ObjectType::Instance) {
        obj->definition = j.value("definition", "");
        obj->materialOverride = j.value("materialOverride", "");
        if (j.contains("variants"))
            obj->variantDefinitions = j["variants"].get<std::vector<std::string>>();
    }

    if (j.contains("metadata")) {
        for (auto it = j["metadata"].begin(); it != j["metadata"].end(); ++it)
            obj->metadata[it.key()] = it.value().get<std::string>();
    }

    // R111 -- structured asset metadata (mesh_world_revival.md §6).
    if (j.contains("assetMetadata")) {
        const auto& m = j["assetMetadata"];
        Mc3AssetMetadata am;
        am.category            = m.value("category", "");
        am.subcategory          = m.value("subcategory", "");
        if (m.contains("semanticTags")) am.semanticTags = m["semanticTags"].get<std::vector<std::string>>();
        if (m.contains("styleTags"))    am.styleTags    = m["styleTags"].get<std::vector<std::string>>();
        if (m.contains("regionTags"))   am.regionTags   = m["regionTags"].get<std::vector<std::string>>();
        if (m.contains("periodTags"))   am.periodTags   = m["periodTags"].get<std::vector<std::string>>();
        if (m.contains("nominalSize"))  am.nominalSize  = toVec3(m["nominalSize"]);
        if (m.contains("bounds")) {
            const auto& b = m["bounds"];
            if (b.contains("min")) am.boundsMin = toVec3(b["min"]);
            if (b.contains("max")) am.boundsMax = toVec3(b["max"]);
        }
        am.facing = m.value("facing", "");
        if (m.contains("sockets"))
            for (auto it = m["sockets"].begin(); it != m["sockets"].end(); ++it)
                am.sockets[it.key()] = toVec3(it.value());
        if (m.contains("materialSlots")) am.materialSlots = m["materialSlots"].get<std::vector<std::string>>();
        am.collisionProxy = m.value("collisionProxy", "");
        if (m.contains("clearanceVolume")) am.clearanceVolume = toVec3(m["clearanceVolume"]);
        if (m.contains("lods"))
            for (auto it = m["lods"].begin(); it != m["lods"].end(); ++it)
                am.lods[it.key()] = it.value().get<std::string>();
        am.instancingEligible     = m.value("instancingEligible", true);
        am.shadowPolicy           = m.value("shadowPolicy", "");
        am.maxVisibilityDistanceM = m.value("maxVisibilityDistanceM", 0.0f);
        am.selectionWeight        = m.value("selectionWeight", 1.0f);
        am.license                = m.value("license", "");
        am.provenance             = m.value("provenance", "");
        am.sourceGeneratorOrHash  = m.value("sourceGeneratorOrHash", "");
        am.semanticVersion        = m.value("semanticVersion", "");
        obj->assetMetadata = std::move(am);
    }

    if (j.contains("states")) {
        for (auto it = j["states"].begin(); it != j["states"].end(); ++it) {
            const auto& se = it.value();
            Mc3ObjectState st;
            if (se.contains("position")) st.position = toVec3(se["position"]);
            if (se.contains("rotation")) st.rotation = toVec3(se["rotation"]);
            if (se.contains("scale"))    st.scale    = toVec3(se["scale"], {1.f,1.f,1.f});
            if (se.contains("visible"))  st.visible  = se["visible"].get<bool>();
            if (se.contains("material")) st.material = se["material"].get<std::string>();
            obj->states[it.key()] = st;
        }
    }

    if (j.contains("children")) {
        for (const auto& ce : j["children"])
            obj->children.push_back(toObject(ce));
    }

    return obj;
}

} // namespace

Mc3Document Mc3JsonParser::parseString(const std::string& jsonText,
                                       const std::filesystem::path& sourceDir,
                                       const Mc3LoadPolicy& /*policy*/) {
    json j;
    try {
        j = json::parse(jsonText);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("Failed to parse mc3.json: ") + e.what());
    }

    Mc3Document doc;
    doc.sourcePath      = sourceDir;
    doc.version         = j.value("version", doc.version);
    doc.model           = j.value("model", "");
    doc.unit            = j.value("unit", doc.unit);
    doc.coordinateSystem= j.value("coordinateSystem", doc.coordinateSystem);
    doc.rotationUnits   = j.value("rotationUnits", doc.rotationUnits);
    doc.eulerOrder      = j.value("eulerOrder", doc.eulerOrder);

    // R110 -- library identity (.mc3lib.json only; absent on ordinary
    // scene/model documents).
    if (j.contains("library")) {
        const auto& lib = j["library"];
        Mc3LibraryInfo info;
        info.libraryNamespace = lib.value("namespace", "");
        info.version          = lib.value("version", "");
        info.contentHash      = lib.value("hash", "");
        doc.library = std::move(info);
    }

    // R101 -- library imports (see Mc3Import's own doc comment).
    if (j.contains("imports"))
        for (const auto& impJson : j["imports"]) {
            Mc3Import imp;
            imp.importNamespace = impJson.value("namespace", "");
            imp.source          = impJson.value("source", "");
            imp.hash            = impJson.value("hash", "");
            doc.imports.push_back(std::move(imp));
        }

    if (j.contains("includes"))
        doc.includes = j["includes"].get<std::vector<std::string>>();

    if (j.contains("metadata"))
        for (auto it = j["metadata"].begin(); it != j["metadata"].end(); ++it)
            doc.metadata[it.key()] = it.value().get<std::string>();
    if (j.contains("meta"))
        for (auto it = j["meta"].begin(); it != j["meta"].end(); ++it)
            doc.meta[it.key()] = it.value().get<std::string>();

    if (j.contains("environment")) {
        const auto& e = j["environment"];
        Mc3Environment env;
        env.backgroundColor  = toVec3(e.value("backgroundColor", json::array({0.f,0.f,0.f})));
        env.backgroundTexture= e.value("backgroundTexture", "");
        env.skyboxTexture    = e.value("skyboxTexture", "");
        if (e.contains("fog")) {
            const auto& f = e["fog"];
            Mc3Fog fog;
            fog.mode    = f.value("mode", "linear") == "exponential" ? FogMode::Exponential : FogMode::Linear;
            fog.color   = toVec3(f.value("color", json::array({0.5f,0.5f,0.5f})));
            fog.start   = f.value("start", 10.0f);
            fog.end     = f.value("end", 100.0f);
            fog.density = f.value("density", 0.01f);
            env.fog = fog;
        }
        doc.environment = env;
    }

    if (j.contains("lights")) {
        for (const auto& le : j["lights"]) {
            Mc3Light l;
            const std::string t = le.value("type", "directional");
            if (t == "ambient")     l.type = LightType::Ambient;
            else if (t == "spot")   l.type = LightType::Spot;
            else if (t == "point")  l.type = LightType::Point;
            else                    l.type = LightType::Directional;
            l.name        = le.value("name", "");
            l.color       = toVec3(le.value("color", json::array({1.f,1.f,1.f})), {1.f,1.f,1.f});
            l.brightness  = le.value("brightness", 1.0f);
            l.direction   = toVec3(le.value("direction", json::array({0.f,-1.f,0.f})), {0.f,-1.f,0.f});
            l.position    = toVec3(le.value("position", json::array({0.f,0.f,0.f})));
            l.range       = le.value("range", 0.0f);
            l.angle       = le.value("angle", 45.0f);
            l.falloff     = le.value("falloff", 0.0f);
            l.castShadows = le.value("castShadows", false);
            doc.lights.push_back(l);
        }
    }

    if (j.contains("cameras")) {
        const auto& c = j["cameras"];
        doc.defaultCamera = c.value("default", "");
        if (c.contains("list")) {
            for (const auto& ce : c["list"]) {
                Mc3Camera cam;
                cam.name  = ce.value("name", "");
                cam.type  = ce.value("type", "perspective") == "orthographic" ? CameraType::Orthographic : CameraType::Perspective;
                cam.position = toVec3(ce.value("position", json::array({0.f,5.f,10.f})), {0.f,5.f,10.f});
                cam.target   = toVec3(ce.value("target", json::array({0.f,0.f,0.f})));
                cam.fov      = ce.value("fov", 60.0f);
                cam.orthoSize   = ce.value("orthoSize", 10.0f);
                cam.orthoAspect = ce.value("orthoAspect", 1.0f);
                cam.nearPlane   = ce.value("near", 0.1f);
                cam.farPlane    = ce.value("far", 1000.0f);
                if (ce.contains("rotation")) cam.rotation = toVec3(ce["rotation"]);
                doc.cameras.push_back(cam);
            }
        }
    }

    if (j.contains("textures")) {
        for (const auto& te : j["textures"]) {
            const std::string id = te.value("id", "");
            if (te.value("type", "bitmap") == "svg") {
                Mc3SvgTexture svg;
                svg.id = id;
                svg.src = te.value("src", "");
                svg.inlineContent = te.value("inlineContent", "");
                doc.svgTextures[id] = svg;
            } else {
                Mc3Texture tex;
                tex.name = te.value("name", id);
                tex.uri  = te.value("uri", "");
                tex.wrapU = te.value("wrapU", tex.wrapU);
                tex.wrapV = te.value("wrapV", tex.wrapV);
                tex.filter = te.value("filter", tex.filter);
                tex.colorSpace = te.value("colorSpace", tex.colorSpace);
                tex.mipMaps = te.value("mipMaps", tex.mipMaps);
                doc.textures[id] = tex;
            }
        }
    }

    if (j.contains("materials")) {
        for (const auto& me : j["materials"]) {
            const std::string id = me.value("id", "");
            Mc3Material mat;
            mat.name = id;
            mat.baseColor = toVec4(me.value("baseColor", json::array({0.8f,0.8f,0.8f,1.0f})), {0.8f,0.8f,0.8f,1.0f});
            mat.roughness = me.value("roughness", mat.roughness);
            mat.metallic  = me.value("metallic", mat.metallic);
            mat.normalScale = me.value("normalScale", mat.normalScale);
            mat.occlusionStrength = me.value("occlusionStrength", mat.occlusionStrength);
            mat.emissiveColor = toVec3(me.value("emissiveColor", json::array({0.f,0.f,0.f})));
            mat.alphaMode = me.value("alphaMode", mat.alphaMode);
            mat.alphaCutoff = me.value("alphaCutoff", mat.alphaCutoff);
            mat.doubleSided = me.value("doubleSided", mat.doubleSided);
            mat.baseColorTexture = me.value("baseColorTexture", "");
            mat.normalTexture = me.value("normalTexture", "");
            mat.emissiveTexture = me.value("emissiveTexture", "");
            mat.metallicRoughnessTexture = me.value("metallicRoughnessTexture", "");
            mat.occlusionTexture = me.value("occlusionTexture", "");
            doc.materials[id] = mat;
        }
    }

    if (j.contains("embeds")) {
        for (const auto& ee : j["embeds"]) {
            Mc3EmbedGltf em;
            em.id = ee.value("id", "");
            em.src = ee.value("src", "");
            em.base64Content = ee.value("base64Content", "");
            doc.embeds[em.id] = em;
        }
    }

    if (j.contains("scripts")) {
        for (const auto& se : j["scripts"]) {
            Mc3Script sc;
            sc.id     = se.value("id", "");
            sc.type   = se.value("scriptType", "lua");
            sc.source = se.value("source", "");
            doc.scripts[sc.id] = sc;
        }
    }

    if (j.contains("sounds")) {
        for (const auto& se : j["sounds"]) {
            Mc3Sound snd;
            snd.id   = se.value("id", "");
            snd.src  = se.value("src", "");
            snd.loop = se.value("loop", false);
            doc.sounds[snd.id] = snd;
        }
    }

    if (j.contains("music")) {
        for (const auto& te : j["music"]) {
            Mc3Music mus;
            mus.id   = te.value("id", "");
            mus.src  = te.value("src", "");
            mus.loop = te.value("loop", true);
            doc.musicTracks[mus.id] = mus;
        }
    }

    if (j.contains("triggers")) {
        for (const auto& te : j["triggers"]) {
            Mc3Trigger trig;
            trig.id = te.value("id", "");
            if (te.contains("steps")) {
                for (const auto& se : te["steps"]) {
                    Mc3TriggerStep step;
                    const std::string t = se.value("type", "playAction");
                    if (t == "playSound")   step.type = TriggerStepType::PlaySound;
                    else if (t == "runScript") step.type = TriggerStepType::RunScript;
                    else if (t == "playMusic") step.type = TriggerStepType::PlayMusic;
                    else step.type = TriggerStepType::PlayAction;
                    step.ref = se.value("ref", "");
                    trig.steps.push_back(step);
                }
            }
            doc.triggers[trig.id] = trig;
        }
    }

    if (j.contains("states")) {
        for (const auto& se : j["states"]) {
            Mc3SceneState state;
            state.name = se.value("name", "");
            if (se.contains("overrides")) {
                for (const auto& oe : se["overrides"]) {
                    Mc3ObjectOverride ovr;
                    ovr.id = oe.value("id", "");
                    if (oe.contains("visible"))  ovr.visible  = oe["visible"].get<bool>();
                    if (oe.contains("position")) ovr.position = toVec3(oe["position"]);
                    if (oe.contains("rotation")) ovr.rotation = toVec3(oe["rotation"]);
                    if (oe.contains("material")) ovr.material = oe["material"].get<std::string>();
                    state.overrides.push_back(ovr);
                }
            }
            doc.sceneStates[state.name] = state;
        }
    }

    if (j.contains("definitions")) {
        for (const auto& de : j["definitions"]) {
            const std::string id = de.value("id", "");
            doc.definitions[id] = toObject(de.value("object", json::object()));
        }
    }

    if (j.contains("objects")) {
        for (const auto& oe : j["objects"])
            doc.objects.push_back(toObject(oe));
    }

    if (j.contains("actions")) {
        for (const auto& ae : j["actions"]) {
            Mc3Action act;
            act.name      = ae.value("name", "");
            act.duration  = ae.value("duration", 1.0f);
            act.loop      = ae.value("loop", false);
            act.autoplay  = ae.value("autoplay", false);
            act.timeScale = ae.value("timeScale", 1.0f);
            if (ae.contains("channels")) {
                for (const auto& ce : ae["channels"]) {
                    Mc3Channel ch;
                    ch.targetObject = ce.value("target", "");
                    const std::string propName = ce.value("property", "");
                    if (auto prop = animatedPropertyFromName(propName))
                        ch.property = *prop;
                    if (ce.contains("keyframes")) {
                        for (const auto& ke : ce["keyframes"]) {
                            Mc3Keyframe kf;
                            kf.time  = ke.value("time", 0.0f);
                            kf.value = ke.value("value", 0.0f);
                            const std::string interp = ke.value("interp", "linear");
                            if (interp == "step")  kf.interpolation = Interpolation::Step;
                            else if (interp == "cubic") kf.interpolation = Interpolation::CubicBezier;
                            else kf.interpolation = Interpolation::Linear;
                            if (ke.contains("handleLeft"))
                                kf.handleLeft = { ke["handleLeft"].at(0).get<float>(), ke["handleLeft"].at(1).get<float>() };
                            if (ke.contains("handleRight"))
                                kf.handleRight = { ke["handleRight"].at(0).get<float>(), ke["handleRight"].at(1).get<float>() };
                            ch.keyframes.push_back(kf);
                        }
                    }
                    act.channels.push_back(std::move(ch));
                }
            }
            doc.actions[act.name] = std::move(act);
        }
    }

    return doc;
}

Mc3Document Mc3JsonParser::parse(const std::filesystem::path& path, const Mc3LoadPolicy& policy) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open mc3.json: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return parseString(ss.str(), path.parent_path(), policy);
}
