#include "MeshCraft/EditorAlgorithms.hpp"
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
        .lockedIds          = objectLockState_.ids(),
        .renderer           = sceneRenderer_.get(),
        .showTimeline       = showTimeline_,
        .currentActionName  = currentActionName_,
        .pivotEditMode      = pivotEditMode_,
        .pendingDropTexture = pendingDropTexture_,
        .hoveredTexSlot     = hoveredTexSlot_,
        .hoveredTexMatId    = hoveredTexMatId_,
        .selectedMaterialKey = selectedMaterialKey_,
        .pushUndo           = [this]{ pushUndo(); },
        .undoOnActivate     = [this](bool changed){ return undoOnActivate(changed); },
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
        .browseForTexture   = [this](const std::string& matId, const std::string& slot) {
            browseForMaterialTexture(matId, slot);
        },
    };

    propertiesPanel_->draw(panelX, panelY, panelW, panelH, ctx);
}

// SYS-W14-15: opens a native OS file-open dialog for a material's texture
// slot. CNA::Devices::FileDialog::ShowOpenFile is asynchronous and its
// callback may run on a different thread (see PendingFileBrowse's own
// comment) -- the callback only ever writes into the shared, mutex-guarded
// result box; Update() is the sole place that acts on it.
void MeshCraftApplication::browseForMaterialTexture(const std::string& matId, const std::string& slot) {
    auto pending = std::make_shared<PendingFileBrowse>();
    pending->targetMatId = matId;
    pending->targetSlot  = slot;
    pendingFileBrowse_ = pending;

    static const std::vector<CNA::Devices::FileDialogFilter> kImageFilters = {
        {"Images", "png;jpg;jpeg;webp;tga;bmp;hdr"},
        {"All files", "*"},
    };
    CNA::Devices::FileDialog::ShowOpenFile(
        [pending](const std::vector<std::string>& files) {
            std::lock_guard<std::mutex> lock(pending->mutex);
            if (!files.empty()) pending->path = files.front();
            pending->done.store(true);
        },
        kImageFilters);
}

std::string MeshCraftApplication::registerTextureFromPath(const std::string& path) {
    return registerTextureFromPathAlg(path, document_);
}

} // namespace MeshCraft
