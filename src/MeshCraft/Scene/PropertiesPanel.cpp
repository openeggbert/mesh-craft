#include "MeshCraft/Scene/PropertiesPanel.hpp"

namespace MeshCraft::Scene {

void PropertiesPanel::setSelection(std::shared_ptr<Mc3::Mc3Object> object) {
    selected_ = std::move(object);
}

// TODO: render properties UI for the selected object

} // namespace MeshCraft::Scene
