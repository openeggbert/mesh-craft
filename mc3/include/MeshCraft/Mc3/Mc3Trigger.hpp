#pragma once
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

enum class TriggerStepType { PlayAction, PlaySound, RunScript, PlayMusic };

struct Mc3TriggerStep {
    TriggerStepType type{TriggerStepType::PlayAction};
    std::string ref;
};

struct Mc3Trigger {
    std::string id;
    std::vector<Mc3TriggerStep> steps;
};

} // namespace MeshCraft::Mc3
