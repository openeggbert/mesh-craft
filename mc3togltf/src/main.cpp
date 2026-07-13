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
using MeshCraft::Mc3::Mc3ValidationSeverity;

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <input.mc3.xml> [output.gltf|output.glb]\n"
              << "\n"
              << "  If the output path is omitted, the input filename is used\n"
              << "  with its extension replaced by .gltf\n"
              << "\n"
              << "  Output extension must be .gltf or .glb (case-insensitive):\n"
              << "    .gltf  → JSON glTF 2.0 (external buffer .bin)\n"
              << "    .glb   → Binary GLB 2.0 (self-contained)\n"
              << "\n"
              << "Options:\n"
              << "  --allow-approximate-csg\n"
              << "    By default, CSG nodes (union/difference/intersection) are evaluated\n"
              << "    using the Manifold library and exported as a single merged mesh.\n"
              << "    This flag disables Manifold evaluation and exports CSG children as\n"
              << "    separate meshes instead — geometrically incorrect and intended only\n"
              << "    as a debug fallback.\n"
              << "\n"
              << "  --allow-external-resources\n"
              << "    By default, texture/mesh file paths that are absolute or escape the\n"
              << "    input document's directory are rejected, so an untrusted .mc3 cannot\n"
              << "    read arbitrary local files into the output. Pass this flag for trusted\n"
              << "    scenes that legitimately reference files outside their own directory.\n"
              << "\n"
              << "  --stats\n"
              << "    Print export statistics after writing the output file.\n";
}


int main(int argc, char* argv[]) {
    bool allowApproxCSG = false;
    bool showStats      = false;
    bool allowExternalResources = false;

    // Collect non-flag arguments
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (a == "--allow-approximate-csg") allowApproxCSG = true;
        else if (a == "--allow-external-resources") allowExternalResources = true;
        else if (a == "--stats")            showStats = true;
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
        exporter.allowExternalResources = allowExternalResources;
        exporter.exportDocument(doc, outputPath, fmt);

        std::cout << "Written: " << outputPath << '\n';

        if (showStats) {
            const auto& s = exporter.stats;
            std::cout << "Export statistics:\n"
                      << "  Objects processed: " << s.objectsProcessed   << "\n"
                      << "  glTF nodes:        " << s.gltfNodes           << "\n"
                      << "  Unique meshes:     " << s.uniqueMeshes        << "\n"
                      << "  Materials:         " << s.materialCount       << "\n"
                      << "  Reused mesh refs:  " << s.reusedMeshRefs      << "\n"
                      << "  Total vertices:    " << s.totalVertices        << "\n"
                      << "  Total triangles:   " << s.totalTriangles       << "\n"
                      << "  OBJ files loaded:  " << s.objMeshesLoaded     << "\n"
                      << "  CSG evaluations:   " << s.csgMeshesEvaluated  << "\n"
                      << "  Warnings:          " << s.warnings             << "\n";

            // SYS-W1-01: pre-export validation findings (documents that
            // reached export without a validating load -- see
            // GltfExporter::validation's doc comment).
            const auto& v = exporter.validation;
            if (!v.empty()) {
                std::cout << "Pre-export validation (" << v.warningCount() << " warning(s), "
                          << v.errorCount() << " error(s)):\n";
                for (const auto& entry : v.entries) {
                    std::cout << "  ["
                              << (entry.severity == Mc3ValidationSeverity::Error ? "error" : "warning")
                              << "] " << (entry.objectId.empty() ? "(document)" : entry.objectId);
                    if (!entry.field.empty()) std::cout << "." << entry.field;
                    std::cout << ": " << entry.message;
                    if (!entry.suggestedRepair.empty()) std::cout << " (" << entry.suggestedRepair << ")";
                    std::cout << "\n";
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
