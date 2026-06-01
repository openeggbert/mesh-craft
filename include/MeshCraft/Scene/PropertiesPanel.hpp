#pragma once

#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <memory>

namespace MeshCraft::Scene {

// Displays and edits the properties of the currently selected object.
class PropertiesPanel {
public:
    void setSelection(std::shared_ptr<Mc3::Mc3Object> object);

    // TODO: render properties UI (transform, material, collision, tags, states)

private:
    std::shared_ptr<Mc3::Mc3Object> selected_;
};

} // namespace MeshCraft::Scene
