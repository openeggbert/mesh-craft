#include <cstdio>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg.h>
#include <nanosvgrast.h>

#include "SvgRasterizer.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

namespace mc3togltf {
namespace {
constexpr int kMaxSvgRasterDimension = 2048;

void setError(std::string* error, const std::string& message) {
    if (error) *error = message;
}

std::string loadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream stream;
    stream << in.rdbuf();
    return stream.str();
}
} // namespace

SvgRasterImage rasterizeSvgTexture(const MeshCraft::Mc3::Mc3SvgTexture& texture,
                                   const std::filesystem::path& basePath,
                                   std::string* error)
{
    std::string markup;
    if (texture.isInline()) {
        markup = texture.inlineContent;
    } else if (texture.isExternal()) {
        const std::filesystem::path path = basePath / texture.src;
        markup = loadTextFile(path);
        if (markup.empty()) {
            setError(error, "cannot read SVG file '" + path.string() + "'");
            return {};
        }
    } else {
        setError(error, "SVG has neither source path nor inline markup");
        return {};
    }

    // nsvgParse mutates the input while tokenizing it.
    NSVGimage* parsed = nsvgParse(markup.data(), "px", 96.0f);
    if (!parsed) {
        setError(error, "invalid or unsupported SVG markup");
        return {};
    }

    const float sourceWidth = parsed->width;
    const float sourceHeight = parsed->height;
    if (!std::isfinite(sourceWidth) || !std::isfinite(sourceHeight) ||
        sourceWidth <= 0.0f || sourceHeight <= 0.0f) {
        nsvgDelete(parsed);
        setError(error, "SVG has invalid dimensions");
        return {};
    }

    const float scale = std::min(
        1.0f, static_cast<float>(kMaxSvgRasterDimension) / std::max(sourceWidth, sourceHeight));
    const int width = std::max(1, static_cast<int>(std::ceil(sourceWidth * scale)));
    const int height = std::max(1, static_cast<int>(std::ceil(sourceHeight * scale)));
    const auto pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4) {
        nsvgDelete(parsed);
        setError(error, "SVG raster size overflows pixel buffer");
        return {};
    }

    SvgRasterImage result;
    result.width = width;
    result.height = height;
    result.rgba.assign(pixelCount * 4, 0);
    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
    if (!rasterizer) {
        nsvgDelete(parsed);
        setError(error, "cannot create SVG rasterizer");
        return {};
    }
    nsvgRasterize(rasterizer, parsed, 0.0f, 0.0f, scale, result.rgba.data(),
                  width, height, width * 4);
    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(parsed);
    return result;
}

} // namespace mc3togltf
