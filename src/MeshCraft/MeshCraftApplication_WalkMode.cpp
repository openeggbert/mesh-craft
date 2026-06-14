#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraftPrivate.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

#include <imgui.h>

namespace MeshCraft {

using namespace Microsoft::Xna::Framework::Input;

static constexpr float kGravity   = -9.81f;
static constexpr float kJumpSpeed =  5.0f;
static constexpr float kPitchMax  = 1.48f; // ~85 degrees

void MeshCraftApplication::enterWalkMode() {
    // Place camera at current editor camera position (at ground level)
    auto pos = camera_.position();
    walkPosX_      = pos.X;
    walkPosY_      = std::max(0.0f, pos.Y - walkHeight_);
    walkPosZ_      = pos.Z;
    walkYaw_       = camera_.yaw;
    walkPitch_     = 0.0f;
    walkVelY_      = 0.0f;
    walkOnGround_  = (walkPosY_ <= 0.0f);
    walkModeEnabled_ = true;
    setStatusMsg("Walk mode — Esc to exit", false, 3.0f);
}

void MeshCraftApplication::exitWalkMode() {
    walkModeEnabled_ = false;
    // Restore editor camera to the walk position so there's no jarring jump
    camera_.target   = {walkPosX_, walkPosY_ + walkHeight_ * 0.5f, walkPosZ_};
    camera_.yaw      = walkYaw_;
    camera_.pitch    = -walkPitch_;
    camera_.distance = 5.0f;
    setStatusMsg("Walk mode exited", false, 1.5f);
}

void MeshCraftApplication::updateWalkMode(float dt,
                                           const KeyboardState& ks,
                                           int mouseDx, int mouseDy)
{
    const float pi = std::numbers::pi_v<float>;

    // Escape: exit walk mode
    if (ks.IsKeyDown(Keys::Escape)) {
        exitWalkMode();
        return;
    }

    // -----------------------------------------------------------------------
    // Look: mouse → yaw/pitch
    // -----------------------------------------------------------------------
    walkYaw_   += static_cast<float>(mouseDx) * walkMouseSens_;
    walkPitch_ -= static_cast<float>(mouseDy) * walkMouseSens_;
    walkPitch_  = std::clamp(walkPitch_, -kPitchMax, kPitchMax);

    // Look: PageUp/PageDown → pitch
    if (ks.IsKeyDown(Keys::PageUp))
        walkPitch_ = std::min(walkPitch_ + walkTurnSpeed_ * dt, kPitchMax);
    if (ks.IsKeyDown(Keys::PageDown))
        walkPitch_ = std::max(walkPitch_ - walkTurnSpeed_ * dt, -kPitchMax);

    // -----------------------------------------------------------------------
    // Yaw: Left/Right arrows or A/D
    // -----------------------------------------------------------------------
    bool turnLeft  = ks.IsKeyDown(Keys::Left)  || ks.IsKeyDown(Keys::A);
    bool turnRight = ks.IsKeyDown(Keys::Right) || ks.IsKeyDown(Keys::D);
    if (turnLeft)  walkYaw_ -= walkTurnSpeed_ * dt;
    if (turnRight) walkYaw_ += walkTurnSpeed_ * dt;

    // -----------------------------------------------------------------------
    // Movement: W/S or Up/Down — horizontal only, ignore pitch for movement
    // -----------------------------------------------------------------------
    bool fwd  = ks.IsKeyDown(Keys::W) || ks.IsKeyDown(Keys::Up);
    bool back = ks.IsKeyDown(Keys::S) || ks.IsKeyDown(Keys::Down);

    float sinY = std::sin(walkYaw_);
    float cosY = std::cos(walkYaw_);

    if (fwd) {
        walkPosX_ += sinY * walkSpeed_ * dt;
        walkPosZ_ -= cosY * walkSpeed_ * dt;
    }
    if (back) {
        walkPosX_ -= sinY * walkSpeed_ * dt;
        walkPosZ_ += cosY * walkSpeed_ * dt;
    }

    // -----------------------------------------------------------------------
    // Jump: Ctrl (only when on ground)
    // -----------------------------------------------------------------------
    bool ctrl = ks.IsKeyDown(Keys::LeftControl) || ks.IsKeyDown(Keys::RightControl);
    if (ctrl && walkOnGround_) {
        walkVelY_     = kJumpSpeed;
        walkOnGround_ = false;
    }

    // -----------------------------------------------------------------------
    // Gravity + ground collision
    // -----------------------------------------------------------------------
    walkVelY_ += kGravity * dt;
    walkPosY_ += walkVelY_ * dt;

    if (walkPosY_ <= 0.0f) {
        walkPosY_     = 0.0f;
        walkVelY_     = 0.0f;
        walkOnGround_ = true;
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
                  walkPosX_, walkPosY_, walkPosZ_, walkHeight_);
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
    ImGui::SliderFloat("##wh", &walkHeight_, 0.5f, 3.0f, "%.2f");
    ImGui::Text("Speed (m/s)");
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("##ws", &walkSpeed_, 1.0f, 20.0f, "%.1f");
    ImGui::Text("Mouse sensitivity");
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("##wm", &walkMouseSens_, 0.001f, 0.010f, "%.3f");
    if (ImGui::Button("Exit Walk Mode (Esc)"))
        exitWalkMode();
    ImGui::End();
}

} // namespace MeshCraft
