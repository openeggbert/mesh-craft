#pragma once

#include <MeshCraft/Mc3/Mc3SvgTexture.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mc3togltf {

struct SvgRasterImage {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgba;
};

// Parses inline markup or basePath / texture.src. Output is RGBA and its
// largest dimension is capped at 2048 pixels to bound allocations from
// user-controlled SVG documents. An empty image indicates failure.
SvgRasterImage rasterizeSvgTexture(const MeshCraft::Mc3::Mc3SvgTexture& texture,
                                   const std::filesystem::path& basePath,
                                   std::string* error = nullptr);

} // namespace mc3togltf
