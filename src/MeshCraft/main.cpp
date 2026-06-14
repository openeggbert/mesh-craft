#include "MeshCraft/MeshCraftApplication.hpp"
#include <filesystem>
#include <iostream>
#include <string>

static void printUsage(const char* prog) {
    std::cout <<
        "Usage:\n"
        "  " << prog << " [scene.mc3.xml]\n"
        "  " << prog << " scene.mc3.xml --screenshot output.png\n"
        "\n"
        "Options:\n"
        "  --screenshot <path>   Render scene to PNG and exit (headless)\n"
        "  --help                Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string filePath;
    std::string screenshotPath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "[MeshCraft] Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            filePath = arg;
        }
    }

    // Screenshot mode requires an existing scene file
    if (!screenshotPath.empty() && filePath.empty()) {
        std::cerr << "[MeshCraft] --screenshot requires a scene file argument.\n";
        printUsage(argv[0]);
        return 1;
    }
    if (!screenshotPath.empty() && !std::filesystem::exists(filePath)) {
        std::cerr << "[MeshCraft] File not found: " << filePath << "\n";
        return 1;
    }

    if (!filePath.empty() && !screenshotPath.empty()) {
        MeshCraft::MeshCraftApplication app(std::filesystem::path(filePath), screenshotPath);
        app.Run();
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
