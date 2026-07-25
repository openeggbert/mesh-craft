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
};

} // namespace MeshCraft::Application::UI
