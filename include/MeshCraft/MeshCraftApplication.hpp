#pragma once

#include "MeshCraft/AiAssistant.hpp"
#include "MeshCraft/ModelRegistry.hpp"
#include "MeshCraft/Editor/ActiveTool.hpp"
#include "MeshCraft/Editor/EditorCamera.hpp"
#include "MeshCraft/Editor/EditorTool.hpp"
#include "MeshCraft/Editor/KeybindingManager.hpp"
#include "MeshCraft/Editor/MacroRecorder.hpp"
#include "MeshCraft/Editor/ObjectIndex.hpp"
#include "MeshCraft/Editor/Preferences.hpp"
#include "MeshCraft/Editor/SelectionManager.hpp"
#include "MeshCraft/Editor/TransformGizmo.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Renderer/GridRenderer.hpp"
#include "MeshCraft/Renderer/SceneRenderer.hpp"
#include "MeshCraft/Scene/PropertiesPanel.hpp"
#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"

#include <Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp>
#include <Microsoft/Xna/Framework/Game.hpp>
#include <Microsoft/Xna/Framework/GameTime.hpp>
#include <Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp>
#include <Microsoft/Xna/Framework/Graphics/Texture2D.hpp>
#include <Microsoft/Xna/Framework/Input/Keyboard.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>
#include <Microsoft/Xna/Framework/Input/MouseState.hpp>
#include <CNA/Devices/FileDialog.hpp>
#include <System/Object.hpp>
#include <array>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace MeshCraft {

// SYS-W14-15: result box for an in-flight native file-browse dialog
// (CNA::Devices::FileDialog). Its callback may run on a different thread
// than the one that opened the dialog (per FileDialog's own documented
// contract) -- mirrors AiRequestResult's threading pattern (AiAssistant.hpp):
// heap-allocated, mutex-guarded, drained once per frame from Update(), never
// touched directly from the callback thread beyond writing into it.
struct PendingFileBrowse {
    std::atomic<bool> done{false};
    std::mutex        mutex; // guards the fields below
    std::string       path;  // empty if canceled
    std::string       targetMatId;
    std::string       targetSlot;
};

// ActiveTool and its bounds-safe activeToolName() mapping live in
// MeshCraft/Editor/ActiveTool.hpp so they can be unit-tested without linking
// the full editor. Included above.

// H12: Macro recorder -- MacroStep/MacroRecorder now live in
// MeshCraft/Editor/MacroRecorder.hpp (SYS-W3-01 Phase 3), included above.

// H9: customizable key binding -- KeyBind/KeybindingManager now live in
// MeshCraft/Editor/KeybindingManager.hpp (SYS-W3-01), included above.

class MeshCraftApplication : public Microsoft::Xna::Framework::Game {
public:
    GetTypeNameHPP()

    MeshCraftApplication();
    explicit MeshCraftApplication(std::filesystem::path filePath);
    MeshCraftApplication(std::filesystem::path filePath, std::string screenshotPath);
    MeshCraftApplication(std::filesystem::path filePath, std::string screenshotPath,
                         std::string exportPath);
    // SYS-W12-02: headless one-shot benchmark mode (see main.cpp's
    // --benchmark flag and runBenchmarkSuite()'s own comment).
    MeshCraftApplication(std::filesystem::path filePath, bool benchmarkMode);

    // True if a non-interactive --export run (see main.cpp) failed — main()
    // uses this to pick the process exit code (STAB-0528).
    [[nodiscard]] bool exportFailed() const { return exportFailed_; }

    // Tears down the SDL event watch, ImGui context/backends, and GL resources
    // registered in LoadContent(). Runs before the base Game destructor, while
    // the SDL window and GL context are still alive.
    ~MeshCraftApplication() override;

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
    // SYS-W5-04: id/name -> object cache over document_.objects, invalidated
    // by pushUndo() and every wholesale document_ replacement. See
    // ObjectIndex.hpp for the exact invariant this depends on.
    Editor::ObjectIndex objectIndex_;

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
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    std::optional<Microsoft::Xna::Framework::Graphics::Texture2D> bgTexture_;
    std::string bgTexturePath_;

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

    // SYS-W14-15: native file-browse dialog for a material texture slot. Only
    // one dialog is meaningfully open at a time (a second click replaces the
    // pending target); see PendingFileBrowse's own comment for the threading
    // invariant.
    std::shared_ptr<PendingFileBrowse> pendingFileBrowse_;
    void browseForMaterialTexture(const std::string& matId, const std::string& slot);

    // Auto-screenshot mode
    std::string autoScreenshotPath_;
    int autoScreenshotCountdown_{0};
    bool pendingScreenshot_{false};

    // Auto-export mode (STAB-0526..0529): non-interactive
    // `--export <path>` runs the same runGltfExport() codepath a menu
    // click would, then exits — no rendering warm-up needed.
    std::string autoExportPath_;
    int  autoExportCountdown_{0};
    bool pendingExport_{false};
    bool exportFailed_{false};

    // SYS-W12-02: headless one-shot benchmark mode (`--benchmark`, no
    // scene modification, prints timing to stdout then exits). Frame
    // timings are captured for the first `kBenchmarkFrames` real Draw()
    // calls (using the app's actual camera/view/proj -- not a hand-rolled
    // stand-in) so "first frame" vs "warm frame" reflects genuine cold-vs-
    // warm cache costs (CSG evaluation, texture load, mesh load) without
    // risking a mismatched render setup. See runBenchmarkSuite() for the
    // rest (traversal/picking/undo-snapshot/animation-eval/registry,
    // which don't need a render context and are timed directly).
    static constexpr int kBenchmarkFrames = 10;
    bool   benchmarkMode_{false};
    int    benchmarkFramesRemaining_{0};
    std::vector<double> benchmarkFrameTimesMs_;
    bool   pendingBenchmark_{false};
    double benchmarkLoadContentMs_{0.0};
    void runBenchmarkSuite();

    // Lights panel selection
    int selectedLightIdx_{-1};

    // Cameras panel selection
    int selectedCameraIdx_{-1};

    // Textures panel selection
    std::string selectedTextureKey_;

    // SVG textures panel selection (STAB-0703)
    std::string selectedSvgTextureKey_;

    // Scripts panel selection (STAB-0705)
    std::string selectedScriptKey_;

    // Triggers panel selection (STAB-0707)
    std::string selectedTriggerKey_;

    // Embedded glTF panel selection (STAB-0704)
    std::string selectedEmbedKey_;

    // Scene states panel selection (STAB-0708)
    std::string selectedSceneStateKey_;

    // Audio panel selection + preview playback (STAB-0706). One shared
    // preview instance at a time (starting a new preview stops any
    // currently-playing one) -- SoundEffectInstance keeps its own audio
    // resource alive independent of the originating SoundEffect (CP-7 in
    // CNA's SoundEffectInstance.hpp), so no separate SoundEffect member is
    // needed here.
    std::string selectedSoundKey_;
    std::string selectedMusicKey_;
    std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffectInstance> audioPreviewInstance_;
    std::string audioPreviewKey_;   // which sound/music id audioPreviewInstance_ belongs to
    std::string audioPreviewError_; // last load/play error, shown inline; cleared on next attempt

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

    // Mesh source browse dialog state (F2)
    bool meshBrowseOpen_{false};
    char meshBrowseBuf_[512]{};
    char meshBrowseErr_[256]{};

    // Import OBJ dialog state (STAB-0717): creates a new Mesh object in the
    // current scene referencing the chosen OBJ file, unlike meshBrowse*_
    // above (which only retargets an already-selected Mesh object's source).
    bool importObjDialogOpen_{false};
    char importObjDialogBuf_[512]{};
    char importObjDialogErr_[256]{};

    // Export selection dialog state (F3)
    bool selExportOpen_{false};
    char selExportBuf_[512]{};
    char selExportErr_[256]{};
    void exportSelectionToFile(const std::string& path);

    // Merge scene dialog state (F4)
    bool mergeSceneOpen_{false};
    char mergeSceneBuf_[512]{};
    char mergeSceneErr_[256]{};
    void mergeSceneFromFile(const std::string& path);

    // CSG mesh export dialog state (K3)
    bool                    csgExportOpen_{false};
    char                    csgExportBuf_[512]{};
    char                    csgExportErr_[256]{};
    const Mc3::Mc3Object*   csgExportObj_{nullptr};

    // GLB export settings dialog state (F8)
    bool glbExportOpen_{false};
    int  glbExportFmt_{0};                // 0 = GLB, 1 = GLTF
    bool glbAllowApproxCSG_{false};       // checkbox: allow approximate CSG export
    char glbExportOutBuf_[512]{};
    char glbExportErr_[256]{};
    void runGltfExport(const std::string& outPath);

    // OBJ export dialog state (STAB-0718): the editor previously only
    // exported to glTF/GLB (via mc3togltf::GltfExporter) or a sub-scene
    // .mc3.xml (Export Selection). Reuses the existing, unmodified
    // GltfExporter as a black box (exports to a temp .glb, reads it back
    // via tinygltf's own reader, walks the resulting flattened node graph)
    // rather than reimplementing scene traversal/CSG/transform composition
    // a second time for OBJ specifically.
    bool objExportOpen_{false};
    char objExportOutBuf_[512]{};
    char objExportErr_[256]{};
    void runObjExport(const std::string& outPath);

    // Subtree export as template dialog state (E8)
    bool subtreeExportOpen_{false};
    char subtreeExportNameBuf_[128]{};  // definition name
    char subtreeExportFileBuf_[512]{};  // optional file path to also save
    char subtreeExportErr_[256]{};
    void exportSubtreeAsTemplate(const std::string& defName, const std::string& filePath);

    // Cached GL function pointers for viewport/scissor control
    void (*fnGlViewport_)(int, int, int, int) = nullptr;
    void (*fnGlScissor_)(int, int, int, int)  = nullptr;
    void (*fnGlEnable_)(unsigned int)          = nullptr;
    void (*fnGlDisable_)(unsigned int)         = nullptr;

    // Bloom post-processing (I6)
    bool  bloomEnabled_{false};
    float bloomStrength_{2.5f};
    int   bloomFboW_{0}, bloomFboH_{0};
    void initBloom(int w, int h);
    void applyBloom(int vx, int glViewY, int vw, int vh,
                    const Microsoft::Xna::Framework::Matrix& view,
                    const Microsoft::Xna::Framework::Matrix& proj);

    // Shadow Map Debug (I7)
    bool     shadowDebugEnabled_{false};
    unsigned shadowDebugFbo_{0};
    unsigned shadowDebugColorTex_{0};
    unsigned shadowDebugDepthTex_{0};

    // True once LoadContent() has initialized the ImGui context + backends, so
    // the destructor only tears them down when they were actually created.
    bool     imguiInitialized_{false};
    static constexpr int kShadowDebugRes = 256;
    void initShadowDebug();
    void renderShadowDebugFbo(const Microsoft::Xna::Framework::Matrix& lightView,
                              const Microsoft::Xna::Framework::Matrix& lightProj);
    void drawShadowDebugOverlay(int screenW, int screenH);

    // Material preview sphere (D7)
    static constexpr int kMatPreviewRes = 128;
    unsigned             matPreviewTexId_{0};   // GL texture name, exposed for ImGui::Image
    void initMatPreview();
    void renderMatPreview(float r, float g, float b, float roughness, float metallic);

    // STAB-0521: drains glGetError() after a full frame's render passes
    // (SSAO/bloom/skybox/gizmos/ImGui) and reports any leaked GL error state,
    // so a bad state left by one pass doesn't silently propagate into the next.
    void checkGlStateLeak(const char* where);
    bool lastGlErrorSeen_{false};

    // SSAO post-processing (I5)
    bool  ssaoEnabled_{false};
    bool  ssaoGlReady_{false};
    float ssaoStrength_{0.8f};
    float ssaoRadius_{0.5f};
    int   ssaoFboW_{0}, ssaoFboH_{0};
    void initSsao(int w, int h);
    void applySsao(int vx, int glViewY, int vw, int vh,
                   float tanHalfFovX, float tanHalfFovY,
                   float nearPlane, float farPlane);
    void initSkybox();
    void drawSkybox(const Microsoft::Xna::Framework::Matrix& view,
                    float fovDegrees, float aspect);

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

    // Proportional editing (H1): influence falloff on nearby objects during Move
    bool  propEditEnabled_{false};
    float propEditRadius_{5.0f};   // world-unit influence radius

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
    void exportObj();
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

    // Audio preview playback (STAB-0706): plays/stops a one-shot preview of
    // a Mc3Sound/Mc3Music entry via CNA's SoundEffect/SoundEffectInstance.
    // `key` identifies which sound/music this preview belongs to (for UI
    // highlighting); `srcPath` is resolved relative to document_.sourcePath
    // by the caller before being passed in.
    void playAudioPreview(const std::string& key, const std::string& srcPath, bool loop);
    void stopAudioPreview();

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
    // Shared-ownership counterpart of flatFindById(), for callers (e.g.
    // undo/redo selection restore) that need a shared_ptr to hand to
    // SelectionManager::select() rather than a raw observing pointer.
    std::shared_ptr<Mc3::Mc3Object> flatFindSharedById(const std::string& id) const;

    void evaluateAndPushAnimOverrides();
    void insertAnimKeyframes(Mc3::Mc3Object& obj,
                             const std::vector<Mc3::AnimatedProperty>& props);
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

    // Material list search filter
    char matFilter_[128]{};

    // Unsaved-changes guard
    enum class PendingAction { None, NewScene, OpenFile, OpenRecentFile, ExitApp };
    PendingAction         pendingAction_{PendingAction::None};
    std::filesystem::path pendingOpenPath_;
    bool                  unsavedDlgOpen_{false};
    void confirmIfModified(PendingAction action, std::filesystem::path path = {});
    void executePendingAction();

    // SYS-W9-02: crash-recovery guard. Offered whenever a file finishes
    // loading (startup, Open Recent, Open File dialog) and its `.autosave`
    // sibling is newer than the file itself -- a strong signal the editor
    // previously crashed/closed with unsaved changes. checkForNewerAutosave()
    // is the single call site all three load paths share so the check can't
    // silently drift out of sync between them again.
    bool                  recoveryDlgOpen_{false};
    std::filesystem::path recoveryFilePath_;
    void checkForNewerAutosave(const std::filesystem::path& file);
    void recoverFromAutosave();
    void discardAutosave();

    // Help dialog
    bool showShortcutsDialog_{false};

    // Undo history dialog
    bool undoHistoryOpen_{false};

    // Command palette
    bool cmdPaletteOpen_{false};
    char cmdPaletteBuf_[256]{};

    // Batch rename dialog
    bool  groupScaleOpen_{false};
    float groupScaleFactor_{2.0f};
    void  groupScaleSelected();

    bool batchRenameOpen_{false};
    char batchRenameBuf_[256]{};
    void batchRenameSelected();
    void selectParent();
    void selectChildren();
    void alignToObject();
    void convertToDefinition();
    void breakInstance();
    void randomizeTransformSelected();
    void dropSelectedToGroundPlane();

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

    // Scatter Along Curve dialog (H4)
    bool  scatterCurveOpen_{false};
    int   scatterCurveCount_{6};
    int   scatterCurveMode_{0};     // 0=Line, 1=Arc
    int   scatterCurveAxis_{0};     // 0=X, 1=Y, 2=Z
    float scatterCurveSpacing_{1.5f};
    float scatterCurveRadius_{4.0f};
    float scatterCurveArcAngle_{180.0f}; // degrees of arc (360=full circle)
    float scatterCurveJitter_{0.0f};
    void  scatterAlongCurve();

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
    float autoSaveInterval_{60.0f};  // seconds; 0 = disabled (F5)
    float autoSaveCountdown_{60.0f};
    void  performAutoSave();
    static std::filesystem::path autoSavePath(const std::filesystem::path& file);

    // Customizable keybindings (H9, SYS-W3-01: extracted into KeybindingManager)
    Editor::KeybindingManager keybindings_;
    std::string keyCaptureAction_;   // non-empty = waiting for next keypress
    bool        keybindOpen_{false}; // open keybind editor dialog

    // Macro recorder (H12, SYS-W3-01 Phase 3: extracted into
    // Editor::MacroRecorder; macroContext() builds the callback struct it
    // needs to call back into this class, built fresh at each play/save/
    // load call site)
    Editor::MacroRecorder macroRecorder_;
    bool                   macroOpen_{false};
    char                   macroFileBuf_[512]{};
    Editor::MacroRecorder::Context macroContext();

    // Preferences dialog (H7, SYS-W3-01 Phase 2: theme + window-open state
    // extracted into Editor::Preferences; loadPrefs()/savePrefs() stay here
    // since they persist prefs_.theme() together with the cross-domain
    // autoSaveInterval_/snap*/gridSpacing_ fields below in one prefs file)
    Editor::Preferences prefs_;
    void loadPrefs();
    void savePrefs();

    // Timed status bar notification
    std::string statusMsg_;
    float statusMsgTimer_{0.0f};
    bool  statusMsgIsError_{false};
    void setStatusMsg(std::string msg, bool isError = false, float duration = 3.0f);

    // STAB-0701: rotation_units="radians" / a non-default euler_order are
    // export/interchange-only fields -- the editor's own rendering, gizmos,
    // and mouse-drag rotation all assume degrees + a fixed XNA axis order
    // and do not consult these document-level settings. Warns the author
    // once per load rather than silently rendering such a file wrong.
    void checkRotationConventionNotice();

    // SYS-W14-11: shared extension dispatch for the 3 real load call sites
    // (startup, Open Recent File, Open File dialog) -- .mcb routes to the
    // MCB reader, .json to the semantic-JSON parser, everything else to the
    // XML parser. Extracted so all 3 sites can't drift out of sync with
    // each other (matching this codebase's established Alg-extraction
    // idiom for exactly this class of duplication risk).
    Mc3::Mc3Document loadSceneFileDispatched(const std::filesystem::path& path,
                                              Mc3::Mc3Validation& validation);

    // Undo/redo
    static constexpr int kUndoMax = 20;
    std::vector<Mc3::Mc3Document> undoStack_;
    std::vector<Mc3::Mc3Document> redoStack_;
    // SYS-W9-03 (human-authorized decision, 2026-07-17): the ids selected at
    // the moment each undoStack_/redoStack_ entry was pushed, kept in
    // lockstep (index-for-index, same push/cap calls) with those stacks --
    // lets performUndo()/performRedo() restore the pre-mutation selection
    // instead of unconditionally clearing it.
    std::vector<std::vector<std::string>> undoSelectionStack_;
    std::vector<std::vector<std::string>> redoSelectionStack_;
    [[nodiscard]] std::vector<std::string> currentSelectionIds() const;
    void restoreSelectionByIds(const std::vector<std::string>& ids);
    void pushUndo();
    void performUndo();
    void performRedo();

    // AUD-036b: central helper for the mutating-widget undo pattern. Call
    // immediately after a Drag*/ColorEdit*/Input* widget, passing its own
    // return value:
    //
    //     if (undoOnActivate(ImGui::DragFloat3("##pos", pos, 0.1f))) {
    //         ...apply the edit...; modified_ = true;
    //     }
    //
    // Snapshots on the widget's activation frame unconditionally, before the
    // caller's mutation code ever runs -- so the AUD-036 bug class (a
    // `pushUndo()` nested inside the widget's changed-block, which silently
    // never fires because IsItemActivated() and a Drag widget's own changed
    // return never coincide) is structurally impossible at call sites that
    // use this helper instead of hand-rolling the pattern. Must be called
    // right after the widget, same as ImGui::IsItemActivated() itself
    // requires. Returns `widgetChanged` unmodified so a call site reads the
    // same as calling the widget directly.
    bool undoOnActivate(bool widgetChanged);

    // SDL event watcher for ImGui event forwarding
    static bool sdlEventWatch(void* userdata, void* event);

    // AI Assistant (M3)
    AiAssistant aiAssistant_;
    bool showAiPanel_{false};
    char aiApiKeyBuf_[128]{};
    char aiModelBuf_[64]{"claude-sonnet-5"};
    char aiPromptBuf_[2048]{};
    int  aiScopeSel_{0};  // 0=Full scene, 1=Selection only
    std::optional<Mc3::Mc3Document> aiPendingDoc_;  // validated+parsed AI result
    std::string aiValidationError_;  // non-empty when auto-validation failed
    bool aiApplyConfirmPending_{false};  // STAB-0395: awaiting 2nd click on a drastic-shrink Apply
    void drawAiPanel();

    // Validation diagnostics (SYS-W14-02, builds on SYS-W1-01)
    bool showValidationPanel_{false};
    Mc3::Mc3Validation lastValidation_;    // result of the most recent load/save/export
    std::string        lastValidationSource_;  // e.g. "Load: house.mc3.xml", "Save", "Export"
    void drawValidationPanel();
    // Records `v` as the most recent validation result and stamps its source
    // label -- called from every load/save/export call site so the panel
    // always reflects "what just happened", not a stale earlier run.
    void recordValidation(std::string source, Mc3::Mc3Validation v);

    // Model Registry (M2)
    ModelRegistry registry_;
    bool showRegistryPanel_{false};
    char regSearchBuf_[128]{};
    bool regSaveDlgOpen_{false};
    char regSaveGroupBuf_[64]{};
    char regSaveNameBuf_[64]{};
    char regSaveVariantBuf_[64]{};
    char regSaveTagsBuf_[128]{};
    char regSaveDescBuf_[256]{};
    char regSaveSourceBuf_[64]{"handmade"};
    std::string regSaveDefId_;
    bool        regSaveFromAi_{false};  // when true, save dialog reads from aiPendingDoc_
    std::vector<ModelRegistry::Entry> regCachedResults_;
    bool regResultsDirty_{true};
    void drawRegistryPanel();

    // H5 — pivot edit mode
    bool pivotEditMode_{false};
    void resetPivot(); // zero pivot, compensate position so world geometry stays fixed

    // -----------------------------------------------------------------------
    // Walk mode (H15) — first-person exploration
    // -----------------------------------------------------------------------
    bool  walkModeEnabled_{false};
    float walkPosX_{0.0f}, walkPosY_{0.0f}, walkPosZ_{5.0f};
    float walkYaw_{0.0f};        // radians, horizontal look
    float walkPitch_{0.0f};      // radians, vertical look (clamped ±85°)
    float walkVelY_{0.0f};       // vertical velocity (gravity / jump)
    bool  walkOnGround_{true};
    float walkHeight_{1.8f};     // eye height above ground (meters)
    float walkSpeed_{5.0f};      // movement speed (m/s)
    float walkTurnSpeed_{1.5f};  // keyboard yaw speed (rad/s)
    float walkMouseSens_{0.003f};// mouse sensitivity (rad/px)
    bool  walkSettingsOpen_{false};
    void  updateWalkMode(float dt, const Microsoft::Xna::Framework::Input::KeyboardState& ks,
                         int mouseDx, int mouseDy);
    void  enterWalkMode();
    void  exitWalkMode();
    void  drawWalkModeHud(int screenW, int screenH);

    // Copy src into a fixed ImGui char buffer; always null-terminates.
    template<std::size_t N>
    static void copyToBuf(char (&dst)[N], std::string_view src) {
        static_assert(N > 0);
        std::snprintf(dst, N, "%s", std::string(src).c_str());
    }
};

} // namespace MeshCraft
