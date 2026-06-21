#include "GltfExporter.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
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
              << "    Export CSG nodes by exporting children separately (geometrically\n"
              << "    incorrect). Without this flag CSG nodes cause an error.\n";
}

static OutputFormat formatFromPath(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(c));
    if (ext == ".glb")  return OutputFormat::GLB;
    if (ext == ".gltf") return OutputFormat::GLTF;
    throw std::runtime_error(
        std::string("Unknown output extension '") + p.extension().string() +
        "'. Only .gltf and .glb are supported.");
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

    try {
        auto doc = Mc3Document::loadFromFile(inputPath);

        OutputFormat fmt = formatFromPath(outputPath);

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
