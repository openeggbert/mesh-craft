#include "MeshCraft/AiAssistant.hpp"
#include "MeshCraft/GraphicsBackendCheck.hpp"
#include "MeshCraft/MeshCraftApplication.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

// The ImGui draw lists are rendered through CNA, but alternate backends remain
// gated until SYS-W8-05 completes a real editor screenshot qualification.
static bool checkBackendSupported() {
#ifdef MESH_CRAFT_GRAPHICS_BACKEND_STR
    static constexpr const char* kBackend = MESH_CRAFT_GRAPHICS_BACKEND_STR;
    if (MeshCraft::isBackendSupportedAlg(kBackend, false)) return true;
    std::cerr << "[MeshCraft] Error: this build was configured with "
                 "MESH_CRAFT_GRAPHICS_BACKEND='" << kBackend << "'. The CNA ImGui renderer "
                 "has no OpenGL dependency, but this backend has not yet passed MeshCraft's "
                 "real editor screenshot qualification.\n";
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
        "  " << prog << " scene.mc3.xml --benchmark\n"
        "\n"
        "Options:\n"
        "  --screenshot <path>   Render scene to PNG and exit (headless)\n"
        "  --export <path>       Export scene to .glb/.gltf and exit (headless)\n"
        "  --benchmark           Time startup/traversal/picking/undo-snapshot/\n"
        "                        animation-eval/registry/first-vs-warm-frame\n"
        "                        (SYS-W12-02), print to stdout, and exit (headless)\n"
        "  --version             Print version and exit\n"
        "  --help                Show this help\n";
}

// AUD-014: declared first (so, per C++ local-destruction-is-reverse-of-
// construction, it is destroyed LAST -- after `app` and everything else in
// main(), right before main() actually returns, on every return path
// below). Its destructor bounded-waits for any AiAssistant::sendAsync()
// worker thread still in flight, so the process doesn't begin static/
// OpenSSL teardown while that thread is still executing httplib/OpenSSL
// code. 5s is enough for a request that's moments from finishing; a
// genuinely hung/slow request is still abandoned after the timeout,
// exactly as before this fix -- this only helps the near-finished case,
// it does not turn sendAsync() into a blocking call in the common case
// (no request in flight -> waitForAllInFlight returns immediately).
struct AiShutdownWaiter {
    ~AiShutdownWaiter() {
        MeshCraft::AiAssistant::waitForAllInFlight(std::chrono::milliseconds(5000));
    }
};

int main(int argc, char* argv[]) {
    AiShutdownWaiter aiShutdownWaiter;
    if (!checkBackendSupported()) return 1;

    std::string filePath;
    std::string screenshotPath;
    std::string exportPath;
    bool benchmarkMode = false;

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
        } else if (arg == "--benchmark") {
            benchmarkMode = true;
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "[MeshCraft] Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            filePath = arg;
        }
    }

    // Screenshot/export/benchmark modes require an existing scene file
    if ((!screenshotPath.empty() || !exportPath.empty() || benchmarkMode) && filePath.empty()) {
        std::cerr << "[MeshCraft] --screenshot/--export/--benchmark require a scene file argument.\n";
        printUsage(argv[0]);
        return 1;
    }
    if ((!screenshotPath.empty() || !exportPath.empty() || benchmarkMode) && !std::filesystem::exists(filePath)) {
        std::cerr << "[MeshCraft] File not found: " << filePath << "\n";
        return 1;
    }
    if (benchmarkMode && (!screenshotPath.empty() || !exportPath.empty())) {
        std::cerr << "[MeshCraft] --benchmark cannot be combined with --screenshot/--export.\n";
        return 1;
    }

    if (benchmarkMode) {
        MeshCraft::MeshCraftApplication app(std::filesystem::path(filePath), /*benchmarkMode=*/true);
        app.Run();
    } else if (!filePath.empty() && (!screenshotPath.empty() || !exportPath.empty())) {
        MeshCraft::MeshCraftApplication app(std::filesystem::path(filePath), screenshotPath, exportPath);
        app.Run();
        if ((!exportPath.empty() && app.exportFailed()) ||
            (!screenshotPath.empty() && app.screenshotFailed()))
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
