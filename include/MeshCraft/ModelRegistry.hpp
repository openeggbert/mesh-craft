#pragma once

#include <MeshCraft/Mc3/Mc3Document.hpp>
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
    struct Entry {
        int64_t     id{0};
        std::string group;
        std::string name;
        std::string variant;
        std::string xml;         // serialized <mc3> fragment: definitions + referenced materials
        std::string tags;
        std::string description;
        std::string source;      // e.g. "handmade", "ai_generated", "imported"
    };

    ModelRegistry() = default;
    ~ModelRegistry();

    void open(const std::filesystem::path& dbPath);
    void close();
    bool isOpen() const;

    std::vector<Entry> search(const std::string& query) const;
    int64_t save(const Entry& e);
    void remove(int64_t id);

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
#ifdef MESHCRAFT_HAS_SQLITE3
    sqlite3* db_{nullptr};
    void createSchema();
#endif
};

} // namespace MeshCraft
