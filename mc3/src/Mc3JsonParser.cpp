#include "Mc3JsonParser.hpp"
#include <MeshCraft/Mc3/Mc3Extrude.hpp>
#include <MeshCraft/Mc3/Mc3EventBinding.hpp>
#include <MeshCraft/Mc3/Mc3SceneState.hpp>
#include <MeshCraft/Mc3/Mc3Trigger.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mc3::Internal;

namespace {

// AUD-005 clamps every segments/sides/subdivisions field to this ceiling on
// the XML load path (Mc3XmlParser.cpp's own kMaxTessellation) because these
// counts come from untrusted input and directly drive geometry allocation
// -- e.g. an unclamped "segments": 100000000 requests ~5e15 vertices, and
// AUD-064 found that even individually-legal Grid subdivisionsX/Z can
// multiply into a per-frame freeze. This JSON loader read every one of
// these fields with a raw get<int>() and applied no bound at all, so a
// hand-edited or AI-generated .mc3.json bypassed that hardening entirely.
// Mirrors the same per-field ceiling here. Still not wired into
// Mc3XmlParser.cpp's document-wide total-tessellation-weight budget
// (AUD-059's thread_local DocumentBudget, file-static with no shared
// header) -- that cross-document protection remains a separate, larger
// follow-up. This parser's other pre-existing gap noted here at the time
// (never honoring Mc3LoadPolicy at all) is now fixed -- see
// validateResourcePathIfConfined below (AUD-068).
constexpr int kMaxTessellation = 4096;

// SYS-W1-08: mirrors Mc3XmlParser.cpp's own g_validation/reportWarning/
// reportError/reportWarningDoc/reportErrorDoc thread_local pattern exactly
// (same reasoning: toObject/toPrimitive/toCrossSection/toPath/toExtrude form
// a recursive call graph with no existing context-object parameter to thread
// a diagnostics sink through). `g_validation` is set once per top-level
// parse()/parseString() call (nullptr when the caller didn't ask for
// diagnostics -- every report*() call below is then a no-op).
thread_local Mc3Validation* g_validation = nullptr;
thread_local std::filesystem::path g_currentSourceFile;

struct ValidationScope {
    explicit ValidationScope(Mc3Validation* v) { g_validation = v; }
    ~ValidationScope() { g_validation = nullptr; }
    ValidationScope(const ValidationScope&) = delete;
};

// Best-effort object identity for a diagnostic: an object's "id" if present
// and non-empty, else its "name". Unlike Mc3XmlParser.cpp (which always has
// an XMLElement to fall back to the tag name), a JSON object's own type name
// is read as a plain field rather than being intrinsic to the node, so there
// is no equivalent fallback here -- callers that already know it (e.g.
// toObject, which has just read "type") pass it as `field`/`message` text
// instead.
std::string jsonObjectIdentity(const json& j) {
    if (!j.is_object()) return {};
    if (auto it = j.find("id"); it != j.end() && it->is_string() && !it->get<std::string>().empty())
        return it->get<std::string>();
    if (auto it = j.find("name"); it != j.end() && it->is_string() && !it->get<std::string>().empty())
        return it->get<std::string>();
    return {};
}

void reportWarning(const std::string& identity, const char* field,
                    const std::string& message, const std::string& repair = {}) {
    if (!g_validation) return;
    g_validation->addWarning(g_currentSourceFile.string(), identity, field, message, repair);
}

void reportError(const std::string& identity, const char* field,
                  const std::string& message, const std::string& repair = {}) {
    if (!g_validation) return;
    g_validation->addError(g_currentSourceFile.string(), identity, field, message, repair);
}

// Whole-document findings (no single object is responsible), e.g. a
// document-wide budget overflow. Unlike Mc3XmlParser.cpp, there is no
// reportWarningDoc() counterpart here: JSON has no <include>-equivalent
// merge concept (this file's own established scope decision, see
// validateResourcePathIfConfined's header comment), which is the only thing
// XML's reportWarningDoc() is used for.
void reportErrorDoc(const char* field, const std::string& message,
                     const std::string& repair = {}) {
    if (!g_validation) return;
    g_validation->addError(g_currentSourceFile.string(), std::string{}, field, message, repair);
}

// `ownerIdentity` is the enclosing object's id/name (JSON nests
// segments/sides/subdivisions under "primitive"/"crossSection"/"extrude"
// sub-objects that have no id/name of their own, unlike XML's flat
// attributes on the object element itself -- see Mc3XmlParser.cpp's
// attrCount(), which this mirrors).
int clampTess(const std::string& ownerIdentity, const char* field, int v,
              int minv, int maxv = kMaxTessellation) {
    int clamped = std::clamp(v, minv, maxv);
    if (clamped != v)
        reportWarning(ownerIdentity, field,
                      "value " + std::to_string(v) + " is outside the allowed range [" +
                      std::to_string(minv) + ", " + std::to_string(maxv) + "]",
                      "clamped to " + std::to_string(clamped));
    return clamped;
}

// 2026-07-20 audit F4: the "separate, larger follow-up" flagged in the
// comment above -- this parser had per-field tessellation clamps
// (kMaxTessellation above) but none of Mc3XmlParser.cpp's document-wide
// running-total budgets (object count, aggregate tessellation weight,
// materials/textures/embeds/actions/channels/keyframes/definitions
// counts, embed byte total). A document with many objects each
// individually under the per-field cap can still sum to an enormous
// aggregate allocation (e.g. 100,000 objects at segments=4096 each), and
// this parser also read the ENTIRE input file into memory unconditionally
// before any size check at all (parse(), below), unlike
// Mc3XmlParser.cpp's checkDocumentByteBudget() which checks file_size()
// BEFORE the file is ever opened for reading.
//
// Mirrors Mc3XmlParser.cpp's own DocumentBudget exactly (same constants,
// same charge*() names) except totalIncludes/chargeInclude(): JSON has no
// <include>-equivalent merge concept at all (AUD-068's own established
// scope decision -- `includes` round-trips as an inert passthrough list),
// so there is nothing for that one dimension to bound here.
struct DocumentBudget {
    long long totalObjects = 0;
    long long totalTessellationWeight = 0;
    long long totalMaterials = 0;
    long long totalTextures = 0;
    long long totalEmbeds = 0;
    long long totalEmbedBytes = 0;
    long long totalActions = 0;
    long long totalClips = 0;
    long long totalChannels = 0;
    long long totalKeyframes = 0;
    long long totalDefinitions = 0;

    static constexpr long long kMaxTotalObjects = 100'000;
    static constexpr long long kMaxTotalTessellationWeight = 500'000;
    static constexpr long long kMaxTotalMaterials = 20'000;
    static constexpr long long kMaxTotalTextures = 20'000;
    static constexpr long long kMaxTotalEmbeds = 1'000;
    static constexpr long long kMaxTotalEmbedBytes = 256ll * 1024 * 1024; // 256MB combined
    static constexpr long long kMaxTotalActions = 10'000;
    static constexpr long long kMaxTotalClips = 100'000;
    static constexpr long long kMaxTotalChannels = 200'000;
    static constexpr long long kMaxTotalKeyframes = 2'000'000;
    static constexpr long long kMaxTotalDefinitions = 20'000;

    void chargeObject() {
        if (++totalObjects > kMaxTotalObjects) {
            std::string msg = "MC3: document exceeds the total object budget (" +
                std::to_string(kMaxTotalObjects) + ") -- rejected before allocating "
                "geometry for all of them";
            reportErrorDoc("objects", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeTessellation(int weight) {
        totalTessellationWeight += weight;
        if (totalTessellationWeight > kMaxTotalTessellationWeight) {
            std::string msg = "MC3: document's total tessellation complexity (sum of all "
                "segments/sides/subdivisions values, " + std::to_string(totalTessellationWeight) +
                ") exceeds the budget (" + std::to_string(kMaxTotalTessellationWeight) +
                ") -- rejected before allocating geometry for all of it";
            reportErrorDoc("tessellation", msg);
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
    void chargeClip() {
        if (++totalClips > kMaxTotalClips) {
            std::string msg = "MC3: document exceeds the total animation clip budget (" +
                std::to_string(kMaxTotalClips) + ")";
            reportErrorDoc("clips", msg);
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
        totalObjects = 0; totalTessellationWeight = 0;
        totalMaterials = 0; totalTextures = 0;
        totalEmbeds = 0; totalEmbedBytes = 0;
        totalActions = 0; totalClips = 0; totalChannels = 0; totalKeyframes = 0;
        totalDefinitions = 0;
    }
};

thread_local DocumentBudget g_budget;

// Mirrors Mc3XmlParser.cpp's own kMaxDocumentBytes/checkDocumentByteBudget:
// checked BEFORE the input is buffered into memory (parse()) or, for
// parseString(), before json::parse() touches the already-in-memory
// string -- still bounds any further work this parser would otherwise do
// on top of an already-oversized document.
constexpr uintmax_t kMaxDocumentBytes = 512ull * 1024ull * 1024ull;

void checkDocumentByteBudget(uintmax_t bytes, const std::string& sourceDescription) {
    if (bytes <= kMaxDocumentBytes) return;
    std::string msg = "MC3: " + sourceDescription + " (" + std::to_string(bytes) +
        " bytes) exceeds the maximum document size (" + std::to_string(kMaxDocumentBytes) +
        " bytes) -- rejected before parsing";
    reportErrorDoc("bytes", msg);
    throw std::runtime_error(msg);
}

// AUD-068: parseString()'s policy parameter used to be entirely unused
// (`const Mc3LoadPolicy& /*policy*/`) -- confineResourcePathsToRoot was a
// silent no-op on this load path, so an untrusted .mc3.json (e.g. from a
// future JSON-based AI-apply/import caller -- no production call site
// currently passes untrusted() to this parser, so this was latent, not yet
// exploited) could name an absolute or `..`-escaping meshSource/texture
// uri/SVG src/embed src/sound src/music src and it would pass straight
// through unvalidated, unlike the equivalent Mc3XmlParser.cpp path.
//
// Scope: only confineResourcePathsToRoot is enforced here, deliberately.
// allowIncludes/confineIncludesToRoot/maxIncludeDepth are NOT wired in --
// unlike the XML format, .mc3.json's `includes` field (parsed further
// down, see doc.includes) is an inert flat string list, never resolved/
// merged into another file's content anywhere in this codebase (confirmed:
// grep for `doc.includes` across mc3/src and src/MeshCraft turns up only
// this file's own read and Mc3JsonWriter.cpp's matching write -- no merge
// logic exists for the JSON path at all). There is therefore no
// local-file-inclusion vector here to gate; wiring those three fields in
// would be dead code enforcing a behavior this parser doesn't have.
//
// Mirrors Mc3XmlParser.cpp's own g_confineResourcePaths/g_resourceRoot +
// includePathWithinRoot exactly (same names, same logic) for consistency
// between the two independent parsers. thread_local, not a parameter
// threaded through toObject() (called recursively for children) and the
// textures/embeds/sounds/music loops in parseString() -- matching
// Mc3XmlParser.cpp's own established reasoning for choosing this pattern
// over threading a policy parameter through every parse* free function.
thread_local bool g_confineResourcePaths = false;
thread_local std::filesystem::path g_resourceRoot;

bool includePathWithinRoot(const std::filesystem::path& candidate,
                            const std::filesystem::path& rootDir) {
    std::error_code ec;
    auto r = std::filesystem::weakly_canonical(
        rootDir.empty() ? std::filesystem::path(".") : rootDir, ec);
    if (ec) return false;

    // AUD-069 (mirrors Mc3XmlParser.cpp's own includePathWithinRoot fix
    // exactly, same reasoning): `candidate` may reference a file that
    // doesn't exist on disk yet. weakly_canonical() only resolves the
    // longest EXISTING prefix of its argument and, empirically, does NOT
    // fall back to resolving a non-existent single-component relative path
    // (like a bare "model.obj" with no directory component) against the
    // current directory at all -- it returns such a path unchanged, still
    // relative. Canonicalizing `candidate` on its own therefore made the
    // result depend on whether the target file exists.
    // std::filesystem::absolute() never requires existence (it
    // unconditionally prepends current_path() to a relative path), so
    // making `candidate` absolute FIRST, then weakly_canonical-ing that
    // combined absolute path as a single unit, resolves it consistently
    // with `r` above regardless of whether the file exists.
    auto absCandidate = std::filesystem::absolute(candidate, ec);
    if (ec) return false;
    auto c = std::filesystem::weakly_canonical(absCandidate, ec);
    if (ec) return false;

    auto rel = std::filesystem::relative(c, r, ec);
    if (ec || rel.empty()) return false;
    return rel.generic_string().rfind("..", 0) != 0;  // does not start with ".."
}

// Validates a texture/SVG/mesh/embed/sound/music `src`/`uri` field against
// the current parse's confinement policy. No-op when not confining or when
// `rawPath` is empty or a non-filesystem pseudo-path (`embed:`/`data:`).
void validateResourcePathIfConfined(const std::string& rawPath, const char* kind,
                                     const std::string& identity = {}) {
    if (!g_confineResourcePaths || rawPath.empty()) return;
    if (rawPath.rfind("embed:", 0) == 0 || rawPath.rfind("data:", 0) == 0) return;

    std::filesystem::path p(rawPath);
    // is_absolute() requires a root-name (e.g. a Windows drive letter) AND a
    // root-directory on Windows, so a POSIX-style rooted path like
    // "/etc/passwd" -- exactly the same filesystem-root escape attempt
    // is_absolute() exists to catch on POSIX -- is false there. Treat
    // root-directory-without-root-name as absolute too, so an untrusted
    // document is rejected with the same message on every platform.
    if (p.is_absolute() || (p.has_root_directory() && !p.has_root_name())) {
        std::string msg = std::string("MC3: ") + kind + " '" + rawPath +
            "' is an absolute path outside the document root; rejected "
            "under the untrusted-content load policy";
        reportError(identity, kind, msg);
        throw std::runtime_error(msg);
    }
    if (!includePathWithinRoot(g_resourceRoot / p, g_resourceRoot)) {
        std::string msg = std::string("MC3: ") + kind + " '" + rawPath +
            "' escapes the document root; rejected under the "
            "untrusted-content load policy";
        reportError(identity, kind, msg);
        throw std::runtime_error(msg);
    }
}

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

Mc3Primitive toPrimitive(const json& j, const std::string& ownerIdentity) {
    Mc3Primitive p;
    if (!j.is_object()) return p;
    if (j.contains("primitiveType")) p.primitiveType = primitiveTypeFromName(j["primitiveType"].get<std::string>());
    if (j.contains("size"))          p.size          = toVec3(j["size"], {1.f,1.f,1.f});
    if (j.contains("radius"))        p.radius        = j["radius"].get<float>();
    if (j.contains("height"))        p.height        = j["height"].get<float>();
    if (j.contains("segments"))      p.segments      = clampTess(ownerIdentity, "segments", j["segments"].get<int>(), 0);
    if (j.contains("axis"))          p.axis          = j["axis"].get<std::string>();
    if (j.contains("majorRadius"))   p.majorRadius   = j["majorRadius"].get<float>();
    if (j.contains("minorRadius"))   p.minorRadius   = j["minorRadius"].get<float>();
    if (j.contains("subdivisionsX")) p.subdivisionsX = clampTess(ownerIdentity, "subdivisionsX", j["subdivisionsX"].get<int>(), 1);
    if (j.contains("subdivisionsZ")) p.subdivisionsZ = clampTess(ownerIdentity, "subdivisionsZ", j["subdivisionsZ"].get<int>(), 1);
    g_budget.chargeTessellation(p.segments);
    g_budget.chargeTessellation(p.subdivisionsX);
    g_budget.chargeTessellation(p.subdivisionsZ);
    return p;
}

Mc3CrossSection toCrossSection(const json& j, const std::string& ownerIdentity) {
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
    if (j.contains("sides"))       cs.sides       = clampTess(ownerIdentity, "sides", j["sides"].get<int>(), 3);
    if (j.contains("segments"))    cs.segments    = clampTess(ownerIdentity, "segments", j["segments"].get<int>(), 1);
    g_budget.chargeTessellation(cs.sides);
    g_budget.chargeTessellation(cs.segments);
    // 2026-07-20 audit F6: mirrors Mc3XmlParser.cpp's own parseCrossSection()
    // fix exactly -- unlike sides/segments above, this child list had no
    // count cap at all.
    if (j.contains("customPoints")) {
        for (const auto& pt : j["customPoints"]) {
            if (static_cast<long long>(cs.customPoints.size()) >= kMaxTessellation) {
                std::string msg = "MC3: crossSection exceeds the maximum "
                    "customPoints count (" + std::to_string(kMaxTessellation) + ")";
                reportError(ownerIdentity, "customPoints", msg);
                throw std::runtime_error(msg);
            }
            cs.customPoints.push_back({pt.at(0).get<float>(), pt.at(1).get<float>()});
        }
    }
    return cs;
}

Mc3ExtrudePath toPath(const json& j, const std::string& ownerIdentity) {
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
    // 2026-07-20 audit F6: mirrors Mc3XmlParser.cpp's own parsePath() fix
    // exactly -- unbounded <path> point list.
    if (j.contains("points")) {
        for (const auto& pe : j["points"]) {
            if (static_cast<long long>(path.points.size()) >= kMaxTessellation) {
                std::string msg = "MC3: path exceeds the maximum points count (" +
                    std::to_string(kMaxTessellation) + ")";
                reportError(ownerIdentity, "points", msg);
                throw std::runtime_error(msg);
            }
            Mc3PathPoint pt;
            if (pe.contains("position"))  pt.position  = toVec3(pe["position"]);
            if (pe.contains("controlIn")) pt.controlIn = toVec3(pe["controlIn"]);
            path.points.push_back(pt);
        }
    }
    return path;
}

Mc3Extrude toExtrude(const json& j, const std::string& ownerIdentity) {
    Mc3Extrude ex;
    if (!j.is_object()) return ex;
    if (j.contains("crossSection")) ex.crossSection = toCrossSection(j["crossSection"], ownerIdentity);
    if (j.contains("path"))         ex.path         = toPath(j["path"], ownerIdentity);
    if (j.contains("twist"))        ex.twist        = j["twist"].get<float>();
    if (j.contains("segments"))     ex.segments     = clampTess(ownerIdentity, "segments", j["segments"].get<int>(), 1);
    if (j.contains("smooth"))       ex.smooth       = j["smooth"].get<bool>();
    if (j.contains("caps"))         ex.caps         = j["caps"].get<bool>();
    g_budget.chargeTessellation(ex.segments);
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

// Recognized `type` field values (objectTypeFromName's own mapping, minus
// its "unknown -> Group" fallback branch).
bool isKnownObjectTypeName(const std::string& s) {
    static const std::set<std::string> kKnown = {
        "group", "box", "cube", "sphere", "cylinder", "cone", "plane", "torus",
        "capsule", "disk", "grid", "icosphere", "mesh", "extrude", "instance",
        "union", "difference", "intersection", "area",
    };
    return kKnown.count(s) != 0;
}

std::shared_ptr<Mc3Object> toObject(const json& j) {
    g_budget.chargeObject();
    auto obj = std::make_shared<Mc3Object>();
    if (!j.is_object()) return obj;

    // Only computed when a validation sink is actually active: this is
    // called once per object across the whole document tree, so for the
    // overwhelmingly common case (no Mc3Validation& passed at all) it must
    // stay a no-op rather than pay for a JSON lookup nothing will ever read.
    const std::string identity = g_validation ? jsonObjectIdentity(j) : std::string{};
    const std::string rawType = j.value("type", "group");
    if (g_validation && !isKnownObjectTypeName(rawType))
        reportWarning(identity, "type", "unknown object type '" + rawType + "'",
                      "defaulted to group");
    obj->type = objectTypeFromName(rawType);
    obj->id       = j.value("id", "");
    obj->name     = j.value("name", "");
    if (j.contains("transform")) obj->transform = toTransform(j["transform"]);
    obj->material = j.value("material", "");
    obj->visible  = j.value("visible", true);
    obj->collision = j.value("collision", "none");
    obj->layer    = j.value("layer", "");
    obj->scriptId = j.value("script", "");
    if (j.contains("tags"))
        obj->tags = j["tags"].get<std::vector<std::string>>();
    obj->isCutter = j.value("role", "") == "cutter";

    if (j.contains("deform"))
        obj->deform = Mc3Deform{ toVec3(j["deform"].value("scale", json::array({1.f,1.f,1.f})), {1.f,1.f,1.f}) };
    if (j.contains("uvMapping")) obj->uvMapping = toUvMapping(j["uvMapping"]);
    if (j.contains("primitive")) obj->primitive = toPrimitive(j["primitive"], identity);
    if (j.contains("extrude"))   obj->extrude   = toExtrude(j["extrude"], identity);
    if (j.contains("csgOperation")) {
        const std::string t = j["csgOperation"].value("csgType", "union");
        Mc3CsgOperation csg;
        if (t == "difference")        csg.csgType = CsgType::Difference;
        else if (t == "intersection") csg.csgType = CsgType::Intersection;
        else                           csg.csgType = CsgType::Union;
        obj->csgOperation = csg;
    }

    if (j.contains("meshSource")) {
        obj->meshSource = j["meshSource"].get<std::string>();
        validateResourcePathIfConfined(obj->meshSource, "mesh source", identity);
    }

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

json parseJsonOrThrow(const std::string& jsonText) {
    try {
        return json::parse(jsonText);
    } catch (const json::parse_error& e) {
        std::string msg = std::string("Failed to parse mc3.json: ") + e.what();
        reportErrorDoc("parse", msg);
        throw std::runtime_error(msg);
    }
}

// SYS-W1-08: shared by parse()/parseString() below -- each of those installs
// its OWN ValidationScope/g_currentSourceFile/byte-budget check first (a real
// file gets tagged with its real path; an in-memory parse gets a synthetic
// "in-memory.mc3.json" path), matching Mc3XmlParser.cpp's independent
// parse()/parseString() entry points rather than having one delegate to the
// other and stomp on the other's source-file tag.
Mc3Document buildDocumentFromJson(const json& j, const std::filesystem::path& sourceDir,
                                   const Mc3LoadPolicy& policy) {
    // AUD-068: set the resource-confinement state for this parse before any
    // texture/SVG/mesh/embed/sound/music field is read (matches
    // Mc3XmlParser.cpp's buildDocumentFromRoot() ordering).
    g_confineResourcePaths = policy.confineResourcePathsToRoot;
    g_resourceRoot = sourceDir;

    // 2026-07-20 audit F4: reset once per top-level parse, matching
    // Mc3XmlParser.cpp's own g_budget.reset() discipline, so a later
    // unrelated parse on the same thread (e.g. the next test in the same
    // process) starts fresh rather than accumulating across calls.
    g_budget.reset();

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
            g_budget.chargeTexture();
            const std::string id = te.value("id", "");
            if (te.value("type", "bitmap") == "svg") {
                Mc3SvgTexture svg;
                svg.id = id;
                svg.src = te.value("src", "");
                validateResourcePathIfConfined(svg.src, "SVG texture src", id);
                svg.inlineContent = te.value("inlineContent", "");
                svg.wrapU = te.value("wrapU", svg.wrapU);
                svg.wrapV = te.value("wrapV", svg.wrapV);
                svg.filter = te.value("filter", svg.filter);
                svg.colorSpace = te.value("colorSpace", svg.colorSpace);
                svg.mipMaps = te.value("mipMaps", svg.mipMaps);
                doc.svgTextures[id] = svg;
            } else {
                Mc3Texture tex;
                tex.name = te.value("name", id);
                tex.uri  = te.value("uri", "");
                validateResourcePathIfConfined(tex.uri, "texture uri", id);
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
            g_budget.chargeMaterial();
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
            validateResourcePathIfConfined(em.src, "embed src", em.id);
            em.base64Content = ee.value("base64Content", "");
            g_budget.chargeEmbed(em.base64Content.size());
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
            validateResourcePathIfConfined(snd.src, "sound src", snd.id);
            snd.loop = se.value("loop", false);
            doc.sounds[snd.id] = snd;
        }
    }

    if (j.contains("music")) {
        for (const auto& te : j["music"]) {
            Mc3Music mus;
            mus.id   = te.value("id", "");
            mus.src  = te.value("src", "");
            validateResourcePathIfConfined(mus.src, "music src", mus.id);
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

    if (j.contains("eventBindings")) {
        for (const auto& be : j["eventBindings"]) {
            Mc3EventBinding binding;
            binding.id = be.value("id", "");
            binding.sourceObjectId = be.value("source", "");
            const std::string event = be.value("event", "enter");
            if (event == "exit") binding.event = EventBindingEvent::Exit;
            else if (event == "click") binding.event = EventBindingEvent::Click;
            else if (event == "timer") binding.event = EventBindingEvent::Timer;
            const std::string targetType = be.value("targetType", "trigger");
            binding.targetType = targetType == "state" ? EventBindingTarget::SceneState
                                                        : EventBindingTarget::Trigger;
            binding.targetId = be.value("target", "");
            binding.enabled = be.value("enabled", true);
            binding.cooldown = be.value("cooldown", 0.0f);
            binding.once = be.value("once", false);
            binding.interval = be.value("timerInterval", 1.0f);
            if (!binding.id.empty() && !binding.sourceObjectId.empty() && !binding.targetId.empty())
                doc.eventBindings.push_back(std::move(binding));
        }
    }

    if (j.contains("definitions")) {
        for (const auto& de : j["definitions"]) {
            g_budget.chargeDefinition();
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
            g_budget.chargeAction();
            Mc3Action act;
            act.name      = ae.value("name", "");
            act.duration  = ae.value("duration", 1.0f);
            act.loop      = ae.value("loop", false);
            act.autoplay  = ae.value("autoplay", false);
            act.timeScale = ae.value("timeScale", 1.0f);
            if (ae.contains("clips")) {
                const float duration = std::max(1e-3f, act.duration);
                for (const auto& clipJson : ae["clips"]) {
                    g_budget.chargeClip();
                    Mc3ActionClip clip;
                    clip.name = clipJson.value("name", "");
                    if (clip.name.empty()) continue;
                    clip.startTime = std::clamp(clipJson.value("start", 0.0f), 0.0f, duration);
                    clip.endTime = std::clamp(clipJson.value("end", duration), 0.0f, duration);
                    if (clip.endTime <= clip.startTime) {
                        clip.startTime = std::min(clip.startTime, duration - 1e-3f);
                        clip.endTime = std::max(clip.startTime + 1e-3f, clip.endTime);
                    }
                    clip.playbackRate = std::max(1e-3f, clipJson.value("playbackRate", 1.0f));
                    clip.loop = clipJson.value("loop", false);
                    clip.reverse = clipJson.value("reverse", false);
                    clip.transitionDuration = std::clamp(clipJson.value("transition", 0.0f), 0.0f, 60.0f);
                    act.clips.push_back(std::move(clip));
                }
            }
            if (ae.contains("channels")) {
                for (const auto& ce : ae["channels"]) {
                    g_budget.chargeChannel();
                    Mc3Channel ch;
                    ch.targetObject = ce.value("target", "");
                    const std::string propName = ce.value("property", "");
                    if (auto prop = animatedPropertyFromName(propName))
                        ch.property = *prop;
                    if (ce.contains("keyframes")) {
                        for (const auto& ke : ce["keyframes"]) {
                            g_budget.chargeKeyframe();
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
                    // F17 (2026-07-20 audit): Mc3XmlParser.cpp sorts keyframes
                    // by time (stable, so equal-time keyframes keep their
                    // first-declared-wins order -- STAB-0468); this path
                    // never did. Mc3Animation.cpp's evaluateChannel() assumes
                    // sorted input (std::upper_bound) -- an out-of-order
                    // .mc3.json-authored channel silently interpolated wrong.
                    std::stable_sort(ch.keyframes.begin(), ch.keyframes.end(),
                        [](const Mc3Keyframe& a, const Mc3Keyframe& b){ return a.time < b.time; });
                    act.channels.push_back(std::move(ch));
                }
            }
            doc.actions[act.name] = std::move(act);
        }
    }

    return doc;
}

} // namespace

Mc3Document Mc3JsonParser::parseString(const std::string& jsonText,
                                       const std::filesystem::path& sourceDir,
                                       const Mc3LoadPolicy& policy,
                                       Mc3Validation* validation) {
    // SYS-W1-08: set the diagnostics sink and current-source-file before the
    // very first thing that can fail, mirroring Mc3XmlParser.cpp's own
    // parseString().
    ValidationScope vscope(validation);
    g_currentSourceFile = sourceDir / "in-memory.mc3.json";

    // 2026-07-20 audit F4: checked before json::parse() touches the
    // already-in-memory string, same as Mc3XmlParser.cpp's parseString().
    checkDocumentByteBudget(jsonText.size(), "in-memory JSON document");

    return buildDocumentFromJson(parseJsonOrThrow(jsonText), sourceDir, policy);
}

Mc3Document Mc3JsonParser::parse(const std::filesystem::path& path, const Mc3LoadPolicy& policy,
                                  Mc3Validation* validation) {
    // SYS-W1-08: set the diagnostics sink and current-source-file before the
    // very first thing that can fail, mirroring Mc3XmlParser.cpp's own
    // parse().
    ValidationScope vscope(validation);
    g_currentSourceFile = path;

    // 2026-07-20 audit F4: checked via file_size() BEFORE the file is ever
    // opened for reading, same as Mc3XmlParser.cpp's parse() -- unlike
    // parseString()'s check above (which still has to hold the string in
    // memory first, since the caller already does), this one avoids ever
    // buffering an oversized file into memory at all.
    {
        std::error_code ec;
        auto sz = std::filesystem::file_size(path, ec);
        if (!ec) checkDocumentByteBudget(sz, "document '" + path.string() + "'");
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::string msg = "Failed to open mc3.json: " + path.string();
        reportErrorDoc("file", msg);
        throw std::runtime_error(msg);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return buildDocumentFromJson(parseJsonOrThrow(ss.str()), path.parent_path(), policy);
}
