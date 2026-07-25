#pragma once

#include <string_view>

namespace MeshCraft::Application::UI {

struct StatsOverlayContext {
    float viewportRight;
    float viewportTop;
    float framesPerSecond;
    bool isolateActive;
    bool lookingThroughCamera;
    std::string_view selectedCameraName{""};
    int totalObjectCount;
    int visibleObjectCount;
    int lockedObjectCount;
    int selectedObjectCount;
    float cameraDistance;
    float cameraTargetX;
    float cameraTargetY;
    float cameraTargetZ;
    int sceneVertexCount;
    int sceneTriangleCount;
};

class StatsOverlay final {
public:
    static void draw(const StatsOverlayContext& context);
};

} // namespace MeshCraft::Application::UI
