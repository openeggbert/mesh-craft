#pragma once

#include <string_view>

namespace MeshCraft::Application::UI {

struct GizmoDragOverlayContext {
    int axisIndex;
    float currentValue;
    float startValue;
    std::string_view unit{""};
    bool isRotation;
    bool snapEnabled;
    float snapRotation;
};

class GizmoDragOverlay final {
public:
    static void draw(const GizmoDragOverlayContext& context);
};

} // namespace MeshCraft::Application::UI
