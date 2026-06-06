#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include <filesystem>

namespace MeshCraft::Mc3::Internal {

class Mc3XmlWriter {
public:
    void write(const Mc3Document& doc, const std::filesystem::path& path);
};

} // namespace MeshCraft::Mc3::Internal
