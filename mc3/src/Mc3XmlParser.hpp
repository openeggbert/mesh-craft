#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"
#include "MeshCraft/Mc3/Mc3Validation.hpp"
#include <filesystem>
#include <string>

namespace MeshCraft::Mc3::Internal {

class Mc3XmlParser {
public:
    // Parse from a file on disk. SYS-W1-01: `validation`, when non-null,
    // receives a warning/error entry for every clamp/default/rejection the
    // parse performs (in addition to, not instead of, the existing
    // throw-on-hard-rejection behavior below) -- see Mc3Validation.hpp.
    Mc3Document parse(const std::filesystem::path& path,
                      const Mc3LoadPolicy& policy = Mc3LoadPolicy::trusted(),
                      Mc3Validation* validation = nullptr);

    // Parse from an in-memory string. `sourceDir` is the base directory that
    // relative include/resource paths resolve against.
    Mc3Document parseString(const std::string& xml,
                            const std::filesystem::path& sourceDir,
                            const Mc3LoadPolicy& policy = Mc3LoadPolicy::untrusted(),
                            Mc3Validation* validation = nullptr);
};

} // namespace MeshCraft::Mc3::Internal
