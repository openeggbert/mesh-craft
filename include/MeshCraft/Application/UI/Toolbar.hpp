#pragma once

#include "MeshCraft/Editor/ActiveTool.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <functional>

namespace MeshCraft::Application::UI {

struct ToolbarToolsContext {
    ActiveTool& activeTool;
    std::function<void(ActiveTool)> selectTool;
    std::function<void(Mc3::ObjectType)> addPrimitive;
};

class Toolbar final {
public:
    static void drawTools(ToolbarToolsContext& context);
    static void drawDisplayToggles(bool& gizmoLocalSpace, bool& showEdges,
                                   bool& showWireframe, bool& showBoundingBox);
    static void drawSurfaceSnap(bool& enabled);
};

} // namespace MeshCraft::Application::UI
