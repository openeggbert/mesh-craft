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
};

} // namespace MeshCraft::Mc3
