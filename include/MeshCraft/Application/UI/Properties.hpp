#pragma once

#include "MeshCraft/Scene/PropertiesPanel.hpp"

namespace MeshCraft::Application::UI {

class Properties final {
public:
    static void draw(Scene::PropertiesPanel& panel, float x, float y, float width,
                     float height, Scene::PropertiesContext& context);
};

} // namespace MeshCraft::Application::UI
