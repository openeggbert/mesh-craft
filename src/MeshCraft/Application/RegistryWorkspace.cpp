#include "MeshCraft/Application/RegistryWorkspace.hpp"

#include <cstdio>
#include <exception>

namespace MeshCraft::Application {

RegistryWorkspace::RegistryWorkspace() = default;
RegistryWorkspace::~RegistryWorkspace() = default;

void RegistryWorkspace::copyToBuffer(char* destination, std::size_t size, std::string_view source)
{
    if (size == 0) return;
    std::snprintf(destination, size, "%.*s", static_cast<int>(source.size()), source.data());
}

bool RegistryWorkspace::ensureOpen(const StatusCallback& reportStatus)
{
    if (registry_.isOpen()) return true;
    try {
        registry_.open(ModelRegistry::defaultPath());
    } catch (const std::exception& ex) {
        reportStatus(std::string("Registry: ") + ex.what(), true);
        return false;
    }
    if (!registry_.isOpen()) {
        reportStatus("Model Registry is not available in this build", true);
        return false;
    }
    resultsDirty_ = true;
    return true;
}

void RegistryWorkspace::prepareAiSave(const Mc3::Mc3Document& aiDocument)
{
    if (aiDocument.definitions.empty()) return;

    panelOpen_ = true;
    saveDialogOpen_ = true;
    const std::string& definitionId = aiDocument.definitions.begin()->first;
    copyToBuffer(saveNameBuffer_, sizeof(saveNameBuffer_), definitionId);
    copyToBuffer(saveGroupBuffer_, sizeof(saveGroupBuffer_), "AI");
    copyToBuffer(saveSourceBuffer_, sizeof(saveSourceBuffer_), "ai_generated");
    saveDescriptionBuffer_[0] = '\0';
    saveVariantBuffer_[0] = '\0';
    saveTagsBuffer_[0] = '\0';
    saveDefinitionId_ = definitionId;
    saveFromAi_ = true;
}

bool RegistryWorkspace::beginAiSave(const Mc3::Mc3Document& aiDocument,
                                    const StatusCallback& reportStatus)
{
    if (aiDocument.definitions.empty() || !ensureOpen(reportStatus)) return false;
    prepareAiSave(aiDocument);
    return true;
}

void RegistryWorkspace::dismissAiSave()
{
    if (!saveFromAi_) return;
    saveDialogOpen_ = false;
    saveFromAi_ = false;
}

std::string RegistryWorkspace::insertDefinition(Mc3::Mc3Document& sceneDocument,
                                                const ModelRegistry::Entry& entry) const
{
    return registry_.insertIntoScene(sceneDocument, entry);
}

void RegistryWorkspace::benchmarkOpenAndSearch()
{
    if (!registry_.isOpen()) registry_.open(ModelRegistry::defaultPath());
    (void)registry_.search("");
}

} // namespace MeshCraft::Application
