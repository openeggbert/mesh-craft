#include "MeshCraft/MeshCraftApplication.hpp"

#include <Microsoft/Xna/Framework/Input/Keyboard.hpp>
#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <System/Object.hpp>

namespace MeshCraft {

GetTypeNameCPP(MeshCraftApplication, "MeshCraft::MeshCraftApplication")

using namespace Microsoft::Xna::Framework;

MeshCraftApplication::MeshCraftApplication() {
    getWindowProperty().setTitleProperty("Mesh Craft");
}

void MeshCraftApplication::LoadContent() {
    document_.model = "Untitled";

    hierarchyPanel_  = std::make_unique<Scene::SceneHierarchyPanel>(document_);
    propertiesPanel_ = std::make_unique<Scene::PropertiesPanel>();

    // TODO: set up 3D viewport (camera, grid, default scene)
    // TODO: load last-used document if available
}

void MeshCraftApplication::Update(GameTime& /*gameTime*/) {
    auto ks = Input::Keyboard::GetState();

    if (ks.IsKeyDown(Input::Keys::Escape))
        Exit();

    // TODO: forward input to active editor tool
    // TODO: camera controls in EditorViewport
}

void MeshCraftApplication::Draw(const GameTime& /*gameTime*/) {
    // TODO: clear viewport and render 3D scene via CNA graphics device
    // TODO: render UI panels (hierarchy, properties, tool palette)
}

} // namespace MeshCraft
