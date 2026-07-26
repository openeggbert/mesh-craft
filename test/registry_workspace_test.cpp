#include "MeshCraft/Application/RegistryWorkspace.hpp"

#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char* description)
{
    if (condition) {
        std::cout << "PASS: " << description << '\n';
    } else {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    MeshCraft::Application::RegistryWorkspace workspace;
    expect(!workspace.panelOpen(), "workspace panel starts closed");
    expect(!workspace.saveDialogOpen(), "save dialog starts closed");

    MeshCraft::Mc3::Mc3Document aiDocument;
    aiDocument.definitions["zebra"] = std::make_shared<MeshCraft::Mc3::Mc3Object>();
    aiDocument.definitions["alpha"] = std::make_shared<MeshCraft::Mc3::Mc3Object>();
    workspace.prepareAiSave(aiDocument);

    expect(workspace.panelOpen(), "AI save opens registry panel");
    expect(workspace.saveDialogOpen(), "AI save opens save dialog");
    expect(workspace.savingFromAi(), "AI save remembers its source");
    expect(workspace.saveDefinitionId() == "alpha", "AI save deterministically selects first definition");

    workspace.dismissAiSave();
    expect(!workspace.saveDialogOpen(), "AI reset closes AI-owned save dialog");
    expect(!workspace.savingFromAi(), "AI reset clears AI-owned save source");

    workspace.openPanel();
    workspace.dismissAiSave();
    expect(workspace.panelOpen(), "dismiss does not close the registry panel");
    expect(!workspace.saveDialogOpen(), "dismiss leaves a non-AI dialog state unchanged");

    return failures == 0 ? 0 : 1;
}
