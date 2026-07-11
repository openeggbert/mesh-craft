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

struct MeshData {
    std::vector<float>    positions;  // x,y,z triplets
    std::vector<float>    normals;    // x,y,z triplets
    std::vector<float>    texcoords;  // u,v pairs
    std::vector<uint32_t> indices;

    int vertexCount() const { return static_cast<int>(positions.size() / 3); }
    bool empty()      const { return indices.empty(); }

    // Apply per-axis scale to all vertex positions (for <deform>)
    void applyScale(float sx, float sy, float sz);
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
