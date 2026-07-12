#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

// R111 -- asset metadata for a reusable definition (mesh_world_revival.md
// §6 "Asset Metadata Requirements"), attached via Mc3Object::assetMetadata.
// Present only on definitions where authored/known; absent elsewhere (an
// ordinary placed instance or ad hoc scene object has no reason to carry
// this). Deliberately covers only AUTHORABLE fields from §6's list --
// fields that need tooling this project doesn't have yet are left out
// rather than added as always-empty placeholders (mirrors R109 deferring
// its JSON Schema and R110 deferring dependency pruning):
//   - triangle/object counts: computable from the geometry itself
//     (Mc3MeshBuilder, mesh-world side) rather than authored -- adding a
//     field here would just invite it going stale.
//   - validation status: MC3Validator already produces this per-parse
//     (Mc3Validation.hpp); duplicating it as authored metadata would be a
//     second, potentially-inconsistent source of truth.
//   - content hash: Mc3LibraryInfo::contentHash (R110) already covers this
//     at the whole-library level; a per-definition hash is real but
//     separate scope (would need the R101 import resolver's per-definition
//     dependency tracking to be meaningful).
//   - thumbnail/preview reference: needs a render pipeline that doesn't
//     exist yet (R115's own scope).
struct Mc3AssetMetadata {
    std::string category;
    std::string subcategory;

    std::vector<std::string> semanticTags;
    std::vector<std::string> styleTags;
    std::vector<std::string> regionTags;
    std::vector<std::string> periodTags;

    std::array<float, 3> nominalSize{0.f, 0.f, 0.f};

    // Bounding box, in the definition's own local space.
    std::array<float, 3> boundsMin{0.f, 0.f, 0.f};
    std::array<float, 3> boundsMax{0.f, 0.f, 0.f};

    // Front-facing axis convention, e.g. "-Z", "+X" (§6 example: "facing").
    std::string facing;

    // Named anchor points AND sockets (both are "a named local-space point
    // on this definition", per §6 -- one map covers both without inventing
    // a redundant near-duplicate concept the source document never
    // actually distinguishes by shape).
    std::map<std::string, std::array<float, 3>> sockets;

    std::vector<std::string> materialSlots;

    // Free-form collision proxy descriptor (e.g. "box", "convex_hull",
    // "none") -- this project has no shared collision-shape enum yet;
    // mirrors Mc3Object::collision's own existing free-form string.
    std::string collisionProxy;

    // Clearance volume as a size (width/height/depth), not a full box --
    // callers who need it centered/offset can combine it with the
    // definition's own transform.pivot.
    std::array<float, 3> clearanceVolume{0.f, 0.f, 0.f};

    // LOD tier name -> definition id, e.g. {"near": "chair.oak.lod0"}.
    std::map<std::string, std::string> lods;

    bool instancingEligible{true};

    // Free-form shadow policy descriptor (e.g. "cast_receive",
    // "cast_only", "none") -- same rationale as collisionProxy.
    std::string shadowPolicy;

    // 0 = unspecified / no limit.
    float maxVisibilityDistanceM{0.f};

    // Selection weight for random-variant picking; higher = more common.
    float selectionWeight{1.f};

    std::string license;     // SPDX id or free text
    std::string provenance;  // author / source description

    // Generator id (e.g. "lua.object.window.simple") or, for AI-produced
    // content, the request/recipe hash that produced it (R115's own scope
    // for actually populating the latter).
    std::string sourceGeneratorOrHash;

    // Per-definition semantic version -- distinct from Mc3LibraryInfo's
    // OWN version (R110), which versions the whole library file, not any
    // one definition inside it.
    std::string semanticVersion;
};

} // namespace MeshCraft::Mc3
