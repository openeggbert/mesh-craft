#pragma once

#include "MeshCraft/Editor/EditorCamera.hpp"
#include "MeshCraft/Ui/BitmapFont.hpp"
#include "MeshCraft/Editor/EditorTool.hpp"
#include "MeshCraft/Editor/SelectionManager.hpp"
#include "MeshCraft/Editor/TransformGizmo.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Renderer/GridRenderer.hpp"
#include "MeshCraft/Renderer/SceneRenderer.hpp"
#include "MeshCraft/Scene/PropertiesPanel.hpp"
#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"

#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Game.hpp>
#include <Microsoft/Xna/Framework/GameTime.hpp>
#include <Microsoft/Xna/Framework/Graphics/BasicEffect.hpp>
#include <Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp>
#include <Microsoft/Xna/Framework/Graphics/Texture2D.hpp>
#include <Microsoft/Xna/Framework/Input/Keyboard.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>
#include <Microsoft/Xna/Framework/Input/MouseState.hpp>
#include <System/Object.hpp>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace MeshCraft {

enum class ActiveTool { Select, Move, Rotate, Scale, AddBox, AddSphere, AddCylinder, AddCone, AddPlane };

class MeshCraftApplication : public Microsoft::Xna::Framework::Game {
public:
    GetTypeNameHPP()

    MeshCraftApplication();
    explicit MeshCraftApplication(std::filesystem::path filePath);
    MeshCraftApplication(std::filesystem::path filePath, std::string screenshotPath);  // auto-screenshot mode

    void LoadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

private:
    // Scene data
    Mc3::Mc3Document document_;
    std::filesystem::path currentFile_;
    bool modified_{false};

    // Editor state
    Editor::EditorCamera  camera_;
    Editor::SelectionManager selection_;
    Editor::TransformGizmo   gizmo_;
    ActiveTool               activeTool_{ActiveTool::Select};

    // Renderers (initialised in LoadContent)
    std::unique_ptr<Renderer::GridRenderer>  gridRenderer_;
    std::unique_ptr<Renderer::SceneRenderer> sceneRenderer_;
    std::unique_ptr<Scene::SceneHierarchyPanel> hierarchyPanel_;
    std::unique_ptr<Scene::PropertiesPanel>     propertiesPanel_;

    // 2D UI rendering
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    Microsoft::Xna::Framework::Graphics::Texture2D whitePx_;

    // Input state (for drag-delta computation)
    Microsoft::Xna::Framework::Input::MouseState prevMouse_;
    bool firstFrame_{true};

    // Transform dragging
    bool   dragging_{false};
    float  dragStartX_{0}, dragStartY_{0};

    // Auto-screenshot mode: save screenshot after N frames then exit
    std::string autoScreenshotPath_;
    int autoScreenshotCountdown_{0};

    // Cached GL function pointers for viewport/scissor control (loaded in LoadContent)
    void (*fnGlViewport_)(int, int, int, int) = nullptr;
    void (*fnGlScissor_)(int, int, int, int)  = nullptr;
    void (*fnGlEnable_)(unsigned int)          = nullptr;
    void (*fnGlDisable_)(unsigned int)         = nullptr;

    // Panel layout
    static constexpr int kToolbarH   = 40;
    static constexpr int kStatusH    = 24;
    static constexpr int kLeftPanelW = 220;
    static constexpr int kRightPanelW = 220;
    static constexpr int kObjRowH    = 22;
    static constexpr int kPanelHdrH  = 26;

    // Helpers
    void newScene();
    void openFile();
    void saveFile();
    void saveFileAs();
    void exportGltf();
    void deleteSelected();
    void duplicateSelected();
    void addPrimitive(Mc3::ObjectType type);
    void handleKeyboardShortcuts(const Microsoft::Xna::Framework::Input::KeyboardState& ks,
                                 const Microsoft::Xna::Framework::Input::KeyboardState& prevKs);
    void handleMouseInput(const Microsoft::Xna::Framework::Input::MouseState& ms,
                          const Microsoft::Xna::Framework::Input::MouseState& prev);
    void updateWindowTitle();

    // UI drawing
    void drawRect(int x, int y, int w, int h, Microsoft::Xna::Framework::Color col);
    void drawUi(int screenW, int screenH);
    Microsoft::Xna::Framework::Color objectTypeColor(Mc3::ObjectType type) const;
    void saveScreenshot(const std::string& path);

    std::vector<const Mc3::Mc3Object*> selectedPointers() const;
    Mc3::Mc3Object* flatFindById(const std::string& id) const;

    // Keyboard state from last frame
    Microsoft::Xna::Framework::Input::KeyboardState prevKs_;

    // Hierarchy panel tree state
    struct HierarchyRow {
        int depth;
        std::shared_ptr<Mc3::Mc3Object> obj;
    };
    std::vector<HierarchyRow> hierarchyRows_;   // rebuilt each Draw()
    std::set<const Mc3::Mc3Object*> collapsedGroups_;

    // Properties panel clickable field positions (rebuilt each Draw())
    struct PropFieldHit {
        int y;        // screen y of the 14-px field row
        int section;  // 0=POS, 1=ROT, 2=SCL
        int axis;     // 0=X, 1=Y, 2=Z
    };
    std::vector<PropFieldHit> propFieldHits_;

    // Inline field editing state
    bool        fieldActive_ {false};
    int         fieldSection_{0};   // 0=POS, 1=ROT, 2=SCL
    int         fieldAxis_   {0};   // 0=X,   1=Y,   2=Z
    std::string fieldBuffer_;

    void activateField(int section, int axis);
    void applyFieldValue();
    void cancelField();
};

} // namespace MeshCraft
