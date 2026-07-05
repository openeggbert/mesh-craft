#include "MeshCraft/MeshCraftApplication.hpp"
#include <filesystem>
#include <iostream>
#include <string>

static void printUsage(const char* prog) {
    std::cout <<
        "Usage:\n"
        "  " << prog << " [scene.mc3.xml]\n"
        "  " << prog << " scene.mc3.xml --screenshot output.png\n"
        "  " << prog << " scene.mc3.xml --export output.glb\n"
        "\n"
        "Options:\n"
        "  --screenshot <path>   Render scene to PNG and exit (headless)\n"
        "  --export <path>       Export scene to .glb/.gltf and exit (headless)\n"
        "  --version             Print version and exit\n"
        "  --help                Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string filePath;
    std::string screenshotPath;
    std::string exportPath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            std::cout << "MeshCraft " << MESHCRAFT_VERSION << "\n";
            return 0;
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else if (arg == "--export" && i + 1 < argc) {
            exportPath = argv[++i];
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "[MeshCraft] Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            filePath = arg;
        }
    }

    // Screenshot/export modes require an existing scene file
    if ((!screenshotPath.empty() || !exportPath.empty()) && filePath.empty()) {
        std::cerr << "[MeshCraft] --screenshot/--export require a scene file argument.\n";
        printUsage(argv[0]);
        return 1;
    }
    if ((!screenshotPath.empty() || !exportPath.empty()) && !std::filesystem::exists(filePath)) {
        std::cerr << "[MeshCraft] File not found: " << filePath << "\n";
        return 1;
    }

    if (!filePath.empty() && (!screenshotPath.empty() || !exportPath.empty())) {
        MeshCraft::MeshCraftApplication app(std::filesystem::path(filePath), screenshotPath, exportPath);
        app.Run();
        if (!exportPath.empty() && app.exportFailed())
            return 1;
    } else if (!filePath.empty()) {
        if (!std::filesystem::exists(filePath)) {
            std::cerr << "[MeshCraft] File not found: " << filePath << "\n";
            std::cerr << "[MeshCraft] Starting with empty scene.\n";
        }
        std::filesystem::path p{filePath};
        MeshCraft::MeshCraftApplication app(p);
        app.Run();
    } else {
        MeshCraft::MeshCraftApplication app;
        app.Run();
    }
    return 0;
}
