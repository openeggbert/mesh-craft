#pragma once
#include <filesystem>

namespace MeshCraft::Mc3 { class Mc3Document; }

namespace MeshCraft::Mc3::Internal {

class Mc3XmlWriter {
public:
    void write(const Mc3Document& doc, const std::filesystem::path& path);
};

} // namespace MeshCraft::Mc3::Internal
