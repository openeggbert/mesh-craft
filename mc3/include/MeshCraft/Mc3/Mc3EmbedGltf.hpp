#pragma once

#include <string>

namespace MeshCraft::Mc3 {

// Embedded or externally-referenced GLTF/GLB asset.
// XML:  <embed type="gltf" id="car" src="models/car.glb"/>
//       <embed type="gltf" id="car">BASE64==</embed>
// A Mesh object may reference an embed instead of a file path by setting
// meshSource to "embed:<id>" (e.g. meshSource = "embed:car").
struct Mc3EmbedGltf {
    std::string id;
    std::string src;           // path to external .glb file (empty if inline)
    std::string base64Content; // base64-encoded GLB data (empty if external)

    [[nodiscard]] bool isInline()   const { return src.empty() && !base64Content.empty(); }
    [[nodiscard]] bool isExternal() const { return !src.empty(); }
};

} // namespace MeshCraft::Mc3
