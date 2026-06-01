#include "MeshCraft/MeshCraftApplication.hpp"

#include <Nova3D/Nova3D.h>

using namespace Nova3D;

namespace MeshCraft {

MeshCraftApplication::MeshCraftApplication(Nova3D::Context* context)
    : Application(context)
{}

void MeshCraftApplication::Setup() {
    // TODO: configure engine settings (window title, size, etc.)
}

void MeshCraftApplication::Start() {
    document_.model = "Untitled";

    hierarchyPanel_  = std::make_unique<Scene::SceneHierarchyPanel>(document_);
    propertiesPanel_ = std::make_unique<Scene::PropertiesPanel>();

    // TODO: set up 3D viewport camera
    // TODO: load last-used document if available
}

void MeshCraftApplication::Stop() {
    // TODO: prompt to save unsaved changes
}

void MeshCraftApplication::Update(float /*timeStep*/) {
    auto* input  = GetContext()->GetInput();
    auto* engine = GetContext()->GetEngine();

    if (input->GetKeyDown(KEY_ESCAPE))
        engine->Exit();

    // TODO: forward input to active editor tool
    // TODO: render viewport
    // TODO: render UI panels
}

} // namespace MeshCraft
