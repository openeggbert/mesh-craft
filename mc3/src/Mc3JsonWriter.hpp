#pragma once
#include <filesystem>
#include <string>

namespace MeshCraft::Mc3 { class Mc3Document; }

namespace MeshCraft::Mc3::Internal {

// R109 -- writes the semantic mc3.json representation of a Mc3Document.
// Unlike Mc3XmlWriter this is NOT a mechanical mirror of the XML attribute
// layout: vectors are JSON arrays, nested structures (transform, primitive,
// extrude, uv_mapping, ...) are nested JSON objects, and every field name is
// camelCase, matching mesh_world_revival.md §4.2's example document.
class Mc3JsonWriter {
public:
    // Renders `doc` to a JSON string (pretty-printed, indent=2).
    std::string toString(const Mc3Document& doc);

    // Writes `doc` to `path` as mc3.json. Uses the same write-to-temp-then-
    // rename pattern as Mc3XmlWriter::write() for crash safety.
    void write(const Mc3Document& doc, const std::filesystem::path& path);
};

} // namespace MeshCraft::Mc3::Internal
