#include "MeshCraft/Application/MeshCraftApplication.hpp"
#include "MeshCraft/Application/UI/Registry.hpp"
#include "MeshCraft/ModelRegistry.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <imgui.h>

#include <cstring>
#include <string>
#include <vector>

namespace MeshCraft::Application {

void MeshCraftApplication::drawRegistryPanel() {
    if (!showRegistryPanel_) return;

    // Auto-open the default DB on first display
    if (!registry_.isOpen()) {
        try {
            registry_.open(ModelRegistry::defaultPath());
        } catch (const std::exception& ex) {
            setStatusMsg(std::string("Registry: ") + ex.what(), true);
            showRegistryPanel_ = false;
            return;
        }
        if (!registry_.isOpen()) {
            setStatusMsg("Model Registry is not available in this build", true);
            showRegistryPanel_ = false;
            return;
        }
        regResultsDirty_ = true;
    }

    // Refresh search results when query changed or marked dirty
    if (regResultsDirty_) {
        regCachedResults_ = registry_.search(ModelRegistry::SearchFilter{
            .text = regSearchBuf_,
            .tag = regTagFilterBuf_,
            .category = regCategoryFilterBuf_,
            .license = regLicenseFilterBuf_,
            .provenance = regProvenanceFilterBuf_,
        });
        regResultsDirty_  = false;
    }

    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Model Registry", &showRegistryPanel_)) {
        ImGui::End();
        return;
    }

    // Search bar
    ImGui::SetNextItemWidth(-60.0f);
    if (ImGui::InputTextWithHint("##regsearch",
                                 "Search name, group, metadata, tags, description or source…",
                                 regSearchBuf_, sizeof(regSearchBuf_)))
        regResultsDirty_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        regSearchBuf_[0] = '\0';
        regTagFilterBuf_[0] = '\0';
        regCategoryFilterBuf_[0] = '\0';
        regLicenseFilterBuf_[0] = '\0';
        regProvenanceFilterBuf_[0] = '\0';
        regResultsDirty_ = true;
    }

    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputTextWithHint("##regtag", "Tag filter", regTagFilterBuf_, sizeof(regTagFilterBuf_)))
        regResultsDirty_ = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputTextWithHint("##regcategory", "Category", regCategoryFilterBuf_, sizeof(regCategoryFilterBuf_)))
        regResultsDirty_ = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputTextWithHint("##reglicense", "License", regLicenseFilterBuf_, sizeof(regLicenseFilterBuf_)))
        regResultsDirty_ = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##regprovenance", "Provenance", regProvenanceFilterBuf_, sizeof(regProvenanceFilterBuf_)))
        regResultsDirty_ = true;

    ImGui::Spacing();

    const auto materialReport = ModelRegistry::inspectMaterials(document_);
    if (!materialReport.duplicateMaterialGroups.empty() || !materialReport.unusedMaterialIds.empty()) {
        if (ImGui::CollapsingHeader("Material health", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& group : materialReport.duplicateMaterialGroups) {
                std::string ids;
                for (const auto& id : group) {
                    if (!ids.empty()) ids += ", ";
                    ids += id;
                }
                ImGui::BulletText("Identical material values: %s", ids.c_str());
            }
            for (const auto& id : materialReport.unusedMaterialIds)
                ImGui::BulletText("Unused material: %s", id.c_str());
        }
    }

    UI::RegistryResultsContext resultsContext{
        .results = regCachedResults_,
        .insert = [this](const ModelRegistry::Entry& entry) {
            try {
                pushUndo();
                std::string defId;
                try {
                    defId = registry_.insertIntoScene(document_, entry);
                } catch (...) {
                    undoManager_.popUndoWithoutApplying();
                    throw;
                }
                auto obj = std::make_shared<Mc3::Mc3Object>();
                obj->type = Mc3::ObjectType::Instance;
                obj->definition = defId;
                obj->name = entry.name + (entry.variant.empty() ? "" : "_" + entry.variant);
                std::string base = "reg_" + defId;
                obj->id = base;
                int n = 1;
                while (flatFindById(obj->id)) obj->id = base + "_" + std::to_string(n++);
                document_.objects.push_back(obj);
                modified_ = true;
                if (!document_.imports.empty()) resolveImports();
                if (importHealthError_.empty()) {
                    setStatusMsg("Inserted '" + entry.name + "' from registry");
                } else {
                    setStatusMsg("Inserted '" + entry.name +
                                 "', but import resolution needs attention: " + importHealthError_, true);
                }
            } catch (const std::exception& ex) {
                setStatusMsg(std::string("Insert failed: ") + ex.what(), true);
            }
        },
        .remove = [this](int64_t id) { registry_.remove(id); regResultsDirty_ = true; },
    };
    UI::Registry::drawResults(resultsContext);

    ImGui::Separator();
    if (ImGui::Button("Save Definition to Registry..."))
        regSaveDlgOpen_ = true;
    ImGui::SameLine();
    if (regCachedResults_.empty()) ImGui::BeginDisabled();
    if (ImGui::Button("Export filtered asset pack...")) {
        regPackDlgOpen_ = true;
        regPackErr_.clear();
    }
    if (regCachedResults_.empty()) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu matching asset%s", regCachedResults_.size(),
                        regCachedResults_.size() == 1 ? "" : "s");

    // -----------------------------------------------------------------------
    // Save-definition dialog (floating child window)
    // -----------------------------------------------------------------------
    if (regSaveDlgOpen_) {
        ImGui::SetNextWindowSize(ImVec2(380, 310), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Save Definition to Registry##regdlg",
                         &regSaveDlgOpen_, ImGuiWindowFlags_NoResize)) {

            // When saving from an AI result, list aiPendingDoc_ definitions, not document_.
            const auto& defSource = (regSaveFromAi_ && aiPendingDoc_.has_value())
                                    ? aiPendingDoc_->definitions
                                    : document_.definitions;
            const char* preview = regSaveDefId_.empty() ? "(select)" : regSaveDefId_.c_str();
            if (ImGui::BeginCombo("##regdef", preview)) {
                for (const auto& [id, _] : defSource) {
                    bool sel = (id == regSaveDefId_);
                    if (ImGui::Selectable(id.c_str(), sel)) {
                        regSaveDefId_ = id;
                        if (regSaveNameBuf_[0] == '\0')
                            copyToBuf(regSaveNameBuf_, id.c_str());
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const float labelW = 80.0f;
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

            ImGui::Text("Desc.:");
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rdesc", regSaveDescBuf_, sizeof(regSaveDescBuf_));

            ImGui::Text("Source:");
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rsource", regSaveSourceBuf_, sizeof(regSaveSourceBuf_));

            ImGui::Spacing();
            bool canSave = !regSaveDefId_.empty() && regSaveNameBuf_[0] != '\0';
            if (!canSave) ImGui::BeginDisabled();
            if (ImGui::Button("Save", ImVec2(80, 0))) {
                try {
                    const Mc3::Mc3Document& srcDoc =
                        (regSaveFromAi_ && aiPendingDoc_.has_value())
                        ? *aiPendingDoc_ : document_;
                    auto entry = registry_.entryFromDefinition(
                        srcDoc, regSaveDefId_,
                        regSaveGroupBuf_, regSaveNameBuf_,
                        regSaveVariantBuf_, regSaveTagsBuf_,
                        regSaveDescBuf_, regSaveSourceBuf_);
                    int64_t savedId = registry_.save(entry);
                    if (savedId < 0) {
                        setStatusMsg("Registry save failed for '" +
                                     std::string(regSaveNameBuf_) + "'", true);
                    } else {
                        regResultsDirty_ = true;
                        regSaveDlgOpen_  = false;
                        regSaveFromAi_   = false;
                        setStatusMsg("Saved '" + std::string(regSaveNameBuf_) + "' to registry");
                    }
                } catch (const std::exception& ex) {
                    setStatusMsg(std::string("Save failed: ") + ex.what(), true);
                }
            }
            if (!canSave) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                regSaveDlgOpen_ = false;
                regSaveFromAi_  = false;
            }
        }
        ImGui::End();
    }

    // The pack deliberately remains an explicit local export. It includes
    // only entries currently matched by the filters plus their already
    // resolved library files; no credentials, upload, or cloud state is
    // involved in this workflow.
    if (regPackDlgOpen_) {
        ImGui::SetNextWindowSize(ImVec2(560, 180), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Export Registry Asset Pack##regpack", &regPackDlgOpen_,
                         ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("Creates an empty local directory containing a manifest, MC3 entry files, "
                               "catalog previews and the resolved library dependencies they use.");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##regpackpath", "New or empty destination directory",
                                     regPackPathBuf_, sizeof(regPackPathBuf_));
            if (!regPackErr_.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "%s", regPackErr_.c_str());
            const bool canExport = regPackPathBuf_[0] != '\0' && !regCachedResults_.empty();
            if (!canExport) ImGui::BeginDisabled();
            if (ImGui::Button("Export", ImVec2(90, 0))) {
                try {
                    std::vector<ModelRegistry::AssetPackDependency> dependencies;
                    dependencies.reserve(importHealth_.size());
                    for (const auto& health : importHealth_) {
                        dependencies.push_back({health.importNamespace, health.source,
                                                health.contentHash, health.resolvedPath});
                    }
                    const auto result = ModelRegistry::exportAssetPack(
                        regPackPathBuf_, regCachedResults_, dependencies);
                    regPackDlgOpen_ = false;
                    setStatusMsg("Exported asset pack with " + std::to_string(result.entryCount) +
                                 " asset(s) and " + std::to_string(result.dependencyCount) + " library file(s)");
                } catch (const std::exception& ex) {
                    regPackErr_ = ex.what();
                }
            }
            if (!canExport) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(90, 0))) regPackDlgOpen_ = false;
        }
        ImGui::End();
    }

    ImGui::End();
}

} // namespace MeshCraft::Application

namespace MeshCraft::Application::UI {

void Registry::drawResults(RegistryResultsContext& context) {
    const float available = ImGui::GetContentRegionAvail().y - 36.0f;
    const float tableH = available > 96.0f ? available : 96.0f;
    if (!ImGui::BeginTable("##regtable", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, tableH))) return;

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
