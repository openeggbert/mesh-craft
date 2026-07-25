#pragma once

namespace MeshCraft::Application::UI {

class MenuBar final {
public:
    static void drawPanelToggles(bool& timeline, bool& registry, bool& ai, bool& validation);
    static void drawOverlays(bool& edges, bool& wireframe, bool& stats, bool& shadowDebug, bool& snap);
};

} // namespace MeshCraft::Application::UI
