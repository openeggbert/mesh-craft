#pragma once

#include <array>

namespace MeshCraft::Application::UI {

struct MeasurementOverlayContext {
    float firstScreenX;
    float firstScreenY;
    bool hasSecondPoint;
    float secondScreenX;
    float secondScreenY;
    float distance;
    std::array<float, 3> firstPoint;
    std::array<float, 3> secondPoint;
    int viewportX;
    int viewportY;
    int viewportHeight;
};

class MeasurementOverlay final {
public:
    static void draw(const MeasurementOverlayContext& context);
};

} // namespace MeshCraft::Application::UI
