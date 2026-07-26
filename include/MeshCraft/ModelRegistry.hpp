#pragma once

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifdef MESHCRAFT_HAS_SQLITE3
struct sqlite3;
#endif

namespace MeshCraft {

class ModelRegistry {
public:
    static constexpr int kThumbnailWidth = 64;
    static constexpr int kThumbnailHeight = 64;

    struct Entry {
        int64_t     id{0};
        std::string group;
        std::string name;
        std::string variant;
        std::string xml;         // serialized <mc3> fragment: definitions + referenced materials
        std::string tags;
        std::string description;
        std::string source;      // e.g. "handmade", "ai_generated", "imported"
        std::string category;
        std::string license;
        std::string provenance;

        // A deterministic 64x64 RGBA visual identity generated from the
        // entry XML. Stored in SQLite so browsing does not regenerate it on
        // every application start. It is intentionally a catalog preview,
        // not a substitute for a full GPU scene render.
        std::string thumbnailFingerprint;
        std::vector<std::uint8_t> thumbnailRgba;
    };

    struct SearchFilter {
        std::string text;
        std::string tag;
        std::string category;
        std::string license;
        std::string provenance;
    };

    struct MaterialReport {
        // Each group contains two or more distinct material IDs whose full
        // serializable PBR values and texture references are identical.
        std::vector<std::vector<std::string>> duplicateMaterialGroups;
        std::vector<std::string> unusedMaterialIds;
    };

    struct AssetPackDependency {
        std::string importNamespace;
        std::string source;
        std::string contentHash;
        std::filesystem::path resolvedPath;
    };

    struct AssetPackResult {
        std::filesystem::path manifestPath;
        std::size_t entryCount{0};
        std::size_t dependencyCount{0};
    };

    ModelRegistry() = default;
    ~ModelRegistry();

    void open(const std::filesystem::path& dbPath);
    void close();
    bool isOpen() const;

    std::vector<Entry> search(const std::string& query);
    std::vector<Entry> search(const SearchFilter& filter);
    int64_t save(const Entry& e);
    void remove(int64_t id);

    // These work without SQLite as well: material inspection is pure over an
    // MC3 document, and a pack is an explicit local-directory export rather
    // than a registry/database operation.
    static MaterialReport inspectMaterials(const Mc3::Mc3Document& doc);
    static AssetPackResult exportAssetPack(
        const std::filesystem::path& destination,
        const std::vector<Entry>& entries,
        const std::vector<AssetPackDependency>& resolvedDependencies);

    // Serialize a definition from doc into a registry Entry (does NOT save to DB)
    Entry entryFromDefinition(const Mc3::Mc3Document& doc,
                              const std::string& defId,
                              const std::string& group,
                              const std::string& name,
                              const std::string& variant,
                              const std::string& tags,
                              const std::string& description = {},
                              const std::string& source = {}) const;

    // Parse entry XML, merge definition (+materials) into doc, return inserted def id
    std::string insertIntoScene(Mc3::Mc3Document& doc, const Entry& e) const;

    static std::filesystem::path defaultPath();

private:
    static void ensureThumbnail(Entry& entry);

#ifdef MESHCRAFT_HAS_SQLITE3
    sqlite3* db_{nullptr};
    void createSchema();
    void updateThumbnailCache(const Entry& entry);
#endif
};

} // namespace MeshCraft
