#pragma once
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <filesystem>
#include <string>

namespace mc3togltf {

enum class OutputFormat { GLTF, GLB };

class GltfExporter {
public:
    // When false (default), difference/intersection nodes throw instead of exporting
    // wrong geometry. Set true with --allow-approximate-csg to export children separately.
    bool allowApproximateCSG{false};

    void exportDocument(const MeshCraft::Mc3::Mc3Document& doc,
                        const std::filesystem::path& outputPath,
                        OutputFormat format);
};

} // namespace mc3togltf
