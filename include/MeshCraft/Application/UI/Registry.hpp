#pragma once

#include "MeshCraft/ModelRegistry.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace MeshCraft::Application::UI {

struct RegistryResultsContext {
    std::vector<ModelRegistry::Entry>& results;
    std::function<void(const ModelRegistry::Entry&)> insert;
    std::function<void(int64_t)> remove;
};

class Registry final {
public:
    static void drawResults(RegistryResultsContext& context);
};

} // namespace MeshCraft::Application::UI
