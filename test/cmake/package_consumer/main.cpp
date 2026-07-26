#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"

#include <sstream>

int main() {
    MeshCraft::Mc3::Mc3Document document;
    document.model = "package-consumer";

    std::ostringstream encoded(std::ios::binary);
    MeshCraft::Mcb::saveToBinary(document, encoded);
    if (encoded.str().empty()) {
        return 1;
    }

    std::istringstream input(encoded.str(), std::ios::binary);
    const auto decoded = MeshCraft::Mcb::loadFromBinary(input);
    return decoded.model == document.model ? 0 : 2;
}
