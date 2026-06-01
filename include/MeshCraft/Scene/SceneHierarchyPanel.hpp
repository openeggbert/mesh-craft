#pragma once

#include "MeshCraft/Mc3/Mc3Document.hpp"

namespace MeshCraft::Scene {

// Displays the MC3 object tree and allows selecting objects.
class SceneHierarchyPanel {
public:
    explicit SceneHierarchyPanel(Mc3::Mc3Document& document);

    // TODO: render panel UI
    // TODO: emit selection-changed events

private:
    Mc3::Mc3Document& document_;
};

} // namespace MeshCraft::Scene
