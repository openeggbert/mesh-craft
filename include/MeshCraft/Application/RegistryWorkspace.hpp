#pragma once

#include "MeshCraft/ModelRegistry.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace MeshCraft::Application {

// Owns the Model Registry's database and transient workflow state. The editor
// deliberately supplies document insertion and notifications as narrow
// callbacks: this subsystem never reaches back into MeshCraftApplication.
class RegistryWorkspace final {
public:
    using StatusCallback = std::function<void(std::string, bool)>;

    struct Frame {
        const Mc3::Mc3Document& sceneDocument;
        const std::optional<Mc3::Mc3Document>& aiPendingDocument;
        std::span<const ModelRegistry::AssetPackDependency> assetPackDependencies;
        std::function<void(const ModelRegistry::Entry&)> insertEntry;
        StatusCallback reportStatus;
    };

    RegistryWorkspace();
    ~RegistryWorkspace();

    RegistryWorkspace(const RegistryWorkspace&) = delete;
    RegistryWorkspace& operator=(const RegistryWorkspace&) = delete;

    [[nodiscard]] bool panelOpen() const { return panelOpen_; }
    bool& panelOpen() { return panelOpen_; }
    void openPanel() { panelOpen_ = true; }

    // Opens the local database on demand. Failures are reported through the
    // caller-provided UI callback; absence of SQLite is a normal false result.
    bool ensureOpen(const StatusCallback& reportStatus);

    // Prepares the regular registry save dialog from an already parsed AI
    // document. Kept separately testable from database availability.
    void prepareAiSave(const Mc3::Mc3Document& aiDocument);
    bool beginAiSave(const Mc3::Mc3Document& aiDocument, const StatusCallback& reportStatus);
    void dismissAiSave();

    // The workflow owns registry-entry deserialization, while the caller owns
    // the undo boundary and placement of the resulting scene instance.
    std::string insertDefinition(Mc3::Mc3Document& sceneDocument,
                                 const ModelRegistry::Entry& entry) const;

    [[nodiscard]] bool saveDialogOpen() const { return saveDialogOpen_; }
    [[nodiscard]] bool savingFromAi() const { return saveFromAi_; }
    [[nodiscard]] std::string_view saveDefinitionId() const { return saveDefinitionId_; }

    // Used by --benchmark without exposing ModelRegistry ownership to the
    // application. Retains its historical failure semantics.
    void benchmarkOpenAndSearch();

    // ImGui implementation lives in RegistryWorkspaceUi.cpp. `Frame` makes
    // its application dependencies explicit and short-lived.
    void draw(const Frame& frame);

private:
    static void copyToBuffer(char* destination, std::size_t size, std::string_view source);

    ModelRegistry registry_;
    bool panelOpen_{false};
    char searchBuffer_[128]{};
    char tagFilterBuffer_[96]{};
    char categoryFilterBuffer_[96]{};
    char licenseFilterBuffer_[96]{};
    char provenanceFilterBuffer_[128]{};
    bool saveDialogOpen_{false};
    char saveGroupBuffer_[64]{};
    char saveNameBuffer_[64]{};
    char saveVariantBuffer_[64]{};
    char saveTagsBuffer_[128]{};
    char saveDescriptionBuffer_[256]{};
    char saveSourceBuffer_[64]{"handmade"};
    std::string saveDefinitionId_;
    bool saveFromAi_{false};
    std::vector<ModelRegistry::Entry> cachedResults_;
    bool resultsDirty_{true};
    bool assetPackDialogOpen_{false};
    char assetPackPathBuffer_[512]{};
    std::string assetPackError_;
};

} // namespace MeshCraft::Application
