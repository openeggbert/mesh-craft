#include "MeshCraft/Editor/Preferences.hpp"

#include <imgui.h>

namespace MeshCraft::Editor {

void Preferences::applyTheme() const {
    switch (theme_) {
    case 1:  ImGui::StyleColorsLight();   break;
    case 2:  ImGui::StyleColorsClassic(); break;
    default: ImGui::StyleColorsDark();    break;
    }
}

} // namespace MeshCraft::Editor
