#pragma once

#include <filesystem>
#include <functional>
#include <stdexcept>

namespace MeshCraft::Mc3 {

// SYS-W9-06: replaces the 4 independently duplicated
// "<destination>.tmp" + std::filesystem::rename() call sites that used to
// live in Mc3XmlWriter, Mc3JsonWriter, McbWriter, and GltfExporter's GLB
// path. Those all shared the same two gaps: a fixed temporary name can
// collide between two concurrent saves of the same destination (e.g.
// autosave racing a manual save), and replacing an *existing* destination
// via rename() was never exercised against a real Windows run.
//
// writeFileAtomically() calls `writeFn` with a unique sibling temporary path
// derived from `destination`, then replaces `destination` with it only after
// `writeFn` returns successfully -- a crash, disk-full, or permission
// failure mid-write can therefore never leave a truncated or corrupt file at
// `destination`. `writeFn` must fully close/flush its own output before
// returning (fstreams do this on destruction; explicit APIs like
// tinyxml2::XMLDocument::SaveFile()/tinygltf's writer already close their
// handle before returning).
//
// If `writeFn` throws, that exception propagates unchanged (so each caller
// keeps its own format-specific message) after the temporary file is
// removed and `destination` is left untouched. If the finalizing replace
// itself fails, `destination` is still left untouched, the temporary file is
// removed, and AtomicFinalizeError is thrown.
class AtomicFinalizeError : public std::runtime_error {
public:
    explicit AtomicFinalizeError(const std::string& message)
        : std::runtime_error(message) {}
};

// Returns a temporary path sibling to `destination` that does not exist yet,
// combining a monotonic call counter with a high-resolution clock reading so
// concurrent calls (same or different destination, same or different
// process) never produce the same name. Exposed separately so it can be
// tested directly; writeFileAtomically() is the primary entry point.
std::filesystem::path uniqueSiblingTempPath(const std::filesystem::path& destination);

void writeFileAtomically(
    const std::filesystem::path& destination,
    const std::function<void(const std::filesystem::path& tmpPath)>& writeFn);

} // namespace MeshCraft::Mc3
