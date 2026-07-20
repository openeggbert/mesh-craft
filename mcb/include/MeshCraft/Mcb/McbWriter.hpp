#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include <filesystem>
#include <iosfwd>

namespace MeshCraft::Mcb {

// Serialize a Mc3Document to MCB binary format.
//
// SYS-W14-25 (2026-07-20): `compress` opts into zlib-compressing the
// document payload (header stays uncompressed and unchanged either way,
// so old readers still correctly reject a compressed file by its flags
// byte rather than misparsing it). Default false -- compression trades
// write-time CPU for file size, so it stays an explicit opt-in rather
// than changing every existing caller's output. Throws std::runtime_error
// if `compress` is true but this build was compiled without zlib
// available (see mcb/CMakeLists.txt's `find_package(ZLIB)` block).
void saveToFile(const Mc3::Mc3Document& doc, const std::filesystem::path& path,
                bool compress = false);
void saveToBinary(const Mc3::Mc3Document& doc, std::ostream& out, bool compress = false);

} // namespace MeshCraft::Mcb
