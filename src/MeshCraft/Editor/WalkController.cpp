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
constexpr float kCollisionEpsilon = 1.0e-4f;

bool verticallyOverlaps(const WalkCollider& c, float feetY, float height) {
    return feetY < c.maxY - kCollisionEpsilon &&
           feetY + height > c.minY + kCollisionEpsilon;
}

bool horizontallyOverlaps(const WalkCollider& c, float x, float z, float radius) {
    return x >= c.minX - radius && x <= c.maxX + radius &&
           z >= c.minZ - radius && z <= c.maxZ + radius;
}

// Raycast the player's horizontal centre through an AABB expanded by the
// player radius. This is a swept test, so a long frame cannot tunnel through
// a thin wall. normalX/normalZ identify the face that blocks the motion.
bool sweepExpandedAabb(const WalkCollider& c, float x, float z, float dx, float dz,
                       float radius, float& hitT, float& normalX, float& normalZ) {
    const float minX = c.minX - radius, maxX = c.maxX + radius;
    const float minZ = c.minZ - radius, maxZ = c.maxZ + radius;
    float entryX, exitX, entryZ, exitZ;
    if (std::abs(dx) < kCollisionEpsilon) {
        if (x < minX || x > maxX) return false;
        entryX = -INFINITY; exitX = INFINITY;
    } else if (dx > 0.0f) {
        entryX = (minX - x) / dx; exitX = (maxX - x) / dx;
    } else {
        entryX = (maxX - x) / dx; exitX = (minX - x) / dx;
    }
    if (std::abs(dz) < kCollisionEpsilon) {
        if (z < minZ || z > maxZ) return false;
        entryZ = -INFINITY; exitZ = INFINITY;
    } else if (dz > 0.0f) {
        entryZ = (minZ - z) / dz; exitZ = (maxZ - z) / dz;
    } else {
        entryZ = (maxZ - z) / dz; exitZ = (minZ - z) / dz;
    }

    const float entry = std::max(entryX, entryZ);
    const float exit = std::min(exitX, exitZ);
    if (entry > exit || exit < 0.0f || entry > 1.0f) return false;

    hitT = std::max(0.0f, entry);
    normalX = normalZ = 0.0f;
    if (entryX > entryZ) normalX = dx > 0.0f ? -1.0f : 1.0f;
    else                 normalZ = dz > 0.0f ? -1.0f : 1.0f;
    return true;
}

void pushOutOfOverlaps(float& x, float& z, float feetY, float height, float radius,
                       std::span<const WalkCollider> colliders) {
    // A walk can start inside a newly-enabled collider. Resolve that state
    // deterministically before the sweep, choosing the nearest expanded face.
    for (int pass = 0; pass < 4; ++pass) {
        bool moved = false;
        for (const auto& c : colliders) {
            if (!verticallyOverlaps(c, feetY, height) || !horizontallyOverlaps(c, x, z, radius)) continue;
            const float left = x - (c.minX - radius);
            const float right = (c.maxX + radius) - x;
            const float front = z - (c.minZ - radius);
            const float back = (c.maxZ + radius) - z;
            const float nearest = std::min({left, right, front, back});
            if (nearest == left)       x = c.minX - radius - kCollisionEpsilon;
            else if (nearest == right) x = c.maxX + radius + kCollisionEpsilon;
            else if (nearest == front) z = c.minZ - radius - kCollisionEpsilon;
            else                       z = c.maxZ + radius + kCollisionEpsilon;
            moved = true;
        }
        if (!moved) return;
    }
}

void moveHorizontally(float& x, float& z, float feetY, float height, float radius,
                      float dx, float dz, std::span<const WalkCollider> colliders) {
    pushOutOfOverlaps(x, z, feetY, height, radius, colliders);
    for (int pass = 0; pass < 4 && (std::abs(dx) > kCollisionEpsilon || std::abs(dz) > kCollisionEpsilon); ++pass) {
        float bestT = 1.0f, normalX = 0.0f, normalZ = 0.0f;
        bool blocked = false;
        for (const auto& c : colliders) {
            if (!verticallyOverlaps(c, feetY, height)) continue;
            float hitT, hitNormalX, hitNormalZ;
            if (sweepExpandedAabb(c, x, z, dx, dz, radius, hitT, hitNormalX, hitNormalZ) && hitT < bestT) {
                bestT = hitT;
                normalX = hitNormalX;
                normalZ = hitNormalZ;
                blocked = true;
            }
        }
        if (!blocked) { x += dx; z += dz; return; }

        const float safeT = std::max(0.0f, bestT - kCollisionEpsilon);
        x += dx * safeT;
        z += dz * safeT;
        const float remaining = 1.0f - bestT;
        dx *= remaining;
        dz *= remaining;
        // Remove the component into the blocking face, preserving the
        // tangential component so diagonal movement slides along walls.
        if (normalX != 0.0f) dx = 0.0f;
        if (normalZ != 0.0f) dz = 0.0f;
    }
}
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
    float dt, const KeyboardState& ks, int mouseDx, int mouseDy,
    std::span<const WalkCollider> colliders)
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

    float moveX = 0.0f, moveZ = 0.0f;
    if (fwd)  { moveX += sinY * speed * dt; moveZ -= cosY * speed * dt; }
    if (back) { moveX -= sinY * speed * dt; moveZ += cosY * speed * dt; }
    moveHorizontally(posX_, posZ_, posY_, height, collisionRadius, moveX, moveZ, colliders);

    // Jump: Ctrl (only when on ground)
    bool ctrl = ks.IsKeyDown(Keys::LeftControl) || ks.IsKeyDown(Keys::RightControl);
    if (ctrl && onGround_) {
        velY_     = kJumpSpeed;
        onGround_ = false;
    }

    // Gravity + y=0 ground collision + opt-in box-collider floors/ceilings.
    velY_  += kGravity * dt;
    const float oldY = posY_;
    float nextY = posY_ + velY_ * dt;
    onGround_ = false;
    if (velY_ <= 0.0f) {
        float landingY = 0.0f;
        for (const auto& c : colliders) {
            if (!horizontallyOverlaps(c, posX_, posZ_, collisionRadius)) continue;
            if (oldY >= c.maxY - kCollisionEpsilon && nextY <= c.maxY + kCollisionEpsilon)
                landingY = std::max(landingY, c.maxY);
        }
        if (nextY <= landingY) {
            posY_ = landingY;
            velY_ = 0.0f;
            onGround_ = true;
        } else {
            posY_ = nextY;
        }
    } else {
        float ceilingY = INFINITY;
        for (const auto& c : colliders) {
            if (!horizontallyOverlaps(c, posX_, posZ_, collisionRadius)) continue;
            if (oldY + height <= c.minY + kCollisionEpsilon && nextY + height >= c.minY - kCollisionEpsilon)
                ceilingY = std::min(ceilingY, c.minY);
        }
        if (ceilingY < INFINITY) {
            posY_ = ceilingY - height;
            velY_ = 0.0f;
        } else {
            posY_ = nextY;
        }
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
