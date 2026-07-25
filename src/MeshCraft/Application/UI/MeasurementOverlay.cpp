#include "MeshCraft/Application/UI/MeasurementOverlay.hpp"

#include <imgui.h>

#include <cstdio>

namespace MeshCraft::Application::UI {

void MeasurementOverlay::draw(const MeasurementOverlayContext& context)
{
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const ImVec2 firstPoint(context.firstScreenX, context.firstScreenY);
    drawList->AddCircleFilled(firstPoint, 5.0f, IM_COL32(80, 220, 220, 220));

    if (context.hasSecondPoint) {
        const ImVec2 secondPoint(context.secondScreenX, context.secondScreenY);
        drawList->AddLine(firstPoint, secondPoint, IM_COL32(80, 220, 220, 200), 2.0f);
        drawList->AddCircleFilled(secondPoint, 5.0f, IM_COL32(80, 220, 220, 220));

        const float midpointX = (firstPoint.x + secondPoint.x) * 0.5f;
        const float midpointY = (firstPoint.y + secondPoint.y) * 0.5f;
        char label[64];
        std::snprintf(label, sizeof(label), "%.4g u", context.distance);
        drawList->AddText(ImVec2(midpointX + 8, midpointY - 8),
                          IM_COL32(200, 255, 200, 255), label);
    } else {
        ImGui::SetNextWindowPos(ImVec2(firstPoint.x + 12, firstPoint.y - 24), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.60f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 3));
        ImGui::Begin("##mhint", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::TextDisabled("Click second point");
        ImGui::End();
        ImGui::PopStyleVar();
    }

    if (!context.hasSecondPoint)
        return;

    ImGui::SetNextWindowPos(
        ImVec2(static_cast<float>(context.viewportX) + 8,
               static_cast<float>(context.viewportY + context.viewportHeight) - 8),
        ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::Begin("##minfo", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "Distance: %.4g u", context.distance);
    ImGui::TextDisabled("A: %.2f, %.2f, %.2f", context.firstPoint[0], context.firstPoint[1],
                        context.firstPoint[2]);
    ImGui::TextDisabled("B: %.2f, %.2f, %.2f", context.secondPoint[0], context.secondPoint[1],
                        context.secondPoint[2]);
    ImGui::TextDisabled("Right-click to reset");
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace MeshCraft::Application::UI
