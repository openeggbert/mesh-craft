#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

static void printUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <input.mc3.xml>  <output.mcb>   # XML  -> MCB\n"
              << "  " << prog << " <input.mcb>       <output.mc3.xml> # MCB  -> XML\n";
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }

    std::filesystem::path src{argv[1]};
    std::filesystem::path dst{argv[2]};

    const std::string srcExt = src.extension().string();
    const std::string dstExt = dst.extension().string();

    try {
        if (srcExt == ".xml" && dstExt == ".mcb") {
            // mc3.xml → mcb
            auto doc = MeshCraft::Mc3::Mc3Document::loadFromFile(src);
            MeshCraft::Mcb::saveToFile(doc, dst);
            std::cout << "Wrote " << dst << "\n";
        } else if (srcExt == ".mcb" && (dstExt == ".xml" || dst.string().ends_with(".mc3.xml"))) {
            // mcb → mc3.xml
            auto doc = MeshCraft::Mcb::loadFromFile(src);
            doc.saveToFile(dst);
            std::cout << "Wrote " << dst << "\n";
        } else {
            std::cerr << "Cannot determine conversion direction from extensions.\n";
            printUsage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
