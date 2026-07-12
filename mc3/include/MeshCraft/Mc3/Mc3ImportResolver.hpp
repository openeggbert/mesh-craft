#pragma once

#include "MeshCraft/Mc3/Mc3Document.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

// R101 -- resolves a document's <imports>/"imports" (Mc3Import, see
// Mc3Document.hpp) into a namespace-qualified map of definitions, loading
// referenced .mc3lib files from a caller-supplied list of search
// directories (mesh_world_revival.md §7).
//
// Resolution convention (v1, a deliberate, reversible choice -- no
// URI-to-file scheme is specified anywhere else in this project yet):
// a "source" of "mc3lib://<name>@<version>" resolves to the first of
// "<searchDir>/<name>-<version>.mc3lib.xml" or
// "<searchDir>/<name>-<version>.mc3lib.json" that exists, checked across
// searchDirs in the order given. Revisit this convention (e.g. a real
// package registry/index) once real content batches (R112+) need it;
// nothing yet depends on a more elaborate scheme.
//
// Scope: resolve() only returns definitions from `doc`'s own DIRECT
// imports, keyed "<importNamespace>:<definitionId>" using `doc`'s own
// local aliases. If a resolved library itself declares further imports,
// those are resolved too (recursively) for cycle-detection and missing-
// dependency validation ONLY -- their definitions are NOT merged into the
// returned map. A library's own internal "<alias>:<id>" instance
// references are that library's own concern, resolved when IT is loaded/
// compiled, not flattened into whatever document happens to import it.
// (Standalone-compile dependency pruning -- computing the minimal set of
// transitively-used definitions for a compiled asset -- is explicitly R110's
// deferred follow-up, not this class's job.)
class Mc3ImportResolver {
public:
    explicit Mc3ImportResolver(std::vector<std::filesystem::path> searchDirs);

    // Throws std::runtime_error on:
    //  - an import whose `source` doesn't resolve to any file in any
    //    search directory (message names the source and the paths tried),
    //  - a content-hash mismatch (message names the source, expected and
    //    actual hash),
    //  - an import cycle (message names the cyclic source chain).
    std::map<std::string, std::shared_ptr<Mc3Object>> resolve(const Mc3Document& doc) const;

    // R102 -- convenience that makes an import actually USABLE end-to-end
    // before R103's dynamic `<script>` placement exists: calls resolve(doc)
    // and inserts every resulting definition directly into
    // doc.definitions[...] under its own "<importNamespace>:<definitionId>"
    // key. Every EXISTING instance consumer (SceneRenderer, mc3togltf, CSG
    // evaluation -- anything that already resolves an `<instance
    // definition="...">` by looking up doc.definitions[key], see
    // Mc3Object::resolvedInstanceDefinitionKey()'s own doc comment) then
    // resolves an imported instance transparently, with no consumer-side
    // changes at all -- a composite object (e.g. a house `<instance
    // definition="door_lib:door.simple">`) just works once its document
    // imports "door_lib" and this is called. Same throw conditions as
    // resolve(). A key that already exists in doc.definitions (a local id
    // that happens to collide with "namespace:id" syntax) is overwritten --
    // an imported definition takes precedence, the same "last write wins"
    // discipline this codebase's <include> merging already uses.
    void resolveAndMergeInto(Mc3Document& doc) const;

private:
    std::filesystem::path resolveSourceToPath(const std::string& source) const;
    Mc3Document loadLibrary(const std::string& source, const std::string& expectedHash) const;
    void checkNoCycleAndDescend(const Mc3Document& libDoc, std::vector<std::string>& inProgress) const;

    std::vector<std::filesystem::path> searchDirs_;
};

} // namespace MeshCraft::Mc3
