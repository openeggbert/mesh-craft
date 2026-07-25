#include "MeshCraft/Application/UI/StatusBar.hpp"

#include <imgui.h>

#include <string>

namespace MeshCraft::Application::UI {

void StatusBar::draw(const StatusBarContext& context)
{
    ImGui::SetNextWindowPos(ImVec2(0, static_cast<float>(context.screenHeight - context.statusHeight)));
    ImGui::SetNextWindowSize(
        ImVec2(static_cast<float>(context.screenWidth), static_cast<float>(context.statusHeight)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 3));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f, 0.094f, 0.176f, 1.0f));
    ImGui::Begin("##statusbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);

    if (context.hasNotification) {
        const ImVec4 color = context.notificationIsError
            ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f)
            : ImVec4(0.55f, 1.0f, 0.55f, 1.0f);
        ImGui::TextColored(color, "%.*s", static_cast<int>(context.notificationMessage.size()),
                           context.notificationMessage.data());
    } else if (context.selectedObjectCount > 0) {
        ImGui::Text("%d objects | %d selected | %.*s", context.totalObjectCount,
                    context.selectedObjectCount, static_cast<int>(context.selectedObjectName.size()),
                    context.selectedObjectName.data());
    } else {
        ImGui::Text("%d objects", context.totalObjectCount);
    }

    if (context.hasValidation) {
        const std::string label =
            "[!] " + std::to_string(context.validationEntryCount) + " validation note(s)";
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - 12.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "%s", label.c_str());
        if (ImGui::IsItemClicked())
            context.openValidation();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("From: %.*s\nClick to open the Validation panel",
                              static_cast<int>(context.validationSource.size()),
                              context.validationSource.data());
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace MeshCraft::Application::UI
