#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

struct Mc3ObjectOverride {
    std::string id;
    std::optional<bool>                  visible;
    std::optional<std::array<float,3>>   position;
    std::optional<std::array<float,3>>   rotation;
    std::optional<std::string>           material;
};

struct Mc3SceneState {
    std::string name;
    std::vector<Mc3ObjectOverride> overrides;
};

} // namespace MeshCraft::Mc3
