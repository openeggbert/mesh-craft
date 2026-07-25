#pragma once

#include "MeshCraft/Editor/CameraBookmarks.hpp"

#include <functional>

namespace MeshCraft::Application::UI {

struct CameraBookmarksContext {
    const Editor::CameraBookmarks& bookmarks;
    std::function<void(int)> save;
    std::function<void(int)> restore;
};

class MenuBar final {
public:
    static void drawPanelToggles(bool& timeline, bool& registry, bool& ai, bool& validation);
    static void drawOverlays(bool& edges, bool& wireframe, bool& stats, bool& shadowDebug, bool& snap);
    static void drawViewDirections(float& yaw, float& pitch);
    static void drawFocusSelection(const std::function<void()>& focus);
    static void drawCameraBookmarks(const CameraBookmarksContext& context);
};

} // namespace MeshCraft::Application::UI
