#include "MeshCraft/MeshCraftApplication.hpp"

#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Rectangle.hpp>
#include <Microsoft/Xna/Framework/Input/Keyboard.hpp>
#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/Mouse.hpp>
#include <Microsoft/Xna/Framework/Input/ButtonState.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/Viewport.hpp>
#include <System/Object.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <numbers>
#include <stdexcept>

// Forward-declare SDL3 GL proc address lookup without including SDL headers.
// SDL_FunctionPointer is typedef void(*)(void) on all platforms.
using SDL_FunctionPointer = void(*)(void);
extern "C" SDL_FunctionPointer SDL_GL_GetProcAddress(const char*);


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
    , autoScreenshotCountdown_(120) // take screenshot after 120 frames (~2s)
{
    getWindowProperty().setTitleProperty("Mesh Craft");
    setIsMouseVisibleProperty(true);
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

    // 2D UI: white pixel texture + SpriteBatch
    spriteBatch_ = std::make_unique<Graphics::SpriteBatch>(gd);
    whitePx_ = Graphics::Texture2D(gd, 1, 1);
    Color white(255, 255, 255, 255);
    whitePx_.SetData(&white, 1);

    // Load GL function pointers for direct viewport/scissor control
    fnGlViewport_ = reinterpret_cast<void(*)(int,int,int,int)>(SDL_GL_GetProcAddress("glViewport"));
    fnGlScissor_  = reinterpret_cast<void(*)(int,int,int,int)>(SDL_GL_GetProcAddress("glScissor"));
    fnGlEnable_   = reinterpret_cast<void(*)(unsigned int)>   (SDL_GL_GetProcAddress("glEnable"));
    fnGlDisable_  = reinterpret_cast<void(*)(unsigned int)>   (SDL_GL_GetProcAddress("glDisable"));

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
// Update
// ---------------------------------------------------------------------------

void MeshCraftApplication::Update(GameTime& /*gameTime*/) {
    auto ks = Keyboard::GetState();
    auto ms = Mouse::GetState();

    if (!firstFrame_) {
        handleKeyboardShortcuts(ks, prevKs_);
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

    // Full screen dimensions
    const auto& vpFull = gd.getViewportProperty();
    int screenW = vpFull.getWidthProperty();
    int screenH = vpFull.getHeightProperty();

    // Panel background — clear entire screen dark first
    gd.Clear(Color(18, 20, 36, 255));


    // Compute 3D viewport (center area excluding panels)
    int viewX = kLeftPanelW;
    int viewY = kToolbarH;
    int viewW = std::max(1, screenW - kLeftPanelW - kRightPanelW);
    int viewH = std::max(1, screenH - kToolbarH - kStatusH);

    // GL uses bottom-left origin; flip Y for viewport/scissor
    constexpr unsigned int GL_SCISSOR_TEST = 0x0C11;
    int glViewY = screenH - viewY - viewH;

    // Enable scissor to restrict the bgColor clear to the 3D area only
    if (fnGlEnable_)  fnGlEnable_(GL_SCISSOR_TEST);
    if (fnGlScissor_) fnGlScissor_(viewX, glViewY, viewW, viewH);

    // Scene background color
    Color bgColor(64, 72, 80, 255);
    if (document_.environment) {
        const auto& bc = document_.environment->backgroundColor;
        bgColor = Color(
            static_cast<int>(std::clamp(bc[0], 0.0f, 1.0f) * 255),
            static_cast<int>(std::clamp(bc[1], 0.0f, 1.0f) * 255),
            static_cast<int>(std::clamp(bc[2], 0.0f, 1.0f) * 255),
            255);
    }
    gd.Clear(bgColor);  // Clear() resets GL viewport to full window; scissor limits it to 3D area

    // Re-apply correct GL viewport for 3D rendering (Clear() reset it to full window)
    if (fnGlViewport_) fnGlViewport_(viewX, glViewY, viewW, viewH);

    float aspect = (viewH > 0) ? static_cast<float>(viewW) / viewH : 16.0f / 9.0f;
    Matrix view = camera_.viewMatrix();
    Matrix proj = camera_.projectionMatrix(aspect);

    // Grid
    gd.SetDepthTestEnabled(false);
    gridRenderer_->draw(view, proj);

    // Scene objects
    gd.SetDepthTestEnabled(true);
    auto selPtrs = selectedPointers();
    sceneRenderer_->draw(document_, view, proj, selPtrs);

    // Transform gizmo — draw on top of scene (depth-test off so always visible)
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

    // Restore full GL viewport and disable scissor before 2D UI overlay
    if (fnGlDisable_)  fnGlDisable_(GL_SCISSOR_TEST);
    if (fnGlViewport_) fnGlViewport_(0, 0, screenW, screenH);

    // Update CPU-side viewport to match
    Graphics::Viewport vpReset;
    vpReset.x = 0;
    vpReset.y = 0;
    vpReset.setWidthProperty(screenW);
    vpReset.setHeightProperty(screenH);
    gd.setViewportProperty(vpReset);

    gd.SetDepthTestEnabled(false);
    spriteBatch_->Begin();
    drawUi(screenW, screenH);
    spriteBatch_->End();

    // Auto-screenshot countdown
    if (autoScreenshotCountdown_ > 0) {
        --autoScreenshotCountdown_;
        if (autoScreenshotCountdown_ == 0 && !autoScreenshotPath_.empty()) {
            saveScreenshot(autoScreenshotPath_);
            std::cout << "[MeshCraft] Auto-screenshot saved to: " << autoScreenshotPath_ << "\n";
            Exit();
        }
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

    // Cancel field if selection was cleared externally
    if (fieldActive_ && !selection_.hasSelection()) cancelField();

    // When a properties field is active, capture text input exclusively
    if (fieldActive_) {
        if (justPressed(ks, prevKs, Keys::Escape)) { cancelField(); return; }
        if (justPressed(ks, prevKs, Keys::Enter))  { applyFieldValue(); return; }
        if (justPressed(ks, prevKs, Keys::Back) && !fieldBuffer_.empty()) {
            fieldBuffer_.pop_back(); return;
        }
        if (fieldSection_ < 0) {
            // String field (name / collision): accept letters, digits, space, dash, underscore, period
            struct { Keys key; char lo; char hi; } letterKeys[] = {
                {Keys::A,'a','A'},{Keys::B,'b','B'},{Keys::C,'c','C'},{Keys::D,'d','D'},
                {Keys::E,'e','E'},{Keys::F,'f','F'},{Keys::G,'g','G'},{Keys::H,'h','H'},
                {Keys::I,'i','I'},{Keys::J,'j','J'},{Keys::K,'k','K'},{Keys::L,'l','L'},
                {Keys::M,'m','M'},{Keys::N,'n','N'},{Keys::O,'o','O'},{Keys::P,'p','P'},
                {Keys::Q,'q','Q'},{Keys::R,'r','R'},{Keys::S,'s','S'},{Keys::T,'t','T'},
                {Keys::U,'u','U'},{Keys::V,'v','V'},{Keys::W,'w','W'},{Keys::X,'x','X'},
                {Keys::Y,'y','Y'},{Keys::Z,'z','Z'},
            };
            for (auto& lk : letterKeys) {
                if (justPressed(ks, prevKs, lk.key)) { fieldBuffer_ += shift ? lk.hi : lk.lo; return; }
            }
            struct { Keys key; char ch; } numKeys[] = {
                {Keys::D0,'0'},{Keys::D1,'1'},{Keys::D2,'2'},{Keys::D3,'3'},{Keys::D4,'4'},
                {Keys::D5,'5'},{Keys::D6,'6'},{Keys::D7,'7'},{Keys::D8,'8'},{Keys::D9,'9'},
                {Keys::NumPad0,'0'},{Keys::NumPad1,'1'},{Keys::NumPad2,'2'},
                {Keys::NumPad3,'3'},{Keys::NumPad4,'4'},{Keys::NumPad5,'5'},
                {Keys::NumPad6,'6'},{Keys::NumPad7,'7'},{Keys::NumPad8,'8'},{Keys::NumPad9,'9'},
            };
            for (auto& nk : numKeys) {
                if (justPressed(ks, prevKs, nk.key)) { fieldBuffer_ += nk.ch; return; }
            }
            if (justPressed(ks, prevKs, Keys::Space)) { fieldBuffer_ += ' '; return; }
            if (justPressed(ks, prevKs, Keys::OemMinus) || justPressed(ks, prevKs, Keys::Subtract)) {
                fieldBuffer_ += shift ? '_' : '-'; return;
            }
            if (justPressed(ks, prevKs, Keys::OemPeriod) || justPressed(ks, prevKs, Keys::Decimal)) {
                fieldBuffer_ += '.'; return;
            }
        } else {
            // Numeric transform fields: digits, decimal point, leading minus only
            struct { Keys key; char ch; } numKeys[] = {
                {Keys::D0,'0'},{Keys::D1,'1'},{Keys::D2,'2'},{Keys::D3,'3'},{Keys::D4,'4'},
                {Keys::D5,'5'},{Keys::D6,'6'},{Keys::D7,'7'},{Keys::D8,'8'},{Keys::D9,'9'},
                {Keys::NumPad0,'0'},{Keys::NumPad1,'1'},{Keys::NumPad2,'2'},
                {Keys::NumPad3,'3'},{Keys::NumPad4,'4'},{Keys::NumPad5,'5'},
                {Keys::NumPad6,'6'},{Keys::NumPad7,'7'},{Keys::NumPad8,'8'},{Keys::NumPad9,'9'},
            };
            for (auto& nk : numKeys) {
                if (justPressed(ks, prevKs, nk.key)) { fieldBuffer_ += nk.ch; return; }
            }
            if (justPressed(ks, prevKs, Keys::OemPeriod) || justPressed(ks, prevKs, Keys::Decimal)) {
                if (fieldBuffer_.find('.') == std::string::npos) fieldBuffer_ += '.';
                return;
            }
            if (justPressed(ks, prevKs, Keys::OemMinus) || justPressed(ks, prevKs, Keys::Subtract)) {
                if (fieldBuffer_.empty()) { fieldBuffer_ += '-'; return; }
            }
        }
        return; // swallow all other keys while editing
    }

    // Escape: first press deselects / resets tool; second press (already in select+empty) exits
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

    // Print help to console (F12)
    if (justPressed(ks, prevKs, Keys::F12)) {
        std::cout <<
            "\n=== Mesh Craft Keyboard Shortcuts ===\n"
            "File:\n"
            "  Ctrl+N        New scene\n"
            "  Ctrl+O        Open mc3.xml file (enter path in console)\n"
            "  Ctrl+S        Save\n"
            "  Ctrl+Shift+S  Save As\n"
            "  Ctrl+E        Export to GLB via mc3togltf\n"
            "Tools:\n"
            "  Q / Esc       Select tool (click to select — TODO ray cast)\n"
            "  G             Move tool\n"
            "  R             Rotate tool\n"
            "  S             Scale tool\n"
            "Add primitives:\n"
            "  F1  Box       F2  Sphere    F3  Cylinder\n"
            "  F4  Cone      F5  Plane\n"
            "Edit:\n"
            "  Del           Delete selected\n"
            "  Ctrl+A        Select all\n"
            "  Ctrl+D        Duplicate selected\n"
            "  Arrow keys    Nudge selected (Shift = 0.1 step)\n"
            "Camera:\n"
            "  Middle-drag   Orbit\n"
            "  Right-drag    Pan\n"
            "  Scroll wheel  Zoom\n"
            "  F             Reset camera\n"
            "Help:\n"
            "  F12           Print this help\n"
            "======================================\n\n";
        return;
    }

    // File ops
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::N)) { newScene(); return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::O)) { openFile();  return; }
    if (ctrl &&  shift && justPressed(ks, prevKs, Keys::S)) { saveFileAs(); return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::S)) { saveFile();  return; }
    if (ctrl && !shift && justPressed(ks, prevKs, Keys::E)) { exportGltf(); return; }

    // Tool selection (Blender/SketchUp style shortcuts, no Ctrl modifier)
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::G)) { activeTool_ = ActiveTool::Move;   updateWindowTitle(); }
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::R)) { activeTool_ = ActiveTool::Rotate; updateWindowTitle(); }
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::S)) { activeTool_ = ActiveTool::Scale;  updateWindowTitle(); }
    if (!ctrl && !alt && justPressed(ks, prevKs, Keys::Q)) { activeTool_ = ActiveTool::Select; updateWindowTitle(); }

    // Add primitives
    if (!ctrl && justPressed(ks, prevKs, Keys::F1)) { addPrimitive(Mc3::ObjectType::Box);      return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F2)) { addPrimitive(Mc3::ObjectType::Sphere);   return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F3)) { addPrimitive(Mc3::ObjectType::Cylinder); return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F4)) { addPrimitive(Mc3::ObjectType::Cone);     return; }
    if (!ctrl && justPressed(ks, prevKs, Keys::F5)) { addPrimitive(Mc3::ObjectType::Plane);    return; }

    // Delete selected
    if (justPressed(ks, prevKs, Keys::Delete)) { deleteSelected(); return; }

    // Camera: F = focus on selection, or reset if nothing selected
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
                        hx = p.size[0] * 0.5f * sx;
                        hy = p.size[1] * 0.5f * sy;
                        hz = p.size[2] * 0.5f * sz;
                        break;
                    case Mc3::PrimitiveType::Sphere:
                        hx = hy = hz = p.radius * std::max({sx,sy,sz});
                        break;
                    case Mc3::PrimitiveType::Cylinder:
                    case Mc3::PrimitiveType::Cone:
                        hx = hz = p.radius * std::max(sx, sz);
                        hy = p.height * 0.5f * sy;
                        break;
                    case Mc3::PrimitiveType::Plane:
                        hx = p.size[0] * 0.5f * sx;
                        hy = 0.05f;
                        hz = p.size[2] * 0.5f * sz;
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

    // Select all — within selected group if one is selected, else top-level
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

    // Duplicate (Ctrl+D)
    if (ctrl && justPressed(ks, prevKs, Keys::D)) { duplicateSelected(); return; }

    // Cut / Copy / Paste
    if (ctrl && justPressed(ks, prevKs, Keys::C)) { copySelected();   return; }
    if (ctrl && justPressed(ks, prevKs, Keys::X)) { cutSelected();    return; }
    if (ctrl && justPressed(ks, prevKs, Keys::V)) { pasteClipboard(); return; }

    // Nudge selected objects with arrow keys
    if (!selection_.hasSelection()) return;
    float nudge = (shift ? 0.1f : 1.0f);
    bool nudged = false;
    // Check if any nudge key is pressed before doing anything (to avoid push without mutation)
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

    bool prevLeft  = prev.getLeftButtonProperty()  == ButtonState::Pressed;

    // End gizmo drag on mouse release
    if (!leftBtn && gizmo_.isDragging()) {
        gizmo_.endDrag();
    }

    // Middle-drag or right-drag: orbit camera
    if (middleBtn && (dx != 0 || dy != 0)) {
        camera_.orbit(dx * 0.005f, dy * 0.005f);
    }
    // Shift + middle-drag: pan
    else if (rightBtn && !middleBtn && (dx != 0 || dy != 0)) {
        camera_.pan(static_cast<float>(-dx), static_cast<float>(dy));
    }

    // Scroll: zoom
    if (dscroll != 0) {
        camera_.zoom(static_cast<float>(dscroll) / 120.0f);
    }

    // Apply gizmo drag while left button is held on a handle
    if (activeTool_ == ActiveTool::Move && gizmo_.isDragging() && leftBtn && (dx != 0 || dy != 0)
        && selection_.hasSelection())
    {
        auto& gd2 = getGraphicsDeviceProperty();
        int sW2 = gd2.getViewportProperty().getWidthProperty();
        int sH2 = gd2.getViewportProperty().getHeightProperty();
        int vX2 = kLeftPanelW, vY2 = kToolbarH;
        int vW2 = std::max(1, sW2 - kLeftPanelW - kRightPanelW);
        int vH2 = std::max(1, sH2 - kToolbarH - kStatusH);
        float asp2 = static_cast<float>(vW2) / static_cast<float>(vH2);
        Matrix vw2 = camera_.viewMatrix();
        Matrix pr2 = camera_.projectionMatrix(asp2);
        Matrix vp2 = vw2 * pr2;

        auto w2s2 = [&](float wx, float wy, float wz) -> std::pair<float,float> {
            float cX = wx*vp2.M11 + wy*vp2.M21 + wz*vp2.M31 + vp2.M41;
            float cY = wx*vp2.M12 + wy*vp2.M22 + wz*vp2.M32 + vp2.M42;
            float cW = wx*vp2.M14 + wy*vp2.M24 + wz*vp2.M34 + vp2.M44;
            if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
            return { (cX/cW * 0.5f + 0.5f) * vW2 + vX2,
                     (1.0f - (cY/cW * 0.5f + 0.5f)) * vH2 + vY2 };
        };

        auto* sel0 = selection_.selection().front().get();
        float px2 = sel0->transform.position[0];
        float py2 = sel0->transform.position[1];
        float pz2 = sel0->transform.position[2];
        float L2  = camera_.distance * 0.15f;
        int axIdx = static_cast<int>(gizmo_.dragAxis()) - 1; // 0=X,1=Y,2=Z
        float tipXYZ[3][3] = {{px2+L2,py2,pz2},{px2,py2+L2,pz2},{px2,py2,pz2+L2}};

        auto [cx2, cy2] = w2s2(px2, py2, pz2);
        auto [tx2, ty2] = w2s2(tipXYZ[axIdx][0], tipXYZ[axIdx][1], tipXYZ[axIdx][2]);
        float axScrX = tx2 - cx2, axScrY = ty2 - cy2;
        float len2d  = std::sqrt(axScrX*axScrX + axScrY*axScrY);
        if (len2d > 0.5f) {
            float dot = dx * (axScrX/len2d) + dy * (axScrY/len2d);
            sel0->transform.position[axIdx] += dot * L2 / len2d;
            modified_ = true;
            updateWindowTitle();
        }
        return; // no other left-button logic during drag
    }

    // Scale gizmo drag
    if (activeTool_ == ActiveTool::Scale && gizmo_.isDragging() && leftBtn && (dx != 0 || dy != 0)
        && selection_.hasSelection())
    {
        auto& gd3 = getGraphicsDeviceProperty();
        int sW3 = gd3.getViewportProperty().getWidthProperty();
        int sH3 = gd3.getViewportProperty().getHeightProperty();
        int vX3 = kLeftPanelW, vY3 = kToolbarH;
        int vW3 = std::max(1, sW3 - kLeftPanelW - kRightPanelW);
        int vH3 = std::max(1, sH3 - kToolbarH - kStatusH);
        float asp3 = static_cast<float>(vW3) / static_cast<float>(vH3);
        Matrix vw3 = camera_.viewMatrix();
        Matrix pr3 = camera_.projectionMatrix(asp3);
        Matrix vp3 = vw3 * pr3;

        auto w2s3 = [&](float wx, float wy, float wz) -> std::pair<float,float> {
            float cX = wx*vp3.M11 + wy*vp3.M21 + wz*vp3.M31 + vp3.M41;
            float cY = wx*vp3.M12 + wy*vp3.M22 + wz*vp3.M32 + vp3.M42;
            float cW = wx*vp3.M14 + wy*vp3.M24 + wz*vp3.M34 + vp3.M44;
            if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
            return { (cX/cW * 0.5f + 0.5f) * vW3 + vX3,
                     (1.0f - (cY/cW * 0.5f + 0.5f)) * vH3 + vY3 };
        };

        auto* sel0 = selection_.selection().front().get();
        float px3 = sel0->transform.position[0];
        float py3 = sel0->transform.position[1];
        float pz3 = sel0->transform.position[2];
        float L3  = camera_.distance * 0.15f;
        int axIdx = static_cast<int>(gizmo_.dragAxis()) - 1;
        float tipXYZ[3][3] = {{px3+L3,py3,pz3},{px3,py3+L3,pz3},{px3,py3,pz3+L3}};

        auto [cx3, cy3] = w2s3(px3, py3, pz3);
        auto [tx3, ty3] = w2s3(tipXYZ[axIdx][0], tipXYZ[axIdx][1], tipXYZ[axIdx][2]);
        float axScrX3 = tx3 - cx3, axScrY3 = ty3 - cy3;
        float len3d   = std::sqrt(axScrX3*axScrX3 + axScrY3*axScrY3);
        if (len3d > 0.5f) {
            float dot = dx * (axScrX3/len3d) + dy * (axScrY3/len3d);
            float& s = sel0->transform.scale[axIdx];
            s = std::max(0.01f, s + dot / len3d);
            modified_ = true;
            updateWindowTitle();
        }
        return;
    }

    // Rotate gizmo drag
    if (activeTool_ == ActiveTool::Rotate && gizmo_.isDragging() && leftBtn && (dx != 0 || dy != 0)
        && selection_.hasSelection())
    {
        auto& gd4 = getGraphicsDeviceProperty();
        int sW4 = gd4.getViewportProperty().getWidthProperty();
        int sH4 = gd4.getViewportProperty().getHeightProperty();
        int vX4 = kLeftPanelW, vY4 = kToolbarH;
        int vW4 = std::max(1, sW4 - kLeftPanelW - kRightPanelW);
        int vH4 = std::max(1, sH4 - kToolbarH - kStatusH);
        float asp4 = static_cast<float>(vW4) / static_cast<float>(vH4);
        Matrix vw4 = camera_.viewMatrix();
        Matrix pr4 = camera_.projectionMatrix(asp4);
        Matrix vp4 = vw4 * pr4;

        auto w2s4 = [&](float wx, float wy, float wz) -> std::pair<float,float> {
            float cX = wx*vp4.M11 + wy*vp4.M21 + wz*vp4.M31 + vp4.M41;
            float cY = wx*vp4.M12 + wy*vp4.M22 + wz*vp4.M32 + vp4.M42;
            float cW = wx*vp4.M14 + wy*vp4.M24 + wz*vp4.M34 + vp4.M44;
            if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
            return { (cX/cW * 0.5f + 0.5f) * vW4 + vX4,
                     (1.0f - (cY/cW * 0.5f + 0.5f)) * vH4 + vY4 };
        };

        auto* sel0 = selection_.selection().front().get();
        float px4 = sel0->transform.position[0];
        float py4 = sel0->transform.position[1];
        float pz4 = sel0->transform.position[2];
        float L4  = camera_.distance * 0.15f;
        int axIdx = static_cast<int>(gizmo_.dragAxis()) - 1;

        auto [cx4, cy4] = w2s4(px4, py4, pz4);
        // Screen-space radius: project a point on the circle perimeter
        float refPts[3][3] = { {px4, py4+L4, pz4}, {px4+L4, py4, pz4}, {px4+L4, py4, pz4} };
        auto [rx4s, ry4s]  = w2s4(refPts[axIdx][0], refPts[axIdx][1], refPts[axIdx][2]);
        float r_screen = std::max(1.0f, std::sqrt((rx4s-cx4)*(rx4s-cx4) + (ry4s-cy4)*(ry4s-cy4)));

        // Tangent at current mouse position (perpendicular to radius from projected centre)
        float curMx = static_cast<float>(ms.getXProperty());
        float curMy = static_cast<float>(ms.getYProperty());
        float radX = curMx - cx4, radY = curMy - cy4;
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

    // Left click
    if (leftBtn && !prevLeft) {
        int mx = ms.getXProperty();
        int my = ms.getYProperty();
        bool ctrl = (Keyboard::GetState().IsKeyDown(Keys::LeftControl) ||
                     Keyboard::GetState().IsKeyDown(Keys::RightControl));

        // Click in toolbar — switch tool or add primitive
        if (my < kToolbarH) {
            cancelField();
            // Tool buttons (Select/Move/Rotate/Scale) at x=4,44,84,124 each 36 wide
            ActiveTool toolMap[] = { ActiveTool::Select, ActiveTool::Move,
                                     ActiveTool::Rotate, ActiveTool::Scale };
            for (int i = 0; i < 4; ++i) {
                int bx = 4 + i * 40;
                if (mx >= bx && mx < bx + 36 && my >= 2 && my < 38) {
                    activeTool_ = toolMap[i];
                    updateWindowTitle();
                    return;
                }
            }
            // Add-primitive buttons at x=175,215,255,295,335 each 36 wide
            Mc3::ObjectType primMap[] = {
                Mc3::ObjectType::Box, Mc3::ObjectType::Sphere,
                Mc3::ObjectType::Cylinder, Mc3::ObjectType::Cone,
                Mc3::ObjectType::Plane
            };
            for (int i = 0; i < 5; ++i) {
                int bx = 175 + i * 40;
                if (mx >= bx && mx < bx + 36 && my >= 2 && my < 38) {
                    addPrimitive(primMap[i]);
                    return;
                }
            }
            return; // click elsewhere in toolbar: ignore
        }

        // Click inside right properties panel — activate a field for editing
        auto& gd0 = getGraphicsDeviceProperty();
        int sW0 = gd0.getViewportProperty().getWidthProperty();
        if (mx >= sW0 - kRightPanelW && my >= kToolbarH + kPanelHdrH && my < sW0) {
            if (fieldActive_) applyFieldValue();
            // Name bar
            if (nameFieldHitY_ >= 0 && my >= nameFieldHitY_ && my < nameFieldHitY_ + 18) {
                activateField(-1, 0);
                return;
            }
            // Transform fields
            for (const auto& hit : propFieldHits_) {
                if (my >= hit.y && my < hit.y + 14) {
                    activateField(hit.section, hit.axis);
                    return;
                }
            }
            // Visible toggle
            if (visToggleHitY_ >= 0 && my >= visToggleHitY_ && my < visToggleHitY_ + 18
                && selection_.hasSelection()) {
                pushUndo();
                selection_.selection().front()->visible = !selection_.selection().front()->visible;
                modified_ = true;
                updateWindowTitle();
                return;
            }
            // Collision field
            if (colFieldHitY_ >= 0 && my >= colFieldHitY_ && my < colFieldHitY_ + 18) {
                activateField(-2, 0);
                return;
            }
            // Click in panel but not on a field — cancel active edit
            if (fieldActive_) cancelField();
            return;
        }

        // Click inside left hierarchy panel
        if (mx < kLeftPanelW && my >= kToolbarH + kPanelHdrH) {
            int row = (my - kToolbarH - kPanelHdrH) / kObjRowH;
            if (row >= 0 && row < static_cast<int>(hierarchyRows_.size())) {
                auto& hr = hierarchyRows_[row];
                bool isGroup = !hr.obj->children.empty() ||
                               hr.obj->type == Mc3::ObjectType::Group ||
                               hr.obj->type == Mc3::ObjectType::Union ||
                               hr.obj->type == Mc3::ObjectType::Difference ||
                               hr.obj->type == Mc3::ObjectType::Intersection;
                // Click on eye icon — toggle visibility without changing selection
                if (mx >= kLeftPanelW - 22 && mx < kLeftPanelW - 10) {
                    pushUndo();
                    hr.obj->visible = !hr.obj->visible;
                    modified_ = true;
                    updateWindowTitle();
                    return;
                }
                // Click on the triangle (expand/collapse region)
                int triX = 5 + hr.depth * 14;
                if (isGroup && mx >= triX && mx < triX + 12) {
                    auto* ptr = hr.obj.get();
                    if (collapsedGroups_.count(ptr)) collapsedGroups_.erase(ptr);
                    else collapsedGroups_.insert(ptr);
                    return;
                }
                // Select the object
                if (!ctrl) selection_.clear();
                selection_.select(hr.obj);
                updateWindowTitle();
            }
            return;
        }

        // Click in 3D viewport
        auto& gd = getGraphicsDeviceProperty();
        int screenW = gd.getViewportProperty().getWidthProperty();
        int screenH = gd.getViewportProperty().getHeightProperty();
        bool in3d = (mx >= kLeftPanelW && mx < screenW - kRightPanelW &&
                     my >= kToolbarH   && my < screenH - kStatusH);
        if (in3d) {
            // Gizmo handle hit test (Move or Scale tool, no Ctrl)
            if ((activeTool_ == ActiveTool::Move || activeTool_ == ActiveTool::Scale)
                && selection_.hasSelection() && !ctrl) {
                auto* sel0 = selection_.selection().front().get();
                float gpx = sel0->transform.position[0];
                float gpy = sel0->transform.position[1];
                float gpz = sel0->transform.position[2];
                float gL  = camera_.distance * 0.15f;

                int gvX = kLeftPanelW, gvY = kToolbarH;
                int gvW = std::max(1, screenW - kLeftPanelW - kRightPanelW);
                int gvH = std::max(1, screenH - kToolbarH - kStatusH);
                float gasp = static_cast<float>(gvW) / static_cast<float>(gvH);
                Matrix gvw = camera_.viewMatrix();
                Matrix gpr = camera_.projectionMatrix(gasp);
                Matrix gvp = gvw * gpr;

                auto gw2s = [&](float wx, float wy, float wz) -> std::pair<float,float> {
                    float cX = wx*gvp.M11 + wy*gvp.M21 + wz*gvp.M31 + gvp.M41;
                    float cY = wx*gvp.M12 + wy*gvp.M22 + wz*gvp.M32 + gvp.M42;
                    float cW = wx*gvp.M14 + wy*gvp.M24 + wz*gvp.M34 + gvp.M44;
                    if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
                    return { (cX/cW * 0.5f + 0.5f) * gvW + gvX,
                             (1.0f - (cY/cW * 0.5f + 0.5f)) * gvH + gvY };
                };

                float gTips[3][3] = {
                    {gpx+gL, gpy,    gpz   },
                    {gpx,    gpy+gL, gpz   },
                    {gpx,    gpy,    gpz+gL},
                };
                for (int gi = 0; gi < 3; ++gi) {
                    auto [gsx, gsy] = gw2s(gTips[gi][0], gTips[gi][1], gTips[gi][2]);
                    float gdist = std::sqrt((mx-gsx)*(mx-gsx) + (my-gsy)*(my-gsy));
                    if (gdist < 12.0f) {
                        pushUndo();
                        gizmo_.startDrag(static_cast<Editor::GizmoAxis>(gi + 1));
                        return; // click consumed by gizmo — skip picking
                    }
                }
            }

            // Gizmo circle hit test (Rotate tool, no Ctrl)
            if (activeTool_ == ActiveTool::Rotate && selection_.hasSelection() && !ctrl) {
                auto* sel0 = selection_.selection().front().get();
                float gpx = sel0->transform.position[0];
                float gpy = sel0->transform.position[1];
                float gpz = sel0->transform.position[2];
                float gL  = camera_.distance * 0.15f;

                int gvX2 = kLeftPanelW, gvY2 = kToolbarH;
                int gvW2 = std::max(1, screenW - kLeftPanelW - kRightPanelW);
                int gvH2 = std::max(1, screenH - kToolbarH - kStatusH);
                float gasp2 = static_cast<float>(gvW2) / static_cast<float>(gvH2);
                Matrix gvw2 = camera_.viewMatrix();
                Matrix gpr2 = camera_.projectionMatrix(gasp2);
                Matrix gvp2 = gvw2 * gpr2;

                auto gw2s2 = [&](float wx, float wy, float wz) -> std::pair<float,float> {
                    float cX = wx*gvp2.M11 + wy*gvp2.M21 + wz*gvp2.M31 + gvp2.M41;
                    float cY = wx*gvp2.M12 + wy*gvp2.M22 + wz*gvp2.M32 + gvp2.M42;
                    float cW = wx*gvp2.M14 + wy*gvp2.M24 + wz*gvp2.M34 + gvp2.M44;
                    if (std::abs(cW) < 1e-6f) return {-1e6f, -1e6f};
                    return { (cX/cW * 0.5f + 0.5f) * gvW2 + gvX2,
                             (1.0f - (cY/cW * 0.5f + 0.5f)) * gvH2 + gvY2 };
                };

                const int CN = 32;
                int bestAx = -1;
                float bestDist = 11.0f; // pixel threshold
                for (int ax = 0; ax < 3; ++ax) {
                    for (int j = 0; j < CN; ++j) {
                        float t = 2.0f * std::numbers::pi_v<float> * j / CN;
                        float c = std::cos(t), s = std::sin(t);
                        float wx, wy, wz;
                        if      (ax == 0) { wx=gpx;      wy=gpy+gL*c; wz=gpz+gL*s; }
                        else if (ax == 1) { wx=gpx+gL*c; wy=gpy;      wz=gpz+gL*s; }
                        else              { wx=gpx+gL*c; wy=gpy+gL*s; wz=gpz;       }
                        auto [sx, sy] = gw2s2(wx, wy, wz);
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

            // Ray-cast picking: unproject click to world-space ray, test AABB per object
            int viewX = kLeftPanelW;
            int viewY = kToolbarH;
            int viewW = std::max(1, screenW - kLeftPanelW - kRightPanelW);
            int viewH = std::max(1, screenH - kToolbarH - kStatusH);
            float ndcX = ((mx - viewX) / static_cast<float>(viewW)) * 2.0f - 1.0f;
            float ndcY = 1.0f - ((my - viewY) / static_cast<float>(viewH)) * 2.0f;
            float aspect = static_cast<float>(viewW) / static_cast<float>(viewH);

            Vector3 rayOrig = camera_.position();
            Vector3 rayDir  = camera_.screenRayDirection(ndcX, ndcY, aspect);

            // Slab-method ray-AABB intersection; returns true and sets tHit if hit
            auto rayAABB = [](const Vector3& ro, const Vector3& rd,
                               const Vector3& bMin, const Vector3& bMax,
                               float& tHit) -> bool {
                float tNear = 0.0f, tFar = 1e30f;
                const float* rov = &ro.X;
                const float* rdv = &rd.X;
                const float* bnv = &bMin.X;
                const float* bxv = &bMax.X;
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

            // Build world-space AABB for object (ignores rotation — AABB wraps shape)
            auto objectAABB = [](const Mc3::Mc3Object& obj,
                                  Vector3& bMin, Vector3& bMax) {
                const auto& t = obj.transform;
                float px = t.position[0], py = t.position[1], pz = t.position[2];
                float sx = t.scale[0],    sy = t.scale[1],    sz = t.scale[2];

                float hx = 0.5f, hy = 0.5f, hz = 0.5f;
                if (obj.primitive) {
                    const auto& p = *obj.primitive;
                    switch (p.primitiveType) {
                    case Mc3::PrimitiveType::Box:
                    case Mc3::PrimitiveType::Cube:
                        hx = p.size[0] * 0.5f;
                        hy = p.size[1] * 0.5f;
                        hz = p.size[2] * 0.5f;
                        break;
                    case Mc3::PrimitiveType::Sphere:
                        hx = hy = hz = p.radius;
                        break;
                    case Mc3::PrimitiveType::Cylinder:
                    case Mc3::PrimitiveType::Cone:
                        hx = hz = p.radius;
                        hy = p.height * 0.5f;
                        break;
                    case Mc3::PrimitiveType::Plane:
                        hx = p.size[0] * 0.5f;
                        hy = 0.05f;
                        hz = p.size[1] * 0.5f;
                        break;
                    }
                }
                hx *= std::abs(sx); hy *= std::abs(sy); hz *= std::abs(sz);
                bMin = { px - hx, py - hy, pz - hz };
                bMax = { px + hx, py + hy, pz + hz };
            };

            // Find closest hit, searching recursively through children
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
                            bestT   = tHit;
                            bestObj = obj;
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
}

// ---------------------------------------------------------------------------
// Scene / file operations
// ---------------------------------------------------------------------------

void MeshCraftApplication::newScene() {
    document_ = Mc3::Mc3Document{};
    document_.model = "Untitled";
    selection_.clear();
    collapsedGroups_.clear();
    hierarchyRows_.clear();
    modified_ = false;
    currentFile_.clear();
    std::cout << "[MeshCraft] New scene\n";
    updateWindowTitle();
}

void MeshCraftApplication::openFile() {
    // Ask user for path via stdin (console) since no file dialog available in CNA yet
    std::cout << "[MeshCraft] Enter file path to open (or press Enter to cancel): ";
    std::string path;
    std::getline(std::cin, path);
    if (path.empty()) return;

    try {
        document_ = Mc3::Mc3Document::loadFromFile(path);
        currentFile_ = path;
        selection_.clear();
        modified_ = false;
        std::cout << "[MeshCraft] Loaded: " << path << "\n";
        updateWindowTitle();
    } catch (const std::exception& e) {
        std::cerr << "[MeshCraft] Error: " << e.what() << "\n";
    }
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
    std::cout << "[MeshCraft] Enter save path (e.g. scene.mc3.xml): ";
    std::string path;
    std::getline(std::cin, path);
    if (path.empty()) return;
    if (path.find(".mc3.xml") == std::string::npos)
        path += ".mc3.xml";
    currentFile_ = path;
    saveFile();
}

void MeshCraftApplication::exportGltf() {
    if (currentFile_.empty()) {
        std::cerr << "[MeshCraft] Save the file first before exporting.\n";
        return;
    }
    // Find mc3togltf binary
    std::string mc3togltf = "mc3togltf";
    // Try relative path (sibling build-mc3togltf or cmake-build-debug)
    auto exePath  = std::filesystem::current_path();
    for (const auto& candidate : {
        std::filesystem::path("build-mc3togltf/mc3togltf"),
        std::filesystem::path("cmake-build-debug/mc3togltf/mc3togltf"),
        std::filesystem::path("../build-mc3togltf/mc3togltf"),
    }) {
        if (std::filesystem::exists(candidate)) {
            mc3togltf = candidate.string();
            break;
        }
    }
    std::string outPath = currentFile_.string();
    // Replace .mc3.xml with .glb
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
    case Mc3::ObjectType::Box:
        prim.primitiveType = Mc3::PrimitiveType::Box;
        prim.size = {1.0f, 1.0f, 1.0f};
        break;
    case Mc3::ObjectType::Sphere:
        prim.primitiveType = Mc3::PrimitiveType::Sphere;
        prim.radius = 0.5f;
        break;
    case Mc3::ObjectType::Cylinder:
        prim.primitiveType = Mc3::PrimitiveType::Cylinder;
        prim.radius = 0.5f; prim.height = 1.0f;
        break;
    case Mc3::ObjectType::Cone:
        prim.primitiveType = Mc3::PrimitiveType::Cone;
        prim.radius = 0.5f; prim.height = 1.0f;
        break;
    case Mc3::ObjectType::Plane:
        prim.primitiveType = Mc3::PrimitiveType::Plane;
        prim.size = {1.0f, 0.0f, 1.0f};
        break;
    default:
        break;
    }
    obj->primitive = prim;

    // Place at camera target so it's visible
    obj->transform.position = {
        camera_.target.X,
        camera_.target.Y + 0.5f,
        camera_.target.Z
    };

    // If a group-like object is selected, insert as its child
    if (selection_.hasSelection()) {
        auto& sel0 = selection_.selection().front();
        bool isGroup = sel0->type == Mc3::ObjectType::Group   ||
                       sel0->type == Mc3::ObjectType::Union   ||
                       sel0->type == Mc3::ObjectType::Difference ||
                       sel0->type == Mc3::ObjectType::Intersection ||
                       !sel0->children.empty();
        if (isGroup) {
            sel0->children.push_back(obj);
            selection_.clear();
            selection_.select(obj);
            modified_ = true;
            std::cout << "[MeshCraft] Added " << obj->name << " as child\n";
            updateWindowTitle();
            return;
        }
    }

    document_.objects.push_back(obj);
    selection_.clear();
    selection_.select(obj);
    modified_ = true;
    std::cout << "[MeshCraft] Added " << obj->name << "\n";
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
    std::cout << "[MeshCraft] Deleted selected objects\n";
    updateWindowTitle();
}

// ---------------------------------------------------------------------------
// Helpers for duplicate

static std::shared_ptr<Mc3::Mc3Object> deepCopyObject(const Mc3::Mc3Object& src) {
    auto copy = std::make_shared<Mc3::Mc3Object>(src);
    copy->children.clear();
    for (const auto& child : src.children)
        copy->children.push_back(deepCopyObject(*child));
    return copy;
}

static std::vector<std::shared_ptr<Mc3::Mc3Object>>*
findParentList(std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
               const Mc3::Mc3Object* target)
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
    auto prev = selection_.selection(); // copy list before we mutate selection
    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;

    for (const auto& s : prev) {
        auto* parent = findParentList(document_.objects, s.get());
        if (!parent) continue;

        auto copy = deepCopyObject(*s);
        copy->name = s->name + "_copy";

        // Insert immediately after the original
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
        std::cout << "[MeshCraft] Duplicated " << newObjs.size() << " object(s)\n";
        updateWindowTitle();
    }
}

// ---------------------------------------------------------------------------
// Cut / Copy / Paste
// ---------------------------------------------------------------------------

void MeshCraftApplication::copySelected() {
    if (!selection_.hasSelection()) return;
    clipboard_.clear();
    for (const auto& s : selection_.selection())
        clipboard_.push_back(deepCopyObject(*s));
    std::cout << "[MeshCraft] Copied " << clipboard_.size() << " object(s)\n";
}

void MeshCraftApplication::cutSelected() {
    if (!selection_.hasSelection()) return;
    copySelected();
    deleteSelected(); // pushUndo is called inside deleteSelected
}

void MeshCraftApplication::pasteClipboard() {
    if (clipboard_.empty()) return;
    pushUndo();

    std::vector<std::shared_ptr<Mc3::Mc3Object>> newObjs;
    for (const auto& src : clipboard_) {
        auto copy = deepCopyObject(*src);
        // Offset slightly so paste doesn't land exactly on top of original
        copy->transform.position[0] += 1.0f;
        newObjs.push_back(copy);
    }

    // Paste into selected group, or at top level
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
            std::cout << "[MeshCraft] Pasted " << newObjs.size() << " object(s) into group\n";
            updateWindowTitle();
            return;
        }
    }

    for (auto& o : newObjs) document_.objects.push_back(o);
    selection_.clear();
    for (auto& o : newObjs) selection_.select(o);
    modified_ = true;
    std::cout << "[MeshCraft] Pasted " << newObjs.size() << " object(s)\n";
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

    const char* toolNames[] = {
        "Select","Move","Rotate","Scale","Add Box","Add Sphere","Add Cylinder","Add Cone","Add Plane"
    };
    title += " | Tool: ";
    title += toolNames[static_cast<int>(activeTool_)];

    getWindowProperty().setTitleProperty(title);
}

// ---------------------------------------------------------------------------
// UI drawing helpers
// ---------------------------------------------------------------------------

void MeshCraftApplication::drawRect(int x, int y, int w, int h, Color col) {
    if (w <= 0 || h <= 0) return;
    spriteBatch_->Draw(whitePx_,
                       Rectangle(x, y, w, h),
                       Rectangle(0, 0, 1, 1),
                       col);
}

Color MeshCraftApplication::objectTypeColor(Mc3::ObjectType type) const {
    switch (type) {
    case Mc3::ObjectType::Box:
    case Mc3::ObjectType::Cube:        return Color(210, 120, 55, 255);
    case Mc3::ObjectType::Sphere:      return Color(55, 120, 210, 255);
    case Mc3::ObjectType::Cylinder:    return Color(55, 185, 100, 255);
    case Mc3::ObjectType::Cone:        return Color(185, 55, 185, 255);
    case Mc3::ObjectType::Plane:       return Color(205, 205, 55, 255);
    case Mc3::ObjectType::Group:       return Color(160, 160, 160, 255);
    case Mc3::ObjectType::Extrude:     return Color(55, 205, 185, 255);
    case Mc3::ObjectType::Instance:    return Color(185, 160, 55, 255);
    case Mc3::ObjectType::Union:       return Color(55, 185, 55, 255);
    case Mc3::ObjectType::Difference:  return Color(205, 55, 55, 255);
    case Mc3::ObjectType::Intersection:return Color(55, 120, 185, 255);
    default:                           return Color(140, 140, 140, 255);
    }
}

void MeshCraftApplication::drawUi(int screenW, int screenH) {
    // -----------------------------------------------------------------------
    // Toolbar (top)
    // -----------------------------------------------------------------------
    drawRect(0, 0, screenW, kToolbarH, Color(22, 24, 45, 255));
    // bottom separator
    drawRect(0, kToolbarH - 1, screenW, 1, Color(55, 60, 100, 255));

    // Tool buttons: Q=Select G=Move R=Rotate S=Scale
    struct ToolBtn { ActiveTool tool; Color activeCol; };
    ToolBtn toolBtns[] = {
        {ActiveTool::Select,  Color(80, 150, 210, 255)},
        {ActiveTool::Move,    Color(55, 190, 100, 255)},
        {ActiveTool::Rotate,  Color(205, 165, 55, 255)},
        {ActiveTool::Scale,   Color(210, 75, 75, 255)},
    };
    for (int i = 0; i < 4; ++i) {
        bool active = (activeTool_ == toolBtns[i].tool);
        int bx = 4 + i * 40;
        Color bg = active ? toolBtns[i].activeCol : Color(38, 44, 72, 255);
        drawRect(bx,      2, 36, 36, bg);
        drawRect(bx,      2, 36,  1, Color(75, 85, 120, 255)); // top border
        drawRect(bx,     37, 36,  1, Color(75, 85, 120, 255)); // bottom
        drawRect(bx,      2,  1, 36, Color(75, 85, 120, 255)); // left
        drawRect(bx + 35, 2,  1, 36, Color(75, 85, 120, 255)); // right
        // inner icon: a small filled square
        Color ico = active ? Color(255, 255, 255, 200) : Color(160, 170, 200, 255);
        drawRect(bx + 13, 15, 10, 10, ico);
    }

    // Add-primitive buttons (right of tool buttons)
    Color addCols[] = {
        Color(210, 120, 55, 255),  // Box
        Color(55, 120, 210, 255),  // Sphere
        Color(55, 185, 100, 255),  // Cylinder
        Color(185, 55, 185, 255),  // Cone
        Color(205, 205, 55, 255),  // Plane
    };
    for (int i = 0; i < 5; ++i) {
        int bx = 175 + i * 40;
        drawRect(bx,      2, 36, 36, addCols[i]);
        drawRect(bx,      2, 36,  1, Color(200, 200, 200, 120));
        drawRect(bx,     37, 36,  1, Color(200, 200, 200, 120));
        drawRect(bx,      2,  1, 36, Color(200, 200, 200, 120));
        drawRect(bx + 35, 2,  1, 36, Color(200, 200, 200, 120));
        // "+" symbol: vertical + horizontal bars
        drawRect(bx + 17, 10, 2, 20, Color(255, 255, 255, 220));
        drawRect(bx + 10, 17, 16, 2, Color(255, 255, 255, 220));
    }

    // Separator between toolbar sections
    drawRect(170, 4, 1, 32, Color(75, 85, 120, 255));

    // -----------------------------------------------------------------------
    // Left panel — Scene Hierarchy
    // -----------------------------------------------------------------------
    int panelH = screenH - kToolbarH - kStatusH;
    drawRect(0, kToolbarH, kLeftPanelW, panelH, Color(26, 28, 50, 240));
    // right edge
    drawRect(kLeftPanelW - 1, kToolbarH, 1, panelH, Color(55, 60, 100, 255));

    // Header
    drawRect(0, kToolbarH, kLeftPanelW - 1, kPanelHdrH, Color(38, 42, 72, 255));
    // accent strip
    drawRect(0, kToolbarH, 4, kPanelHdrH, Color(80, 130, 210, 255));
    // header bottom line
    drawRect(0, kToolbarH + kPanelHdrH - 1, kLeftPanelW - 1, 1, Color(55, 60, 100, 255));

    // Helper: wrap drawRect for BitmapFont callback
    auto fillRect = [this](int x, int y, int w, int h, Color c) { drawRect(x, y, w, h, c); };

    // "SCENE" panel header label
    Ui::drawBitmapText("SCENE", 8, kToolbarH + (kPanelHdrH - 7) / 2, 1,
                       Color(160, 175, 210, 255), fillRect);

    // Object list rows — recursive tree with indent and expand/collapse
    const auto& objs = document_.objects;
    int rowY = kToolbarH + kPanelHdrH;
    int maxY = screenH - kStatusH - kObjRowH;
    hierarchyRows_.clear();

    std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&, int)> drawObjList;
    drawObjList = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list, int depth) {
        for (const auto& obj : list) {
            if (rowY > maxY) break;

            bool sel      = selection_.isSelected(obj.get());
            bool isGroup  = !obj->children.empty() ||
                            obj->type == Mc3::ObjectType::Group ||
                            obj->type == Mc3::ObjectType::Union ||
                            obj->type == Mc3::ObjectType::Difference ||
                            obj->type == Mc3::ObjectType::Intersection;
            bool expanded = !collapsedGroups_.count(obj.get());

            hierarchyRows_.push_back({depth, obj});
            int rowIdx = static_cast<int>(hierarchyRows_.size()) - 1;

            int indent = depth * 14;
            int iconX  = 7 + indent;
            int textX  = iconX + 15;

            // Row background
            Color rowBg = sel
                ? ((rowIdx % 2 == 0) ? Color(62, 70, 118, 255) : Color(58, 66, 112, 255))
                : ((rowIdx % 2 == 0) ? Color(32, 35, 60, 255)  : Color(28, 31, 54, 255));
            drawRect(0, rowY, kLeftPanelW - 1, kObjRowH, rowBg);

            // Type color strip (always at x=0)
            Color tc = objectTypeColor(obj->type);
            drawRect(0, rowY, 5, kObjRowH, tc);

            // Vertical indent guide for children
            if (depth > 0) {
                drawRect(5 + (depth - 1) * 14 + 9, rowY, 1, kObjRowH,
                         Color(55, 60, 90, 200));
            }

            // Expand/collapse triangle (">" = collapsed, "v" = expanded)
            if (isGroup) {
                const char* tri = expanded ? "v" : ">";
                Ui::drawBitmapText(tri, 5 + indent + 1, rowY + (kObjRowH - 7) / 2,
                                   1, Color(160, 175, 210, 200), fillRect);
            }

            // Type icon
            drawRect(iconX, rowY + (kObjRowH - 10) / 2, 10, 10, tc);

            // Object name (truncated to fit, leaving room for eye icon)
            {
                const std::string& name = obj->name.empty() ? obj->id : obj->name;
                int maxChars = std::max(1, (kLeftPanelW - 22 - textX - 2) / 6);
                std::string label = (int)name.size() > maxChars
                                    ? name.substr(0, maxChars - 1) + "~" : name;
                Color textCol = !obj->visible ? Color(110, 115, 130, 255)
                               : sel          ? Color(235, 240, 255, 255)
                                              : Color(185, 195, 215, 255);
                Ui::drawBitmapText(label, textX, rowY + (kObjRowH - 7) / 2, 1,
                                   textCol, fillRect);
            }

            // Eye icon (visibility toggle) — drawn before selection indicator
            {
                int eyeX = kLeftPanelW - 21;
                int eyeY = rowY + (kObjRowH - 8) / 2;
                Color eyeCol = obj->visible ? Color(60, 190, 90, 210) : Color(70, 70, 80, 160);
                drawRect(eyeX, eyeY, 10, 8, eyeCol);
                if (obj->visible)
                    drawRect(eyeX + 3, eyeY + 2, 4, 4, Color(20, 90, 35, 255));
            }

            // Selection right indicator
            if (sel) {
                drawRect(kLeftPanelW - 8, rowY, 7, kObjRowH, Color(80, 130, 210, 255));
            }

            // Row separator
            drawRect(5, rowY + kObjRowH - 1, kLeftPanelW - 6, 1, Color(40, 44, 72, 100));

            rowY += kObjRowH;

            // Recurse into children when expanded
            if (isGroup && expanded && !obj->children.empty()) {
                drawObjList(obj->children, depth + 1);
            }
        }
    };
    drawObjList(objs, 0);

    // -----------------------------------------------------------------------
    // Right panel — Properties
    // -----------------------------------------------------------------------
    int rpX = screenW - kRightPanelW;
    drawRect(rpX, kToolbarH, kRightPanelW, panelH, Color(26, 28, 50, 240));
    // left edge
    drawRect(rpX, kToolbarH, 1, panelH, Color(55, 60, 100, 255));

    // Header
    drawRect(rpX + 1, kToolbarH, kRightPanelW - 1, kPanelHdrH, Color(38, 42, 72, 255));
    drawRect(rpX + kRightPanelW - 4, kToolbarH, 3, kPanelHdrH, Color(210, 110, 80, 255));
    drawRect(rpX + 1, kToolbarH + kPanelHdrH - 1, kRightPanelW - 1, 1, Color(55, 60, 100, 255));
    Ui::drawBitmapText("PROPERTIES", rpX + 8, kToolbarH + (kPanelHdrH - 7) / 2, 1,
                       Color(160, 175, 210, 255), fillRect);

    propFieldHits_.clear();
    nameFieldHitY_  = -1;
    visToggleHitY_  = -1;
    colFieldHitY_   = -1;
    if (selection_.hasSelection()) {
        const auto& sel0 = selection_.selection().front();
        int py = kToolbarH + kPanelHdrH + 6;
        int pw = kRightPanelW - 12;
        int px = rpX + 6;

        // Object type indicator + name (clickable to rename)
        nameFieldHitY_ = py;
        bool nameActive = fieldActive_ && fieldSection_ == -1;
        Color tc = nameActive ? Color(50, 70, 140, 255) : objectTypeColor(sel0->type);
        drawRect(px, py, pw, 18, tc);
        {
            std::string display;
            if (nameActive) {
                display = fieldBuffer_ + "_";
                Ui::drawBitmapText(display, px + 4, py + (18 - 7) / 2, 1, Color(255, 255, 160, 255), fillRect);
            } else {
                const std::string& nm = sel0->name.empty() ? sel0->id : sel0->name;
                Ui::drawBitmapText(nm, px + 4, py + (18 - 7) / 2, 1, Color(255, 255, 255, 255), fillRect);
            }
        }
        py += 24;

        const char* axisLabels[3] = {"X", "Y", "Z"};
        Color axisCols[3] = {Color(210, 60, 60, 255), Color(60, 210, 60, 255), Color(60, 60, 210, 255)};

        // Helper: draw one editable transform field row
        // section: 0=POS, 1=ROT, 2=SCL  axis: 0=X,1=Y,2=Z  v: current value  barScale: max value for bar
        auto drawField = [&](int section, int axis, float v, float barScale, const char* fmt) {
            bool active = fieldActive_ && fieldSection_ == section && fieldAxis_ == axis;
            propFieldHits_.push_back({py, section, axis});

            if (active) {
                // Highlighted background for active editing field
                drawRect(px, py, pw, 14, Color(50, 70, 140, 255));
                drawRect(px, py, 2, 14, axisCols[axis]);
            } else {
                int filled = static_cast<int>(std::clamp(std::abs(v) / barScale, 0.0f, 1.0f) * (pw - 4));
                drawRect(px, py, pw, 14, Color(22, 24, 44, 255));
                drawRect(px, py, std::max(2, filled + 2), 14, axisCols[axis]);
            }

            Ui::drawBitmapText(axisLabels[axis], px + 3, py + (14 - 7) / 2, 1,
                               Color(220, 220, 220, 255), fillRect);

            if (active) {
                std::string display = fieldBuffer_ + "_";
                Ui::drawBitmapText(display, px + 12, py + (14 - 7) / 2, 1,
                                   Color(255, 255, 160, 255), fillRect);
            } else {
                char buf[20]; std::snprintf(buf, sizeof(buf), fmt, v);
                Ui::drawBitmapText(buf, px + 12, py + (14 - 7) / 2, 1,
                                   Color(220, 220, 220, 255), fillRect);
            }
            py += 16;
        };

        // ----- Position section -----
        drawRect(px, py, pw, 18, Color(38, 42, 72, 255));
        drawRect(px, py, 3, 18, Color(80, 130, 210, 255));
        Ui::drawBitmapText("POS", px + 6, py + (18 - 7) / 2, 1, Color(160, 175, 210, 255), fillRect);
        py += 20;
        for (int a = 0; a < 3; ++a)
            drawField(0, a, sel0->transform.position[a], 20.0f, "%.2f");
        py += 4;

        // ----- Rotation section -----
        drawRect(px, py, pw, 18, Color(38, 42, 72, 255));
        drawRect(px, py, 3, 18, Color(205, 165, 55, 255));
        Ui::drawBitmapText("ROT", px + 6, py + (18 - 7) / 2, 1, Color(160, 175, 210, 255), fillRect);
        py += 20;
        for (int a = 0; a < 3; ++a)
            drawField(1, a, sel0->transform.rotation[a], 360.0f, "%.1f");
        py += 4;

        // ----- Scale section -----
        drawRect(px, py, pw, 18, Color(38, 42, 72, 255));
        drawRect(px, py, 3, 18, Color(210, 75, 75, 255));
        Ui::drawBitmapText("SCL", px + 6, py + (18 - 7) / 2, 1, Color(160, 175, 210, 255), fillRect);
        py += 20;
        for (int a = 0; a < 3; ++a)
            drawField(2, a, sel0->transform.scale[a], 4.0f, "%.2f");
        py += 8;

        // Material color swatch
        if (!sel0->material.empty()) {
            drawRect(px, py, pw, 18, Color(38, 42, 72, 255));
            drawRect(px, py, 3, 18, Color(55, 185, 185, 255));
            Ui::drawBitmapText("MTL", px + 6, py + (18 - 7) / 2, 1, Color(160, 175, 210, 255), fillRect);
            Ui::drawBitmapText(sel0->material, px + 28, py + (18 - 7) / 2, 1, Color(185, 220, 220, 255), fillRect);
            for (const auto& [key, mat] : document_.materials) {
                if (key == sel0->material) {
                    Color mc(
                        static_cast<int>(std::clamp(mat.baseColor[0], 0.0f, 1.0f) * 255),
                        static_cast<int>(std::clamp(mat.baseColor[1], 0.0f, 1.0f) * 255),
                        static_cast<int>(std::clamp(mat.baseColor[2], 0.0f, 1.0f) * 255),
                        255);
                    drawRect(rpX + kRightPanelW - 28, py, 22, 18, mc);
                    break;
                }
            }
            py += 22;
        }
        py += 4;

        // Visible toggle
        visToggleHitY_ = py;
        {
            bool vis = sel0->visible;
            drawRect(px, py, pw, 18, Color(38, 42, 72, 255));
            drawRect(px, py, 3, 18, Color(55, 185, 120, 255));
            Ui::drawBitmapText("VIS", px + 6, py + (18 - 7) / 2, 1, Color(160, 175, 210, 255), fillRect);
            Ui::drawBitmapText(vis ? "ON" : "OFF", px + 28, py + (18 - 7) / 2, 1,
                               Color(185, 220, 185, 255), fillRect);
            Color indCol = vis ? Color(60, 200, 80, 255) : Color(160, 50, 50, 255);
            drawRect(rpX + kRightPanelW - 28, py + 3, 22, 12, indCol);
        }
        py += 22;

        // Collision field (click to edit string)
        colFieldHitY_ = py;
        {
            bool colActive = fieldActive_ && fieldSection_ == -2;
            drawRect(px, py, pw, 18, colActive ? Color(50, 70, 140, 255) : Color(38, 42, 72, 255));
            drawRect(px, py, 3, 18, Color(185, 120, 55, 255));
            Ui::drawBitmapText("COL", px + 6, py + (18 - 7) / 2, 1, Color(160, 175, 210, 255), fillRect);
            if (colActive) {
                std::string display = fieldBuffer_ + "_";
                Ui::drawBitmapText(display, px + 28, py + (18 - 7) / 2, 1,
                                   Color(255, 255, 160, 255), fillRect);
            } else {
                Ui::drawBitmapText(sel0->collision, px + 28, py + (18 - 7) / 2, 1,
                                   Color(185, 220, 220, 255), fillRect);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Status bar (bottom)
    // -----------------------------------------------------------------------
    drawRect(0, screenH - kStatusH, screenW, kStatusH, Color(22, 24, 45, 255));
    drawRect(0, screenH - kStatusH, screenW, 1, Color(55, 60, 100, 255));

    // Object count dots (type-colored, up to 24)
    int dotX = 6;
    int dotSize = 14;
    size_t limit = std::min(objs.size(), static_cast<size_t>(24));
    for (size_t i = 0; i < limit; ++i) {
        bool sel = selection_.isSelected(objs[i].get());
        Color dc = sel ? Color(255, 255, 255, 255) : objectTypeColor(objs[i]->type);
        drawRect(dotX, screenH - kStatusH + 5, dotSize, dotSize, dc);
        if (sel) drawRect(dotX, screenH - kStatusH + 5, dotSize, 2, Color(255, 255, 100, 255));
        dotX += dotSize + 2;
    }

    // Status info text: "N objects · M selected"
    {
        int totalObjs = 0;
        std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> countAll =
            [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
                totalObjs += static_cast<int>(list.size());
                for (const auto& o : list) countAll(o->children);
            };
        countAll(objs);
        int selCount = static_cast<int>(selection_.selection().size());

        char infoBuf[48];
        if (selCount > 0)
            std::snprintf(infoBuf, sizeof(infoBuf), "%d objects  %d selected", totalObjs, selCount);
        else
            std::snprintf(infoBuf, sizeof(infoBuf), "%d objects", totalObjs);

        int textX = dotX + 8;
        int textY = screenH - kStatusH + (kStatusH - 7) / 2;
        Ui::drawBitmapText(infoBuf, textX, textY, 1, Color(130, 145, 185, 255), fillRect);
    }

    // Selection count indicator strip on far right
    if (selection_.hasSelection()) {
        int selCount = static_cast<int>(selection_.selection().size());
        int barW = std::min(selCount * 16, 80);
        drawRect(screenW - barW - 4, screenH - kStatusH + 4, barW, dotSize,
                 Color(80, 130, 210, 255));
    }

    // Tool indicator strip at bottom right corner
    Color toolStrip(40, 50, 80, 255);
    drawRect(screenW - kRightPanelW, screenH - kStatusH + 2, 20, dotSize, toolStrip);
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
    Mc3::Mc3Document copy = src;  // value fields (strings, maps of values) copy correctly
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
// Field editing
// ---------------------------------------------------------------------------

void MeshCraftApplication::activateField(int section, int axis) {
    if (!selection_.hasSelection()) return;
    fieldSection_ = section;
    fieldAxis_    = axis;
    if (section == -1) {
        fieldBuffer_ = selection_.selection().front()->name;
    } else if (section == -2) {
        fieldBuffer_ = selection_.selection().front()->collision;
    } else {
        const auto& t = selection_.selection().front()->transform;
        float v = 0.0f;
        if (section == 0) v = t.position[axis];
        else if (section == 1) v = t.rotation[axis];
        else                   v = t.scale[axis];
        char buf[20];
        std::snprintf(buf, sizeof(buf), "%g", v);
        fieldBuffer_ = buf;
    }
    fieldActive_ = true;
}

void MeshCraftApplication::applyFieldValue() {
    if (!fieldActive_ || !selection_.hasSelection()) { cancelField(); return; }
    pushUndo();
    if (fieldSection_ == -1) {
        selection_.selection().front()->name = fieldBuffer_;
        modified_ = true;
        updateWindowTitle();
    } else if (fieldSection_ == -2) {
        selection_.selection().front()->collision = fieldBuffer_;
        modified_ = true;
        updateWindowTitle();
    } else {
        try {
            float val = std::stof(fieldBuffer_);
            auto& t = selection_.selection().front()->transform;
            if (fieldSection_ == 0) t.position[fieldAxis_] = val;
            else if (fieldSection_ == 1) t.rotation[fieldAxis_] = val;
            else                         t.scale[fieldAxis_]    = val;
            modified_ = true;
            updateWindowTitle();
        } catch (...) {}
    }
    fieldActive_ = false;
    fieldBuffer_.clear();
}

void MeshCraftApplication::cancelField() {
    fieldActive_ = false;
    fieldBuffer_.clear();
}

// ---------------------------------------------------------------------------
// Screenshot
// ---------------------------------------------------------------------------

void MeshCraftApplication::saveScreenshot(const std::string& path) {
    auto& gd = getGraphicsDeviceProperty();
    int w = gd.getViewportProperty().getWidthProperty();
    int h = gd.getViewportProperty().getHeightProperty();
    if (w <= 0 || h <= 0) return;

    using PFNGLFINISH = void(*)();
    using PFNGLBINDBUFFER = void(*)(unsigned int, unsigned int);
    using PFNGLREADPIXELS = void(*)(int, int, int, int, unsigned int, unsigned int, void*);
    auto fnFinish     = reinterpret_cast<PFNGLFINISH>    (SDL_GL_GetProcAddress("glFinish"));
    auto fnBindBuffer = reinterpret_cast<PFNGLBINDBUFFER>(SDL_GL_GetProcAddress("glBindBuffer"));
    auto fnReadPixels = reinterpret_cast<PFNGLREADPIXELS>(SDL_GL_GetProcAddress("glReadPixels"));
    if (!fnReadPixels) {
        std::cerr << "[Screenshot] glReadPixels not available\n";
        return;
    }
    if (fnFinish) fnFinish();
    if (fnBindBuffer) fnBindBuffer(0x88EC, 0); // GL_PIXEL_PACK_BUFFER = 0x88EC

    // OpenGL ES guarantees RGBA + GL_UNSIGNED_BYTE
    constexpr unsigned int GL_RGBA          = 0x1908;
    constexpr unsigned int GL_UNSIGNED_BYTE = 0x1401;
    std::vector<unsigned char> pixels(w * h * 4);
    fnReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // PPM P6 is RGB; strip alpha. glReadPixels gives bottom-to-top — flip vertically.
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int row = h - 1; row >= 0; --row) {
        for (int col = 0; col < w; ++col) {
            int idx = (row * w + col) * 4;
            f.write(reinterpret_cast<char*>(&pixels[idx]), 3);
        }
    }
    std::cout << "[Screenshot] written " << path << "\n";
}

} // namespace MeshCraft
