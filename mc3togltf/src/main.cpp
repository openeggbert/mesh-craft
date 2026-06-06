#include "GltfExporter.hpp"
#include "Mc3XmlParser.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace mc3togltf;

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <input.mc3.xml> [output.gltf|output.glb]\n"
              << "\n"
              << "  If the output path is omitted, the input filename is used\n"
              << "  with its extension replaced by .gltf\n"
              << "\n"
              << "  Output format is determined by the output file extension:\n"
              << "    .gltf  → JSON glTF 2.0 (external buffer .bin)\n"
              << "    .glb   → Binary GLB 2.0 (self-contained)\n";
}

static OutputFormat formatFromPath(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(c));
    if (ext == ".glb") return OutputFormat::GLB;
    return OutputFormat::GLTF;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    fs::path inputPath  = argv[1];
    fs::path outputPath;

    if (argc >= 3) {
        outputPath = argv[2];
    } else {
        outputPath = inputPath;
        // strip .xml, then replace/append .gltf
        if (outputPath.extension() == ".xml") {
            outputPath = outputPath.stem(); // removes .xml  (e.g. foo.mc3)
        }
        outputPath.replace_extension(".gltf");
    }

    if (!fs::exists(inputPath)) {
        std::cerr << "Error: input file not found: " << inputPath << '\n';
        return 1;
    }

    try {
        Mc3XmlParser parser;
        auto doc = parser.parse(inputPath);

        OutputFormat fmt = formatFromPath(outputPath);

        GltfExporter exporter;
        exporter.exportDocument(doc, outputPath, fmt);

        std::cout << "Written: " << outputPath << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
