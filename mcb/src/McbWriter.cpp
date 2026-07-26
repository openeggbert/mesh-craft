#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3AtomicFileWriter.hpp"
#include "MeshCraft/Mc3/Mc3Camera.hpp"
#include "MeshCraft/Mc3/Mc3Environment.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3EmbedGltf.hpp"
#include "MeshCraft/Mc3/Mc3EventBinding.hpp"
#include "MeshCraft/Mc3/Mc3Music.hpp"
#include "MeshCraft/Mc3/Mc3SceneState.hpp"
#include "MeshCraft/Mc3/Mc3Script.hpp"
#include "MeshCraft/Mc3/Mc3Sound.hpp"
#include "MeshCraft/Mc3/Mc3Trigger.hpp"
#include "MeshCraft/Mc3/Mc3SvgTexture.hpp"
#include "MeshCraft/Mc3/Mc3Texture.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef MESHCRAFT_HAS_ZLIB
#include <zlib.h>
#endif

namespace MeshCraft::Mcb {

// ---------------------------------------------------------------------------
// Low-level write helpers
// ---------------------------------------------------------------------------

static void wU8(std::ostream& o, uint8_t v) { o.put(static_cast<char>(v)); }

static void wU32(std::ostream& o, uint32_t v) {
    char b[4];
    b[0] = static_cast<char>(v & 0xFF);
    b[1] = static_cast<char>((v >> 8) & 0xFF);
    b[2] = static_cast<char>((v >> 16) & 0xFF);
    b[3] = static_cast<char>((v >> 24) & 0xFF);
    o.write(b, 4);
}
static void wI32(std::ostream& o, int32_t v)  { wU32(o, static_cast<uint32_t>(v)); }
static void wF32(std::ostream& o, float v)     { uint32_t u; std::memcpy(&u,&v,4); wU32(o,u); }

static void wRawStr(std::ostream& o, const std::string& s) {
    wU32(o, static_cast<uint32_t>(s.size()));
    if (!s.empty()) o.write(s.data(), static_cast<std::streamsize>(s.size()));
}
static void wVec3(std::ostream& o, const std::array<float,3>& v) { wF32(o,v[0]); wF32(o,v[1]); wF32(o,v[2]); }
static void wVec4(std::ostream& o, const std::array<float,4>& v) { wF32(o,v[0]); wF32(o,v[1]); wF32(o,v[2]); wF32(o,v[3]); }

static void wKey(std::ostream& o, const char* k) {
    auto len = static_cast<uint8_t>(std::strlen(k));
    wU8(o, len);
    o.write(k, len);
}
static void wEnd(std::ostream& o) { wU8(o, 0); }

// Unconditional field writers
static void wFieldStr (std::ostream& o, const char* k, const std::string& v)         { wKey(o,k); wU8(o,TAG_STR);  wRawStr(o,v); }
static void wFieldF32 (std::ostream& o, const char* k, float v)                       { wKey(o,k); wU8(o,TAG_F32);  wF32(o,v); }
static void wFieldI32 (std::ostream& o, const char* k, int v)                         { wKey(o,k); wU8(o,TAG_I32);  wI32(o,v); }
static void wFieldBool(std::ostream& o, const char* k, bool v)                        { wKey(o,k); wU8(o,TAG_BOOL); wU8(o,v?1:0); }
static void wFieldVec3(std::ostream& o, const char* k, const std::array<float,3>& v) { wKey(o,k); wU8(o,TAG_VEC3); wVec3(o,v); }
static void wFieldVec4(std::ostream& o, const char* k, const std::array<float,4>& v) { wKey(o,k); wU8(o,TAG_VEC4); wVec4(o,v); }
static void wKeyObj   (std::ostream& o, const char* k)                                { wKey(o,k); wU8(o,TAG_OBJ); }
static void wKeyArr   (std::ostream& o, const char* k, uint32_t count)                { wKey(o,k); wU8(o,TAG_ARR); wU32(o,count); }
static void wKeyMap   (std::ostream& o, const char* k, uint32_t count)                { wKey(o,k); wU8(o,TAG_MAP); wU32(o,count); }

// Conditional field writers — only write when value differs from default
static void wIfStr (std::ostream& o, const char* k, const std::string& v, const char* def = "")         { if (v != def) wFieldStr(o,k,v); }
static void wIfF32 (std::ostream& o, const char* k, float v, float def)                                  { if (v != def) wFieldF32(o,k,v); }
static void wIfI32 (std::ostream& o, const char* k, int v, int def)                                      { if (v != def) wFieldI32(o,k,v); }
static void wIfBool(std::ostream& o, const char* k, bool v, bool def)                                    { if (v != def) wFieldBool(o,k,v); }
static void wIfVec3(std::ostream& o, const char* k, const std::array<float,3>& v, const std::array<float,3>& def) { if (v != def) wFieldVec3(o,k,v); }
static void wIfVec4(std::ostream& o, const char* k, const std::array<float,4>& v, const std::array<float,4>& def) { if (v != def) wFieldVec4(o,k,v); }

// ---------------------------------------------------------------------------
// Mc3 type serializers — only non-default fields are written
// ---------------------------------------------------------------------------

static constexpr std::array<float,3> kZero3{0.0f,0.0f,0.0f};
static constexpr std::array<float,3> kOne3 {1.0f,1.0f,1.0f};

static void writeTransform(std::ostream& o, const Mc3::Mc3Transform& tf) {
    wIfVec3(o, "position", tf.position, kZero3);
    wIfVec3(o, "rotation", tf.rotation, kZero3);
    wIfVec3(o, "scale",    tf.scale,    kOne3);
    wIfVec3(o, "pivot",    tf.pivot,    kZero3);
    wEnd(o);
}

static void writePrimitive(std::ostream& o, const Mc3::Mc3Primitive& p) {
    const Mc3::Mc3Primitive def;
    wIfI32 (o, "primitiveType", static_cast<int>(p.primitiveType), static_cast<int>(def.primitiveType));
    wIfVec3(o, "size",          p.size,          def.size);
    wIfF32 (o, "radius",        p.radius,        def.radius);
    wIfF32 (o, "height",        p.height,        def.height);
    wIfI32 (o, "segments",      p.segments,      def.segments);
    wIfStr (o, "axis",          p.axis,          def.axis.c_str());
    wIfF32 (o, "majorRadius",   p.majorRadius,   def.majorRadius);
    wIfF32 (o, "minorRadius",   p.minorRadius,   def.minorRadius);
    wIfI32 (o, "subdivisionsX", p.subdivisionsX, def.subdivisionsX);
    wIfI32 (o, "subdivisionsZ", p.subdivisionsZ, def.subdivisionsZ);
    wEnd(o);
}

static void writeDeform(std::ostream& o, const Mc3::Mc3Deform& d) {
    wIfVec3(o, "scale", d.scale, kOne3);
    wEnd(o);
}

static void writeCsgOp(std::ostream& o, const Mc3::Mc3CsgOperation& csg) {
    const Mc3::Mc3CsgOperation def;
    wIfI32(o, "csgType", static_cast<int>(csg.csgType), static_cast<int>(def.csgType));
    wEnd(o);
}

static void writeCrossSection(std::ostream& o, const Mc3::Mc3CrossSection& cs) {
    const Mc3::Mc3CrossSection def;
    wIfI32(o, "type",        static_cast<int>(cs.type), static_cast<int>(def.type));
    wIfF32(o, "width",       cs.width,       def.width);
    wIfF32(o, "height",      cs.height,      def.height);
    wIfF32(o, "radius",      cs.radius,      def.radius);
    wIfF32(o, "innerRadius", cs.innerRadius, def.innerRadius);
    wIfI32(o, "sides",       cs.sides,       def.sides);
    wIfI32(o, "segments",    cs.segments,    def.segments);
    if (!cs.customPoints.empty()) {
        wKeyArr(o, "customPoints", static_cast<uint32_t>(cs.customPoints.size()));
        for (const auto& pt : cs.customPoints) {
            wU8(o, TAG_VEC3);
            wF32(o, pt.x); wF32(o, pt.y); wF32(o, 0.0f);
        }
    }
    wEnd(o);
}

static void writePathPoint(std::ostream& o, const Mc3::Mc3PathPoint& pp) {
    wIfVec3(o, "position",  pp.position,  kZero3);
    wIfVec3(o, "controlIn", pp.controlIn, kZero3);
    wEnd(o);
}

static void writePath(std::ostream& o, const Mc3::Mc3ExtrudePath& p) {
    const Mc3::Mc3ExtrudePath def;
    wIfI32(o, "type",        static_cast<int>(p.type), static_cast<int>(def.type));
    wIfF32(o, "length",      p.length,      def.length);
    wIfStr(o, "axis",        p.axis,        def.axis.c_str());
    wIfF32(o, "arcRadius",   p.arcRadius,   def.arcRadius);
    wIfF32(o, "arcAngle",    p.arcAngle,    def.arcAngle);
    wIfF32(o, "helixRadius", p.helixRadius, def.helixRadius);
    wIfF32(o, "helixHeight", p.helixHeight, def.helixHeight);
    wIfF32(o, "helixTurns",  p.helixTurns,  def.helixTurns);
    if (!p.points.empty()) {
        wKeyArr(o, "points", static_cast<uint32_t>(p.points.size()));
        for (const auto& pt : p.points) { wU8(o, TAG_OBJ); writePathPoint(o, pt); }
    }
    wEnd(o);
}

static void writeExtrude(std::ostream& o, const Mc3::Mc3Extrude& ex) {
    const Mc3::Mc3Extrude def;
    // Only write crossSection if non-default
    const Mc3::Mc3CrossSection csdef;
    if (ex.crossSection.type        != csdef.type       ||
        ex.crossSection.width       != csdef.width      ||
        ex.crossSection.height      != csdef.height     ||
        ex.crossSection.radius      != csdef.radius     ||
        ex.crossSection.innerRadius != csdef.innerRadius||
        ex.crossSection.sides       != csdef.sides      ||
        ex.crossSection.segments    != csdef.segments   ||
        !ex.crossSection.customPoints.empty()) {
        wKeyObj(o, "crossSection"); writeCrossSection(o, ex.crossSection);
    }
    // Only write path if non-default
    const Mc3::Mc3ExtrudePath pdef;
    if (ex.path.type        != pdef.type      ||
        ex.path.length      != pdef.length    ||
        ex.path.axis        != pdef.axis      ||
        ex.path.arcRadius   != pdef.arcRadius ||
        ex.path.arcAngle    != pdef.arcAngle  ||
        ex.path.helixRadius != pdef.helixRadius||
        ex.path.helixHeight != pdef.helixHeight||
        ex.path.helixTurns  != pdef.helixTurns ||
        !ex.path.points.empty()) {
        wKeyObj(o, "path"); writePath(o, ex.path);
    }
    wIfF32 (o, "twist",    ex.twist,    def.twist);
    wIfI32 (o, "segments", ex.segments, def.segments);
    wIfBool(o, "smooth",   ex.smooth,   def.smooth);
    wIfBool(o, "caps",     ex.caps,     def.caps);
    wEnd(o);
}

static void writeUvMapping(std::ostream& o, const Mc3::Mc3UvMapping& uv) {
    const Mc3::Mc3UvMapping def;
    wIfI32(o, "projection", static_cast<int>(uv.projection), static_cast<int>(def.projection));
    wIfF32(o, "scaleU",     uv.scaleU,   def.scaleU);
    wIfF32(o, "scaleV",     uv.scaleV,   def.scaleV);
    wIfF32(o, "offsetU",    uv.offsetU,  def.offsetU);
    wIfF32(o, "offsetV",    uv.offsetV,  def.offsetV);
    wIfF32(o, "rotation",   uv.rotation, def.rotation);
    wEnd(o);
}

static void writeAssetMetadata(std::ostream& o, const Mc3::Mc3AssetMetadata& am) {
    const Mc3::Mc3AssetMetadata def;
    // Key names deliberately match the XML/JSON wire attribute names (not
    // always the literal C++ field name, e.g. "maxVisibilityDistance" not
    // "maxVisibilityDistanceM") so all three formats agree on one wire
    // vocabulary for this struct -- see field_matrix.py / SYS-W1-04.
    wIfStr (o, "category",           am.category,           "");
    wIfStr (o, "subcategory",        am.subcategory,        "");
    wIfStr (o, "facing",             am.facing,             "");
    wIfStr (o, "collisionProxy",     am.collisionProxy,     "");
    wIfStr (o, "shadowPolicy",       am.shadowPolicy,       "");
    wIfStr (o, "license",            am.license,            "");
    wIfStr (o, "provenance",         am.provenance,         "");
    wIfStr (o, "source",             am.sourceGeneratorOrHash, "");
    wIfStr (o, "version",            am.semanticVersion,    "");
    wIfBool(o, "instancingEligible", am.instancingEligible, def.instancingEligible);
    wIfF32 (o, "maxVisibilityDistance", am.maxVisibilityDistanceM, def.maxVisibilityDistanceM);
    wIfF32 (o, "selectionWeight",    am.selectionWeight,    def.selectionWeight);
    wIfVec3(o, "nominalSize",        am.nominalSize,        kZero3);
    wIfVec3(o, "boundsMin",          am.boundsMin,          kZero3);
    wIfVec3(o, "boundsMax",          am.boundsMax,          kZero3);
    wIfVec3(o, "clearanceVolume",    am.clearanceVolume,    kZero3);

    auto writeTagList = [&](const char* key, const std::vector<std::string>& tags) {
        if (tags.empty()) return;
        wKeyArr(o, key, static_cast<uint32_t>(tags.size()));
        for (const auto& t : tags) { wU8(o, TAG_STR); wRawStr(o, t); }
    };
    writeTagList("semanticTags",  am.semanticTags);
    writeTagList("styleTags",     am.styleTags);
    writeTagList("regionTags",    am.regionTags);
    writeTagList("periodTags",    am.periodTags);
    writeTagList("materialSlots", am.materialSlots);

    if (!am.sockets.empty()) {
        wKeyMap(o, "sockets", static_cast<uint32_t>(am.sockets.size()));
        for (const auto& [name, pos] : am.sockets) { wRawStr(o, name); wU8(o, TAG_VEC3); wVec3(o, pos); }
    }
    if (!am.lods.empty()) {
        // "tier" is a native map key here (Mc3AssetMetadata::lods), not a
        // separate field -- same shape as the allowlisted "key" entries.
        wKeyMap(o, "lods", static_cast<uint32_t>(am.lods.size()));
        for (const auto& [tier, defId] : am.lods) { wRawStr(o, tier); wU8(o, TAG_STR); wRawStr(o, defId); }
    }
    wEnd(o);
}

static void writeObjectState(std::ostream& o, const Mc3::Mc3ObjectState& st) {
    if (st.position) wFieldVec3(o, "position", *st.position);
    if (st.rotation) wFieldVec3(o, "rotation", *st.rotation);
    if (st.scale)    wFieldVec3(o, "scale",    *st.scale);
    if (st.visible)  wFieldBool(o, "visible",  *st.visible);
    if (st.material) wFieldStr (o, "material", *st.material);
    wEnd(o);
}

static void writeObject(std::ostream& o, const Mc3::Mc3Object& obj);

static void writeObject(std::ostream& o, const Mc3::Mc3Object& obj) {
    const Mc3::Mc3Object def;
    wIfI32 (o, "type",     static_cast<int>(obj.type), static_cast<int>(def.type));
    wIfStr (o, "name",     obj.name,     "");
    wIfStr (o, "id",       obj.id,       "");
    wIfStr (o, "material", obj.material, "");
    wIfBool(o, "visible",  obj.visible,  def.visible);
    wIfStr (o, "collision",obj.collision,def.collision.c_str());
    if (!obj.layer.empty())            wFieldStr (o, "layer",            obj.layer);
    if (obj.isCutter)                  wFieldBool(o, "isCutter",         obj.isCutter);
    if (!obj.definition.empty())       wFieldStr (o, "definition",       obj.definition);
    if (!obj.meshSource.empty())       wFieldStr (o, "meshSource",       obj.meshSource);
    if (!obj.materialOverride.empty()) wFieldStr (o, "materialOverride", obj.materialOverride);
    // R103: MCB/XML/JSON all agree on the wire name "script" (not the C++
    // field name "scriptId") -- see field_matrix.py's "script" allowlist entry.
    if (!obj.scriptId.empty())         wFieldStr (o, "script",           obj.scriptId);

    // Transform — only write if non-default
    const Mc3::Mc3Transform tdef;
    if (obj.transform.position != tdef.position ||
        obj.transform.rotation != tdef.rotation ||
        obj.transform.scale    != tdef.scale    ||
        obj.transform.pivot    != tdef.pivot) {
        wKeyObj(o, "transform"); writeTransform(o, obj.transform);
    }

    if (obj.deform)       { wKeyObj(o, "deform");       writeDeform(o, *obj.deform); }
    if (obj.primitive)    { wKeyObj(o, "primitive");    writePrimitive(o, *obj.primitive); }
    if (obj.csgOperation) { wKeyObj(o, "csgOperation"); writeCsgOp(o, *obj.csgOperation); }
    if (obj.extrude)      { wKeyObj(o, "extrude");      writeExtrude(o, *obj.extrude); }
    if (obj.uvMapping)    { wKeyObj(o, "uvMapping");    writeUvMapping(o, *obj.uvMapping); }
    if (obj.assetMetadata) { wKeyObj(o, "assetMetadata"); writeAssetMetadata(o, *obj.assetMetadata); }

    if (!obj.tags.empty()) {
        wKeyArr(o, "tags", static_cast<uint32_t>(obj.tags.size()));
        for (const auto& t : obj.tags) { wU8(o, TAG_STR); wRawStr(o, t); }
    }
    if (!obj.variantDefinitions.empty()) {
        wKeyArr(o, "variantDefs", static_cast<uint32_t>(obj.variantDefinitions.size()));
        for (const auto& d : obj.variantDefinitions) { wU8(o, TAG_STR); wRawStr(o, d); }
    }
    if (!obj.metadata.empty()) {
        wKeyMap(o, "metadata", static_cast<uint32_t>(obj.metadata.size()));
        for (const auto& [k, v] : obj.metadata) { wRawStr(o, k); wU8(o, TAG_STR); wRawStr(o, v); }
    }
    if (!obj.states.empty()) {
        wKeyMap(o, "states", static_cast<uint32_t>(obj.states.size()));
        for (const auto& [k, v] : obj.states) { wRawStr(o, k); wU8(o, TAG_OBJ); writeObjectState(o, v); }
    }
    if (!obj.children.empty()) {
        wKeyArr(o, "children", static_cast<uint32_t>(obj.children.size()));
        for (const auto& c : obj.children) { wU8(o, TAG_OBJ); writeObject(o, *c); }
    }
    wEnd(o);
}

static void writeTexture(std::ostream& o, const Mc3::Mc3Texture& tex) {
    const Mc3::Mc3Texture def;
    wIfStr(o, "name",       tex.name,       "");
    wIfStr(o, "uri",        tex.uri,        "");
    wIfStr(o, "wrapU",      tex.wrapU,      def.wrapU.c_str());
    wIfStr(o, "wrapV",      tex.wrapV,      def.wrapV.c_str());
    wIfStr(o, "filter",     tex.filter,     def.filter.c_str());
    wIfStr(o, "colorSpace", tex.colorSpace, def.colorSpace.c_str());
    wIfBool(o, "mipMaps",   tex.mipMaps,    def.mipMaps);
    wEnd(o);
}

static void writeSvgTexture(std::ostream& o, const Mc3::Mc3SvgTexture& svg) {
    wIfStr(o, "src",           svg.src,           "");
    wIfStr(o, "inlineContent", svg.inlineContent, "");
    wEnd(o);
}

static void writeEmbed(std::ostream& o, const Mc3::Mc3EmbedGltf& em) {
    wIfStr(o, "src",           em.src,           "");
    wIfStr(o, "base64Content", em.base64Content, "");
    wEnd(o);
}

static void writeScript(std::ostream& o, const Mc3::Mc3Script& sc) {
    wIfStr(o, "type",   sc.type,   "");
    wIfStr(o, "source", sc.source, "");
    wEnd(o);
}

static void writeSound(std::ostream& o, const Mc3::Mc3Sound& snd) {
    wIfStr (o, "src",  snd.src,  "");
    wIfBool(o, "loop", snd.loop, false);
    wEnd(o);
}

static void writeMusic(std::ostream& o, const Mc3::Mc3Music& mus) {
    wIfStr (o, "src",  mus.src,  "");
    wIfBool(o, "loop", mus.loop, true);
    wEnd(o);
}

static void writeObjectOverride(std::ostream& o, const Mc3::Mc3ObjectOverride& ovr) {
    wIfStr(o, "id", ovr.id, "");
    if (ovr.visible.has_value())  wFieldBool(o, "visible",  *ovr.visible);
    if (ovr.position.has_value()) wFieldVec3(o, "position", *ovr.position);
    if (ovr.rotation.has_value()) wFieldVec3(o, "rotation", *ovr.rotation);
    if (ovr.material.has_value()) wFieldStr(o,  "material", *ovr.material);
    wEnd(o);
}

static void writeSceneState(std::ostream& o, const Mc3::Mc3SceneState& state) {
    if (!state.overrides.empty()) {
        wKeyArr(o, "overrides", static_cast<uint32_t>(state.overrides.size()));
        for (const auto& ovr : state.overrides) { wU8(o, TAG_OBJ); writeObjectOverride(o, ovr); }
    }
    wEnd(o);
}

static const char* triggerStepTypeStr(Mc3::TriggerStepType t) {
    switch (t) {
    case Mc3::TriggerStepType::PlayAction: return "play-action";
    case Mc3::TriggerStepType::PlaySound:  return "play-sound";
    case Mc3::TriggerStepType::RunScript:  return "run-script";
    case Mc3::TriggerStepType::PlayMusic:  return "play-music";
    }
    return "";
}

static void writeTrigger(std::ostream& o, const Mc3::Mc3Trigger& trig) {
    if (!trig.steps.empty()) {
        wKeyArr(o, "steps", static_cast<uint32_t>(trig.steps.size()));
        for (const auto& step : trig.steps) {
            wU8(o, TAG_OBJ);
            wIfStr(o, "type", triggerStepTypeStr(step.type), "");
            wIfStr(o, "ref",  step.ref,                      "");
            wEnd(o);
        }
    }
    wEnd(o);
}

static const char* eventBindingEventStr(Mc3::EventBindingEvent event) {
    switch (event) {
    case Mc3::EventBindingEvent::Enter: return "enter";
    case Mc3::EventBindingEvent::Exit:  return "exit";
    case Mc3::EventBindingEvent::Click: return "click";
    case Mc3::EventBindingEvent::Timer: return "timer";
    }
    return "enter";
}

static const char* eventBindingTargetStr(Mc3::EventBindingTarget target) {
    return target == Mc3::EventBindingTarget::SceneState ? "state" : "trigger";
}

static void writeEventBinding(std::ostream& o, const Mc3::Mc3EventBinding& binding) {
    const Mc3::Mc3EventBinding def;
    wIfStr(o, "id", binding.id, "");
    wIfStr(o, "source", binding.sourceObjectId, "");
    wIfStr(o, "event", eventBindingEventStr(binding.event), "enter");
    wIfStr(o, "targetType", eventBindingTargetStr(binding.targetType), "trigger");
    wIfStr(o, "target", binding.targetId, "");
    wIfBool(o, "enabled", binding.enabled, def.enabled);
    wIfF32(o, "cooldown", binding.cooldown, def.cooldown);
    wIfBool(o, "once", binding.once, def.once);
    wIfF32(o, "interval", binding.interval, def.interval);
    wEnd(o);
}

static void writeMaterial(std::ostream& o, const Mc3::Mc3Material& m) {
    const Mc3::Mc3Material def;
    wIfStr (o, "name",                     m.name,                     "");
    wIfVec4(o, "baseColor",                m.baseColor,                def.baseColor);
    wIfStr (o, "baseColorTexture",         m.baseColorTexture,         "");
    wIfStr (o, "normalTexture",            m.normalTexture,            "");
    wIfStr (o, "emissiveTexture",          m.emissiveTexture,          "");
    wIfStr (o, "metallicRoughnessTexture", m.metallicRoughnessTexture, "");
    wIfStr (o, "occlusionTexture",         m.occlusionTexture,         "");
    wIfF32 (o, "roughness",                m.roughness,                def.roughness);
    wIfF32 (o, "metallic",                 m.metallic,                 def.metallic);
    wIfF32 (o, "normalScale",              m.normalScale,              def.normalScale);
    wIfF32 (o, "occlusionStrength",        m.occlusionStrength,        def.occlusionStrength);
    wIfVec3(o, "emissiveColor",            m.emissiveColor,            def.emissiveColor);
    wIfStr (o, "alphaMode",                m.alphaMode,                def.alphaMode.c_str());
    wIfF32 (o, "alphaCutoff",              m.alphaCutoff,              def.alphaCutoff);
    wIfBool(o, "doubleSided",              m.doubleSided,              def.doubleSided);
    wEnd(o);
}

static void writeLight(std::ostream& o, const Mc3::Mc3Light& lt) {
    const Mc3::Mc3Light def;
    wIfI32 (o, "type",        static_cast<int>(lt.type), static_cast<int>(def.type));
    wIfStr (o, "name",        lt.name,        "");
    wIfVec3(o, "color",       lt.color,       def.color);
    wIfF32 (o, "brightness",  lt.brightness,  def.brightness);
    wIfVec3(o, "direction",   lt.direction,   def.direction);
    wIfVec3(o, "position",    lt.position,    kZero3);
    wIfF32 (o, "range",       lt.range,       def.range);
    wIfF32 (o, "angle",       lt.angle,       def.angle);
    wIfF32 (o, "falloff",     lt.falloff,     def.falloff);
    wIfBool(o, "castShadows", lt.castShadows, def.castShadows);
    wEnd(o);
}

static void writeCamera(std::ostream& o, const Mc3::Mc3Camera& cam) {
    const Mc3::Mc3Camera def;
    wIfStr (o, "name",      cam.name,      "");
    wIfI32 (o, "type",      static_cast<int>(cam.type), static_cast<int>(def.type));
    wIfVec3(o, "position",  cam.position,  def.position);
    wIfVec3(o, "target",    cam.target,    def.target);
    if (cam.rotation) wFieldVec3(o, "rotation", *cam.rotation);
    wIfF32 (o, "nearPlane", cam.nearPlane, def.nearPlane);
    wIfF32 (o, "farPlane",  cam.farPlane,  def.farPlane);
    wIfF32 (o, "fov",       cam.fov,       def.fov);
    wIfF32 (o, "orthoSize", cam.orthoSize, def.orthoSize);
    wIfF32 (o, "orthoAspect", cam.orthoAspect, def.orthoAspect);   // STAB-0695
    wEnd(o);
}

static void writeFog(std::ostream& o, const Mc3::Mc3Fog& fog) {
    const Mc3::Mc3Fog def;
    wIfVec3(o, "color",   fog.color,   def.color);
    wIfI32 (o, "mode",    static_cast<int>(fog.mode), static_cast<int>(def.mode));
    wIfF32 (o, "start",   fog.start,   def.start);
    wIfF32 (o, "end",     fog.end,     def.end);
    wIfF32 (o, "density", fog.density, def.density);
    wEnd(o);
}

static void writeEnvironment(std::ostream& o, const Mc3::Mc3Environment& env) {
    const Mc3::Mc3Environment def;
    wIfVec3(o, "backgroundColor",  env.backgroundColor,  def.backgroundColor);
    wIfStr (o, "backgroundTexture", env.backgroundTexture, "");
    wIfStr (o, "skyboxTexture",    env.skyboxTexture,     "");
    if (env.fog) { wKeyObj(o, "fog"); writeFog(o, *env.fog); }
    wEnd(o);
}

static void writeKeyframe(std::ostream& o, const Mc3::Mc3Keyframe& kf) {
    const Mc3::Mc3Keyframe def;
    wIfF32(o, "time",          kf.time,                           def.time);
    wFieldF32(o, "value",      kf.value); // always write — semantically meaningful even at 0
    wIfI32(o, "interpolation", static_cast<int>(kf.interpolation), static_cast<int>(def.interpolation));
    wIfF32(o, "leftDt",        kf.handleLeft.dt,  0.0f);
    wIfF32(o, "leftDv",        kf.handleLeft.dv,  0.0f);
    wIfF32(o, "rightDt",       kf.handleRight.dt, 0.0f);
    wIfF32(o, "rightDv",       kf.handleRight.dv, 0.0f);
    wEnd(o);
}

static void writeChannel(std::ostream& o, const Mc3::Mc3Channel& ch) {
    wIfStr(o, "targetObject", ch.targetObject, "");
    wFieldI32(o, "property", static_cast<int>(ch.property)); // always write
    if (!ch.keyframes.empty()) {
        wKeyArr(o, "keyframes", static_cast<uint32_t>(ch.keyframes.size()));
        for (const auto& kf : ch.keyframes) { wU8(o, TAG_OBJ); writeKeyframe(o, kf); }
    }
    wEnd(o);
}

static void writeActionClip(std::ostream& o, const Mc3::Mc3ActionClip& clip) {
    const Mc3::Mc3ActionClip def;
    wIfStr(o, "name",               clip.name,               "");
    wIfF32(o, "startTime",          clip.startTime,          def.startTime);
    wIfF32(o, "endTime",            clip.endTime,            def.endTime);
    wIfF32(o, "playbackRate",       clip.playbackRate,       def.playbackRate);
    wIfBool(o, "loop",              clip.loop,               def.loop);
    wIfBool(o, "reverse",           clip.reverse,            def.reverse);
    wIfF32(o, "transitionDuration", clip.transitionDuration, def.transitionDuration);
    wEnd(o);
}

static void writeAction(std::ostream& o, const Mc3::Mc3Action& act) {
    const Mc3::Mc3Action def;
    wIfStr (o, "name",      act.name,      "");
    wIfF32 (o, "duration",  act.duration,  def.duration);
    wIfBool(o, "loop",      act.loop,      def.loop);
    wIfBool(o, "autoplay",  act.autoplay,  def.autoplay);
    wIfF32 (o, "timeScale", act.timeScale, def.timeScale); // STAB-0460
    if (!act.clips.empty()) {
        wKeyArr(o, "clips", static_cast<uint32_t>(act.clips.size()));
        for (const auto& clip : act.clips) { wU8(o, TAG_OBJ); writeActionClip(o, clip); }
    }
    if (!act.channels.empty()) {
        wKeyArr(o, "channels", static_cast<uint32_t>(act.channels.size()));
        for (const auto& ch : act.channels) { wU8(o, TAG_OBJ); writeChannel(o, ch); }
    }
    wEnd(o);
}

static void writeLibraryInfo(std::ostream& o, const Mc3::Mc3LibraryInfo& lib) {
    wIfStr(o, "namespace", lib.libraryNamespace, "");
    wIfStr(o, "version",   lib.version,          "");
    wIfStr(o, "hash",      lib.contentHash,      "");
    wEnd(o);
}

static void writeImport(std::ostream& o, const Mc3::Mc3Import& imp) {
    wIfStr(o, "namespace", imp.importNamespace, "");
    wIfStr(o, "source",    imp.source,          "");
    wIfStr(o, "hash",      imp.hash,            "");
    wEnd(o);
}

static void writeDocument(std::ostream& o, const Mc3::Mc3Document& doc) {
    const Mc3::Mc3Document def;
    wIfStr(o, "version",          doc.version,          def.version.c_str());
    wIfStr(o, "model",            doc.model,            "");
    wIfStr(o, "unit",             doc.unit,             def.unit.c_str());
    wIfStr(o, "coordinateSystem", doc.coordinateSystem, def.coordinateSystem.c_str());
    wIfStr(o, "rotationUnits",    doc.rotationUnits,    def.rotationUnits.c_str());
    wIfStr(o, "eulerOrder",       doc.eulerOrder,       def.eulerOrder.c_str());
    wIfStr(o, "defaultCamera",    doc.defaultCamera,    "");

    // R110/R101: library identity + imports (mc3lib documents / consumers).
    if (doc.library) { wKeyObj(o, "library"); writeLibraryInfo(o, *doc.library); }
    if (!doc.imports.empty()) {
        wKeyArr(o, "imports", static_cast<uint32_t>(doc.imports.size()));
        for (const auto& imp : doc.imports) { wU8(o, TAG_OBJ); writeImport(o, imp); }
    }

    if (!doc.meta.empty()) {
        wKeyMap(o, "meta", static_cast<uint32_t>(doc.meta.size()));
        for (const auto& [k, v] : doc.meta) { wRawStr(o, k); wU8(o, TAG_STR); wRawStr(o, v); }
    }
    // STAB-0131/0144: doc.includes, its skip-sets, and the legacy doc.metadata
    // map were never written at all — an MCB roundtrip of a scene that used
    // <include> silently lost the include structure (everything the included
    // file contributed would get inlined on the next XML save instead of
    // staying in the referenced library file).
    if (!doc.metadata.empty()) {
        wKeyMap(o, "metadata", static_cast<uint32_t>(doc.metadata.size()));
        for (const auto& [k, v] : doc.metadata) { wRawStr(o, k); wU8(o, TAG_STR); wRawStr(o, v); }
    }
    if (!doc.includes.empty()) {
        wKeyArr(o, "includes", static_cast<uint32_t>(doc.includes.size()));
        for (const auto& inc : doc.includes) { wU8(o, TAG_STR); wRawStr(o, inc); }
    }
    if (!doc.includedDefs.empty()) {
        wKeyArr(o, "includedDefs", static_cast<uint32_t>(doc.includedDefs.size()));
        for (const auto& id : doc.includedDefs) { wU8(o, TAG_STR); wRawStr(o, id); }
    }
    if (!doc.includedMaterials.empty()) {
        wKeyArr(o, "includedMaterials", static_cast<uint32_t>(doc.includedMaterials.size()));
        for (const auto& id : doc.includedMaterials) { wU8(o, TAG_STR); wRawStr(o, id); }
    }
    if (!doc.includedTextures.empty()) {
        wKeyArr(o, "includedTextures", static_cast<uint32_t>(doc.includedTextures.size()));
        for (const auto& id : doc.includedTextures) { wU8(o, TAG_STR); wRawStr(o, id); }
    }
    if (!doc.includedEmbeds.empty()) {
        wKeyArr(o, "includedEmbeds", static_cast<uint32_t>(doc.includedEmbeds.size()));
        for (const auto& id : doc.includedEmbeds) { wU8(o, TAG_STR); wRawStr(o, id); }
    }

    if (doc.environment) { wKeyObj(o, "environment"); writeEnvironment(o, *doc.environment); }

    if (!doc.lights.empty()) {
        wKeyArr(o, "lights", static_cast<uint32_t>(doc.lights.size()));
        for (const auto& l : doc.lights) { wU8(o, TAG_OBJ); writeLight(o, l); }
    }
    if (!doc.cameras.empty()) {
        wKeyArr(o, "cameras", static_cast<uint32_t>(doc.cameras.size()));
        for (const auto& c : doc.cameras) { wU8(o, TAG_OBJ); writeCamera(o, c); }
    }

    if (!doc.textures.empty()) {
        wKeyMap(o, "textures", static_cast<uint32_t>(doc.textures.size()));
        for (const auto& [k, v] : doc.textures) { wRawStr(o, k); wU8(o, TAG_OBJ); writeTexture(o, v); }
    }
    if (!doc.svgTextures.empty()) {
        wKeyMap(o, "svgTextures", static_cast<uint32_t>(doc.svgTextures.size()));
        for (const auto& [k, v] : doc.svgTextures) { wRawStr(o, k); wU8(o, TAG_OBJ); writeSvgTexture(o, v); }
    }
    if (!doc.embeds.empty()) {
        wKeyMap(o, "embeds", static_cast<uint32_t>(doc.embeds.size()));
        for (const auto& [k, v] : doc.embeds) { wRawStr(o, k); wU8(o, TAG_OBJ); writeEmbed(o, v); }
    }
    if (!doc.scripts.empty()) {
        wKeyMap(o, "scripts", static_cast<uint32_t>(doc.scripts.size()));
        for (const auto& [k, v] : doc.scripts) { wRawStr(o, k); wU8(o, TAG_OBJ); writeScript(o, v); }
    }
    if (!doc.sounds.empty()) {
        wKeyMap(o, "sounds", static_cast<uint32_t>(doc.sounds.size()));
        for (const auto& [k, v] : doc.sounds) { wRawStr(o, k); wU8(o, TAG_OBJ); writeSound(o, v); }
    }
    if (!doc.musicTracks.empty()) {
        wKeyMap(o, "musicTracks", static_cast<uint32_t>(doc.musicTracks.size()));
        for (const auto& [k, v] : doc.musicTracks) { wRawStr(o, k); wU8(o, TAG_OBJ); writeMusic(o, v); }
    }
    if (!doc.triggers.empty()) {
        wKeyMap(o, "triggers", static_cast<uint32_t>(doc.triggers.size()));
        for (const auto& [k, v] : doc.triggers) { wRawStr(o, k); wU8(o, TAG_OBJ); writeTrigger(o, v); }
    }
    if (!doc.sceneStates.empty()) {
        wKeyMap(o, "sceneStates", static_cast<uint32_t>(doc.sceneStates.size()));
        for (const auto& [k, v] : doc.sceneStates) { wRawStr(o, k); wU8(o, TAG_OBJ); writeSceneState(o, v); }
    }
    if (!doc.eventBindings.empty()) {
        wKeyArr(o, "eventBindings", static_cast<uint32_t>(doc.eventBindings.size()));
        for (const auto& binding : doc.eventBindings) { wU8(o, TAG_OBJ); writeEventBinding(o, binding); }
    }
    if (!doc.materials.empty()) {
        wKeyMap(o, "materials", static_cast<uint32_t>(doc.materials.size()));
        for (const auto& [k, v] : doc.materials) { wRawStr(o, k); wU8(o, TAG_OBJ); writeMaterial(o, v); }
    }
    if (!doc.definitions.empty()) {
        wKeyMap(o, "definitions", static_cast<uint32_t>(doc.definitions.size()));
        for (const auto& [k, v] : doc.definitions) { wRawStr(o, k); wU8(o, TAG_OBJ); writeObject(o, *v); }
    }
    if (!doc.objects.empty()) {
        wKeyArr(o, "objects", static_cast<uint32_t>(doc.objects.size()));
        for (const auto& obj : doc.objects) { wU8(o, TAG_OBJ); writeObject(o, *obj); }
    }
    if (!doc.actions.empty()) {
        wKeyMap(o, "actions", static_cast<uint32_t>(doc.actions.size()));
        for (const auto& [k, v] : doc.actions) { wRawStr(o, k); wU8(o, TAG_OBJ); writeAction(o, v); }
    }
    wEnd(o);
}

// ---------------------------------------------------------------------------
// SYS-W14-25: zlib (deflate) compression of the document payload
// ---------------------------------------------------------------------------

#ifdef MESHCRAFT_HAS_ZLIB
// One-shot in-memory compress via zlib's compress2() -- the whole document
// payload is already fully materialized in `raw` (a legitimate mc3 scene is
// at most tens of MB, never a streaming-scale workload), so the simpler
// one-shot API is appropriate here, not the incremental deflate()/inflate()
// streaming loop zlib also offers.
static std::vector<uint8_t> zlibCompress(const std::string& raw) {
    uLongf destLen = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> dest(destLen);
    int rc = compress2(dest.data(), &destLen,
                       reinterpret_cast<const Bytef*>(raw.data()),
                       static_cast<uLong>(raw.size()), Z_BEST_COMPRESSION);
    if (rc != Z_OK)
        throw std::runtime_error("MCB: zlib compression failed (code " + std::to_string(rc) + ")");
    dest.resize(destLen);
    return dest;
}
#endif

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void saveToBinary(const Mc3::Mc3Document& doc, std::ostream& out, bool compress) {
    out.write(MCB_MAGIC, 4);
    wU8(out, MCB_VERSION);

    if (compress) {
#ifdef MESHCRAFT_HAS_ZLIB
        std::ostringstream payload(std::ios::binary);
        wU8(payload, TAG_OBJ);
        writeDocument(payload, doc);
        const std::string raw = payload.str();

        const std::vector<uint8_t> compressed = zlibCompress(raw);

        wU8(out, MCB_FLAG_COMPRESSED);
        wU8(out, 0); wU8(out, 0); // reserved
        wU32(out, static_cast<uint32_t>(raw.size()));        // uncompressed payload size
        wU32(out, static_cast<uint32_t>(compressed.size())); // compressed payload size
        out.write(reinterpret_cast<const char*>(compressed.data()),
                  static_cast<std::streamsize>(compressed.size()));
        return;
#else
        throw std::runtime_error(
            "MCB: compression requested but this build was compiled without zlib support");
#endif
    }

    wU8(out, 0); // no compression
    wU8(out, 0); wU8(out, 0); // reserved
    wU8(out, TAG_OBJ);
    writeDocument(out, doc);
}

void saveToFile(const Mc3::Mc3Document& doc, const std::filesystem::path& path, bool compress) {
    // AUDIT-0019/SYS-W9-06: same atomic write-then-replace primitive as
    // Mc3XmlWriter::write() so a crash/disk-full/permission failure mid-write
    // can never leave a truncated or corrupt file at `path`.
    Mc3::writeFileAtomically(path, [&](const std::filesystem::path& tmpPath) {
        std::ofstream f(tmpPath, std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("Cannot open for writing: " + tmpPath.string());
        saveToBinary(doc, f, compress);
        if (!f) throw std::runtime_error("Write error: " + path.string());
    });
}

} // namespace MeshCraft::Mcb
