#include "MeshCraft/AiAssistant.hpp"
#include "MeshCraft/GraphicsBackendCheck.hpp"
#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

// The ImGui draw lists are rendered through CNA. Vulkan is enabled for the
// current explicit manual SYS-W8-05 qualification run; other backends remain
// gated until their real editor screenshot qualification completes.
static bool checkBackendSupported() {
#ifdef MESH_CRAFT_GRAPHICS_BACKEND_STR
    static constexpr const char* kBackend = MESH_CRAFT_GRAPHICS_BACKEND_STR;
    if (MeshCraft::isBackendSupportedAlg(kBackend, false)) return true;
    std::cerr << "[MeshCraft] Error: this build was configured with "
                 "MESH_CRAFT_GRAPHICS_BACKEND='" << kBackend << "'. This backend has not yet "
                 "passed MeshCraft's real editor screenshot qualification.\n";
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

// Every `app` below is an owning raw pointer with an explicit `delete` after Run(), and that shape
// is load-bearing under Emscripten -- do NOT "modernise" it into a local, a unique_ptr, or anything
// else with a destructor.
//
// Game::Run() ends in emscripten_set_main_loop(callback, 0, /*simulateInfiniteLoop=*/1), which the
// Emscripten runtime implements as a raw JavaScript `throw 'unwind'`: the browser takes over and
// calls the registered callback once per animation frame from then on. Both this project and CNA
// compile with -fwasm-exceptions, under which the cleanup landing pad generated for a local with a
// non-trivial destructor genuinely catches that foreign JS throw -- so a stack-allocated
// MeshCraftApplication is destroyed for real at the emscripten_set_main_loop call site, before the
// first tick. That tears down CNA's platform along with it, which quits SDL's video subsystem,
// and the first window event the browser then delivers dies on a window whose subsystem is gone
// ("SDL_GetWindowSize failed: Video subsystem has not been initialized") -- misread for a long
// time as a canvas-sizing problem. See ../cna/docs/emscripten-mainloop-game-lifetime.md.
//
// A raw pointer has no landing pad, so the unwind cannot destroy it; and the `delete` is simply
// never reached under Emscripten (the throw propagates out of Run()), which is exactly right for an
// object that must live as long as the page. Native builds are unaffected: Run() returns normally
// there and the delete happens at the same point the old local's destructor did.

int main(int argc, char* argv[]) {
#if !defined(__EMSCRIPTEN__)
    // Left out of the Emscripten build on purpose: main() never returns there, so this can never
    // do its job -- it could only fire at the wrong moment, during the unwind described above,
    // blocking the browser's JS thread for up to 5s before the first frame.
    AiShutdownWaiter aiShutdownWaiter;
#endif
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

#if defined(__EMSCRIPTEN__)
    // Web builds only. A browser tab has no command line to pass a scene on, and the Open dialog
    // can reach nothing but Emscripten's virtual filesystem -- whose entire contents are whatever
    // CMakeLists.txt preloaded from test/ at build time (--preload-file ... @/test). So the page
    // opened on an empty document and looked like it had failed to load anything. Open one of the
    // bundled scenes instead, and one with real geometry and textures rather than a primitive.
    //
    // Deliberately only when nothing else was requested: --screenshot/--export/--benchmark all
    // demand their own scene argument and are rejected above without one, so reaching here with
    // an empty filePath means the plain interactive start.
    if (filePath.empty()) {
        constexpr const char* kWebStartupScene = "/test/medieval_castle.mc3.xml";
        if (std::filesystem::exists(kWebStartupScene)) {
            filePath = kWebStartupScene;
        } else {
            std::cerr << "[MeshCraft] Bundled startup scene missing from the virtual filesystem: "
                      << kWebStartupScene << " — starting with an empty scene.\n";
        }
    }
#endif

    if (benchmarkMode) {
        auto* app = new MeshCraft::Application::MeshCraftApplication(
            std::filesystem::path(filePath), /*benchmarkMode=*/true);
        app->Run();
        delete app;
    } else if (!filePath.empty() && (!screenshotPath.empty() || !exportPath.empty())) {
        auto* app = new MeshCraft::Application::MeshCraftApplication(
            std::filesystem::path(filePath), screenshotPath, exportPath);
        app->Run();
        const bool failed = (!exportPath.empty() && app->exportFailed()) ||
                            (!screenshotPath.empty() && app->screenshotFailed());
        delete app;
        if (failed) return 1;
    } else if (!filePath.empty()) {
        if (!std::filesystem::exists(filePath)) {
            std::cerr << "[MeshCraft] File not found: " << filePath << "\n";
            std::cerr << "[MeshCraft] Starting with empty scene.\n";
        }
        std::filesystem::path p{filePath};
        auto* app = new MeshCraft::Application::MeshCraftApplication(p);
        app->Run();
        delete app;
    } else {
        auto* app = new MeshCraft::Application::MeshCraftApplication();
        app->Run();
        delete app;
    }
    return 0;
}
