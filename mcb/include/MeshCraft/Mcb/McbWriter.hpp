#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include <filesystem>
#include <iosfwd>

namespace MeshCraft::Mcb {

// Serialize a Mc3Document to MCB binary format.
void saveToFile(const Mc3::Mc3Document& doc, const std::filesystem::path& path);
void saveToBinary(const Mc3::Mc3Document& doc, std::ostream& out);

} // namespace MeshCraft::Mcb
