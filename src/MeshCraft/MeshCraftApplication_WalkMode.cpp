#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>

#include <cstdio>

#include <imgui.h>

namespace MeshCraft {

using namespace Microsoft::Xna::Framework::Input;

// SYS-W3-01 Phase 6: the movement/look physics and the view-matrix
// computation now live in the self-contained, CNA-coupled-where-
// unavoidable Editor::WalkController (self-contained like Preferences/
// KeybindingManager -- no callback DI needed, unlike MacroRecorder).
// These three methods are now thin wrappers translating walkController_'s
// results into Editor::EditorCamera state, plus the HUD drawing (which
// stays here as UI glue, matching the PropertiesPanel precedent of UI
// files calling into an extracted class rather than being extracted
// themselves).

void MeshCraftApplication::enterWalkMode() {
    walkController_.enter(camera_.position(), camera_.yaw);
    setStatusMsg("Walk mode — Esc to exit", false, 3.0f);
}

void MeshCraftApplication::exitWalkMode() {
    auto s = walkController_.exit();
    camera_.target   = s.target;
    camera_.yaw      = s.yaw;
    camera_.pitch    = s.pitch;
    camera_.distance = s.distance;
    setStatusMsg("Walk mode exited", false, 1.5f);
}

void MeshCraftApplication::updateWalkMode(float dt,
                                           const KeyboardState& ks,
                                           int mouseDx, int mouseDy)
{
    if (auto exitState = walkController_.update(dt, ks, mouseDx, mouseDy)) {
        camera_.target   = exitState->target;
        camera_.yaw      = exitState->yaw;
        camera_.pitch    = exitState->pitch;
        camera_.distance = exitState->distance;
        setStatusMsg("Walk mode exited", false, 1.5f);
    }
}

void MeshCraftApplication::drawWalkModeHud(int screenW, int screenH) {
    // Crosshair at viewport centre
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float cx = static_cast<float>(screenW) * 0.5f;
    float cy = static_cast<float>(screenH) * 0.5f;
    const float arm = 10.0f;
    const ImU32 col = IM_COL32(255, 255, 255, 200);
    dl->AddLine(ImVec2(cx - arm, cy), ImVec2(cx + arm, cy), col, 1.5f);
    dl->AddLine(ImVec2(cx, cy - arm), ImVec2(cx, cy + arm), col, 1.5f);

    // Top-left info banner
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(kLeftPanelW) + 8, static_cast<float>(imguiTopH_) + 8),
                            ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##walkbanner", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "WALK MODE");
    ImGui::Text("W/S  Forward/Back   |  A/D or Arrows  Turn");
    ImGui::Text("PgUp/PgDn or Mouse  Look   |  Ctrl  Jump   |  Esc  Exit");
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Pos: (%.1f, %.1f, %.1f)  Height: %.2f m",
                  walkController_.posX(), walkController_.posY(), walkController_.posZ(),
                  walkController_.height);
    ImGui::TextDisabled("%s", buf);
    ImGui::End();

    // Settings popup (click to open)
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(screenW) - static_cast<float>(kRightPanelW) - 160.0f,
                                   static_cast<float>(imguiTopH_) + 8),
                            ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.70f);
    ImGui::Begin("##walksettings", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Person height (m)");
    ImGui::SetNextItemWidth(140);
    // AlwaysClamp (AUDIT-0047): without it, Ctrl+Click text entry can set
    // these outside their slider bounds (incl. zero/negative), which would
    // break walk-mode movement/camera math with no other downstream guard.
    ImGui::SliderFloat("##wh", &walkController_.height, 0.5f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Text("Speed (m/s)");
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("##ws", &walkController_.speed, 1.0f, 20.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Text("Mouse sensitivity");
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("##wm", &walkController_.mouseSens, 0.001f, 0.010f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::Button("Exit Walk Mode (Esc)"))
        exitWalkMode();
    ImGui::End();
}

} // namespace MeshCraft
