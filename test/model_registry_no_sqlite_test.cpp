#include "MeshCraft/ModelRegistry.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

using namespace MeshCraft;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void pass(const std::string& message) { std::cout << "PASS: " << message << "\n"; }
static void fail(const std::string& message) { std::cerr << "FAIL: " << message << "\n"; ++failures; }
#define CHECK(condition, message) do { if (condition) pass(message); else fail(message); } while (0)

static std::string validEntryXml(const std::filesystem::path& root) {
    Mc3Document doc;
    doc.model = "No SQLite fixture";
    auto box = std::make_shared<Mc3Object>();
    box->id = "box";
    box->type = ObjectType::Box;
    box->primitive = Mc3Primitive{};
    doc.objects.push_back(box);
    const auto path = root / "entry.mc3.xml";
    doc.saveToFile(path);
    std::ifstream input(path, std::ios::binary);
    const std::string xml((std::istreambuf_iterator<char>(input)), {});
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return xml;
}

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "meshcraft_registry_no_sqlite";
    fs::remove_all(root);
    fs::create_directories(root);

    ModelRegistry registry;
    registry.open(root / "ignored.sqlite3");
    CHECK(!registry.isOpen(), "no-SQLite: registry remains unavailable after open");
    CHECK(registry.search("anything").empty(), "no-SQLite: text search is a harmless empty result");
    ModelRegistry::SearchFilter filter;
    filter.category = "furniture";
    CHECK(registry.search(filter).empty(), "no-SQLite: metadata search is a harmless empty result");
    ModelRegistry::Entry unavailable;
    unavailable.name = "unavailable";
    unavailable.xml = "<mc3/>";
    CHECK(registry.save(unavailable) == -1, "no-SQLite: save returns its documented unavailable result");
    registry.remove(42);
    CHECK(true, "no-SQLite: remove is harmless");

    ModelRegistry::Entry entry;
    entry.group = "Fixtures";
    entry.name = "Local box";
    entry.xml = validEntryXml(root);
    const auto pack = root / "local-pack";
    try {
        const auto result = ModelRegistry::exportAssetPack(pack, {entry}, {});
        CHECK(result.entryCount == 1 && result.dependencyCount == 0,
              "no-SQLite: local pack export remains available without a database");
        CHECK(fs::is_regular_file(result.manifestPath), "no-SQLite: local pack manifest exists");
        CHECK(fs::is_regular_file(pack / "entries" / "1-Local_box.mc3.xml"),
              "no-SQLite: entry XML is exported");
        CHECK(fs::is_regular_file(pack / "thumbnails" / "1-Local_box.rgba"),
              "no-SQLite: generated catalog preview is exported");
    } catch (const std::exception& ex) {
        fail(std::string("no-SQLite: local pack export threw: ") + ex.what());
    }

    fs::remove_all(root);
    std::cout << "\n" << (failures == 0 ? "All no-SQLite registry tests passed."
                                        : "FAILURES: " + std::to_string(failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
