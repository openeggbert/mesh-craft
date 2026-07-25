#pragma once

#include "MeshCraft/Editor/CameraBookmarks.hpp"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {
enum class ObjectType;
}

namespace MeshCraft::Application::UI {

struct CameraBookmarksContext {
    const Editor::CameraBookmarks& bookmarks;
    std::function<void(int)> save;
    std::function<void(int)> restore;
};

struct WalkModeContext {
    bool active;
    std::function<void()> toggle;
};

struct HelpMenuContext {
    std::function<void()> openPreferences;
    std::function<void()> openCommandPalette;
    std::function<void()> openKeyboardShortcuts;
};

struct EditHistoryContext {
    bool canUndo;
    bool canRedo;
    std::function<void()> undo;
    std::function<void()> redo;
    std::function<void()> openHistory;
};

struct EditClipboardContext {
    std::function<void()> cut;
    std::function<void()> copy;
    std::function<void()> paste;
};

struct EditObjectActionsContext {
    bool canDuplicateAtOffset;
    std::function<void()> duplicate;
    std::function<void()> duplicateAtOffset;
    std::function<void()> deleteSelection;
};

struct EditSelectionActionsContext {
    std::function<void()> selectAll;
    std::function<void()> invertSelection;
};

struct EditSelectByTypeContext {
    std::function<std::vector<Mc3::ObjectType>()> getPresentTypes;
    std::function<void(Mc3::ObjectType)> selectType;
};

struct EditSelectByTagContext {
    std::function<std::set<std::string>()> getTags;
    std::function<void(const std::string&)> selectTag;
};

struct EditSelectByMaterialContext {
    std::function<std::set<std::string>()> getMaterials;
    std::function<void(const std::string&)> selectMaterial;
};

struct EditCopyPropertiesContext {
    bool canCopy;
    std::function<void()> openDialog;
};

struct EditGroupingContext {
    std::function<void()> group;
    std::function<void()> ungroup;
};

struct EditConvertToDefinitionContext {
    bool canConvert;
    std::function<void()> convert;
};

struct EditExportSubtreeContext {
    bool canExport;
    std::function<void()> openDialog;
};

struct EditBreakInstanceContext {
    bool canBreak;
    std::function<void()> breakInstance;
};

enum class EditAlignmentTarget {
    Minimum,
    Center,
    Maximum,
};

struct EditAlignSelectionContext {
    bool canAlign;
    bool canAlignToFirst;
    std::function<void(int, EditAlignmentTarget)> align;
    std::function<void()> alignToFirst;
};

struct EditDistributeSelectionContext {
    bool canDistribute;
    std::function<void(int)> distribute;
};

struct EditDropToGroundContext {
    bool canDrop;
    std::function<void()> drop;
};

struct EditSnapSelectionToGridContext {
    bool canSnap;
    std::function<void()> snap;
};

struct EditMirrorSelectionContext {
    bool canMirror;
    std::function<void(int)> mirror;
};

struct EditGroupScaleContext {
    bool canScale;
    std::function<void()> openDialog;
};

struct EditLinearArrayContext {
    bool canOpen;
    std::function<void()> openDialog;
};

struct EditScatterAlongCurveContext {
    bool canOpen;
    std::function<void()> openDialog;
};

struct EditBatchRenameContext {
    bool canOpen;
    std::function<void()> openDialog;
};

class MenuBar final {
public:
    static void drawAddMenu(const std::function<void(Mc3::ObjectType)>& addPrimitive);
    static void drawEditClipboard(const EditClipboardContext& context);
    static void drawEditCopyProperties(const EditCopyPropertiesContext& context);
    static void drawEditGrouping(const EditGroupingContext& context);
    static void drawEditConvertToDefinition(const EditConvertToDefinitionContext& context);
    static void drawEditExportSubtree(const EditExportSubtreeContext& context);
    static void drawEditBreakInstance(const EditBreakInstanceContext& context);
    static void drawEditAlignSelection(const EditAlignSelectionContext& context);
    static void drawEditDistributeSelection(const EditDistributeSelectionContext& context);
    static void drawEditDropToGround(const EditDropToGroundContext& context);
    static void drawEditSnapSelectionToGrid(const EditSnapSelectionToGridContext& context);
    static void drawEditMirrorSelection(const EditMirrorSelectionContext& context);
    static void drawEditGroupScale(const EditGroupScaleContext& context);
    static void drawEditLinearArray(const EditLinearArrayContext& context);
    static void drawEditScatterAlongCurve(const EditScatterAlongCurveContext& context);
    static void drawEditBatchRename(const EditBatchRenameContext& context);
    static void drawEditHistory(const EditHistoryContext& context);
    static void drawEditObjectActions(const EditObjectActionsContext& context);
    static void drawEditSelectionActions(const EditSelectionActionsContext& context);
    static void drawEditSelectByType(const EditSelectByTypeContext& context);
    static void drawEditSelectByTag(const EditSelectByTagContext& context);
    static void drawEditSelectByMaterial(const EditSelectByMaterialContext& context);
    static void drawPanelToggles(bool& timeline, bool& registry, bool& ai, bool& validation);
    static void drawOverlays(bool& edges, bool& wireframe, bool& stats, bool& shadowDebug, bool& snap);
    static void drawViewDirections(float& yaw, float& pitch);
    static void drawFocusSelection(const std::function<void()>& focus);
    static void drawCameraBookmarks(const CameraBookmarksContext& context);
    static void drawWalkMode(const WalkModeContext& context);
    static void drawHelpMenu(const HelpMenuContext& context);
};

} // namespace MeshCraft::Application::UI
