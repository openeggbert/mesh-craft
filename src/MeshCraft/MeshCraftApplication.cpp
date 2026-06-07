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

    // Restrict rendering to 3D viewport
    Graphics::Viewport vp3d;
    vp3d.x = viewX;
    vp3d.y = viewY;
    vp3d.setWidthProperty(viewW);
    vp3d.setHeightProperty(viewH);
    gd.setViewportProperty(vp3d);

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
    gd.Clear(bgColor);

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

    // Restore full viewport and draw 2D UI overlay
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

static bool justPressed(const KeyboardState& cur, const KeyboardState& prev, Keys k) {
    return cur.IsKeyDown(k) && prev.IsKeyUp(k);
}

void MeshCraftApplication::handleKeyboardShortcuts(const KeyboardState& ks, const KeyboardState& prevKs) {
    bool ctrl  = ks.IsKeyDown(Keys::LeftControl)  || ks.IsKeyDown(Keys::RightControl);
    bool shift = ks.IsKeyDown(Keys::LeftShift)    || ks.IsKeyDown(Keys::RightShift);
    bool alt   = ks.IsKeyDown(Keys::LeftAlt)      || ks.IsKeyDown(Keys::RightAlt);

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

    // Camera reset
    if (!ctrl && justPressed(ks, prevKs, Keys::F)) { camera_.reset(); return; }

    // Select all
    if (ctrl && justPressed(ks, prevKs, Keys::A)) {
        selection_.clear();
        for (auto& o : document_.objects) selection_.select(o);
        updateWindowTitle();
        return;
    }

    // Nudge selected objects with arrow keys
    if (!selection_.hasSelection()) return;
    float nudge = (shift ? 0.1f : 1.0f);
    bool nudged = false;
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

    // Left click
    if (leftBtn && !prevLeft) {
        int mx = ms.getXProperty();
        int my = ms.getYProperty();
        bool ctrl = (Keyboard::GetState().IsKeyDown(Keys::LeftControl) ||
                     Keyboard::GetState().IsKeyDown(Keys::RightControl));

        // Click inside left hierarchy panel
        if (mx < kLeftPanelW && my >= kToolbarH + kPanelHdrH) {
            int row = (my - kToolbarH - kPanelHdrH) / kObjRowH;
            if (row >= 0 && row < static_cast<int>(document_.objects.size())) {
                if (!ctrl) selection_.clear();
                selection_.select(document_.objects[row]);
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
            if (!ctrl) selection_.clear();
            updateWindowTitle();
            // TODO: ray-cast pick into scene
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

    document_.objects.push_back(obj);
    selection_.clear();
    selection_.select(obj);
    modified_ = true;
    std::cout << "[MeshCraft] Added " << obj->name << "\n";
    updateWindowTitle();
}

void MeshCraftApplication::deleteSelected() {
    auto& sel = selection_.selection();
    for (const auto& s : sel) {
        auto& objs = document_.objects;
        objs.erase(
            std::remove_if(objs.begin(), objs.end(),
                [&](const auto& o){ return o.get() == s.get(); }),
            objs.end());
    }
    // Also check children (recursive)
    // For simplicity: only top-level deletion for now
    selection_.clear();
    modified_ = true;
    std::cout << "[MeshCraft] Deleted selected objects\n";
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

    // Object list rows
    const auto& objs = document_.objects;
    int rowY = kToolbarH + kPanelHdrH;
    int maxY  = screenH - kStatusH - kObjRowH;
    for (size_t i = 0; i < objs.size() && rowY <= maxY; ++i) {
        const auto& obj = objs[i];
        bool sel = selection_.isSelected(obj.get());

        Color rowBg = sel
            ? ((i % 2 == 0) ? Color(62, 70, 118, 255) : Color(58, 66, 112, 255))
            : ((i % 2 == 0) ? Color(32, 35, 60, 255)  : Color(28, 31, 54, 255));
        drawRect(0, rowY, kLeftPanelW - 1, kObjRowH, rowBg);

        // Type color strip on left
        Color tc = objectTypeColor(obj->type);
        drawRect(0, rowY, 5, kObjRowH, tc);

        // Inner icon: small colored square
        drawRect(10, rowY + 5, 12, 12, tc);

        // Selection right indicator
        if (sel) {
            drawRect(kLeftPanelW - 8, rowY, 7, kObjRowH, Color(80, 130, 210, 255));
        }

        // Row bottom separator
        drawRect(5, rowY + kObjRowH - 1, kLeftPanelW - 6, 1, Color(40, 44, 72, 100));

        rowY += kObjRowH;
    }

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

    if (selection_.hasSelection()) {
        const auto& sel0 = selection_.selection().front();
        int py = kToolbarH + kPanelHdrH + 6;
        int pw = kRightPanelW - 12;
        int px = rpX + 6;

        // Object type indicator
        Color tc = objectTypeColor(sel0->type);
        drawRect(px, py, pw, 18, tc);
        py += 24;

        // ----- Position section -----
        drawRect(px, py, pw, 18, Color(38, 42, 72, 255));
        drawRect(px, py, 3, 18, Color(80, 130, 210, 255));
        py += 20;
        const char* axisLabels[3] = {"X", "Y", "Z"};
        Color axisCols[3] = {Color(210, 60, 60, 255), Color(60, 210, 60, 255), Color(60, 60, 210, 255)};
        for (int a = 0; a < 3; ++a) {
            float v = sel0->transform.position[a];
            int filled = static_cast<int>(std::clamp(std::abs(v) / 20.0f, 0.0f, 1.0f) * (pw - 4));
            drawRect(px, py, pw, 14, Color(22, 24, 44, 255));
            drawRect(px, py, filled + 2, 14, axisCols[a]);
            drawRect(px, py, pw, 14, Color(0, 0, 0, 0)); // transparent overlay space
            py += 16;
        }
        py += 4;

        // ----- Rotation section -----
        drawRect(px, py, pw, 18, Color(38, 42, 72, 255));
        drawRect(px, py, 3, 18, Color(205, 165, 55, 255));
        py += 20;
        for (int a = 0; a < 3; ++a) {
            float v = sel0->transform.rotation[a];
            int filled = static_cast<int>(std::clamp(std::abs(v) / 360.0f, 0.0f, 1.0f) * (pw - 4));
            drawRect(px, py, pw, 14, Color(22, 24, 44, 255));
            drawRect(px, py, filled + 2, 14, axisCols[a]);
            py += 16;
        }
        py += 4;

        // ----- Scale section -----
        drawRect(px, py, pw, 18, Color(38, 42, 72, 255));
        drawRect(px, py, 3, 18, Color(210, 75, 75, 255));
        py += 20;
        for (int a = 0; a < 3; ++a) {
            float v = sel0->transform.scale[a];
            int filled = static_cast<int>(std::clamp(v / 4.0f, 0.0f, 1.0f) * (pw - 4));
            drawRect(px, py, pw, 14, Color(22, 24, 44, 255));
            drawRect(px, py, std::max(2, filled), 14, axisCols[a]);
            py += 16;
        }
        py += 8;

        // Material color swatch
        if (!sel0->material.empty()) {
            drawRect(px, py, pw, 18, Color(38, 42, 72, 255));
            drawRect(px, py, 3, 18, Color(55, 185, 185, 255));
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
