#include "MeshCraft/Application/MeshCraftApplication.hpp"

#include <exception>
#include <span>
#include <string>
#include <vector>

namespace MeshCraft::Application {

void MeshCraftApplication::drawRegistryPanel()
{
    if (!registryWorkspace_.panelOpen()) return;

    std::vector<ModelRegistry::AssetPackDependency> dependencies;
    dependencies.reserve(importHealth_.size());
    for (const auto& health : importHealth_) {
        dependencies.push_back({health.importNamespace, health.source,
                                health.contentHash, health.resolvedPath});
    }

    registryWorkspace_.draw({
        .sceneDocument = document_,
        .aiPendingDocument = aiPendingDoc_,
        .assetPackDependencies = std::span<const ModelRegistry::AssetPackDependency>(dependencies),
        .insertEntry = [this](const ModelRegistry::Entry& entry) {
            try {
                pushUndo();
                std::string definitionId;
                try {
                    definitionId = registryWorkspace_.insertDefinition(document_, entry);
                } catch (...) {
                    undoManager_.popUndoWithoutApplying();
                    throw;
                }
                auto object = std::make_shared<Mc3::Mc3Object>();
                object->type = Mc3::ObjectType::Instance;
                object->definition = definitionId;
                object->name = entry.name + (entry.variant.empty() ? "" : "_" + entry.variant);
                const std::string base = "reg_" + definitionId;
                object->id = base;
                int suffix = 1;
                while (flatFindById(object->id))
                    object->id = base + "_" + std::to_string(suffix++);
                document_.objects.push_back(object);
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
        .reportStatus = [this](std::string message, bool error) {
            setStatusMsg(std::move(message), error);
        },
    });
}

} // namespace MeshCraft::Application
