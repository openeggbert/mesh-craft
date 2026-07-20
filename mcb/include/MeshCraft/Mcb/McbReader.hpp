#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"
#include "MeshCraft/Mc3/Mc3Validation.hpp"
#include <filesystem>
#include <iosfwd>

namespace MeshCraft::Mcb {

// Deserialize a Mc3Document from MCB binary format. The default policy is
// trusted (permissive) -- pass Mc3LoadPolicy::untrusted() for content that
// must not read an arbitrary local file via a resource path (meshSource,
// texture uri, SVG/embed/sound/music src). MCB has no <include>-equivalent
// concept (it's a binary snapshot of an already-fully-resolved document),
// so only Mc3LoadPolicy::confineResourcePathsToRoot has any effect here --
// allowIncludes/confineIncludesToRoot/maxIncludeDepth are inert for this
// format, matching Mc3JsonParser's own established scope decision (AUD-068).
Mc3::Mc3Document loadFromFile(const std::filesystem::path& path);
Mc3::Mc3Document loadFromFile(const std::filesystem::path& path, const Mc3::Mc3LoadPolicy& policy);
Mc3::Mc3Document loadFromBinary(std::istream& in);
Mc3::Mc3Document loadFromBinary(std::istream& in, const Mc3::Mc3LoadPolicy& policy);

// SYS-W1-01: validation-capturing counterparts. `validation` receives a
// warning/error entry for every clamp/rejection the read performs (e.g. an
// out-of-range enum clamped to 0, a type-tag mismatch, a sanity-limit
// overflow) -- ADDITIVE to, not a replacement for, the existing
// throw-on-hard-rejection / clamp-on-recoverable-issue behavior of the
// overloads above. See Mc3Validation.hpp for the entry shape.
Mc3::Mc3Document loadFromFile(const std::filesystem::path& path, Mc3::Mc3Validation& validation);
Mc3::Mc3Document loadFromFile(const std::filesystem::path& path, const Mc3::Mc3LoadPolicy& policy,
                               Mc3::Mc3Validation& validation);
Mc3::Mc3Document loadFromBinary(std::istream& in, Mc3::Mc3Validation& validation);
Mc3::Mc3Document loadFromBinary(std::istream& in, const Mc3::Mc3LoadPolicy& policy,
                                 Mc3::Mc3Validation& validation);

} // namespace MeshCraft::Mcb
