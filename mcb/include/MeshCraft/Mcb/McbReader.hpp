#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Validation.hpp"
#include <filesystem>
#include <iosfwd>

namespace MeshCraft::Mcb {

// Deserialize a Mc3Document from MCB binary format.
Mc3::Mc3Document loadFromFile(const std::filesystem::path& path);
Mc3::Mc3Document loadFromBinary(std::istream& in);

// SYS-W1-01: validation-capturing counterparts. `validation` receives a
// warning/error entry for every clamp/rejection the read performs (e.g. an
// out-of-range enum clamped to 0, a type-tag mismatch, a sanity-limit
// overflow) -- ADDITIVE to, not a replacement for, the existing
// throw-on-hard-rejection / clamp-on-recoverable-issue behavior of the
// overloads above. See Mc3Validation.hpp for the entry shape.
Mc3::Mc3Document loadFromFile(const std::filesystem::path& path, Mc3::Mc3Validation& validation);
Mc3::Mc3Document loadFromBinary(std::istream& in, Mc3::Mc3Validation& validation);

} // namespace MeshCraft::Mcb
