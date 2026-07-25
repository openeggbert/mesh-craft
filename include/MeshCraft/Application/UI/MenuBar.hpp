#pragma once

#include "MeshCraft/Editor/CameraBookmarks.hpp"

#include <functional>

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

class MenuBar final {
public:
    static void drawAddMenu(const std::function<void(Mc3::ObjectType)>& addPrimitive);
    static void drawEditClipboard(const EditClipboardContext& context);
    static void drawEditHistory(const EditHistoryContext& context);
    static void drawPanelToggles(bool& timeline, bool& registry, bool& ai, bool& validation);
    static void drawOverlays(bool& edges, bool& wireframe, bool& stats, bool& shadowDebug, bool& snap);
    static void drawViewDirections(float& yaw, float& pitch);
    static void drawFocusSelection(const std::function<void()>& focus);
    static void drawCameraBookmarks(const CameraBookmarksContext& context);
    static void drawWalkMode(const WalkModeContext& context);
    static void drawHelpMenu(const HelpMenuContext& context);
};

} // namespace MeshCraft::Application::UI
