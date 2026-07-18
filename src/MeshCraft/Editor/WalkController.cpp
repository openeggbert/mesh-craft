#include "MeshCraft/Editor/WalkController.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>

#include <algorithm>
#include <cmath>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;

namespace MeshCraft::Editor {

namespace {
constexpr float kGravity   = -9.81f;
constexpr float kJumpSpeed =  5.0f;
constexpr float kPitchMax  =  1.48f; // ~85 degrees
} // namespace

void WalkController::enter(Vector3 cameraPos, float cameraYaw) {
    posX_ = cameraPos.X;
    posY_ = std::max(0.0f, cameraPos.Y - height);
    posZ_ = cameraPos.Z;
    yaw_       = cameraYaw;
    pitch_     = 0.0f;
    velY_      = 0.0f;
    onGround_  = (posY_ <= 0.0f);
    active_    = true;
}

WalkController::ExitCameraState WalkController::exit() {
    active_ = false;
    ExitCameraState s;
    s.target   = Vector3(posX_, posY_ + height * 0.5f, posZ_);
    s.yaw      = yaw_;
    s.pitch    = -pitch_;
    s.distance = 5.0f;
    return s;
}

std::optional<WalkController::ExitCameraState> WalkController::update(
    float dt, const KeyboardState& ks, int mouseDx, int mouseDy)
{
    if (ks.IsKeyDown(Keys::Escape))
        return exit();

    // Look: mouse -> yaw/pitch
    yaw_   += static_cast<float>(mouseDx) * mouseSens;
    pitch_ -= static_cast<float>(mouseDy) * mouseSens;
    pitch_  = std::clamp(pitch_, -kPitchMax, kPitchMax);

    // Look: PageUp/PageDown -> pitch
    if (ks.IsKeyDown(Keys::PageUp))
        pitch_ = std::min(pitch_ + turnSpeed * dt, kPitchMax);
    if (ks.IsKeyDown(Keys::PageDown))
        pitch_ = std::max(pitch_ - turnSpeed * dt, -kPitchMax);

    // Yaw: Left/Right arrows or A/D
    bool turnLeft  = ks.IsKeyDown(Keys::Left)  || ks.IsKeyDown(Keys::A);
    bool turnRight = ks.IsKeyDown(Keys::Right) || ks.IsKeyDown(Keys::D);
    if (turnLeft)  yaw_ -= turnSpeed * dt;
    if (turnRight) yaw_ += turnSpeed * dt;

    // Movement: W/S or Up/Down -- horizontal only, ignore pitch for movement
    bool fwd  = ks.IsKeyDown(Keys::W) || ks.IsKeyDown(Keys::Up);
    bool back = ks.IsKeyDown(Keys::S) || ks.IsKeyDown(Keys::Down);

    float sinY = std::sin(yaw_);
    float cosY = std::cos(yaw_);

    if (fwd) {
        posX_ += sinY * speed * dt;
        posZ_ -= cosY * speed * dt;
    }
    if (back) {
        posX_ -= sinY * speed * dt;
        posZ_ += cosY * speed * dt;
    }

    // Jump: Ctrl (only when on ground)
    bool ctrl = ks.IsKeyDown(Keys::LeftControl) || ks.IsKeyDown(Keys::RightControl);
    if (ctrl && onGround_) {
        velY_     = kJumpSpeed;
        onGround_ = false;
    }

    // Gravity + ground collision
    velY_  += kGravity * dt;
    posY_  += velY_ * dt;

    if (posY_ <= 0.0f) {
        posY_     = 0.0f;
        velY_     = 0.0f;
        onGround_ = true;
    }

    return std::nullopt;
}

Matrix WalkController::viewMatrix() const {
    float cosP = std::cos(pitch_), sinP = std::sin(pitch_);
    float sinY = std::sin(yaw_),   cosY = std::cos(yaw_);
    float eyeX = posX_, eyeY = posY_ + height, eyeZ = posZ_;
    Vector3 eye(eyeX, eyeY, eyeZ);
    Vector3 target(eyeX + sinY * cosP, eyeY + sinP, eyeZ - cosY * cosP);
    Vector3 up(0.0f, 1.0f, 0.0f);
    return Matrix::CreateLookAt(eye, target, up);
}

} // namespace MeshCraft::Editor
