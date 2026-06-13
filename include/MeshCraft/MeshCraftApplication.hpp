#pragma once

#include "MeshCraft/Editor/EditorCamera.hpp"
#include "MeshCraft/Editor/EditorTool.hpp"
#include "MeshCraft/Editor/SelectionManager.hpp"
#include "MeshCraft/Editor/TransformGizmo.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Renderer/GridRenderer.hpp"
#include "MeshCraft/Renderer/SceneRenderer.hpp"
#include "MeshCraft/Scene/PropertiesPanel.hpp"
#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"

#include <Microsoft/Xna/Framework/Game.hpp>
#include <Microsoft/Xna/Framework/GameTime.hpp>
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
    MeshCraftApplication(std::filesystem::path filePath, std::string screenshotPath);

    void LoadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

protected:
    bool BeginDraw() override;
    void EndDraw() override;

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

    // Input state
    Microsoft::Xna::Framework::Input::MouseState prevMouse_;
    bool firstFrame_{true};

    // Transform dragging
    bool   dragging_{false};
    float  dragStartX_{0}, dragStartY_{0};

    // Box-select drag state
    bool boxSelectActive_{false};
    int  boxSelectX0_{0}, boxSelectY0_{0};
    int  boxSelectX1_{0}, boxSelectY1_{0};

    // Auto-screenshot mode
    std::string autoScreenshotPath_;
    int autoScreenshotCountdown_{0};
    bool pendingScreenshot_{false};

    // Lights panel selection
    int selectedLightIdx_{-1};

    // Cameras panel selection
    int selectedCameraIdx_{-1};

    // Textures panel selection
    std::string selectedTextureKey_;

    // Definitions panel selection
    std::string selectedDefId_;

    // ImGui file dialog state (buffers persist across frames)
    bool openDialogOpen_{false};
    char openDialogBuf_[512]{};
    char openDialogErr_[256]{};

    bool saveDialogOpen_{false};
    char saveDialogBuf_[512]{};
    char saveDialogErr_[256]{};

    // Cached GL function pointers for viewport/scissor control
    void (*fnGlViewport_)(int, int, int, int) = nullptr;
    void (*fnGlScissor_)(int, int, int, int)  = nullptr;
    void (*fnGlEnable_)(unsigned int)          = nullptr;
    void (*fnGlDisable_)(unsigned int)         = nullptr;

    // Panel layout constants (used for 3D viewport computation)
    static constexpr int kLeftPanelW  = 220;
    static constexpr int kRightPanelW = 220;
    static constexpr int kStatusH     = 22;
    static constexpr int kTimelineH   = 190;

    // Dynamic top-area height (menu bar + toolbar), updated each frame by drawImGuiUi()
    int imguiTopH_{60};

    // Edge overlay toggle (black wireframe lines over all objects)
    bool showEdgeOverlay_{false};

    // Gizmo snap-to-grid
    bool  snapEnabled_{false};
    float snapTranslate_{0.5f};   // world units
    float snapRotate_{15.0f};     // degrees
    float snapScale_{0.25f};      // scale units

    // Grid cell spacing (world units per cell)
    float gridSpacing_{1.0f};

    // Animation playback state
    std::string currentActionName_;
    float       animTime_{0.0f};
    bool        animPlaying_{false};
    bool        showTimeline_{false};

    // Helpers
    void newScene();
    void openFile();
    void saveFile();
    void saveFileAs();
    void exportGltf();
    void deleteSelected();
    void duplicateSelected();
    void copySelected();
    void cutSelected();
    void pasteClipboard();
    void groupSelected();
    void ungroupSelected();
    void addPrimitive(Mc3::ObjectType type);
    void handleKeyboardShortcuts(const Microsoft::Xna::Framework::Input::KeyboardState& ks,
                                 const Microsoft::Xna::Framework::Input::KeyboardState& prevKs);
    void handleMouseInput(const Microsoft::Xna::Framework::Input::MouseState& ms,
                          const Microsoft::Xna::Framework::Input::MouseState& prev);
    void updateWindowTitle();
    void saveScreenshot(const std::string& path);
    void drawImGuiUi(int screenW, int screenH);

    std::vector<const Mc3::Mc3Object*> selectedPointers() const;
    Mc3::Mc3Object* flatFindById(const std::string& id) const;
    Mc3::Mc3Object* flatFindByName(const std::string& name) const;

    void evaluateAndPushAnimOverrides();
    void insertAnimKeyframes(Mc3::Mc3Object& obj,
                             std::initializer_list<Mc3::AnimatedProperty> props);
    void drawTimelinePanel(int screenW, int screenH);

    // Keyboard state from last frame
    Microsoft::Xna::Framework::Input::KeyboardState prevKs_;

    // Clipboard
    std::vector<std::shared_ptr<Mc3::Mc3Object>> clipboard_;

    // Recent files
    static constexpr int kMaxRecentFiles = 10;
    std::vector<std::filesystem::path> recentFiles_;
    void loadRecentFiles();
    void saveRecentFiles();
    void addRecentFile(const std::filesystem::path& path);

    // Locked object IDs (lock prevents gizmo/nudge/delete; persists in memory only)
    std::set<std::string> lockedIds_;

    // Hierarchy search filter
    char hierarchyFilter_[128]{};

    // Inline rename state (hierarchy panel)
    std::string renamingId_;
    char renameBuf_[256]{};
    bool renameNeedsFocus_{false};

    // Unsaved-changes guard
    enum class PendingAction { None, NewScene, OpenFile, OpenRecentFile, ExitApp };
    PendingAction         pendingAction_{PendingAction::None};
    std::filesystem::path pendingOpenPath_;
    bool                  unsavedDlgOpen_{false};
    void confirmIfModified(PendingAction action, std::filesystem::path path = {});
    void executePendingAction();

    // Help dialog
    bool showShortcutsDialog_{false};

    // Auto-save
    float autoSaveCountdown_{60.0f};
    void  performAutoSave();
    static std::filesystem::path autoSavePath(const std::filesystem::path& file);

    // Timed status bar notification
    std::string statusMsg_;
    float statusMsgTimer_{0.0f};
    bool  statusMsgIsError_{false};
    void setStatusMsg(std::string msg, bool isError = false, float duration = 3.0f);

    // Undo/redo
    static constexpr int kUndoMax = 20;
    std::vector<Mc3::Mc3Document> undoStack_;
    std::vector<Mc3::Mc3Document> redoStack_;
    void pushUndo();

    // SDL event watcher for ImGui event forwarding
    static bool sdlEventWatch(void* userdata, void* event);
};

} // namespace MeshCraft
