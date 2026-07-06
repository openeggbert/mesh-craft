#pragma once
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <filesystem>
#include <string>

namespace mc3togltf {

enum class OutputFormat { GLTF, GLB };

// Returns the OutputFormat implied by path's extension (.gltf / .glb, case-insensitive).
// Throws std::runtime_error for any other extension.
OutputFormat outputFormatFromPath(const std::filesystem::path& outputPath);

// Aggregate statistics populated by GltfExporter::exportDocument().
struct ExportStats {
    int objectsProcessed{0};   // MC3 objects visited (buildNode calls)
    int gltfNodes{0};          // glTF nodes written
    int uniqueMeshes{0};       // unique glTF meshes in the buffer
    int materialCount{0};      // glTF materials written
    int reusedMeshRefs{0};     // node-to-mesh assignments that reused an existing mesh
    int totalVertices{0};      // vertex count summed over unique meshes
    int totalTriangles{0};     // triangle count summed over unique meshes
    int objMeshesLoaded{0};    // external OBJ files actually parsed (cache misses)
    int csgMeshesEvaluated{0}; // CSG boolean evaluations via Manifold
    int warnings{0};           // non-fatal warnings emitted during export
};

class GltfExporter {
public:
    // When false (default), CSG nodes (union/difference/intersection) are evaluated
    // using the Manifold library and exported as a single merged mesh.  Throws on
    // evaluation failure.
    // When true (--allow-approximate-csg / editor checkbox), Manifold evaluation is
    // skipped and children are exported as separate meshes — geometrically incorrect,
    // useful only for debugging or previewing unsupported scene graphs.
    bool allowApproximateCSG{false};

    // Populated after exportDocument() returns successfully.
    ExportStats stats;

    void exportDocument(const MeshCraft::Mc3::Mc3Document& doc,
                        const std::filesystem::path& outputPath,
                        OutputFormat format);
};

} // namespace mc3togltf
