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

    SDL_Window*    sdlWindow = reinterpret_cast<SDL_Window*>(getWindowProperty().getHandleProperty());
    SDL_GLContext  glCtx     = SDL_GL_GetCurrentContext();
    ImGui_ImplSDL3_InitForOpenGL(sdlWindow, glCtx);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    SDL_AddEventWatch(reinterpret_cast<SDL_EventFilter>(sdlEventWatch), this);

    // F7: hide window in screenshot/headless mode
    if (!autoScreenshotPath_.empty())
        SDL_HideWindow(sdlWindow);

    loadRecentFiles();

    if (!currentFile_.empty() && std::filesystem::exists(currentFile_)) {
        try {
            document_ = Mc3::Mc3Document::loadFromFile(currentFile_);
            addRecentFile(currentFile_);
            std::cout << "[MeshCraft] Loaded: " << currentFile_ << "\n";
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

        if (!currentFile_.empty() && modified_ && autoSaveInterval_ > 0.0f) {
            autoSaveCountdown_ -= dt;
            if (autoSaveCountdown_ <= 0.0f) {
                performAutoSave();
                autoSaveCountdown_ = autoSaveInterval_;
            }
        } else {
            autoSaveCountdown_ = autoSaveInterval_ > 0.0f ? autoSaveInterval_ : 60.0f;
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

    // Consume dropped texture image (D6)
    if (!pendingDropTexture_.empty()) {
        std::string texPath = std::move(pendingDropTexture_);
        pendingDropTexture_.clear();
        if (!hoveredTexSlot_.empty() && !hoveredTexMatId_.empty() &&
            document_.materials.count(hoveredTexMatId_)) {
            // Assign directly to the hovered slot
            pushUndo();
            auto& mat = document_.materials[hoveredTexMatId_];
            if      (hoveredTexSlot_ == "base")       mat.baseColorTexture         = texPath;
            else if (hoveredTexSlot_ == "normal")     mat.normalTexture            = texPath;
            else if (hoveredTexSlot_ == "emissive")   mat.emissiveTexture          = texPath;
            else if (hoveredTexSlot_ == "metalrough") mat.metallicRoughnessTexture = texPath;
            else if (hoveredTexSlot_ == "occlusion")  mat.occlusionTexture         = texPath;
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

        if (walkModeEnabled_) {
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

    // Walk mode: override view with first-person camera
    if (walkModeEnabled_) {
        const float pi = std::numbers::pi_v<float>;
        float cosP = std::cos(walkPitch_), sinP = std::sin(walkPitch_);
        float sinY = std::sin(walkYaw_),   cosY = std::cos(walkYaw_);
        float eyeX = walkPosX_, eyeY = walkPosY_ + walkHeight_, eyeZ = walkPosZ_;
        Vector3 eye(eyeX, eyeY, eyeZ);
        Vector3 target(eyeX + sinY * cosP, eyeY + sinP, eyeZ - cosY * cosP);
        Vector3 up(0.0f, 1.0f, 0.0f);
        view = Matrix::CreateLookAt(eye, target, up);
        float fovRad = camera_.fovDegrees * pi / 180.0f;
        proj = Matrix::CreatePerspectiveFieldOfView(fovRad, aspect,
                                                     camera_.nearPlane, camera_.farPlane);
    }

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
            if (pivotEditMode_) {
                // Render gizmo at position + pivot
                Mc3::Mc3Object pivProxy = *sel0;
                for (int i = 0; i < 3; ++i) {
                    pivProxy.transform.position[i] += pivProxy.transform.pivot[i];
                    pivProxy.transform.pivot[i] = 0.0f;
                }
                sceneRenderer_->drawGizmo(&pivProxy, view, proj, gizmoLen, gizmoLocalSpace_);
            } else {
                sceneRenderer_->drawGizmo(sel0, view, proj, gizmoLen, gizmoLocalSpace_);
            }
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

    // Bloom post-process (I6) — additive emissive glow
    if (bloomEnabled_) {
        if (bloomFboW_ != viewW || bloomFboH_ != viewH)
            initBloom(viewW, viewH);
        applyBloom(viewX, glViewY, viewW, viewH, view, proj);
    }

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

    if (walkModeEnabled_)
        drawWalkModeHud(screenW, screenH);
    else
        drawStatsOverlay(screenW, screenH);
    drawStatusBar(screenW, screenH);
    if (!walkModeEnabled_)
        drawPanelSplitters(screenW, screenH);
    drawDialogs();
}

// ---------------------------------------------------------------------------
// Bloom post-processing (I6) — FBO + blur + additive composite
// ---------------------------------------------------------------------------
// GL infrastructure lives in a TU-private anonymous namespace to avoid
// polluting the MeshCraft namespace with raw GL types.
} // namespace MeshCraft — temporarily closed so anonymous ns is file-scope

namespace {

// GL constants (no GL headers to avoid conflicts with CNA/XNA GL headers)
constexpr unsigned kGL_FRAMEBUFFER          = 0x8D40u;
constexpr unsigned kGL_COLOR_ATTACHMENT0    = 0x8CE0u;
constexpr unsigned kGL_TEXTURE_2D           = 0x0DE1u;
constexpr unsigned kGL_TEXTURE0             = 0x84C0u;
constexpr unsigned kGL_RGBA                 = 0x1908u;
constexpr unsigned kGL_RGBA8                = 0x8058u;
constexpr unsigned kGL_UNSIGNED_BYTE        = 0x1401u;
constexpr unsigned kGL_LINEAR               = 0x2601u;
constexpr unsigned kGL_CLAMP_TO_EDGE        = 0x812Fu;
constexpr unsigned kGL_TEXTURE_MIN_FILTER   = 0x2801u;
constexpr unsigned kGL_TEXTURE_MAG_FILTER   = 0x2800u;
constexpr unsigned kGL_TEXTURE_WRAP_S       = 0x2802u;
constexpr unsigned kGL_TEXTURE_WRAP_T       = 0x2803u;
constexpr unsigned kGL_FRAMEBUFFER_COMPLETE = 0x8CD5u;
constexpr unsigned kGL_VERTEX_SHADER        = 0x8B31u;
constexpr unsigned kGL_FRAGMENT_SHADER      = 0x8B30u;
constexpr unsigned kGL_ARRAY_BUFFER         = 0x8892u;
constexpr unsigned kGL_STATIC_DRAW          = 0x88B4u;
constexpr unsigned kGL_FLOAT                = 0x1406u;
constexpr unsigned kGL_TRIANGLE_STRIP       = 0x0005u;
constexpr unsigned kGL_ONE                  = 0x0001u;
constexpr unsigned kGL_SRC_ALPHA            = 0x0302u;
constexpr unsigned kGL_ONE_MINUS_SRC_ALPHA  = 0x0303u;
constexpr unsigned kGL_FUNC_ADD             = 0x8006u;
constexpr unsigned kGL_BLEND                = 0x0BE2u;
constexpr unsigned kGL_DEPTH_TEST           = 0x0B71u;
constexpr unsigned kGL_COLOR_BUFFER_BIT     = 0x4000u;
constexpr unsigned kGL_LINK_STATUS          = 0x8B82u;
constexpr unsigned kGL_COMPILE_STATUS       = 0x8B81u;

struct BloomGL {
    void     (*GenFramebuffers)(int, unsigned*) = nullptr;
    void     (*BindFramebuffer)(unsigned, unsigned) = nullptr;
    void     (*FramebufferTexture2D)(unsigned, unsigned, unsigned, unsigned, int) = nullptr;
    unsigned (*CheckFramebufferStatus)(unsigned) = nullptr;
    void     (*DeleteFramebuffers)(int, const unsigned*) = nullptr;
    void     (*GenTextures)(int, unsigned*) = nullptr;
    void     (*BindTexture)(unsigned, unsigned) = nullptr;
    void     (*TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*) = nullptr;
    void     (*TexParameteri)(unsigned, unsigned, int) = nullptr;
    void     (*DeleteTextures)(int, const unsigned*) = nullptr;
    void     (*ActiveTexture)(unsigned) = nullptr;
    void     (*GenVertexArrays)(int, unsigned*) = nullptr;
    void     (*BindVertexArray)(unsigned) = nullptr;
    void     (*DeleteVertexArrays)(int, const unsigned*) = nullptr;
    void     (*GenBuffers)(int, unsigned*) = nullptr;
    void     (*BindBuffer)(unsigned, unsigned) = nullptr;
    void     (*BufferData)(unsigned, long, const void*, unsigned) = nullptr;
    void     (*DeleteBuffers)(int, const unsigned*) = nullptr;
    void     (*EnableVertexAttribArray)(unsigned) = nullptr;
    void     (*VertexAttribPointer)(unsigned, int, unsigned, unsigned char, int, const void*) = nullptr;
    unsigned (*CreateShader)(unsigned) = nullptr;
    void     (*ShaderSource)(unsigned, int, const char* const*, const int*) = nullptr;
    void     (*CompileShader)(unsigned) = nullptr;
    void     (*GetShaderiv)(unsigned, unsigned, int*) = nullptr;
    void     (*DeleteShader)(unsigned) = nullptr;
    unsigned (*CreateProgram)() = nullptr;
    void     (*AttachShader)(unsigned, unsigned) = nullptr;
    void     (*LinkProgram)(unsigned) = nullptr;
    void     (*GetProgramiv)(unsigned, unsigned, int*) = nullptr;
    void     (*DeleteProgram)(unsigned) = nullptr;
    void     (*UseProgram)(unsigned) = nullptr;
    int      (*GetUniformLocation)(unsigned, const char*) = nullptr;
    void     (*Uniform1i)(int, int) = nullptr;
    void     (*Uniform1f)(int, float) = nullptr;
    void     (*Uniform2f)(int, float, float) = nullptr;
    void     (*BlendFunc)(unsigned, unsigned) = nullptr;
    void     (*BlendEquation)(unsigned) = nullptr;
    void     (*DrawArrays)(unsigned, int, int) = nullptr;
    void     (*ClearColor)(float, float, float, float) = nullptr;
    void     (*Clear)(unsigned) = nullptr;
    void     (*Viewport)(int, int, int, int) = nullptr;
    void     (*Enable)(unsigned) = nullptr;
    void     (*Disable)(unsigned) = nullptr;

    unsigned fboA{0}, fboB{0};
    unsigned texA{0}, texB{0};
    unsigned progBlur{0}, progComposite{0};
    unsigned quadVAO{0}, quadVBO{0};
    bool     fnLoaded{false};
    bool     ready{false};

    bool loadFunctions() {
        if (fnLoaded) return true;
#define LD(m,n) m = reinterpret_cast<decltype(m)>(SDL_GL_GetProcAddress(n))
        LD(GenFramebuffers,        "glGenFramebuffers");
        LD(BindFramebuffer,        "glBindFramebuffer");
        LD(FramebufferTexture2D,   "glFramebufferTexture2D");
        LD(CheckFramebufferStatus, "glCheckFramebufferStatus");
        LD(DeleteFramebuffers,     "glDeleteFramebuffers");
        LD(GenTextures,            "glGenTextures");
        LD(BindTexture,            "glBindTexture");
        LD(TexImage2D,             "glTexImage2D");
        LD(TexParameteri,          "glTexParameteri");
        LD(DeleteTextures,         "glDeleteTextures");
        LD(ActiveTexture,          "glActiveTexture");
        LD(GenVertexArrays,        "glGenVertexArrays");
        LD(BindVertexArray,        "glBindVertexArray");
        LD(DeleteVertexArrays,     "glDeleteVertexArrays");
        LD(GenBuffers,             "glGenBuffers");
        LD(BindBuffer,             "glBindBuffer");
        LD(BufferData,             "glBufferData");
        LD(DeleteBuffers,          "glDeleteBuffers");
        LD(EnableVertexAttribArray,"glEnableVertexAttribArray");
        LD(VertexAttribPointer,    "glVertexAttribPointer");
        LD(CreateShader,           "glCreateShader");
        LD(ShaderSource,           "glShaderSource");
        LD(CompileShader,          "glCompileShader");
        LD(GetShaderiv,            "glGetShaderiv");
        LD(DeleteShader,           "glDeleteShader");
        LD(CreateProgram,          "glCreateProgram");
        LD(AttachShader,           "glAttachShader");
        LD(LinkProgram,            "glLinkProgram");
        LD(GetProgramiv,           "glGetProgramiv");
        LD(DeleteProgram,          "glDeleteProgram");
        LD(UseProgram,             "glUseProgram");
        LD(GetUniformLocation,     "glGetUniformLocation");
        LD(Uniform1i,              "glUniform1i");
        LD(Uniform1f,              "glUniform1f");
        LD(Uniform2f,              "glUniform2f");
        LD(BlendFunc,              "glBlendFunc");
        LD(BlendEquation,          "glBlendEquation");
        LD(DrawArrays,             "glDrawArrays");
        LD(ClearColor,             "glClearColor");
        LD(Clear,                  "glClear");
        LD(Viewport,               "glViewport");
        LD(Enable,                 "glEnable");
        LD(Disable,                "glDisable");
#undef LD
        fnLoaded = GenFramebuffers && DrawArrays && UseProgram && CreateShader;
        if (!fnLoaded) std::cerr << "[Bloom] Failed to load GL functions\n";
        return fnLoaded;
    }

    unsigned compileShader(unsigned type, const char* src) {
        unsigned s = CreateShader(type);
        const char* srcs[] = { src };
        ShaderSource(s, 1, srcs, nullptr);
        CompileShader(s);
        int ok = 0; GetShaderiv(s, kGL_COMPILE_STATUS, &ok);
        if (!ok) { std::cerr << "[Bloom] Shader compile error\n"; DeleteShader(s); return 0; }
        return s;
    }

    unsigned makeProgram(const char* vsrc, const char* fsrc) {
        unsigned vs = compileShader(kGL_VERTEX_SHADER, vsrc);
        unsigned fs = compileShader(kGL_FRAGMENT_SHADER, fsrc);
        if (!vs || !fs) { if (vs) DeleteShader(vs); if (fs) DeleteShader(fs); return 0; }
        unsigned p = CreateProgram();
        AttachShader(p, vs); AttachShader(p, fs);
        LinkProgram(p);
        DeleteShader(vs); DeleteShader(fs);
        int ok = 0; GetProgramiv(p, kGL_LINK_STATUS, &ok);
        if (!ok) { std::cerr << "[Bloom] Program link error\n"; DeleteProgram(p); return 0; }
        return p;
    }

    void cleanup() {
        if (fboA)        { DeleteFramebuffers(1, &fboA);    fboA = 0; }
        if (fboB)        { DeleteFramebuffers(1, &fboB);    fboB = 0; }
        if (texA)        { DeleteTextures(1, &texA);         texA = 0; }
        if (texB)        { DeleteTextures(1, &texB);         texB = 0; }
        if (progBlur)      { DeleteProgram(progBlur);       progBlur = 0; }
        if (progComposite) { DeleteProgram(progComposite);  progComposite = 0; }
        if (quadVAO) { DeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
        if (quadVBO) { DeleteBuffers(1, &quadVBO);       quadVBO = 0; }
        ready = false;
    }
};
BloomGL s_bloom;

const char* kBloomVS = R"(#version 300 es
layout(location = 0) in vec2 a_pos;
out vec2 v_uv;
void main() {
    v_uv = a_pos * 0.5 + 0.5;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";
const char* kBloomBlurFS = R"(#version 300 es
precision mediump float;
uniform sampler2D u_tex;
uniform vec2 u_dir;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec4 c  = texture(u_tex, v_uv)                          * 0.22702702;
    c += texture(u_tex, v_uv + u_dir *  1.38461538) * 0.31621621;
    c += texture(u_tex, v_uv - u_dir *  1.38461538) * 0.31621621;
    c += texture(u_tex, v_uv + u_dir *  3.23076923) * 0.07027027;
    c += texture(u_tex, v_uv - u_dir *  3.23076923) * 0.07027027;
    fragColor = c;
}
)";
const char* kBloomCompositeFS = R"(#version 300 es
precision mediump float;
uniform sampler2D u_tex;
uniform float u_strength;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    fragColor = vec4(texture(u_tex, v_uv).rgb * u_strength, 1.0);
}
)";

} // anonymous namespace

namespace MeshCraft {

void MeshCraftApplication::initBloom(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    auto& gl = s_bloom;
    if (!gl.loadFunctions()) return;
    gl.cleanup();

    gl.GenTextures(1, &gl.texA);
    gl.GenTextures(1, &gl.texB);
    for (unsigned t : { gl.texA, gl.texB }) {
        gl.BindTexture(kGL_TEXTURE_2D, t);
        gl.TexImage2D(kGL_TEXTURE_2D, 0, (int)kGL_RGBA8, w, h, 0,
                      kGL_RGBA, kGL_UNSIGNED_BYTE, nullptr);
        gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, (int)kGL_LINEAR);
        gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, (int)kGL_LINEAR);
        gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S,     (int)kGL_CLAMP_TO_EDGE);
        gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T,     (int)kGL_CLAMP_TO_EDGE);
    }
    gl.BindTexture(kGL_TEXTURE_2D, 0);

    gl.GenFramebuffers(1, &gl.fboA);
    gl.GenFramebuffers(1, &gl.fboB);
    for (auto [fbo, tex] : { std::pair{gl.fboA, gl.texA}, std::pair{gl.fboB, gl.texB} }) {
        gl.BindFramebuffer(kGL_FRAMEBUFFER, fbo);
        gl.FramebufferTexture2D(kGL_FRAMEBUFFER, kGL_COLOR_ATTACHMENT0,
                                kGL_TEXTURE_2D, tex, 0);
        if (gl.CheckFramebufferStatus(kGL_FRAMEBUFFER) != kGL_FRAMEBUFFER_COMPLETE)
            std::cerr << "[Bloom] FBO incomplete\n";
    }
    gl.BindFramebuffer(kGL_FRAMEBUFFER, 0);

    gl.progBlur      = gl.makeProgram(kBloomVS, kBloomBlurFS);
    gl.progComposite = gl.makeProgram(kBloomVS, kBloomCompositeFS);
    if (!gl.progBlur || !gl.progComposite) { gl.cleanup(); return; }

    gl.GenVertexArrays(1, &gl.quadVAO);
    gl.BindVertexArray(gl.quadVAO);
    gl.GenBuffers(1, &gl.quadVBO);
    gl.BindBuffer(kGL_ARRAY_BUFFER, gl.quadVBO);
    const float quad[] = { -1.f, 1.f,  -1.f, -1.f,  1.f, 1.f,  1.f, -1.f };
    gl.BufferData(kGL_ARRAY_BUFFER, (long)sizeof(quad), quad, kGL_STATIC_DRAW);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 2, kGL_FLOAT, 0, 8, nullptr);
    gl.BindVertexArray(0);
    gl.BindBuffer(kGL_ARRAY_BUFFER, 0);

    bloomFboW_ = w;
    bloomFboH_ = h;
    gl.ready   = true;
}

void MeshCraftApplication::applyBloom(
    int vx, int glViewY, int vw, int vh,
    const Matrix& view, const Matrix& proj)
{
    auto& gl = s_bloom;
    if (!gl.ready || vw <= 0 || vh <= 0) return;

    // Step = 2 screen pixels so blur spreads wider (wider glow on small emitters)
    const float iw = 2.0f / static_cast<float>(vw);
    const float ih = 2.0f / static_cast<float>(vh);

    // Pass 0: render emissive objects into FBO A
    gl.BindFramebuffer(kGL_FRAMEBUFFER, gl.fboA);
    gl.Viewport(0, 0, vw, vh);
    gl.Disable(kGL_BLEND);
    gl.ClearColor(0.f, 0.f, 0.f, 0.f);
    gl.Clear(kGL_COLOR_BUFFER_BIT);
    sceneRenderer_->drawEmissivePass(document_, view, proj);

    // Passes 1–4: ping-pong Gaussian blur (2 iterations each axis)
    auto blur = [&](unsigned srcTex, unsigned dstFbo, float dx, float dy) {
        gl.BindFramebuffer(kGL_FRAMEBUFFER, dstFbo);
        gl.Viewport(0, 0, vw, vh);
        gl.UseProgram(gl.progBlur);
        gl.Uniform1i(gl.GetUniformLocation(gl.progBlur, "u_tex"), 0);
        gl.Uniform2f(gl.GetUniformLocation(gl.progBlur, "u_dir"), dx, dy);
        gl.ActiveTexture(kGL_TEXTURE0);
        gl.BindTexture(kGL_TEXTURE_2D, srcTex);
        gl.BindVertexArray(gl.quadVAO);
        gl.DrawArrays(kGL_TRIANGLE_STRIP, 0, 4);
        gl.BindVertexArray(0);
    };
    for (int i = 0; i < 4; ++i) {
        blur(gl.texA, gl.fboB, iw, 0.f);   // horizontal: A → B
        blur(gl.texB, gl.fboA, 0.f, ih);   // vertical:   B → A
    }

    // Pass 5: additive composite onto viewport area
    gl.BindFramebuffer(kGL_FRAMEBUFFER, 0);
    gl.Viewport(vx, glViewY, vw, vh);
    gl.Disable(kGL_DEPTH_TEST);
    gl.Enable(kGL_BLEND);
    gl.BlendEquation(kGL_FUNC_ADD);
    gl.BlendFunc(kGL_ONE, kGL_ONE);
    gl.UseProgram(gl.progComposite);
    gl.Uniform1i(gl.GetUniformLocation(gl.progComposite, "u_tex"), 0);
    gl.Uniform1f(gl.GetUniformLocation(gl.progComposite, "u_strength"), 2.5f);
    gl.ActiveTexture(kGL_TEXTURE0);
    gl.BindTexture(kGL_TEXTURE_2D, gl.texA);
    gl.BindVertexArray(gl.quadVAO);
    gl.DrawArrays(kGL_TRIANGLE_STRIP, 0, 4);
    gl.BindVertexArray(0);

    // Restore GL state
    gl.Disable(kGL_BLEND);
    gl.Enable(kGL_DEPTH_TEST);
    gl.BlendFunc(kGL_SRC_ALPHA, kGL_ONE_MINUS_SRC_ALPHA);
    gl.BlendEquation(kGL_FUNC_ADD);
    gl.UseProgram(0);
    gl.BindTexture(kGL_TEXTURE_2D, 0);
    gl.Viewport(0, 0, cachedScreenW_, cachedScreenH_);
}

} // namespace MeshCraft
