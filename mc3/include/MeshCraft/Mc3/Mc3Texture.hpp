#pragma once

#include <string>

namespace MeshCraft::Mc3 {

struct Mc3Texture {
    std::string name;
    std::string uri;
    std::string wrapU{"repeat"};
    std::string wrapV{"repeat"};
    std::string filter{"linear"};
    std::string colorSpace{"srgb"};
    bool mipMaps{true};

    // --- Builder helpers --------------------------------------------------
    Mc3Texture() = default;
    Mc3Texture(std::string n, std::string u) : name(std::move(n)), uri(std::move(u)) {}
    Mc3Texture(std::string n, std::string u, std::string wu, std::string wv,
               std::string filt = "linear", std::string cs = "srgb")
        : name(std::move(n)), uri(std::move(u)),
          wrapU(std::move(wu)), wrapV(std::move(wv)),
          filter(std::move(filt)), colorSpace(std::move(cs)) {}

    Mc3Texture& withWrap(std::string u, std::string v) { wrapU=u; wrapV=v; return *this; }
    Mc3Texture& withFilter(std::string f)  { filter = std::move(f); return *this; }
    Mc3Texture& asLinear()                 { colorSpace = "linear"; return *this; }
    Mc3Texture& asSrgb()                   { colorSpace = "srgb";   return *this; }
};

} // namespace MeshCraft::Mc3
