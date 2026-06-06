#include "MeshCraft/MeshCraftApplication.hpp"
#include <filesystem>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::filesystem::path filePath = argv[1];
        if (!std::filesystem::exists(filePath)) {
            std::cerr << "[MeshCraft] File not found: " << filePath << "\n";
            std::cerr << "[MeshCraft] Starting with empty scene.\n";
            MeshCraft::MeshCraftApplication app;
            app.Run();
        } else {
            MeshCraft::MeshCraftApplication app(filePath);
            app.Run();
        }
    } else {
        MeshCraft::MeshCraftApplication app;
        app.Run();
    }
    return 0;
}
