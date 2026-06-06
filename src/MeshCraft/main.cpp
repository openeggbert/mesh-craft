#include "MeshCraft/MeshCraftApplication.hpp"
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string filePath;
    std::string screenshotPath;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--screenshot" && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else {
            filePath = arg;
        }
    }

    if (!filePath.empty() && !screenshotPath.empty()) {
        MeshCraft::MeshCraftApplication app(std::filesystem::path(filePath), screenshotPath);
        app.Run();
    } else if (!filePath.empty()) {
        if (!std::filesystem::exists(filePath)) {
            std::cerr << "[MeshCraft] File not found: " << filePath << "\n";
            std::cerr << "[MeshCraft] Starting with empty scene.\n";
            MeshCraft::MeshCraftApplication app;
            app.Run();
        } else {
            std::filesystem::path p = filePath;
            MeshCraft::MeshCraftApplication app(p);
            app.Run();
        }
    } else {
        MeshCraft::MeshCraftApplication app;
        app.Run();
    }
    return 0;
}
