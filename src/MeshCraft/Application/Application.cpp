#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include "MeshCraft/AnimationPreviewAlgorithms.hpp"
#include "MeshCraft/CoordinateSystemAlgorithms.hpp"
#include "MeshCraft/EditorPersistenceAlgorithms.hpp"
#include "MeshCraft/EditorTransformAlgorithms.hpp"
#include "MeshCraft/GraphicsBackendCheck.hpp"
#include "MeshCraft/MeshCraftPrivate.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL.h>

#include <chrono>

#include <Microsoft/Xna/Framework/Input/Keyboard.hpp>
#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/Mouse.hpp>
#include <Microsoft/Xna/Framework/Input/ButtonState.hpp>
#include <Microsoft/Xna/Framework/Graphics/DepthFormat.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/RasterizerState.hpp>
#include <Microsoft/Xna/Framework/Graphics/SamplerState.hpp>
#include <Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp>
#include <Microsoft/Xna/Framework/Graphics/Viewport.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Rectangle.hpp>
#include <System/Object.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <stb_image.h>

namespace MeshCraft::Application {

GetTypeNameCPP(MeshCraftApplication, "MeshCraft::Application::MeshCraftApplication")

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;
using namespace Microsoft::Xna::Framework::Graphics;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MeshCraftApplication::MeshCraftApplication() {
    getWindowProperty().setTitleProperty("Mesh Craft");
    setIsMouseVisibleProperty(true);
}

MeshCraftApplication::MeshCraftApplication(std::filesystem::path filePath)
    : currentFile_(std::move(filePath))
{
    getWindowProperty().setTitleProperty("Mesh Craft");
    setIsMouseVisibleProperty(true);
}

MeshCraftApplication::MeshCraftApplication(std::filesystem::path filePath, std::string screenshotPath)
    : currentFile_(std::move(filePath))
    , autoScreenshotPath_(std::move(screenshotPath))
    , autoScreenshotCountdown_(120)
{
    getWindowProperty().setTitleProperty("Mesh Craft");
    setIsMouseVisibleProperty(true);
}

MeshCraftApplication::MeshCraftApplication(std::filesystem::path filePath, std::string screenshotPath,
                                           std::string exportPath)
    : currentFile_(std::move(filePath))
    , autoScreenshotPath_(std::move(screenshotPath))
    , autoScreenshotCountdown_(autoScreenshotPath_.empty() ? 0 : 120)
    , autoExportPath_(std::move(exportPath))
    , autoExportCountdown_(autoExportPath_.empty() ? 0 : 2)
{
    // Test-only hooks: Bloom and SSAO are UI-menu-only toggles with no CLI/
    // scene-file equivalent. Keep their force switches independent so each
    // headless regression test exercises only the effect it intends to cover.
    if (std::getenv("MESHCRAFT_TEST_FORCE_POSTFX")) {
        bloomEnabled_ = true;
    }
    if (std::getenv("MESHCRAFT_TEST_FORCE_SSAO")) {
        ssaoEnabled_ = true;
    }
    // AUD-088 test-only hook, same shape as AUD-058's above: Shadow Map
    // Debug is a UI-menu-only toggle with no CLI/scene-file equivalent, so
    // a headless --screenshot run never exercises initShadowDebug()/
    // renderShadowDebugFbo() at all.
    if (std::getenv("MESHCRAFT_TEST_FORCE_SHADOWDEBUG")) {
        shadowDebugEnabled_ = true;
    }
    getWindowProperty().setTitleProperty("Mesh Craft");
    setIsMouseVisibleProperty(true);
}

MeshCraftApplication::MeshCraftApplication(std::filesystem::path filePath, bool benchmarkMode)
    : currentFile_(std::move(filePath))
    , benchmarkProgress_(benchmarkMode)
{
    getWindowProperty().setTitleProperty("Mesh Craft");
    setIsMouseVisibleProperty(true);
}

// ---------------------------------------------------------------------------
// SDL event watcher — forwards each event to ImGui before CNA processes it
// ---------------------------------------------------------------------------

bool MeshCraftApplication::sdlEventWatch(void* userdata, void* eventPtr) {
    auto* ev = static_cast<SDL_Event*>(eventPtr);
    ImGui_ImplSDL3_ProcessEvent(ev);
    if (userdata && ev->type == SDL_EVENT_DROP_FILE && ev->drop.data) {
        std::string path = ev->drop.data;
        auto* self = static_cast<MeshCraftApplication*>(userdata);
        // Route image files to texture drop handler; everything else opens as scene
        auto ext = std::filesystem::path(path).extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        static const char* kImgExts[] = {
            ".png",".jpg",".jpeg",".webp",".tga",".bmp",".gif",".hdr",".exr",nullptr
        };
        bool isImage = false;
        for (int i = 0; kImgExts[i]; ++i) if (ext == kImgExts[i]) { isImage = true; break; }
        if (isImage)
            self->pendingDropTexture_ = path;
        else
            self->pendingDropFile_ = path;
    }
    return true;
}

// ---------------------------------------------------------------------------
// LoadContent
// ---------------------------------------------------------------------------

void MeshCraftApplication::LoadContent() {
    // SYS-W12-02: "startup" benchmark category -- times this whole function
    // (scene load + renderer/panel construction), the editor's real
    // one-time initialization cost.
    auto benchmarkLoadStart = std::chrono::steady_clock::now();

    auto& gd = getGraphicsDeviceProperty();

    if (!supportsTextShaderEffects()) {
        // Do not feed GLSL source to CNA's Vulkan ShaderEffect path: it
        // accepts SPIR-V only and cannot represent MeshCraft's named uniforms
        // or 3D depth-prepass pipeline yet.  The editor, scene, and CNA ImGui
        // renderer continue normally; only optional source-shader features
        // are unavailable on this backend.
        bloomEnabled_ = false;
        ssaoEnabled_ = false;
        std::cerr << "[MeshCraft] Source-GLSL ShaderEffects are unavailable on this backend; "
                     "Bloom, SSAO, skybox shading, and material preview are disabled.\n";
    }

    gridRenderer_  = std::make_unique<Renderer::GridRenderer>(gd);
    sceneRenderer_ = std::make_unique<Renderer::SceneRenderer>(gd);
    spriteBatch_   = std::make_unique<Graphics::SpriteBatch>(gd);

    hierarchyPanel_  = std::make_unique<Scene::SceneHierarchyPanel>(document_);
    propertiesPanel_ = std::make_unique<Scene::PropertiesPanel>();

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Blender-inspired theme — neutral dark, orange accent
    {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding    = 4.0f;
        s.ChildRounding     = 3.0f;
        s.FrameRounding     = 3.0f;
        s.GrabRounding      = 3.0f;
        s.TabRounding       = 3.0f;
        s.ScrollbarRounding = 4.0f;
        s.PopupRounding     = 3.0f;
        s.WindowBorderSize  = 1.0f;
        s.FrameBorderSize   = 0.0f;
        s.ItemSpacing       = ImVec2(6, 4);
        s.ItemInnerSpacing  = ImVec2(4, 4);
        s.FramePadding      = ImVec2(6, 3);
        s.WindowPadding     = ImVec2(8, 6);
        s.IndentSpacing     = 18.0f;
        s.ScrollbarSize     = 12.0f;
        s.GrabMinSize       = 8.0f;

        ImVec4* c = s.Colors;
        c[ImGuiCol_Text]                  = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
        c[ImGuiCol_TextDisabled]          = ImVec4(0.48f, 0.48f, 0.48f, 1.00f);
        c[ImGuiCol_WindowBg]              = ImVec4(0.145f,0.145f,0.145f,1.00f);
        c[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        c[ImGuiCol_PopupBg]               = ImVec4(0.18f, 0.18f, 0.18f, 0.97f);
        c[ImGuiCol_Border]                = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
        c[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg]               = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        c[ImGuiCol_FrameBgHovered]        = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        c[ImGuiCol_FrameBgActive]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
        c[ImGuiCol_TitleBg]               = ImVec4(0.105f,0.105f,0.105f,1.00f);
        c[ImGuiCol_TitleBgActive]         = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.105f,0.105f,0.105f,1.00f);
        c[ImGuiCol_MenuBarBg]             = ImVec4(0.115f,0.115f,0.115f,1.00f);
        c[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.62f, 0.62f, 0.62f, 1.00f);
        c[ImGuiCol_CheckMark]             = ImVec4(0.90f, 0.52f, 0.07f, 1.00f);
        c[ImGuiCol_SliderGrab]            = ImVec4(0.90f, 0.52f, 0.07f, 1.00f);
        c[ImGuiCol_SliderGrabActive]      = ImVec4(1.00f, 0.62f, 0.12f, 1.00f);
        c[ImGuiCol_Button]                = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        c[ImGuiCol_ButtonHovered]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        c[ImGuiCol_ButtonActive]          = ImVec4(0.90f, 0.52f, 0.07f, 1.00f);
        c[ImGuiCol_Header]                = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        c[ImGuiCol_HeaderHovered]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        c[ImGuiCol_HeaderActive]          = ImVec4(0.90f, 0.52f, 0.07f, 0.80f);
        c[ImGuiCol_Separator]             = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
        c[ImGuiCol_SeparatorHovered]      = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
        c[ImGuiCol_SeparatorActive]       = ImVec4(0.90f, 0.52f, 0.07f, 1.00f);
        c[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        c[ImGuiCol_ResizeGripHovered]     = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        c[ImGuiCol_ResizeGripActive]      = ImVec4(0.90f, 0.52f, 0.07f, 1.00f);
        c[ImGuiCol_Tab]                   = ImVec4(0.175f,0.175f,0.175f,1.00f);
        c[ImGuiCol_TabHovered]            = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        c[ImGuiCol_TabActive]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        c[ImGuiCol_TabUnfocused]          = ImVec4(0.135f,0.135f,0.135f,1.00f);
        c[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        c[ImGuiCol_PlotLines]             = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        c[ImGuiCol_PlotLinesHovered]      = ImVec4(0.90f, 0.52f, 0.07f, 1.00f);
        c[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.52f, 0.07f, 1.00f);
        c[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.62f, 0.12f, 1.00f);
        c[ImGuiCol_TableHeaderBg]         = ImVec4(0.175f,0.175f,0.175f,1.00f);
        c[ImGuiCol_TableBorderStrong]     = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
        c[ImGuiCol_TableBorderLight]      = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        c[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
        c[ImGuiCol_TextSelectedBg]        = ImVec4(0.90f, 0.52f, 0.07f, 0.35f);
        c[ImGuiCol_DragDropTarget]        = ImVec4(0.90f, 0.52f, 0.07f, 1.00f);
        c[ImGuiCol_NavHighlight]          = ImVec4(0.90f, 0.52f, 0.07f, 1.00f);
        c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
    }

    SDL_Window* sdlWindow = reinterpret_cast<SDL_Window*>(getWindowProperty().getHandleProperty());
    imguiRenderer_ = ImGuiRenderer::createCnaRenderer();
    if (!imguiRenderer_->initialize(getGraphicsDeviceProperty(), sdlWindow))
        throw std::runtime_error("Failed to initialize the CNA ImGui renderer");
    imguiInitialized_ = true;

    SDL_AddEventWatch(reinterpret_cast<SDL_EventFilter>(sdlEventWatch), this);

    // F7: hide window in screenshot/headless mode
    if (!autoScreenshotPath_.empty() || benchmarkProgress_.enabled())
        SDL_HideWindow(sdlWindow);

    loadRecentFiles();
    loadPrefs();
    // STAB-0327: savePrefs() previously only ran from the Preferences
    // dialog's own Close button, so a fresh install never got a prefs.ini
    // on disk until the user explicitly opened and closed that dialog.
    // Write one immediately on first launch, with the defaults loadPrefs()
    // just left in place.
    if (!std::filesystem::exists(prefsPath())) savePrefs();
    keybindings_.load(keybindingsPath());

    if (!currentFile_.empty() && std::filesystem::exists(currentFile_)) {
        try {
            // SYS-W1-01 (pre-render integration point): capture load-time
            // diagnostics for the file about to become the active document
            // (and be rendered) -- same clamp/default/rejection entries a
            // plain loadFromFile() already silently applies, now reported.
            // SYS-W14-11: dispatches on extension (.mcb/.json/else), so
            // `MeshCraft scene.mc3.json` on the command line now works too.
            Mc3::Mc3Validation loadValidation;
            document_ = loadSceneFileDispatched(currentFile_, loadValidation);
            resetEventPreview();
            resetImportHealth();
            objectIndex_.invalidate();  // SYS-W5-04: wholesale document_ replacement
            if (!loadValidation.empty())
                std::cout << "[MeshCraft] Load: " << loadValidation.warningCount() << " warning(s), "
                          << loadValidation.errorCount() << " error(s) in " << currentFile_ << "\n";
            recordValidation("Load: " + currentFile_.filename().string(), loadValidation);
            addRecentFile(currentFile_);
            std::cout << "[MeshCraft] Loaded: " << currentFile_ << "\n";
            checkRotationConventionNotice();
            resolveImports();
            // Auto-start the first action marked autoplay="true"
            for (const auto& [aname, act] : document_.actions) {
                if (act.autoplay) {
                    currentActionName_ = aname;
                    currentActionClipName_.clear();
                    clearAnimationPreviewTransition();
                    animTime_    = 0.0f;
                    animPlaying_ = true;
                    break;
                }
            }
            // In screenshot mode, auto-activate the default_camera so the render
            // uses the scene's own camera instead of the editor viewport camera.
            if (!autoScreenshotPath_.empty() && !document_.defaultCamera.empty()) {
                for (int i = 0; i < static_cast<int>(document_.cameras.size()); ++i) {
                    if (document_.cameras[i].name == document_.defaultCamera) {
                        selectedCameraIdx_ = i;
                        lookThroughCamera_ = true;
                        std::cout << "[Screenshot] Using scene camera: "
                                  << document_.defaultCamera << "\n";
                        break;
                    }
                }
            }
            // SYS-W9-02: offer real recovery (not just a status-message hint)
            // when a newer .autosave sibling exists -- see
            // checkForNewerAutosave()/recoverFromAutosave() in
            // MeshCraftApplication_FileOps.cpp.
            checkForNewerAutosave(currentFile_);
        } catch (const std::exception& e) {
            std::cerr << "[MeshCraft] Failed to load file: " << e.what() << "\n";
        }
    } else {
        newScene();
        // SYS-W9-07: offer recovery for a previous session's never-saved
        // document -- checked once, right after the fresh untitled scene
        // newScene() just created, matching checkForNewerAutosave()'s own
        // "offered whenever a load finishes" placement for the named-file
        // case above.
        checkForUntitledRecovery();
    }

    updateWindowTitle();

    if (benchmarkProgress_.enabled())
        benchmarkProgress_.setLoadContentMs(std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - benchmarkLoadStart).count());
}

// ---------------------------------------------------------------------------
// BeginDraw / EndDraw — ImGui frame lifecycle
// ---------------------------------------------------------------------------

bool MeshCraftApplication::BeginDraw() {
    imguiRenderer_->newFrame();
    ImGui::NewFrame();
    return Game::BeginDraw();
}

void MeshCraftApplication::EndDraw() {
    ImGui::Render();
    imguiRenderer_->render(ImGui::GetDrawData());

    // AUD-087 test-only hook (see the render-to-RT half in Draw()): blit the
    // swatch on top of everything, including ImGui, which only just finished
    // rasterizing above -- a blit anywhere in Draw() would have been
    // overwritten by this same ImGui render call.
    if (std::getenv("MESHCRAFT_TEST_FORCE_MATPREVIEW") && matPreviewTextureToken_ && matPreviewRt_) {
        auto& gd = getGraphicsDeviceProperty();
        gd.setViewportProperty(Viewport(0, 0, cachedScreenW_, cachedScreenH_));
        const Rectangle corner(0, 0, kMatPreviewRes, kMatPreviewRes);
        spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque);
        spriteBatch_->Draw(*matPreviewRt_, corner, Color::White);
        spriteBatch_->End();
    }

    // AUD-088 test-only hook, same rationale as AUD-087's above: blit into
    // a fixed, known top-right corner rather than relying on the real
    // "Shadow Frustum" ImGui overlay's own ImGuiCond_FirstUseEver layout
    // (title bar height, borders, text-line wrapping) for a test's pixel
    // coordinates -- more robust than reverse-engineering ImGui's own
    // window-chrome geometry.
    if (std::getenv("MESHCRAFT_TEST_FORCE_SHADOWDEBUG") && shadowDebugTextureToken_ && shadowDebugRt_) {
        auto& gd = getGraphicsDeviceProperty();
        gd.setViewportProperty(Viewport(0, 0, cachedScreenW_, cachedScreenH_));
        const Rectangle corner(cachedScreenW_ - kShadowDebugRes, 0, kShadowDebugRes, kShadowDebugRes);
        spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque);
        spriteBatch_->Draw(*shadowDebugRt_, corner, Color::White);
        spriteBatch_->End();
    }

    if (pendingExport_) {
        pendingExport_ = false;
        try {
            runGltfExport(autoExportPath_);
            std::cout << "[MeshCraft] Auto-export complete: " << autoExportPath_ << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[MeshCraft] Export error: " << e.what() << "\n";
            exportFailed_ = true;
        }
        if (autoScreenshotPath_.empty())
            Exit();
    }

    if (pendingScreenshot_) {
        pendingScreenshot_ = false;
        if (saveScreenshot(autoScreenshotPath_)) {
            std::cout << "[MeshCraft] Auto-screenshot saved to: " << autoScreenshotPath_ << "\n";
        } else {
            screenshotFailed_ = true;
            std::cerr << "[MeshCraft] Auto-screenshot failed: " << autoScreenshotPath_ << "\n";
        }
        std::cout << "[CsgCache] evaluations: " << sceneRenderer_->csgCacheEvaluationCount() << "\n";
        std::cout << "[CsgCache] size: " << sceneRenderer_->csgMeshCacheSize() << "\n";
        if (!document_.objects.empty()) {
            std::cout << "[LOD] level=" << sceneRenderer_->lastLodLevel(document_.objects.front()->id) << "\n";
            if (const auto assetLod = sceneRenderer_->lastAssetLodSelection(
                    document_.objects.front()->id)) {
                std::cout << "[AssetLOD] tier=" << assetLodTierNameAlg(assetLod->tier)
                          << " culled=" << (assetLod->culled ? 1 : 0)
                          << " definition=" << assetLod->definitionId
                          << " reason=" << assetLod->reason << "\n";
            }
            std::cout << "[CsgTriCount] count=" << sceneRenderer_->csgCachedTriCount(document_.objects.front()->id) << "\n";
        }
        Exit();
    }

    if (benchmarkProgress_.consumeCompletion()) {
        runBenchmarkSuite();
        Exit();
    }
    Game::EndDraw();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void MeshCraftApplication::Update(GameTime& gameTime) {
    // FPS (exponential moving average, α=0.1 per frame)
    {
        float dt = static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
        if (dt > 0.0f) {
            float instant = 1.0f / dt;
            displayFps_ = displayFps_ < 1.0f ? instant : displayFps_ * 0.9f + instant * 0.1f;
        }
    }

    // Status bar notification countdown + auto-save
    {
        float dt = static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
        statusNotification_.advance(dt);

        // AUD-031: was a hand-copied duplicate of autoSaveTickAlg's own
        // countdown logic; now delegates to it directly (single tested
        // implementation).
        if (autoSaveTickAlg(!currentFile_.empty(), modified_, autoSaveInterval_,
                             dt, autoSaveCountdown_)) {
            performAutoSave();
        }
        // SYS-W9-07: same tick logic, own countdown, inverted hasCurrentFile
        // so a never-saved document gets its own periodic safety-net save
        // without touching autoSaveTickAlg's tested "no current file never
        // auto-saves" behavior above.
        if (autoSaveTickAlg(currentFile_.empty(), modified_, autoSaveInterval_,
                             dt, autoSaveUntitledCountdown_)) {
            performUntitledRecoverySave();
        }
    }

    // SYS-W14-40: Preview/Play is the only event-execution route.  Timers
    // and Walk Mode Area transitions are both sent through the same bounded,
    // transactional runner; ordinary editing never dispatches bindings.
    if (automationWorkspace_.previewEnabled) {
        const float dt = static_cast<float>(gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
        executeEventPreview(automationWorkspace_.previewRunner.advanceTimers(document_, dt));
        if (walkController_.isActive()) {
            const auto authoredPosition = coordinateFromYUpAlg(
                document_.coordinateSystem,
                {walkController_.posX(), walkController_.posY(), walkController_.posZ()});
            executeEventPreview(automationWorkspace_.previewRunner.updateAreaTransitions(document_, authoredPosition));
        }
    }

    // Advance animation clock
    if (animPlaying_ && !currentActionName_.empty()) {
        auto it = document_.actions.find(currentActionName_);
        if (it != document_.actions.end()) {
            float dt = static_cast<float>(
                gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
            const auto& action = it->second;
            const auto* clip = findAnimationClip(action, currentActionClipName_);
            const auto range = animationPreviewRange(action, clip);
            animTime_ = advanceAnimationPreviewTime(range, animTime_, dt, animPlaying_);
            if (animTransitionDuration_ > 0.0f) {
                const auto* fromClip = findAnimationClip(action, animTransitionFromClipName_);
                bool fromPlaying = true; // a completed source holds its endpoint until cross-fade ends
                animTransitionFromTime_ = advanceAnimationPreviewTime(
                    animationPreviewRange(action, fromClip), animTransitionFromTime_, dt, fromPlaying);
                animTransitionElapsed_ += dt;
                if (animTransitionElapsed_ >= animTransitionDuration_)
                    clearAnimationPreviewTransition();
            }
            evaluateAndPushAnimOverrides();
        } else {
            currentActionClipName_.clear();
            clearAnimationPreviewTransition();
            animPlaying_ = false;
        }
    }

    // Consume dropped file path (set by SDL event watcher)
    if (!pendingDropFile_.empty()) {
        std::filesystem::path dropPath = std::move(pendingDropFile_);
        pendingDropFile_.clear();
        if (isDroppableScenePathAlg(dropPath))
            confirmIfModified(PendingAction::OpenRecentFile, dropPath);
        else
            setStatusMsg("Unsupported file type: " + dropPath.filename().string(), true, 3.0f);
    }

    // SYS-W14-15: consume a resolved native file-browse dialog by feeding it
    // into the exact same pendingDropTexture_/hoveredTex*_ pipeline the OS
    // drag-and-drop path (D6) already uses below -- the browse target
    // (matId/slot) was recorded at click time, unlike hover state, so this
    // always takes the "assign directly" branch, never the picker-popup one.
    if (pendingFileBrowse_ && pendingFileBrowse_->done.load()) {
        std::string browsedPath, targetMatId, targetSlot;
        {
            std::lock_guard<std::mutex> lock(pendingFileBrowse_->mutex);
            browsedPath = pendingFileBrowse_->path;
            targetMatId = pendingFileBrowse_->targetMatId;
            targetSlot  = pendingFileBrowse_->targetSlot;
        }
        pendingFileBrowse_.reset();
        if (!browsedPath.empty()) {
            hoveredTexMatId_    = targetMatId;
            hoveredTexSlot_     = targetSlot;
            pendingDropTexture_ = browsedPath;
        }
    }

    // Consume dropped texture image (D6)
    if (!pendingDropTexture_.empty()) {
        std::string texPath = std::move(pendingDropTexture_);
        pendingDropTexture_.clear();
        if (!hoveredTexSlot_.empty() && !hoveredTexMatId_.empty() &&
            document_.materials.count(hoveredTexMatId_)) {
            // Assign directly to the hovered slot
            // F8: promote to local -- see MeshCraftApplication_UiLeftPanel.cpp's
            // "Mat" tab pushUndoMat for the full rationale.
            pushUndo();
            document_.includedMaterials.erase(hoveredTexMatId_);
            auto& mat = document_.materials[hoveredTexMatId_];
            // F9: a texture-slot field is a doc.textures KEY, not a raw
            // path -- register (or reuse) an entry for the dropped file and
            // assign its id, or the renderer's/exporter's doc.textures.find()
            // lookup silently fails to resolve it.
            std::string texId = registerTextureFromPath(texPath);
            if      (hoveredTexSlot_ == "base")       mat.baseColorTexture         = texId;
            else if (hoveredTexSlot_ == "normal")     mat.normalTexture            = texId;
            else if (hoveredTexSlot_ == "emissive")   mat.emissiveTexture          = texId;
            else if (hoveredTexSlot_ == "metalrough") mat.metallicRoughnessTexture = texId;
            else if (hoveredTexSlot_ == "occlusion")  mat.occlusionTexture         = texId;
            modified_ = true;
            setStatusMsg("Texture dropped into " + hoveredTexSlot_ + " slot");
        } else {
            // No hovered slot — open picker popup
            dropTexPickerPath_   = texPath;
            dropTexPickerMatId_  = selectedMaterialKey_;
            dropTexPickerOpen_   = true;
        }
    }

    auto ks = Keyboard::GetState();
    auto ms = Mouse::GetState();

    if (!firstFrame_) {
        auto& io = ImGui::GetIO();

        if (walkController_.isActive()) {
            // Walk mode consumes all keyboard + mouse; skip normal handlers
            float dt = static_cast<float>(
                gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
            int mdx = ms.getXProperty() - prevMouse_.getXProperty();
            int mdy = ms.getYProperty() - prevMouse_.getYProperty();
            if (!io.WantCaptureKeyboard)
                updateWalkMode(dt, ks, mdx, mdy);
        } else {
            if (!io.WantCaptureKeyboard)
                handleKeyboardShortcuts(ks, prevKs_);
            if (!io.WantCaptureMouse)
                handleMouseInput(ms, prevMouse_);
        }
    }

    firstFrame_ = false;
    prevKs_     = ks;
    prevMouse_  = ms;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void MeshCraftApplication::Draw(const GameTime& /*gameTime*/) {
    // SYS-W12-02: "first frame" vs "warm frame" benchmark timing -- wraps
    // the REAL per-frame render path (correct camera_/view/proj already
    // computed below) rather than a hand-rolled stand-in, so cold-vs-warm
    // cache costs (CSG evaluation, texture load, mesh load) are genuine.
    auto benchmarkFrameStart = std::chrono::steady_clock::now();

    auto& gd = getGraphicsDeviceProperty();

    // ImGui_ImplSDL3_NewFrame() (called in BeginDraw) queries SDL_GetWindowSizeInPixels
    // and stores the result in DisplaySize — use it as the authoritative screen size so
    // that resizing the OS window is reflected immediately without CNA viewport lag.
    const ImVec2 dsz = ImGui::GetIO().DisplaySize;
    int screenW = (dsz.x > 0) ? static_cast<int>(dsz.x) : gd.getViewportProperty().getWidthProperty();
    int screenH = (dsz.y > 0) ? static_cast<int>(dsz.y) : gd.getViewportProperty().getHeightProperty();
    cachedScreenW_ = screenW;
    cachedScreenH_ = screenH;

    // Compute top area height from ImGui (menu bar + toolbar window).
    // On the very first frame imguiTopH_ is 0; a reasonable fallback is 60.
    int topH = imguiTopH_ > 0 ? imguiTopH_ : 60;

    // Panel background
    gd.Clear(Color(18, 20, 36, 255));

    int viewX = kLeftPanelW;
    int viewY = topH;
    int viewW = std::max(1, screenW - kLeftPanelW - kRightPanelW);
    int timelineH = showTimeline_ ? kTimelineH : 0;
    int viewH = std::max(1, screenH - topH - timelineH - kStatusH);

    RasterizerState rsScissorOn = gd.getRasterizerStateProperty();
    rsScissorOn.setScissorTestEnableProperty(true);
    gd.setScissorRectangleProperty(Rectangle(viewX, viewY, viewW, viewH));
    gd.setRasterizerStateProperty(rsScissorOn);

    Color bgColor(64, 72, 80, 255);
    if (document_.environment) {
        const auto& bc = document_.environment->backgroundColor;
        bgColor = Color(
            static_cast<int>(std::clamp(bc[0], 0.0f, 1.0f) * 255),
            static_cast<int>(std::clamp(bc[1], 0.0f, 1.0f) * 255),
            static_cast<int>(std::clamp(bc[2], 0.0f, 1.0f) * 255),
            255);
    }
    gd.Clear(bgColor);

    // I1: draw background texture stretched to fill viewport (before 3D scene)
    if (spriteBatch_ && document_.environment &&
        !document_.environment->backgroundTexture.empty()) {
        const std::string& btUri = document_.environment->backgroundTexture;
        std::string absPath = (document_.sourcePath / btUri).string();
        if (absPath != bgTexturePath_) {
            bgTexturePath_ = absPath;
            bgTexture_.reset();
            try { bgTexture_.emplace(absPath, gd); } catch (...) {}
        }
        if (bgTexture_) {
            Rectangle destRect{viewX, viewY, viewW, viewH};
            spriteBatch_->Begin();
            spriteBatch_->Draw(*bgTexture_, destRect, Color::White);
            spriteBatch_->End();
        }
    } else if (document_.environment &&
               document_.environment->backgroundTexture.empty() &&
               !bgTexturePath_.empty()) {
        bgTexturePath_.clear();
        bgTexture_.reset();
    }

    gd.setViewportProperty(Viewport(viewX, viewY, viewW, viewH));

    float aspect = (viewH > 0) ? static_cast<float>(viewW) / viewH : 16.0f / 9.0f;
    Matrix view = camera_.viewMatrix();
    Matrix proj = camera_.projectionMatrix(aspect);
    float effectiveFovDegrees = camera_.fovDegrees;

    // Walk mode: override view with first-person camera. The projection
    // stays computed from camera_'s own fovDegrees/nearPlane/farPlane --
    // walk mode has never had its own FOV/clip-plane settings, only its
    // own eye position/orientation (Editor::WalkController::viewMatrix()).
    if (walkController_.isActive()) {
        const float pi = std::numbers::pi_v<float>;
        view = walkController_.viewMatrix();
        float fovRad = camera_.fovDegrees * pi / 180.0f;
        proj = Matrix::CreatePerspectiveFieldOfView(fovRad, aspect,
                                                     camera_.nearPlane, camera_.farPlane);
    }

    // Look-through-camera mode: override view/proj from selected Mc3Camera
    if (lookThroughCamera_ && selectedCameraIdx_ >= 0 &&
        selectedCameraIdx_ < static_cast<int>(document_.cameras.size())) {
        const auto& cam = document_.cameras[selectedCameraIdx_];
        const float pi = std::numbers::pi_v<float>;
        const auto camPosYUp = coordinateToYUpAlg(document_.coordinateSystem, cam.position);
        Vector3 camPos(camPosYUp[0], camPosYUp[1], camPosYUp[2]);
        Vector3 up(0.0f, 1.0f, 0.0f);
        // 2026-07-20 audit finding #6: a camera authored with `rotation`
        // instead of `target` (target left at its {0,0,0} default) used to
        // always look at the origin here -- CreateLookAt needs a target
        // point, not a direction, so derive one an arbitrary distance
        // along the rotation-derived forward vector (any positive
        // distance produces the same view direction).
        Vector3 camTarget;
        if (cam.rotation.has_value()) {
            Vector3 fwd = Renderer::SceneRenderer::cameraForwardFromRotation(document_, *cam.rotation);
            const auto fwdYUp = coordinateToYUpAlg(document_.coordinateSystem,
                                                    {fwd.X, fwd.Y, fwd.Z});
            fwd = {fwdYUp[0], fwdYUp[1], fwdYUp[2]};
            camTarget = camPos + fwd;
        } else {
            const auto targetYUp = coordinateToYUpAlg(document_.coordinateSystem, cam.target);
            camTarget = Vector3(targetYUp[0], targetYUp[1], targetYUp[2]);
        }
        view = Matrix::CreateLookAt(camPos, camTarget, up);
        if (cam.type == Mc3::CameraType::Orthographic) {
            float hw = cam.orthoSize * aspect;
            proj = Matrix::CreateOrthographic(hw * 2.0f, cam.orthoSize * 2.0f,
                                               cam.nearPlane, cam.farPlane);
        } else {
            float fovRad = cam.fov * pi / 180.0f;
            proj = Matrix::CreatePerspectiveFieldOfView(fovRad, aspect,
                                                         cam.nearPlane, cam.farPlane);
            effectiveFovDegrees = cam.fov;
        }
    } else {
        lookThroughCamera_ = false; // auto-clear if camera removed
    }

    // Cache for measurement overlay projection
    cachedVP_ = view * proj;
    cachedVX_ = viewX; cachedVY_ = viewY; cachedVW_ = viewW; cachedVH_ = viewH;

    // Shadow map debug (I7) — render scene from first castShadows directional light
    if (shadowDebugEnabled_) {
        for (const auto& l : document_.lights) {
            if (l.type == Mc3::LightType::Directional && l.castShadows) {
                const auto dirYUp = coordinateToYUpAlg(document_.coordinateSystem, l.direction);
                Vector3 ldir(dirYUp[0], dirYUp[1], dirYUp[2]);
                ldir = Vector3::Normalize(ldir);
                constexpr float kShadowDist = 50.f;
                Vector3 center(0.f, 0.f, 0.f);
                Vector3 lpos = center - ldir * kShadowDist;
                Vector3 up(0.f, 1.f, 0.f);
                if (std::abs(ldir.Y) > 0.99f) up = Vector3(1.f, 0.f, 0.f);
                Matrix lv = Matrix::CreateLookAt(lpos, center, up);
                Matrix lp = Matrix::CreateOrthographic(kShadowDist * 2.f, kShadowDist * 2.f,
                                                        0.1f, kShadowDist * 3.f);
                if (!shadowDebugRt_) initShadowDebug();
                if (shadowDebugRt_)  renderShadowDebugFbo(lv, lp);
                break;
            }
        }
    }

    // I2: equirectangular skybox (drawn before scene, no depth write)
    drawSkybox(view, effectiveFovDegrees, aspect);

    gd.SetDepthTestEnabled(false);
    gridRenderer_->draw(view, proj);

    gd.SetDepthTestEnabled(true);
    auto selPtrs = selectedPointers();
    if (!showWireframeMode_) {
        sceneRenderer_->draw(document_, view, proj, selPtrs);
        if (benchmarkProgress_.recordingFrames())
            benchmarkTextureStats_.push_back(sceneRenderer_->lastTextureProcessingStats());
    }

    if (showEdgeOverlay_ || showWireframeMode_) {
        gd.SetDepthTestEnabled(true);
        sceneRenderer_->drawEdgeOverlay(document_, view, proj);
    }

    // Screenshot regressions for lighting need to measure shaded geometry,
    // not the deliberately bright editor overlays. This test-only switch does
    // not change normal editor behaviour or the dedicated gizmo fixtures.
    if (!std::getenv("MESHCRAFT_TEST_HIDE_SCENE_GIZMOS")) {
        gd.SetDepthTestEnabled(false);
        sceneRenderer_->drawLightGizmos(document_, view, proj);
        sceneRenderer_->drawCameraGizmos(document_, view, proj);
        sceneRenderer_->drawCsgGizmos(document_, view, proj);
        gd.SetDepthTestEnabled(true);
    }

    // SYS-W14-30: while walking, show the exact active collision shapes in
    // world space. Unsupported authored proxy types are deliberately omitted
    // here and called out persistently by the Walk Mode HUD instead.
    if (walkController_.isActive() && !walkColliders_.empty()) {
        gd.SetDepthTestEnabled(false);
        sceneRenderer_->drawWalkCollisionDebug(walkColliders_, view, proj);
        gd.SetDepthTestEnabled(true);
    }

    if (selection_.hasSelection()) {
        float gizmoLen = camera_.distance * 0.15f;
        auto* sel0 = selection_.selection().front().get();
        if (activeTool_ == ActiveTool::Move) {
            gd.SetDepthTestEnabled(false);
            if (pivotEditMode_) {
                // Render gizmo at position + pivot
                Mc3::Mc3Object pivProxy = *sel0;
                for (int i = 0; i < 3; ++i) {
                    pivProxy.transform.position[i] += pivProxy.transform.pivot[i];
                    pivProxy.transform.pivot[i] = 0.0f;
                }
                sceneRenderer_->drawGizmo(&pivProxy, document_, view, proj, gizmoLen, gizmoLocalSpace_);
            } else {
                sceneRenderer_->drawGizmo(sel0, document_, view, proj, gizmoLen, gizmoLocalSpace_);
            }
        } else if (activeTool_ == ActiveTool::Scale) {
            gd.SetDepthTestEnabled(false);
            sceneRenderer_->drawScaleGizmo(sel0, document_, view, proj, gizmoLen, gizmoLocalSpace_);
        } else if (activeTool_ == ActiveTool::Rotate) {
            gd.SetDepthTestEnabled(false);
            sceneRenderer_->drawRotateGizmo(sel0, document_, view, proj, gizmoLen, gizmoLocalSpace_);
        }

        // Bounding box overlay (cyan wire box for each selected object)
        if (showBoundingBox_) {
            gd.SetDepthTestEnabled(false);
            Color bboxColor(80, 220, 255, 200);
            for (const auto& selObj : selection_.selection())
                sceneRenderer_->drawObjectWireframe(*selObj, document_, view, proj, bboxColor);
        }
    }

    // Proportional editing (H1) falloff radius indicator — a wireframe
    // sphere centered on the selection's average position, matching the
    // center used by applyProportionalFalloffAlg() during a Move drag.
    if (propEditEnabled_ && propEditRadius_ > 0.0f && selection_.hasSelection()) {
        gd.SetDepthTestEnabled(false);
        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        for (const auto& s : selection_.selection()) {
            cx += s->transform.position[0];
            cy += s->transform.position[1];
            cz += s->transform.position[2];
        }
        float n = static_cast<float>(selection_.selection().size());
        Color propColor(255, 170, 60, 140);
        sceneRenderer_->drawWireSphereAt({cx / n, cy / n, cz / n}, propEditRadius_, document_,
                                          view, proj, propColor);
    }

    // Locked-object outline (red wireframe around every locked object)
    if (!objectLockState_.empty()) {
        gd.SetDepthTestEnabled(false);
        Color lockColor(220, 60, 60, 180);
        std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> drawLocked;
        drawLocked = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
            for (const auto& obj : list) {
                if (objectLockState_.isLocked(obj->id))
                    sceneRenderer_->drawObjectWireframe(*obj, document_, view, proj, lockColor);
                if (!obj->children.empty()) drawLocked(obj->children);
            }
        };
        drawLocked(document_.objects);
    }

    RasterizerState rsScissorOff = gd.getRasterizerStateProperty();
    rsScissorOff.setScissorTestEnableProperty(false);
    gd.setRasterizerStateProperty(rsScissorOff);

    Graphics::Viewport vpReset;
    vpReset.setXProperty(0); vpReset.setYProperty(0);
    vpReset.setWidthProperty(screenW);
    vpReset.setHeightProperty(screenH);
    gd.setViewportProperty(vpReset);

    // SSAO post-process (I5) — multiplicative ambient occlusion darkening
    if (ssaoEnabled_) {
        if (ssaoFboW_ != viewW || ssaoFboH_ != viewH || !ssaoDepthRt_)
            initSsao(viewW, viewH);
        const float tanHalfFovY = std::tan(camera_.fovDegrees * 0.5f * std::numbers::pi_v<float> / 180.f);
        applySsao(viewX, viewY, viewW, viewH, view, proj,
                  tanHalfFovY * aspect, tanHalfFovY,
                  camera_.nearPlane, camera_.farPlane);
    }

    // Bloom post-process (I6) — additive emissive glow
    if (bloomEnabled_) {
        if (bloomFboW_ != viewW || bloomFboH_ != viewH)
            initBloom(viewW, viewH);
        applyBloom(viewX, viewY, viewW, viewH, view, proj);
    }

    // AUD-087 test-only hook: the material preview swatch is only ever drawn
    // inside the left panel's Materials section, gated on a material being
    // selected in the UI -- no CLI/scene-file equivalent exists, so a plain
    // headless --screenshot run never exercises initMatPreview()/
    // renderMatPreview() at all. Setting this env var renders a swatch with
    // a known distinctive color, matching AUD-058's own established pattern
    // for bloom/SSAO; the actual on-screen blit happens in EndDraw(), after
    // the CNA renderer in EndDraw() -- drawImGuiUi() here only queues ImGui's
    // draw list, it doesn't rasterize pixels yet, so a blit anywhere
    // in Draw() (even after this call) would still get overwritten once
    // ImGui's own real rendering runs afterward in EndDraw().
    if (std::getenv("MESHCRAFT_TEST_FORCE_MATPREVIEW")) {
        initMatPreview();
        renderMatPreview(1.0f, 0.0f, 0.0f, 0.5f, 0.0f);
    }

    // Box-select rectangle overlay drawn via ImGui in drawImGuiUi()
    gd.SetDepthTestEnabled(false);
    drawImGuiUi(screenW, screenH);

    if (autoScreenshotCountdown_ > 0) {
        --autoScreenshotCountdown_;
        if (autoScreenshotCountdown_ == 0 && !autoScreenshotPath_.empty())
            pendingScreenshot_ = true;
    }

    if (autoExportCountdown_ > 0) {
        --autoExportCountdown_;
        if (autoExportCountdown_ == 0 && !autoExportPath_.empty())
            pendingExport_ = true;
    }

    if (benchmarkProgress_.recordingFrames()) {
        double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - benchmarkFrameStart).count();
        benchmarkProgress_.recordFrame(ms);
    }
}

// ---------------------------------------------------------------------------
// ImGui UI
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// ImGui UI — orchestrator
// ---------------------------------------------------------------------------

void MeshCraftApplication::drawImGuiUi(int screenW, int screenH)
{
    float menuBarH  = drawMenuBar();
    float toolbarH  = drawToolbar(menuBarH, screenW);
    imguiTopH_      = static_cast<int>(menuBarH + toolbarH);

    int   tlPanelH  = showTimeline_ ? kTimelineH : 0;
    float panelY    = menuBarH + toolbarH;
    float panelH    = static_cast<float>(screenH) - panelY
                      - static_cast<float>(kStatusH + tlPanelH);

    drawLeftPanel(panelY, panelH);
    drawPropertiesPanel(panelY, panelH, screenW, screenH);

    if (showTimeline_)
        drawTimelinePanel(screenW, screenH);

    if (walkController_.isActive())
        drawWalkModeHud(screenW, screenH);
    else
        drawStatsOverlay(screenW, screenH);
    drawStatusBar(screenW, screenH);
    if (!walkController_.isActive())
        drawPanelSplitters(screenW, screenH);
    drawShadowDebugOverlay(screenW, screenH);
    drawDialogs();
}

// ---------------------------------------------------------------------------
// Bloom post-processing (I6) — CNA RenderTarget2D + ShaderEffect passes
// ---------------------------------------------------------------------------
/*
 * The post-processing shaders below stay in the MeshCraft namespace. CNA's
 * cross-backend ShaderEffect compiles them; MeshCraft owns no native graphics
 * function table or graphics-object lifecycle for these passes.
 */
// AUD-084: Bloom's own blur/composite passes now go through CNA's
// SpriteBatch + ShaderEffect (RenderTarget2D-backed) instead of the raw-GL
// gl_VertexID/FBO machinery above -- SpriteBatch supplies its own vertex
// shader interface (aPos/aTexCoord/aColor + a `projection` uniform, and its
// custom-effect texture unit 0 binds to a uniform literally named
// `texture1` -- both confirmed against ../cna/examples/
// easygl_bloom_pipeline_test.cpp, a real passing end-to-end SpriteBatch+
// RenderTarget2D+ShaderEffect bloom pipeline test in this exact
// environment), so these need a different vertex-shader interface than
// the former raw-GL full-screen quad, but the blur/composite math is unchanged from the
// original kBloomBlurFS/kBloomCompositeFS (just u_tex -> texture1,
// v_uv -> TexCoord).
const char* kBloomVertSrc = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
out vec2 TexCoord;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";
const char* kBloomBlurFragSrc = R"(#version 300 es
precision mediump float;
uniform sampler2D texture1;
uniform vec2 u_dir;
in vec2 TexCoord;
out vec4 fragColor;
void main() {
    vec4 c  = texture(texture1, TexCoord)                          * 0.22702702;
    c += texture(texture1, TexCoord + u_dir *  1.38461538) * 0.31621621;
    c += texture(texture1, TexCoord - u_dir *  1.38461538) * 0.31621621;
    c += texture(texture1, TexCoord + u_dir *  3.23076923) * 0.07027027;
    c += texture(texture1, TexCoord - u_dir *  3.23076923) * 0.07027027;
    fragColor = c;
}
)";
const char* kBloomCompositeFragSrc = R"(#version 300 es
precision mediump float;
uniform sampler2D texture1;
uniform float u_strength;
in vec2 TexCoord;
out vec4 fragColor;
void main() {
    fragColor = vec4(texture(texture1, TexCoord).rgb * u_strength, 1.0);
}
)";

// AUD-086: Skybox now goes through SpriteBatch + ShaderEffect like Bloom's
// blur/composite passes (see kBloomVertSrc, reused directly below), so the
// per-fragment view-direction math that used to live in the vertex shader
// (computed from gl_VertexID-synthesized NDC) is recomputed here from
// SpriteBatch's own TexCoord varying instead -- same formula, just fed
// from TexCoord*2-1 (X) / 1-TexCoord*2 (Y, flipping SpriteBatch's top-left-
// origin UV convention back to the original's +Y-is-up NDC convention).
const char* kSkyboxFragSrc = R"(#version 300 es
precision mediump float;
uniform sampler2D texture1;
uniform vec3 u_right;
uniform vec3 u_up;
uniform vec3 u_forward;
uniform vec2 u_tanFov;
in vec2 TexCoord;
out vec4 fragColor;
const float PI = 3.14159265;
void main() {
    vec2 ndc = vec2(TexCoord.x * 2.0 - 1.0, 1.0 - TexCoord.y * 2.0);
    vec3 dir = u_forward + ndc.x * u_tanFov.x * u_right + ndc.y * u_tanFov.y * u_up;
    vec3 d = normalize(dir);
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;
    float v = asin(clamp(d.y, -1.0, 1.0)) / PI + 0.5;
    fragColor = texture(texture1, vec2(u, 1.0 - v));
}
)";

const char* kSsaoFS = R"(#version 300 es
precision highp float;
uniform sampler2D u_depth;
uniform vec2      u_tanHalfFov;
uniform float     u_near;
uniform float     u_far;
uniform float     u_radius;
in vec2 v_uv;
out vec4 fragColor;

float linDepth(float rawD) {
    float z = rawD * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

vec3 viewPos(vec2 uv, float ld) {
    vec2 ndc = uv * 2.0 - 1.0;
    return vec3(ndc * u_tanHalfFov * ld, -ld);
}

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    float rawD = texture(u_depth, v_uv).r;
    if (rawD >= 0.9999) { fragColor = vec4(1.0); return; }

    float ld  = linDepth(rawD);
    vec3  pos = viewPos(v_uv, ld);
    vec3  N   = normalize(cross(dFdx(pos), dFdy(pos)));

    const int SAMPLES = 16;
    float occ = 0.0;
    for (int i = 0; i < SAMPLES; i++) {
        float fi  = float(i);
        float r   = rand(v_uv + vec2(fi * 0.137, fi * 0.371)) * u_radius;
        float phi = rand(v_uv + vec2(fi * 0.721, fi * 0.173)) * 6.28318;
        float cth = rand(v_uv + vec2(fi * 0.531, fi * 0.979));
        float sth = sqrt(max(0.0, 1.0 - cth * cth));
        vec3 sDir = vec3(sth * cos(phi), sth * sin(phi), cth);
        if (dot(sDir, N) < 0.0) sDir = -sDir;
        vec3 sPos = pos + sDir * r;
        vec2 sNdc = sPos.xy / (-sPos.z * u_tanHalfFov);
        vec2 sUv  = sNdc * 0.5 + 0.5;
        if (any(lessThan(sUv, vec2(0.0))) || any(greaterThan(sUv, vec2(1.0)))) continue;
        float sLin = linDepth(texture(u_depth, sUv).r);
        float rangeCheck = smoothstep(0.0, 1.0, u_radius / abs(ld - sLin + 0.001));
        if (sLin < ld - 0.025) occ += rangeCheck;
    }
    occ /= float(SAMPLES);
    float ao = 1.0 - occ;
    fragColor = vec4(ao, ao, ao, 1.0);
}
)";

const char* kSsaoBlurFS = R"(#version 300 es
precision mediump float;
uniform sampler2D u_ao;
uniform vec2      u_texelSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    float ao = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            ao += texture(u_ao, v_uv + vec2(float(x), float(y)) * u_texelSize).r;
        }
    }
    ao /= 25.0;
    fragColor = vec4(ao, ao, ao, 1.0);
}
)";

const char* kSsaoCompositeFS = R"(#version 300 es
precision mediump float;
uniform sampler2D u_ao;
uniform float     u_strength;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    float ao     = texture(u_ao, v_uv).r;
    float factor = mix(1.0, ao, u_strength);
    fragColor = vec4(factor, factor, factor, 1.0);
}
)";

// AUD-085: the CNA-native SSAO variants use SpriteBatch's texture1/TexCoord
// interface. TexCoord is top-left-origin, unlike the old raw-GL fullscreen
// quad, hence the explicit Y conversion when projecting a sample position.
const char* kSsaoCnaFragSrc = R"(#version 300 es
precision highp float;
uniform sampler2D texture1;
uniform vec2      u_tanHalfFov;
uniform float     u_near;
uniform float     u_far;
uniform float     u_radius;
in vec2 TexCoord;
out vec4 fragColor;
float linDepth(float rawD) {
    float z = rawD * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}
vec3 viewPos(vec2 uv, float ld) {
    vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    return vec3(ndc * u_tanHalfFov * ld, -ld);
}
float rand(vec2 co) { return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    float rawD = texture(texture1, TexCoord).r;
    if (rawD >= 0.9999) { fragColor = vec4(1.0); return; }
    float ld = linDepth(rawD);
    vec3 pos = viewPos(TexCoord, ld);
    vec3 normal = normalize(cross(dFdx(pos), dFdy(pos)));
    float occ = 0.0;
    for (int i = 0; i < 16; ++i) {
        float fi = float(i);
        float r = rand(TexCoord + vec2(fi * 0.137, fi * 0.371)) * u_radius;
        float phi = rand(TexCoord + vec2(fi * 0.721, fi * 0.173)) * 6.28318;
        float cth = rand(TexCoord + vec2(fi * 0.531, fi * 0.979));
        float sth = sqrt(max(0.0, 1.0 - cth * cth));
        vec3 sampleDir = vec3(sth * cos(phi), sth * sin(phi), cth);
        if (dot(sampleDir, normal) < 0.0) sampleDir = -sampleDir;
        vec3 samplePos = pos + sampleDir * r;
        vec2 sampleNdc = samplePos.xy / (-samplePos.z * u_tanHalfFov);
        vec2 sampleUv = vec2(sampleNdc.x * 0.5 + 0.5, 0.5 - sampleNdc.y * 0.5);
        if (any(lessThan(sampleUv, vec2(0.0))) || any(greaterThan(sampleUv, vec2(1.0)))) continue;
        float sampleLinear = linDepth(texture(texture1, sampleUv).r);
        float rangeCheck = smoothstep(0.0, 1.0, u_radius / abs(ld - sampleLinear + 0.001));
        if (sampleLinear < ld - 0.025) occ += rangeCheck;
    }
    float ao = 1.0 - occ / 16.0;
    fragColor = vec4(ao, ao, ao, 1.0);
}
)";

const char* kSsaoCnaBlurFragSrc = R"(#version 300 es
precision mediump float;
uniform sampler2D texture1;
uniform vec2 u_texelSize;
in vec2 TexCoord;
out vec4 fragColor;
void main() {
    float ao = 0.0;
    for (int x = -2; x <= 2; ++x)
        for (int y = -2; y <= 2; ++y)
            ao += texture(texture1, TexCoord + vec2(float(x), float(y)) * u_texelSize).r;
    ao /= 25.0;
    fragColor = vec4(ao, ao, ao, 1.0);
}
)";

const char* kSsaoCnaCompositeFragSrc = R"(#version 300 es
precision mediump float;
uniform sampler2D texture1;
uniform float u_strength;
in vec2 TexCoord;
out vec4 fragColor;
void main() {
    float ao = texture(texture1, TexCoord).r;
    float factor = mix(1.0, ao, u_strength);
    fragColor = vec4(factor, factor, factor, 1.0);
}
)";

// Material preview sphere shader (D7): fullscreen triangle, SDF sphere with Blinn-Phong.
// AUD-087: TexCoord (SpriteBatch's varying, top-left origin) is flipped in Y here to
// match the original v_uv's bottom-left-origin convention (the old kBloomVS's own
// gl_VertexID-driven UV) -- the light direction below is Y-asymmetric (L.y=1.0), so
// getting this flip right actually matters for a faithful highlight position, unlike
// AUD-086's skybox (whose own Y-flip need was verified against the same reference).
const char* kMatPreviewFragSrc = R"(#version 300 es
precision mediump float;
uniform vec3  u_color;
uniform float u_roughness;
uniform float u_metallic;
in vec2 TexCoord;
out vec4 fragColor;
void main() {
    vec2 v_uv = vec2(TexCoord.x, 1.0 - TexCoord.y);
    vec2 p = v_uv * 2.0 - 1.0;
    float r2 = dot(p, p);
    if (r2 > 1.0) discard;
    float z = sqrt(1.0 - r2);
    vec3 N = normalize(vec3(p, z));
    vec3 L = normalize(vec3(0.6, 1.0, 0.8));
    vec3 V = vec3(0.0, 0.0, 1.0);
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float shininess = mix(128.0, 2.0, u_roughness);
    float spec = pow(NdotH, shininess) * (1.0 - u_roughness * 0.7);
    vec3 f0 = mix(vec3(0.04), u_color, u_metallic);
    vec3 diffuse = u_color * (1.0 - u_metallic);
    vec3 ambient = u_color * 0.12;
    vec3 col = ambient + diffuse * NdotL * 0.88 + f0 * spec * 0.9;
    fragColor = vec4(col, 1.0);
}
)";

} // namespace MeshCraft::Application

namespace MeshCraft::Application {

void MeshCraftApplication::initBloom(int w, int h)
{
    if (!supportsTextShaderEffects() || w <= 0 || h <= 0) return;
    auto& gd = getGraphicsDeviceProperty();

    bloomRtA_.emplace(gd, w, h);
    bloomRtB_.emplace(gd, w, h);
    bloomBlurFx_.emplace(gd, kBloomVertSrc, kBloomBlurFragSrc);
    bloomCompositeFx_.emplace(gd, kBloomVertSrc, kBloomCompositeFragSrc);
    if (!bloomBlurFx_->IsEffectValid() || !bloomCompositeFx_->IsEffectValid()) {
        std::cerr << "[Bloom] Failed to compile shaders\n";
        bloomRtA_.reset(); bloomRtB_.reset();
        bloomBlurFx_.reset(); bloomCompositeFx_.reset();
        bloomFboW_ = bloomFboH_ = 0;
        return;
    }

    bloomFboW_ = w;
    bloomFboH_ = h;
}

void MeshCraftApplication::initSkybox()
{
    if (!supportsTextShaderEffects()) return;
    auto& gd = getGraphicsDeviceProperty();
    skyboxFx_.emplace(gd, kBloomVertSrc, kSkyboxFragSrc);
    if (!skyboxFx_->IsEffectValid()) {
        std::cerr << "[Skybox] Failed to compile shader\n";
        skyboxFx_.reset();
    }
}

void MeshCraftApplication::drawSkybox(const Matrix& view, float fovDegrees, float aspect)
{
    if (!document_.environment || document_.environment->skyboxTexture.empty()) return;
    const std::string& uriRaw = document_.environment->skyboxTexture;
    std::string absPath = document_.sourcePath.empty()
        ? uriRaw
        : (document_.sourcePath / uriRaw).string();

    if (!skyboxFx_) initSkybox();
    if (!skyboxFx_) return;

    // (Re)load texture when path changes
    if (absPath != skyboxTexPath_) {
        skyboxTex_.reset();
        skyboxTexPath_.clear();
        try {
            skyboxTex_.emplace(absPath, getGraphicsDeviceProperty());
            skyboxTexPath_ = absPath;
        } catch (...) {
            std::cerr << "[Skybox] Failed to load: " << absPath << "\n";
            return;
        }
    }
    if (!skyboxTex_) return;

    // Extract camera basis vectors from view matrix (XNA row-major):
    //   row 0 = right, row 1 = up, row 2 = back (-forward)
    float rx = view.M11, ry = view.M12, rz = view.M13;
    float ux = view.M21, uy = view.M22, uz = view.M23;
    float fx = -view.M31, fy = -view.M32, fz = -view.M33;  // forward = -back

    const float pi = std::numbers::pi_v<float>;
    float tanHalfFovY = std::tan(fovDegrees * pi / 180.0f * 0.5f);
    float tanHalfFovX = tanHalfFovY * aspect;

    auto& gd = getGraphicsDeviceProperty();
    gd.SetDepthTestEnabled(false);
    skyboxFx_->Apply();
    skyboxFx_->SetUniformVec3("u_right",   rx, ry, rz);
    skyboxFx_->SetUniformVec3("u_up",      ux, uy, uz);
    skyboxFx_->SetUniformVec3("u_forward", fx, fy, fz);
    skyboxFx_->SetUniformVec2("u_tanFov",  tanHalfFovX, tanHalfFovY);
    // Equirect wraps horizontally at the seam (u=0/1) but must not wrap
    // vertically (v=0/1 are the poles) -- matches the original raw-GL
    // GL_REPEAT(S)/GL_CLAMP_TO_EDGE(T) pair.
    SamplerState skyboxSampler = SamplerState::LinearWrap;
    skyboxSampler.setAddressVProperty(TextureAddressMode::Clamp);
    // The viewport is already set to the clipped 3D-viewport rectangle by
    // Draw() before this call; SpriteBatch's custom-effect draws only size
    // their projection to a bound RenderTarget2D, not a custom Viewport, so
    // (like AUD-084's Bloom composite pass) the destRect for this
    // backbuffer-targeted draw must be window-absolute.
    const auto& vp = gd.getViewportProperty();
    const Rectangle screenRect(vp.getXProperty(), vp.getYProperty(),
                               vp.getWidthProperty(), vp.getHeightProperty());
    spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque,
                        &skyboxSampler, nullptr, nullptr, &*skyboxFx_);
    spriteBatch_->Draw(*skyboxTex_, screenRect, Color::White);
    spriteBatch_->End();
    gd.SetDepthTestEnabled(true);
}

void MeshCraftApplication::applyBloom(
    int vx, int viewY, int vw, int vh,
    const Matrix& view, const Matrix& proj)
{
    if (!bloomRtA_ || !bloomRtB_ || !bloomBlurFx_ || !bloomCompositeFx_ ||
        vw <= 0 || vh <= 0) return;

    auto& gd = getGraphicsDeviceProperty();
    const float iw = 1.0f / static_cast<float>(vw);
    const float ih = 1.0f / static_cast<float>(vh);
    // Blur passes render RT-to-RT, so destRect is in that RT's own local
    // (0,0)-(vw,vh) space (EasyGLSpriteBatchBackend sizes its projection to
    // the bound RenderTarget2D -- Task 1078 in EasyGLGraphicsBackend.cpp).
    const Rectangle fullRect(0, 0, vw, vh);

    // Pass 0: render emissive objects into RT A.
    // NOTE: RenderTarget2D defaults to RenderTargetUsage::DiscardContents, and
    // GraphicsDevice::SetRenderTarget() unconditionally clears a DiscardContents
    // target on every bind (matching real XNA/FNA semantics) -- a second,
    // "defensive" SetRenderTarget(&*bloomRtA_) call here would silently wipe out
    // the emissive draw immediately after making it. Bind exactly once.
    gd.SetRenderTarget(&*bloomRtA_);
    gd.SetDepthTestEnabled(false);
    gd.Clear(Color(0, 0, 0, 0));
    sceneRenderer_->drawEmissivePass(document_, view, proj);

    // Passes 1-8: ping-pong Gaussian blur (4 iterations H+V), same 5-tap
    // weights/offsets as the original raw-GL shader.
    auto blur = [&](RenderTarget2D& src, RenderTarget2D& dst, float dx, float dy) {
        gd.SetRenderTarget(&dst);
        bloomBlurFx_->Apply();
        bloomBlurFx_->SetUniformVec2("u_dir", dx, dy);
        spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque,
                            nullptr, nullptr, nullptr, &*bloomBlurFx_);
        spriteBatch_->Draw(src, fullRect, Color::White);
        spriteBatch_->End();
    };
    for (int i = 0; i < 4; ++i) {
        blur(*bloomRtA_, *bloomRtB_, iw, 0.f);   // horizontal: A → B
        blur(*bloomRtB_, *bloomRtA_, 0.f, ih);   // vertical:   B → A
    }

    // Pass 5: additive composite onto viewport area in the default framebuffer.
    // Unlike the RT-bound blur passes above, EasyGLSpriteBatchBackend::FlushBatch()
    // sizes its projection to the *window* whenever no RenderTarget2D is bound --
    // it does not honor a custom GraphicsDevice.Viewport in that case -- so the
    // destRect here must be given in window-absolute coordinates (vx,viewY,vw,vh),
    // not RT-local (0,0,vw,vh).
    gd.SetRenderTarget(nullptr);
    gd.setViewportProperty(Viewport(vx, viewY, vw, vh));
    const Rectangle screenRect(vx, viewY, vw, vh);
    bloomCompositeFx_->Apply();
    bloomCompositeFx_->SetUniformFloat("u_strength", bloomStrength_);
    spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Additive,
                        nullptr, nullptr, nullptr, &*bloomCompositeFx_);
    spriteBatch_->Draw(*bloomRtA_, screenRect, Color::White);
    spriteBatch_->End();

    // Restore state for the rest of the frame.
    gd.SetDepthTestEnabled(true);
    gd.setViewportProperty(Viewport(0, 0, cachedScreenW_, cachedScreenH_));
}

void MeshCraftApplication::initSsao(int w, int h)
{
    if (!supportsTextShaderEffects() || w <= 0 || h <= 0 || !sceneRenderer_ || !sceneRenderer_->depthPassAvailable()) return;
    auto& gd = getGraphicsDeviceProperty();
    ssaoDepthRt_.emplace(gd, w, h, false, SurfaceFormat::Color, DepthFormat::Depth24);
    ssaoRt_.emplace(gd, w, h);
    ssaoBlurRt_.emplace(gd, w, h);
    ssaoFx_.emplace(gd, kBloomVertSrc, kSsaoCnaFragSrc);
    ssaoBlurFx_.emplace(gd, kBloomVertSrc, kSsaoCnaBlurFragSrc);
    ssaoCompositeFx_.emplace(gd, kBloomVertSrc, kSsaoCnaCompositeFragSrc);
    if (!ssaoFx_->IsEffectValid() || !ssaoBlurFx_->IsEffectValid() || !ssaoCompositeFx_->IsEffectValid()) {
        std::cerr << "[SSAO] Failed to compile CNA shaders\n";
        ssaoDepthRt_.reset(); ssaoRt_.reset(); ssaoBlurRt_.reset();
        ssaoFx_.reset(); ssaoBlurFx_.reset(); ssaoCompositeFx_.reset();
        ssaoFboW_ = ssaoFboH_ = 0;
        return;
    }
    ssaoFboW_ = w;
    ssaoFboH_ = h;
}

void MeshCraftApplication::applySsao(
    int vx, int viewY, int vw, int vh, const Matrix& view, const Matrix& proj,
    float tanHalfFovX, float tanHalfFovY, float nearPlane, float farPlane)
{
    if (!ssaoDepthRt_ || !ssaoRt_ || !ssaoBlurRt_ || !ssaoFx_ || !ssaoBlurFx_ || !ssaoCompositeFx_ ||
        vw <= 0 || vh <= 0 || !sceneRenderer_) return;
    auto& gd = getGraphicsDeviceProperty();
    const Rectangle targetRect(0, 0, vw, vh);
    const Rectangle screenRect(vx, viewY, vw, vh);

    // Pass 1: redraw scene geometry with a depth-writing ShaderEffect into a
    // color target. This is the CNA-supported replacement for reading the
    // default framebuffer's private depth attachment.
    gd.SetRenderTarget(&*ssaoDepthRt_);
    gd.setViewportProperty(Viewport(0, 0, vw, vh));
    gd.Clear(Color::White, 1.0f);
    sceneRenderer_->drawDepthPass(document_, view, proj);

    // Pass 2: depth -> ambient-occlusion mask.
    gd.SetRenderTarget(&*ssaoRt_);
    gd.setViewportProperty(Viewport(0, 0, vw, vh));
    ssaoFx_->Apply();
    ssaoFx_->SetUniformVec2("u_tanHalfFov", tanHalfFovX, tanHalfFovY);
    ssaoFx_->SetUniformFloat("u_near", nearPlane);
    ssaoFx_->SetUniformFloat("u_far", farPlane);
    ssaoFx_->SetUniformFloat("u_radius", ssaoRadius_);
    spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque, nullptr, nullptr, nullptr, &*ssaoFx_);
    spriteBatch_->Draw(*ssaoDepthRt_, targetRect, Color::White);
    spriteBatch_->End();

    // Pass 3: denoise the AO field.
    gd.SetRenderTarget(&*ssaoBlurRt_);
    gd.setViewportProperty(Viewport(0, 0, vw, vh));
    ssaoBlurFx_->Apply();
    ssaoBlurFx_->SetUniformVec2("u_texelSize", 1.0f / static_cast<float>(vw), 1.0f / static_cast<float>(vh));
    spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque, nullptr, nullptr, nullptr, &*ssaoBlurFx_);
    spriteBatch_->Draw(*ssaoRt_, targetRect, Color::White);
    spriteBatch_->End();

    // Pass 4: multiplicative composite in the editor viewport only.
    BlendState multiply;
    multiply.setColorSourceBlendProperty(Blend::DestinationColor);
    multiply.setColorDestinationBlendProperty(Blend::Zero);
    multiply.setAlphaSourceBlendProperty(Blend::One);
    multiply.setAlphaDestinationBlendProperty(Blend::Zero);
    gd.SetRenderTarget(nullptr);
    gd.setViewportProperty(Viewport(vx, viewY, vw, vh));
    gd.SetDepthTestEnabled(false);
    ssaoCompositeFx_->Apply();
    ssaoCompositeFx_->SetUniformFloat("u_strength", ssaoStrength_);
    spriteBatch_->Begin(SpriteSortMode::Deferred, multiply, nullptr, nullptr, nullptr, &*ssaoCompositeFx_);
    spriteBatch_->Draw(*ssaoBlurRt_, screenRect, Color::White);
    spriteBatch_->End();
    gd.SetDepthTestEnabled(true);
    gd.setViewportProperty(Viewport(0, 0, cachedScreenW_, cachedScreenH_));
}

void MeshCraftApplication::initShadowDebug()
{
    auto& gd = getGraphicsDeviceProperty();
    shadowDebugRt_.emplace(gd, kShadowDebugRes, kShadowDebugRes,
                           /*mipMap=*/false, SurfaceFormat::Color, DepthFormat::Depth24);
}

// ---------------------------------------------------------------------------
// Destructor — deterministic teardown of everything LoadContent() set up.
//
// Runs when `app` goes out of scope in main() (after Run() returns), which is
// before the base Game destructor, so the SDL window and its graphics context
// are still valid here. Ordering mirrors LoadContent in reverse.
// ---------------------------------------------------------------------------
MeshCraftApplication::~MeshCraftApplication() {
    // SYS-W9-07: an ordinary (non-crash) shutdown reaches this destructor,
    // which is exactly the signal used to distinguish "the user closed the
    // app" from "the app crashed" -- a crash skips this entirely, leaving
    // the file for the next startup's checkForUntitledRecovery() to find.
    // Removed unconditionally whenever the document is still untitled,
    // regardless of `modified_`: there is no quit-confirmation gate in this
    // app, so reaching a clean exit while modified already means the user
    // chose to close without saving.
    if (currentFile_.empty()) {
        std::error_code ec;
        std::filesystem::remove(untitledRecoveryPath(), ec);
    }

    // Remove the event watch first so the callback can never fire against this
    // half-destroyed object. Harmless if it was never added.
    SDL_RemoveEventWatch(reinterpret_cast<SDL_EventFilter>(sdlEventWatch), this);

    if (imguiInitialized_) {
        imguiRenderer_->shutdown();
        imguiRenderer_.reset();
        ImGui::DestroyContext();
        imguiInitialized_ = false;
    }

    // AUD-088: shadowDebugRt_ (RenderTarget2D) is released automatically via
    // its own destructor (implicit member cleanup, after this body runs) --
    // no manual glDelete* call needed here anymore, matching bloomRtA_/
    // bloomRtB_/skyboxTex_/matPreviewRt_'s own established pattern.

}

void MeshCraftApplication::renderShadowDebugFbo(const Matrix& lightView, const Matrix& lightProj)
{
    if (!shadowDebugRt_) return;
    auto& gd = getGraphicsDeviceProperty();

    gd.SetRenderTarget(&*shadowDebugRt_);
    gd.Clear(Color(102, 102, 128, 255), 1.0f);
    gd.SetDepthTestEnabled(true);

    sceneRenderer_->draw(document_, lightView, lightProj, {});

    if (!shadowDebugTextureToken_)
        shadowDebugTextureToken_ = imguiRenderer_->registerTexture(*shadowDebugRt_);

    // SetRenderTarget(nullptr) already resets Viewport/ScissorRectangle to
    // the full backbuffer size on its own -- see GraphicsDevice.cpp's own
    // documented behavior (matches FNA), no separate restore call needed.
    gd.SetRenderTarget(nullptr);
}

// ---------------------------------------------------------------------------
// D7: Material preview sphere (128×128 FBO, SDF Blinn-Phong shader)
// ---------------------------------------------------------------------------
void MeshCraftApplication::initMatPreview() {
    if (!supportsTextShaderEffects() || matPreviewRt_) return;  // already initialised/unavailable

    auto& gd = getGraphicsDeviceProperty();
    matPreviewRt_.emplace(gd, kMatPreviewRes, kMatPreviewRes);
    matPreviewFx_.emplace(gd, kBloomVertSrc, kMatPreviewFragSrc);
    const std::vector<std::uint8_t> white{255, 255, 255, 255};
    matPreviewDummyTex_ = Texture2D::CreateFromPixels(gd, 1, 1, white);
    if (!matPreviewFx_->IsEffectValid()) {
        matPreviewRt_.reset();
        matPreviewFx_.reset();
        matPreviewDummyTex_.reset();
    }
}

void MeshCraftApplication::renderMatPreview(float r, float g, float b,
                                             float roughness, float metallic) {
    if (!matPreviewRt_ || !matPreviewFx_ || !matPreviewDummyTex_) return;

    auto& gd = getGraphicsDeviceProperty();
    gd.SetRenderTarget(&*matPreviewRt_);
    gd.SetDepthTestEnabled(false);
    gd.Clear(Color(46, 46, 46, 255));  // 0.18 srgb-ish, matches the original ClearColor
    matPreviewFx_->Apply();
    matPreviewFx_->SetUniformVec3("u_color", r, g, b);
    matPreviewFx_->SetUniformFloat("u_roughness", roughness);
    matPreviewFx_->SetUniformFloat("u_metallic", metallic);
    const Rectangle fullRect(0, 0, kMatPreviewRes, kMatPreviewRes);
    spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque,
                        nullptr, nullptr, nullptr, &*matPreviewFx_);
    spriteBatch_->Draw(*matPreviewDummyTex_, fullRect, Color::White);
    spriteBatch_->End();
    gd.SetRenderTarget(nullptr);
    gd.setViewportProperty(Viewport(0, 0, cachedScreenW_, cachedScreenH_));
    gd.SetDepthTestEnabled(true);
    if (!matPreviewTextureToken_)
        matPreviewTextureToken_ = imguiRenderer_->registerTexture(*matPreviewRt_);
}

} // namespace MeshCraft::Application
