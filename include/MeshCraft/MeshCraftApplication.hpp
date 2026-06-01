#pragma once

#include "MeshCraft/Editor/EditorViewport.hpp"
#include "MeshCraft/Editor/SelectionManager.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Scene/PropertiesPanel.hpp"
#include "MeshCraft/Scene/SceneHierarchyPanel.hpp"

#include <Microsoft/Xna/Framework/Game.hpp>
#include <Microsoft/Xna/Framework/GameTime.hpp>
#include <System/Object.hpp>
#include <memory>

namespace MeshCraft {

class MeshCraftApplication : public Microsoft::Xna::Framework::Game {
public:
    GetTypeNameHPP()

    MeshCraftApplication();

    void LoadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

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
