#include "MeshCraft/Application/UI/Registry.hpp"

#include <imgui.h>

namespace MeshCraft::Application::UI {

void Registry::drawResults(RegistryResultsContext& context)
{
    const float available = ImGui::GetContentRegionAvail().y - 36.0f;
    const float tableHeight = available > 96.0f ? available : 96.0f;
    if (!ImGui::BeginTable("##regtable", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, tableHeight))) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 42.0f);
    ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthStretch, 0.15f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.25f);
    ImGui::TableSetupColumn("Variant", ImGuiTableColumnFlags_WidthStretch, 0.15f);
    ImGui::TableSetupColumn("Metadata", ImGuiTableColumnFlags_WidthStretch, 0.25f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 66.0f);
    ImGui::TableHeadersRow();

    for (auto& entry : context.results) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const std::size_t expectedPixels = static_cast<std::size_t>(ModelRegistry::kThumbnailWidth) *
                                           ModelRegistry::kThumbnailHeight * 4;
        const ImVec2 topLeft = ImGui::GetCursorScreenPos();
        constexpr float tile = 4.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(topLeft, ImVec2(topLeft.x + 32.0f, topLeft.y + 32.0f),
                                IM_COL32(45, 45, 45, 255));
        if (entry.thumbnailRgba.size() == expectedPixels) {
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    const std::size_t source = (static_cast<std::size_t>(y * 8 + 4) *
                                                    ModelRegistry::kThumbnailWidth + x * 8 + 4) * 4;
                    const auto* rgba = entry.thumbnailRgba.data() + source;
                    drawList->AddRectFilled(ImVec2(topLeft.x + x * tile, topLeft.y + y * tile),
                                            ImVec2(topLeft.x + (x + 1) * tile, topLeft.y + (y + 1) * tile),
                                            IM_COL32(rgba[0], rgba[1], rgba[2], rgba[3]));
                }
            }
        }
        ImGui::Dummy(ImVec2(34.0f, 34.0f));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Deterministic catalog preview; refreshed when this asset changes");
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(entry.group.c_str());
        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(entry.name.c_str());
        ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(entry.variant.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(entry.category.empty() ? "—" : entry.category.c_str());
        if (!entry.license.empty()) ImGui::TextDisabled("%s", entry.license.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("License: %s\nProvenance: %s\nSource: %s",
                              entry.license.empty() ? "unspecified" : entry.license.c_str(),
                              entry.provenance.empty() ? "unspecified" : entry.provenance.c_str(),
                              entry.source.empty() ? "unspecified" : entry.source.c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::PushID(static_cast<int>(entry.id));
        if (ImGui::SmallButton("Insert")) context.insert(entry);
        ImGui::SameLine(0, 4);
        if (ImGui::SmallButton("X")) context.remove(entry.id);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove from registry");
        ImGui::PopID();
    }
    if (context.results.empty()) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("(empty)");
    }
    ImGui::EndTable();
}

} // namespace MeshCraft::Application::UI
