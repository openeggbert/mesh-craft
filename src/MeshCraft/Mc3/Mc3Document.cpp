#include "MeshCraft/Mc3/Mc3Document.hpp"

#include <stdexcept>

namespace MeshCraft::Mc3 {

Mc3Document Mc3Document::loadFromFile(const std::filesystem::path& path) {
    // TODO: parse YAML from path using a YAML library (e.g. yaml-cpp)
    // Steps: parse top-level fields, load textures, create materials,
    //        resolve definitions and instances, build object hierarchy.
    throw std::runtime_error("Mc3Document::loadFromFile not yet implemented");
}

void Mc3Document::saveToFile(const std::filesystem::path& path) const {
    // TODO: serialise document back to YAML
    throw std::runtime_error("Mc3Document::saveToFile not yet implemented");
}

} // namespace MeshCraft::Mc3
