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
    // AUD-036b: forwards to MeshCraftApplication::undoOnActivate -- snapshots
    // on the just-drawn widget's activation frame and returns its `changed`
    // value unmodified. Prefer this over hand-rolling
    // `if (IsItemActivated()) ctx.pushUndo();` next to a separately-stored
    // changed bool: it collapses the two steps into one call so the snapshot
    // can't end up nested inside the changed-block by accident (the AUD-036
    // dead-pattern bug class).
    std::function<bool(bool)>    undoOnActivate;
    std::function<void()>        markModified;    // modified_ = true; updateWindowTitle()
    std::function<void()>        updateTitle;     // updateWindowTitle() only
    std::function<void(Mc3::Mc3Object&, const std::vector<Mc3::AnimatedProperty>&)>
                                 insertKeyframes;
    std::function<void()>        resetPivot;
    std::function<void()>        setPivotMoveTool;
    std::function<void()>        generateSimpleCollisionProxy;
    std::function<void(const Mc3::Mc3Object*)>  openCsgExport;
    std::function<void(const std::string&)>     openMeshBrowse;
    // SYS-W14-15: opens a native OS file-open dialog for a material's texture
    // slot (matId, slot) -- see MeshCraftApplication::browseForMaterialTexture.
    std::function<void(const std::string&, const std::string&)> browseForTexture;
};

class PropertiesPanel {
public:
    void draw(float panelX, float panelY, float panelW, float panelH,
              const PropertiesContext& ctx);
};

} // namespace MeshCraft::Scene
