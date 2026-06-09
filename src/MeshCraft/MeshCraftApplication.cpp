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
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <numbers>
#include <stdexcept>

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

bool MeshCraftApplication::sdlEventWatch(void* /*userdata*/, void* eventPtr) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event*>(eventPtr));
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

    SDL_AddEventWatch(reinterpret_cast<SDL_EventFilter>(sdlEventWatch), nullptr);

    if (!currentFile_.empty() && std::filesystem::exists(currentFile_)) {
        try {
            document_ = Mc3::Mc3Document::loadFromFile(currentFile_);
            std::cout << "[MeshCraft] Loaded: " << currentFile_ << "\n";
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

void MeshCraftApplication::Update(GameTime& /*gameTime*/) {
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

    const auto& vpFull = gd.getViewportProperty();
    int screenW = vpFull.getWidthProperty();
    int screenH = vpFull.getHeightProperty();

    // Compute top area height from ImGui (menu bar + toolbar window).
    // On the very first frame imguiTopH_ is 0; a reasonable fallback is 60.
    int topH = imguiTopH_ > 0 ? imguiTopH_ : 60;

    // Panel background
    gd.Clear(Color(18, 20, 36, 255));

    int viewX = kLeftPanelW;
    int viewY = topH;
    int viewW = std::max(1, screenW - kLeftPanelW - kRightPanelW);
    int viewH = std::max(1, screenH - topH - kStatusH);

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

    gd.SetDepthTestEnabled(false);
    gridRenderer_->draw(view, proj);

    gd.SetDepthTestEnabled(true);
    auto selPtrs = selectedPointers();
    sceneRenderer_->draw(document_, view, proj, selPtrs);

    if (selection_.hasSelection()) {
        float gizmoLen = camera_.distance * 0.15f;
        auto* sel0 = selection_.selection().front().get();
        if (activeTool_ == ActiveTool::Move) {
            gd.SetDepthTestEnabled(false);
            sceneRenderer_->drawGizmo(sel0, view, proj, gizmoLen);
        } else if (activeTool_ == ActiveTool::Scale) {
            gd.SetDepthTestEnabled(false);
            sceneRenderer_->drawScaleGizmo(sel0, view, proj, gizmoLen);
        } else if (activeTool_ == ActiveTool::Rotate) {
            gd.SetDepthTestEnabled(false);
            sceneRenderer_->drawRotateGizmo(sel0, view, proj, gizmoLen);
        }
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
// Keyboard shortcuts
// ---------------------------------------------------------------------------

static std::shared_ptr<Mc3::Mc3Object> deepCopyObj(const std::shared_ptr<Mc3::Mc3Object>& src);
static Mc3::Mc3Document deepCopyDoc(const Mc3::Mc3Document& src);

static bool justPressed(const KeyboardState& cur, const KeyboardState& prev, Keys k) {
    return cur.IsKeyDown(k) && prev.IsKeyUp(k);
}

void MeshCraftApplication::handleKeyboardShortcuts(const KeyboardState& ks, const KeyboardState& prevKs) {
    bool ctrl  = ks.IsKeyDown(Keys::LeftControl)  || ks.IsKeyDown(Keys::RightControl);
    bool shift = ks.IsKeyDown(Keys::LeftShift)    || ks.IsKeyDown(Keys::RightShift);
    bool alt   = ks.IsKeyDown(Keys::LeftAlt)      || ks.IsKeyDown(Keys::RightAlt);

    // Escape: deselect / reset tool / exit
    if (justPressed(ks, prevKs, Keys::Escape)) {
        if (activeTool_ != ActiveTool::Select || selection_.hasSelection()) {
            activeTool_ = ActiveTool::Select;
            selection_.clear();
            updateWindowTitle();
        } else {
            Exit();
        }
        return;
    }

    // Undo / Redo
    if (ctrl && justPressed(ks, prevKs, Keys::Z)) {
        if (!undoStack_.empty()) {
            redoStack_.push_back(deepCopyDoc(document_));
            if (static_cast<int>(redoStack_.size()) > kUndoMax)
                redoStack_.erase(redoStack_.begin());
            document_ = std::move(undoStack_.back());
            undoStack_.pop_back();
            selection_.clear();
            modified_ = true;
            updateWindowTitle();
        }
        return;
    }
    if (ctrl && justPressed(ks, prevKs, Keys::Y)) {
        if (!redoStack_.empty()) {
            undoStack_.push_back(deepCopyDoc(document_));
            if (static_cast<int>(undoStack_.size()) > kUndoMax)
                undoStack_.erase(undoStack_.begin());
            document_ = std::move(redoStack_.back());
            redoStack_.pop_back();
            selection_.clear();
            modified_ = true;
            updateWindowTitle();
        }
        return;
    }

    // Screenshot (F11)
    if (justPressed(ks, prevKs, Keys::F11)) {
        saveScreenshot("screenshot.ppm");
        std::cout << "[MeshCraft] Screenshot saved to screenshot.ppm\n";
        return;
    }

    // File ops
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::N)) { newScene();   return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::O)) { openFile();   return; }
    if (ctrl &&  shift && justPressed(ks, prevKs, Keys::S)) { saveFileAs(); return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::S)) { saveFile();   return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::E)) { exportGltf(); return; }

    // Tool selection
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::G)) { activeTool_ = ActiveTool::Move;   updateWindowTitle(); }
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::R)) { activeTool_ = ActiveTool::Rotate; updateWindowTitle(); }
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::S)) { activeTool_ = ActiveTool::Scale;  updateWindowTitle(); }
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::Q)) { activeTool_ = ActiveTool::Select; updateWindowTitle(); }

    // Preset camera views (Numpad)
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad1)) { camera_.yaw = 0.0f;                                camera_.pitch = 0.0f;  return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad3)) { camera_.yaw = std::numbers::pi_v<float> * 0.5f;  camera_.pitch = 0.0f;  return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad5)) { camera_.yaw = std::numbers::pi_v<float>;          camera_.pitch = 0.0f;  return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad7)) { camera_.yaw = 0.0f;                                camera_.pitch = 1.47f; return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::NumPad9)) { camera_.yaw = 0.0f;                                camera_.pitch =-1.47f; return; }

    // Add primitives
    if (!ctrl && justPressed(ks, prevKs, Keys::F1)) { addPrimitive(Mc3::ObjectType::Box);      return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F2)) { addPrimitive(Mc3::ObjectType::Sphere);   return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F3)) { addPrimitive(Mc3::ObjectType::Cylinder); return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F4)) { addPrimitive(Mc3::ObjectType::Cone);     return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F5)) { addPrimitive(Mc3::ObjectType::Plane);    return; }

    // Delete selected
    if (justPressed(ks, prevKs, Keys::Delete)) { deleteSelected(); return; }

    // Camera: F = focus on selection or reset
    if (!ctrl && justPressed(ks, prevKs, Keys::F)) {
        if (selection_.hasSelection()) {
            float bMinX = 1e30f, bMinY = 1e30f, bMinZ = 1e30f;
            float bMaxX = -1e30f, bMaxY = -1e30f, bMaxZ = -1e30f;
            for (const auto& s : selection_.selection()) {
                float px = s->transform.position[0];
                float py = s->transform.position[1];
                float pz = s->transform.position[2];
                float hx = 1.0f, hy = 1.0f, hz = 1.0f;
                if (s->primitive) {
                    const auto& p = *s->primitive;
                    float sx = std::abs(s->transform.scale[0]);
                    float sy = std::abs(s->transform.scale[1]);
                    float sz = std::abs(s->transform.scale[2]);
                    hx = hy = hz = 0.5f;
                    switch (p.primitiveType) {
                    case Mc3::PrimitiveType::Box:
                    case Mc3::PrimitiveType::Cube:
                        hx = p.size[0] * 0.5f * sx; hy = p.size[1] * 0.5f * sy; hz = p.size[2] * 0.5f * sz;
                        break;
                    case Mc3::PrimitiveType::Sphere:
                        hx = hy = hz = p.radius * std::max({sx,sy,sz});
                        break;
                    case Mc3::PrimitiveType::Cylinder:
                    case Mc3::PrimitiveType::Cone:
                        hx = hz = p.radius * std::max(sx, sz); hy = p.height * 0.5f * sy;
                        break;
                    case Mc3::PrimitiveType::Plane:
                        hx = p.size[0] * 0.5f * sx; hy = 0.05f; hz = p.size[2] * 0.5f * sz;
                        break;
                    }
                }
                bMinX = std::min(bMinX, px - hx); bMaxX = std::max(bMaxX, px + hx);
                bMinY = std::min(bMinY, py - hy); bMaxY = std::max(bMaxY, py + hy);
                bMinZ = std::min(bMinZ, pz - hz); bMaxZ = std::max(bMaxZ, pz + hz);
            }
            float cx = (bMinX + bMaxX) * 0.5f;
            float cy = (bMinY + bMaxY) * 0.5f;
            float cz = (bMinZ + bMaxZ) * 0.5f;
            float radius = std::max({bMaxX-bMinX, bMaxY-bMinY, bMaxZ-bMinZ}) * 0.5f;
            camera_.focusOn(cx, cy, cz, std::max(radius, 0.5f));
        } else {
            camera_.reset();
        }
        return;
    }

    // Select all
    if (ctrl && justPressed(ks, prevKs, Keys::A)) {
        if (selection_.hasSelection()) {
            auto& sel0 = selection_.selection().front();
            bool isGroup = sel0->type == Mc3::ObjectType::Group   ||
                           sel0->type == Mc3::ObjectType::Union   ||
                           sel0->type == Mc3::ObjectType::Difference ||
                           sel0->type == Mc3::ObjectType::Intersection ||
                           !sel0->children.empty();
            if (isGroup && !sel0->children.empty()) {
                selection_.clear();
                for (auto& child : sel0->children) selection_.select(child);
                updateWindowTitle();
                return;
            }
        }
        selection_.clear();
        for (auto& o : document_.objects) selection_.select(o);
        updateWindowTitle();
        return;
    }

    if (ctrl && justPressed(ks, prevKs, Keys::D)) { duplicateSelected(); return; }
    if (ctrl && justPressed(ks, prevKs, Keys::C)) { copySelected();   return; }
    if (ctrl && justPressed(ks, prevKs, Keys::X)) { cutSelected();    return; }
    if (ctrl && justPressed(ks, prevKs, Keys::V)) { pasteClipboard(); return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::G)) { groupSelected();   return; }
    if (ctrl &&  shift && justPressed(ks, prevKs, Keys::G)) { ungroupSelected(); return; }

    // Nudge selected objects
    if (!selection_.hasSelection()) return;
    float nudge = (shift ? 0.1f : 1.0f);
    bool nudged = false;
    bool anyNudgePressed =
        justPressed(ks, prevKs, Keys::Left)  || justPressed(ks, prevKs, Keys::Right) ||
        justPressed(ks, prevKs, Keys::Up)    || justPressed(ks, prevKs, Keys::Down)  ||
        justPressed(ks, prevKs, Keys::PageUp)|| justPressed(ks, prevKs, Keys::PageDown);
    if (anyNudgePressed) pushUndo();
    for (auto& selObj : selection_.selection()) {
        auto* obj = selObj.get();
        if (justPressed(ks, prevKs, Keys::Left))     { obj->transform.position[0] -= nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::Right))    { obj->transform.position[0] += nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::Up))       { obj->transform.position[1] += nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::Down))     { obj->transform.position[1] -= nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::PageUp))   { obj->transform.position[2] -= nudge; nudged = true; }
        if (justPressed(ks, prevKs, Keys::PageDown)) { obj->transform.position[2] += nudge; nudged = true; }
    }
    if (nudged) { modified_ = true; updateWindowTitle(); }
}

// ---------------------------------------------------------------------------
// Mouse input
// ---------------------------------------------------------------------------

void MeshCraftApplication::handleMouseInput(const MouseState& ms, const MouseState& prev) {
    int dx = ms.getXProperty() - prev.getXProperty();
    int dy = ms.getYProperty() - prev.getYProperty();
    int dscroll = ms.getScrollWheelValueProperty() - prev.getScrollWheelValueProperty();

    bool leftBtn   = ms.getLeftButtonProperty()   == ButtonState::Pressed;
    bool rightBtn  = ms.getRightButtonProperty()  == ButtonState::Pressed;
    bool middleBtn = ms.getMiddleButtonProperty() == ButtonState::Pressed;
    bool prevLeft  = prev.getLeftButtonProperty() == ButtonState::Pressed;

    // End gizmo drag on mouse release
    if (!leftBtn && gizmo_.isDragging())
        gizmo_.endDrag();

    // Camera orbit / pan / zoom
    if (middleBtn && (dx != 0 || dy != 0))
        camera_.orbit(dx * 0.005f, dy * 0.005f);
    else if (rightBtn && !middleBtn && (dx != 0 || dy != 0))
        camera_.pan(static_cast<float>(-dx), static_cast<float>(dy));

    if (dscroll != 0)
        camera_.zoom(static_cast<float>(dscroll) / 120.0f);

    // Compute 3D viewport bounds (same formula as Draw())
    auto& gd = getGraphicsDeviceProperty();
    int screenW = gd.getViewportProperty().getWidthProperty();
    int screenH = gd.getViewportProperty().getHeightProperty();
    int topH    = imguiTopH_ > 0 ? imguiTopH_ : 60;
    int vX = kLeftPanelW, vY = topH;
    int vW = std::max(1, screenW - kLeftPanelW - kRightPanelW);
    int vH = std::max(1, screenH - topH - kStatusH);
    float asp = static_cast<float>(vW) / static_cast<float>(vH);

    // Apply gizmo drag (Move)
    if (activeTool_ == ActiveTool::Move && gizmo_.isDragging() && leftBtn && (dx != 0 || dy != 0)
        && selection_.hasSelection())
    {
        Matrix vw = camera_.viewMatrix();
        Matrix pr = camera_.projectionMatrix(asp);
        Matrix vp = vw * pr;

        auto w2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
            float cX = wx*vp.M11 + wy*vp.M21 + wz*vp.M31 + vp.M41;
            float cY = wx*vp.M12 + wy*vp.M22 + wz*vp.M32 + vp.M42;
            float cW = wx*vp.M14 + wy*vp.M24 + wz*vp.M34 + vp.M44;
            if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
            return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                     (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
        };

        auto* sel0 = selection_.selection().front().get();
        float px = sel0->transform.position[0], py2 = sel0->transform.position[1], pz = sel0->transform.position[2];
        float L  = camera_.distance * 0.15f;
        int axIdx = static_cast<int>(gizmo_.dragAxis()) - 1;
        float tipXYZ[3][3] = {{px+L,py2,pz},{px,py2+L,pz},{px,py2,pz+L}};

        auto [cx, cy] = w2s(px, py2, pz);
        auto [tx, ty] = w2s(tipXYZ[axIdx][0], tipXYZ[axIdx][1], tipXYZ[axIdx][2]);
        float axScrX = tx - cx, axScrY = ty - cy;
        float len2d  = std::sqrt(axScrX*axScrX + axScrY*axScrY);
        if (len2d > 0.5f) {
            float dot = dx * (axScrX/len2d) + dy * (axScrY/len2d);
            sel0->transform.position[axIdx] += dot * L / len2d;
            modified_ = true;
            updateWindowTitle();
        }
        return;
    }

    // Apply gizmo drag (Scale)
    if (activeTool_ == ActiveTool::Scale && gizmo_.isDragging() && leftBtn && (dx != 0 || dy != 0)
        && selection_.hasSelection())
    {
        Matrix vw = camera_.viewMatrix();
        Matrix pr = camera_.projectionMatrix(asp);
        Matrix vp = vw * pr;

        auto w2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
            float cX = wx*vp.M11 + wy*vp.M21 + wz*vp.M31 + vp.M41;
            float cY = wx*vp.M12 + wy*vp.M22 + wz*vp.M32 + vp.M42;
            float cW = wx*vp.M14 + wy*vp.M24 + wz*vp.M34 + vp.M44;
            if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
            return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                     (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
        };

        auto* sel0 = selection_.selection().front().get();
        float px = sel0->transform.position[0], py2 = sel0->transform.position[1], pz = sel0->transform.position[2];
        float L  = camera_.distance * 0.15f;
        int axIdx = static_cast<int>(gizmo_.dragAxis()) - 1;
        float tipXYZ[3][3] = {{px+L,py2,pz},{px,py2+L,pz},{px,py2,pz+L}};

        auto [cx, cy] = w2s(px, py2, pz);
        auto [tx, ty] = w2s(tipXYZ[axIdx][0], tipXYZ[axIdx][1], tipXYZ[axIdx][2]);
        float axScrX = tx - cx, axScrY = ty - cy;
        float len3d  = std::sqrt(axScrX*axScrX + axScrY*axScrY);
        if (len3d > 0.5f) {
            float dot = dx * (axScrX/len3d) + dy * (axScrY/len3d);
            float& s = sel0->transform.scale[axIdx];
            s = std::max(0.01f, s + dot / len3d);
            modified_ = true;
            updateWindowTitle();
        }
        return;
    }

    // Apply gizmo drag (Rotate)
    if (activeTool_ == ActiveTool::Rotate && gizmo_.isDragging() && leftBtn && (dx != 0 || dy != 0)
        && selection_.hasSelection())
    {
        Matrix vw = camera_.viewMatrix();
        Matrix pr = camera_.projectionMatrix(asp);
        Matrix vp = vw * pr;

        auto w2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
            float cX = wx*vp.M11 + wy*vp.M21 + wz*vp.M31 + vp.M41;
            float cY = wx*vp.M12 + wy*vp.M22 + wz*vp.M32 + vp.M42;
            float cW = wx*vp.M14 + wy*vp.M24 + wz*vp.M34 + vp.M44;
            if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
            return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                     (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
        };

        auto* sel0 = selection_.selection().front().get();
        float px = sel0->transform.position[0], py2 = sel0->transform.position[1], pz = sel0->transform.position[2];
        float L  = camera_.distance * 0.15f;
        int axIdx = static_cast<int>(gizmo_.dragAxis()) - 1;

        auto [cx, cy] = w2s(px, py2, pz);
        float refPts[3][3] = { {px, py2+L, pz}, {px+L, py2, pz}, {px+L, py2, pz} };
        auto [rx4s, ry4s]  = w2s(refPts[axIdx][0], refPts[axIdx][1], refPts[axIdx][2]);
        float r_screen = std::max(1.0f, std::sqrt((rx4s-cx)*(rx4s-cx) + (ry4s-cy)*(ry4s-cy)));

        float curMx = static_cast<float>(ms.getXProperty());
        float curMy = static_cast<float>(ms.getYProperty());
        float radX = curMx - cx, radY = curMy - cy;
        float radLen = std::sqrt(radX*radX + radY*radY);
        if (radLen > 2.0f) {
            float tx = -radY/radLen, ty = radX/radLen;
            float dot = dx * tx + dy * ty;
            float degsPerPixel = 180.0f / (std::numbers::pi_v<float> * r_screen);
            sel0->transform.rotation[axIdx] += dot * degsPerPixel;
            modified_ = true;
            updateWindowTitle();
        }
        return;
    }

    // Left click in 3D viewport
    if (leftBtn && !prevLeft) {
        int mx = ms.getXProperty();
        int my = ms.getYProperty();
        bool ctrl = (Keyboard::GetState().IsKeyDown(Keys::LeftControl) ||
                     Keyboard::GetState().IsKeyDown(Keys::RightControl));

        bool in3d = (mx >= vX && mx < vX + vW && my >= vY && my < vY + vH);
        if (in3d) {
            // Gizmo handle hit test (Move or Scale)
            if ((activeTool_ == ActiveTool::Move || activeTool_ == ActiveTool::Scale)
                && selection_.hasSelection() && !ctrl)
            {
                auto* sel0 = selection_.selection().front().get();
                float gpx = sel0->transform.position[0];
                float gpy = sel0->transform.position[1];
                float gpz = sel0->transform.position[2];
                float gL  = camera_.distance * 0.15f;

                Matrix gvw = camera_.viewMatrix();
                Matrix gpr = camera_.projectionMatrix(asp);
                Matrix gvp = gvw * gpr;

                auto gw2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
                    float cX = wx*gvp.M11 + wy*gvp.M21 + wz*gvp.M31 + gvp.M41;
                    float cY = wx*gvp.M12 + wy*gvp.M22 + wz*gvp.M32 + gvp.M42;
                    float cW = wx*gvp.M14 + wy*gvp.M24 + wz*gvp.M34 + gvp.M44;
                    if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
                    return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                             (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
                };

                float gTips[3][3] = {{gpx+gL,gpy,gpz},{gpx,gpy+gL,gpz},{gpx,gpy,gpz+gL}};
                for (int gi = 0; gi < 3; ++gi) {
                    auto [gsx, gsy] = gw2s(gTips[gi][0], gTips[gi][1], gTips[gi][2]);
                    float gdist = std::sqrt((mx-gsx)*(mx-gsx) + (my-gsy)*(my-gsy));
                    if (gdist < 12.0f) {
                        pushUndo();
                        gizmo_.startDrag(static_cast<Editor::GizmoAxis>(gi + 1));
                        return;
                    }
                }
            }

            // Gizmo circle hit test (Rotate)
            if (activeTool_ == ActiveTool::Rotate && selection_.hasSelection() && !ctrl) {
                auto* sel0 = selection_.selection().front().get();
                float gpx = sel0->transform.position[0];
                float gpy = sel0->transform.position[1];
                float gpz = sel0->transform.position[2];
                float gL  = camera_.distance * 0.15f;

                Matrix gvw = camera_.viewMatrix();
                Matrix gpr = camera_.projectionMatrix(asp);
                Matrix gvp = gvw * gpr;

                auto gw2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
                    float cX = wx*gvp.M11 + wy*gvp.M21 + wz*gvp.M31 + gvp.M41;
                    float cY = wx*gvp.M12 + wy*gvp.M22 + wz*gvp.M32 + gvp.M42;
                    float cW = wx*gvp.M14 + wy*gvp.M24 + wz*gvp.M34 + gvp.M44;
                    if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
                    return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                             (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
                };

                const int CN = 32;
                int bestAx = -1;
                float bestDist = 11.0f;
                for (int ax = 0; ax < 3; ++ax) {
                    for (int j = 0; j < CN; ++j) {
                        float t = 2.0f * std::numbers::pi_v<float> * j / CN;
                        float c = std::cos(t), s = std::sin(t);
                        float wx, wy, wz;
                        if      (ax == 0) { wx=gpx;      wy=gpy+gL*c; wz=gpz+gL*s; }
                        else if (ax == 1) { wx=gpx+gL*c; wy=gpy;      wz=gpz+gL*s; }
                        else              { wx=gpx+gL*c; wy=gpy+gL*s; wz=gpz;       }
                        auto [sx, sy] = gw2s(wx, wy, wz);
                        float d = std::sqrt((mx-sx)*(mx-sx) + (my-sy)*(my-sy));
                        if (d < bestDist) { bestDist = d; bestAx = ax; }
                    }
                }
                if (bestAx >= 0) {
                    pushUndo();
                    gizmo_.startDrag(static_cast<Editor::GizmoAxis>(bestAx + 1));
                    return;
                }
            }

            // Ray-cast picking
            float ndcX = ((mx - vX) / static_cast<float>(vW)) * 2.0f - 1.0f;
            float ndcY = 1.0f - ((my - vY) / static_cast<float>(vH)) * 2.0f;

            Vector3 rayOrig = camera_.position();
            Vector3 rayDir  = camera_.screenRayDirection(ndcX, ndcY, asp);

            auto rayAABB = [](const Vector3& ro, const Vector3& rd,
                               const Vector3& bMin, const Vector3& bMax,
                               float& tHit) -> bool {
                float tNear = 0.0f, tFar = 1e30f;
                const float* rov = &ro.X; const float* rdv = &rd.X;
                const float* bnv = &bMin.X; const float* bxv = &bMax.X;
                for (int i = 0; i < 3; ++i) {
                    if (std::abs(rdv[i]) < 1e-9f) {
                        if (rov[i] < bnv[i] || rov[i] > bxv[i]) return false;
                    } else {
                        float t1 = (bnv[i] - rov[i]) / rdv[i];
                        float t2 = (bxv[i] - rov[i]) / rdv[i];
                        if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                        tNear = std::max(tNear, t1);
                        tFar  = std::min(tFar,  t2);
                        if (tNear > tFar) return false;
                    }
                }
                tHit = tNear;
                return tNear >= 0.0f;
            };

            auto objectAABB = [](const Mc3::Mc3Object& obj, Vector3& bMin, Vector3& bMax) {
                const auto& t = obj.transform;
                float px = t.position[0], py2 = t.position[1], pz = t.position[2];
                float sx = t.scale[0], sy = t.scale[1], sz = t.scale[2];
                float hx = 0.5f, hy = 0.5f, hz = 0.5f;
                if (obj.primitive) {
                    const auto& p = *obj.primitive;
                    switch (p.primitiveType) {
                    case Mc3::PrimitiveType::Box: case Mc3::PrimitiveType::Cube:
                        hx = p.size[0] * 0.5f; hy = p.size[1] * 0.5f; hz = p.size[2] * 0.5f; break;
                    case Mc3::PrimitiveType::Sphere:
                        hx = hy = hz = p.radius; break;
                    case Mc3::PrimitiveType::Cylinder: case Mc3::PrimitiveType::Cone:
                        hx = hz = p.radius; hy = p.height * 0.5f; break;
                    case Mc3::PrimitiveType::Plane:
                        hx = p.size[0] * 0.5f; hy = 0.05f; hz = p.size[1] * 0.5f; break;
                    }
                }
                hx *= std::abs(sx); hy *= std::abs(sy); hz *= std::abs(sz);
                bMin = { px - hx, py2 - hy, pz - hz };
                bMax = { px + hx, py2 + hy, pz + hz };
            };

            float bestT = 1e30f;
            std::shared_ptr<Mc3::Mc3Object> bestObj;

            std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> testList;
            testList = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
                for (const auto& obj : list) {
                    if (!obj || !obj->visible) continue;
                    if (obj->primitive) {
                        Vector3 bMin, bMax;
                        objectAABB(*obj, bMin, bMax);
                        float tHit = 0.0f;
                        if (rayAABB(rayOrig, rayDir, bMin, bMax, tHit) && tHit < bestT) {
                            bestT = tHit; bestObj = obj;
                        }
                    }
                    if (!obj->children.empty()) testList(obj->children);
                }
            };
            testList(document_.objects);

            if (!ctrl) selection_.clear();
            if (bestObj) selection_.select(bestObj);
            updateWindowTitle();
        }
    }

    // Box-select: drag in 3D viewport with Select tool
    if (activeTool_ == ActiveTool::Select && !gizmo_.isDragging()) {
        int mxB = ms.getXProperty(), myB = ms.getYProperty();
        bool in3dB = (mxB >= vX && mxB < vX + vW && myB >= vY && myB < vY + vH);

        if (leftBtn && !prevLeft && in3dB) {
            boxSelectX0_ = mxB; boxSelectY0_ = myB;
        }
        if (leftBtn && prevLeft && !boxSelectActive_ && in3dB) {
            int ddx = mxB - boxSelectX0_, ddy = myB - boxSelectY0_;
            if (std::abs(ddx) > 4 || std::abs(ddy) > 4)
                boxSelectActive_ = true;
        }
        if (boxSelectActive_ && leftBtn) {
            boxSelectX1_ = mxB; boxSelectY1_ = myB;
        }
        if (boxSelectActive_ && !leftBtn && prevLeft) {
            Matrix vwB = camera_.viewMatrix();
            Matrix prB = camera_.projectionMatrix(asp);
            Matrix vpB = vwB * prB;

            auto w2sB = [&](float wx, float wy, float wz) -> std::pair<float,float> {
                float cX = wx*vpB.M11 + wy*vpB.M21 + wz*vpB.M31 + vpB.M41;
                float cY = wx*vpB.M12 + wy*vpB.M22 + wz*vpB.M32 + vpB.M42;
                float cW = wx*vpB.M14 + wy*vpB.M24 + wz*vpB.M34 + vpB.M44;
                if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
                return { (cX/cW * 0.5f + 0.5f) * vW + vX,
                         (1.0f - (cY/cW * 0.5f + 0.5f)) * vH + vY };
            };

            int bxMin = std::min(boxSelectX0_, mxB), bxMax = std::max(boxSelectX0_, mxB);
            int byMin = std::min(boxSelectY0_, myB), byMax = std::max(boxSelectY0_, myB);

            bool additive = (Keyboard::GetState().IsKeyDown(Keys::LeftControl) ||
                             Keyboard::GetState().IsKeyDown(Keys::RightControl));
            if (!additive) selection_.clear();

            std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> boxTest;
            boxTest = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
                for (const auto& obj : list) {
                    if (!obj || !obj->visible) continue;
                    auto [sx, sy] = w2sB(obj->transform.position[0],
                                         obj->transform.position[1],
                                         obj->transform.position[2]);
                    if (sx >= bxMin && sx <= bxMax && sy >= byMin && sy <= byMax)
                        selection_.select(obj);
                    if (!obj->children.empty()) boxTest(obj->children);
                }
            };
            boxTest(document_.objects);
            updateWindowTitle();
            boxSelectActive_ = false;
        }
    }
    if (activeTool_ != ActiveTool::Select) boxSelectActive_ = false;
}

// ---------------------------------------------------------------------------
// Scene / file operations
// ---------------------------------------------------------------------------

void MeshCraftApplication::newScene() {
    document_ = Mc3::Mc3Document{};
    document_.model = "Untitled";
    selection_.clear();
    modified_ = false;
    currentFile_.clear();
    std::cout << "[MeshCraft] New scene\n";
    updateWindowTitle();
}

void MeshCraftApplication::openFile() {
    openDialogBuf_[0] = '\0';
    openDialogErr_[0] = '\0';
    openDialogOpen_ = true;
}

void MeshCraftApplication::saveFile() {
    if (currentFile_.empty()) { saveFileAs(); return; }
    try {
        document_.saveToFile(currentFile_);
        modified_ = false;
        std::cout << "[MeshCraft] Saved: " << currentFile_ << "\n";
        updateWindowTitle();
    } catch (const std::exception& e) {
        std::cerr << "[MeshCraft] Save error: " << e.what() << "\n";
    }
}

void MeshCraftApplication::saveFileAs() {
    auto s = currentFile_.string();
    std::strncpy(saveDialogBuf_, s.c_str(), sizeof(saveDialogBuf_) - 1);
    saveDialogBuf_[sizeof(saveDialogBuf_) - 1] = '\0';
    saveDialogErr_[0] = '\0';
    saveDialogOpen_ = true;
}

void MeshCraftApplication::exportGltf() {
    if (currentFile_.empty()) {
        std::cerr << "[MeshCraft] Save the file first before exporting.\n";
        return;
    }
    std::string mc3togltf = "mc3togltf";
    for (const auto& candidate : {
        std::filesystem::path("build-mc3togltf/mc3togltf"),
        std::filesystem::path("cmake-build-debug/mc3togltf/mc3togltf"),
        std::filesystem::path("../build-mc3togltf/mc3togltf"),
    }) {
        if (std::filesystem::exists(candidate)) { mc3togltf = candidate.string(); break; }
    }
    std::string outPath = currentFile_.string();
    auto pos = outPath.rfind(".mc3.xml");
    if (pos != std::string::npos) outPath.replace(pos, 8, ".glb");
    else outPath += ".glb";

    std::string cmd = mc3togltf + " \"" + currentFile_.string() + "\" \"" + outPath + "\"";
    std::cout << "[MeshCraft] Exporting: " << cmd << "\n";
    int ret = std::system(cmd.c_str());
    if (ret == 0) std::cout << "[MeshCraft] Exported to: " << outPath << "\n";
    else          std::cerr << "[MeshCraft] Export failed (exit code " << ret << ")\n";
}

// ---------------------------------------------------------------------------
// Edit operations
// ---------------------------------------------------------------------------

void MeshCraftApplication::addPrimitive(Mc3::ObjectType type) {
    pushUndo();
    auto obj = std::make_shared<Mc3::Mc3Object>();
    obj->type = type;

    static int counter = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Object%d", ++counter);
    obj->name = buf;

    Mc3::Mc3Primitive prim;
    switch (type) {
    case Mc3::ObjectType::Box:      prim.primitiveType = Mc3::PrimitiveType::Box;      prim.size = {1.0f,1.0f,1.0f}; break;
    case Mc3::ObjectType::Sphere:   prim.primitiveType = Mc3::PrimitiveType::Sphere;   prim.radius = 0.5f; break;
    case Mc3::ObjectType::Cylinder: prim.primitiveType = Mc3::PrimitiveType::Cylinder; prim.radius = 0.5f; prim.height = 1.0f; break;
    case Mc3::ObjectType::Cone:     prim.primitiveType = Mc3::PrimitiveType::Cone;     prim.radius = 0.5f; prim.height = 1.0f; break;
    case Mc3::ObjectType::Plane:    prim.primitiveType = Mc3::PrimitiveType::Plane;    prim.size = {1.0f,0.0f,1.0f}; break;
    default: break;
    }
    obj->primitive = prim;
    obj->transform.position = { camera_.target.X, camera_.target.Y + 0.5f, camera_.target.Z };

    if (selection_.hasSelection()) {
        auto& sel0 = selection_.selection().front();
        bool isGroup = sel0->type == Mc3::ObjectType::Group   ||
                       sel0->type == Mc3::ObjectType::Union   ||
                       sel0->type == Mc3::ObjectType::Difference ||
                       sel0->type == Mc3::ObjectType::Intersection ||
                       !sel0->children.empty();
        if (isGroup) {
            sel0->children.push_back(obj);
            selection_.clear(); selection_.select(obj);
            modified_ = true;
            updateWindowTitle(); return;
        }
    }
    document_.objects.push_back(obj);
    selection_.clear(); selection_.select(obj);
    modified_ = true;
    updateWindowTitle();
}

static void removeFromList(std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
                            const Mc3::Mc3Object* target)
{
    list.erase(std::remove_if(list.begin(), list.end(),
        [&](const auto& o){ return o.get() == target; }), list.end());
    for (auto& obj : list)
        if (!obj->children.empty())
            removeFromList(obj->children, target);
}

void MeshCraftApplication::deleteSelected() {
    pushUndo();
    for (const auto& s : selection_.selection())
        removeFromList(document_.objects, s.get());
    selection_.clear();
    modified_ = true;
    updateWindowTitle();
}

static std::shared_ptr<Mc3::Mc3Object> deepCopyObject(const Mc3::Mc3Object& src) {
    auto copy = std::make_shared<Mc3::Mc3Object>(src);
    copy->children.clear();
    for (const auto& child : src.children)
        copy->children.push_back(deepCopyObject(*child));
    return copy;
}

static std::vector<std::shared_ptr<Mc3::Mc3Object>>*
findParentList(std::vector<std::shared_ptr<Mc3::Mc3Object>>& list, const Mc3::Mc3Object* target)
{
    for (auto& obj : list) {
        if (obj.get() == target) return &list;
        if (!obj->children.empty()) {
            auto* found = findParentList(obj->children, target);
            if (found) return found;
        }
    }
    return nullptr;
}

void MeshCraftApplication::duplicateSelected() {
    if (!selection_.hasSelection()) return;
    pushUndo();
    auto prev = selection_.selection();
    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;

    for (const auto& s : prev) {
        auto* parent = findParentList(document_.objects, s.get());
        if (!parent) continue;
        auto copy = deepCopyObject(*s);
        copy->name = s->name + "_copy";
        auto it = std::find_if(parent->begin(), parent->end(),
            [&](const auto& o){ return o.get() == s.get(); });
        if (it != parent->end()) ++it;
        parent->insert(it, copy);
        newObjs.push_back(copy);
    }

    if (!newObjs.empty()) {
        selection_.clear();
        for (auto& o : newObjs) selection_.select(o);
        modified_ = true;
        updateWindowTitle();
    }
}

void MeshCraftApplication::copySelected() {
    if (!selection_.hasSelection()) return;
    clipboard_.clear();
    for (const auto& s : selection_.selection())
        clipboard_.push_back(deepCopyObject(*s));
}

void MeshCraftApplication::cutSelected() {
    if (!selection_.hasSelection()) return;
    copySelected();
    deleteSelected();
}

void MeshCraftApplication::pasteClipboard() {
    if (clipboard_.empty()) return;
    pushUndo();
    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;
    for (const auto& src : clipboard_) {
        auto copy = deepCopyObject(*src);
        copy->transform.position[0] += 1.0f;
        newObjs.push_back(copy);
    }
    if (selection_.hasSelection()) {
        auto& sel0 = selection_.selection().front();
        bool isGroup = sel0->type == Mc3::ObjectType::Group   ||
                       sel0->type == Mc3::ObjectType::Union   ||
                       sel0->type == Mc3::ObjectType::Difference ||
                       sel0->type == Mc3::ObjectType::Intersection ||
                       !sel0->children.empty();
        if (isGroup) {
            for (auto& o : newObjs) sel0->children.push_back(o);
            selection_.clear();
            for (auto& o : newObjs) selection_.select(o);
            modified_ = true;
            updateWindowTitle(); return;
        }
    }
    for (auto& o : newObjs) document_.objects.push_back(o);
    selection_.clear();
    for (auto& o : newObjs) selection_.select(o);
    modified_ = true;
    updateWindowTitle();
}

void MeshCraftApplication::groupSelected() {
    if (!selection_.hasSelection()) return;
    pushUndo();
    auto prev = selection_.selection();
    size_t insertIdx = document_.objects.size();
    for (const auto& s : prev)
        for (size_t i = 0; i < document_.objects.size(); ++i)
            if (document_.objects[i].get() == s.get()) { insertIdx = std::min(insertIdx, i); break; }

    auto group = std::make_shared<Mc3::Mc3Object>();
    group->type = Mc3::ObjectType::Group;
    static int groupCounter = 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Group%d", ++groupCounter);
    group->name = buf;

    for (const auto& s : prev) {
        group->children.push_back(s);
        removeFromList(document_.objects, s.get());
    }
    insertIdx = std::min(insertIdx, document_.objects.size());
    document_.objects.insert(document_.objects.begin() + static_cast<std::ptrdiff_t>(insertIdx), group);
    selection_.clear(); selection_.select(group);
    modified_ = true;
    updateWindowTitle();
}

void MeshCraftApplication::ungroupSelected() {
    if (!selection_.hasSelection()) return;
    auto& sel0 = selection_.selection().front();
    if (sel0->type != Mc3::ObjectType::Group || sel0->children.empty()) return;
    pushUndo();
    auto children = sel0->children;
    auto* parentList = findParentList(document_.objects, sel0.get());
    if (!parentList) return;
    auto it = std::find_if(parentList->begin(), parentList->end(),
        [&](const auto& o) { return o.get() == sel0.get(); });
    if (it == parentList->end()) return;
    auto insertIt = parentList->erase(it);
    for (const auto& child : children) { insertIt = parentList->insert(insertIt, child); ++insertIt; }
    selection_.clear();
    for (auto& child : children) selection_.select(child);
    modified_ = true;
    updateWindowTitle();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::vector<const Mc3::Mc3Object*> MeshCraftApplication::selectedPointers() const {
    std::vector<const Mc3::Mc3Object*> result;
    for (const auto& s : selection_.selection())
        result.push_back(s.get());
    return result;
}

void MeshCraftApplication::updateWindowTitle() {
    std::string title = "Mesh Craft";
    if (!document_.model.empty() && document_.model != "unnamed")
        title += " - " + document_.model;
    if (!currentFile_.empty())
        title += " [" + currentFile_.filename().string() + "]";
    if (modified_) title += " *";
    const char* toolNames[] = { "Select","Move","Rotate","Scale","Add Box","Add Sphere","Add Cylinder","Add Cone","Add Plane" };
    title += " | "; title += toolNames[static_cast<int>(activeTool_)];
    getWindowProperty().setTitleProperty(title);
}

// ---------------------------------------------------------------------------
// ImGui UI
// ---------------------------------------------------------------------------

void MeshCraftApplication::drawImGuiUi(int screenW, int screenH) {
    // -----------------------------------------------------------------------
    // Main menu bar
    // -----------------------------------------------------------------------
    float menuBarH = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        menuBarH = ImGui::GetWindowHeight();
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New",     "Ctrl+N")) newScene();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) openFile();
            ImGui::Separator();
            if (ImGui::MenuItem("Save",    "Ctrl+S")) saveFile();
            if (ImGui::MenuItem("Save As...","Ctrl+Shift+S")) saveFileAs();
            ImGui::Separator();
            if (ImGui::MenuItem("Export GLB", "Ctrl+E")) exportGltf();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) Exit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack_.empty())) {
                if (!undoStack_.empty()) {
                    redoStack_.push_back(deepCopyDoc(document_));
                    document_ = std::move(undoStack_.back()); undoStack_.pop_back();
                    selection_.clear(); modified_ = true; updateWindowTitle();
                }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !redoStack_.empty())) {
                if (!redoStack_.empty()) {
                    undoStack_.push_back(deepCopyDoc(document_));
                    document_ = std::move(redoStack_.back()); redoStack_.pop_back();
                    selection_.clear(); modified_ = true; updateWindowTitle();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut",       "Ctrl+X")) cutSelected();
            if (ImGui::MenuItem("Copy",      "Ctrl+C")) copySelected();
            if (ImGui::MenuItem("Paste",     "Ctrl+V")) pasteClipboard();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) duplicateSelected();
            if (ImGui::MenuItem("Delete",    "Del"))    deleteSelected();
            ImGui::Separator();
            if (ImGui::MenuItem("Select All","Ctrl+A")) {
                selection_.clear();
                for (auto& o : document_.objects) selection_.select(o);
                updateWindowTitle();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Group",   "Ctrl+G"))       groupSelected();
            if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G")) ungroupSelected();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Box"))      addPrimitive(Mc3::ObjectType::Box);
            if (ImGui::MenuItem("Sphere"))   addPrimitive(Mc3::ObjectType::Sphere);
            if (ImGui::MenuItem("Cylinder")) addPrimitive(Mc3::ObjectType::Cylinder);
            if (ImGui::MenuItem("Cone"))     addPrimitive(Mc3::ObjectType::Cone);
            if (ImGui::MenuItem("Plane"))    addPrimitive(Mc3::ObjectType::Plane);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Front",  "Num1")) { camera_.yaw = 0.0f;                               camera_.pitch = 0.0f; }
            if (ImGui::MenuItem("Right",  "Num3")) { camera_.yaw = std::numbers::pi_v<float> * 0.5f;  camera_.pitch = 0.0f; }
            if (ImGui::MenuItem("Back",   "Num5")) { camera_.yaw = std::numbers::pi_v<float>;          camera_.pitch = 0.0f; }
            if (ImGui::MenuItem("Top",    "Num7")) { camera_.yaw = 0.0f;                               camera_.pitch = 1.47f; }
            if (ImGui::MenuItem("Bottom", "Num9")) { camera_.yaw = 0.0f;                               camera_.pitch =-1.47f; }
            ImGui::Separator();
            if (ImGui::MenuItem("Focus on selection", "F")) {
                if (selection_.hasSelection()) {
                    auto* s = selection_.selection().front().get();
                    camera_.focusOn(s->transform.position[0], s->transform.position[1], s->transform.position[2]);
                } else { camera_.reset(); }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // -----------------------------------------------------------------------
    // Toolbar (below menu bar)
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(screenW), 40.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::Begin("##toolbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);

    // Tool buttons
    struct { ActiveTool tool; const char* label; ImVec4 col; } toolBtns[] = {
        { ActiveTool::Select, "Select [Q]", ImVec4(0.31f,0.59f,0.82f,1.f) },
        { ActiveTool::Move,   "Move   [G]", ImVec4(0.22f,0.74f,0.39f,1.f) },
        { ActiveTool::Rotate, "Rotate [R]", ImVec4(0.80f,0.65f,0.22f,1.f) },
        { ActiveTool::Scale,  "Scale  [S]", ImVec4(0.82f,0.29f,0.29f,1.f) },
    };
    for (auto& tb : toolBtns) {
        bool active = (activeTool_ == tb.tool);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, tb.col);
        if (ImGui::Button(tb.label, ImVec2(84, 30))) { activeTool_ = tb.tool; updateWindowTitle(); }
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Add primitive buttons
    struct { Mc3::ObjectType type; const char* label; ImVec4 col; } addBtns[] = {
        { Mc3::ObjectType::Box,      "+Box",  ImVec4(0.82f,0.47f,0.22f,1.f) },
        { Mc3::ObjectType::Sphere,   "+Sph",  ImVec4(0.22f,0.47f,0.82f,1.f) },
        { Mc3::ObjectType::Cylinder, "+Cyl",  ImVec4(0.22f,0.73f,0.39f,1.f) },
        { Mc3::ObjectType::Cone,     "+Con",  ImVec4(0.73f,0.22f,0.73f,1.f) },
        { Mc3::ObjectType::Plane,    "+Pln",  ImVec4(0.80f,0.80f,0.22f,1.f) },
    };
    for (auto& ab : addBtns) {
        ImGui::PushStyleColor(ImGuiCol_Button, ab.col);
        if (ImGui::Button(ab.label, ImVec2(40, 30))) addPrimitive(ab.type);
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    float toolbarH = ImGui::GetWindowHeight();
    imguiTopH_ = static_cast<int>(menuBarH + toolbarH);

    ImGui::End();
    ImGui::PopStyleVar(2);

    float panelY = menuBarH + toolbarH;
    float panelH = static_cast<float>(screenH) - panelY - static_cast<float>(kStatusH);

    // -----------------------------------------------------------------------
    // Left panel — Scene Hierarchy
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(0, panelY));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kLeftPanelW), panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.20f, 1.0f));
    ImGui::Begin("Scene", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> drawHierarchy;
    drawHierarchy = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
        for (const auto& obj : list) {
            ImGui::PushID(obj->id.c_str());
            bool sel = selection_.isSelected(obj.get());
            bool hasChildren = !obj->children.empty();
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;
            if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (sel)          flags |= ImGuiTreeNodeFlags_Selected;

            // Visibility indicator
            ImGui::PushStyleColor(ImGuiCol_Text,
                obj->visible ? ImVec4(1,1,1,1) : ImVec4(0.5f,0.5f,0.5f,1));
            const std::string& displayName = obj->name.empty() ? obj->id : obj->name;
            bool nodeOpen = ImGui::TreeNodeEx(displayName.c_str(), flags);
            ImGui::PopStyleColor();

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                bool ctrl = ImGui::GetIO().KeyCtrl;
                if (!ctrl) selection_.clear();
                selection_.select(obj);
                updateWindowTitle();
            }

            // Context menu
            if (ImGui::BeginPopupContextItem("##objctx")) {
                if (ImGui::MenuItem("Duplicate")) duplicateSelected();
                if (ImGui::MenuItem("Delete"))    deleteSelected();
                ImGui::Separator();
                if (ImGui::MenuItem(obj->visible ? "Hide" : "Show")) {
                    pushUndo(); obj->visible = !obj->visible; modified_ = true; updateWindowTitle();
                }
                ImGui::EndPopup();
            }

            if (hasChildren && nodeOpen)
                drawHierarchy(obj->children);
            if (hasChildren && nodeOpen)
                ImGui::TreePop();

            ImGui::PopID();
        }
    };
    drawHierarchy(document_.objects);
    ImGui::End();
    ImGui::PopStyleColor();

    // -----------------------------------------------------------------------
    // Right panel — Properties
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(screenW - kRightPanelW), panelY));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kRightPanelW), panelH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.11f, 0.20f, 1.0f));
    ImGui::Begin("Properties", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    if (selection_.hasSelection()) {
        auto& sel0 = selection_.selection().front();

        // Object name
        {
            char nameBuf[128];
            std::strncpy(nameBuf, sel0->name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf)-1] = '\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                pushUndo();
                sel0->name = nameBuf;
                modified_ = true;
                updateWindowTitle();
            }
        }

        ImGui::Spacing();

        // Transform: position
        ImGui::TextDisabled("Position");
        {
            float pos[3] = { sel0->transform.position[0], sel0->transform.position[1], sel0->transform.position[2] };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat3("##pos", pos, 0.1f)) {
                if (ImGui::IsItemActivated()) pushUndo();
                sel0->transform.position[0] = pos[0];
                sel0->transform.position[1] = pos[1];
                sel0->transform.position[2] = pos[2];
                modified_ = true; updateWindowTitle();
            }
        }

        // Transform: rotation
        ImGui::TextDisabled("Rotation");
        {
            float rot[3] = { sel0->transform.rotation[0], sel0->transform.rotation[1], sel0->transform.rotation[2] };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat3("##rot", rot, 0.5f)) {
                if (ImGui::IsItemActivated()) pushUndo();
                sel0->transform.rotation[0] = rot[0];
                sel0->transform.rotation[1] = rot[1];
                sel0->transform.rotation[2] = rot[2];
                modified_ = true; updateWindowTitle();
            }
        }

        // Transform: scale
        ImGui::TextDisabled("Scale");
        {
            float scl[3] = { sel0->transform.scale[0], sel0->transform.scale[1], sel0->transform.scale[2] };
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat3("##scl", scl, 0.01f, 0.001f, 100.0f)) {
                if (ImGui::IsItemActivated()) pushUndo();
                sel0->transform.scale[0] = std::max(0.001f, scl[0]);
                sel0->transform.scale[1] = std::max(0.001f, scl[1]);
                sel0->transform.scale[2] = std::max(0.001f, scl[2]);
                modified_ = true; updateWindowTitle();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Visible
        {
            bool vis = sel0->visible;
            if (ImGui::Checkbox("Visible", &vis)) {
                pushUndo(); sel0->visible = vis; modified_ = true; updateWindowTitle();
            }
        }

        // Collision
        {
            char colBuf[128];
            std::strncpy(colBuf, sel0->collision.c_str(), sizeof(colBuf) - 1);
            colBuf[sizeof(colBuf)-1] = '\0';
            ImGui::TextDisabled("Collision");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##col", colBuf, sizeof(colBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                pushUndo(); sel0->collision = colBuf; modified_ = true;
            }
        }

        // Tags
        {
            std::string joined;
            for (size_t i = 0; i < sel0->tags.size(); ++i) {
                if (i > 0) joined += ',';
                joined += sel0->tags[i];
            }
            char tagsBuf[256];
            std::strncpy(tagsBuf, joined.c_str(), sizeof(tagsBuf) - 1);
            tagsBuf[sizeof(tagsBuf)-1] = '\0';
            ImGui::TextDisabled("Tags (comma-separated)");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##tags", tagsBuf, sizeof(tagsBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                pushUndo();
                sel0->tags.clear();
                std::string tok;
                for (char ch : std::string(tagsBuf)) {
                    if (ch == ',') {
                        auto s = tok.find_first_not_of(" \t");
                        auto e = tok.find_last_not_of(" \t");
                        if (s != std::string::npos) sel0->tags.push_back(tok.substr(s, e - s + 1));
                        tok.clear();
                    } else tok += ch;
                }
                {
                    auto s = tok.find_first_not_of(" \t");
                    auto e = tok.find_last_not_of(" \t");
                    if (s != std::string::npos) sel0->tags.push_back(tok.substr(s, e - s + 1));
                }
                modified_ = true;
            }
        }

        // Geometry parameters
        if (sel0->primitive) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Geometry");

            auto& p = *sel0->primitive;

            switch (p.primitiveType) {
            case Mc3::PrimitiveType::Box:
            case Mc3::PrimitiveType::Cube: {
                float sz[3] = { p.size[0], p.size[1], p.size[2] };
                ImGui::TextDisabled("Size (W/H/D)");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat3("##psize", sz, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.size[0] = std::max(0.001f, sz[0]);
                    p.size[1] = std::max(0.001f, sz[1]);
                    p.size[2] = std::max(0.001f, sz[2]);
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::Sphere: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##prad", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.radius = std::max(0.001f, r);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##psegs", &segs, 4, 64)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.segments = segs;
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::Cylinder:
            case Mc3::PrimitiveType::Cone: {
                ImGui::TextDisabled("Radius");
                float r = p.radius;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##prad", &r, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.radius = std::max(0.001f, r);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Height");
                float h = p.height;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##phgt", &h, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.height = std::max(0.001f, h);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Segments");
                int segs = p.segments;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderInt("##psegs", &segs, 4, 64)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.segments = segs;
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            case Mc3::PrimitiveType::Plane: {
                ImGui::TextDisabled("Width");
                float w = p.size[0];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ppw", &w, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.size[0] = std::max(0.001f, w);
                    modified_ = true; updateWindowTitle();
                }
                ImGui::TextDisabled("Depth");
                float d = p.size[2];
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##ppd", &d, 0.01f, 0.001f, 1000.0f)) {
                    if (ImGui::IsItemActivated()) pushUndo();
                    p.size[2] = std::max(0.001f, d);
                    modified_ = true; updateWindowTitle();
                }
                break;
            }
            }
        }

        // Material swatch (read-only)
        if (!sel0->material.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Material: %s", sel0->material.c_str());
            for (const auto& [key, mat] : document_.materials) {
                if (key == sel0->material) {
                    ImGui::ColorButton("##matcol",
                        ImVec4(mat.baseColor[0], mat.baseColor[1], mat.baseColor[2], 1.0f),
                        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker);
                    break;
                }
            }
        }
    } else {
        ImGui::TextDisabled("Nothing selected");
    }

    ImGui::End();
    ImGui::PopStyleColor();

    // -----------------------------------------------------------------------
    // Status bar (bottom)
    // -----------------------------------------------------------------------
    ImGui::SetNextWindowPos(ImVec2(0, static_cast<float>(screenH - kStatusH)));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(screenW), static_cast<float>(kStatusH)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 3));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f, 0.094f, 0.176f, 1.0f));
    ImGui::Begin("##statusbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);
    {
        int totalObjs = 0;
        std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> countAll =
            [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
                totalObjs += static_cast<int>(list.size());
                for (const auto& o : list) countAll(o->children);
            };
        countAll(document_.objects);
        int selCount = static_cast<int>(selection_.selection().size());
        if (selCount > 0) {
            const std::string& selName = selection_.selection().front()->name;
            ImGui::Text("%d objects | %d selected | %s", totalObjs, selCount, selName.c_str());
        } else {
            ImGui::Text("%d objects", totalObjs);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // -----------------------------------------------------------------------
    // Box-select overlay (drawn via ImGui drawlist on top of everything)
    // -----------------------------------------------------------------------
    if (boxSelectActive_) {
        int bx0 = std::min(boxSelectX0_, boxSelectX1_);
        int by0 = std::min(boxSelectY0_, boxSelectY1_);
        int bx1 = std::max(boxSelectX0_, boxSelectX1_);
        int by1 = std::max(boxSelectY0_, boxSelectY1_);
        if (bx1 > bx0 && by1 > by0) {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            dl->AddRectFilled(ImVec2((float)bx0, (float)by0), ImVec2((float)bx1, (float)by1),
                              IM_COL32(60, 120, 200, 40));
            dl->AddRect(ImVec2((float)bx0, (float)by0), ImVec2((float)bx1, (float)by1),
                        IM_COL32(100, 160, 255, 220));
        }
    }

    // -----------------------------------------------------------------------
    // File dialog modals
    // -----------------------------------------------------------------------
    if (openDialogOpen_) {
        ImGui::OpenPopup("Open File##dlg");
        openDialogOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Open File##dlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("File path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##openpath", openDialogBuf_, sizeof(openDialogBuf_));
        if (openDialogErr_[0]) ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s", openDialogErr_);
        if (ImGui::Button("Open") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            try {
                document_ = Mc3::Mc3Document::loadFromFile(openDialogBuf_);
                currentFile_ = openDialogBuf_;
                selection_.clear();
                undoStack_.clear(); redoStack_.clear();
                modified_ = false;
                openDialogErr_[0] = '\0';
                std::cout << "[MeshCraft] Loaded: " << openDialogBuf_ << "\n";
                updateWindowTitle();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& e) {
                std::strncpy(openDialogErr_, e.what(), sizeof(openDialogErr_) - 1);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (saveDialogOpen_) {
        ImGui::OpenPopup("Save As##dlg");
        saveDialogOpen_ = false;
    }
    if (ImGui::BeginPopupModal("Save As##dlg", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("File path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##savepath", saveDialogBuf_, sizeof(saveDialogBuf_));
        if (saveDialogErr_[0]) ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s", saveDialogErr_);
        if (ImGui::Button("Save") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            std::string path = saveDialogBuf_;
            if (path.find(".mc3.xml") == std::string::npos) path += ".mc3.xml";
            try {
                document_.saveToFile(path);
                currentFile_ = path;
                modified_ = false;
                saveDialogErr_[0] = '\0';
                std::cout << "[MeshCraft] Saved: " << path << "\n";
                updateWindowTitle();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& e) {
                std::strncpy(saveDialogErr_, e.what(), sizeof(saveDialogErr_) - 1);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Undo/redo
// ---------------------------------------------------------------------------

static std::shared_ptr<Mc3::Mc3Object> deepCopyObj(const std::shared_ptr<Mc3::Mc3Object>& src) {
    auto copy = std::make_shared<Mc3::Mc3Object>(*src);
    copy->children.clear();
    for (const auto& child : src->children)
        copy->children.push_back(deepCopyObj(child));
    return copy;
}

static Mc3::Mc3Document deepCopyDoc(const Mc3::Mc3Document& src) {
    Mc3::Mc3Document copy = src;
    copy.objects.clear();
    for (const auto& obj : src.objects)
        copy.objects.push_back(deepCopyObj(obj));
    copy.definitions.clear();
    for (const auto& [key, obj] : src.definitions)
        copy.definitions[key] = deepCopyObj(obj);
    return copy;
}

void MeshCraftApplication::pushUndo() {
    undoStack_.push_back(deepCopyDoc(document_));
    if (static_cast<int>(undoStack_.size()) > kUndoMax)
        undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

// ---------------------------------------------------------------------------
// Screenshot
// ---------------------------------------------------------------------------

void MeshCraftApplication::saveScreenshot(const std::string& path) {
    auto& gd = getGraphicsDeviceProperty();
    int w = gd.getViewportProperty().getWidthProperty();
    int h = gd.getViewportProperty().getHeightProperty();
    if (w <= 0 || h <= 0) return;

    using PFNGLFINISH     = void(*)();
    using PFNGLBINDBUFFER = void(*)(unsigned int, unsigned int);
    using PFNGLREADPIXELS = void(*)(int, int, int, int, unsigned int, unsigned int, void*);
    auto fnFinish     = reinterpret_cast<PFNGLFINISH>    (SDL_GL_GetProcAddress("glFinish"));
    auto fnBindBuffer = reinterpret_cast<PFNGLBINDBUFFER>(SDL_GL_GetProcAddress("glBindBuffer"));
    auto fnReadPixels = reinterpret_cast<PFNGLREADPIXELS>(SDL_GL_GetProcAddress("glReadPixels"));
    if (!fnReadPixels) { std::cerr << "[Screenshot] glReadPixels not available\n"; return; }
    if (fnFinish)     fnFinish();
    if (fnBindBuffer) fnBindBuffer(0x88EC, 0);

    constexpr unsigned int GL_RGBA          = 0x1908;
    constexpr unsigned int GL_UNSIGNED_BYTE = 0x1401;
    std::vector<unsigned char> pixels(w * h * 4);
    fnReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int row = h - 1; row >= 0; --row)
        for (int col = 0; col < w; ++col) {
            int idx = (row * w + col) * 4;
            f.write(reinterpret_cast<char*>(&pixels[idx]), 3);
        }
    std::cout << "[Screenshot] written " << path << "\n";
}

// ---------------------------------------------------------------------------
// Misc helpers
// ---------------------------------------------------------------------------

Mc3::Mc3Object* MeshCraftApplication::flatFindById(const std::string& id) const {
    std::function<Mc3::Mc3Object*(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> find;
    find = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) -> Mc3::Mc3Object* {
        for (const auto& obj : list) {
            if (obj->id == id) return obj.get();
            if (!obj->children.empty()) {
                auto* r = find(obj->children);
                if (r) return r;
            }
        }
        return nullptr;
    };
    return find(document_.objects);
}

} // namespace MeshCraft
