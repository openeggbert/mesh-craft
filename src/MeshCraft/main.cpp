#include "MeshCraft/GraphicsBackendCheck.hpp"
#include "MeshCraft/MeshCraftApplication.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

// AUD-039b (Gate C): the editor's ImGui UI is hard-wired to OpenGL/GLES3 and
// only renders under the EASYGL backend (see MeshCraftApplication.cpp's
// ImGui_ImplOpenGL3_Init / ImGui_ImplSDL3_InitForOpenGL). Configuring with a
// different CNA backend (SDL_RENDERER/BGFX/VULKAN) still compiles this
// executable -- restructuring the CMake target to skip building it entirely
// under those backends would touch several hundred lines of stabilized build
// configuration for high regression risk. This runtime check is the
// surgical alternative: refuse to proceed BEFORE any window/GL
// initialization, so an unsupported configuration is rejected clearly (Gate
// C) rather than silently launching a window whose UI never draws.
// MESH_CRAFT_ALLOW_UNSUPPORTED_BACKEND=1 is the explicit, documented escape
// hatch for deliberate CNA-only experimentation (mirrors the CMake-side
// MESH_CRAFT_SILENCE_BACKEND_WARNING flag). The pure decision logic lives in
// MeshCraft::isBackendSupportedAlg() (GraphicsBackendCheck.hpp) so it is
// headlessly unit-testable without a full alternate-backend rebuild.
static bool checkBackendSupported() {
#ifdef MESH_CRAFT_GRAPHICS_BACKEND_STR
    static constexpr const char* kBackend = MESH_CRAFT_GRAPHICS_BACKEND_STR;
    bool allowOverride = std::getenv("MESH_CRAFT_ALLOW_UNSUPPORTED_BACKEND") != nullptr;
    if (MeshCraft::isBackendSupportedAlg(kBackend, allowOverride)) {
        if (std::string(kBackend) != "EASYGL") {
            std::cerr << "[MeshCraft] Warning: MESH_CRAFT_GRAPHICS_BACKEND_STR='" << kBackend
                      << "' is not EASYGL; the editor UI will not render, but "
                         "MESH_CRAFT_ALLOW_UNSUPPORTED_BACKEND is set -- continuing anyway.\n";
        }
        return true;
    }
    std::cerr << "[MeshCraft] Error: this build was configured with "
                 "MESH_CRAFT_GRAPHICS_BACKEND='" << kBackend << "', but the MeshCraft editor "
                 "GUI only renders under EASYGL (its ImGui UI is hard-wired to OpenGL/GLES3). "
                 "Reconfigure with -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL, or set "
                 "MESH_CRAFT_ALLOW_UNSUPPORTED_BACKEND=1 to launch anyway for CNA-only "
                 "experimentation (the UI will not draw).\n";
    return false;
#else
    return true;
#endif
}

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
    if (!checkBackendSupported()) return 1;

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
