#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"
#include <filesystem>
#include <string>

namespace MeshCraft::Mc3::Internal {

// R109 -- reads the semantic mc3.json representation (see Mc3JsonWriter's
// header comment) back into the SAME Mc3Document AST the XML parser
// produces. Deliberately does not implement <include>-style merging (the
// JSON format's `includes` field round-trips as a plain string list, same
// as Mc3Document::includes itself) -- that is orthogonal to this task and
// stays XML-only for now.
class Mc3JsonParser {
public:
    Mc3Document parse(const std::filesystem::path& path,
                      const Mc3LoadPolicy& policy = Mc3LoadPolicy::trusted());

    Mc3Document parseString(const std::string& jsonText,
                            const std::filesystem::path& sourceDir,
                            const Mc3LoadPolicy& policy = Mc3LoadPolicy::untrusted());
};

} // namespace MeshCraft::Mc3::Internal
