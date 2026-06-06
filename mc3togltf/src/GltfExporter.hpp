#pragma once
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <filesystem>
#include <string>

namespace mc3togltf {

enum class OutputFormat { GLTF, GLB };

class GltfExporter {
public:
    // Convert Mc3Document to glTF or GLB file.
    // format is inferred from outputPath extension (.gltf / .glb) if not specified.
    void exportDocument(const MeshCraft::Mc3::Mc3Document& doc,
                        const std::filesystem::path& outputPath,
                        OutputFormat format);
};

} // namespace mc3togltf
