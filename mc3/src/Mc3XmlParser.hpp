#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"
#include <filesystem>
#include <string>

namespace MeshCraft::Mc3::Internal {

class Mc3XmlParser {
public:
    // Parse from a file on disk.
    Mc3Document parse(const std::filesystem::path& path,
                      const Mc3LoadPolicy& policy = Mc3LoadPolicy::trusted());

    // Parse from an in-memory string. `sourceDir` is the base directory that
    // relative include/resource paths resolve against.
    Mc3Document parseString(const std::string& xml,
                            const std::filesystem::path& sourceDir,
                            const Mc3LoadPolicy& policy = Mc3LoadPolicy::untrusted());
};

} // namespace MeshCraft::Mc3::Internal
