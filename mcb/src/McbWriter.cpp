#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Camera.hpp"
#include "MeshCraft/Mc3/Mc3Environment.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Texture.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace MeshCraft::Mcb {

// ---------------------------------------------------------------------------
// Low-level write helpers
// ---------------------------------------------------------------------------

static void wU8(std::ostream& o, uint8_t v) {
    o.put(static_cast<char>(v));
}
static void wU32(std::ostream& o, uint32_t v) {
    char b[4];
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    b[2] = static_cast<char>((v >> 16) & 0xFF);
    b[3] = static_cast<char>((v >> 24) & 0xFF);
    o.write(b, 4);
}
static void wI32(std::ostream& o, int32_t v) {
    wU32(o, static_cast<uint32_t>(v));
}
static void wF32(std::ostream& o, float v) {
    uint32_t u; std::memcpy(&u, &v, 4); wU32(o, u);
}
static void wRawStr(std::ostream& o, const std::string& s) {
    wU32(o, static_cast<uint32_t>(s.size()));
    if (!s.empty()) o.write(s.data(), static_cast<std::streamsize>(s.size()));
}
static void wVec3(std::ostream& o, const std::array<float,3>& v) {
    wF32(o, v[0]); wF32(o, v[1]); wF32(o, v[2]);
}
static void wVec4(std::ostream& o, const std::array<float,4>& v) {
    wF32(o, v[0]); wF32(o, v[1]); wF32(o, v[2]); wF32(o, v[3]);
}

// Key = uint8 len + bytes  (keys are always short ASCII, len < 256)
static void wKey(std::ostream& o, const char* k) {
    auto len = static_cast<uint8_t>(std::strlen(k));
    wU8(o, len);
    o.write(k, len);
}
static void wEnd(std::ostream& o) { wU8(o, 0); } // end-of-object sentinel

// Field = key + tag + data
static void wFieldStr (std::ostream& o, const char* k, const std::string& v)   { wKey(o,k); wU8(o,TAG_STR);  wRawStr(o,v); }
static void wFieldF32 (std::ostream& o, const char* k, float v)                 { wKey(o,k); wU8(o,TAG_F32);  wF32(o,v); }
static void wFieldI32 (std::ostream& o, const char* k, int v)                   { wKey(o,k); wU8(o,TAG_I32);  wI32(o,v); }
static void wFieldBool(std::ostream& o, const char* k, bool v)                  { wKey(o,k); wU8(o,TAG_BOOL); wU8(o, v?1:0); }
static void wFieldVec3(std::ostream& o, const char* k, const std::array<float,3>& v) { wKey(o,k); wU8(o,TAG_VEC3); wVec3(o,v); }
static void wFieldVec4(std::ostream& o, const char* k, const std::array<float,4>& v) { wKey(o,k); wU8(o,TAG_VEC4); wVec4(o,v); }
// Begin a nested object field (caller must write fields then wEnd)
static void wKeyObj(std::ostream& o, const char* k) { wKey(o,k); wU8(o,TAG_OBJ); }
// Begin an array field (caller must write count elements, each with wU8(tag)+data)
static void wKeyArr(std::ostream& o, const char* k, uint32_t count) { wKey(o,k); wU8(o,TAG_ARR); wU32(o,count); }
// Begin a map field
static void wKeyMap(std::ostream& o, const char* k, uint32_t count) { wKey(o,k); wU8(o,TAG_MAP); wU32(o,count); }

// ---------------------------------------------------------------------------
// Mc3 type serializers
// ---------------------------------------------------------------------------

static void writeTransform(std::ostream& o, const Mc3::Mc3Transform& tf) {
    wFieldVec3(o, "position", tf.position);
    wFieldVec3(o, "rotation", tf.rotation);
    wFieldVec3(o, "scale",    tf.scale);
    wFieldVec3(o, "pivot",    tf.pivot);
    wEnd(o);
}

static void writePrimitive(std::ostream& o, const Mc3::Mc3Primitive& p) {
    // primitiveType as int (enum)
    wFieldI32 (o, "primitiveType", static_cast<int>(p.primitiveType));
    wFieldVec3(o, "size",          p.size);
    wFieldF32 (o, "radius",        p.radius);
    wFieldF32 (o, "height",        p.height);
    wFieldI32 (o, "segments",      p.segments);
    wFieldStr (o, "axis",          p.axis);
    wFieldF32 (o, "majorRadius",   p.majorRadius);
    wFieldF32 (o, "minorRadius",   p.minorRadius);
    wFieldI32 (o, "subdivisionsX", p.subdivisionsX);
    wFieldI32 (o, "subdivisionsZ", p.subdivisionsZ);
    wEnd(o);
}

static void writeDeform(std::ostream& o, const Mc3::Mc3Deform& d) {
    wFieldVec3(o, "scale", d.scale);
    wEnd(o);
}

static void writeCsgOp(std::ostream& o, const Mc3::Mc3CsgOperation& csg) {
    wFieldI32(o, "csgType", static_cast<int>(csg.csgType));
    wEnd(o);
}

static void writeCrossSection(std::ostream& o, const Mc3::Mc3CrossSection& cs) {
    wFieldI32(o, "type",        static_cast<int>(cs.type));
    wFieldF32(o, "width",       cs.width);
    wFieldF32(o, "height",      cs.height);
    wFieldF32(o, "radius",      cs.radius);
    wFieldF32(o, "innerRadius", cs.innerRadius);
    wFieldI32(o, "sides",       cs.sides);
    wFieldI32(o, "segments",    cs.segments);
    wKeyArr(o, "customPoints", static_cast<uint32_t>(cs.customPoints.size()));
    for (const auto& pt : cs.customPoints) {
        wU8(o, TAG_VEC3);
        wF32(o, pt.x); wF32(o, pt.y); wF32(o, 0.0f);
    }
    wEnd(o);
}

static void writePathPoint(std::ostream& o, const Mc3::Mc3PathPoint& pp) {
    wFieldVec3(o, "position",   pp.position);
    wFieldVec3(o, "controlIn",  pp.controlIn);
    wEnd(o);
}

static void writePath(std::ostream& o, const Mc3::Mc3ExtrudePath& p) {
    wFieldI32(o, "type",         static_cast<int>(p.type));
    wFieldF32(o, "length",       p.length);
    wFieldStr(o, "axis",         p.axis);
    wFieldF32(o, "arcRadius",    p.arcRadius);
    wFieldF32(o, "arcAngle",     p.arcAngle);
    wFieldF32(o, "helixRadius",  p.helixRadius);
    wFieldF32(o, "helixHeight",  p.helixHeight);
    wFieldF32(o, "helixTurns",   p.helixTurns);
    wKeyArr(o, "points", static_cast<uint32_t>(p.points.size()));
    for (const auto& pt : p.points) {
        wU8(o, TAG_OBJ);
        writePathPoint(o, pt);
    }
    wEnd(o);
}

static void writeExtrude(std::ostream& o, const Mc3::Mc3Extrude& ex) {
    wKeyObj(o, "crossSection"); writeCrossSection(o, ex.crossSection);
    wKeyObj(o, "path");         writePath(o, ex.path);
    wFieldF32 (o, "twist",    ex.twist);
    wFieldI32 (o, "segments", ex.segments);
    wFieldBool(o, "smooth",   ex.smooth);
    wFieldBool(o, "caps",     ex.caps);
    wEnd(o);
}

static void writeUvMapping(std::ostream& o, const Mc3::Mc3UvMapping& uv) {
    wFieldI32(o, "projection", static_cast<int>(uv.projection));
    wFieldF32(o, "scaleU",     uv.scaleU);
    wFieldF32(o, "scaleV",     uv.scaleV);
    wFieldF32(o, "offsetU",    uv.offsetU);
    wFieldF32(o, "offsetV",    uv.offsetV);
    wFieldF32(o, "rotation",   uv.rotation);
    wEnd(o);
}

static void writeObjectState(std::ostream& o, const Mc3::Mc3ObjectState& st) {
    if (st.position) { wFieldVec3(o, "position", *st.position); }
    if (st.rotation) { wFieldVec3(o, "rotation", *st.rotation); }
    if (st.scale)    { wFieldVec3(o, "scale",    *st.scale); }
    if (st.visible)  { wFieldBool(o, "visible",  *st.visible); }
    if (st.material) { wFieldStr (o, "material", *st.material); }
    wEnd(o);
}

static void writeObject(std::ostream& o, const Mc3::Mc3Object& obj);

static void writeObject(std::ostream& o, const Mc3::Mc3Object& obj) {
    wFieldI32 (o, "type",             static_cast<int>(obj.type));
    wFieldStr (o, "name",             obj.name);
    wFieldStr (o, "id",               obj.id);
    wFieldStr (o, "material",         obj.material);
    wFieldBool(o, "visible",          obj.visible);
    wFieldStr (o, "collision",        obj.collision);
    if (!obj.layer.empty())
        wFieldStr(o, "layer", obj.layer);
    if (obj.isCutter)
        wFieldBool(o, "isCutter", obj.isCutter);
    if (!obj.definition.empty())
        wFieldStr(o, "definition", obj.definition);
    if (!obj.meshSource.empty())
        wFieldStr(o, "meshSource", obj.meshSource);
    if (!obj.materialOverride.empty())
        wFieldStr(o, "materialOverride", obj.materialOverride);

    // Transform
    wKeyObj(o, "transform"); writeTransform(o, obj.transform);

    // Optional components
    if (obj.deform) {
        wKeyObj(o, "deform"); writeDeform(o, *obj.deform);
    }
    if (obj.primitive) {
        wKeyObj(o, "primitive"); writePrimitive(o, *obj.primitive);
    }
    if (obj.csgOperation) {
        wKeyObj(o, "csgOperation"); writeCsgOp(o, *obj.csgOperation);
    }
    if (obj.extrude) {
        wKeyObj(o, "extrude"); writeExtrude(o, *obj.extrude);
    }
    if (obj.uvMapping) {
        wKeyObj(o, "uvMapping"); writeUvMapping(o, *obj.uvMapping);
    }

    // Tags
    if (!obj.tags.empty()) {
        wKeyArr(o, "tags", static_cast<uint32_t>(obj.tags.size()));
        for (const auto& t : obj.tags) { wU8(o, TAG_STR); wRawStr(o, t); }
    }
    // variantDefinitions
    if (!obj.variantDefinitions.empty()) {
        wKeyArr(o, "variantDefs", static_cast<uint32_t>(obj.variantDefinitions.size()));
        for (const auto& d : obj.variantDefinitions) { wU8(o, TAG_STR); wRawStr(o, d); }
    }
    // States
    if (!obj.states.empty()) {
        wKeyMap(o, "states", static_cast<uint32_t>(obj.states.size()));
        for (const auto& [k, v] : obj.states) {
            wRawStr(o, k); wU8(o, TAG_OBJ); writeObjectState(o, v);
        }
    }
    // Children
    if (!obj.children.empty()) {
        wKeyArr(o, "children", static_cast<uint32_t>(obj.children.size()));
        for (const auto& c : obj.children) { wU8(o, TAG_OBJ); writeObject(o, *c); }
    }
    wEnd(o);
}

static void writeTexture(std::ostream& o, const Mc3::Mc3Texture& tex) {
    wFieldStr(o, "name",       tex.name);
    wFieldStr(o, "uri",        tex.uri);
    wFieldStr(o, "wrapU",      tex.wrapU);
    wFieldStr(o, "wrapV",      tex.wrapV);
    wFieldStr(o, "filter",     tex.filter);
    wFieldStr(o, "colorSpace", tex.colorSpace);
    wEnd(o);
}

static void writeMaterial(std::ostream& o, const Mc3::Mc3Material& m) {
    wFieldStr (o, "name",                     m.name);
    wFieldVec4(o, "baseColor",                m.baseColor);
    wFieldStr (o, "baseColorTexture",         m.baseColorTexture);
    wFieldStr (o, "normalTexture",            m.normalTexture);
    wFieldStr (o, "emissiveTexture",          m.emissiveTexture);
    wFieldStr (o, "metallicRoughnessTexture", m.metallicRoughnessTexture);
    wFieldStr (o, "occlusionTexture",         m.occlusionTexture);
    wFieldF32 (o, "roughness",                m.roughness);
    wFieldF32 (o, "metallic",                 m.metallic);
    wFieldF32 (o, "normalScale",              m.normalScale);
    wFieldF32 (o, "occlusionStrength",        m.occlusionStrength);
    wFieldVec3(o, "emissiveColor",            m.emissiveColor);
    wFieldStr (o, "alphaMode",                m.alphaMode);
    wFieldF32 (o, "alphaCutoff",              m.alphaCutoff);
    wFieldBool(o, "doubleSided",              m.doubleSided);
    wEnd(o);
}

static void writeLight(std::ostream& o, const Mc3::Mc3Light& lt) {
    wFieldI32 (o, "type",        static_cast<int>(lt.type));
    wFieldStr (o, "name",        lt.name);
    wFieldVec3(o, "color",       lt.color);
    wFieldF32 (o, "brightness",  lt.brightness);
    wFieldVec3(o, "direction",   lt.direction);
    wFieldVec3(o, "position",    lt.position);
    wFieldF32 (o, "range",       lt.range);
    wFieldF32 (o, "angle",       lt.angle);
    wFieldF32 (o, "falloff",     lt.falloff);
    wFieldBool(o, "castShadows", lt.castShadows);
    wEnd(o);
}

static void writeCamera(std::ostream& o, const Mc3::Mc3Camera& cam) {
    wFieldStr (o, "name",      cam.name);
    wFieldI32 (o, "type",      static_cast<int>(cam.type));
    wFieldVec3(o, "position",  cam.position);
    wFieldVec3(o, "target",    cam.target);
    if (cam.rotation) { wFieldVec3(o, "rotation", *cam.rotation); }
    wFieldF32 (o, "nearPlane", cam.nearPlane);
    wFieldF32 (o, "farPlane",  cam.farPlane);
    wFieldF32 (o, "fov",       cam.fov);
    wFieldF32 (o, "orthoSize", cam.orthoSize);
    wEnd(o);
}

static void writeFog(std::ostream& o, const Mc3::Mc3Fog& fog) {
    wFieldVec3(o, "color",   fog.color);
    wFieldI32 (o, "mode",    static_cast<int>(fog.mode));
    wFieldF32 (o, "start",   fog.start);
    wFieldF32 (o, "end",     fog.end);
    wFieldF32 (o, "density", fog.density);
    wEnd(o);
}

static void writeEnvironment(std::ostream& o, const Mc3::Mc3Environment& env) {
    wFieldVec3(o, "backgroundColor", env.backgroundColor);
    wFieldStr (o, "backgroundTexture", env.backgroundTexture);
    if (env.fog) { wKeyObj(o, "fog"); writeFog(o, *env.fog); }
    wEnd(o);
}

static void writeKeyframe(std::ostream& o, const Mc3::Mc3Keyframe& kf) {
    wFieldF32(o, "time",           kf.time);
    wFieldF32(o, "value",          kf.value);
    wFieldI32(o, "interpolation",  static_cast<int>(kf.interpolation));
    wFieldF32(o, "leftDt",         kf.handleLeft.dt);
    wFieldF32(o, "leftDv",         kf.handleLeft.dv);
    wFieldF32(o, "rightDt",        kf.handleRight.dt);
    wFieldF32(o, "rightDv",        kf.handleRight.dv);
    wEnd(o);
}

static void writeChannel(std::ostream& o, const Mc3::Mc3Channel& ch) {
    wFieldStr(o, "targetObject", ch.targetObject);
    wFieldI32(o, "property",     static_cast<int>(ch.property));
    wKeyArr(o, "keyframes", static_cast<uint32_t>(ch.keyframes.size()));
    for (const auto& kf : ch.keyframes) { wU8(o, TAG_OBJ); writeKeyframe(o, kf); }
    wEnd(o);
}

static void writeAction(std::ostream& o, const Mc3::Mc3Action& act) {
    wFieldStr (o, "name",     act.name);
    wFieldF32 (o, "duration", act.duration);
    wFieldBool(o, "loop",     act.loop);
    wKeyArr(o, "channels", static_cast<uint32_t>(act.channels.size()));
    for (const auto& ch : act.channels) { wU8(o, TAG_OBJ); writeChannel(o, ch); }
    wEnd(o);
}

static void writeDocument(std::ostream& o, const Mc3::Mc3Document& doc) {
    wFieldStr(o, "version",           doc.version);
    wFieldStr(o, "model",             doc.model);
    wFieldStr(o, "unit",              doc.unit);
    wFieldStr(o, "coordinateSystem",  doc.coordinateSystem);
    wFieldStr(o, "defaultCamera",     doc.defaultCamera);

    if (doc.environment) { wKeyObj(o, "environment"); writeEnvironment(o, *doc.environment); }

    // Lights
    wKeyArr(o, "lights", static_cast<uint32_t>(doc.lights.size()));
    for (const auto& l : doc.lights) { wU8(o, TAG_OBJ); writeLight(o, l); }

    // Cameras
    wKeyArr(o, "cameras", static_cast<uint32_t>(doc.cameras.size()));
    for (const auto& c : doc.cameras) { wU8(o, TAG_OBJ); writeCamera(o, c); }

    // Textures
    wKeyMap(o, "textures", static_cast<uint32_t>(doc.textures.size()));
    for (const auto& [k, v] : doc.textures) { wRawStr(o, k); wU8(o, TAG_OBJ); writeTexture(o, v); }

    // Materials
    wKeyMap(o, "materials", static_cast<uint32_t>(doc.materials.size()));
    for (const auto& [k, v] : doc.materials) { wRawStr(o, k); wU8(o, TAG_OBJ); writeMaterial(o, v); }

    // Definitions
    wKeyMap(o, "definitions", static_cast<uint32_t>(doc.definitions.size()));
    for (const auto& [k, v] : doc.definitions) { wRawStr(o, k); wU8(o, TAG_OBJ); writeObject(o, *v); }

    // Objects
    wKeyArr(o, "objects", static_cast<uint32_t>(doc.objects.size()));
    for (const auto& obj : doc.objects) { wU8(o, TAG_OBJ); writeObject(o, *obj); }

    // Actions
    wKeyMap(o, "actions", static_cast<uint32_t>(doc.actions.size()));
    for (const auto& [k, v] : doc.actions) { wRawStr(o, k); wU8(o, TAG_OBJ); writeAction(o, v); }

    wEnd(o);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void saveToBinary(const Mc3::Mc3Document& doc, std::ostream& out) {
    // Header: magic (4) + version (1) + flags (1) + reserved (2)
    out.write(MCB_MAGIC, 4);
    wU8(out, MCB_VERSION);
    wU8(out, 0); // no compression
    wU8(out, 0); wU8(out, 0); // reserved

    // Payload: document as root object
    wU8(out, TAG_OBJ);
    writeDocument(out, doc);
}

void saveToFile(const Mc3::Mc3Document& doc, const std::filesystem::path& path) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot open for writing: " + path.string());
    saveToBinary(doc, f);
    if (!f) throw std::runtime_error("Write error: " + path.string());
}

} // namespace MeshCraft::Mcb
