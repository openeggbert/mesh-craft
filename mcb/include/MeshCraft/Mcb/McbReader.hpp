#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include <filesystem>
#include <iosfwd>

namespace MeshCraft::Mcb {

// Deserialize a Mc3Document from MCB binary format.
Mc3::Mc3Document loadFromFile(const std::filesystem::path& path);
Mc3::Mc3Document loadFromBinary(std::istream& in);

} // namespace MeshCraft::Mcb
