#pragma once

#include <MeshCraft/Mc3/Mc3Validation.hpp>

#include <string>

namespace MeshCraft::Application::UI {

struct ValidationContext {
    bool& visible;
    const std::string& source;
    const Mc3::Mc3Validation& validation;
};

class Validation final {
public:
    static void draw(ValidationContext& context);
};

} // namespace MeshCraft::Application::UI
