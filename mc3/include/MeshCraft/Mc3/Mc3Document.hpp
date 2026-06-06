#pragma once

#include "MeshCraft/Mc3/Mc3Camera.hpp"
#include "MeshCraft/Mc3/Mc3Environment.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Texture.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

class Mc3Document {
public:
    std::string version{"0.1"};
    std::string model;
    std::string unit{"meter"};
    std::string coordinateSystem{"right_handed_y_up"};

    std::optional<Mc3Environment> environment;
    std::vector<Mc3Light>   lights;
    std::vector<Mc3Camera>  cameras;
    std::string defaultCamera;

    std::map<std::string, Mc3Texture>  textures;
    std::map<std::string, Mc3Material> materials;
    std::map<std::string, std::shared_ptr<Mc3Object>> definitions;
    std::vector<std::shared_ptr<Mc3Object>> objects;

    // TODO: actions map

    // Load from MC3 XML (.mc3.xml) — implemented in mc3togltf
    static Mc3Document loadFromFile(const std::filesystem::path& path);

    // TODO: save to XML
    void saveToFile(const std::filesystem::path& path) const;
};

} // namespace MeshCraft::Mc3
