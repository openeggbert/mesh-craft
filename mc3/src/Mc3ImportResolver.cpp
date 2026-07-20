#include "MeshCraft/Mc3/Mc3ImportResolver.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace MeshCraft::Mc3 {

namespace {

// Splits "mc3lib://<name>@<version>" into {name, version}. Throws
// std::runtime_error if `source` doesn't match that shape.
std::pair<std::string, std::string> parseMc3LibSource(const std::string& source) {
    constexpr const char* kPrefix = "mc3lib://";
    if (source.rfind(kPrefix, 0) != 0)
        throw std::runtime_error("Mc3ImportResolver: source '" + source +
                                  "' does not start with '" + kPrefix + "'");
    const std::string rest = source.substr(std::string(kPrefix).size());
    const auto at = rest.rfind('@');
    if (at == std::string::npos || at == 0 || at == rest.size() - 1)
        throw std::runtime_error("Mc3ImportResolver: source '" + source +
                                  "' is missing a '<name>@<version>' part");
    return {rest.substr(0, at), rest.substr(at + 1)};
}

// F18 (2026-07-20 audit): every other recursive parse path in this codebase
// has a depth cap (Mc3XmlParser.cpp's <include> processing via
// Mc3LoadPolicy::maxIncludeDepth, McbReader.cpp's RecursionGuard<256>,
// GltfExporter.cpp's kMaxNodeDepth, CsgEvaluator.cpp's CSG_MAX_DEPTH) --
// specifically because a native C++ stack overflow is not a catchable
// exception, unlike every other rejection this class already throws
// cleanly. checkNoCycleAndDescend() below had cycle detection (a REPEATED
// source in the chain) but nothing bounding a long CHAIN of DISTINCT
// .mc3lib files (A imports B imports C imports ... imports Z, no repeats),
// which recursed unbounded. 16 matches Mc3LoadPolicy::maxIncludeDepth's own
// default for the same reason: deep enough for any legitimate composed
// asset, far short of where a real stack overflow becomes a risk.
constexpr size_t kMaxImportDepth = 16;

} // namespace

Mc3ImportResolver::Mc3ImportResolver(std::vector<std::filesystem::path> searchDirs)
    : searchDirs_(std::move(searchDirs)) {}

std::filesystem::path Mc3ImportResolver::resolveSourceToPath(const std::string& source) const {
    const auto [name, version] = parseMc3LibSource(source);
    const std::string stem = name + "-" + version;

    std::ostringstream tried;
    for (const auto& dir : searchDirs_) {
        for (const char* ext : {".mc3lib.xml", ".mc3lib.json"}) {
            std::filesystem::path candidate = dir / (stem + ext);
            tried << candidate.string() << "; ";
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec) && !ec) return candidate;
        }
    }
    throw std::runtime_error("Mc3ImportResolver: could not resolve '" + source +
                              "' -- no file found, tried: " + tried.str());
}

Mc3Document Mc3ImportResolver::loadLibrary(const std::string& source,
                                           const std::string& expectedHash) const {
    const std::filesystem::path path = resolveSourceToPath(source);
    Mc3Document libDoc = (path.extension() == ".json")
                              ? Mc3Document::loadFromLibraryJsonFile(path)
                              : Mc3Document::loadFromLibraryFile(path);

    if (!expectedHash.empty()) {
        const std::string actual = "sha256:" + libDoc.computeLibraryContentHash();
        if (actual != expectedHash)
            throw std::runtime_error("Mc3ImportResolver: content hash mismatch for '" + source +
                                      "' (resolved " + path.string() + ") -- expected " +
                                      expectedHash + ", got " + actual);
    }
    return libDoc;
}

void Mc3ImportResolver::checkNoCycleAndDescend(const Mc3Document& libDoc,
                                                std::vector<std::string>& inProgress) const {
    if (inProgress.size() > kMaxImportDepth) {
        std::ostringstream chain;
        for (const auto& s : inProgress) chain << s << " -> ";
        throw std::runtime_error("Mc3ImportResolver: import chain exceeds the depth limit (" +
                                  std::to_string(kMaxImportDepth) + "): " + chain.str() + "...");
    }
    for (const auto& imp : libDoc.imports) {
        if (std::find(inProgress.begin(), inProgress.end(), imp.source) != inProgress.end()) {
            std::ostringstream chain;
            for (const auto& s : inProgress) chain << s << " -> ";
            chain << imp.source;
            throw std::runtime_error("Mc3ImportResolver: import cycle detected: " + chain.str());
        }
        inProgress.push_back(imp.source);
        const Mc3Document nested = loadLibrary(imp.source, imp.hash);
        checkNoCycleAndDescend(nested, inProgress);
        inProgress.pop_back();
    }
}

std::map<std::string, std::shared_ptr<Mc3Object>> Mc3ImportResolver::resolve(const Mc3Document& doc) const {
    std::map<std::string, std::shared_ptr<Mc3Object>> out;

    for (const auto& imp : doc.imports) {
        std::vector<std::string> inProgress{imp.source};
        const Mc3Document libDoc = loadLibrary(imp.source, imp.hash);

        for (const auto& [defId, obj] : libDoc.definitions)
            out[imp.importNamespace + ":" + defId] = obj;

        // Recurse into the imported library's OWN imports purely for cycle
        // detection / missing-dependency validation (see class doc comment
        // for why their definitions are not merged into `out`).
        checkNoCycleAndDescend(libDoc, inProgress);
    }

    return out;
}

void Mc3ImportResolver::resolveAndMergeInto(Mc3Document& doc) const {
    for (auto& [key, obj] : resolve(doc)) doc.definitions[key] = std::move(obj);
}

} // namespace MeshCraft::Mc3
