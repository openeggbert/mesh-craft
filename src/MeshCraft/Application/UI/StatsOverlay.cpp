#include "MeshCraft/Application/UI/StatsOverlay.hpp"

#include <imgui.h>

namespace MeshCraft::Application::UI {

void StatsOverlay::draw(const StatsOverlayContext& context)
{
    ImGui::SetNextWindowPos(ImVec2(context.viewportRight - 8.0f, context.viewportTop + 8.0f),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.50f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::Begin("##statsoverlay", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%.0f FPS", context.framesPerSecond);
    if (context.isolateActive)
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.1f, 1.0f), "ISOLATED");
    if (context.lookingThroughCamera) {
        ImGui::TextColored(ImVec4(0.75f, 0.45f, 1.0f, 1.0f), "CAM: %.*s",
                           static_cast<int>(context.selectedCameraName.size()),
                           context.selectedCameraName.data());
    }
    ImGui::Separator();
    ImGui::Text("Objects: %d", context.totalObjectCount);
    ImGui::Text("Visible: %d", context.visibleObjectCount);
    if (context.lockedObjectCount > 0)
        ImGui::Text("Locked:  %d", context.lockedObjectCount);
    ImGui::Separator();
    if (context.selectedObjectCount > 0) {
        ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f),
                           "Selected: %d", context.selectedObjectCount);
    } else {
        ImGui::TextDisabled("Selected: 0");
    }
    ImGui::Separator();
    ImGui::TextDisabled("Cam dist: %.2f", context.cameraDistance);
    ImGui::TextDisabled("Target: %.1f, %.1f, %.1f",
                        context.cameraTargetX, context.cameraTargetY, context.cameraTargetZ);
    ImGui::Separator();
    ImGui::TextDisabled("Verts: %d", context.sceneVertexCount);
    ImGui::TextDisabled("Tris:  %d", context.sceneTriangleCount);
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace MeshCraft::Application::UI
