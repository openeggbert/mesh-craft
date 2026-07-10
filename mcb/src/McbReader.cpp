#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Camera.hpp"
#include "MeshCraft/Mc3/Mc3Environment.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3EmbedGltf.hpp"
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
#include <istream>
#include <memory>
#include <stdexcept>
#include <string>

namespace MeshCraft::Mcb {

// ---------------------------------------------------------------------------
// Low-level read helpers
// ---------------------------------------------------------------------------

static uint8_t rU8(std::istream& in) {
    unsigned char b;
    if (!in.read(reinterpret_cast<char*>(&b), 1))
        throw std::runtime_error("MCB: unexpected end of stream");
    return b;
}

static uint32_t rU32(std::istream& in) {
    unsigned char b[4];
    if (!in.read(reinterpret_cast<char*>(b), 4))
        throw std::runtime_error("MCB: unexpected end of stream");
    return static_cast<uint32_t>(b[0])
         | (static_cast<uint32_t>(b[1]) << 8)
         | (static_cast<uint32_t>(b[2]) << 16)
         | (static_cast<uint32_t>(b[3]) << 24);
}

static int32_t rI32(std::istream& in) {
    return static_cast<int32_t>(rU32(in));
}

static float rF32(std::istream& in) {
    uint32_t u = rU32(in);
    float v;
    std::memcpy(&v, &u, 4);
    return v;
}

// A claimed length is validated against a generous sanity ceiling before
// allocating — not just against "did the read succeed" after the fact.
// std::string's fill-constructor actually commits (zero-writes) len bytes
// immediately, unlike e.g. vector::reserve(), so an unvalidated claim from a
// tiny corrupted/malicious file (a handful of real bytes, one length field
// claiming ~4GB) forces multi-GB memory commit and multi-second CPU time
// before the truncation check below ever gets a chance to fire — confirmed
// empirically. No legitimate mc3 scene has a single string field anywhere
// near this size.
static constexpr uint32_t kMcbMaxStringLen = 64u * 1024u * 1024u; // 64 MB

static std::string rRawStr(std::istream& in) {
    uint32_t len = rU32(in);
    if (len == 0) return {};
    if (len > kMcbMaxStringLen)
        throw std::runtime_error("MCB: string length " + std::to_string(len) +
            " exceeds sanity limit (corrupted or malicious file?)");
    std::string s(len, '\0');
    if (!in.read(s.data(), static_cast<std::streamsize>(len)))
        throw std::runtime_error("MCB: string truncated");
    return s;
}

// Same rationale as kMcbMaxStringLen/rRawStr above, applied to every
// count-prefixed collection field in the format (array/map lengths, object
// counts, keyframe/channel counts, etc.): a claimed count is validated
// against a generous sanity ceiling before it's used to .reserve() a vector
// or bound a loop, not just trusted from an untrusted stream. No legitimate
// mc3 scene has any single collection anywhere near this size.
static constexpr uint32_t kMcbMaxCollectionCount = 10u * 1000u * 1000u; // 10M

static uint32_t rU32Bounded(std::istream& in) {
    uint32_t n = rU32(in);
    if (n > kMcbMaxCollectionCount)
        throw std::runtime_error("MCB: collection count " + std::to_string(n) +
            " exceeds sanity limit (corrupted or malicious file?)");
    return n;
}

static std::array<float,3> rVec3(std::istream& in) {
    float x = rF32(in), y = rF32(in), z = rF32(in);
    return {x, y, z};
}

static std::array<float,4> rVec4(std::istream& in) {
    float x = rF32(in), y = rF32(in), z = rF32(in), w = rF32(in);
    return {x, y, z, w};
}

// Read key: returns the key string, or "" if key_len == 0 (end-of-object sentinel)
static std::string rKey(std::istream& in) {
    uint8_t len = rU8(in);
    if (len == 0) return {};
    std::string k(len, '\0');
    if (!in.read(k.data(), len))
        throw std::runtime_error("MCB: key truncated");
    return k;
}

// Defensive recursion-depth guard against a corrupted/malicious MCB file
// with deeply-nested objects/arrays causing unbounded recursion. Unlike a
// normal parse error, a native stack overflow is not a catchable
// std::exception and crashes the whole process — confirmed empirically: a
// few-hundred-KB file with ~20,000 levels of nested <children> segfaults,
// even though every real call site (mc3tomcb, the editor's Open dialog)
// wraps loadFromFile/loadFromBinary in a plain catch(const
// std::exception&). Mirrors CsgEvaluator.cpp's CSG_MAX_DEPTH pattern. Two
// independent counters (one per recursion tree: skipValue's skip-path,
// readObject's real-object-tree path) are used rather than one shared
// counter — simpler than threading state across both call chains, and the
// combined worst-case stack depth (each counter capped independently) is
// still trivially within a normal 8MB thread stack.
template <int MaxDepth>
class RecursionGuard {
public:
    RecursionGuard() {
        if (++depth_ > MaxDepth) {
            --depth_;
            throw std::runtime_error(
                "MCB: nesting depth exceeds " + std::to_string(MaxDepth) +
                " (corrupted or malicious file?)");
        }
    }
    ~RecursionGuard() { --depth_; }
    RecursionGuard(const RecursionGuard&) = delete;
private:
    static thread_local int depth_;
};
template <int MaxDepth>
thread_local int RecursionGuard<MaxDepth>::depth_ = 0;

// Forward declaration
static void skipValue(std::istream& in, uint8_t tag);

static void skipObject(std::istream& in) {
    while (true) {
        std::string k = rKey(in);
        if (k.empty()) break;
        uint8_t tag = rU8(in);
        skipValue(in, tag);
    }
}

static void skipValue(std::istream& in, uint8_t tag) {
    RecursionGuard<256> guard;
    switch (tag) {
    case TAG_NULL:                                     break;
    case TAG_BOOL: rU8(in);                            break;
    case TAG_I32:  rU32(in);                           break;
    case TAG_F32:  rU32(in);                           break;
    case TAG_STR:  rRawStr(in);                        break;
    case TAG_VEC3: rU32(in); rU32(in); rU32(in);      break;
    case TAG_VEC4: rU32(in); rU32(in); rU32(in); rU32(in); break;
    case TAG_OBJ:  skipObject(in);                     break;
    case TAG_ARR: {
        uint32_t n = rU32Bounded(in);
        for (uint32_t i = 0; i < n; ++i) {
            uint8_t t = rU8(in);
            skipValue(in, t);
        }
        break;
    }
    case TAG_MAP: {
        uint32_t n = rU32Bounded(in);
        for (uint32_t i = 0; i < n; ++i) {
            rRawStr(in); // key
            uint8_t t = rU8(in);
            skipValue(in, t);
        }
        break;
    }
    default:
        throw std::runtime_error("MCB: unknown tag " + std::to_string(tag));
    }
}

// ---------------------------------------------------------------------------
// Mc3 type deserializers
// ---------------------------------------------------------------------------

static Mc3::Mc3Transform readTransform(std::istream& in) {
    Mc3::Mc3Transform tf;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "position") tf.position = rVec3(in);
        else if (k == "rotation") tf.rotation = rVec3(in);
        else if (k == "scale")    tf.scale    = rVec3(in);
        else if (k == "pivot")    tf.pivot    = rVec3(in);
        else                      skipValue(in, tag);
    }
    return tf;
}

static Mc3::Mc3Primitive readPrimitive(std::istream& in) {
    Mc3::Mc3Primitive p;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "primitiveType") p.primitiveType = static_cast<Mc3::PrimitiveType>(rI32(in));
        else if (k == "size")          p.size          = rVec3(in);
        else if (k == "radius")        p.radius        = rF32(in);
        else if (k == "height")        p.height        = rF32(in);
        else if (k == "segments")      p.segments      = rI32(in);
        else if (k == "axis")          p.axis          = rRawStr(in);
        else if (k == "majorRadius")   p.majorRadius   = rF32(in);
        else if (k == "minorRadius")   p.minorRadius   = rF32(in);
        else if (k == "subdivisionsX") p.subdivisionsX = rI32(in);
        else if (k == "subdivisionsZ") p.subdivisionsZ = rI32(in);
        else                           skipValue(in, tag);
    }
    return p;
}

static Mc3::Mc3Deform readDeform(std::istream& in) {
    Mc3::Mc3Deform d;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if (k == "scale") d.scale = rVec3(in);
        else               skipValue(in, tag);
    }
    return d;
}

static Mc3::Mc3CsgOperation readCsgOp(std::istream& in) {
    Mc3::Mc3CsgOperation csg;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if (k == "csgType") csg.csgType = static_cast<Mc3::CsgType>(rI32(in));
        else                 skipValue(in, tag);
    }
    return csg;
}

static Mc3::Mc3CrossSection readCrossSection(std::istream& in) {
    Mc3::Mc3CrossSection cs;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "type")        cs.type        = static_cast<Mc3::CrossSectionType>(rI32(in));
        else if (k == "width")       cs.width       = rF32(in);
        else if (k == "height")      cs.height      = rF32(in);
        else if (k == "radius")      cs.radius      = rF32(in);
        else if (k == "innerRadius") cs.innerRadius = rF32(in);
        else if (k == "sides")       cs.sides       = rI32(in);
        else if (k == "segments")    cs.segments    = rI32(in);
        else if (k == "customPoints") {
            // TAG_ARR of TAG_VEC3
            uint32_t n = rU32Bounded(in);
            cs.customPoints.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_VEC3) {
                    float x = rF32(in), y = rF32(in); rF32(in); // z unused
                    Mc3::Mc3CrossSection::Point2D pt; pt.x = x; pt.y = y;
                    cs.customPoints.push_back(pt);
                } else {
                    skipValue(in, t);
                }
            }
        }
        else skipValue(in, tag);
    }
    return cs;
}

static Mc3::Mc3PathPoint readPathPoint(std::istream& in) {
    Mc3::Mc3PathPoint pp;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "position")  pp.position  = rVec3(in);
        else if (k == "controlIn") pp.controlIn = rVec3(in);
        else                       skipValue(in, tag);
    }
    return pp;
}

static Mc3::Mc3ExtrudePath readPath(std::istream& in) {
    Mc3::Mc3ExtrudePath p;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "type")        p.type        = static_cast<Mc3::ExtrudePathType>(rI32(in));
        else if (k == "length")      p.length      = rF32(in);
        else if (k == "axis")        p.axis        = rRawStr(in);
        else if (k == "arcRadius")   p.arcRadius   = rF32(in);
        else if (k == "arcAngle")    p.arcAngle    = rF32(in);
        else if (k == "helixRadius") p.helixRadius = rF32(in);
        else if (k == "helixHeight") p.helixHeight = rF32(in);
        else if (k == "helixTurns")  p.helixTurns  = rF32(in);
        else if (k == "points") {
            uint32_t n = rU32Bounded(in);
            p.points.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) p.points.push_back(readPathPoint(in));
                else               skipValue(in, t);
            }
        }
        else skipValue(in, tag);
    }
    return p;
}

static Mc3::Mc3Extrude readExtrude(std::istream& in) {
    Mc3::Mc3Extrude ex;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "crossSection") ex.crossSection = readCrossSection(in);
        else if (k == "path")         ex.path         = readPath(in);
        else if (k == "twist")        ex.twist        = rF32(in);
        else if (k == "segments")     ex.segments     = rI32(in);
        else if (k == "smooth")       ex.smooth       = rU8(in) != 0;
        else if (k == "caps")         ex.caps         = rU8(in) != 0;
        else                          skipValue(in, tag);
    }
    return ex;
}

static Mc3::Mc3UvMapping readUvMapping(std::istream& in) {
    Mc3::Mc3UvMapping uv;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "projection") uv.projection = static_cast<Mc3::UvProjection>(rI32(in));
        else if (k == "scaleU")     uv.scaleU     = rF32(in);
        else if (k == "scaleV")     uv.scaleV     = rF32(in);
        else if (k == "offsetU")    uv.offsetU    = rF32(in);
        else if (k == "offsetV")    uv.offsetV    = rF32(in);
        else if (k == "rotation")   uv.rotation   = rF32(in);
        else                        skipValue(in, tag);
    }
    return uv;
}

static Mc3::Mc3ObjectState readObjectState(std::istream& in) {
    Mc3::Mc3ObjectState st;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "position") st.position = rVec3(in);
        else if (k == "rotation") st.rotation = rVec3(in);
        else if (k == "scale")    st.scale    = rVec3(in);
        else if (k == "visible")  st.visible  = rU8(in) != 0;
        else if (k == "material") st.material = rRawStr(in);
        else                      skipValue(in, tag);
    }
    return st;
}

// Forward declaration for recursive children
static std::shared_ptr<Mc3::Mc3Object> readObject(std::istream& in);

static std::shared_ptr<Mc3::Mc3Object> readObject(std::istream& in) {
    RecursionGuard<256> guard;
    auto obj = std::make_shared<Mc3::Mc3Object>();
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "type")             obj->type             = static_cast<Mc3::ObjectType>(rI32(in));
        else if (k == "name")             obj->name             = rRawStr(in);
        else if (k == "id")               obj->id               = rRawStr(in);
        else if (k == "material")         obj->material         = rRawStr(in);
        else if (k == "visible")          obj->visible          = rU8(in) != 0;
        else if (k == "collision")        obj->collision        = rRawStr(in);
        else if (k == "layer")            obj->layer            = rRawStr(in);
        else if (k == "isCutter")         obj->isCutter         = rU8(in) != 0;
        else if (k == "definition")       obj->definition       = rRawStr(in);
        else if (k == "meshSource")       obj->meshSource       = rRawStr(in);
        else if (k == "materialOverride") obj->materialOverride = rRawStr(in);
        else if (k == "transform")        obj->transform        = readTransform(in);
        else if (k == "deform")           obj->deform           = readDeform(in);
        else if (k == "primitive")        obj->primitive        = readPrimitive(in);
        else if (k == "csgOperation")     obj->csgOperation     = readCsgOp(in);
        else if (k == "extrude")          obj->extrude          = readExtrude(in);
        else if (k == "uvMapping")        obj->uvMapping        = readUvMapping(in);
        else if (k == "tags") {
            uint32_t n = rU32Bounded(in);
            obj->tags.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) obj->tags.push_back(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "variantDefs") {
            uint32_t n = rU32Bounded(in);
            obj->variantDefinitions.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) obj->variantDefinitions.push_back(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "states") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string sk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) obj->states[sk] = readObjectState(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "children") {
            uint32_t n = rU32Bounded(in);
            obj->children.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) obj->children.push_back(readObject(in));
                else               skipValue(in, t);
            }
        }
        else skipValue(in, tag);
    }
    return obj;
}

static Mc3::Mc3Texture readTexture(std::istream& in) {
    Mc3::Mc3Texture tex;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "name")       tex.name       = rRawStr(in);
        else if (k == "uri")        tex.uri        = rRawStr(in);
        else if (k == "wrapU")      tex.wrapU      = rRawStr(in);
        else if (k == "wrapV")      tex.wrapV      = rRawStr(in);
        else if (k == "filter")     tex.filter     = rRawStr(in);
        else if (k == "colorSpace") tex.colorSpace = rRawStr(in);
        else                        skipValue(in, tag);
    }
    return tex;
}

static Mc3::Mc3SvgTexture readSvgTexture(std::istream& in, const std::string& id) {
    Mc3::Mc3SvgTexture svg;
    svg.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "src")           svg.src           = rRawStr(in);
        else if (k == "inlineContent") svg.inlineContent = rRawStr(in);
        else                           skipValue(in, tag);
    }
    return svg;
}

static Mc3::Mc3ObjectOverride readObjectOverride(std::istream& in) {
    Mc3::Mc3ObjectOverride ovr;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "id")       ovr.id       = rRawStr(in);
        else if (k == "visible")  ovr.visible  = rU8(in) != 0;
        else if (k == "position") ovr.position = rVec3(in);
        else if (k == "rotation") ovr.rotation = rVec3(in);
        else if (k == "material") ovr.material = rRawStr(in);
        else                      skipValue(in, tag);
    }
    return ovr;
}

static Mc3::Mc3SceneState readSceneState(std::istream& in, const std::string& name) {
    Mc3::Mc3SceneState state;
    state.name = name;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if (k == "overrides" && tag == TAG_ARR) {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) state.overrides.push_back(readObjectOverride(in));
                else               skipValue(in, t);
            }
        } else {
            skipValue(in, tag);
        }
    }
    return state;
}

static Mc3::TriggerStepType parseTriggerStepType(const std::string& s) {
    if (s == "play-sound")  return Mc3::TriggerStepType::PlaySound;
    if (s == "run-script")  return Mc3::TriggerStepType::RunScript;
    if (s == "play-music")  return Mc3::TriggerStepType::PlayMusic;
    return Mc3::TriggerStepType::PlayAction;
}

static Mc3::Mc3Trigger readTrigger(std::istream& in, const std::string& id) {
    Mc3::Mc3Trigger trig;
    trig.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if (k == "steps" && tag == TAG_ARR) {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t != TAG_OBJ) { skipValue(in, t); continue; }
                Mc3::Mc3TriggerStep step;
                while (true) {
                    std::string sk = rKey(in); if (sk.empty()) break;
                    uint8_t st = rU8(in);
                    if      (sk == "type") step.type = parseTriggerStepType(rRawStr(in));
                    else if (sk == "ref")  step.ref  = rRawStr(in);
                    else                   skipValue(in, st);
                }
                trig.steps.push_back(std::move(step));
            }
        } else {
            skipValue(in, tag);
        }
    }
    return trig;
}

static Mc3::Mc3Sound readSound(std::istream& in, const std::string& id) {
    Mc3::Mc3Sound snd;
    snd.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "src")  snd.src  = rRawStr(in);
        else if (k == "loop") snd.loop = rU8(in) != 0;
        else                  skipValue(in, tag);
    }
    return snd;
}

static Mc3::Mc3Music readMusic(std::istream& in, const std::string& id) {
    Mc3::Mc3Music mus;
    mus.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "src")  mus.src  = rRawStr(in);
        else if (k == "loop") mus.loop = rU8(in) != 0;
        else                  skipValue(in, tag);
    }
    return mus;
}

static Mc3::Mc3Script readScript(std::istream& in, const std::string& id) {
    Mc3::Mc3Script sc;
    sc.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "type")   sc.type   = rRawStr(in);
        else if (k == "source") sc.source = rRawStr(in);
        else                    skipValue(in, tag);
    }
    return sc;
}

static Mc3::Mc3EmbedGltf readEmbed(std::istream& in, const std::string& id) {
    Mc3::Mc3EmbedGltf em;
    em.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "src")           em.src           = rRawStr(in);
        else if (k == "base64Content") em.base64Content = rRawStr(in);
        else                           skipValue(in, tag);
    }
    return em;
}

static Mc3::Mc3Material readMaterial(std::istream& in) {
    Mc3::Mc3Material m;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "name")                     m.name                     = rRawStr(in);
        else if (k == "baseColor")                m.baseColor                = rVec4(in);
        else if (k == "baseColorTexture")         m.baseColorTexture         = rRawStr(in);
        else if (k == "normalTexture")            m.normalTexture            = rRawStr(in);
        else if (k == "emissiveTexture")          m.emissiveTexture          = rRawStr(in);
        else if (k == "metallicRoughnessTexture") m.metallicRoughnessTexture = rRawStr(in);
        else if (k == "occlusionTexture")         m.occlusionTexture         = rRawStr(in);
        else if (k == "roughness")                m.roughness                = rF32(in);
        else if (k == "metallic")                 m.metallic                 = rF32(in);
        else if (k == "normalScale")              m.normalScale              = rF32(in);
        else if (k == "occlusionStrength")        m.occlusionStrength        = rF32(in);
        else if (k == "emissiveColor")            m.emissiveColor            = rVec3(in);
        else if (k == "alphaMode")                m.alphaMode                = rRawStr(in);
        else if (k == "alphaCutoff")              m.alphaCutoff              = rF32(in);
        else if (k == "doubleSided")              m.doubleSided              = rU8(in) != 0;
        else                                      skipValue(in, tag);
    }
    return m;
}

static Mc3::Mc3Light readLight(std::istream& in) {
    Mc3::Mc3Light lt;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "type")        lt.type        = static_cast<Mc3::LightType>(rI32(in));
        else if (k == "name")        lt.name        = rRawStr(in);
        else if (k == "color")       lt.color       = rVec3(in);
        else if (k == "brightness")  lt.brightness  = rF32(in);
        else if (k == "direction")   lt.direction   = rVec3(in);
        else if (k == "position")    lt.position    = rVec3(in);
        else if (k == "range")       lt.range       = rF32(in);
        else if (k == "angle")       lt.angle       = rF32(in);
        else if (k == "falloff")     lt.falloff     = rF32(in);
        else if (k == "castShadows") lt.castShadows = rU8(in) != 0;
        else                         skipValue(in, tag);
    }
    return lt;
}

static Mc3::Mc3Camera readCamera(std::istream& in) {
    Mc3::Mc3Camera cam;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "name")      cam.name      = rRawStr(in);
        else if (k == "type")      cam.type      = static_cast<Mc3::CameraType>(rI32(in));
        else if (k == "position")  cam.position  = rVec3(in);
        else if (k == "target")    cam.target    = rVec3(in);
        else if (k == "rotation")  cam.rotation  = rVec3(in);
        else if (k == "nearPlane") cam.nearPlane = rF32(in);
        else if (k == "farPlane")  cam.farPlane  = rF32(in);
        else if (k == "fov")       cam.fov       = rF32(in);
        else if (k == "orthoSize") cam.orthoSize = rF32(in);
        else                       skipValue(in, tag);
    }
    return cam;
}

static Mc3::Mc3Fog readFog(std::istream& in) {
    Mc3::Mc3Fog fog;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "color")   fog.color   = rVec3(in);
        else if (k == "mode")    fog.mode    = static_cast<Mc3::FogMode>(rI32(in));
        else if (k == "start")   fog.start   = rF32(in);
        else if (k == "end")     fog.end     = rF32(in);
        else if (k == "density") fog.density = rF32(in);
        else                     skipValue(in, tag);
    }
    return fog;
}

static Mc3::Mc3Environment readEnvironment(std::istream& in) {
    Mc3::Mc3Environment env;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "backgroundColor")  env.backgroundColor  = rVec3(in);
        else if (k == "backgroundTexture") env.backgroundTexture = rRawStr(in);
        else if (k == "skyboxTexture")     env.skyboxTexture     = rRawStr(in);
        else if (k == "fog")               env.fog              = readFog(in);
        else                               skipValue(in, tag);
    }
    return env;
}

static Mc3::Mc3Keyframe readKeyframe(std::istream& in) {
    Mc3::Mc3Keyframe kf;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "time")          kf.time                = rF32(in);
        else if (k == "value")         kf.value               = rF32(in);
        else if (k == "interpolation") kf.interpolation       = static_cast<Mc3::Interpolation>(rI32(in));
        else if (k == "leftDt")        kf.handleLeft.dt       = rF32(in);
        else if (k == "leftDv")        kf.handleLeft.dv       = rF32(in);
        else if (k == "rightDt")       kf.handleRight.dt      = rF32(in);
        else if (k == "rightDv")       kf.handleRight.dv      = rF32(in);
        else                           skipValue(in, tag);
    }
    return kf;
}

static Mc3::Mc3Channel readChannel(std::istream& in) {
    Mc3::Mc3Channel ch;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "targetObject") ch.targetObject = rRawStr(in);
        else if (k == "property")     ch.property     = static_cast<Mc3::AnimatedProperty>(rI32(in));
        else if (k == "keyframes") {
            uint32_t n = rU32Bounded(in);
            ch.keyframes.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) ch.keyframes.push_back(readKeyframe(in));
                else               skipValue(in, t);
            }
        }
        else skipValue(in, tag);
    }
    return ch;
}

static Mc3::Mc3Action readAction(std::istream& in) {
    Mc3::Mc3Action act;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "name")     act.name     = rRawStr(in);
        else if (k == "duration") act.duration = rF32(in);
        else if (k == "loop")     act.loop     = rU8(in) != 0;
        else if (k == "autoplay") act.autoplay = rU8(in) != 0;
        else if (k == "channels") {
            uint32_t n = rU32Bounded(in);
            act.channels.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) act.channels.push_back(readChannel(in));
                else               skipValue(in, t);
            }
        }
        else skipValue(in, tag);
    }
    return act;
}

static Mc3::Mc3Document readDocument(std::istream& in) {
    Mc3::Mc3Document doc;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "version")          doc.version          = rRawStr(in);
        else if (k == "model")            doc.model            = rRawStr(in);
        else if (k == "unit")             doc.unit             = rRawStr(in);
        else if (k == "coordinateSystem") doc.coordinateSystem = rRawStr(in);
        else if (k == "rotationUnits")    doc.rotationUnits    = rRawStr(in);
        else if (k == "eulerOrder")       doc.eulerOrder       = rRawStr(in);
        else if (k == "defaultCamera")    doc.defaultCamera    = rRawStr(in);
        else if (k == "meta") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.meta[mk] = rRawStr(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "metadata") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.metadata[mk] = rRawStr(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "includes") {
            uint32_t n = rU32Bounded(in);
            doc.includes.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.includes.push_back(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "includedDefs") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.includedDefs.insert(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "includedMaterials") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.includedMaterials.insert(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "includedTextures") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.includedTextures.insert(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "environment")      doc.environment      = readEnvironment(in);
        else if (k == "lights") {
            uint32_t n = rU32Bounded(in);
            doc.lights.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.lights.push_back(readLight(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "cameras") {
            uint32_t n = rU32Bounded(in);
            doc.cameras.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.cameras.push_back(readCamera(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "textures") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.textures[mk] = readTexture(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "svgTextures") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.svgTextures[mk] = readSvgTexture(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "embeds") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.embeds[mk] = readEmbed(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "scripts") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.scripts[mk] = readScript(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "sounds") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.sounds[mk] = readSound(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "musicTracks") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.musicTracks[mk] = readMusic(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "triggers") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.triggers[mk] = readTrigger(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "sceneStates") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.sceneStates[mk] = readSceneState(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "materials") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.materials[mk] = readMaterial(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "definitions") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.definitions[mk] = readObject(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "objects") {
            uint32_t n = rU32Bounded(in);
            doc.objects.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.objects.push_back(readObject(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "actions") {
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.actions[mk] = readAction(in);
                else               skipValue(in, t);
            }
        }
        else skipValue(in, tag);
    }
    return doc;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Mc3::Mc3Document loadFromBinary(std::istream& in) {
    // Validate header
    char magic[4];
    if (!in.read(magic, 4) || std::memcmp(magic, MCB_MAGIC, 4) != 0)
        throw std::runtime_error("MCB: invalid magic");
    uint8_t version = rU8(in);
    if (version != MCB_VERSION)
        throw std::runtime_error("MCB: unsupported version " + std::to_string(version));
    uint8_t flags = rU8(in);
    if (flags & MCB_FLAG_COMPRESSED)
        throw std::runtime_error("MCB: compressed format not yet supported");
    rU8(in); rU8(in); // reserved

    // Root must be TAG_OBJ
    uint8_t rootTag = rU8(in);
    if (rootTag != TAG_OBJ)
        throw std::runtime_error("MCB: root is not an object");

    return readDocument(in);
}

Mc3::Mc3Document loadFromFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("MCB: cannot open: " + path.string());
    auto doc = loadFromBinary(f);
    doc.sourcePath = path.parent_path();
    return doc;
}

} // namespace MeshCraft::Mcb
