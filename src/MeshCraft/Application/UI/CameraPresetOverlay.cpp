#include "MeshCraft/Application/UI/CameraPresetOverlay.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"

#include <imgui.h>

namespace MeshCraft::Application::UI {

void CameraPresetOverlay::draw(float viewportLeft, float viewportTop,
                               const CameraPresetOverlayContext& context)
{
    ImGui::SetNextWindowPos(ImVec2(viewportLeft, viewportTop), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.45f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3, 3));
    ImGui::Begin("##campresets", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    for (const auto& preset : cameraPresetsAlg()) {
        if (ImGui::Button(preset.label, ImVec2(38, 18))) {
            if (preset.reset) context.resetCamera();
            else context.setOrbitDirection(preset.yaw, preset.pitch);
        }
        if (ImGui::IsItemHovered()) {
            if (preset.reset) ImGui::SetTooltip("Reset to default perspective view");
            else ImGui::SetTooltip("Set camera to %s view", preset.label);
        }
        ImGui::SameLine();
    }

    if (context.orthographic)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.80f, 1.f));
    if (ImGui::Button(context.orthographic ? "Ortho" : "Persp", ImVec2(42, 18)))
        context.setOrthographic(!context.orthographic);
    if (context.orthographic)
        ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(context.orthographic
            ? "Orthographic projection (click for Perspective)"
            : "Perspective projection (click for Orthographic)");
    ImGui::SameLine();

    if (context.hasSelectedCamera) {
        if (context.lookingThroughCamera)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.30f, 0.80f, 1.f));
        if (ImGui::Button("Cam", ImVec2(34, 18)))
            context.toggleLookThroughCamera();
        if (context.lookingThroughCamera)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            if (context.lookingThroughCamera)
                ImGui::SetTooltip("Looking through: %s\nClick to exit", context.selectedCameraName.data());
            else
                ImGui::SetTooltip("Look through camera: %s", context.selectedCameraName.data());
        }
        ImGui::SameLine();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace MeshCraft::Application::UI
