#include "MeshCraft/Application/RegistryWorkspace.hpp"
#include "MeshCraft/Application/UI/Registry.hpp"

#include <imgui.h>

#include <exception>
#include <string>
#include <vector>

namespace MeshCraft::Application {

void RegistryWorkspace::draw(const Frame& frame)
{
    if (!panelOpen_) return;
    if (!ensureOpen(frame.reportStatus)) {
        panelOpen_ = false;
        return;
    }

    if (resultsDirty_) {
        cachedResults_ = registry_.search(ModelRegistry::SearchFilter{
            .text = searchBuffer_,
            .tag = tagFilterBuffer_,
            .category = categoryFilterBuffer_,
            .license = licenseFilterBuffer_,
            .provenance = provenanceFilterBuffer_,
        });
        resultsDirty_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Model Registry", &panelOpen_)) {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(-60.0f);
    if (ImGui::InputTextWithHint("##regsearch",
                                 "Search name, group, metadata, tags, description or source…",
                                 searchBuffer_, sizeof(searchBuffer_)))
        resultsDirty_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        searchBuffer_[0] = '\0';
        tagFilterBuffer_[0] = '\0';
        categoryFilterBuffer_[0] = '\0';
        licenseFilterBuffer_[0] = '\0';
        provenanceFilterBuffer_[0] = '\0';
        resultsDirty_ = true;
    }

    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputTextWithHint("##regtag", "Tag filter", tagFilterBuffer_, sizeof(tagFilterBuffer_)))
        resultsDirty_ = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputTextWithHint("##regcategory", "Category", categoryFilterBuffer_, sizeof(categoryFilterBuffer_)))
        resultsDirty_ = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputTextWithHint("##reglicense", "License", licenseFilterBuffer_, sizeof(licenseFilterBuffer_)))
        resultsDirty_ = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##regprovenance", "Provenance", provenanceFilterBuffer_, sizeof(provenanceFilterBuffer_)))
        resultsDirty_ = true;

    ImGui::Spacing();
    const auto materialReport = ModelRegistry::inspectMaterials(frame.sceneDocument);
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
        .results = cachedResults_,
        .insert = frame.insertEntry,
        .remove = [this](int64_t id) { registry_.remove(id); resultsDirty_ = true; },
    };
    UI::Registry::drawResults(resultsContext);

    ImGui::Separator();
    if (ImGui::Button("Save Definition to Registry..."))
        saveDialogOpen_ = true;
    ImGui::SameLine();
    if (cachedResults_.empty()) ImGui::BeginDisabled();
    if (ImGui::Button("Export filtered asset pack...")) {
        assetPackDialogOpen_ = true;
        assetPackError_.clear();
    }
    if (cachedResults_.empty()) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu matching asset%s", cachedResults_.size(),
                        cachedResults_.size() == 1 ? "" : "s");

    if (saveDialogOpen_) {
        ImGui::SetNextWindowSize(ImVec2(380, 310), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Save Definition to Registry##regdlg",
                         &saveDialogOpen_, ImGuiWindowFlags_NoResize)) {
            const auto& definitions = (saveFromAi_ && frame.aiPendingDocument.has_value())
                ? frame.aiPendingDocument->definitions : frame.sceneDocument.definitions;
            const char* preview = saveDefinitionId_.empty() ? "(select)" : saveDefinitionId_.c_str();
            if (ImGui::BeginCombo("##regdef", preview)) {
                for (const auto& [id, _] : definitions) {
                    const bool selected = id == saveDefinitionId_;
                    if (ImGui::Selectable(id.c_str(), selected)) {
                        saveDefinitionId_ = id;
                        if (saveNameBuffer_[0] == '\0')
                            copyToBuffer(saveNameBuffer_, sizeof(saveNameBuffer_), id);
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            constexpr float labelWidth = 80.0f;
            ImGui::Text("Group:");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rg", saveGroupBuffer_, sizeof(saveGroupBuffer_));
            ImGui::Text("Name:");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rn", saveNameBuffer_, sizeof(saveNameBuffer_));
            ImGui::Text("Variant:");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rv", saveVariantBuffer_, sizeof(saveVariantBuffer_));
            ImGui::Text("Tags:");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rt", saveTagsBuffer_, sizeof(saveTagsBuffer_));
            ImGui::Text("Desc.:");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rdesc", saveDescriptionBuffer_, sizeof(saveDescriptionBuffer_));
            ImGui::Text("Source:");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##rsource", saveSourceBuffer_, sizeof(saveSourceBuffer_));

            ImGui::Spacing();
            const bool canSave = !saveDefinitionId_.empty() && saveNameBuffer_[0] != '\0';
            if (!canSave) ImGui::BeginDisabled();
            if (ImGui::Button("Save", ImVec2(80, 0))) {
                try {
                    const Mc3::Mc3Document& sourceDocument =
                        (saveFromAi_ && frame.aiPendingDocument.has_value())
                        ? *frame.aiPendingDocument : frame.sceneDocument;
                    const auto entry = registry_.entryFromDefinition(
                        sourceDocument, saveDefinitionId_, saveGroupBuffer_, saveNameBuffer_,
                        saveVariantBuffer_, saveTagsBuffer_, saveDescriptionBuffer_, saveSourceBuffer_);
                    const int64_t savedId = registry_.save(entry);
                    if (savedId < 0) {
                        frame.reportStatus("Registry save failed for '" + std::string(saveNameBuffer_) + "'", true);
                    } else {
                        resultsDirty_ = true;
                        saveDialogOpen_ = false;
                        saveFromAi_ = false;
                        frame.reportStatus("Saved '" + std::string(saveNameBuffer_) + "' to registry", false);
                    }
                } catch (const std::exception& ex) {
                    frame.reportStatus(std::string("Save failed: ") + ex.what(), true);
                }
            }
            if (!canSave) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                saveDialogOpen_ = false;
                saveFromAi_ = false;
            }
        }
        ImGui::End();
    }

    if (assetPackDialogOpen_) {
        ImGui::SetNextWindowSize(ImVec2(560, 180), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Export Registry Asset Pack##regpack", &assetPackDialogOpen_,
                         ImGuiWindowFlags_NoResize)) {
            ImGui::TextWrapped("Creates an empty local directory containing a manifest, MC3 entry files, "
                               "catalog previews and the resolved library dependencies they use.");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##regpackpath", "New or empty destination directory",
                                     assetPackPathBuffer_, sizeof(assetPackPathBuffer_));
            if (!assetPackError_.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "%s", assetPackError_.c_str());
            const bool canExport = assetPackPathBuffer_[0] != '\0' && !cachedResults_.empty();
            if (!canExport) ImGui::BeginDisabled();
            if (ImGui::Button("Export", ImVec2(90, 0))) {
                try {
                    const std::vector<ModelRegistry::AssetPackDependency> dependencies(
                        frame.assetPackDependencies.begin(), frame.assetPackDependencies.end());
                    const auto result = ModelRegistry::exportAssetPack(
                        assetPackPathBuffer_, cachedResults_, dependencies);
                    assetPackDialogOpen_ = false;
                    frame.reportStatus("Exported asset pack with " + std::to_string(result.entryCount) +
                                       " asset(s) and " + std::to_string(result.dependencyCount) + " library file(s)", false);
                } catch (const std::exception& ex) {
                    assetPackError_ = ex.what();
                }
            }
            if (!canExport) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(90, 0))) assetPackDialogOpen_ = false;
        }
        ImGui::End();
    }

    ImGui::End();
}

} // namespace MeshCraft::Application
