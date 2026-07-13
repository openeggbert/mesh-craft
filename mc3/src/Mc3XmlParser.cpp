#include "Mc3XmlParser.hpp"
#include "MathUtils.hpp"

#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Camera.hpp"
#include "MeshCraft/Mc3/Mc3Environment.hpp"
#include "MeshCraft/Mc3/Mc3Extrude.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
#include "MeshCraft/Mc3/Mc3Validation.hpp"

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

// ---------------------------------------------------------------------------
// SYS-W1-01: Mc3Validation reporting
// ---------------------------------------------------------------------------
//
// Mirrors the g_budget / g_confineResourcePaths thread_local pattern already
// used in this file (see the DocumentBudget comment further down for the full
// rationale): parseObject/parseChildren/parsePrimitive/parseCrossSection/
// parseExtrude/attrF/attrI/attrCount/etc. form a large, already-recursive
// call graph with no existing context-object parameter to thread a
// diagnostics sink through, and adding one now is a much larger refactor
// than this task's scope justifies. `g_validation` is set once per top-level
// parse() / parseString() call (nullptr when the caller didn't ask for
// diagnostics -- every report*() call below is then a no-op) and reset
// unconditionally at the start of every call, so nothing can leak a stale
// pointer into an unrelated later parse on the same thread.
static thread_local Mc3Validation* g_validation = nullptr;

// The file currently being parsed -- the top-level document, or whichever
// <include>d file is being merged right now. Set at the top of parse()/
// parseString() and temporarily overridden (via SourceFileScope, defined
// near mergeInclude()) for the duration of merging one <include> file.
static thread_local std::filesystem::path g_currentSourceFile;

// Best-effort object identity for a diagnostic: the element's `id` if
// present, else its `name`, else its tag name. Never guesses/synthesizes
// beyond what's actually on the element.
static std::string objectIdentity(const XMLElement* el) {
    if (!el) return {};
    if (const char* id = el->Attribute("id"); id && id[0]) return id;
    if (const char* name = el->Attribute("name"); name && name[0]) return name;
    return el->Name() ? el->Name() : std::string{};
}

static void reportWarning(const XMLElement* el, const char* field,
                           const std::string& message, const std::string& repair = {}) {
    if (!g_validation) return;
    g_validation->addWarning(g_currentSourceFile.string(), objectIdentity(el), field,
                             message, repair);
}

static void reportError(const XMLElement* el, const char* field,
                         const std::string& message, const std::string& repair = {}) {
    if (!g_validation) return;
    g_validation->addError(g_currentSourceFile.string(), objectIdentity(el), field,
                           message, repair);
}

// Whole-document findings (no single element is responsible), e.g. a
// document-wide budget overflow or an <include> resolution failure.
static void reportWarningDoc(const char* field, const std::string& message,
                              const std::string& repair = {}) {
    if (!g_validation) return;
    g_validation->addWarning(g_currentSourceFile.string(), std::string{}, field,
                             message, repair);
}

static void reportErrorDoc(const char* field, const std::string& message,
                            const std::string& repair = {}) {
    if (!g_validation) return;
    g_validation->addError(g_currentSourceFile.string(), std::string{}, field,
                           message, repair);
}

// RAII guard for g_validation, used at the top of Mc3XmlParser::parse()/
// parseString() (the only two call sites that own a top-level parse). Clears
// the thread_local pointer again on scope exit -- including via exception --
// so a caller-owned Mc3Validation never remains reachable via the thread_local
// past the end of the call that was given it.
struct ValidationScope {
    explicit ValidationScope(Mc3Validation* v) { g_validation = v; }
    ~ValidationScope() { g_validation = nullptr; }
    ValidationScope(const ValidationScope&) = delete;
};

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
    try {
        float raw = std::stof(v);
        float sanitized = Internal::finiteOr(raw, def);
        if (sanitized != raw) // NaN/Inf were replaced (NaN != NaN is true under IEEE-754)
            reportWarning(el, name, std::string("value '") + v + "' is not finite (NaN/Inf)",
                          "replaced with " + std::to_string(sanitized));
        return sanitized;
    } catch (...) {
        reportWarning(el, name, std::string("value '") + v + "' is not a valid number",
                      "defaulted to " + std::to_string(def));
        return def;
    }
}

static int attrI(const XMLElement* el, const char* name, int def = 0) {
    const char* v = el->Attribute(name);
    if (!v) return def;
    try { return std::stoi(v); } catch (...) {
        reportWarning(el, name, std::string("value '") + v + "' is not a valid integer",
                      "defaulted to " + std::to_string(def));
        return def;
    }
}

// ---------------------------------------------------------------------------
// SYS-W1-02: generic per-field numeric-range helpers
// ---------------------------------------------------------------------------
//
// attrCount() (below) already clamps integer tessellation counts into a
// range; these are the float equivalents, used for fields whose valid domain
// is documented in MC3_FORMAT.md but was previously enforced nowhere beyond
// attrF()'s own NaN/Inf sanitization (finiteOr) -- a value could be a
// perfectly finite float and still be semantically nonsensical (a FOV of
// 600 degrees, a roughness of -3, a negative sphere radius). Named per-field
// constants live next to the call sites that use them (camera/material/
// geometry/environment/animation/transform sections below), following the
// same "named constant + short rationale comment" convention kMaxTessellation
// established.

// Clamps an already-finite float into [lo, hi], reporting a Mc3Validation
// warning (clamped, not rejected -- an out-of-range field is treated as a
// likely authoring mistake, not a resource-exhaustion attack, matching this
// session's established judgment call for AUD-059's tessellation clamp) when
// the raw value was actually outside the range.
static float clampRange(const XMLElement* el, const char* field, float raw,
                         float lo, float hi) {
    if (raw >= lo && raw <= hi) return raw;
    float v = std::clamp(raw, lo, hi);
    reportWarning(el, field,
                  "value " + std::to_string(raw) + " is outside the documented range [" +
                  std::to_string(lo) + ", " + std::to_string(hi) + "]",
                  "clamped to " + std::to_string(v));
    return v;
}

// attrF() + clampRange() combined: parse a float attribute and clamp it into
// a documented semantic range in one call.
static float attrFClamped(const XMLElement* el, const char* name, float def,
                           float lo, float hi) {
    return clampRange(el, name, attrF(el, name, def), lo, hi);
}

// One-sided floor clamp: reports+clamps `raw` up to `minv` when it falls
// below it. `reason` is a short human-readable clause appended to the
// diagnostic message (e.g. "must be > 0").
static float clampMin(const XMLElement* el, const char* field, float raw, float minv,
                       const char* reason) {
    if (raw >= minv) return raw;
    reportWarning(el, field, "value " + std::to_string(raw) + " " + reason,
                  "clamped to " + std::to_string(minv));
    return minv;
}

// SYS-W1-02: geometry dimensions (radius/height/size/majorRadius/
// minorRadius, cross-section width/height/radius/inner_radius, extrude path
// length/radius/height) are only ever meaningful as non-negative -- a
// negative box size or sphere radius has no physical meaning and produces
// geometry with inverted/self-intersecting winding downstream (MeshBuilder.
// cpp assumes positive dimensions when computing normals). Clamped to 0, not
// rejected: same authoring-mistake judgment call as the other SYS-W1-02
// ranges. Deliberately clamps only NEGATIVE values, leaving exactly-zero
// untouched -- zero is already a meaningful, pre-existing input for several
// of these fields (e.g. <disk inner_radius="0"/> means "solid disk"), so
// forcing a strictly-positive floor would change legitimate existing
// behavior that finite_input_test/roundtrip_test already rely on.
static float rejectNegative(const XMLElement* el, const char* field, float raw) {
    if (raw >= 0.0f) return raw;
    reportWarning(el, field, "value " + std::to_string(raw) + " is negative "
                  "(dimensions/radii must be >= 0)",
                  "clamped to 0");
    return 0.0f;
}

// Tessellation counts (segments / sides / subdivisions) come from untrusted
// input and directly drive geometry allocation, so they are clamped to a sane
// range. The upper bound is far above any legitimate mesh but stops a hostile
// <sphere segments="100000000"/> (which would request ~5e15 vertices) or
// <grid subdivisions_x="1000000"/> from exhausting memory.
static constexpr int kMaxTessellation = 4096;

static int attrCount(const XMLElement* el, const char* name, int def,
                     int minv, int maxv = kMaxTessellation) {
    int raw = attrI(el, name, def);
    int v = raw;
    if (v < minv) v = minv;
    if (v > maxv) v = maxv;
    // Only report when an attribute that was actually PRESENT got clamped --
    // not when a missing attribute's own default happens to need no clamping.
    if (v != raw && el->Attribute(name))
        reportWarning(el, name,
                      "value " + std::to_string(raw) + " is outside the allowed range [" +
                      std::to_string(minv) + ", " + std::to_string(maxv) + "]",
                      "clamped to " + std::to_string(v));
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
    long long totalIncludes = 0; // count of genuinely-new (non-cyclic, non-diamond-dup) <include> merges
    long long totalMaterials = 0; // SYS-W1-03
    long long totalTextures = 0;  // SYS-W1-03: <texture> and <texture type="svg"> combined
    long long totalEmbeds = 0;      // SYS-W1-03
    long long totalEmbedBytes = 0;  // SYS-W1-03: sum of every embed's base64Content.size()
    long long totalActions = 0;   // SYS-W1-03
    long long totalChannels = 0;  // SYS-W1-03
    long long totalKeyframes = 0; // SYS-W1-03
    long long totalDefinitions = 0; // SYS-W1-03

    // Generous enough for any real scene (the largest checked-in stress
    // fixture sums to a few thousand) while still bounding the pathological
    // many-objects-near-the-per-field-cap case to a small multiple of that cap.
    static constexpr long long kMaxTotalObjects = 100'000;
    static constexpr long long kMaxTotalTessellationWeight = 500'000;

    // SYS-W1-04: cycle detection (inProgress set) and per-chain depth
    // (policy.maxIncludeDepth) already bound a cyclic or deep-linear
    // <include> chain, but neither bounds FAN-OUT -- a single document
    // directly including thousands of distinct sibling files (e.g. <include
    // file="f0.mc3.xml"/> ... <include file="f9999.mc3.xml"/>), each of
    // which is individually well-formed and non-cyclic. Empirically
    // confirmed unbounded before this fix: 1500 trivial sibling includes
    // merged successfully with no error. No legitimate scene includes
    // anywhere near this many distinct files.
    static constexpr long long kMaxTotalIncludes = 1'000;

    // SYS-W1-03: materials/textures are heavier than a bare object (several
    // string fields each: name/uri/wrap/filter/color_space, or 5 texture
    // slots + several floats for materials) but still far cheaper than
    // geometry -- a ceiling an order of magnitude below kMaxTotalObjects,
    // generous for any real material/texture library, still bounds
    // worst-case map-entry memory.
    static constexpr long long kMaxTotalMaterials = 20'000;
    static constexpr long long kMaxTotalTextures = 20'000;

    // SYS-W1-03: kMaxEmbedBase64Length (below, SYS-W1-04) already bounds any
    // SINGLE <embed>'s inline base64 body to 64MB, but nothing previously
    // bounded how many such embeds a document could have -- N embeds each
    // individually under the 64MB cap can still sum to unbounded memory
    // (e.g. 1000 embeds at 63MB each = ~63GB). kMaxTotalEmbeds bounds the
    // COUNT (generous for any real prop/mesh library); kMaxTotalEmbedBytes
    // separately bounds the SUM of every embed's base64 length, the same
    // "per-field cap + document-wide running-total cap" pattern already
    // established by kMaxTessellation + kMaxTotalTessellationWeight above.
    static constexpr long long kMaxTotalEmbeds = 1'000;
    static constexpr long long kMaxTotalEmbedBytes = 256ll * 1024 * 1024; // 256MB combined

    // SYS-W1-03: an animation action/channel/keyframe is one of the
    // cheapest structures in the format (a handful of floats/enums each),
    // so these ceilings are generous even by kMaxTotalObjects' standard --
    // sized to comfortably cover any real character-rig-scale animation set
    // (hundreds of actions, tens of channels each, tens to low-hundreds of
    // keyframes each) while still bounding worst-case vector/map memory
    // against a document that's all animation data and nothing else.
    static constexpr long long kMaxTotalActions = 10'000;
    static constexpr long long kMaxTotalChannels = 200'000;
    static constexpr long long kMaxTotalKeyframes = 2'000'000;

    // SYS-W1-03: a <definition>'s own root object already separately charges
    // chargeObject() (and, transitively, its subtree does too), so this
    // dimension isn't primarily about geometry/allocation cost -- it bounds
    // std::map<std::string, shared_ptr<Mc3Object>> entry-count overhead
    // (doc.definitions) independent of what's inside each definition. Same
    // order of magnitude as materials/textures: far beyond any real prop
    // library (hundreds of reusable definitions is already a large one).
    static constexpr long long kMaxTotalDefinitions = 20'000;

    void chargeObject() {
        if (++totalObjects > kMaxTotalObjects) {
            std::string msg = "MC3: document exceeds the total object budget (" +
                std::to_string(kMaxTotalObjects) + ") -- rejected before allocating"
                " geometry for all of them";
            reportErrorDoc("objects", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeTessellation(int weight) {
        totalTessellationWeight += weight;
        if (totalTessellationWeight > kMaxTotalTessellationWeight) {
            std::string msg = "MC3: document's total tessellation complexity (sum of all "
                "segments/sides/subdivisions values, " +
                std::to_string(totalTessellationWeight) + ") exceeds the budget (" +
                std::to_string(kMaxTotalTessellationWeight) +
                ") -- rejected before allocating geometry for all of it";
            reportErrorDoc("tessellation", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeInclude() {
        if (++totalIncludes > kMaxTotalIncludes) {
            std::string msg = "MC3: document exceeds the total <include> budget (" +
                std::to_string(kMaxTotalIncludes) + ") -- rejected (a hostile "
                "fan-out of many distinct include files?)";
            reportErrorDoc("include", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeMaterial() {
        if (++totalMaterials > kMaxTotalMaterials) {
            std::string msg = "MC3: document exceeds the total material budget (" +
                std::to_string(kMaxTotalMaterials) + ")";
            reportErrorDoc("materials", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeTexture() {
        if (++totalTextures > kMaxTotalTextures) {
            std::string msg = "MC3: document exceeds the total texture budget (" +
                std::to_string(kMaxTotalTextures) + ")";
            reportErrorDoc("textures", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeEmbed(size_t base64Bytes) {
        if (++totalEmbeds > kMaxTotalEmbeds) {
            std::string msg = "MC3: document exceeds the total embed budget (" +
                std::to_string(kMaxTotalEmbeds) + ")";
            reportErrorDoc("embeds", msg);
            throw std::runtime_error(msg);
        }
        totalEmbedBytes += static_cast<long long>(base64Bytes);
        if (totalEmbedBytes > kMaxTotalEmbedBytes) {
            std::string msg = "MC3: document's total embed base64 content (" +
                std::to_string(totalEmbedBytes) + " bytes) exceeds the combined budget (" +
                std::to_string(kMaxTotalEmbedBytes) + " bytes) -- rejected before holding "
                "it all in memory (many embeds each individually under the per-embed cap?)";
            reportErrorDoc("embeds", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeAction() {
        if (++totalActions > kMaxTotalActions) {
            std::string msg = "MC3: document exceeds the total action budget (" +
                std::to_string(kMaxTotalActions) + ")";
            reportErrorDoc("actions", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeChannel() {
        if (++totalChannels > kMaxTotalChannels) {
            std::string msg = "MC3: document exceeds the total channel budget (" +
                std::to_string(kMaxTotalChannels) + ")";
            reportErrorDoc("channels", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeKeyframe() {
        if (++totalKeyframes > kMaxTotalKeyframes) {
            std::string msg = "MC3: document exceeds the total keyframe budget (" +
                std::to_string(kMaxTotalKeyframes) + ")";
            reportErrorDoc("keyframes", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeDefinition() {
        if (++totalDefinitions > kMaxTotalDefinitions) {
            std::string msg = "MC3: document exceeds the total definition budget (" +
                std::to_string(kMaxTotalDefinitions) + ")";
            reportErrorDoc("definitions", msg);
            throw std::runtime_error(msg);
        }
    }
    void reset() {
        totalObjects = 0; totalTessellationWeight = 0; totalIncludes = 0;
        totalMaterials = 0; totalTextures = 0;
        totalEmbeds = 0; totalEmbedBytes = 0;
        totalActions = 0; totalChannels = 0; totalKeyframes = 0;
        totalDefinitions = 0;
    }
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
// at the definition site. `el` (SYS-W1-01) is the element the path came from,
// used only for the Mc3Validation entry's object identity -- may be nullptr.
static void validateResourcePathIfConfined(const XMLElement* el, const std::string& rawPath,
                                            const char* kind);

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

// SYS-W1-02: exactly-zero (or extremely-near-zero) scale on any axis
// degenerates the transform matrix to non-invertible, which breaks any
// downstream inverse-transpose normal calculation (NaN/Inf normals).
// Deliberately does NOT clamp all negative scale -- negative scale is a
// legitimate mirroring feature: mc3togltf's GltfExporter.cpp passes
// t.scale straight through to glTF's node.scale (GltfExporter.cpp:830),
// and glTF's own spec explicitly supports negative scale for mirroring.
// Only magnitude near zero is degenerate, regardless of sign; the sign is
// preserved when clamping so a tiny-but-intentionally-negative scale
// doesn't flip to positive.
static constexpr float kMinScaleMagnitude = 1e-4f;

static float clampScaleAxis(const XMLElement* el, float raw) {
    if (std::fabs(raw) >= kMinScaleMagnitude) return raw;
    float clamped = (raw < 0.0f) ? -kMinScaleMagnitude : kMinScaleMagnitude;
    reportWarning(el, "scale", "value " + std::to_string(raw) +
                  " has near-zero magnitude (degenerates the transform matrix)",
                  "clamped to " + std::to_string(clamped));
    return clamped;
}

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
            catch (...) {
                t.scale = {1, 1, 1};
                reportWarning(el, "scale", "value '" + s + "' is not a valid number",
                              "defaulted to 1 1 1");
            }
        } else {
            t.scale = parseVec3(s, {1,1,1});
        }
        for (int i = 0; i < 3; ++i) t.scale[i] = clampScaleAxis(el, t.scale[i]);
    }
    t.pivot = attrVec3(el, "pivot");
    return t;
}

static std::optional<Mc3Deform> parseDeform(const XMLElement* el) {
    const XMLElement* d = el->FirstChildElement("deform");
    if (!d) return std::nullopt;
    Mc3Deform def;
    def.scale = attrVec3(d, "scale", {1,1,1});
    // SYS-W1-02: deform.scale feeds the same Mat4::scaling() as the main
    // transform's scale (CsgEvaluator.cpp, GltfExporter.cpp) -- same
    // near-zero-degenerate-matrix hazard, same fix.
    for (int i = 0; i < 3; ++i) def.scale[i] = clampScaleAxis(d, def.scale[i]);
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
    cs.width       = rejectNegative(el, "width",        attrF(el, "width",        0.3f));
    cs.height      = rejectNegative(el, "height",       attrF(el, "height",       0.3f));
    cs.radius      = rejectNegative(el, "radius",       attrF(el, "radius",       0.1f));
    cs.innerRadius = rejectNegative(el, "inner_radius", attrF(el, "inner_radius", 0.0f));
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
    path.length      = rejectNegative(el, "length", attrF(el, "length",  1.0f));
    path.axis        = attr (el, "axis",   "y");
    path.arcRadius   = rejectNegative(el, "radius", attrF(el, "radius", 1.0f));
    path.arcAngle    = attrF(el, "angle",  180.0f);
    path.helixRadius = rejectNegative(el, "radius", attrF(el, "radius", 0.5f));
    path.helixHeight = rejectNegative(el, "height", attrF(el, "height", 2.0f));
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
        reportWarning(el, "cross_section/path", "<extrude> missing <cross_section> or <path>",
                      "extrude skipped");
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
            catch (...) {
                /* p.size keeps its default-constructed value */
                reportWarning(el, "size", "value '" + s + "' is not a valid number",
                              "kept default size");
            }
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
        // SYS-W1-02: a negative box/plane/grid dimension is nonsensical
        // (see rejectNegative's comment above) regardless of which of the
        // three size-parsing branches above produced it.
        p.size[0] = rejectNegative(el, "size", p.size[0]);
        p.size[1] = rejectNegative(el, "size", p.size[1]);
        p.size[2] = rejectNegative(el, "size", p.size[2]);
    }
    p.radius        = rejectNegative(el, "radius", attrF(el, "radius", 0.5f));
    p.height        = rejectNegative(el, "height", attrF(el, "height", 1.0f));
    // IcoSphere default "segments" is 2; all other primitives default to 32.
    // The count is clamped to a safe upper bound so a hostile value can't drive
    // unbounded allocation. Note IcoSphere reuses the same 0..32-style scale as
    // Sphere: buildIcoSphere() internally maps it to min(4, segments/8)
    // subdivisions, so its actual triangle count is already bounded there — the
    // clamp here just stops the raw integer from being absurd.
    p.segments      = attrCountBudgeted(el, "segments", type == ObjectType::IcoSphere ? 2 : 32, 0);
    p.axis          = attr (el, "axis",           "y");
    p.majorRadius   = rejectNegative(el, "major_radius", attrF(el, "major_radius", 0.35f));
    if (type == ObjectType::Disk) {
        // Disk uses inner_radius (0=solid); accept legacy minor_radius for compat.
        // -1.0f is a sentinel meaning "attribute absent" (any legitimate
        // inner_radius is >= 0), so a negative inner_radius -- whether the
        // sentinel default or a genuinely-authored negative value -- falls
        // back to the legacy minor_radius attribute (itself still checked
        // below); an explicitly-negative attribute additionally gets a
        // diagnostic so the mistake isn't silently swallowed.
        float ir = attrF(el, "inner_radius", -1.0f);
        if (ir < 0.0f) {
            if (el->Attribute("inner_radius"))
                reportWarning(el, "inner_radius", "value " + std::to_string(ir) +
                              " is negative (dimensions/radii must be >= 0)",
                              "ignored, falling back to legacy minor_radius/default");
            ir = attrF(el, "minor_radius", 0.0f);
        }
        p.minorRadius = rejectNegative(el, "minor_radius", ir);
    } else {
        p.minorRadius = rejectNegative(el, "minor_radius", attrF(el, "minor_radius", 0.15f));
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
    obj.scriptId  = attr(el, "script");
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

    // R111 -- structured asset metadata (mesh_world_revival.md §6).
    if (const XMLElement* ame = el->FirstChildElement("assetMetadata")) {
        Mc3AssetMetadata am;
        am.category            = attr(ame, "category");
        am.subcategory          = attr(ame, "subcategory");
        am.facing               = attr(ame, "facing");
        am.collisionProxy       = attr(ame, "collision_proxy");
        am.shadowPolicy         = attr(ame, "shadow_policy");
        am.license              = attr(ame, "license");
        am.provenance           = attr(ame, "provenance");
        am.sourceGeneratorOrHash= attr(ame, "source");
        am.semanticVersion      = attr(ame, "version");
        am.instancingEligible   = std::strcmp(attr(ame, "instancing_eligible", "true"), "false") != 0;
        am.maxVisibilityDistanceM = attrF(ame, "max_visibility_distance", 0.0f);
        am.selectionWeight        = attrF(ame, "selection_weight", 1.0f);
        if (const char* ns = ame->Attribute("nominal_size")) am.nominalSize = parseVec3(ns);
        if (const char* bmin = ame->Attribute("bounds_min")) am.boundsMin = parseVec3(bmin);
        if (const char* bmax = ame->Attribute("bounds_max")) am.boundsMax = parseVec3(bmax);
        if (const char* cv = ame->Attribute("clearance_volume")) am.clearanceVolume = parseVec3(cv);

        auto readTagList = [&](const char* tag, std::vector<std::string>& out) {
            if (const XMLElement* te = ame->FirstChildElement(tag))
                for (const XMLElement* ie = te->FirstChildElement("tag"); ie;
                     ie = ie->NextSiblingElement("tag"))
                    if (const char* v = ie->Attribute("value")) out.emplace_back(v);
        };
        readTagList("semanticTags", am.semanticTags);
        readTagList("styleTags",    am.styleTags);
        readTagList("regionTags",   am.regionTags);
        readTagList("periodTags",   am.periodTags);
        readTagList("materialSlots", am.materialSlots);

        if (const XMLElement* se = ame->FirstChildElement("sockets"))
            for (const XMLElement* pe = se->FirstChildElement("socket"); pe;
                 pe = pe->NextSiblingElement("socket"))
                if (const char* name = pe->Attribute("name"))
                    if (const char* pos = pe->Attribute("position"))
                        am.sockets[name] = parseVec3(pos);

        if (const XMLElement* le = ame->FirstChildElement("lods"))
            for (const XMLElement* te = le->FirstChildElement("lod"); te;
                 te = te->NextSiblingElement("lod"))
                if (const char* tier = te->Attribute("tier"))
                    if (const char* defId = te->Attribute("definition"))
                        am.lods[tier] = defId;

        obj.assetMetadata = std::move(am);
    }
}

// SYS-W1-03: a single object with an enormous number of DIRECT children
// (breadth) is a distinct pathological shape from either total object count
// (kMaxTotalObjects, chargeObject() above) or nesting DEPTH (bounded by
// tinyxml2's own built-in TINYXML2_MAX_ELEMENT_DEPTH=500 -- see
// mc3_document_budget_test's recursion-depth coverage). A single <group>
// with, say, 25,000 trivial direct <box> children stays comfortably under
// kMaxTotalObjects (100,000) yet produces one absurdly wide node that chokes
// any per-child linear-scan UI code (hierarchy panel, selection) or a
// non-virtualized tree widget. This is a LOCAL per-call counter, not a
// DocumentBudget running total -- "per node" is inherently scoped to one
// parseChildren() call, unlike the whole-document dimensions above.
//
// Deliberately scoped to actual nested-group children only (parseChildren,
// used for group/union/difference/intersection/area), not the top-level
// <objects>/<definitions> lists -- top-level breadth is already effectively
// the same dimension as total object count when there's no nesting, so a
// separate cap there would be redundant.
static constexpr int kMaxChildrenPerNode = 20'000;

static void parseChildren(const XMLElement* el, Mc3Object& obj) {
    int childIndex = 0;
    for (const XMLElement* c = el->FirstChildElement(); c; c = c->NextSiblingElement()) {
        if (++childIndex > kMaxChildrenPerNode) {
            std::string msg = "MC3: object '" + objectIdentity(el) +
                "' exceeds the per-node children budget (" +
                std::to_string(kMaxChildrenPerNode) + ") -- rejected (a hostile wide "
                "fan-out under a single node?)";
            reportError(el, "children", msg);
            throw std::runtime_error(msg);
        }
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
        validateResourcePathIfConfined(el, obj->meshSource, "mesh source");
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
        reportWarning(el, "type", "unknown object type <" + tag + ">", "object skipped");
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
        if (s->Attribute("scale")) {
            st.scale = attrVec3(s, "scale", {1,1,1});
            // SYS-W1-02: a named state's scale overrides the base
            // transform's scale when the state is applied -- same
            // near-zero-degenerate-matrix hazard as parseTransform's scale.
            for (int i = 0; i < 3; ++i) (*st.scale)[i] = clampScaleAxis(s, (*st.scale)[i]);
        }
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
        // SYS-W1-02: start should be < end for linear fog to actually
        // produce a gradient. NOT clamped/swapped here -- SceneRenderer.cpp
        // (I3 per-object fog) already defensively guards `if (f.end >
        // f.start)` before computing the blend factor, so a start>=end
        // config is already safe (renders as "no fog", never a division by
        // zero or NaN); this is a warning-only diagnostic to help authors
        // spot a likely mistake, not a value repair.
        if (fog.start >= fog.end)
            reportWarning(fg, "start",
                          "fog start (" + std::to_string(fog.start) +
                          ") is not less than fog end (" + std::to_string(fog.end) +
                          "); linear fog will render as fully absent rather than a gradient");
        // Negative density inverts the exponential falloff's intended
        // direction (fog would visually strengthen with camera distance
        // instead of weakening) -- already numerically safe downstream
        // (SceneRenderer.cpp's std::clamp() catches the resulting -inf), but
        // clamped here anyway since it's clearly an authoring mistake, not
        // an intentional effect (unlike, say, extrude path arcAngle's
        // legitimate use of a negative value for direction).
        fog.density = rejectNegative(fg, "density", attrF(fg, "density", 0.01f));
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
            reportWarning(c, "type", "unknown light type <" + t + ">", "light ignored");
            continue;
        }
        doc.lights.push_back(light);
    }
}

// SYS-W1-02: camera field ranges (documented in MC3_FORMAT.md's Cameras
// section).
//
// fov: vertical field of view in degrees. Perspective projection matrices
// divide by tan(fov/2) -- fov == 0 makes that divide-by-zero-adjacent
// (tan(0) == 0) and fov >= 180 is not representable by a symmetric frustum
// at all (tan(90 deg) is undefined). Clamped, not rejected: an out-of-range
// FOV (e.g. a "600" typo for "60") is a common accidental authoring mistake,
// not an attack.
static constexpr float kMinCameraFovDegrees = 1.0f;
static constexpr float kMaxCameraFovDegrees = 179.0f;

// near/far clip planes: near must be strictly positive -- the standard
// perspective projection matrix divides by near, and a zero/negative near
// plane is undefined. far must exceed near by a usable margin, or the
// z-buffer's depth range collapses to nothing (near>=far also makes the
// projection matrix singular).
static constexpr float kMinCameraNear = 1e-4f;
static constexpr float kMinCameraNearFarMargin = 1e-3f;

// orthoAspect: view-volume width/height ratio -- must be strictly positive;
// zero or negative mirrors or collapses the ortho frustum.
static constexpr float kMinCameraAspect = 1e-4f;

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
        float near = clampMin(c, "near", attrF(c, "near", 0.1f), kMinCameraNear,
                               "must be > 0 (a zero/negative near plane is undefined for a "
                               "perspective projection)");
        float far = attrF(c, "far", 1000.0f);
        if (far < near + kMinCameraNearFarMargin) {
            float clampedFar = near + kMinCameraNearFarMargin;
            reportWarning(c, "far",
                          "value " + std::to_string(far) + " does not exceed near (" +
                          std::to_string(near) + ") by a usable z-buffer-precision margin",
                          "clamped to " + std::to_string(clampedFar));
            far = clampedFar;
        }
        cam.nearPlane = near;
        cam.farPlane  = far;
        cam.fov       = attrFClamped(c, "fov", 60.0f, kMinCameraFovDegrees, kMaxCameraFovDegrees);
        cam.orthoSize = attrF(c, "size",  10.0f);
        cam.orthoAspect = clampMin(c, "aspect", attrF(c, "aspect", 1.0f), kMinCameraAspect,
                                    "must be > 0 (a zero/negative aspect ratio mirrors or "
                                    "collapses the view volume)");   // STAB-0695
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
        g_budget.chargeTexture(); // SYS-W1-03
        std::string id   = attr(c, "id");
        std::string type = attr(c, "type");
        if (type == "svg") {
            Mc3SvgTexture svg;
            svg.id  = id;
            svg.src = attr(c, "src");
            validateResourcePathIfConfined(c, svg.src, "SVG texture src");
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
        validateResourcePathIfConfined(c, tex.uri, "texture uri");
        tex.wrapU      = attr(c, "wrap_u",      "repeat");
        tex.wrapV      = attr(c, "wrap_v",      "repeat");
        tex.filter     = attr(c, "filter",      "linear");
        tex.colorSpace = attr(c, "color_space", "srgb");
        tex.mipMaps    = attrB(c, "mip_maps",   true);
        doc.textures[id] = tex;
    }
}

// SYS-W1-02: material field ranges (documented in MC3_FORMAT.md's Materials
// section). roughness/metallic/occlusion_strength/alpha_cutoff, and the
// base_color alpha (opacity) channel, all follow glTF PBR's canonical [0,1]
// convention -- every consumer (SceneRenderer's live PBR shading,
// mc3togltf's glTF pbrMetallicRoughness export) assumes the convention
// holds, so an out-of-range value like roughness=-3 or metallic=5 wouldn't
// just look "unusually shiny" downstream, it would map to fundamentally
// undefined shader behavior. Clamped, not rejected -- same authoring-mistake
// judgment call as the camera ranges above.
//
// Deliberately NOT applied to base_color/emissive_color's RGB channels or
// normal_scale: emissive_color is explicitly documented (MC3_FORMAT.md) as
// allowing HDR values above 1.0 for bloom/glow, base_color's RGB has no
// established need to reject e.g. a deliberately-authored >1.0 multiplier in
// a non-physically-based art style, and normal_scale is a signed multiplier
// (glTF permits negative values to invert a normal map's effect).
static constexpr float kMinMaterialUnit = 0.0f;
static constexpr float kMaxMaterialUnit = 1.0f;

static void parseMaterials(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement("material"); c;
         c = c->NextSiblingElement("material")) {
        g_budget.chargeMaterial(); // SYS-W1-03
        Mc3Material mat;
        std::string id = attr(c, "id");
        mat.name        = id;
        mat.roughness   = clampRange(c, "roughness", attrF(c, "roughness", 0.5f),
                                      kMinMaterialUnit, kMaxMaterialUnit);
        mat.metallic    = clampRange(c, "metallic", attrF(c, "metallic", 0.0f),
                                      kMinMaterialUnit, kMaxMaterialUnit);
        mat.alphaMode   = attr (c, "alpha_mode",  "opaque");
        mat.doubleSided = attrB(c, "double_sided", false);
        std::string bcText = childText(c, "base_color");
        if (!bcText.empty()) {
            auto v = parseVec4(bcText, {0.8f,0.8f,0.8f,1.0f});
            float alpha = clampRange(c, "base_color", v[3], kMinMaterialUnit, kMaxMaterialUnit);
            mat.baseColor = {v[0], v[1], v[2], alpha};
        }
        mat.baseColorTexture         = childText(c, "base_color_texture");
        mat.metallicRoughnessTexture = childText(c, "metallic_roughness_texture");
        mat.normalTexture            = childText(c, "normal_texture");
        mat.occlusionTexture         = childText(c, "occlusion_texture");
        mat.emissiveTexture          = childText(c, "emissive_texture");
        mat.normalScale       = attrF(c, "normal_scale",      1.0f);
        mat.occlusionStrength = clampRange(c, "occlusion_strength",
                                            attrF(c, "occlusion_strength", 1.0f),
                                            kMinMaterialUnit, kMaxMaterialUnit);
        mat.alphaCutoff       = clampRange(c, "alpha_cutoff", attrF(c, "alpha_cutoff", 0.5f),
                                            kMinMaterialUnit, kMaxMaterialUnit);
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
        validateResourcePathIfConfined(c, snd.src, "sound src");
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
        validateResourcePathIfConfined(c, mus.src, "music src");
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

// SYS-W1-04: an inline <embed> body is base64-encoded GLB data held as a raw
// std::string with no consumer-side decode step yet (SYS-W14-05 is
// deferred), but the string itself is already fully materialized in memory
// at parse time regardless -- an attacker can simply submit a very large
// base64 text blob directly (no compression-bomb trick needed). Empirically
// confirmed unbounded before this fix: a 20MB inline embed body loaded with
// no error in ~120ms. 64MB of base64 text (~48MB decoded) is far beyond any
// legitimate embedded prop/mesh and matches the sanity-limit philosophy
// already used for MCB string fields (kMcbMaxStringLen in McbReader.cpp).
static constexpr size_t kMaxEmbedBase64Length = 64ull * 1024ull * 1024ull;

// SYS-W1-03 "max bytes": a raw ceiling on the INPUT XML text/file size
// itself, checked before tinyxml2 even attempts to buffer/parse it --
// deliberately NOT a DocumentBudget running total like the dimensions
// above: a single oversized document doesn't need accumulation across
// <include>s to be dangerous on its own, and checking it up front avoids
// ever handing tinyxml2 (or this process' own file-read buffer) a
// pathologically large blob in the first place. 512MB is far beyond any
// legitimate mc3 scene's XML text -- even a scene with many embedded assets
// near the existing kMaxTotalEmbedBytes=256MB aggregate cap would still be
// comfortably under this, since base64 embed content IS part of the XML
// text at that point. (This is the one budget dimension this session
// decided NOT to also express as a fuzzy "total generated output bytes"
// estimate -- that would require guessing at downstream GLB/geometry size
// across the whole document, which is imprecise enough to not usefully
// bound anything; the INPUT byte ceiling here is precise and directly
// actionable.)
static constexpr uintmax_t kMaxDocumentBytes = 512ull * 1024ull * 1024ull;

static void checkDocumentByteBudget(uintmax_t bytes, const std::string& sourceDescription) {
    if (bytes <= kMaxDocumentBytes) return;
    std::string msg = "MC3: " + sourceDescription + " (" + std::to_string(bytes) +
        " bytes) exceeds the maximum document size (" + std::to_string(kMaxDocumentBytes) +
        " bytes) -- rejected before parsing";
    reportErrorDoc("bytes", msg);
    throw std::runtime_error(msg);
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
        validateResourcePathIfConfined(c, em.src, "embed src");
        if (em.src.empty()) {
            const char* text = c->GetText();
            if (text) em.base64Content = text;
        }
        if (em.base64Content.size() > kMaxEmbedBase64Length) {
            std::string msg = "MC3: embed '" + id + "' inline base64Content length (" +
                std::to_string(em.base64Content.size()) + ") exceeds the sanity "
                "limit (" + std::to_string(kMaxEmbedBase64Length) +
                ") -- rejected before holding it in memory (corrupted or "
                "malicious file?)";
            reportError(c, "base64Content", msg,
                       "rejected (not held in memory)");
            throw std::runtime_error(msg);
        }
        g_budget.chargeEmbed(em.base64Content.size()); // SYS-W1-03
        doc.embeds[id] = std::move(em);
    }
}

static void parseDefinitions(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* c = el->FirstChildElement("definition"); c;
         c = c->NextSiblingElement("definition")) {
        g_budget.chargeDefinition(); // SYS-W1-03
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

// SYS-W1-02: time_scale is a playback-speed multiplier (STAB-0460's own
// comment: "2.0 = twice as fast, 0.5 = half speed"); a value of exactly 0
// permanently stalls the action's clock (dt * 0 == 0, forever) and a
// negative value isn't a documented/supported "play in reverse" feature
// here (unlike, say, extrude path arcAngle's legitimate negative-direction
// use) -- both are almost certainly an authoring mistake, not intentional.
// Clamped to a small positive epsilon rather than a specific fallback like
// 1.0 so a near-correct authored value (e.g. "0.001" for very slow motion)
// survives unchanged.
static constexpr float kMinTimeScale = 1e-3f;

static void parseActions(const XMLElement* el, Mc3Document& doc) {
    for (const XMLElement* ae = el->FirstChildElement("action"); ae;
         ae = ae->NextSiblingElement("action")) {
        g_budget.chargeAction(); // SYS-W1-03
        Mc3Action action;
        action.name      = attr(ae, "name");
        action.duration  = attrF(ae, "duration", 1.0f);
        action.loop      = attrB(ae, "loop", false);
        action.autoplay  = attrB(ae, "autoplay", false);
        action.timeScale = clampMin(ae, "time_scale", attrF(ae, "time_scale", 1.0f),
                                     kMinTimeScale,
                                     "must be > 0 (a zero/negative time_scale stalls or "
                                     "breaks playback)"); // STAB-0460
        if (action.name.empty()) continue;

        for (const XMLElement* ce = ae->FirstChildElement("channel"); ce;
             ce = ce->NextSiblingElement("channel")) {
            g_budget.chargeChannel(); // SYS-W1-03
            Mc3Channel ch;
            ch.targetObject = attr(ce, "target");
            auto prop = animatedPropertyFromName(attr(ce, "property", ""));
            if (!prop || ch.targetObject.empty()) continue;
            ch.property = *prop;

            for (const XMLElement* ke = ce->FirstChildElement("keyframe"); ke;
                 ke = ke->NextSiblingElement("keyframe")) {
                g_budget.chargeKeyframe(); // SYS-W1-03
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
static void validateResourcePathIfConfined(const XMLElement* el, const std::string& rawPath,
                                            const char* kind) {
    if (!g_confineResourcePaths || rawPath.empty()) return;
    if (rawPath.rfind("embed:", 0) == 0 || rawPath.rfind("data:", 0) == 0) return;

    std::filesystem::path p(rawPath);
    if (p.is_absolute()) {
        std::string msg = std::string("MC3: ") + kind + " '" + rawPath +
            "' is an absolute path outside the document root; rejected under "
            "the untrusted-content load policy";
        reportError(el, kind, msg);
        throw std::runtime_error(msg);
    }

    if (!includePathWithinRoot(g_resourceRoot / p, g_resourceRoot)) {
        std::string msg = std::string("MC3: ") + kind + " '" + rawPath +
            "' escapes the document root; rejected under the untrusted-content "
            "load policy";
        reportError(el, kind, msg);
        throw std::runtime_error(msg);
    }
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

// SYS-W1-01: RAII scope guard that temporarily points g_currentSourceFile at
// `file` for the duration of the enclosing block (restoring the previous
// value on exit, including via exception), so diagnostics reported while
// merging one <include> file are attributed to that file rather than its
// parent (or a sibling include processed earlier).
struct SourceFileScope {
    std::filesystem::path previous;
    explicit SourceFileScope(const std::filesystem::path& file)
        : previous(g_currentSourceFile) { g_currentSourceFile = file; }
    ~SourceFileScope() { g_currentSourceFile = previous; }
    SourceFileScope(const SourceFileScope&) = delete;
};

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

    if (inProgress.count(canonical)) {
        std::string msg = "Cyclic <include> detected: " + includePath.string();
        reportErrorDoc("include", msg);
        throw std::runtime_error(msg);
    }

    if (processed.count(canonical))
        return;  // already merged via a different include path — skip silently

    // AUD-006b follow-up (SYS-W1-04): charge against the fan-out budget only
    // for a genuinely new file (past the cycle/diamond-dedup checks above),
    // matching g_budget.chargeObject()'s charge-on-real-work discipline.
    // Charged (and, on overflow, reported) against the PARENT file -- the one
    // containing the offending <include> -- since g_currentSourceFile hasn't
    // been switched to includePath yet at this point.
    g_budget.chargeInclude();

    // SYS-W1-01: from here on, diagnostics are attributed to the file being
    // merged (includePath), not its parent -- restored automatically (even on
    // exception) when this function returns.
    SourceFileScope fileScope(includePath);

    // SYS-W1-03 "max bytes": checked before tinyxml2 buffers this included
    // file's contents, same as the top-level parse() below.
    {
        std::error_code ec;
        auto sz = std::filesystem::file_size(includePath, ec);
        if (!ec)
            checkDocumentByteBudget(sz, "<include> file '" + includePath.string() + "'");
    }

    XMLDocument xml;
    if (xml.LoadFile(includePath.string().c_str()) != XML_SUCCESS) {
        std::string msg = "Failed to load <include> file '" +
                           includePath.string() + "': " + xml.ErrorStr();
        reportErrorDoc("include", msg);
        throw std::runtime_error(msg);
    }

    const XMLElement* root = xml.FirstChildElement("mc3");
    if (!root) {
        std::string msg = "No <mc3> root element in included file: " + includePath.string();
        reportErrorDoc("include", msg);
        throw std::runtime_error(msg);
    }

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
                if (doc.textures.count(id) || doc.svgTextures.count(id)) {
                    std::cerr << "Warning: <include file=\"" << includePath.string()
                              << "\"> texture id '" << id
                              << "' collides with an already-loaded texture; last-write-wins.\n";
                    reportWarning(c, "id", std::string("texture id '") + id +
                                  "' collides with an already-loaded texture",
                                  "last-write-wins");
                }
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
                if (doc.materials.count(id)) {
                    std::cerr << "Warning: <include file=\"" << includePath.string()
                              << "\"> material id '" << id
                              << "' collides with an already-loaded material; last-write-wins.\n";
                    reportWarning(c, "id", std::string("material id '") + id +
                                  "' collides with an already-loaded material",
                                  "last-write-wins");
                }
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
                if (doc.definitions.count(id)) {
                    std::cerr << "Warning: <include file=\"" << includePath.string()
                              << "\"> definition id '" << id
                              << "' collides with an already-loaded definition; last-write-wins.\n";
                    reportWarning(c, "id", std::string("definition id '") + id +
                                  "' collides with an already-loaded definition",
                                  "last-write-wins");
                }
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
                if (doc.embeds.count(id)) {
                    std::cerr << "Warning: <include file=\"" << includePath.string()
                              << "\"> embed id '" << id
                              << "' collides with an already-loaded embed; last-write-wins.\n";
                    reportWarning(c, "id", std::string("embed id '") + id +
                                  "' collides with an already-loaded embed",
                                  "last-write-wins");
                }
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
    if (depth > policy.maxIncludeDepth) {
        std::string msg = "<include> nesting exceeds the policy limit (" +
            std::to_string(policy.maxIncludeDepth) + ")";
        reportErrorDoc("include", msg);
        throw std::runtime_error(msg);
    }

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
            std::string msg = "<include file=\"" + std::string(fileAttr) +
                "\"> escapes the document root (rejected by load policy)";
            reportError(inc, "file", msg);
            throw std::runtime_error(msg);
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

    // R110 -- library identity (.mc3lib.xml only; absent on ordinary
    // scene/model documents).
    if (const XMLElement* libEl = root->FirstChildElement("library")) {
        Mc3LibraryInfo lib;
        lib.libraryNamespace = attr(libEl, "namespace");
        lib.version          = attr(libEl, "version");
        lib.contentHash      = attr(libEl, "hash");
        doc.library = std::move(lib);
    }

    // R101 -- library imports (see Mc3Import's own doc comment).
    if (const XMLElement* importsEl = root->FirstChildElement("imports"))
        for (const XMLElement* impEl = importsEl->FirstChildElement("import"); impEl;
             impEl = impEl->NextSiblingElement("import")) {
            Mc3Import imp;
            imp.importNamespace = attr(impEl, "namespace");
            imp.source          = attr(impEl, "source");
            imp.hash            = attr(impEl, "hash");
            doc.imports.push_back(std::move(imp));
        }

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
        reportWarningDoc("include", "<include> present but ignored under the current "
                         "load policy (untrusted/AI content)", "include(s) skipped");
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
                                const Mc3LoadPolicy& policy,
                                Mc3Validation* validation) {
    // SYS-W1-01: set the diagnostics sink and current-source-file BEFORE the
    // very first thing that can fail, so even a "file won't load"/"no <mc3>
    // root" rejection is captured for a caller that passed a validation sink.
    ValidationScope vscope(validation);
    g_currentSourceFile = path;

    // SYS-W1-03 "max bytes": checked before tinyxml2 buffers the file.
    {
        std::error_code ec;
        auto sz = std::filesystem::file_size(path, ec);
        if (!ec) checkDocumentByteBudget(sz, "document '" + path.string() + "'");
    }

    XMLDocument xml;
    if (xml.LoadFile(path.string().c_str()) != XML_SUCCESS) {
        std::string msg = "Failed to load XML: " + path.string() + ": " + xml.ErrorStr();
        reportErrorDoc("file", msg);
        throw std::runtime_error(msg);
    }
    const XMLElement* root = xml.FirstChildElement("mc3");
    if (!root) {
        std::string msg = "Root element <mc3> not found in " + path.string();
        reportErrorDoc("root", msg);
        throw std::runtime_error(msg);
    }
    return buildDocumentFromRoot(root, path, policy);
}

Mc3Document Mc3XmlParser::parseString(const std::string& xmlText,
                                      const std::filesystem::path& sourceDir,
                                      const Mc3LoadPolicy& policy,
                                      Mc3Validation* validation) {
    ValidationScope vscope(validation);
    // Synthetic self-path so relative includes/resources resolve against
    // sourceDir; also used as the diagnostics source-path until/unless an
    // <include> switches it (see SourceFileScope).
    std::filesystem::path selfPath = sourceDir / "in-memory.mc3.xml";
    g_currentSourceFile = selfPath;

    // SYS-W1-03 "max bytes": checked before tinyxml2 buffers/parses the
    // in-memory string (the caller already holds it in memory, but this
    // still bounds any further work this parser would otherwise do on top
    // of an already-oversized string).
    checkDocumentByteBudget(xmlText.size(), "in-memory XML document");

    XMLDocument xml;
    if (xml.Parse(xmlText.c_str(), xmlText.size()) != XML_SUCCESS) {
        std::string msg = std::string("Failed to parse XML: ") + xml.ErrorStr();
        reportErrorDoc("file", msg);
        throw std::runtime_error(msg);
    }
    const XMLElement* root = xml.FirstChildElement("mc3");
    if (!root) {
        std::string msg = "Root element <mc3> not found in in-memory document";
        reportErrorDoc("root", msg);
        throw std::runtime_error(msg);
    }
    return buildDocumentFromRoot(root, selfPath, policy);
}
