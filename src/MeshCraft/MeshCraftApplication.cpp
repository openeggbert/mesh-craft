#include "MeshCraft/MeshCraftApplication.hpp"

#include <Microsoft/Xna/Framework/Color.hpp>
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

// ---------------------------------------------------------------------------
// LoadContent
// ---------------------------------------------------------------------------

void MeshCraftApplication::LoadContent() {
    auto& gd = getGraphicsDeviceProperty();

    gridRenderer_  = std::make_unique<Renderer::GridRenderer>(gd);
    sceneRenderer_ = std::make_unique<Renderer::SceneRenderer>(gd);

    hierarchyPanel_  = std::make_unique<Scene::SceneHierarchyPanel>(document_);
    propertiesPanel_ = std::make_unique<Scene::PropertiesPanel>();

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

    // Background color from scene environment
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

    const auto& vp = gd.getViewportProperty();
    float aspect = (vp.getHeightProperty() > 0)
        ? static_cast<float>(vp.getWidthProperty()) / vp.getHeightProperty()
        : 16.0f / 9.0f;

    Matrix view = camera_.viewMatrix();
    Matrix proj = camera_.projectionMatrix(aspect);

    // Grid
    gd.SetDepthTestEnabled(false);
    gridRenderer_->draw(view, proj);

    // Scene objects
    gd.SetDepthTestEnabled(true);
    auto selPtrs = selectedPointers();
    sceneRenderer_->draw(document_, view, proj, selPtrs);
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

    // Left click: select object (simple pick by proximity to click)
    if (leftBtn && !prevLeft) {
        // For now, just clear selection on click (proper ray-cast to be added)
        bool ctrl = (Keyboard::GetState().IsKeyDown(Keys::LeftControl) ||
                     Keyboard::GetState().IsKeyDown(Keys::RightControl));
        if (!ctrl) selection_.clear();
        // TODO: ray-cast pick into scene
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

    // Append active tool
    const char* toolNames[] = {
        "Select","Move","Rotate","Scale","Add Box","Add Sphere","Add Cylinder","Add Cone","Add Plane"
    };
    title += " | Tool: ";
    title += toolNames[static_cast<int>(activeTool_)];

    getWindowProperty().setTitleProperty(title);
}

} // namespace MeshCraft
