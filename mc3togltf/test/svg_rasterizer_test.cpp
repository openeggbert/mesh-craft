#include "SvgRasterizer.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using MeshCraft::Mc3::Mc3SvgTexture;

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void writeSvg(const std::filesystem::path& path, const char* color) {
    std::ofstream out(path);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\">"
        << "<rect width=\"8\" height=\"8\" fill=\"" << color << "\"/></svg>";
}
} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("meshcraft_svg_rasterizer_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) {
        std::cerr << "FAIL: cannot create temporary directory\n";
        return 1;
    }

    const auto source = root / "icon.svg";
    Mc3SvgTexture texture;
    texture.id = "icon";
    texture.src = "icon.svg";
    writeSvg(source, "#e02020");

    const std::string key = mc3togltf::svgTextureCacheKey(texture, root);
    check(key.rfind("svg:", 0) == 0 && key.size() < 32,
          "cache key is compact and does not contain SVG source text");
    const auto before = mc3togltf::svgTextureLastWriteTime(texture, root);
    check(before.has_value(), "external SVG provides a write timestamp");
    const auto red = mc3togltf::rasterizeSvgTexture(texture, root);
    check(red.width == 8 && red.height == 8 && red.rgba.size() == 8U * 8U * 4U,
          "external SVG rasterizes to expected dimensions");
    check(!red.rgba.empty() && red.rgba[0] > 180 && red.rgba[1] < 80,
          "first rasterization contains red source pixels");

    writeSvg(source, "#2060e0");
    if (before)
        std::filesystem::last_write_time(source, *before + std::chrono::seconds(2), ec);
    const auto after = mc3togltf::svgTextureLastWriteTime(texture, root);
    check(after.has_value() && after != before,
          "editing external SVG changes the cache invalidation timestamp");
    const auto blue = mc3togltf::rasterizeSvgTexture(texture, root);
    check(!blue.rgba.empty() && blue.rgba[2] > 180 && blue.rgba[0] < 80,
          "rasterization after edit contains new blue source pixels");

    Mc3SvgTexture inlineTexture;
    inlineTexture.id = "inline";
    inlineTexture.inlineContent = "<svg width=\"1\" height=\"1\"/>";
    check(!mc3togltf::svgTextureLastWriteTime(inlineTexture, root).has_value(),
          "inline SVG has no filesystem timestamp");

    std::filesystem::remove_all(root, ec);
    if (failures == 0) std::cout << "SVG rasterizer cache test: PASS\n";
    return failures == 0 ? 0 : 1;
}
