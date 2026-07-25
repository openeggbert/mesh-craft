#include "MeshCraft/Application/UI/GizmoDragOverlay.hpp"

#include <imgui.h>

namespace MeshCraft::Application::UI {

void GizmoDragOverlay::draw(const GizmoDragOverlayContext& context)
{
    ImVec2 mousePosition = ImGui::GetIO().MousePos;
    ImGui::SetNextWindowPos(ImVec2(mousePosition.x + 18.0f, mousePosition.y - 10.0f),
                            ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 4));
    ImGui::Begin("##gizmoDelta", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    static constexpr const char* kAxis[] = {"X", "Y", "Z"};
    static const ImVec4 kAxisColor[] = {
        {1.0f, 0.25f, 0.25f, 1.0f},
        {0.25f, 1.0f, 0.25f, 1.0f},
        {0.25f, 0.55f, 1.0f, 1.0f},
    };
    const float delta = context.currentValue - context.startValue;

    ImGui::TextColored(kAxisColor[context.axisIndex], "%s", kAxis[context.axisIndex]);
    ImGui::SameLine(0, 4);
    if (delta >= 0.0f)
        ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "+%.4g%s", delta, context.unit.data());
    else
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.8f, 1.0f), "%.4g%s", delta, context.unit.data());
    ImGui::SameLine(0, 6);
    ImGui::TextDisabled("(%.4g)", context.currentValue);
    if (context.isRotation && (ImGui::GetIO().KeyCtrl || context.snapEnabled)) {
        ImGui::SameLine(0, 6);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                           "[snap %.4g\xc2\xb0]", context.snapRotation);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace MeshCraft::Application::UI
