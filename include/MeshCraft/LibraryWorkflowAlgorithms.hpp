#pragma once

// CNA-free helpers for the native MC3 library workflow. The editor UI calls
// these directly; the same functions are exercised by library_workflow_test
// so publishing/import behaviour is not only proven through ImGui widgets.

#include "MeshCraft/EditorAlgorithms.hpp"
#include <MeshCraft/Mc3/Mc3ImportResolver.hpp>

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace MeshCraft {

enum class LibraryFileFormatAlg { Xml, Json };

inline LibraryFileFormatAlg libraryFileFormatFromPathAlg(const std::filesystem::path& path)
{
    const std::string name = path.filename().string();
    constexpr std::string_view xmlSuffix = ".mc3lib.xml";
    constexpr std::string_view jsonSuffix = ".mc3lib.json";
    if (name.size() >= xmlSuffix.size() &&
        name.compare(name.size() - xmlSuffix.size(), xmlSuffix.size(), xmlSuffix) == 0)
        return LibraryFileFormatAlg::Xml;
    if (name.size() >= jsonSuffix.size() &&
        name.compare(name.size() - jsonSuffix.size(), jsonSuffix.size(), jsonSuffix) == 0)
        return LibraryFileFormatAlg::Json;
    throw std::invalid_argument("Library path must end in .mc3lib.xml or .mc3lib.json");
}

inline bool isLibraryIdentifierAlg(std::string_view value)
{
    if (value.empty()) return false;
    const unsigned char first = static_cast<unsigned char>(value.front());
    if (!std::isalpha(first) && value.front() != '_') return false;
    for (char c : value) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '_' && c != '-' && c != '.') return false;
    }
    return true;
}

inline bool isSemanticVersionAlg(std::string_view value)
{
    int components = 1;
    bool digitSeen = false;
    for (char c : value) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            digitSeen = true;
        } else if (c == '.' && digitSeen && components < 3) {
            ++components;
            digitSeen = false;
        } else {
            return false;
        }
    }
    return digitSeen && components == 3;
}

inline void requireLibraryIdentityAlg(const Mc3::Mc3LibraryInfo& library)
{
    if (!isLibraryIdentifierAlg(library.libraryNamespace))
        throw std::invalid_argument(
            "Library namespace must start with a letter or '_' and use only letters, digits, '.', '-', '_'");
    if (!isSemanticVersionAlg(library.version))
        throw std::invalid_argument("Library version must use semantic version major.minor.patch");
}

// Creates a named reusable definition from one placed scene object and swaps
// that placement for an Instance. Unlike the old auto-named conversion, this
// is suitable for a deliberate authoring/publishing workflow and rejects a
// name that would silently overwrite an existing definition.
inline std::shared_ptr<Mc3::Mc3Object> createNamedDefinitionFromSelectionAlg(
    Mc3::Mc3Document& doc, const std::shared_ptr<Mc3::Mc3Object>& src,
    const std::string& definitionId)
{
    if (!src) throw std::invalid_argument("Cannot create a definition from an empty selection");
    if (!isLibraryIdentifierAlg(definitionId))
        throw std::invalid_argument("Definition id must start with a letter or '_' and use only letters, digits, '.', '-', '_'");
    if (doc.definitions.count(definitionId))
        throw std::invalid_argument("Definition already exists: " + definitionId);

    auto defObj = deepCopyObjectAlg(*src);
    defObj->id   = definitionId;
    defObj->name = definitionId;
    defObj->transform.position = {0.0f, 0.0f, 0.0f};
    defObj->transform.rotation = {0.0f, 0.0f, 0.0f};
    defObj->transform.scale    = {1.0f, 1.0f, 1.0f};
    doc.definitions[definitionId] = defObj;

    auto inst = std::make_shared<Mc3::Mc3Object>();
    inst->id         = src->id;
    inst->name       = src->name.empty() ? definitionId : src->name;
    inst->type       = Mc3::ObjectType::Instance;
    inst->definition = definitionId;
    inst->transform  = src->transform;
    inst->visible    = src->visible;
    inst->tags       = src->tags;
    inst->layer      = src->layer;

    auto* parentList = findParentListAlg(doc.objects, src.get());
    if (parentList) {
        for (auto& obj : *parentList)
            if (obj.get() == src.get()) { obj = inst; break; }
    } else {
        doc.objects.push_back(inst);
    }
    return inst;
}

// Builds a self-contained reusable library containing one definition and the
// dependent materials/textures selected by exportSubtreeTemplateAlg(). The
// caller picks XML versus JSON through the dedicated Mc3Document library I/O.
inline Mc3::Mc3Document publishDefinitionAsLibraryAlg(
    const Mc3::Mc3Document& source, const std::string& definitionId,
    const std::string& libraryNamespace, const std::string& version)
{
    if (!isLibraryIdentifierAlg(definitionId))
        throw std::invalid_argument("Definition id is not valid for publishing: " + definitionId);
    const auto it = source.definitions.find(definitionId);
    if (it == source.definitions.end() || !it->second)
        throw std::invalid_argument("Definition not found for publishing: " + definitionId);

    Mc3::Mc3LibraryInfo library{libraryNamespace, version, ""};
    requireLibraryIdentityAlg(library);
    Mc3::Mc3Document published = exportSubtreeTemplateAlg(source, definitionId, it->second);
    published.model = libraryNamespace;
    published.library = std::move(library);
    published.library->contentHash = "sha256:" + published.computeLibraryContentHash();
    return published;
}

struct ImportedDefinitionsRefreshAlg {
    std::vector<Mc3::Mc3ResolvedImport> imports;
    std::size_t definitionCount{0};
};

// Re-resolves direct imports without serializing their transient definitions
// into the owning scene. An imported definition is marked in includedDefs so
// the existing writer omits it; editing it through the Definitions panel
// clears that marker and intentionally turns it into a local copy. Failed
// refreshes restore the last working imported set, so a temporary missing
// library does not make a currently-open scene disappear.
inline ImportedDefinitionsRefreshAlg refreshImportedDefinitionsAlg(
    Mc3::Mc3Document& document, const std::vector<std::filesystem::path>& searchDirs,
    std::set<std::string>& importedDefinitionKeys)
{
    std::map<std::string, std::shared_ptr<Mc3::Mc3Object>> previous;
    std::set<std::string> previousKeys;
    for (const auto& key : importedDefinitionKeys) {
        if (document.includedDefs.erase(key) != 0) {
            auto it = document.definitions.find(key);
            if (it != document.definitions.end()) {
                previous.emplace(key, it->second);
                document.definitions.erase(it);
                previousKeys.insert(key);
            }
        }
    }
    importedDefinitionKeys.clear();

    auto restorePrevious = [&] {
        for (const auto& [key, object] : previous) {
            document.definitions[key] = object;
            document.includedDefs.insert(key);
        }
        importedDefinitionKeys = previousKeys;
    };

    try {
        std::set<std::string> aliases;
        for (const auto& request : document.imports) {
            if (!isLibraryIdentifierAlg(request.importNamespace))
                throw std::runtime_error("Invalid import namespace: " + request.importNamespace);
            if (!aliases.insert(request.importNamespace).second)
                throw std::runtime_error("Definition collision: duplicate import namespace '" +
                                         request.importNamespace + "'");
        }

        Mc3::Mc3ImportResolver resolver(searchDirs);
        auto resolution = resolver.resolveWithHealth(document);
        for (const auto& [key, object] : resolution.definitions) {
            (void)object;
            if (document.definitions.count(key))
                throw std::runtime_error("Definition collision: imported definition '" + key +
                                         "' would overwrite a local definition");
        }

        for (auto& [key, object] : resolution.definitions) {
            document.definitions[key] = std::move(object);
            document.includedDefs.insert(key);
            importedDefinitionKeys.insert(key);
        }
        return ImportedDefinitionsRefreshAlg{
            std::move(resolution.imports), importedDefinitionKeys.size()};
    } catch (...) {
        restorePrevious();
        throw;
    }
}

} // namespace MeshCraft
