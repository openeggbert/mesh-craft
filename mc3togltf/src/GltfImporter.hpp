#pragma once

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace mc3togltf {

// Bounded result of importing a self-contained GLB into editable MC3 scene
// structure. Mesh geometry remains in one inline MC3 <embed>; per-primitive
// Mesh children carry selectors understood by the viewport and exporter while
// their hierarchy, transforms, materials, cameras, and lights are normal MC3
// data that can be edited after import.
struct GltfImportResult {
    MeshCraft::Mc3::Mc3Document document;
    std::vector<std::string> warnings;
    int triangleCount{0};
};

// Import one self-contained .glb. The source must fit the tighter 48 MiB
// decoded ceiling so its base64 representation remains valid under MC3's
// existing 64 MiB inline-embed parser limit. External .gltf/buffer/image
// resources are intentionally not accepted by this untrusted route.
GltfImportResult importSelfContainedGlb(const std::filesystem::path& path);

// Explicitly trusted route for a textual glTF and its declared resources.
// The source is converted to the same bounded inline GLB representation as
// importSelfContainedGlb(), so the resulting MC3 document has no continuing
// dependency on those external paths. Callers must provide an affirmative UI
// opt-in before invoking this function.
GltfImportResult importTrustedGltf(const std::filesystem::path& path);

} // namespace mc3togltf
