#include "MeshCraft/ModelRegistry.hpp"

#ifdef MESHCRAFT_HAS_SQLITE3
#include <sqlite3.h>
#endif

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

static std::atomic<int> gRegTmpCounter{0};

namespace MeshCraft {

// ---------------------------------------------------------------------------
// defaultPath
// ---------------------------------------------------------------------------

std::filesystem::path ModelRegistry::defaultPath() {
#ifdef _WIN32
    const char* h = std::getenv("USERPROFILE");
    if (!h) h = ".";
#else
    const char* h = std::getenv("HOME");
    if (!h) h = ".";
#endif
    return std::filesystem::path(h) / ".meshcraft" / "modelregistry.sqlite3";
}

// ---------------------------------------------------------------------------
// Stubs when SQLite3 is absent (Emscripten / Android builds)
// ---------------------------------------------------------------------------
#ifndef MESHCRAFT_HAS_SQLITE3

ModelRegistry::~ModelRegistry() = default;
void ModelRegistry::open(const std::filesystem::path&) {}
void ModelRegistry::close() {}
bool ModelRegistry::isOpen() const { return false; }
std::vector<ModelRegistry::Entry> ModelRegistry::search(const std::string&) const { return {}; }
int64_t ModelRegistry::save(const Entry&) { return -1; }
void ModelRegistry::remove(int64_t) {}

ModelRegistry::Entry ModelRegistry::entryFromDefinition(
    const Mc3::Mc3Document&, const std::string&,
    const std::string&, const std::string&,
    const std::string&, const std::string&,
    const std::string&, const std::string&) const { return {}; }

std::string ModelRegistry::insertIntoScene(Mc3::Mc3Document&, const Entry&) const { return {}; }

#else // MESHCRAFT_HAS_SQLITE3

// ---------------------------------------------------------------------------
// Collect all material IDs referenced within an object tree
// ---------------------------------------------------------------------------

static void collectMaterials(const Mc3::Mc3Object* obj, std::set<std::string>& out) {
    if (!obj) return;
    if (!obj->material.empty()) out.insert(obj->material);
    if (!obj->materialOverride.empty()) out.insert(obj->materialOverride);
    for (const auto& child : obj->children)
        collectMaterials(child.get(), out);
}

static std::string readFile(const std::filesystem::path& p) {
    std::ifstream f(p);
    if (!f) throw std::runtime_error("Cannot read temp file: " + p.string());
    return std::string(std::istreambuf_iterator<char>(f), {});
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

ModelRegistry::~ModelRegistry() { close(); }

void ModelRegistry::open(const std::filesystem::path& dbPath) {
    close();
    std::error_code ec;
    std::filesystem::create_directories(dbPath.parent_path(), ec);
    int rc = sqlite3_open(dbPath.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "unknown";
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("ModelRegistry open failed: " + err);
    }
    createSchema();
}

void ModelRegistry::close() {
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

bool ModelRegistry::isOpen() const { return db_ != nullptr; }

void ModelRegistry::createSchema() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS models ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  grp         TEXT NOT NULL DEFAULT '',"
        "  name        TEXT NOT NULL,"
        "  variant     TEXT NOT NULL DEFAULT '',"
        "  xml         TEXT NOT NULL,"
        "  tags        TEXT NOT NULL DEFAULT '',"
        "  description TEXT NOT NULL DEFAULT '',"
        "  source      TEXT NOT NULL DEFAULT '',"
        "  created     INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_models_name ON models(name);";
    char* errmsg = nullptr;
    sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
    if (errmsg) {
        std::string err = errmsg;
        sqlite3_free(errmsg);
        throw std::runtime_error("ModelRegistry schema error: " + err);
    }
    // Migrate older DBs that lack description / source columns
    sqlite3_exec(db_,
        "ALTER TABLE models ADD COLUMN description TEXT NOT NULL DEFAULT '';",
        nullptr, nullptr, nullptr);
    sqlite3_exec(db_,
        "ALTER TABLE models ADD COLUMN source TEXT NOT NULL DEFAULT '';",
        nullptr, nullptr, nullptr);
    // Errors above are silently ignored — ALTER TABLE fails harmlessly if column exists.
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

std::vector<ModelRegistry::Entry> ModelRegistry::search(const std::string& query) const {
    if (!db_) return {};
    std::vector<Entry> results;

    const char* sql = query.empty()
        ? "SELECT id,grp,name,variant,xml,tags,description,source FROM models ORDER BY grp,name,variant;"
        : "SELECT id,grp,name,variant,xml,tags,description,source FROM models "
          "WHERE lower(name)        LIKE lower(?1) "
          "   OR lower(grp)         LIKE lower(?1) "
          "   OR lower(tags)        LIKE lower(?1) "
          "   OR lower(description) LIKE lower(?1) "
          "   OR lower(source)      LIKE lower(?1) "
          "ORDER BY grp,name,variant;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK || !stmt)
        return {};

    if (!query.empty()) {
        std::string pat = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, pat.c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Entry e;
        e.id = sqlite3_column_int64(stmt, 0);
        auto col = [&](int i) -> std::string {
            const auto* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            return t ? t : "";
        };
        e.group       = col(1);
        e.name        = col(2);
        e.variant     = col(3);
        e.xml         = col(4);
        e.tags        = col(5);
        e.description = col(6);
        e.source      = col(7);
        results.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return results;
}

// ---------------------------------------------------------------------------
// Save (insert or update)
// ---------------------------------------------------------------------------

int64_t ModelRegistry::save(const Entry& e) {
    if (!db_) return -1;
    sqlite3_stmt* stmt = nullptr;
    if (e.id > 0) {
        sqlite3_prepare_v2(db_,
            "UPDATE models SET grp=?1,name=?2,variant=?3,xml=?4,tags=?5,"
            "description=?6,source=?7 WHERE id=?8;",
            -1, &stmt, nullptr);
        sqlite3_bind_text (stmt, 1, e.group.c_str(),       -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 2, e.name.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 3, e.variant.c_str(),     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 4, e.xml.c_str(),         -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 5, e.tags.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 6, e.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 7, e.source.c_str(),      -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 8, e.id);
    } else {
        sqlite3_prepare_v2(db_,
            "INSERT INTO models(grp,name,variant,xml,tags,description,source)"
            " VALUES(?1,?2,?3,?4,?5,?6,?7);",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, e.group.c_str(),       -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, e.name.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, e.variant.c_str(),     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, e.xml.c_str(),         -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, e.tags.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, e.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, e.source.c_str(),      -1, SQLITE_TRANSIENT);
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return e.id > 0 ? e.id : sqlite3_last_insert_rowid(db_);
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

void ModelRegistry::remove(int64_t id) {
    if (!db_) return;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM models WHERE id=?1;", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ---------------------------------------------------------------------------
// entryFromDefinition
// ---------------------------------------------------------------------------

ModelRegistry::Entry ModelRegistry::entryFromDefinition(
    const Mc3::Mc3Document& doc,
    const std::string& defId,
    const std::string& group,
    const std::string& name,
    const std::string& variant,
    const std::string& tags,
    const std::string& description,
    const std::string& source) const
{
    auto it = doc.definitions.find(defId);
    if (it == doc.definitions.end())
        throw std::runtime_error("Definition not found: " + defId);

    // Build a minimal Mc3Document containing the definition + referenced materials
    Mc3::Mc3Document tmp;
    tmp.version = doc.version;
    tmp.model   = "registry_entry";
    tmp.definitions[defId] = it->second;

    // Collect and include all materials referenced by this definition
    std::set<std::string> matIds;
    collectMaterials(it->second.get(), matIds);
    for (const auto& mid : matIds) {
        auto mit = doc.materials.find(mid);
        if (mit != doc.materials.end())
            tmp.materials[mid] = mit->second;
    }
    // Also include textures referenced by those materials
    for (const auto& [mid, mat] : tmp.materials) {
        for (const auto& texPath : {
                mat.baseColorTexture, mat.normalTexture, mat.emissiveTexture,
                mat.metallicRoughnessTexture, mat.occlusionTexture }) {
            if (!texPath.empty()) {
                auto tit = doc.textures.find(texPath);
                if (tit != doc.textures.end())
                    tmp.textures[texPath] = tit->second;
            }
        }
    }

    auto tmpPath = std::filesystem::temp_directory_path() /
                   ("mc_reg_save_" + std::to_string(gRegTmpCounter++) + ".mc3.xml");
    tmp.saveToFile(tmpPath);
    std::string xml = readFile(tmpPath);
    std::error_code ec;
    std::filesystem::remove(tmpPath, ec);

    Entry e;
    e.group       = group;
    e.name        = name;
    e.variant     = variant;
    e.xml         = std::move(xml);
    e.tags        = tags;
    e.description = description;
    e.source      = source;
    return e;
}

// ---------------------------------------------------------------------------
// insertIntoScene
// ---------------------------------------------------------------------------

std::string ModelRegistry::insertIntoScene(Mc3::Mc3Document& doc, const Entry& e) const {
    auto tmpPath = std::filesystem::temp_directory_path() /
                   ("mc_reg_insert_" + std::to_string(gRegTmpCounter++) + ".mc3.xml");
    {
        std::ofstream f(tmpPath);
        if (!f) throw std::runtime_error("Cannot write temp file");
        f << e.xml;
    }

    Mc3::Mc3Document tmp = Mc3::Mc3Document::loadFromFile(tmpPath);
    std::error_code ec;
    std::filesystem::remove(tmpPath, ec);

    if (tmp.definitions.empty())
        throw std::runtime_error("Registry entry has no definitions");

    auto& [defId, defObj] = *tmp.definitions.begin();

    // Ensure unique definition id in the scene
    std::string finalId = defId;
    int suffix = 1;
    while (doc.definitions.count(finalId))
        finalId = defId + "_" + std::to_string(suffix++);

    doc.definitions[finalId] = std::move(defObj);

    // Merge materials (don't overwrite existing)
    for (auto& [matId, mat] : tmp.materials)
        if (!doc.materials.count(matId))
            doc.materials[matId] = mat;

    // Merge textures (don't overwrite existing)
    for (auto& [texId, tex] : tmp.textures)
        if (!doc.textures.count(texId))
            doc.textures[texId] = tex;

    return finalId;
}

#endif // MESHCRAFT_HAS_SQLITE3

} // namespace MeshCraft
