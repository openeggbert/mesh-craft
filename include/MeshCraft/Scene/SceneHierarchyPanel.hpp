#pragma once

#include "MeshCraft/Editor/ObjectLockState.hpp"
#include "MeshCraft/Editor/SelectionManager.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace MeshCraft::Scene {

struct HierarchyCallbacks {
    std::function<void()> pushUndo;
    std::function<void()> markModified;     // modified_ = true; updateWindowTitle()
    std::function<void()> duplicateSel;
    std::function<void()> deleteSel;
    std::function<void()> selectParent;
    std::function<void()> selectChildren;
    std::function<void()> openBatchRename;
};

class SceneHierarchyPanel {
public:
    explicit SceneHierarchyPanel(Mc3::Mc3Document& document);

    void draw(Editor::SelectionManager& selection,
              Editor::ObjectLockState& objectLockState,
              const HierarchyCallbacks& cb);

    void scrollToObject(const std::string& id) { scrollToId_ = id; }

private:
    Mc3::Mc3Document& document_;

    char        searchBuf_[128]{};
    int         typeFilter_{0};
    std::string layerFilter_, tagFilter_, matFilter_;
    bool        filterOr_{false};

    std::string anchorId_;
    std::string renamingId_;
    char        renameBuf_[256]{};
    bool        renameNeedsFocus_{false};

    std::string scrollToId_;
    std::vector<std::shared_ptr<Mc3::Mc3Object>> flatOrder_;
};

} // namespace MeshCraft::Scene
