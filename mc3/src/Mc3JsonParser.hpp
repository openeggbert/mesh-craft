#pragma once
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"
#include "MeshCraft/Mc3/Mc3Validation.hpp"
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
    // SYS-W1-08: `validation`, when non-null, receives a warning/error entry
    // for every clamp/default/rejection the parse performs -- in addition
    // to, not instead of, the existing throw-on-hard-rejection behavior
    // below. Mirrors Mc3XmlParser's own validation-capturing overload; see
    // Mc3Validation.hpp.
    Mc3Document parse(const std::filesystem::path& path,
                      const Mc3LoadPolicy& policy = Mc3LoadPolicy::trusted(),
                      Mc3Validation* validation = nullptr);

    Mc3Document parseString(const std::string& jsonText,
                            const std::filesystem::path& sourceDir,
                            const Mc3LoadPolicy& policy = Mc3LoadPolicy::untrusted(),
                            Mc3Validation* validation = nullptr);
};

} // namespace MeshCraft::Mc3::Internal
