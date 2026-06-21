#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraft/ModelRegistry.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <imgui.h>

#include <cstring>
#include <string>

namespace MeshCraft {

void MeshCraftApplication::drawRegistryPanel() {
    if (!showRegistryPanel_) return;

    // Auto-open the default DB on first display
    if (!registry_.isOpen()) {
        try {
            registry_.open(ModelRegistry::defaultPath());
            regResultsDirty_ = true;
        } catch (const std::exception& ex) {
            setStatusMsg(std::string("Registry: ") + ex.what(), true);
            showRegistryPanel_ = false;
            return;
        }
    }

    // Refresh search results when query changed or marked dirty
    if (regResultsDirty_) {
        regCachedResults_ = registry_.search(regSearchBuf_);
        regResultsDirty_  = false;
    }

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Model Registry", &showRegistryPanel_)) {
        ImGui::End();
        return;
    }

    // Search bar
    ImGui::SetNextItemWidth(-60.0f);
    if (ImGui::InputTextWithHint("##regsearch", "Search by name, group or tags…",
                                 regSearchBuf_, sizeof(regSearchBuf_)))
        regResultsDirty_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        regSearchBuf_[0] = '\0';
        regResultsDirty_ = true;
    }

    ImGui::Spacing();

    // Results table
    const float tableH = ImGui::GetContentRegionAvail().y - 36.0f;
    if (ImGui::BeginTable("##regtable", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, tableH)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Group",   ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_WidthStretch, 0.38f);
        ImGui::TableSetupColumn("Variant", ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableSetupColumn("",        ImGuiTableColumnFlags_WidthFixed,   66.0f);
        ImGui::TableHeadersRow();

        for (auto& entry : regCachedResults_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(entry.group.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(entry.name.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(entry.variant.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(static_cast<int>(entry.id));
            if (ImGui::SmallButton("Insert")) {
                try {
                    std::string defId = registry_.insertIntoScene(document_, entry);
                    // Place an instance of the definition at the origin
                    auto obj          = std::make_shared<Mc3::Mc3Object>();
                    obj->type         = Mc3::ObjectType::Instance;
                    obj->definition   = defId;
                    obj->name         = entry.name +
                                        (entry.variant.empty() ? "" : "_" + entry.variant);
                    std::string base  = "reg_" + defId;
                    obj->id           = base;
                    int n             = 1;
                    while (flatFindById(obj->id))
                        obj->id = base + "_" + std::to_string(n++);

                    pushUndo();
                    document_.objects.push_back(obj);
                    modified_ = true;
                    setStatusMsg("Inserted '" + entry.name + "' from registry");
                } catch (const std::exception& ex) {
                    setStatusMsg(std::string("Insert failed: ") + ex.what(), true);
                }
            }
            // Delete button (right-click context or explicit —  keep visible for power users)
            ImGui::SameLine(0, 4);
            if (ImGui::SmallButton("X")) {
                registry_.remove(entry.id);
                regResultsDirty_ = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Remove from registry");
            ImGui::PopID();
        }

        if (regCachedResults_.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("(empty)");
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    if (ImGui::Button("Save Definition to Registry..."))
        regSaveDlgOpen_ = true;

    // -----------------------------------------------------------------------
    // Save-definition dialog (floating child window)
    // -----------------------------------------------------------------------
    if (regSaveDlgOpen_) {
        ImGui::SetNextWindowSize(ImVec2(360, 240), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Save Definition to Registry##regdlg",
                         &regSaveDlgOpen_, ImGuiWindowFlags_NoResize)) {

            ImGui::Text("Definition:");
            const char* preview = regSaveDefId_.empty() ? "(select)" : regSaveDefId_.c_str();
            if (ImGui::BeginCombo("##regdef", preview)) {
                for (const auto& [id, _] : document_.definitions) {
                    bool sel = (id == regSaveDefId_);
                    if (ImGui::Selectable(id.c_str(), sel)) {
                        regSaveDefId_ = id;
                        if (regSaveNameBuf_[0] == '\0')
                            std::strncpy(regSaveNameBuf_, id.c_str(),
                                         sizeof(regSaveNameBuf_) - 1);
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const float labelW = 70.0f;
            ImGui::Text("Group:");
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rg", regSaveGroupBuf_, sizeof(regSaveGroupBuf_));

            ImGui::Text("Name:");
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rn", regSaveNameBuf_, sizeof(regSaveNameBuf_));

            ImGui::Text("Variant:");
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rv", regSaveVariantBuf_, sizeof(regSaveVariantBuf_));

            ImGui::Text("Tags:");
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rt", regSaveTagsBuf_, sizeof(regSaveTagsBuf_));

            ImGui::Spacing();
            bool canSave = !regSaveDefId_.empty() && regSaveNameBuf_[0] != '\0';
            if (!canSave) ImGui::BeginDisabled();
            if (ImGui::Button("Save", ImVec2(80, 0))) {
                try {
                    auto entry = registry_.entryFromDefinition(
                        document_, regSaveDefId_,
                        regSaveGroupBuf_, regSaveNameBuf_,
                        regSaveVariantBuf_, regSaveTagsBuf_);
                    registry_.save(entry);
                    regResultsDirty_ = true;
                    regSaveDlgOpen_  = false;
                    setStatusMsg("Saved '" + std::string(regSaveNameBuf_) + "' to registry");
                } catch (const std::exception& ex) {
                    setStatusMsg(std::string("Save failed: ") + ex.what(), true);
                }
            }
            if (!canSave) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                regSaveDlgOpen_ = false;
            }
        }
        ImGui::End();
    }

    ImGui::End();
}

} // namespace MeshCraft
