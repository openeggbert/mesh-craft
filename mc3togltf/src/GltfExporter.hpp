#pragma once
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <filesystem>
#include <string>

namespace mc3togltf {

enum class OutputFormat { GLTF, GLB };

// Returns the OutputFormat implied by path's extension (.gltf / .glb, case-insensitive).
// Throws std::runtime_error for any other extension.
OutputFormat outputFormatFromPath(const std::filesystem::path& outputPath);

class GltfExporter {
public:
    // When false (default), CSG nodes (union/difference/intersection) are evaluated
    // using the Manifold library and exported as a single merged mesh.  Throws on
    // evaluation failure.
    // When true (--allow-approximate-csg / editor checkbox), Manifold evaluation is
    // skipped and children are exported as separate meshes — geometrically incorrect,
    // useful only for debugging or previewing unsupported scene graphs.
    bool allowApproximateCSG{false};

    void exportDocument(const MeshCraft::Mc3::Mc3Document& doc,
                        const std::filesystem::path& outputPath,
                        OutputFormat format);
};

} // namespace mc3togltf
