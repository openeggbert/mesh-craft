#pragma once

#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Texture.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

// Top-level container for an MC3 model file.
class Mc3Document {
public:
    std::string version{"0.1"};
    std::string model;
    std::string unit{"meter"};
    std::string coordinateSystem{"right_handed_y_up"};

    std::map<std::string, Mc3Texture>  textures;
    std::map<std::string, Mc3Material> materials;
    std::map<std::string, std::shared_ptr<Mc3Object>> definitions;
    std::vector<std::shared_ptr<Mc3Object>> objects;

    // TODO: actions map

    // TODO: load from YAML (.mc3.yaml)
    static Mc3Document loadFromFile(const std::filesystem::path& path);

    // TODO: save to YAML (.mc3.yaml)
    void saveToFile(const std::filesystem::path& path) const;
};

} // namespace MeshCraft::Mc3
