#pragma once
#include <MeshCraft/Mc3/Mc3Extrude.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
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

MeshData buildPrimitive(const MeshCraft::Mc3::Mc3Primitive& prim);
MeshData buildExtrude(const MeshCraft::Mc3::Mc3Extrude& ext);

// Load an OBJ file and return its triangulated geometry.
// source may be an absolute path or relative to basePath.
MeshData loadObjMesh(const std::filesystem::path& basePath,
                     const std::string& source);

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
