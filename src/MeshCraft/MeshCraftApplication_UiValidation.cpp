#include "MeshCraft/MeshCraftApplication.hpp"

#include <MeshCraft/Mc3/Mc3Validation.hpp>
#include <imgui.h>

namespace MeshCraft {

void MeshCraftApplication::recordValidation(std::string source, Mc3::Mc3Validation v) {
    lastValidationSource_ = std::move(source);
    lastValidation_       = std::move(v);
}

void MeshCraftApplication::drawValidationPanel() {
    if (!showValidationPanel_) return;

    ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Validation", &showValidationPanel_)) {
        ImGui::End();
        return;
    }

    if (lastValidationSource_.empty()) {
        ImGui::TextDisabled("No load, save, or export has run yet this session.");
        ImGui::End();
        return;
    }

    ImGui::Text("Last: %s", lastValidationSource_.c_str());
    ImGui::SameLine();
    if (lastValidation_.empty()) {
        ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f), "— no findings");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "— %zu warning(s), %zu error(s)",
                            lastValidation_.warningCount(), lastValidation_.errorCount());
    }

    ImGui::Spacing();

    if (lastValidation_.empty()) {
        ImGui::End();
        return;
    }

    const float tableH = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginTable("##validationtable", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, tableH)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("",         ImGuiTableColumnFlags_WidthFixed,   18.0f);
        ImGui::TableSetupColumn("Object",   ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableSetupColumn("Field",    ImGuiTableColumnFlags_WidthStretch, 0.14f);
        ImGui::TableSetupColumn("Message",  ImGuiTableColumnFlags_WidthStretch, 0.40f);
        ImGui::TableSetupColumn("Repair",   ImGuiTableColumnFlags_WidthStretch, 0.26f);
        ImGui::TableHeadersRow();

        for (const auto& e : lastValidation_.entries) {
            ImGui::TableNextRow();
            bool isError = e.severity == Mc3::Mc3ValidationSeverity::Error;
            ImVec4 col = isError ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f) : ImVec4(1.0f, 0.75f, 0.35f, 1.0f);
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(col, "%s", isError ? "E" : "W");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(e.objectId.empty() ? "(document)" : e.objectId.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(e.field.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s", e.message.c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextWrapped("%s", e.suggestedRepair.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace MeshCraft
