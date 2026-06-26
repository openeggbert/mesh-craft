#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraft/Scene/PropertiesPanel.hpp"

#include <cstring>
#include <filesystem>

namespace MeshCraft {

void MeshCraftApplication::drawPropertiesPanel(float panelY, float panelH, int screenW, int screenH)
{
    (void)screenH;
    const float panelX = static_cast<float>(screenW - kRightPanelW);
    const float panelW = static_cast<float>(kRightPanelW);

    Scene::PropertiesContext ctx{
        .document           = document_,
        .selection          = selection_,
        .lockedIds          = lockedIds_,
        .renderer           = sceneRenderer_.get(),
        .showTimeline       = showTimeline_,
        .currentActionName  = currentActionName_,
        .pivotEditMode      = pivotEditMode_,
        .pendingDropTexture = pendingDropTexture_,
        .hoveredTexSlot     = hoveredTexSlot_,
        .hoveredTexMatId    = hoveredTexMatId_,
        .selectedMaterialKey = selectedMaterialKey_,
        .pushUndo           = [this]{ pushUndo(); },
        .markModified       = [this]{ modified_ = true; updateWindowTitle(); },
        .updateTitle        = [this]{ updateWindowTitle(); },
        .insertKeyframes    = [this](Mc3::Mc3Object& obj,
                                     const std::vector<Mc3::AnimatedProperty>& props) {
                                  insertAnimKeyframes(obj, props);
                              },
        .resetPivot         = [this]{ resetPivot(); },
        .setPivotMoveTool   = [this]{ activeTool_ = ActiveTool::Move; },
        .openCsgExport      = [this](const Mc3::Mc3Object* obj) {
            std::filesystem::path base = currentFile_.empty()
                ? std::filesystem::current_path() / "csg_export.obj"
                : currentFile_.parent_path() /
                  (currentFile_.stem().string() + "_csg.obj");
            std::strncpy(csgExportBuf_, base.string().c_str(), sizeof(csgExportBuf_) - 1);
            csgExportBuf_[sizeof(csgExportBuf_) - 1] = '\0';
            csgExportErr_[0] = '\0';
            csgExportObj_ = obj;
            csgExportOpen_ = true;
        },
        .openMeshBrowse     = [this](const std::string& srcPath) {
            std::strncpy(meshBrowseBuf_, srcPath.c_str(), sizeof(meshBrowseBuf_) - 1);
            meshBrowseBuf_[sizeof(meshBrowseBuf_) - 1] = '\0';
            meshBrowseErr_[0] = '\0';
            meshBrowseOpen_   = true;
        },
    };

    propertiesPanel_->draw(panelX, panelY, panelW, panelH, ctx);
}

} // namespace MeshCraft
