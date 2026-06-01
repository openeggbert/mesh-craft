#pragma once

#include "MeshCraft/Editor/EditorViewport.hpp"
#include "MeshCraft/Editor/SelectionManager.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Scene/PropertiesPanel.hpp"
#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"

#include <Nova3D/Application.h>
#include <memory>

namespace MeshCraft {

class MeshCraftApplication : public Nova3D::Application {
public:
    explicit MeshCraftApplication(Nova3D::Context* context);

    void Setup() override;
    void Start() override;
    void Stop()  override;
    void Update(float timeStep) override;

private:
    Mc3::Mc3Document document_;

    Editor::EditorViewport    viewport_;
    Editor::SelectionManager  selection_;

    std::unique_ptr<Scene::SceneHierarchyPanel> hierarchyPanel_;
    std::unique_ptr<Scene::PropertiesPanel>     propertiesPanel_;

    // TODO: tool palette and active tool
    // TODO: action/animation editor
};

} // namespace MeshCraft
