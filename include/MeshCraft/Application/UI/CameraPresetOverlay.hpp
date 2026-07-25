#pragma once

#include <functional>
#include <string_view>

namespace MeshCraft::Application::UI {

struct CameraPresetOverlayContext {
    bool orthographic;
    bool hasSelectedCamera;
    bool lookingThroughCamera;
    std::string_view selectedCameraName;
    std::function<void()> resetCamera;
    std::function<void(float, float)> setOrbitDirection;
    std::function<void(bool)> setOrthographic;
    std::function<void()> toggleLookThroughCamera;
};

class CameraPresetOverlay final {
public:
    static void draw(float viewportLeft, float viewportTop,
                     const CameraPresetOverlayContext& context);
};

} // namespace MeshCraft::Application::UI
