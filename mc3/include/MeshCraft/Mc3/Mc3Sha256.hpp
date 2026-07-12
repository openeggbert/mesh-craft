#pragma once

#include <string>

namespace MeshCraft::Mc3 {

// R110 -- minimal, self-contained SHA-256 (FIPS 180-4), used to compute a
// library's content hash (mesh_world_revival.md §7: `hash="sha256:..."`).
// Implemented in-tree rather than pulling in a new FetchContent dependency
// for one stable, well-defined 63-year-old algorithm with no moving parts.
// Returns the lowercase hex digest, WITHOUT a "sha256:" prefix (callers
// that need the prefixed form, e.g. Mc3Document::library->contentHash,
// prepend it themselves).
std::string sha256Hex(const std::string& data);

} // namespace MeshCraft::Mc3
