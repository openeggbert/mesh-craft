#pragma once
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <filesystem>
#include <string>

namespace mc3togltf {

// Parse a .mc3.xml file and populate an Mc3Document.
// Throws std::runtime_error on fatal parse errors.
class Mc3XmlParser {
public:
    MeshCraft::Mc3::Mc3Document parse(const std::filesystem::path& path);

};

} // namespace mc3togltf
