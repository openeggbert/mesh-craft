#include "GltfExporter.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace mc3togltf;
using MeshCraft::Mc3::Mc3Document;

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--allow-approximate-csg] <input.mc3.xml> [output.gltf|output.glb]\n"
              << "\n"
              << "  If the output path is omitted, the input filename is used\n"
              << "  with its extension replaced by .gltf\n"
              << "\n"
              << "  Output extension must be .gltf or .glb (case-insensitive):\n"
              << "    .gltf  → JSON glTF 2.0 (external buffer .bin)\n"
              << "    .glb   → Binary GLB 2.0 (self-contained)\n"
              << "\n"
              << "  --allow-approximate-csg\n"
              << "    By default, CSG nodes (union/difference/intersection) are evaluated\n"
              << "    using the Manifold library and exported as a single merged mesh.\n"
              << "    This flag disables Manifold evaluation and exports CSG children as\n"
              << "    separate meshes instead — geometrically incorrect and intended only\n"
              << "    as a debug fallback.\n";
}


int main(int argc, char* argv[]) {
    bool allowApproxCSG = false;

    // Collect non-flag arguments
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--allow-approximate-csg") allowApproxCSG = true;
        else args.push_back(a);
    }

    if (args.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    fs::path inputPath  = args[0];
    fs::path outputPath;

    if (args.size() >= 2) {
        outputPath = args[1];
    } else {
        fs::path stem = inputPath.filename();
        if (stem.extension() == ".xml") stem = stem.stem();   // foo.mc3.xml → foo.mc3
        if (stem.extension() == ".mc3") stem = stem.stem();   // foo.mc3 → foo
        outputPath = inputPath.parent_path() / stem;
        outputPath.replace_extension(".gltf");
    }

    if (!fs::exists(inputPath)) {
        std::cerr << "Error: input file not found: " << inputPath << '\n';
        return 1;
    }

    // Validate output extension before loading the document (fast failure).
    OutputFormat fmt;
    try {
        fmt = outputFormatFromPath(outputPath);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    try {
        auto doc = Mc3Document::loadFromFile(inputPath);

        GltfExporter exporter;
        exporter.allowApproximateCSG = allowApproxCSG;
        exporter.exportDocument(doc, outputPath, fmt);

        std::cout << "Written: " << outputPath << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
