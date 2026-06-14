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
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace MeshCraft {

enum class ActiveTool { Select, Move, Rotate, Scale, AddBox, AddSphere, AddCylinder, AddCone, AddPlane, Measure };

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

    // Gizmo drag delta overlay state
    float gizmoDragStartVal_{0.0f}; // value of the dragged axis at drag start
    int   gizmoDragAxisIdx_{0};     // 0=X 1=Y 2=Z

    // Box-select drag state
    bool boxSelectActive_{false};
    int  boxSelectX0_{0}, boxSelectY0_{0};
    int  boxSelectX1_{0}, boxSelectY1_{0};

    // Drag-and-drop file open (set from SDL event watcher, consumed in Update)
    std::string pendingDropFile_;

    // Drag-and-drop texture onto material texture fields (D6)
    std::string pendingDropTexture_;   // image file dropped from OS
    std::string hoveredTexSlot_;       // last-hovered slot: "base","normal","emissive","metalrough","occlusion"
    std::string hoveredTexMatId_;      // material being edited when that slot was hovered
    bool        dropTexPickerOpen_{false}; // slot-picker popup trigger
    std::string dropTexPickerPath_;        // image path pending slot assignment
    std::string dropTexPickerMatId_;       // material to assign to

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

    // Materials panel selection
    std::string selectedMaterialKey_;
    char newMaterialNameBuf_[128]{};

    // Definitions panel selection
    std::string selectedDefId_;

    // ImGui file dialog state (buffers persist across frames)
    bool openDialogOpen_{false};
    char openDialogBuf_[512]{};
    char openDialogErr_[256]{};

    bool saveDialogOpen_{false};
    char saveDialogBuf_[512]{};
    char saveDialogErr_[256]{};

    // Material export/import dialog state (D5)
    bool        matExportOpen_{false};
    std::string matExportId_;
    char        matExportBuf_[512]{};
    char        matExportErr_[256]{};
    bool        matImportOpen_{false};
    char        matImportBuf_[512]{};
    char        matImportErr_[256]{};

    // Cached GL function pointers for viewport/scissor control
    void (*fnGlViewport_)(int, int, int, int) = nullptr;
    void (*fnGlScissor_)(int, int, int, int)  = nullptr;
    void (*fnGlEnable_)(unsigned int)          = nullptr;
    void (*fnGlDisable_)(unsigned int)         = nullptr;

    // Panel layout (widths are user-resizable via splitter drag)
    int kLeftPanelW  = 220;
    int kRightPanelW = 220;
    static constexpr int kStatusH     = 22;
    static constexpr int kTimelineH   = 190;

    // Dynamic top-area height (menu bar + toolbar), updated each frame by drawImGuiUi()
    int imguiTopH_{60};

    // Edge overlay toggle (black wireframe lines over all objects)
    bool showEdgeOverlay_{false};

    // Full wireframe mode (skip solid rendering, show only edges)
    bool showWireframeMode_{false};

    // Bounding box overlay toggle
    bool showBoundingBox_{false};

    // Look-through-camera mode
    bool lookThroughCamera_{false};

    // Measurement tool state
    bool mPt1Set_{false};
    bool mPt2Set_{false};
    std::array<float,3> mPt1_{};
    std::array<float,3> mPt2_{};
    float mDist_{0.0f};

    // Cached screen dimensions (from io.DisplaySize, updated every Draw frame)
    int cachedScreenW_{1}, cachedScreenH_{1};

    // Cached view-projection + viewport for overlay projection
    Microsoft::Xna::Framework::Matrix cachedVP_;
    int   cachedVX_{0}, cachedVY_{0}, cachedVW_{1}, cachedVH_{1};

    // Gizmo space toggle (false = World, true = Local)
    bool gizmoLocalSpace_{false};

    // Gizmo snap-to-grid
    bool  snapEnabled_{false};
    float snapTranslate_{0.5f};   // world units
    float snapRotate_{15.0f};     // degrees
    float snapScale_{0.25f};      // scale units
    bool  surfaceSnapEnabled_{false}; // B8: snap Y to surface below object

    // Grid cell spacing (world units per cell)
    float gridSpacing_{1.0f};

    // Animation playback state
    std::string currentActionName_;
    float       animTime_{0.0f};
    bool        animPlaying_{false};
    bool        showTimeline_{false};

    // Timeline keyframe drag & selection state
    int   tlDragChan_{-1};
    int   tlDragKf_{-1};
    int   tlSelChan_{-1};
    int   tlSelKf_{-1};
    // Multi-select
    std::set<std::pair<int,int>> tlMultiSel_;
    bool  tlGroupDrag_{false};
    float tlGroupDragPrev_{0.0f};
    bool  tlBoxActive_{false};
    float tlBoxX0_{0.0f}, tlBoxY0_{0.0f};
    float tlBoxX1_{0.0f}, tlBoxY1_{0.0f};

    // Add Channel dialog state
    bool addChannelOpen_{false};
    char addChannelObjBuf_[128]{};
    int  addChannelPropIdx_{0};
    // Rename Action dialog state
    bool renameActionOpen_{false};
    char renameActionBuf_[128]{};

    // Scale Channel dialog state (A8)
    bool  scaleChannelOpen_{false};
    int   scaleChannelIdx_{-1};
    float scaleChannelFactor_{1.0f};

    // Keyframe clipboard (copy/paste)
    struct KfClipEntry {
        std::string targetObject;
        Mc3::AnimatedProperty property;
        float relTime{0.0f};
        float value{0.0f};
        Mc3::Interpolation interpolation{Mc3::Interpolation::Linear};
        Mc3::Mc3BezierHandle handleLeft, handleRight;
    };
    std::vector<KfClipEntry> kfClipboard_;

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

    // drawImGuiUi sub-sections
    float drawMenuBar();
    float drawToolbar(float menuBarH, int screenW);
    void  drawLeftPanel(float panelY, float panelH);
    void  drawPropertiesPanel(float panelY, float panelH, int screenW, int screenH);
    void  drawStatsOverlay(int screenW, int screenH);
    void  drawStatusBar(int screenW, int screenH);
    void  drawDialogs();
    void  drawPanelSplitters(int screenW, int screenH);

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

    // Isolation mode: hides all non-selected objects; Alt+I to toggle
    bool isolateActive_{false};
    std::map<std::string, bool> preisolateVisibility_;
    void toggleIsolate();

    // Hierarchy search filter
    char hierarchyFilter_[128]{};

    // Hierarchy type filter (E5): 0=All 1=Prim 2=Mesh 3=Group 4=Instance 5=CSG 6=Extrude
    int  hierTypeFilter_{0};

    // Hierarchy layer filter (E7): empty = show all layers
    std::string hierLayerFilter_;

    // ID to scroll into view in hierarchy next frame (set by command palette object selection)
    std::string hierarchyScrollToId_;

    // Material list search filter
    char matFilter_[128]{};

    // Hierarchy shift-click range selection
    std::string hierarchyAnchorId_;
    std::vector<std::shared_ptr<Mc3::Mc3Object>> hierarchyFlatOrder_;

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

    // Undo history dialog
    bool undoHistoryOpen_{false};

    // Command palette
    bool cmdPaletteOpen_{false};
    char cmdPaletteBuf_[256]{};

    // Batch rename dialog
    bool batchRenameOpen_{false};
    char batchRenameBuf_[256]{};
    void batchRenameSelected();
    void selectParent();
    void selectChildren();
    void alignToObject();
    void convertToDefinition();
    void breakInstance();
    void randomizeTransformSelected();

    // Find & Replace names dialog
    bool findReplaceOpen_{false};
    char findBuf_[128]{};
    char replaceBuf_[128]{};
    bool findCaseSensitive_{false};
    bool findSelectedOnly_{false};
    void findReplaceNames();

    // Linear Array dialog
    bool  arrayDupOpen_{false};
    int   arrayDupCount_{3};
    int   arrayDupAxis_{0};     // 0=X, 1=Y, 2=Z
    float arrayDupSpacing_{1.0f};
    bool  arrayDupRelative_{true};
    void  arrayDuplicate();

    // Copy Properties to Selected dialog
    bool copyPropsOpen_{false};
    bool copyPropsMaterial_{true};
    bool copyPropsMaterialOverride_{false};
    bool copyPropsCollision_{false};
    bool copyPropsTags_{false};
    bool copyPropsVisibility_{false};
    bool copyPropsIsCutter_{false};
    void copyPropsToSelected();

    // Randomize Transform dialog state
    bool  randomizeOpen_{false};
    float scatterPosRange_[3]{1.0f, 0.0f, 1.0f};
    float scatterRotRange_[3]{0.0f, 180.0f, 0.0f};
    float scatterScaleRange_{0.0f};

    // Viewport stats overlay
    bool  showStatsOverlay_{true};
    float displayFps_{0.0f};

    // Camera bookmarks (5 slots)
    struct CameraBookmark {
        float yaw{0}, pitch{0.4f}, distance{15.0f};
        float targetX{0}, targetY{0}, targetZ{0};
        bool  valid{false};
    };
    std::array<CameraBookmark, 5> cameraBookmarks_{};
    void saveCameraBookmark(int slot);
    void restoreCameraBookmark(int slot);

    // Transform clipboard (Ctrl+Shift+C / Ctrl+Shift+V)
    struct TransformClipboard {
        std::array<float, 3> position{0, 0, 0};
        std::array<float, 3> rotation{0, 0, 0};
        std::array<float, 3> scale{1, 1, 1};
        bool valid{false};
    };
    TransformClipboard transformClipboard_;

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
