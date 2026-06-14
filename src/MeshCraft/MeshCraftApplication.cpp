#include "MeshCraft/MeshCraftApplication.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <SDL3/SDL.h>

#include <Microsoft/Xna/Framework/Input/Keyboard.hpp>
#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/Mouse.hpp>
#include <Microsoft/Xna/Framework/Input/ButtonState.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
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
#include <cstring>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include "MeshCraftPrivate.hpp"

namespace MeshCraft {

GetTypeNameCPP(MeshCraftApplication, "MeshCraft::MeshCraftApplication")

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

// ---------------------------------------------------------------------------
// SDL event watcher — forwards each event to ImGui before CNA processes it
// ---------------------------------------------------------------------------

bool MeshCraftApplication::sdlEventWatch(void* userdata, void* eventPtr) {
    auto* ev = static_cast<SDL_Event*>(eventPtr);
    ImGui_ImplSDL3_ProcessEvent(ev);
    if (userdata && ev->type == SDL_EVENT_DROP_FILE && ev->drop.data) {
        static_cast<MeshCraftApplication*>(userdata)->pendingDropFile_ = ev->drop.data;
    }
    return true;
}

// ---------------------------------------------------------------------------
// LoadContent
// ---------------------------------------------------------------------------

void MeshCraftApplication::LoadContent() {
    auto& gd = getGraphicsDeviceProperty();

    gridRenderer_  = std::make_unique<Renderer::GridRenderer>(gd);
    sceneRenderer_ = std::make_unique<Renderer::SceneRenderer>(gd);

    hierarchyPanel_  = std::make_unique<Scene::SceneHierarchyPanel>(document_);
    propertiesPanel_ = std::make_unique<Scene::PropertiesPanel>();

    // Load GL function pointers for direct viewport/scissor control
    fnGlViewport_ = reinterpret_cast<void(*)(int,int,int,int)>(SDL_GL_GetProcAddress("glViewport"));
    fnGlScissor_  = reinterpret_cast<void(*)(int,int,int,int)>(SDL_GL_GetProcAddress("glScissor"));
    fnGlEnable_   = reinterpret_cast<void(*)(unsigned int)>   (SDL_GL_GetProcAddress("glEnable"));
    fnGlDisable_  = reinterpret_cast<void(*)(unsigned int)>   (SDL_GL_GetProcAddress("glDisable"));

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    SDL_Window*    sdlWindow = reinterpret_cast<SDL_Window*>(getWindowProperty().getHandleProperty());
    SDL_GLContext  glCtx     = SDL_GL_GetCurrentContext();
    ImGui_ImplSDL3_InitForOpenGL(sdlWindow, glCtx);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    SDL_AddEventWatch(reinterpret_cast<SDL_EventFilter>(sdlEventWatch), this);

    loadRecentFiles();

    if (!currentFile_.empty() && std::filesystem::exists(currentFile_)) {
        try {
            document_ = Mc3::Mc3Document::loadFromFile(currentFile_);
            addRecentFile(currentFile_);
            std::cout << "[MeshCraft] Loaded: " << currentFile_ << "\n";
            // Warn if a newer autosave exists (unsaved crash recovery hint)
            auto asPath = autoSavePath(currentFile_);
            std::error_code ec;
            if (std::filesystem::exists(asPath, ec)) {
                auto savedTime = std::filesystem::last_write_time(currentFile_,  ec);
                auto asTime    = std::filesystem::last_write_time(asPath, ec);
                if (asTime > savedTime)
                    setStatusMsg("Autosave found — may be newer than saved file: " +
                                 asPath.filename().string(), true, 8.0f);
            }
        } catch (const std::exception& e) {
            std::cerr << "[MeshCraft] Failed to load file: " << e.what() << "\n";
        }
    } else {
        newScene();
    }

    updateWindowTitle();
}

// ---------------------------------------------------------------------------
// BeginDraw / EndDraw — ImGui frame lifecycle
// ---------------------------------------------------------------------------

bool MeshCraftApplication::BeginDraw() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    return Game::BeginDraw();
}

void MeshCraftApplication::EndDraw() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (pendingScreenshot_) {
        pendingScreenshot_ = false;
        saveScreenshot(autoScreenshotPath_);
        std::cout << "[MeshCraft] Auto-screenshot saved to: " << autoScreenshotPath_ << "\n";
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
        if (statusMsgTimer_ > 0) statusMsgTimer_ -= dt;

        if (!currentFile_.empty() && modified_) {
            autoSaveCountdown_ -= dt;
            if (autoSaveCountdown_ <= 0.0f) {
                performAutoSave();
                autoSaveCountdown_ = 60.0f;
            }
        } else {
            autoSaveCountdown_ = 60.0f;
        }
    }

    // Advance animation clock
    if (animPlaying_ && !currentActionName_.empty()) {
        auto it = document_.actions.find(currentActionName_);
        if (it != document_.actions.end()) {
            float dt = static_cast<float>(
                gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty());
            animTime_ += dt;
            float dur = it->second.duration;
            if (animTime_ >= dur) {
                if (it->second.loop) animTime_ = std::fmod(animTime_, dur);
                else { animTime_ = dur; animPlaying_ = false; }
            }
            evaluateAndPushAnimOverrides();
        } else {
            animPlaying_ = false;
        }
    }

    // Consume dropped file path (set by SDL event watcher)
    if (!pendingDropFile_.empty()) {
        std::filesystem::path dropPath = std::move(pendingDropFile_);
        pendingDropFile_.clear();
        if (dropPath.extension() == ".xml" || dropPath.string().find(".mc3") != std::string::npos)
            confirmIfModified(PendingAction::OpenRecentFile, dropPath);
        else
            setStatusMsg("Unsupported file type: " + dropPath.filename().string(), true, 3.0f);
    }

    auto ks = Keyboard::GetState();
    auto ms = Mouse::GetState();

    if (!firstFrame_) {
        auto& io = ImGui::GetIO();
        if (!io.WantCaptureKeyboard)
            handleKeyboardShortcuts(ks, prevKs_);
        if (!io.WantCaptureMouse)
            handleMouseInput(ms, prevMouse_);
    }

    firstFrame_ = false;
    prevKs_     = ks;
    prevMouse_  = ms;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void MeshCraftApplication::Draw(const GameTime& /*gameTime*/) {
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

    constexpr unsigned int GL_SCISSOR_TEST = 0x0C11;
    int glViewY = screenH - viewY - viewH;

    if (fnGlEnable_)  fnGlEnable_(GL_SCISSOR_TEST);
    if (fnGlScissor_) fnGlScissor_(viewX, glViewY, viewW, viewH);

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

    if (fnGlViewport_) fnGlViewport_(viewX, glViewY, viewW, viewH);

    float aspect = (viewH > 0) ? static_cast<float>(viewW) / viewH : 16.0f / 9.0f;
    Matrix view = camera_.viewMatrix();
    Matrix proj = camera_.projectionMatrix(aspect);

    // Look-through-camera mode: override view/proj from selected Mc3Camera
    if (lookThroughCamera_ && selectedCameraIdx_ >= 0 &&
        selectedCameraIdx_ < static_cast<int>(document_.cameras.size())) {
        const auto& cam = document_.cameras[selectedCameraIdx_];
        const float pi = std::numbers::pi_v<float>;
        Vector3 camPos(cam.position[0], cam.position[1], cam.position[2]);
        Vector3 camTarget(cam.target[0], cam.target[1], cam.target[2]);
        Vector3 up(0.0f, 1.0f, 0.0f);
        view = Matrix::CreateLookAt(camPos, camTarget, up);
        if (cam.type == Mc3::CameraType::Orthographic) {
            float hw = cam.orthoSize * aspect;
            proj = Matrix::CreateOrthographic(hw * 2.0f, cam.orthoSize * 2.0f,
                                               cam.nearPlane, cam.farPlane);
        } else {
            float fovRad = cam.fov * pi / 180.0f;
            proj = Matrix::CreatePerspectiveFieldOfView(fovRad, aspect,
                                                         cam.nearPlane, cam.farPlane);
        }
    } else {
        lookThroughCamera_ = false; // auto-clear if camera removed
    }

    // Cache for measurement overlay projection
    cachedVP_ = view * proj;
    cachedVX_ = viewX; cachedVY_ = viewY; cachedVW_ = viewW; cachedVH_ = viewH;

    gd.SetDepthTestEnabled(false);
    gridRenderer_->draw(view, proj);

    gd.SetDepthTestEnabled(true);
    auto selPtrs = selectedPointers();
    if (!showWireframeMode_)
        sceneRenderer_->draw(document_, view, proj, selPtrs);

    if (showEdgeOverlay_ || showWireframeMode_) {
        gd.SetDepthTestEnabled(true);
        sceneRenderer_->drawEdgeOverlay(document_, view, proj);
    }

    // Scene-level gizmos (lights / cameras)
    gd.SetDepthTestEnabled(false);
    sceneRenderer_->drawLightGizmos(document_.lights, view, proj);
    sceneRenderer_->drawCameraGizmos(document_.cameras, view, proj);
    sceneRenderer_->drawCsgGizmos(document_, view, proj);
    gd.SetDepthTestEnabled(true);

    if (selection_.hasSelection()) {
        float gizmoLen = camera_.distance * 0.15f;
        auto* sel0 = selection_.selection().front().get();
        if (activeTool_ == ActiveTool::Move) {
            gd.SetDepthTestEnabled(false);
            sceneRenderer_->drawGizmo(sel0, view, proj, gizmoLen, gizmoLocalSpace_);
        } else if (activeTool_ == ActiveTool::Scale) {
            gd.SetDepthTestEnabled(false);
            sceneRenderer_->drawScaleGizmo(sel0, view, proj, gizmoLen, gizmoLocalSpace_);
        } else if (activeTool_ == ActiveTool::Rotate) {
            gd.SetDepthTestEnabled(false);
            sceneRenderer_->drawRotateGizmo(sel0, view, proj, gizmoLen, gizmoLocalSpace_);
        }

        // Bounding box overlay (cyan wire box for each selected object)
        if (showBoundingBox_) {
            gd.SetDepthTestEnabled(false);
            Color bboxColor(80, 220, 255, 200);
            for (const auto& selObj : selection_.selection())
                sceneRenderer_->drawObjectWireframe(*selObj, view, proj, bboxColor);
        }
    }

    // Locked-object outline (red wireframe around every locked object)
    if (!lockedIds_.empty()) {
        gd.SetDepthTestEnabled(false);
        Color lockColor(220, 60, 60, 180);
        std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> drawLocked;
        drawLocked = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
            for (const auto& obj : list) {
                if (lockedIds_.count(obj->id))
                    sceneRenderer_->drawObjectWireframe(*obj, view, proj, lockColor);
                if (!obj->children.empty()) drawLocked(obj->children);
            }
        };
        drawLocked(document_.objects);
    }

    if (fnGlDisable_)  fnGlDisable_(GL_SCISSOR_TEST);
    if (fnGlViewport_) fnGlViewport_(0, 0, screenW, screenH);

    Graphics::Viewport vpReset;
    vpReset.x = 0; vpReset.y = 0;
    vpReset.setWidthProperty(screenW);
    vpReset.setHeightProperty(screenH);
    gd.setViewportProperty(vpReset);

    // Box-select rectangle overlay drawn via ImGui in drawImGuiUi()
    gd.SetDepthTestEnabled(false);
    drawImGuiUi(screenW, screenH);

    if (autoScreenshotCountdown_ > 0) {
        --autoScreenshotCountdown_;
        if (autoScreenshotCountdown_ == 0 && !autoScreenshotPath_.empty())
            pendingScreenshot_ = true;
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

    drawStatsOverlay(screenW, screenH);
    drawStatusBar(screenW, screenH);
    drawPanelSplitters(screenW, screenH);
    drawDialogs();
}

} // namespace MeshCraft
