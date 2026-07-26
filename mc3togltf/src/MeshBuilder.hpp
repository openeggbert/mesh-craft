#pragma once
#include <MeshCraft/Mc3/Mc3Extrude.hpp>
#include <MeshCraft/Mc3/Mc3EmbedGltf.hpp>
#include <MeshCraft/Mc3/Mc3Material.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mc3togltf {

// Ear-clipping triangulation of a simple 2D polygon. Returns triangles as index
// triples into `pts`, preserving the polygon's winding order. Correct for
// concave polygons (unlike a triangle fan). Exposed for testing; used by the
// extrude cap builder.
std::vector<std::array<uint32_t,3>>
earClipPolygon(const std::vector<std::array<float,2>>& pts);

// Untrusted-input containment for external resources (texture URIs, OBJ mesh
// sources). The exporter embeds referenced file bytes into the GLB, so a
// malicious .mc3 could exfiltrate arbitrary local files. Unless external
// resources are explicitly allowed, a resource path that is absolute or escapes
// the document root (`..` traversal) is rejected with a std::runtime_error.
// `embed:` and `data:` references are not filesystem paths and are ignored.
// `kind` names the resource in the error message.
void assertResourceAllowed(const std::filesystem::path& basePath,
                           const std::string& rawPath,
                           bool allowExternal,
                           const char* kind);

struct MeshData {
    std::vector<float>    positions;  // x,y,z triplets
    std::vector<float>    normals;    // x,y,z triplets
    std::vector<float>    texcoords;  // u,v pairs
    std::vector<uint32_t> indices;

    int vertexCount() const { return static_cast<int>(positions.size() / 3); }
    bool empty()      const { return indices.empty(); }

    // Apply per-axis scale to all vertex positions (for <deform>)
    void applyScale(float sx, float sy, float sz);

    // AUD-024: apply a Mc3UvMapping's scale/offset/rotation to the existing
    // TEXCOORD_0 pairs (rotation in degrees, about the UV origin, applied
    // before offset -- scale/rotate/translate order). Operates on whatever
    // TEXCOORD_0 is already present -- the default per-primitive planar
    // unwrap, or the output of applyBoxProjectionUv()/applySphereProjectionUv()
    // below if the caller regenerated it first.
    void applyUvMapping(float scaleU, float scaleV,
                        float offsetU, float offsetV,
                        float rotationDegrees);

    // Regenerates TEXCOORD_0 by projecting local X/Z coordinates. This is
    // intentionally simple and unnormalised: it is the procedural baseline
    // for meshes (notably CSG results) that have no authored unwrap, and the
    // caller may still apply scale/offset/rotation afterwards.
    void applyPlanarProjectionUv();

    // SYS-W14-24: regenerates TEXCOORD_0 via an axis-aligned box/triplanar
    // projection -- per vertex, picks the dominant axis (from that vertex's
    // own normal, or the direction from the mesh's local bounding-box
    // center if normals are absent) and projects onto the other two axes
    // using raw local-space coordinates. Deliberately NOT normalized to
    // [0,1] -- matches how box/triplanar projection behaves in DCC tools:
    // texture scale is tied to object-space size, so applyUvMapping's own
    // scaleU/scaleV (called after this) are the tiling control, the same
    // role they already play against the default planar unwrap.
    void applyBoxProjectionUv();

    // SYS-W14-24: regenerates TEXCOORD_0 via an equirectangular spherical
    // projection around the mesh's local bounding-box center -- longitude
    // (atan2 of x/z) maps to U, latitude (asin of y/radius) maps to V, both
    // normalized to [0,1]. Degenerates to (0.5, 0.5) for a vertex exactly at
    // the center (zero radius) rather than producing NaN.
    void applySphereProjectionUv();
};

// Metadata written by the editor's OBJ importer on each generated Mesh child.
// The value is the tinyobjloader material index for faces that child owns;
// -1 represents faces which had no resolvable `usemtl` assignment.  Storing
// the selector as metadata keeps the serialized `src` a real filesystem path,
// so normal resource-path validation is never bypassed by URI fragments.
inline constexpr std::string_view kObjMaterialIndexMetadataKey =
    "meshcraft.obj.material_index";

// A safely triangulated OBJ material group. `material` is populated only when
// materialIndex names a material actually loaded from the companion MTL file;
// unassigned/missing-MTL groups keep material.name empty and use MC3's normal
// no-material fallback.
struct ObjMaterialGroup {
    int materialIndex{-1};
    std::string materialName;
    MeshCraft::Mc3::Mc3Material material;
    MeshData mesh;
};

// Result of a material-aware OBJ import. Warnings enumerate MTL properties
// that have no faithful MC3 equivalent; callers should show or log them rather
// than silently claiming a lossless conversion.
struct ObjMaterialImportResult {
    std::vector<ObjMaterialGroup> groups;
    std::vector<std::string> warnings;
};

// Strictly parse the persisted material-group selector. It intentionally
// accepts only a base-10 signed integer so malformed metadata is rejected by
// consumers instead of silently falling back to the whole source OBJ.
std::optional<int> parseObjMaterialIndex(std::string_view value);

// Parse an OBJ once, validate every referenced vertex/normal/UV index, split
// its triangulated faces by usemtl material assignment, and map the safe MTL
// PBR subset onto MC3 materials. source may be absolute or relative to
// basePath; resource-policy validation remains the caller's responsibility.
ObjMaterialImportResult importObjMaterialGroups(const std::filesystem::path& basePath,
                                                const std::string& source);

MeshData buildPrimitive(const MeshCraft::Mc3::Mc3Primitive& prim);
MeshData buildExtrude(const MeshCraft::Mc3::Mc3Extrude& ext);

// Load an OBJ file and return its triangulated geometry. If materialIndex is
// supplied, only faces assigned to that usemtl material (or -1 for unassigned
// faces) are returned. source may be an absolute path or relative to basePath.
MeshData loadObjMesh(const std::filesystem::path& basePath,
                     const std::string& source,
                     std::optional<int> materialIndex = std::nullopt);

// Load the triangle geometry of a self-contained GLB asset referenced by an
// MC3 <embed>.  The asset's scene-node transforms are flattened into the
// returned local geometry; source materials, textures, skins, morph targets,
// and animations deliberately remain owned by MC3 rather than being merged
// into the document.  `embed.src` and inline `base64Content` are both
// supported.  Throws std::runtime_error for malformed, unsupported, or
// resource-exhausting input.
MeshData loadEmbeddedGltfMesh(const std::filesystem::path& basePath,
                              const MeshCraft::Mc3::Mc3EmbedGltf& embed);

// Individual primitives (used by buildPrimitive)
MeshData buildBox      (float w, float h, float d);
MeshData buildSphere   (float radius, int segments);
MeshData buildCylinder (float radius, float height, int segments, const std::string& axis);
MeshData buildCone     (float radius, float height, int segments);
MeshData buildPlane    (float w, float d, const std::string& axis);
MeshData buildTorus    (float majorRadius, float minorRadius, int segments);
MeshData buildCapsule  (float radius, float height, int segments, const std::string& axis);
MeshData buildDisk     (float radius, float innerRadius, int segments, const std::string& axis);
MeshData buildGrid     (float w, float d, int subdX, int subdZ);
MeshData buildIcoSphere(float radius, int subdivisions);

} // namespace mc3togltf
