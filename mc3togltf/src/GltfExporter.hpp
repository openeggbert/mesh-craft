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
    // When false (default), any CSG node (union/difference/intersection) throws instead of
    // exporting wrong geometry. Set true with --allow-approximate-csg (CLI) or the
    // "Allow approximate CSG export" checkbox (editor) to export children separately.
    bool allowApproximateCSG{false};

    void exportDocument(const MeshCraft::Mc3::Mc3Document& doc,
                        const std::filesystem::path& outputPath,
                        OutputFormat format);
};

} // namespace mc3togltf
