#pragma once
#include <string>

namespace MeshCraft::Mc3 {

struct Mc3Script {
    std::string id;
    std::string type;    // "lua"
    std::string source;  // inline script source

    [[nodiscard]] bool hasSource() const { return !source.empty(); }
};

} // namespace MeshCraft::Mc3
