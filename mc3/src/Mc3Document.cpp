#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "Mc3XmlParser.hpp"
#include "Mc3XmlWriter.hpp"

namespace MeshCraft::Mc3 {

Mc3Document Mc3Document::loadFromFile(const std::filesystem::path& path) {
    Internal::Mc3XmlParser parser;
    return parser.parse(path);
}

void Mc3Document::saveToFile(const std::filesystem::path& path) const {
    Internal::Mc3XmlWriter writer;
    writer.write(*this, path);
}

} // namespace MeshCraft::Mc3
