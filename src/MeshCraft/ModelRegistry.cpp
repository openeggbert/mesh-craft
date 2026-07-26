#include "MeshCraft/ModelRegistry.hpp"

#ifdef MESHCRAFT_HAS_SQLITE3
#include <sqlite3.h>
#endif

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/TempFile.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MeshCraft {

// ---------------------------------------------------------------------------
// defaultPath
// ---------------------------------------------------------------------------

std::filesystem::path ModelRegistry::defaultPath() {
    // STAB-0360: lets a user/script redirect the registry DB (e.g. to a
    // shared network location, or an isolated path for testing) without
    // needing a UI for it.
    if (const char* override = std::getenv("MESHCRAFT_REGISTRY_DB"); override && override[0])
        return std::filesystem::path(override);

#ifdef _WIN32
    const char* h = std::getenv("USERPROFILE");
    if (!h) h = ".";
#else
    const char* h = std::getenv("HOME");
    if (!h) h = ".";
#endif
    return std::filesystem::path(h) / ".meshcraft" / "modelregistry.sqlite3";
}

namespace {

std::uint64_t fnv1a64(std::string_view text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char c : text) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string fingerprintForXml(const std::string& xml) {
    std::ostringstream out;
    out << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << fnv1a64(xml);
    return out.str();
}

std::array<std::uint8_t, 3> thumbnailAccent(std::uint64_t hash) {
    // Bright, bounded RGB derived from different hash lanes. The floor keeps
    // catalog tiles legible on both the dark and light editor themes.
    return {
        static_cast<std::uint8_t>(80 + ((hash >>  0) & 0x7f)),
        static_cast<std::uint8_t>(80 + ((hash >> 13) & 0x7f)),
        static_cast<std::uint8_t>(80 + ((hash >> 26) & 0x7f)),
    };
}

std::vector<std::uint8_t> generateThumbnailRgba(const std::string& xml) {
    constexpr int width = ModelRegistry::kThumbnailWidth;
    constexpr int height = ModelRegistry::kThumbnailHeight;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4, 255);
    const std::uint64_t hash = fnv1a64(xml);
    const auto accent = thumbnailAccent(hash);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int dx = x - width / 2;
            const int dy = y - height / 2;
            const bool inDisc = dx * dx + dy * dy < 23 * 23;
            const unsigned bit = static_cast<unsigned>((x / 8 + (y / 8) * 5) & 31);
            const bool motif = ((hash >> bit) & 1ull) != 0;
            const std::size_t i = (static_cast<std::size_t>(y) * width + x) * 4;
            const std::uint8_t background = static_cast<std::uint8_t>(22 + ((x + y) & 7));
            if (inDisc && motif) {
                const std::uint8_t shade = static_cast<std::uint8_t>(
                    70 + ((x * 5 + y * 3 + static_cast<int>(hash & 31)) & 31));
                pixels[i + 0] = static_cast<std::uint8_t>(accent[0] * shade / 127);
                pixels[i + 1] = static_cast<std::uint8_t>(accent[1] * shade / 127);
                pixels[i + 2] = static_cast<std::uint8_t>(accent[2] * shade / 127);
            } else {
                pixels[i + 0] = background;
                pixels[i + 1] = static_cast<std::uint8_t>(background + 5);
                pixels[i + 2] = static_cast<std::uint8_t>(background + 11);
            }
        }
    }
    return pixels;
}

void collectMaterialReferences(const Mc3::Mc3Object* object,
                               std::set<const Mc3::Mc3Object*>& visited,
                               std::set<std::string>& references) {
    if (!object || !visited.insert(object).second) return;
    if (!object->material.empty()) references.insert(object->material);
    if (!object->materialOverride.empty()) references.insert(object->materialOverride);
    for (const auto& [stateName, state] : object->states) {
        (void)stateName;
        if (state.material && !state.material->empty()) references.insert(*state.material);
    }
    for (const auto& child : object->children)
        collectMaterialReferences(child.get(), visited, references);
}

std::string materialSignature(const Mc3::Mc3Material& material) {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (float value : material.baseColor) out << value << '|';
    out << material.baseColorTexture << '|' << material.normalTexture << '|'
        << material.emissiveTexture << '|' << material.metallicRoughnessTexture << '|'
        << material.occlusionTexture << '|' << material.roughness << '|' << material.metallic
        << '|' << material.normalScale << '|' << material.occlusionStrength << '|';
    for (float value : material.emissiveColor) out << value << '|';
    out << material.alphaMode << '|' << material.alphaCutoff << '|' << material.doubleSided;
    return out.str();
}

std::string packSafeStem(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_') out.push_back(static_cast<char>(c));
        else out.push_back('_');
    }
    if (out.empty()) out = "entry";
    return out;
}

void writeJsonString(std::ostream& out, const std::string& value) {
    out << '"';
    for (const unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(c) << std::dec << std::setfill(' ');
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    out << '"';
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write asset-pack file: " + path.string());
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) throw std::runtime_error("Cannot finish asset-pack file: " + path.string());
}

void writeBinaryFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write asset-pack thumbnail: " + path.string());
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("Cannot finish asset-pack thumbnail: " + path.string());
}

} // namespace

void ModelRegistry::ensureThumbnail(Entry& entry) {
    const std::string fingerprint = fingerprintForXml(entry.xml);
    const std::size_t expectedBytes = static_cast<std::size_t>(kThumbnailWidth) * kThumbnailHeight * 4;
    if (entry.thumbnailFingerprint == fingerprint && entry.thumbnailRgba.size() == expectedBytes) return;
    entry.thumbnailFingerprint = fingerprint;
    entry.thumbnailRgba = generateThumbnailRgba(entry.xml);
}

ModelRegistry::MaterialReport ModelRegistry::inspectMaterials(const Mc3::Mc3Document& doc) {
    MaterialReport report;
    std::set<const Mc3::Mc3Object*> visited;
    std::set<std::string> references;
    for (const auto& object : doc.objects)
        collectMaterialReferences(object.get(), visited, references);
    for (const auto& [definitionId, definition] : doc.definitions) {
        (void)definitionId;
        collectMaterialReferences(definition.get(), visited, references);
    }

    std::map<std::string, std::vector<std::string>> bySignature;
    for (const auto& [id, material] : doc.materials) {
        if (!references.count(id)) report.unusedMaterialIds.push_back(id);
        bySignature[materialSignature(material)].push_back(id);
    }
    for (auto& [signature, ids] : bySignature) {
        (void)signature;
        if (ids.size() > 1) report.duplicateMaterialGroups.push_back(std::move(ids));
    }
    return report;
}

ModelRegistry::AssetPackResult ModelRegistry::exportAssetPack(
    const std::filesystem::path& destination,
    const std::vector<Entry>& entries,
    const std::vector<AssetPackDependency>& resolvedDependencies)
{
    if (destination.empty()) throw std::invalid_argument("Asset-pack destination is empty");
    std::error_code ec;
    if (std::filesystem::exists(destination, ec)) {
        if (ec || !std::filesystem::is_directory(destination, ec) ||
            std::filesystem::directory_iterator(destination, ec) != std::filesystem::directory_iterator())
            throw std::runtime_error("Asset-pack destination must be a new or empty directory: " +
                                     destination.string());
    }

    struct PackEntry {
        Entry entry;
        std::string entryFile;
        std::string thumbnailFile;
        std::vector<Mc3::Mc3Import> imports;
    };
    std::vector<PackEntry> packEntries;
    packEntries.reserve(entries.size());
    for (std::size_t index = 0; index < entries.size(); ++index) {
        PackEntry prepared;
        prepared.entry = entries[index];
        ensureThumbnail(prepared.entry);
        try {
            const Mc3::Mc3Document document = Mc3::Mc3Document::loadFromString(
                prepared.entry.xml, {}, Mc3::Mc3LoadPolicy::untrusted());
            prepared.imports = document.imports;
        } catch (const std::exception& ex) {
            throw std::runtime_error("Cannot package registry entry '" + prepared.entry.name +
                                     "': " + ex.what());
        }
        const std::string prefix = std::to_string(prepared.entry.id > 0 ? prepared.entry.id :
                                                   static_cast<int64_t>(index + 1));
        const std::string stem = prefix + "-" + packSafeStem(prepared.entry.name);
        prepared.entryFile = "entries/" + stem + ".mc3.xml";
        prepared.thumbnailFile = "thumbnails/" + stem + ".rgba";
        packEntries.push_back(std::move(prepared));
    }

    std::map<std::string, AssetPackDependency> dependencyBySource;
    for (const auto& dependency : resolvedDependencies) {
        if (!dependency.source.empty()) dependencyBySource.emplace(dependency.source, dependency);
    }
    std::set<std::string> neededSources;
    for (const auto& entry : packEntries) {
        for (const auto& imported : entry.imports) {
            const auto it = dependencyBySource.find(imported.source);
            if (it == dependencyBySource.end() || it->second.resolvedPath.empty() ||
                !std::filesystem::is_regular_file(it->second.resolvedPath, ec))
                throw std::runtime_error("Asset-pack dependency is not resolved locally: " + imported.source);
            // A pinned import must never be bundled with a merely similarly
            // named resolved file. `contentHash` comes from the verified
            // import-health record, so matching it preserves MC3's pinning
            // contract in a portable pack.
            if (!imported.hash.empty() && imported.hash != it->second.contentHash)
                throw std::runtime_error("Asset-pack dependency hash does not match import '" +
                                         imported.source + "'");
            neededSources.insert(imported.source);
        }
    }

    std::vector<AssetPackDependency> usedDependencies;
    for (const auto& source : neededSources) {
        const auto it = dependencyBySource.find(source);
        if (it == dependencyBySource.end() || it->second.resolvedPath.empty() ||
            !std::filesystem::is_regular_file(it->second.resolvedPath, ec))
            throw std::runtime_error("Asset-pack dependency is not resolved locally: " + source);
        usedDependencies.push_back(it->second);
    }

    std::filesystem::create_directories(destination / "entries", ec);
    if (ec) throw std::runtime_error("Cannot create asset-pack entries directory: " + ec.message());
    std::filesystem::create_directories(destination / "thumbnails", ec);
    if (ec) throw std::runtime_error("Cannot create asset-pack thumbnails directory: " + ec.message());
    if (!usedDependencies.empty()) {
        std::filesystem::create_directories(destination / "libraries", ec);
        if (ec) throw std::runtime_error("Cannot create asset-pack libraries directory: " + ec.message());
    }

    for (const auto& entry : packEntries) {
        writeTextFile(destination / entry.entryFile, entry.entry.xml);
        writeBinaryFile(destination / entry.thumbnailFile, entry.entry.thumbnailRgba);
    }

    std::vector<std::string> dependencyFiles;
    dependencyFiles.reserve(usedDependencies.size());
    for (std::size_t index = 0; index < usedDependencies.size(); ++index) {
        const auto& dependency = usedDependencies[index];
        const std::string file = "libraries/" + std::to_string(index + 1) + "-" +
                                 packSafeStem(dependency.resolvedPath.filename().string());
        std::filesystem::copy_file(dependency.resolvedPath, destination / file,
                                   std::filesystem::copy_options::none, ec);
        if (ec) throw std::runtime_error("Cannot copy asset-pack dependency '" +
                                         dependency.resolvedPath.string() + "': " + ec.message());
        dependencyFiles.push_back(file);
    }

    const std::filesystem::path manifest = destination / "manifest.json";
    std::ofstream out(manifest, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write asset-pack manifest: " + manifest.string());
    out << "{\n  \"format\": \"meshcraft-asset-pack\",\n  \"version\": 1,\n  \"entries\": [\n";
    for (std::size_t index = 0; index < packEntries.size(); ++index) {
        const auto& entry = packEntries[index];
        out << "    {\"id\": " << entry.entry.id << ", \"name\": "; writeJsonString(out, entry.entry.name);
        out << ", \"group\": "; writeJsonString(out, entry.entry.group);
        out << ", \"category\": "; writeJsonString(out, entry.entry.category);
        out << ", \"license\": "; writeJsonString(out, entry.entry.license);
        out << ", \"provenance\": "; writeJsonString(out, entry.entry.provenance);
        out << ", \"entry_file\": "; writeJsonString(out, entry.entryFile);
        out << ", \"thumbnail\": {\"file\": "; writeJsonString(out, entry.thumbnailFile);
        out << ", \"width\": " << kThumbnailWidth << ", \"height\": " << kThumbnailHeight
            << ", \"fingerprint\": "; writeJsonString(out, entry.entry.thumbnailFingerprint);
        out << "}}" << (index + 1 == packEntries.size() ? "\n" : ",\n");
    }
    out << "  ],\n  \"libraries\": [\n";
    for (std::size_t index = 0; index < usedDependencies.size(); ++index) {
        const auto& dependency = usedDependencies[index];
        out << "    {\"namespace\": "; writeJsonString(out, dependency.importNamespace);
        out << ", \"source\": "; writeJsonString(out, dependency.source);
        out << ", \"content_hash\": "; writeJsonString(out, dependency.contentHash);
        out << ", \"file\": "; writeJsonString(out, dependencyFiles[index]);
        out << "}" << (index + 1 == usedDependencies.size() ? "\n" : ",\n");
    }
    out << "  ]\n}\n";
    if (!out) throw std::runtime_error("Cannot finish asset-pack manifest: " + manifest.string());
    return {manifest, packEntries.size(), usedDependencies.size()};
}

// ---------------------------------------------------------------------------
// Stubs when SQLite3 is absent (Emscripten / Android builds)
// ---------------------------------------------------------------------------
#ifndef MESHCRAFT_HAS_SQLITE3

ModelRegistry::~ModelRegistry() = default;
void ModelRegistry::open(const std::filesystem::path&) {}
void ModelRegistry::close() {}
bool ModelRegistry::isOpen() const { return false; }
std::vector<ModelRegistry::Entry> ModelRegistry::search(const std::string&) { return {}; }
std::vector<ModelRegistry::Entry> ModelRegistry::search(const SearchFilter&) { return {}; }
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

// Keep a registry entry self-describing without copying every library import
// from the source scene. An imported definition is denoted by `alias:id`.
// Collect aliases through the saved definition tree, including the definition
// id itself (a registry may store a re-exported imported definition).
static void collectImportNamespaces(const Mc3::Mc3Object* obj,
                                    std::set<std::string>& namespaces) {
    if (!obj) return;
    const auto colon = obj->definition.find(':');
    if (colon != std::string::npos && colon > 0)
        namespaces.insert(obj->definition.substr(0, colon));
    for (const auto& child : obj->children)
        collectImportNamespaces(child.get(), namespaces);
}

static void appendTag(std::string& target, const std::string& value) {
    if (value.empty()) return;
    if (!target.empty()) target.push_back(' ');
    target += value;
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
    try {
        createSchema();
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
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
        "  category    TEXT NOT NULL DEFAULT '',"
        "  license     TEXT NOT NULL DEFAULT '',"
        "  provenance  TEXT NOT NULL DEFAULT '',"
        "  thumbnail_fingerprint TEXT NOT NULL DEFAULT '',"
        "  thumbnail_rgba BLOB,"
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
    auto addColumn = [&](const char* sqlText) {
        char* alterError = nullptr;
        const int rc = sqlite3_exec(db_, sqlText, nullptr, nullptr, &alterError);
        if (rc == SQLITE_OK) return;
        const std::string error = alterError ? alterError : "unknown SQLite migration error";
        sqlite3_free(alterError);
        // SQLite has no ADD COLUMN IF NOT EXISTS. Reopening a current DB
        // legitimately reports this exact error; every other migration error
        // must remain visible instead of silently leaving a half-upgraded DB.
        if (error.find("duplicate column name") == std::string::npos)
            throw std::runtime_error("ModelRegistry migration error: " + error);
    };
    addColumn("ALTER TABLE models ADD COLUMN description TEXT NOT NULL DEFAULT '';");
    addColumn("ALTER TABLE models ADD COLUMN source TEXT NOT NULL DEFAULT '';");
    addColumn("ALTER TABLE models ADD COLUMN category TEXT NOT NULL DEFAULT '';");
    addColumn("ALTER TABLE models ADD COLUMN license TEXT NOT NULL DEFAULT '';");
    addColumn("ALTER TABLE models ADD COLUMN provenance TEXT NOT NULL DEFAULT '';");
    addColumn("ALTER TABLE models ADD COLUMN thumbnail_fingerprint TEXT NOT NULL DEFAULT '';");
    addColumn("ALTER TABLE models ADD COLUMN thumbnail_rgba BLOB;");
}

void ModelRegistry::updateThumbnailCache(const Entry& entry) {
    if (!db_ || entry.id <= 0) return;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
            "UPDATE models SET thumbnail_fingerprint=?1,thumbnail_rgba=?2 WHERE id=?3;",
            -1, &stmt, nullptr) != SQLITE_OK || !stmt)
        return;
    sqlite3_bind_text(stmt, 1, entry.thumbnailFingerprint.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, entry.thumbnailRgba.data(),
                      static_cast<int>(entry.thumbnailRgba.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, entry.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

std::vector<ModelRegistry::Entry> ModelRegistry::search(const std::string& query) {
    SearchFilter filter;
    filter.text = query;
    return search(filter);
}

std::vector<ModelRegistry::Entry> ModelRegistry::search(const SearchFilter& filter) {
    if (!db_) return {};
    std::vector<Entry> results;

    std::vector<std::string> predicates;
    std::vector<std::string> parameters;
    auto addLike = [&](const char* column, const std::string& value) {
        if (value.empty()) return;
        predicates.push_back(std::string("lower(") + column + ") LIKE lower(?" +
                             std::to_string(parameters.size() + 1) + ")");
        parameters.push_back("%" + value + "%");
    };
    if (!filter.text.empty()) {
        const std::string placeholder = "?" + std::to_string(parameters.size() + 1);
        predicates.push_back("(lower(name) LIKE lower(" + placeholder + ") OR "
                             "lower(grp) LIKE lower(" + placeholder + ") OR "
                             "lower(variant) LIKE lower(" + placeholder + ") OR "
                             "lower(tags) LIKE lower(" + placeholder + ") OR "
                             "lower(description) LIKE lower(" + placeholder + ") OR "
                             "lower(source) LIKE lower(" + placeholder + ") OR "
                             "lower(category) LIKE lower(" + placeholder + ") OR "
                             "lower(license) LIKE lower(" + placeholder + ") OR "
                             "lower(provenance) LIKE lower(" + placeholder + "))");
        parameters.push_back("%" + filter.text + "%");
    }
    addLike("tags", filter.tag);
    addLike("category", filter.category);
    addLike("license", filter.license);
    addLike("provenance", filter.provenance);

    std::string sql = "SELECT id,grp,name,variant,xml,tags,description,source,category,license,provenance,"
                      "thumbnail_fingerprint,thumbnail_rgba FROM models";
    if (!predicates.empty()) {
        sql += " WHERE ";
        for (std::size_t i = 0; i < predicates.size(); ++i) {
            if (i) sql += " AND ";
            sql += predicates[i];
        }
    }
    sql += " ORDER BY grp,name,variant;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK || !stmt)
        return {};

    for (std::size_t i = 0; i < parameters.size(); ++i)
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), parameters[i].c_str(), -1, SQLITE_TRANSIENT);

    std::vector<Entry> staleThumbnails;
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
        e.category    = col(8);
        e.license     = col(9);
        e.provenance  = col(10);
        e.thumbnailFingerprint = col(11);
        const auto* blob = static_cast<const std::uint8_t*>(sqlite3_column_blob(stmt, 12));
        const int blobBytes = sqlite3_column_bytes(stmt, 12);
        if (blob && blobBytes > 0)
            e.thumbnailRgba.assign(blob, blob + blobBytes);
        const std::string expectedFingerprint = fingerprintForXml(e.xml);
        const std::size_t expectedBytes = static_cast<std::size_t>(kThumbnailWidth) * kThumbnailHeight * 4;
        if (e.thumbnailFingerprint != expectedFingerprint || e.thumbnailRgba.size() != expectedBytes) {
            ensureThumbnail(e);
            staleThumbnails.push_back(e);
        }
        results.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    for (const auto& entry : staleThumbnails) updateThumbnailCache(entry);
    return results;
}

// ---------------------------------------------------------------------------
// Save (insert or update)
// ---------------------------------------------------------------------------

int64_t ModelRegistry::save(const Entry& e) {
    if (!db_) return -1;
    Entry prepared = e;
    ensureThumbnail(prepared);
    sqlite3_stmt* stmt = nullptr;
    if (prepared.id > 0) {
        if (sqlite3_prepare_v2(db_,
                "UPDATE models SET grp=?1,name=?2,variant=?3,xml=?4,tags=?5,"
                "description=?6,source=?7,category=?8,license=?9,provenance=?10,"
                "thumbnail_fingerprint=?11,thumbnail_rgba=?12 WHERE id=?13;",
                -1, &stmt, nullptr) != SQLITE_OK || !stmt)
            return -1;
        sqlite3_bind_text (stmt, 1, prepared.group.c_str(),       -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 2, prepared.name.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 3, prepared.variant.c_str(),     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 4, prepared.xml.c_str(),         -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 5, prepared.tags.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 6, prepared.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 7, prepared.source.c_str(),      -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 8, prepared.category.c_str(),    -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 9, prepared.license.c_str(),     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt,10, prepared.provenance.c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt,11, prepared.thumbnailFingerprint.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob (stmt,12, prepared.thumbnailRgba.data(),
                           static_cast<int>(prepared.thumbnailRgba.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 13, prepared.id);
    } else {
        if (sqlite3_prepare_v2(db_,
                "INSERT INTO models(grp,name,variant,xml,tags,description,source,category,license,provenance,"
                "thumbnail_fingerprint,thumbnail_rgba) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12);",
                -1, &stmt, nullptr) != SQLITE_OK || !stmt)
            return -1;
        sqlite3_bind_text(stmt, 1, prepared.group.c_str(),       -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, prepared.name.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, prepared.variant.c_str(),     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, prepared.xml.c_str(),         -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, prepared.tags.c_str(),        -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, prepared.description.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, prepared.source.c_str(),      -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, prepared.category.c_str(),    -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, prepared.license.c_str(),     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt,10, prepared.provenance.c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt,11, prepared.thumbnailFingerprint.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt,12, prepared.thumbnailRgba.data(),
                          static_cast<int>(prepared.thumbnailRgba.size()), SQLITE_TRANSIENT);
    }
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (!ok) return -1;
    return prepared.id > 0 ? prepared.id : sqlite3_last_insert_rowid(db_);
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

    std::set<std::string> importNamespaces;
    const auto definitionColon = defId.find(':');
    if (definitionColon != std::string::npos && definitionColon > 0)
        importNamespaces.insert(defId.substr(0, definitionColon));
    collectImportNamespaces(it->second.get(), importNamespaces);
    for (const auto& import : doc.imports) {
        if (importNamespaces.count(import.importNamespace))
            tmp.imports.push_back(import);
    }

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

    // Temp file is always removed, even if saveToFile()/readFile() throws
    // partway through (same leak class as STAB-0392's parseXmlAlg fix).
    auto tmpPath = uniqueTempPath("mc_reg_save", ".mc3.xml");
    std::string xml;
    std::error_code ec;
    try {
        tmp.saveToFile(tmpPath);
        xml = readFile(tmpPath);
        std::filesystem::remove(tmpPath, ec);
    } catch (...) {
        std::filesystem::remove(tmpPath, ec);
        throw;
    }

    Entry e;
    e.group       = group;
    e.name        = name;
    e.variant     = variant;
    e.xml         = std::move(xml);
    e.tags        = tags;
    e.description = description;
    e.source      = source;
    if (it->second->assetMetadata) {
        const auto& metadata = *it->second->assetMetadata;
        e.category = metadata.category;
        e.license = metadata.license;
        e.provenance = metadata.provenance;
        for (const auto& tag : metadata.semanticTags) appendTag(e.tags, tag);
        for (const auto& tag : metadata.styleTags) appendTag(e.tags, tag);
        for (const auto& tag : metadata.regionTags) appendTag(e.tags, tag);
        for (const auto& tag : metadata.periodTags) appendTag(e.tags, tag);
    }
    ensureThumbnail(e);
    return e;
}

// ---------------------------------------------------------------------------
// insertIntoScene
// ---------------------------------------------------------------------------

// STAB-0425: rewrite every material/texture reference in obj and its
// descendants (material, materialOverride, and each named state's material
// override) according to remap. Called after a name collision forces an
// imported material/texture onto a new suffixed id, so the imported object
// tree still points at its own (renamed) material/texture, not silently at
// whatever happened to already occupy that name in the target scene.
static void remapMaterialRefs(Mc3::Mc3Object& obj,
                               const std::map<std::string, std::string>& remap)
{
    auto apply = [&](std::string& id) {
        auto it = remap.find(id);
        if (it != remap.end()) id = it->second;
    };
    apply(obj.material);
    apply(obj.materialOverride);
    for (auto& [stateName, state] : obj.states)
        if (state.material) apply(*state.material);
    for (auto& child : obj.children)
        if (child) remapMaterialRefs(*child, remap);
}

std::string ModelRegistry::insertIntoScene(Mc3::Mc3Document& doc, const Entry& e) const {
    // Temp file is always removed, even if loadFromFile() throws on a
    // corrupted/malformed registry entry (same leak class as STAB-0392's
    // parseXmlAlg fix) — reproduced empirically: a malformed e.xml made
    // loadFromFile() throw before the removal line ran, leaking the file.
    auto tmpPath = uniqueTempPath("mc_reg_insert", ".mc3.xml");
    std::error_code ec;
    Mc3::Mc3Document tmp;
    try {
        {
            std::ofstream f(tmpPath);
            if (!f) throw std::runtime_error("Cannot write temp file");
            f << e.xml;
        }
        tmp = Mc3::Mc3Document::loadFromFile(tmpPath);
        std::filesystem::remove(tmpPath, ec);
    } catch (...) {
        std::filesystem::remove(tmpPath, ec);
        throw;
    }

    if (tmp.definitions.empty())
        throw std::runtime_error("Registry entry has no definitions");

    // Registry entries can reference versioned library definitions. Merge
    // only compatible import declarations before touching the target scene;
    // a conflicting alias would otherwise create a dangling or misleading
    // `alias:id` instance with no deterministic resolution policy.
    std::vector<Mc3::Mc3Import> importsToAdd;
    for (const auto& imported : tmp.imports) {
        const auto existing = std::find_if(doc.imports.begin(), doc.imports.end(),
            [&](const Mc3::Mc3Import& current) {
                return current.importNamespace == imported.importNamespace;
            });
        if (existing == doc.imports.end()) {
            importsToAdd.push_back(imported);
            continue;
        }
        if (existing->source != imported.source || existing->hash != imported.hash) {
            throw std::runtime_error("Registry entry import namespace '" +
                                     imported.importNamespace +
                                     "' conflicts with the current scene");
        }
    }

    auto& [defId, defObj] = *tmp.definitions.begin();

    // Ensure unique definition id in the scene
    std::string finalId = defId;
    int suffix = 1;
    while (doc.definitions.count(finalId))
        finalId = defId + "_" + std::to_string(suffix++);

    // STAB-0425: a texture/material name colliding with the scene's existing
    // one must NOT silently keep whichever value the scene already had —
    // give the imported entry's texture/material a suffixed id instead
    // (mirroring finalId above) and rewrite every reference to it, so the
    // inserted object always ends up with its own actual material/texture,
    // never an unrelated same-named one already in the scene.
    std::map<std::string, std::string> texRemap;
    for (auto& [texId, tex] : tmp.textures) {
        std::string finalTexId = texId;
        int texSuffix = 1;
        while (doc.textures.count(finalTexId))
            finalTexId = texId + "_" + std::to_string(texSuffix++);
        if (finalTexId != texId) texRemap[texId] = finalTexId;
        doc.textures[finalTexId] = tex;
    }

    auto remapTex = [&](std::string& texId) {
        auto it = texRemap.find(texId);
        if (it != texRemap.end()) texId = it->second;
    };

    std::map<std::string, std::string> matRemap;
    for (auto& [matId, mat] : tmp.materials) {
        remapTex(mat.baseColorTexture);
        remapTex(mat.normalTexture);
        remapTex(mat.metallicRoughnessTexture);
        remapTex(mat.occlusionTexture);
        remapTex(mat.emissiveTexture);

        std::string finalMatId = matId;
        int matSuffix = 1;
        while (doc.materials.count(finalMatId))
            finalMatId = matId + "_" + std::to_string(matSuffix++);
        if (finalMatId != matId) matRemap[matId] = finalMatId;
        doc.materials[finalMatId] = mat;
    }

    if (defObj) remapMaterialRefs(*defObj, matRemap);
    doc.definitions[finalId] = std::move(defObj);
    doc.imports.insert(doc.imports.end(), importsToAdd.begin(), importsToAdd.end());

    return finalId;
}

#endif // MESHCRAFT_HAS_SQLITE3

} // namespace MeshCraft
