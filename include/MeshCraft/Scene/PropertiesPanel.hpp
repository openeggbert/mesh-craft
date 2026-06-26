#pragma once

#include "MeshCraft/Editor/SelectionManager.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace MeshCraft::Renderer { class SceneRenderer; }

namespace MeshCraft::Scene {

struct PropertiesContext {
    Mc3::Mc3Document&            document;
    Editor::SelectionManager&    selection;
    const std::set<std::string>& lockedIds;
    Renderer::SceneRenderer*     renderer;
    bool                         showTimeline;
    const std::string&           currentActionName;
    bool&                        pivotEditMode;
    // Texture drag-drop state (D6)
    const std::string&           pendingDropTexture;
    std::string&                 hoveredTexSlot;
    std::string&                 hoveredTexMatId;
    const std::string&           selectedMaterialKey;
    // Callbacks
    std::function<void()>        pushUndo;
    std::function<void()>        markModified;    // modified_ = true; updateWindowTitle()
    std::function<void()>        updateTitle;     // updateWindowTitle() only
    std::function<void(Mc3::Mc3Object&, const std::vector<Mc3::AnimatedProperty>&)>
                                 insertKeyframes;
    std::function<void()>        resetPivot;
    std::function<void()>        setPivotMoveTool;
    std::function<void(const Mc3::Mc3Object*)>  openCsgExport;
    std::function<void(const std::string&)>     openMeshBrowse;
};

class PropertiesPanel {
public:
    void draw(float panelX, float panelY, float panelW, float panelH,
              const PropertiesContext& ctx);
};

} // namespace MeshCraft::Scene
