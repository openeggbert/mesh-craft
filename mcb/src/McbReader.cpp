#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbFormat.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Camera.hpp"
#include "MeshCraft/Mc3/Mc3Environment.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"
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
#include "MeshCraft/Mc3/Mc3Validation.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <istream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef MESHCRAFT_HAS_ZLIB
#include <zlib.h>
#endif

namespace MeshCraft::Mcb {

// ---------------------------------------------------------------------------
// SYS-W1-01: Mc3Validation reporting
// ---------------------------------------------------------------------------
//
// Mirrors the thread_local pattern used for the same reason in
// mc3/src/Mc3XmlParser.cpp: the read*() call graph here is large and already
// recursive (readObject -> readPrimitive/readTransform/... -> readObject for
// children) with no existing context-object parameter to thread a
// diagnostics sink through. `g_validation` is set once per top-level
// loadFromFile()/loadFromBinary() call (nullptr when the caller didn't ask
// for diagnostics) via ValidationScope below, and reset unconditionally on
// scope exit, so nothing can leak a stale pointer into an unrelated later
// call on the same thread.
static thread_local Mc3::Mc3Validation* g_validation = nullptr;

// The file currently being read, when known (loadFromFile knows its path;
// loadFromBinary(std::istream&) does not and leaves this empty).
static thread_local std::filesystem::path g_sourceFile;

// Best-effort "current object" identity stack. Unlike the MC3 XML parser
// (which has an XMLElement with attributes available at every clamp site),
// MCB is a flat key/value binary stream -- an object's own id/name field may
// be read before OR after the field that triggers a diagnostic, and for a
// deliberately-adversarial file may not appear at all. Each recursive
// read*() for a thing with its own identity (readObject, readLight,
// readCamera, ..., or any read*(id) helper that already knows its id from
// the enclosing map key) pushes one entry via IdentityScope and updates it
// as soon as a name/id field is actually read; anything read while that
// entry is on top of the stack (including nested sub-structures like
// transform/primitive that have no identity of their own) is attributed to
// it. This is a best-effort identity, not a guarantee -- exactly the same
// caveat plan.md accepts ("as much as is practically threadable... without a
// huge refactor").
static thread_local std::vector<std::string> g_identityStack;

struct IdentityScope {
    IdentityScope() { g_identityStack.emplace_back(); }
    explicit IdentityScope(std::string id) { g_identityStack.push_back(std::move(id)); }
    ~IdentityScope() { g_identityStack.pop_back(); }
    void set(std::string id) { g_identityStack.back() = std::move(id); }
    IdentityScope(const IdentityScope&) = delete;
};

static std::string currentIdentity() {
    return g_identityStack.empty() ? std::string{} : g_identityStack.back();
}

static void reportWarning(const char* field, const std::string& message,
                           const std::string& repair = {}) {
    if (!g_validation) return;
    g_validation->addWarning(g_sourceFile.string(), currentIdentity(), field, message, repair);
}

static void reportError(const char* field, const std::string& message,
                         const std::string& repair = {}) {
    if (!g_validation) return;
    g_validation->addError(g_sourceFile.string(), currentIdentity(), field, message, repair);
}

// RAII guard for g_validation -- set at the top of each public entry point
// (loadFromFile/loadFromBinary, both overloads), cleared again on scope exit
// (including via exception) so a caller-owned Mc3Validation never remains
// reachable via the thread_local past the end of the call it was given to.
struct ValidationScope {
    explicit ValidationScope(Mc3::Mc3Validation* v) { g_validation = v; }
    ~ValidationScope() { g_validation = nullptr; }
    ValidationScope(const ValidationScope&) = delete;
};

// ---------------------------------------------------------------------------
// 2026-07-20 audit F3: Mc3LoadPolicy resource-path confinement
// ---------------------------------------------------------------------------
//
// Mirrors Mc3XmlParser.cpp's / Mc3JsonParser.cpp's own g_confineResourcePaths/
// g_resourceRoot/includePathWithinRoot/validateResourcePathIfConfined
// exactly (same names, same logic) for consistency across all three
// independent parsers. Until this fix, McbReader had NO Mc3LoadPolicy
// integration at all -- loadFromFile()/loadFromBinary() took no policy
// parameter, so meshSource/texture uri/SVG src/embed src/sound src/music
// src were read completely unconfined, structurally unable to reject an
// absolute or `..`-escaping path the way the XML/JSON parsers already do.
//
// Deliberately scoped to confineResourcePathsToRoot only, matching
// Mc3JsonParser.cpp's own established scope decision (AUD-068): MCB is a
// binary snapshot of an already-fully-resolved document, with no
// <include>-equivalent concept at all, so allowIncludes/
// confineIncludesToRoot/maxIncludeDepth have nothing to gate here.
static thread_local bool g_confineResourcePaths = false;
static thread_local std::filesystem::path g_resourceRoot;

static bool includePathWithinRoot(const std::filesystem::path& candidate,
                                   const std::filesystem::path& rootDir) {
    std::error_code ec;
    auto r = std::filesystem::weakly_canonical(
        rootDir.empty() ? std::filesystem::path(".") : rootDir, ec);
    if (ec) return false;

    // Mirrors Mc3XmlParser.cpp's/Mc3JsonParser.cpp's own AUD-069 fix: make
    // `candidate` absolute FIRST (never requires existence), then
    // weakly_canonical the combined absolute path as a single unit, so
    // resolution doesn't depend on whether the target file exists.
    auto absCandidate = std::filesystem::absolute(candidate, ec);
    if (ec) return false;
    auto c = std::filesystem::weakly_canonical(absCandidate, ec);
    if (ec) return false;

    auto rel = std::filesystem::relative(c, r, ec);
    if (ec || rel.empty()) return false;
    return rel.generic_string().rfind("..", 0) != 0; // does not start with ".."
}

// Validates a texture/SVG/mesh/embed/sound/music `src`/`uri` field against
// the current load's confinement policy. No-op when not confining or when
// `rawPath` is empty or a non-filesystem pseudo-path (`embed:`/`data:`).
static void validateResourcePathIfConfined(const std::string& rawPath, const char* kind) {
    if (!g_confineResourcePaths || rawPath.empty()) return;
    if (rawPath.rfind("embed:", 0) == 0 || rawPath.rfind("data:", 0) == 0) return;

    std::filesystem::path p(rawPath);
    // is_absolute() requires a root-name (e.g. a Windows drive letter) AND a
    // root-directory on Windows, so a POSIX-style rooted path like
    // "/etc/passwd" -- exactly the same filesystem-root escape attempt
    // is_absolute() exists to catch on POSIX -- is false there. Treat
    // root-directory-without-root-name as absolute too, so an untrusted MCB
    // document is rejected with the same message on every platform.
    if (p.is_absolute() || (p.has_root_directory() && !p.has_root_name())) {
        std::string msg = std::string("MCB: ") + kind + " '" + rawPath +
            "' is an absolute path outside the document root; rejected "
            "under the untrusted-content load policy";
        reportError(kind, msg);
        throw std::runtime_error(msg);
    }
    if (!includePathWithinRoot(g_resourceRoot / p, g_resourceRoot)) {
        std::string msg = std::string("MCB: ") + kind + " '" + rawPath +
            "' escapes the document root; rejected under the "
            "untrusted-content load policy";
        reportError(kind, msg);
        throw std::runtime_error(msg);
    }
}

// RAII guard for g_confineResourcePaths/g_resourceRoot -- same pattern as
// ValidationScope above, set at the top of each public entry point.
struct ResourcePolicyScope {
    ResourcePolicyScope(bool confine, std::filesystem::path root) {
        g_confineResourcePaths = confine;
        g_resourceRoot = std::move(root);
    }
    ~ResourcePolicyScope() {
        g_confineResourcePaths = false;
        g_resourceRoot.clear();
    }
    ResourcePolicyScope(const ResourcePolicyScope&) = delete;
};

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
    if (len > kMcbMaxStringLen) {
        std::string msg = "MCB: string length " + std::to_string(len) +
            " exceeds sanity limit (corrupted or malicious file?)";
        reportError("string", msg);
        throw std::runtime_error(msg);
    }
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
    if (n > kMcbMaxCollectionCount) {
        std::string msg = "MCB: collection count " + std::to_string(n) +
            " exceeds sanity limit (corrupted or malicious file?)";
        reportError("collection", msg);
        throw std::runtime_error(msg);
    }
    return n;
}

// ---------------------------------------------------------------------------
// SYS-W14-25: zlib (deflate) decompression of a compressed document payload
// ---------------------------------------------------------------------------
//
// Same zip-bomb defense as every other length-prefixed field in this reader
// (kMcbMaxStringLen/kMcbMaxCollectionCount above): the claimed *uncompressed*
// size is checked against a sanity ceiling BEFORE it is used to size the
// decompression output buffer -- a tiny malicious/corrupted file (a valid
// header + one absurd uncompressed-size claim) must not force a multi-GB
// allocation before decompression even starts. 512 MB is generous for any
// legitimate mc3 scene (which is already sanity-capped well below that by
// kMcbMaxStringLen/kMcbMaxCollectionCount on every field within it).
static constexpr uint32_t kMcbMaxUncompressedPayload = 512u * 1024u * 1024u; // 512 MB
static constexpr uint32_t kMcbMaxCompressedPayload   = 512u * 1024u * 1024u; // 512 MB

#ifdef MESHCRAFT_HAS_ZLIB
// Decompresses exactly `uncompressedSize` bytes via zlib's uncompress() --
// the destination buffer is pre-sized to the claimed size (already sanity-
// checked by the caller), so a compressed stream that would inflate to MORE
// than it claims fails with Z_BUF_ERROR (destination too small) rather than
// silently over-running or growing unbounded; one that inflates to LESS
// fails because destLen won't match uncompressedSize on return.
static std::string zlibDecompress(const std::vector<uint8_t>& compressed, uint32_t uncompressedSize) {
    std::string raw(uncompressedSize, '\0');
    uLongf destLen = uncompressedSize;
    int rc = uncompress(reinterpret_cast<Bytef*>(raw.data()), &destLen,
                        compressed.data(), static_cast<uLong>(compressed.size()));
    if (rc != Z_OK) {
        std::string msg = "MCB: zlib decompression failed (code " + std::to_string(rc) +
            ") -- corrupted or malicious file?";
        reportError("flags", msg);
        throw std::runtime_error(msg);
    }
    if (destLen != uncompressedSize) {
        std::string msg = "MCB: decompressed size (" + std::to_string(destLen) +
            ") does not match the claimed size (" + std::to_string(uncompressedSize) + ")";
        reportError("flags", msg);
        throw std::runtime_error(msg);
    }
    return raw;
}
#endif

// rU32Bounded above only proves a claimed count is under kMcbMaxCollectionCount
// (10M) -- it is not a promise the stream actually CONTAINS that many
// elements. Every .reserve(n) call site below used to reserve the full
// claimed count up front, so a tiny malicious/corrupted file (a valid
// header + one oversized-but-legal count + EOF) could force a near-
// gigabyte transient allocation (e.g. 10M x a ~100-byte Mc3Light/Mc3Camera)
// before a single element was read or validated. Cap the up-front reserve
// to a small hint instead; a genuinely large, legitimate file still grows
// the vector correctly via normal amortized push_back/emplace_back
// reallocation as elements are actually read -- this only removes the
// single unconditional worst-case allocation.
static constexpr uint32_t kMcbReserveHint = 4096;
static size_t reserveHint(uint32_t n) {
    return std::min(n, kMcbReserveHint);
}

// 2026-07-20 audit F1: mirrors Mc3XmlParser.cpp's kMaxTessellation /
// Mc3JsonParser.cpp's clampTess() -- neither sibling parser ever leaves a
// tessellation-driving field (segments/sides/subdivisions) unclamped, since
// their PRODUCT (e.g. a Grid's subdivisions_x * subdivisions_z, or a
// hollow Extrude's path segments * cross-section sides) feeds vertex-buffer
// allocation downstream. This reader read all of these via a bare rI32()
// with no clamp at all -- a hostile/corrupted .mcb file could request an
// allocation many orders of magnitude beyond what any legitimate scene
// needs, reachable directly from the editor's own Open-file path.
static constexpr int kMcbMaxTessellation = 4096;
static int clampTess(int v, int minv, int maxv = kMcbMaxTessellation) {
    return std::clamp(v, minv, maxv);
}

// 2026-07-20 audit finding #1 (P0): every per-field/per-collection cap in
// this reader (kMcbMaxStringLen, kMcbMaxCollectionCount, kMcbMaxTessellation
// via clampTess, kMcbMaxUncompressedPayload/kMcbMaxCompressedPayload) bounds
// a SINGLE field/collection -- none of them bound the AGGREGATE across the
// whole document. Mc3XmlParser.cpp/Mc3JsonParser.cpp both defend against
// exactly this with a DocumentBudget (see Mc3XmlParser.cpp's own copy for
// the full per-field rationale) that this reader never got. Concretely
// exploitable before this fix: the "objects" array accepts up to
// kMcbMaxCollectionCount (10,000,000) entries, and each entry can be
// encoded as just 2 bytes (TAG_OBJ + a zero-length key immediately ending
// readObject()) -- a ~20MB crafted file could force construction of 10
// million std::shared_ptr<Mc3Object>, reachable directly from the editor's
// Open-file dialog. Mirrors Mc3XmlParser.cpp's DocumentBudget exactly (same
// constants, same charge*() call-site placement) rather than inventing a
// different set of limits for the same format.
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
            std::string msg = "MCB: document exceeds the total object budget (" +
                std::to_string(kMaxTotalObjects) + ") -- rejected before allocating"
                " geometry for all of them";
            reportError("objects", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeTessellation(int weight) {
        totalTessellationWeight += weight;
        if (totalTessellationWeight > kMaxTotalTessellationWeight) {
            std::string msg = "MCB: document's total tessellation complexity (sum of all "
                "segments/sides/subdivisions values, " +
                std::to_string(totalTessellationWeight) + ") exceeds the budget (" +
                std::to_string(kMaxTotalTessellationWeight) +
                ") -- rejected before allocating geometry for all of it";
            reportError("tessellation", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeMaterial() {
        if (++totalMaterials > kMaxTotalMaterials) {
            std::string msg = "MCB: document exceeds the total material budget (" +
                std::to_string(kMaxTotalMaterials) + ")";
            reportError("materials", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeTexture() {
        if (++totalTextures > kMaxTotalTextures) {
            std::string msg = "MCB: document exceeds the total texture budget (" +
                std::to_string(kMaxTotalTextures) + ")";
            reportError("textures", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeEmbed(size_t base64Bytes) {
        if (++totalEmbeds > kMaxTotalEmbeds) {
            std::string msg = "MCB: document exceeds the total embed budget (" +
                std::to_string(kMaxTotalEmbeds) + ")";
            reportError("embeds", msg);
            throw std::runtime_error(msg);
        }
        totalEmbedBytes += static_cast<long long>(base64Bytes);
        if (totalEmbedBytes > kMaxTotalEmbedBytes) {
            std::string msg = "MCB: document's total embed base64 content (" +
                std::to_string(totalEmbedBytes) + " bytes) exceeds the combined budget (" +
                std::to_string(kMaxTotalEmbedBytes) + " bytes) -- rejected before holding "
                "it all in memory (many embeds each individually under the per-embed cap?)";
            reportError("embeds", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeAction() {
        if (++totalActions > kMaxTotalActions) {
            std::string msg = "MCB: document exceeds the total action budget (" +
                std::to_string(kMaxTotalActions) + ")";
            reportError("actions", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeClip() {
        if (++totalClips > kMaxTotalClips) {
            std::string msg = "MCB: document exceeds the total animation clip budget (" +
                std::to_string(kMaxTotalClips) + ")";
            reportError("clips", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeChannel() {
        if (++totalChannels > kMaxTotalChannels) {
            std::string msg = "MCB: document exceeds the total channel budget (" +
                std::to_string(kMaxTotalChannels) + ")";
            reportError("channels", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeKeyframe() {
        if (++totalKeyframes > kMaxTotalKeyframes) {
            std::string msg = "MCB: document exceeds the total keyframe budget (" +
                std::to_string(kMaxTotalKeyframes) + ")";
            reportError("keyframes", msg);
            throw std::runtime_error(msg);
        }
    }
    void chargeDefinition() {
        if (++totalDefinitions > kMaxTotalDefinitions) {
            std::string msg = "MCB: document exceeds the total definition budget (" +
                std::to_string(kMaxTotalDefinitions) + ")";
            reportError("definitions", msg);
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
static thread_local DocumentBudget g_budget;

// Mirrors Mc3XmlParser.cpp's attrCountBudgeted(): clamps a tessellation
// field to the per-field ceiling AND charges it against the document-wide
// running total, at every one of this reader's 6 tessellation-driving
// field sites.
static int clampTessBudgeted(int v, int minv, int maxv = kMcbMaxTessellation) {
    int clamped = clampTess(v, minv, maxv);
    g_budget.chargeTessellation(clamped);
    return clamped;
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
// std::exception&). Mirrors CsgEvaluator.cpp's CSG_MAX_DEPTH pattern.
//
// AUD-016: both recursion sites (skipValue below, and readObject further
// down) instantiate the SAME specialization, RecursionGuard<256> — one
// template instantiation means one shared `thread_local depth_`, NOT two
// independent 256-deep counters as an earlier version of this comment
// claimed. The combined object-tree-depth + skip-path-depth therefore
// share a single 256 budget. This is harmless (strictly more conservative
// than two independent counters would be) and not worth the added
// complexity of tagging the two call sites with distinct template
// parameters just to reach a 512 combined budget no real file needs — but
// the comment must describe what the code actually does.
template <int MaxDepth>
class RecursionGuard {
public:
    RecursionGuard() {
        if (++depth_ > MaxDepth) {
            --depth_;
            std::string msg = "MCB: nesting depth exceeds " + std::to_string(MaxDepth) +
                " (corrupted or malicious file?)";
            reportError("nesting", msg);
            throw std::runtime_error(msg);
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
    default: {
        std::string msg = "MCB: unknown tag " + std::to_string(tag);
        reportError("tag", msg);
        throw std::runtime_error(msg);
    }
    }
}

// AUD-015: for every recognized key, the writer always emits a specific tag
// (McbWriter.cpp's wField*/wKeyObj/wIfI32/etc.), but the reader previously
// used `tag` only in the unknown-key fallback (`skipValue(in, tag)`) -- known
// keys were decoded purely by name, with no check that the byte stream's tag
// actually matched what that decode function expects. A corrupt/hostile file
// whose known field carries a mismatched tag (e.g. key "visible" with a
// TAG_STR length-prefixed string where a TAG_BOOL byte is expected) was not
// rejected at the field; it desynced the stream and read whatever bytes
// happened to be next as if they were the expected type. expectTag() makes a
// tag/decoder mismatch a clear, immediate error instead.
static void expectTag(uint8_t got, uint8_t want, const char* key) {
    if (got != want) {
        std::string msg = "MCB: type mismatch for key '" + std::string(key) +
                           "' (expected tag " + std::to_string(want) +
                           ", got " + std::to_string(got) + ")";
        reportError(key, msg);
        throw std::runtime_error(msg);
    }
}

// AUD-017: every enum field read from a file is a raw static_cast of an
// attacker-controlled int32, with no check that the value is one of the
// enum's real (sequential, 0-based) enumerators -- a malformed/hostile file
// could inject an out-of-range value that a downstream switch (mesh
// generation, glTF export) has no default case for. Clamp any out-of-range
// value to enumerator 0 instead, matching the unknown-key forward-
// compatibility philosophy already used throughout this reader (degrade to
// a defined value rather than reject the whole file over one bad field).
// `count` is the enum's real enumerator count and must be kept in sync by
// hand -- C++ has no reflection to derive it from the enum definition.
template <typename Enum>
static Enum clampEnum(int32_t raw, int count, const char* field) {
    if (raw >= 0 && raw < count) return static_cast<Enum>(raw);
    reportWarning(field,
                  "enum value " + std::to_string(raw) + " is out of range [0, " +
                  std::to_string(count) + ")",
                  "clamped to enumerator 0");
    return static_cast<Enum>(0);
}

// ---------------------------------------------------------------------------
// Mc3 type deserializers
// ---------------------------------------------------------------------------

static Mc3::Mc3Transform readTransform(std::istream& in) {
    Mc3::Mc3Transform tf;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "position") { expectTag(tag, TAG_VEC3, "position"); tf.position = rVec3(in); }
        else if (k == "rotation") { expectTag(tag, TAG_VEC3, "rotation"); tf.rotation = rVec3(in); }
        else if (k == "scale")    { expectTag(tag, TAG_VEC3, "scale");    tf.scale    = rVec3(in); }
        else if (k == "pivot")    { expectTag(tag, TAG_VEC3, "pivot");    tf.pivot    = rVec3(in); }
        else                      skipValue(in, tag);
    }
    return tf;
}

static Mc3::Mc3Primitive readPrimitive(std::istream& in) {
    Mc3::Mc3Primitive p;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "primitiveType") { expectTag(tag, TAG_I32, "primitiveType"); p.primitiveType = clampEnum<Mc3::PrimitiveType>(rI32(in), 11, "primitiveType"); }
        else if (k == "size")          { expectTag(tag, TAG_VEC3, "size");         p.size          = rVec3(in); }
        else if (k == "radius")        { expectTag(tag, TAG_F32, "radius");        p.radius        = rF32(in); }
        else if (k == "height")        { expectTag(tag, TAG_F32, "height");        p.height        = rF32(in); }
        else if (k == "segments")      { expectTag(tag, TAG_I32, "segments");      p.segments      = clampTessBudgeted(rI32(in), 0); }
        else if (k == "axis")          { expectTag(tag, TAG_STR, "axis");          p.axis          = rRawStr(in); }
        else if (k == "majorRadius")   { expectTag(tag, TAG_F32, "majorRadius");   p.majorRadius   = rF32(in); }
        else if (k == "minorRadius")   { expectTag(tag, TAG_F32, "minorRadius");   p.minorRadius   = rF32(in); }
        else if (k == "subdivisionsX") { expectTag(tag, TAG_I32, "subdivisionsX"); p.subdivisionsX = clampTessBudgeted(rI32(in), 1); }
        else if (k == "subdivisionsZ") { expectTag(tag, TAG_I32, "subdivisionsZ"); p.subdivisionsZ = clampTessBudgeted(rI32(in), 1); }
        else                           skipValue(in, tag);
    }
    return p;
}

static Mc3::Mc3Deform readDeform(std::istream& in) {
    Mc3::Mc3Deform d;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if (k == "scale") { expectTag(tag, TAG_VEC3, "scale"); d.scale = rVec3(in); }
        else               skipValue(in, tag);
    }
    return d;
}

static Mc3::Mc3CsgOperation readCsgOp(std::istream& in) {
    Mc3::Mc3CsgOperation csg;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if (k == "csgType") { expectTag(tag, TAG_I32, "csgType"); csg.csgType = clampEnum<Mc3::CsgType>(rI32(in), 3, "csgType"); }
        else                 skipValue(in, tag);
    }
    return csg;
}

static Mc3::Mc3CrossSection readCrossSection(std::istream& in) {
    Mc3::Mc3CrossSection cs;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "type")        { expectTag(tag, TAG_I32, "type");        cs.type        = clampEnum<Mc3::CrossSectionType>(rI32(in), 5, "type"); }
        else if (k == "width")       { expectTag(tag, TAG_F32, "width");       cs.width       = rF32(in); }
        else if (k == "height")      { expectTag(tag, TAG_F32, "height");      cs.height      = rF32(in); }
        else if (k == "radius")      { expectTag(tag, TAG_F32, "radius");      cs.radius      = rF32(in); }
        else if (k == "innerRadius") { expectTag(tag, TAG_F32, "innerRadius"); cs.innerRadius = rF32(in); }
        else if (k == "sides")       { expectTag(tag, TAG_I32, "sides");       cs.sides       = clampTessBudgeted(rI32(in), 3); }
        else if (k == "segments")    { expectTag(tag, TAG_I32, "segments");    cs.segments    = clampTessBudgeted(rI32(in), 1); }
        else if (k == "customPoints") {
            expectTag(tag, TAG_ARR, "customPoints");
            // TAG_ARR of TAG_VEC3
            uint32_t n = rU32Bounded(in);
            cs.customPoints.reserve(reserveHint(n));
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
        if      (k == "position")  { expectTag(tag, TAG_VEC3, "position");  pp.position  = rVec3(in); }
        else if (k == "controlIn") { expectTag(tag, TAG_VEC3, "controlIn"); pp.controlIn = rVec3(in); }
        else                       skipValue(in, tag);
    }
    return pp;
}

static Mc3::Mc3ExtrudePath readPath(std::istream& in) {
    Mc3::Mc3ExtrudePath p;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        // AUD-017: ExtrudePathType(5 members) was missed by the original
        // enum-clamp pass -- clampEnum applied here too now.
        if      (k == "type")        { expectTag(tag, TAG_I32, "type");        p.type        = clampEnum<Mc3::ExtrudePathType>(rI32(in), 5, "type"); }
        else if (k == "length")      { expectTag(tag, TAG_F32, "length");      p.length      = rF32(in); }
        else if (k == "axis")        { expectTag(tag, TAG_STR, "axis");        p.axis        = rRawStr(in); }
        else if (k == "arcRadius")   { expectTag(tag, TAG_F32, "arcRadius");   p.arcRadius   = rF32(in); }
        else if (k == "arcAngle")    { expectTag(tag, TAG_F32, "arcAngle");    p.arcAngle    = rF32(in); }
        else if (k == "helixRadius") { expectTag(tag, TAG_F32, "helixRadius"); p.helixRadius = rF32(in); }
        else if (k == "helixHeight") { expectTag(tag, TAG_F32, "helixHeight"); p.helixHeight = rF32(in); }
        else if (k == "helixTurns")  { expectTag(tag, TAG_F32, "helixTurns");  p.helixTurns  = rF32(in); }
        else if (k == "points") {
            expectTag(tag, TAG_ARR, "points");
            uint32_t n = rU32Bounded(in);
            p.points.reserve(reserveHint(n));
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
        if      (k == "crossSection") { expectTag(tag, TAG_OBJ, "crossSection"); ex.crossSection = readCrossSection(in); }
        else if (k == "path")         { expectTag(tag, TAG_OBJ, "path");         ex.path         = readPath(in); }
        else if (k == "twist")        { expectTag(tag, TAG_F32, "twist");        ex.twist        = rF32(in); }
        else if (k == "segments")     { expectTag(tag, TAG_I32, "segments");     ex.segments     = clampTessBudgeted(rI32(in), 1); }
        else if (k == "smooth")       { expectTag(tag, TAG_BOOL, "smooth");      ex.smooth       = rU8(in) != 0; }
        else if (k == "caps")         { expectTag(tag, TAG_BOOL, "caps");        ex.caps         = rU8(in) != 0; }
        else                          skipValue(in, tag);
    }
    return ex;
}

static Mc3::Mc3UvMapping readUvMapping(std::istream& in) {
    Mc3::Mc3UvMapping uv;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        // AUD-017: UvProjection (3 members) was missed by the original
        // enum-clamp pass -- clampEnum applied here too now.
        if      (k == "projection") { expectTag(tag, TAG_I32, "projection"); uv.projection = clampEnum<Mc3::UvProjection>(rI32(in), 3, "projection"); }
        else if (k == "scaleU")     { expectTag(tag, TAG_F32, "scaleU");     uv.scaleU     = rF32(in); }
        else if (k == "scaleV")     { expectTag(tag, TAG_F32, "scaleV");     uv.scaleV     = rF32(in); }
        else if (k == "offsetU")    { expectTag(tag, TAG_F32, "offsetU");    uv.offsetU    = rF32(in); }
        else if (k == "offsetV")    { expectTag(tag, TAG_F32, "offsetV");    uv.offsetV    = rF32(in); }
        else if (k == "rotation")   { expectTag(tag, TAG_F32, "rotation");   uv.rotation   = rF32(in); }
        else                        skipValue(in, tag);
    }
    return uv;
}

static Mc3::Mc3ObjectState readObjectState(std::istream& in) {
    Mc3::Mc3ObjectState st;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "position") { expectTag(tag, TAG_VEC3, "position"); st.position = rVec3(in); }
        else if (k == "rotation") { expectTag(tag, TAG_VEC3, "rotation"); st.rotation = rVec3(in); }
        else if (k == "scale")    { expectTag(tag, TAG_VEC3, "scale");    st.scale    = rVec3(in); }
        else if (k == "visible")  { expectTag(tag, TAG_BOOL, "visible");  st.visible  = rU8(in) != 0; }
        else if (k == "material") { expectTag(tag, TAG_STR, "material");  st.material = rRawStr(in); }
        else                      skipValue(in, tag);
    }
    return st;
}

static Mc3::Mc3AssetMetadata readAssetMetadata(std::istream& in) {
    Mc3::Mc3AssetMetadata am;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "category")         { expectTag(tag, TAG_STR, "category");         am.category         = rRawStr(in); }
        else if (k == "subcategory")      { expectTag(tag, TAG_STR, "subcategory");      am.subcategory      = rRawStr(in); }
        else if (k == "facing")           { expectTag(tag, TAG_STR, "facing");           am.facing           = rRawStr(in); }
        else if (k == "collisionProxy")   { expectTag(tag, TAG_STR, "collisionProxy");   am.collisionProxy   = rRawStr(in); }
        else if (k == "shadowPolicy")     { expectTag(tag, TAG_STR, "shadowPolicy");     am.shadowPolicy     = rRawStr(in); }
        else if (k == "license")          { expectTag(tag, TAG_STR, "license");          am.license          = rRawStr(in); }
        else if (k == "provenance")       { expectTag(tag, TAG_STR, "provenance");       am.provenance       = rRawStr(in); }
        else if (k == "source")           { expectTag(tag, TAG_STR, "source");           am.sourceGeneratorOrHash = rRawStr(in); }
        else if (k == "version")          { expectTag(tag, TAG_STR, "version");          am.semanticVersion  = rRawStr(in); }
        else if (k == "instancingEligible") { expectTag(tag, TAG_BOOL, "instancingEligible"); am.instancingEligible = rU8(in) != 0; }
        else if (k == "maxVisibilityDistance") { expectTag(tag, TAG_F32, "maxVisibilityDistance"); am.maxVisibilityDistanceM = rF32(in); }
        else if (k == "selectionWeight")  { expectTag(tag, TAG_F32, "selectionWeight");  am.selectionWeight  = rF32(in); }
        else if (k == "nominalSize")      { expectTag(tag, TAG_VEC3, "nominalSize");     am.nominalSize      = rVec3(in); }
        else if (k == "boundsMin")        { expectTag(tag, TAG_VEC3, "boundsMin");       am.boundsMin        = rVec3(in); }
        else if (k == "boundsMax")        { expectTag(tag, TAG_VEC3, "boundsMax");       am.boundsMax        = rVec3(in); }
        else if (k == "clearanceVolume")  { expectTag(tag, TAG_VEC3, "clearanceVolume"); am.clearanceVolume  = rVec3(in); }
        else if (k == "semanticTags" || k == "styleTags" || k == "regionTags" ||
                 k == "periodTags"   || k == "materialSlots") {
            expectTag(tag, TAG_ARR, k.c_str());
            auto& out = k == "semanticTags" ? am.semanticTags
                      : k == "styleTags"    ? am.styleTags
                      : k == "regionTags"   ? am.regionTags
                      : k == "periodTags"   ? am.periodTags
                      :                        am.materialSlots;
            uint32_t n = rU32Bounded(in);
            out.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) out.push_back(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "sockets") {
            expectTag(tag, TAG_MAP, "sockets");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string name = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_VEC3) am.sockets[name] = rVec3(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "lods") {
            expectTag(tag, TAG_MAP, "lods");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string tier = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_STR) am.lods[tier] = rRawStr(in);
                else               skipValue(in, t);
            }
        }
        else skipValue(in, tag);
    }
    return am;
}

// Forward declaration for recursive children
static std::shared_ptr<Mc3::Mc3Object> readObject(std::istream& in);

static std::shared_ptr<Mc3::Mc3Object> readObject(std::istream& in) {
    RecursionGuard<256> guard;
    IdentityScope idScope; // SYS-W1-01: updated below once name/id is read
    g_budget.chargeObject();
    auto obj = std::make_shared<Mc3::Mc3Object>();
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "type")             { expectTag(tag, TAG_I32, "type");             obj->type             = clampEnum<Mc3::ObjectType>(rI32(in), 19, "type"); }
        else if (k == "name")             { expectTag(tag, TAG_STR, "name");             obj->name             = rRawStr(in); idScope.set(obj->name); }
        else if (k == "id")               { expectTag(tag, TAG_STR, "id");               obj->id               = rRawStr(in); idScope.set(obj->id); }
        else if (k == "material")         { expectTag(tag, TAG_STR, "material");         obj->material         = rRawStr(in); }
        else if (k == "visible")          { expectTag(tag, TAG_BOOL, "visible");         obj->visible          = rU8(in) != 0; }
        else if (k == "collision")        { expectTag(tag, TAG_STR, "collision");        obj->collision        = rRawStr(in); }
        else if (k == "layer")            { expectTag(tag, TAG_STR, "layer");            obj->layer            = rRawStr(in); }
        else if (k == "isCutter")         { expectTag(tag, TAG_BOOL, "isCutter");        obj->isCutter         = rU8(in) != 0; }
        else if (k == "definition")       { expectTag(tag, TAG_STR, "definition");       obj->definition       = rRawStr(in); }
        else if (k == "meshSource")       { expectTag(tag, TAG_STR, "meshSource");       obj->meshSource       = rRawStr(in); validateResourcePathIfConfined(obj->meshSource, "mesh source"); }
        else if (k == "materialOverride") { expectTag(tag, TAG_STR, "materialOverride"); obj->materialOverride = rRawStr(in); }
        else if (k == "script")           { expectTag(tag, TAG_STR, "script");           obj->scriptId         = rRawStr(in); }
        else if (k == "transform")        { expectTag(tag, TAG_OBJ, "transform");        obj->transform        = readTransform(in); }
        else if (k == "deform")           { expectTag(tag, TAG_OBJ, "deform");           obj->deform           = readDeform(in); }
        else if (k == "primitive")        { expectTag(tag, TAG_OBJ, "primitive");        obj->primitive        = readPrimitive(in); }
        else if (k == "csgOperation")     { expectTag(tag, TAG_OBJ, "csgOperation");     obj->csgOperation     = readCsgOp(in); }
        else if (k == "extrude")          { expectTag(tag, TAG_OBJ, "extrude");          obj->extrude          = readExtrude(in); }
        else if (k == "uvMapping")        { expectTag(tag, TAG_OBJ, "uvMapping");        obj->uvMapping        = readUvMapping(in); }
        else if (k == "assetMetadata")    { expectTag(tag, TAG_OBJ, "assetMetadata");    obj->assetMetadata    = readAssetMetadata(in); }
        else if (k == "tags") {
            expectTag(tag, TAG_ARR, "tags");
            uint32_t n = rU32Bounded(in);
            obj->tags.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) obj->tags.push_back(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "variantDefs") {
            expectTag(tag, TAG_ARR, "variantDefs");
            uint32_t n = rU32Bounded(in);
            obj->variantDefinitions.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) obj->variantDefinitions.push_back(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "metadata") {
            expectTag(tag, TAG_MAP, "metadata");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_STR) obj->metadata[mk] = rRawStr(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "states") {
            expectTag(tag, TAG_MAP, "states");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string sk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) obj->states[sk] = readObjectState(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "children") {
            expectTag(tag, TAG_ARR, "children");
            uint32_t n = rU32Bounded(in);
            obj->children.reserve(reserveHint(n));
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
    IdentityScope idScope;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "name")       { expectTag(tag, TAG_STR, "name");       tex.name       = rRawStr(in); idScope.set(tex.name); }
        else if (k == "uri")        { expectTag(tag, TAG_STR, "uri");        tex.uri        = rRawStr(in); validateResourcePathIfConfined(tex.uri, "texture uri"); }
        else if (k == "wrapU")      { expectTag(tag, TAG_STR, "wrapU");      tex.wrapU      = rRawStr(in); }
        else if (k == "wrapV")      { expectTag(tag, TAG_STR, "wrapV");      tex.wrapV      = rRawStr(in); }
        else if (k == "filter")     { expectTag(tag, TAG_STR, "filter");     tex.filter     = rRawStr(in); }
        else if (k == "colorSpace") { expectTag(tag, TAG_STR, "colorSpace"); tex.colorSpace = rRawStr(in); }
        else if (k == "mipMaps")    { expectTag(tag, TAG_BOOL, "mipMaps");   tex.mipMaps    = rU8(in) != 0; }
        else                        skipValue(in, tag);
    }
    return tex;
}

static Mc3::Mc3SvgTexture readSvgTexture(std::istream& in, const std::string& id) {
    IdentityScope idScope(id);
    Mc3::Mc3SvgTexture svg;
    svg.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "src")           { expectTag(tag, TAG_STR, "src");           svg.src           = rRawStr(in); validateResourcePathIfConfined(svg.src, "SVG texture src"); }
        else if (k == "inlineContent") { expectTag(tag, TAG_STR, "inlineContent"); svg.inlineContent = rRawStr(in); }
        else                           skipValue(in, tag);
    }
    return svg;
}

static Mc3::Mc3ObjectOverride readObjectOverride(std::istream& in) {
    Mc3::Mc3ObjectOverride ovr;
    IdentityScope idScope;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "id")       { expectTag(tag, TAG_STR, "id");       ovr.id       = rRawStr(in); idScope.set(ovr.id); }
        else if (k == "visible")  { expectTag(tag, TAG_BOOL, "visible"); ovr.visible  = rU8(in) != 0; }
        else if (k == "position") { expectTag(tag, TAG_VEC3, "position"); ovr.position = rVec3(in); }
        else if (k == "rotation") { expectTag(tag, TAG_VEC3, "rotation"); ovr.rotation = rVec3(in); }
        else if (k == "material") { expectTag(tag, TAG_STR, "material"); ovr.material = rRawStr(in); }
        else                      skipValue(in, tag);
    }
    return ovr;
}

static Mc3::Mc3SceneState readSceneState(std::istream& in, const std::string& name) {
    IdentityScope idScope(name);
    Mc3::Mc3SceneState state;
    state.name = name;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if (k == "overrides") {
            expectTag(tag, TAG_ARR, "overrides");
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
    IdentityScope idScope(id);
    Mc3::Mc3Trigger trig;
    trig.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if (k == "steps") {
            expectTag(tag, TAG_ARR, "steps");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t != TAG_OBJ) { skipValue(in, t); continue; }
                Mc3::Mc3TriggerStep step;
                while (true) {
                    std::string sk = rKey(in); if (sk.empty()) break;
                    uint8_t st = rU8(in);
                    if      (sk == "type") { expectTag(st, TAG_STR, "type"); step.type = parseTriggerStepType(rRawStr(in)); }
                    else if (sk == "ref")  { expectTag(st, TAG_STR, "ref");  step.ref  = rRawStr(in); }
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

static Mc3::EventBindingEvent parseEventBindingEvent(const std::string& event) {
    if (event == "exit")  return Mc3::EventBindingEvent::Exit;
    if (event == "click") return Mc3::EventBindingEvent::Click;
    if (event == "timer") return Mc3::EventBindingEvent::Timer;
    return Mc3::EventBindingEvent::Enter;
}

static Mc3::EventBindingTarget parseEventBindingTarget(const std::string& target) {
    return target == "state" ? Mc3::EventBindingTarget::SceneState
                             : Mc3::EventBindingTarget::Trigger;
}

static Mc3::Mc3EventBinding readEventBinding(std::istream& in) {
    Mc3::Mc3EventBinding binding;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "id")            { expectTag(tag, TAG_STR, "id"); binding.id = rRawStr(in); }
        else if (k == "source")        { expectTag(tag, TAG_STR, "source"); binding.sourceObjectId = rRawStr(in); }
        else if (k == "event")         { expectTag(tag, TAG_STR, "event"); binding.event = parseEventBindingEvent(rRawStr(in)); }
        else if (k == "targetType")    { expectTag(tag, TAG_STR, "targetType"); binding.targetType = parseEventBindingTarget(rRawStr(in)); }
        else if (k == "target")        { expectTag(tag, TAG_STR, "target"); binding.targetId = rRawStr(in); }
        else if (k == "enabled")       { expectTag(tag, TAG_BOOL, "enabled"); binding.enabled = rU8(in) != 0; }
        else if (k == "cooldown") { expectTag(tag, TAG_F32, "cooldown"); binding.cooldown = rF32(in); }
        else if (k == "once")     { expectTag(tag, TAG_BOOL, "once"); binding.once = rU8(in) != 0; }
        else if (k == "interval") { expectTag(tag, TAG_F32, "interval"); binding.interval = rF32(in); }
        else                             skipValue(in, tag);
    }
    return binding;
}

static Mc3::Mc3Sound readSound(std::istream& in, const std::string& id) {
    IdentityScope idScope(id);
    Mc3::Mc3Sound snd;
    snd.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "src")  { expectTag(tag, TAG_STR, "src");  snd.src  = rRawStr(in); validateResourcePathIfConfined(snd.src, "sound src"); }
        else if (k == "loop") { expectTag(tag, TAG_BOOL, "loop"); snd.loop = rU8(in) != 0; }
        else                  skipValue(in, tag);
    }
    return snd;
}

static Mc3::Mc3Music readMusic(std::istream& in, const std::string& id) {
    IdentityScope idScope(id);
    Mc3::Mc3Music mus;
    mus.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "src")  { expectTag(tag, TAG_STR, "src");  mus.src  = rRawStr(in); validateResourcePathIfConfined(mus.src, "music src"); }
        else if (k == "loop") { expectTag(tag, TAG_BOOL, "loop"); mus.loop = rU8(in) != 0; }
        else                  skipValue(in, tag);
    }
    return mus;
}

static Mc3::Mc3Script readScript(std::istream& in, const std::string& id) {
    IdentityScope idScope(id);
    Mc3::Mc3Script sc;
    sc.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "type")   { expectTag(tag, TAG_STR, "type");   sc.type   = rRawStr(in); }
        else if (k == "source") { expectTag(tag, TAG_STR, "source"); sc.source = rRawStr(in); }
        else                    skipValue(in, tag);
    }
    return sc;
}

static Mc3::Mc3EmbedGltf readEmbed(std::istream& in, const std::string& id) {
    IdentityScope idScope(id);
    Mc3::Mc3EmbedGltf em;
    em.id = id;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "src")           { expectTag(tag, TAG_STR, "src");           em.src           = rRawStr(in); validateResourcePathIfConfined(em.src, "embed src"); }
        else if (k == "base64Content") { expectTag(tag, TAG_STR, "base64Content"); em.base64Content = rRawStr(in); }
        else                           skipValue(in, tag);
    }
    return em;
}

static Mc3::Mc3Material readMaterial(std::istream& in) {
    Mc3::Mc3Material m;
    IdentityScope idScope;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "name")                     { expectTag(tag, TAG_STR, "name");                     m.name                     = rRawStr(in); idScope.set(m.name); }
        else if (k == "baseColor")                { expectTag(tag, TAG_VEC4, "baseColor");                m.baseColor                = rVec4(in); }
        else if (k == "baseColorTexture")         { expectTag(tag, TAG_STR, "baseColorTexture");         m.baseColorTexture         = rRawStr(in); }
        else if (k == "normalTexture")            { expectTag(tag, TAG_STR, "normalTexture");            m.normalTexture            = rRawStr(in); }
        else if (k == "emissiveTexture")          { expectTag(tag, TAG_STR, "emissiveTexture");          m.emissiveTexture          = rRawStr(in); }
        else if (k == "metallicRoughnessTexture") { expectTag(tag, TAG_STR, "metallicRoughnessTexture"); m.metallicRoughnessTexture = rRawStr(in); }
        else if (k == "occlusionTexture")         { expectTag(tag, TAG_STR, "occlusionTexture");         m.occlusionTexture         = rRawStr(in); }
        else if (k == "roughness")                { expectTag(tag, TAG_F32, "roughness");                m.roughness                = rF32(in); }
        else if (k == "metallic")                 { expectTag(tag, TAG_F32, "metallic");                 m.metallic                 = rF32(in); }
        else if (k == "normalScale")              { expectTag(tag, TAG_F32, "normalScale");              m.normalScale              = rF32(in); }
        else if (k == "occlusionStrength")        { expectTag(tag, TAG_F32, "occlusionStrength");        m.occlusionStrength        = rF32(in); }
        else if (k == "emissiveColor")            { expectTag(tag, TAG_VEC3, "emissiveColor");           m.emissiveColor            = rVec3(in); }
        else if (k == "alphaMode")                { expectTag(tag, TAG_STR, "alphaMode");                m.alphaMode                = rRawStr(in); }
        else if (k == "alphaCutoff")              { expectTag(tag, TAG_F32, "alphaCutoff");              m.alphaCutoff              = rF32(in); }
        else if (k == "doubleSided")              { expectTag(tag, TAG_BOOL, "doubleSided");             m.doubleSided              = rU8(in) != 0; }
        else                                      skipValue(in, tag);
    }
    return m;
}

static Mc3::Mc3Light readLight(std::istream& in) {
    Mc3::Mc3Light lt;
    IdentityScope idScope;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "type")        { expectTag(tag, TAG_I32, "type");        lt.type        = clampEnum<Mc3::LightType>(rI32(in), 4, "type"); }
        else if (k == "name")        { expectTag(tag, TAG_STR, "name");        lt.name        = rRawStr(in); idScope.set(lt.name); }
        else if (k == "color")       { expectTag(tag, TAG_VEC3, "color");      lt.color       = rVec3(in); }
        else if (k == "brightness")  { expectTag(tag, TAG_F32, "brightness");  lt.brightness  = rF32(in); }
        else if (k == "direction")   { expectTag(tag, TAG_VEC3, "direction");  lt.direction   = rVec3(in); }
        else if (k == "position")    { expectTag(tag, TAG_VEC3, "position");   lt.position    = rVec3(in); }
        else if (k == "range")       { expectTag(tag, TAG_F32, "range");       lt.range       = rF32(in); }
        else if (k == "angle")       { expectTag(tag, TAG_F32, "angle");       lt.angle       = rF32(in); }
        else if (k == "falloff")     { expectTag(tag, TAG_F32, "falloff");     lt.falloff     = rF32(in); }
        else if (k == "castShadows") { expectTag(tag, TAG_BOOL, "castShadows"); lt.castShadows = rU8(in) != 0; }
        else                         skipValue(in, tag);
    }
    return lt;
}

static Mc3::Mc3Camera readCamera(std::istream& in) {
    Mc3::Mc3Camera cam;
    IdentityScope idScope;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "name")      { expectTag(tag, TAG_STR, "name");      cam.name      = rRawStr(in); idScope.set(cam.name); }
        else if (k == "type")      { expectTag(tag, TAG_I32, "type");      cam.type      = clampEnum<Mc3::CameraType>(rI32(in), 2, "type"); }
        else if (k == "position")  { expectTag(tag, TAG_VEC3, "position"); cam.position  = rVec3(in); }
        else if (k == "target")    { expectTag(tag, TAG_VEC3, "target");   cam.target    = rVec3(in); }
        else if (k == "rotation")  { expectTag(tag, TAG_VEC3, "rotation"); cam.rotation  = rVec3(in); }
        else if (k == "nearPlane") { expectTag(tag, TAG_F32, "nearPlane"); cam.nearPlane = rF32(in); }
        else if (k == "farPlane")  { expectTag(tag, TAG_F32, "farPlane");  cam.farPlane  = rF32(in); }
        else if (k == "fov")       { expectTag(tag, TAG_F32, "fov");       cam.fov       = rF32(in); }
        else if (k == "orthoSize") { expectTag(tag, TAG_F32, "orthoSize"); cam.orthoSize = rF32(in); }
        else if (k == "orthoAspect") { expectTag(tag, TAG_F32, "orthoAspect"); cam.orthoAspect = rF32(in); }   // STAB-0695
        else                       skipValue(in, tag);
    }
    return cam;
}

static Mc3::Mc3Fog readFog(std::istream& in) {
    Mc3::Mc3Fog fog;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "color")   { expectTag(tag, TAG_VEC3, "color");   fog.color   = rVec3(in); }
        else if (k == "mode")    { expectTag(tag, TAG_I32, "mode");     fog.mode    = clampEnum<Mc3::FogMode>(rI32(in), 2, "mode"); }
        else if (k == "start")   { expectTag(tag, TAG_F32, "start");    fog.start   = rF32(in); }
        else if (k == "end")     { expectTag(tag, TAG_F32, "end");      fog.end     = rF32(in); }
        else if (k == "density") { expectTag(tag, TAG_F32, "density");  fog.density = rF32(in); }
        else                     skipValue(in, tag);
    }
    return fog;
}

static Mc3::Mc3Environment readEnvironment(std::istream& in) {
    Mc3::Mc3Environment env;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "backgroundColor")  { expectTag(tag, TAG_VEC3, "backgroundColor");  env.backgroundColor  = rVec3(in); }
        else if (k == "backgroundTexture") { expectTag(tag, TAG_STR, "backgroundTexture"); env.backgroundTexture = rRawStr(in); }
        else if (k == "skyboxTexture")     { expectTag(tag, TAG_STR, "skyboxTexture");     env.skyboxTexture     = rRawStr(in); }
        else if (k == "fog")               { expectTag(tag, TAG_OBJ, "fog");               env.fog              = readFog(in); }
        else                               skipValue(in, tag);
    }
    return env;
}

static Mc3::Mc3Keyframe readKeyframe(std::istream& in) {
    Mc3::Mc3Keyframe kf;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "time")          { expectTag(tag, TAG_F32, "time");          kf.time                = rF32(in); }
        else if (k == "value")         { expectTag(tag, TAG_F32, "value");         kf.value               = rF32(in); }
        else if (k == "interpolation") { expectTag(tag, TAG_I32, "interpolation"); kf.interpolation       = clampEnum<Mc3::Interpolation>(rI32(in), 3, "interpolation"); }
        else if (k == "leftDt")        { expectTag(tag, TAG_F32, "leftDt");        kf.handleLeft.dt       = rF32(in); }
        else if (k == "leftDv")        { expectTag(tag, TAG_F32, "leftDv");        kf.handleLeft.dv       = rF32(in); }
        else if (k == "rightDt")       { expectTag(tag, TAG_F32, "rightDt");       kf.handleRight.dt      = rF32(in); }
        else if (k == "rightDv")       { expectTag(tag, TAG_F32, "rightDv");       kf.handleRight.dv      = rF32(in); }
        else                           skipValue(in, tag);
    }
    return kf;
}

static Mc3::Mc3Channel readChannel(std::istream& in) {
    Mc3::Mc3Channel ch;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "targetObject") { expectTag(tag, TAG_STR, "targetObject"); ch.targetObject = rRawStr(in); }
        else if (k == "property")     { expectTag(tag, TAG_I32, "property");     ch.property     = clampEnum<Mc3::AnimatedProperty>(rI32(in), 22, "property"); }
        else if (k == "keyframes") {
            expectTag(tag, TAG_ARR, "keyframes");
            uint32_t n = rU32Bounded(in);
            ch.keyframes.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) { g_budget.chargeKeyframe(); ch.keyframes.push_back(readKeyframe(in)); }
                else               skipValue(in, t);
            }
        }
        else skipValue(in, tag);
    }
    // F17 (2026-07-20 audit): Mc3XmlParser.cpp sorts keyframes by time
    // (stable, so equal-time keyframes keep their first-declared-wins
    // order -- STAB-0468); this path never did. Mc3Animation.cpp's
    // evaluateChannel() assumes sorted input (std::upper_bound) -- an
    // out-of-order .mcb-authored channel silently interpolated wrong.
    std::stable_sort(ch.keyframes.begin(), ch.keyframes.end(),
        [](const Mc3::Mc3Keyframe& a, const Mc3::Mc3Keyframe& b){ return a.time < b.time; });
    return ch;
}

static Mc3::Mc3ActionClip readActionClip(std::istream& in) {
    Mc3::Mc3ActionClip clip;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "name")               { expectTag(tag, TAG_STR, "name");               clip.name = rRawStr(in); }
        else if (k == "startTime")          { expectTag(tag, TAG_F32, "startTime");          clip.startTime = rF32(in); }
        else if (k == "endTime")            { expectTag(tag, TAG_F32, "endTime");            clip.endTime = rF32(in); }
        else if (k == "playbackRate")       { expectTag(tag, TAG_F32, "playbackRate");       clip.playbackRate = rF32(in); }
        else if (k == "loop")               { expectTag(tag, TAG_BOOL, "loop");              clip.loop = rU8(in) != 0; }
        else if (k == "reverse")            { expectTag(tag, TAG_BOOL, "reverse");           clip.reverse = rU8(in) != 0; }
        else if (k == "transitionDuration") { expectTag(tag, TAG_F32, "transitionDuration"); clip.transitionDuration = rF32(in); }
        else skipValue(in, tag);
    }
    // MCB is intentionally forward-compatible (unknown map keys are skipped),
    // but malformed known clip values still get normalized before reaching
    // the preview/export paths.
    clip.playbackRate = std::max(1e-3f, clip.playbackRate);
    clip.transitionDuration = std::clamp(clip.transitionDuration, 0.0f, 60.0f);
    return clip;
}

static Mc3::Mc3Action readAction(std::istream& in) {
    Mc3::Mc3Action act;
    IdentityScope idScope;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "name")      { expectTag(tag, TAG_STR, "name");      act.name      = rRawStr(in); idScope.set(act.name); }
        else if (k == "duration")  { expectTag(tag, TAG_F32, "duration");  act.duration  = rF32(in); }
        else if (k == "loop")      { expectTag(tag, TAG_BOOL, "loop");     act.loop      = rU8(in) != 0; }
        else if (k == "autoplay")  { expectTag(tag, TAG_BOOL, "autoplay"); act.autoplay  = rU8(in) != 0; }
        else if (k == "timeScale") { expectTag(tag, TAG_F32, "timeScale"); act.timeScale = rF32(in); } // STAB-0460
        else if (k == "clips") {
            expectTag(tag, TAG_ARR, "clips");
            uint32_t n = rU32Bounded(in);
            act.clips.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) { g_budget.chargeClip(); act.clips.push_back(readActionClip(in)); }
                else              skipValue(in, t);
            }
        }
        else if (k == "channels") {
            expectTag(tag, TAG_ARR, "channels");
            uint32_t n = rU32Bounded(in);
            act.channels.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) { g_budget.chargeChannel(); act.channels.push_back(readChannel(in)); }
                else               skipValue(in, t);
            }
        }
        else skipValue(in, tag);
    }
    const float duration = std::max(1e-3f, act.duration);
    act.timeScale = std::max(1e-3f, act.timeScale);
    for (auto& clip : act.clips) {
        clip.startTime = std::clamp(clip.startTime, 0.0f, duration);
        clip.endTime = std::clamp(clip.endTime, 0.0f, duration);
        if (clip.endTime <= clip.startTime) {
            clip.startTime = std::min(clip.startTime, duration - 1e-3f);
            clip.endTime = std::max(clip.startTime + 1e-3f, clip.endTime);
        }
    }
    return act;
}

static Mc3::Mc3LibraryInfo readLibraryInfo(std::istream& in) {
    Mc3::Mc3LibraryInfo lib;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "namespace") { expectTag(tag, TAG_STR, "namespace"); lib.libraryNamespace = rRawStr(in); }
        else if (k == "version")   { expectTag(tag, TAG_STR, "version");   lib.version          = rRawStr(in); }
        else if (k == "hash")      { expectTag(tag, TAG_STR, "hash");      lib.contentHash      = rRawStr(in); }
        else skipValue(in, tag);
    }
    return lib;
}

static Mc3::Mc3Import readImport(std::istream& in) {
    Mc3::Mc3Import imp;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "namespace") { expectTag(tag, TAG_STR, "namespace"); imp.importNamespace = rRawStr(in); }
        else if (k == "source")    { expectTag(tag, TAG_STR, "source");    imp.source          = rRawStr(in); }
        else if (k == "hash")      { expectTag(tag, TAG_STR, "hash");      imp.hash            = rRawStr(in); }
        else skipValue(in, tag);
    }
    return imp;
}

static Mc3::Mc3Document readDocument(std::istream& in) {
    Mc3::Mc3Document doc;
    while (true) {
        std::string k = rKey(in); if (k.empty()) break;
        uint8_t tag = rU8(in);
        if      (k == "version")          { expectTag(tag, TAG_STR, "version");          doc.version          = rRawStr(in); }
        else if (k == "model")            { expectTag(tag, TAG_STR, "model");            doc.model            = rRawStr(in); }
        else if (k == "unit")             { expectTag(tag, TAG_STR, "unit");             doc.unit             = rRawStr(in); }
        else if (k == "coordinateSystem") { expectTag(tag, TAG_STR, "coordinateSystem"); doc.coordinateSystem = rRawStr(in); }
        else if (k == "rotationUnits")    { expectTag(tag, TAG_STR, "rotationUnits");    doc.rotationUnits    = rRawStr(in); }
        else if (k == "eulerOrder")       { expectTag(tag, TAG_STR, "eulerOrder");       doc.eulerOrder       = rRawStr(in); }
        else if (k == "defaultCamera")    { expectTag(tag, TAG_STR, "defaultCamera");    doc.defaultCamera    = rRawStr(in); }
        else if (k == "library") { expectTag(tag, TAG_OBJ, "library"); doc.library = readLibraryInfo(in); }
        else if (k == "imports") {
            expectTag(tag, TAG_ARR, "imports");
            uint32_t n = rU32Bounded(in);
            doc.imports.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.imports.push_back(readImport(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "meta") {
            expectTag(tag, TAG_MAP, "meta");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.meta[mk] = rRawStr(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "metadata") {
            expectTag(tag, TAG_MAP, "metadata");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.metadata[mk] = rRawStr(in);
                else               skipValue(in, t);
            }
        }
        else if (k == "includes") {
            expectTag(tag, TAG_ARR, "includes");
            uint32_t n = rU32Bounded(in);
            doc.includes.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.includes.push_back(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "includedDefs") {
            expectTag(tag, TAG_ARR, "includedDefs");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.includedDefs.insert(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "includedMaterials") {
            expectTag(tag, TAG_ARR, "includedMaterials");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.includedMaterials.insert(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "includedTextures") {
            expectTag(tag, TAG_ARR, "includedTextures");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.includedTextures.insert(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "includedEmbeds") {
            expectTag(tag, TAG_ARR, "includedEmbeds");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_STR) doc.includedEmbeds.insert(rRawStr(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "environment")      { expectTag(tag, TAG_OBJ, "environment");      doc.environment      = readEnvironment(in); }
        else if (k == "lights") {
            expectTag(tag, TAG_ARR, "lights");
            uint32_t n = rU32Bounded(in);
            doc.lights.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.lights.push_back(readLight(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "cameras") {
            expectTag(tag, TAG_ARR, "cameras");
            uint32_t n = rU32Bounded(in);
            doc.cameras.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.cameras.push_back(readCamera(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "textures") {
            expectTag(tag, TAG_MAP, "textures");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) { g_budget.chargeTexture(); doc.textures[mk] = readTexture(in); }
                else               skipValue(in, t);
            }
        }
        else if (k == "svgTextures") {
            expectTag(tag, TAG_MAP, "svgTextures");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.svgTextures[mk] = readSvgTexture(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "embeds") {
            expectTag(tag, TAG_MAP, "embeds");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) {
                    Mc3::Mc3EmbedGltf em = readEmbed(in, mk);
                    g_budget.chargeEmbed(em.base64Content.size());
                    doc.embeds[mk] = std::move(em);
                }
                else               skipValue(in, t);
            }
        }
        else if (k == "scripts") {
            expectTag(tag, TAG_MAP, "scripts");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.scripts[mk] = readScript(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "sounds") {
            expectTag(tag, TAG_MAP, "sounds");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.sounds[mk] = readSound(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "musicTracks") {
            expectTag(tag, TAG_MAP, "musicTracks");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.musicTracks[mk] = readMusic(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "triggers") {
            expectTag(tag, TAG_MAP, "triggers");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.triggers[mk] = readTrigger(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "sceneStates") {
            expectTag(tag, TAG_MAP, "sceneStates");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.sceneStates[mk] = readSceneState(in, mk);
                else               skipValue(in, t);
            }
        }
        else if (k == "eventBindings") {
            expectTag(tag, TAG_ARR, "eventBindings");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.eventBindings.push_back(readEventBinding(in));
                else              skipValue(in, t);
            }
        }
        else if (k == "materials") {
            expectTag(tag, TAG_MAP, "materials");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) { g_budget.chargeMaterial(); doc.materials[mk] = readMaterial(in); }
                else               skipValue(in, t);
            }
        }
        else if (k == "definitions") {
            expectTag(tag, TAG_MAP, "definitions");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) { g_budget.chargeDefinition(); doc.definitions[mk] = readObject(in); }
                else               skipValue(in, t);
            }
        }
        else if (k == "objects") {
            expectTag(tag, TAG_ARR, "objects");
            uint32_t n = rU32Bounded(in);
            doc.objects.reserve(reserveHint(n));
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) doc.objects.push_back(readObject(in));
                else               skipValue(in, t);
            }
        }
        else if (k == "actions") {
            expectTag(tag, TAG_MAP, "actions");
            uint32_t n = rU32Bounded(in);
            for (uint32_t i = 0; i < n; ++i) {
                std::string mk = rRawStr(in);
                uint8_t t = rU8(in);
                if (t == TAG_OBJ) { g_budget.chargeAction(); doc.actions[mk] = readAction(in); }
                else               skipValue(in, t);
            }
        }
        else skipValue(in, tag);
    }
    return doc;
}

// ---------------------------------------------------------------------------
// Version migration (AUDIT-0038)
// ---------------------------------------------------------------------------
//
// MCB_VERSION has never been bumped since the format's introduction (still
// 1), so there is nothing to migrate FROM yet -- this registry is
// intentionally empty. Policy going forward: a version bump that changes
// wire-level *meaning* registers an upgrade function here, keyed by the
// version it upgrades FROM (the reader has already fully parsed that
// version's wire format into a Mc3Document at this point -- an "upgrade"
// is any in-memory transform needed to match current semantics, e.g. a
// renamed/repurposed field). loadFromBinary() applies every registered
// upgrade whose source version is >= the file's version, in ascending
// version order, so a v1 file read by a hypothetical v3 reader would run
// the v1 upgrade then the v2 upgrade in sequence. Never remove an entry
// while MCB_MIN_SUPPORTED_VERSION still covers its source version.
using McbUpgradeFn = void (*)(Mc3::Mc3Document&);
static const std::map<uint8_t, McbUpgradeFn>& mcbUpgrades() {
    static const std::map<uint8_t, McbUpgradeFn> kUpgrades = {
        // e.g. {1, &upgradeV1ToV2},  -- register here on the next version bump
    };
    return kUpgrades;
}

static void applyMcbUpgrades(Mc3::Mc3Document& doc, uint8_t fromVersion) {
    for (const auto& [srcVersion, fn] : mcbUpgrades())
        if (srcVersion >= fromVersion) fn(doc);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// SYS-W1-01: shared implementation. Assumes g_validation/g_sourceFile have
// already been set up by the caller (each public overload below owns exactly
// one ValidationScope, so nesting/reset ambiguity can't arise).
static Mc3::Mc3Document loadFromBinaryImpl(std::istream& in) {
    // 2026-07-20 audit finding #1: reset the document-wide budget once per
    // top-level load, matching Mc3XmlParser.cpp's own g_budget.reset()
    // placement at the start of its top-level entry point.
    g_budget.reset();

    // Validate header
    char magic[4];
    if (!in.read(magic, 4) || std::memcmp(magic, MCB_MAGIC, 4) != 0) {
        std::string msg = "MCB: invalid magic";
        reportError("magic", msg);
        throw std::runtime_error(msg);
    }
    uint8_t version = rU8(in);
    if (version < MCB_MIN_SUPPORTED_VERSION || version > MCB_VERSION) {
        std::string msg = "MCB: unsupported version " + std::to_string(version);
        reportError("version", msg);
        throw std::runtime_error(msg);
    }
    uint8_t flags = rU8(in);

    // SYS-W14-25: compressed payload -- read the (sanity-capped) claimed
    // uncompressed/compressed sizes, pull exactly that many compressed bytes
    // off `in`, decompress into a memory buffer, then parse the document
    // from an istringstream over that buffer via the exact same
    // rootTag+readDocument() path the uncompressed branch below uses.
    if (flags & MCB_FLAG_COMPRESSED) {
#ifdef MESHCRAFT_HAS_ZLIB
        rU8(in); rU8(in); // reserved
        uint32_t uncompressedSize = rU32(in);
        if (uncompressedSize > kMcbMaxUncompressedPayload) {
            std::string msg = "MCB: claimed uncompressed size " + std::to_string(uncompressedSize) +
                " exceeds sanity limit (corrupted or malicious file?)";
            reportError("flags", msg);
            throw std::runtime_error(msg);
        }
        uint32_t compressedSize = rU32(in);
        if (compressedSize > kMcbMaxCompressedPayload) {
            std::string msg = "MCB: claimed compressed size " + std::to_string(compressedSize) +
                " exceeds sanity limit (corrupted or malicious file?)";
            reportError("flags", msg);
            throw std::runtime_error(msg);
        }
        std::vector<uint8_t> compressed(compressedSize);
        if (compressedSize > 0 &&
            !in.read(reinterpret_cast<char*>(compressed.data()), compressedSize)) {
            std::string msg = "MCB: compressed payload truncated";
            reportError("flags", msg);
            throw std::runtime_error(msg);
        }

        std::string raw = zlibDecompress(compressed, uncompressedSize);
        std::istringstream payloadIn(raw, std::ios::binary);

        uint8_t rootTag = rU8(payloadIn);
        if (rootTag != TAG_OBJ) {
            std::string msg = "MCB: root is not an object";
            reportError("root", msg);
            throw std::runtime_error(msg);
        }
        Mc3::Mc3Document doc = readDocument(payloadIn);
        if (version != MCB_VERSION) applyMcbUpgrades(doc, version);
        return doc;
#else
        std::string msg = "MCB: compressed format requires zlib, but this build was "
                          "compiled without it";
        reportError("flags", msg);
        throw std::runtime_error(msg);
#endif
    }

    rU8(in); rU8(in); // reserved

    // Root must be TAG_OBJ
    uint8_t rootTag = rU8(in);
    if (rootTag != TAG_OBJ) {
        std::string msg = "MCB: root is not an object";
        reportError("root", msg);
        throw std::runtime_error(msg);
    }

    Mc3::Mc3Document doc = readDocument(in);
    if (version != MCB_VERSION) applyMcbUpgrades(doc, version);
    return doc;
}

Mc3::Mc3Document loadFromBinary(std::istream& in) {
    return loadFromBinary(in, Mc3::Mc3LoadPolicy::trusted());
}

Mc3::Mc3Document loadFromBinary(std::istream& in, const Mc3::Mc3LoadPolicy& policy) {
    ValidationScope vscope(nullptr);
    ResourcePolicyScope pscope(policy.confineResourcePathsToRoot, {});
    g_sourceFile.clear(); // no path known for a raw stream
    return loadFromBinaryImpl(in);
}

Mc3::Mc3Document loadFromBinary(std::istream& in, Mc3::Mc3Validation& validation) {
    return loadFromBinary(in, Mc3::Mc3LoadPolicy::trusted(), validation);
}

Mc3::Mc3Document loadFromBinary(std::istream& in, const Mc3::Mc3LoadPolicy& policy,
                                 Mc3::Mc3Validation& validation) {
    ValidationScope vscope(&validation);
    ResourcePolicyScope pscope(policy.confineResourcePathsToRoot, {});
    g_sourceFile.clear();
    return loadFromBinaryImpl(in);
}

Mc3::Mc3Document loadFromFile(const std::filesystem::path& path) {
    return loadFromFile(path, Mc3::Mc3LoadPolicy::trusted());
}

Mc3::Mc3Document loadFromFile(const std::filesystem::path& path, const Mc3::Mc3LoadPolicy& policy) {
    ValidationScope vscope(nullptr);
    ResourcePolicyScope pscope(policy.confineResourcePathsToRoot, path.parent_path());
    g_sourceFile = path;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::string msg = "MCB: cannot open: " + path.string();
        reportError("file", msg);
        throw std::runtime_error(msg);
    }
    auto doc = loadFromBinaryImpl(f);
    doc.sourcePath = path.parent_path();
    return doc;
}

Mc3::Mc3Document loadFromFile(const std::filesystem::path& path, Mc3::Mc3Validation& validation) {
    return loadFromFile(path, Mc3::Mc3LoadPolicy::trusted(), validation);
}

Mc3::Mc3Document loadFromFile(const std::filesystem::path& path, const Mc3::Mc3LoadPolicy& policy,
                               Mc3::Mc3Validation& validation) {
    ValidationScope vscope(&validation);
    ResourcePolicyScope pscope(policy.confineResourcePathsToRoot, path.parent_path());
    g_sourceFile = path;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::string msg = "MCB: cannot open: " + path.string();
        reportError("file", msg);
        throw std::runtime_error(msg);
    }
    auto doc = loadFromBinaryImpl(f);
    doc.sourcePath = path.parent_path();
    return doc;
}

} // namespace MeshCraft::Mcb
