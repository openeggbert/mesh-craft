#pragma once

namespace MeshCraft::Application::UI {

class MenuBar final {
public:
    static void drawPanelToggles(bool& timeline, bool& registry, bool& ai, bool& validation);
};

} // namespace MeshCraft::Application::UI
