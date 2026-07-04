#pragma once
#include <filesystem>
#include <random>
#include <sstream>
#include <string>

namespace MeshCraft {

// A per-process atomic counter alone (the pattern this replaces, found
// independently in ModelRegistry.cpp, AiResponseAlgorithms.hpp, and
// MeshCraftApplication_UiAi.cpp) resets to 0 on every process launch, so two
// MeshCraft processes sharing the same OS temp directory can collide on the
// exact same filename. Append a random 64-bit suffix (thread_local RNG
// seeded from std::random_device) instead, so concurrent processes/threads
// can't collide (STAB-0611/STAB-0635).
inline std::filesystem::path uniqueTempPath(const std::string& prefix,
                                             const std::string& extension)
{
    thread_local std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream oss;
    oss << prefix << "_" << std::hex << rng() << extension;
    return std::filesystem::temp_directory_path() / oss.str();
}

} // namespace MeshCraft
