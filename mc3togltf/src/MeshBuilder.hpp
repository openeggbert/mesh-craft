#pragma once
#include <MeshCraft/Mc3/Mc3Extrude.hpp>
#include <MeshCraft/Mc3/Mc3Primitive.hpp>
#include <cstdint>
#include <vector>

namespace mc3togltf {

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

// Individual primitives (used by buildPrimitive)
MeshData buildBox     (float w, float h, float d);
MeshData buildSphere  (float radius, int segments);
MeshData buildCylinder(float radius, float height, int segments, const std::string& axis);
MeshData buildCone    (float radius, float height, int segments);
MeshData buildPlane   (float w, float d, const std::string& axis);

} // namespace mc3togltf
