#pragma once

#include <MeshCraft/Mc3/Mc3SvgTexture.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
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

// Compact deterministic identity for the raster cache. In particular, inline
// markup is represented by a 64-bit FNV-1a digest rather than being copied
// into a map key every frame.
std::string svgTextureCacheKey(const MeshCraft::Mc3::Mc3SvgTexture& texture,
                               const std::filesystem::path& basePath);

// External SVGs are hot-reloaded when this timestamp changes. Inline SVGs do
// not have a filesystem timestamp and return std::nullopt.
std::optional<std::filesystem::file_time_type>
svgTextureLastWriteTime(const MeshCraft::Mc3::Mc3SvgTexture& texture,
                        const std::filesystem::path& basePath);

} // namespace mc3togltf
