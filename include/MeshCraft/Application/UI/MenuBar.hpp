#pragma once

namespace MeshCraft::Application::UI {

class MenuBar final {
public:
    static void drawPanelToggles(bool& timeline, bool& registry, bool& ai, bool& validation);
    static void drawOverlays(bool& edges, bool& wireframe, bool& stats, bool& shadowDebug, bool& snap);
    static void drawViewDirections(float& yaw, float& pitch);
    static void drawFocusSelection(const std::function<void()>& focus);
};

} // namespace MeshCraft::Application::UI
