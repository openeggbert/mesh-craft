#pragma once

namespace MeshCraft::Application::UI {

class MenuBar final {
public:
    static void drawPanelToggles(bool& timeline, bool& registry, bool& ai, bool& validation);
    static void drawOverlays(bool& edges, bool& wireframe, bool& stats, bool& shadowDebug, bool& snap);
    static void drawViewDirections(float& yaw, float& pitch);
};

} // namespace MeshCraft::Application::UI
