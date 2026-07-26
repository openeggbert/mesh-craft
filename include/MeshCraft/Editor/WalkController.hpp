#pragma once

#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>

#include <cstddef>
#include <optional>
#include <span>

namespace MeshCraft::Editor {

enum class WalkColliderType {
    Box,
    Sphere,
    Capsule,
};

// World-space solid used by walk mode. Box is axis-aligned; sphere and
// capsule are deliberately limited to round shapes that retain their exact
// world-space semantics. For capsules, axisMinY/axisMaxY describe the two
// centres of the hemispherical end caps (the capsule's central vertical axis).
struct WalkCollider {
    float minX{0.0f}, minY{0.0f}, minZ{0.0f};
    float maxX{0.0f}, maxY{0.0f}, maxZ{0.0f};

    WalkColliderType type{WalkColliderType::Box};
    float centerX{0.0f}, centerZ{0.0f};
    float axisMinY{0.0f}, axisMaxY{0.0f};
    float radius{0.0f};

    [[nodiscard]] static WalkCollider sphere(float x, float y, float z, float r) {
        WalkCollider result;
        result.type = WalkColliderType::Sphere;
        result.centerX = x;
        result.centerZ = z;
        result.axisMinY = result.axisMaxY = y;
        result.radius = r;
        return result;
    }

    [[nodiscard]] static WalkCollider capsule(float x, float minAxisY, float maxAxisY,
                                               float z, float r) {
        WalkCollider result;
        result.type = WalkColliderType::Capsule;
        result.centerX = x;
        result.centerZ = z;
        result.axisMinY = minAxisY;
        result.axisMaxY = maxAxisY;
        result.radius = r;
        return result;
    }
};

// SYS-W3-01 Phase 6: first-person "walk mode" extracted out of
// MeshCraftApplication (H-series). Self-contained physics/camera state,
// like KeybindingManager/Preferences/UndoManager -- no callback DI Context
// needed, unlike MacroRecorder. update() genuinely needs CNA's
// KeyboardState, matching KeybindingManager's own precedent of being
// CNA-coupled where the subsystem unavoidably is.
//
// Tunable settings (height/speed/turnSpeed/mouseSens) are public fields,
// not getter/setter pairs, mirroring EditorCamera's own yaw/pitch/
// distance/fovDegrees -- the walk-settings ImGui sliders bind directly to
// these, same as the camera's own widgets bind to EditorCamera's fields.
class WalkController {
public:
    // The application reports and omits excess authored proxies when entering
    // walk mode. Retaining this defensive controller-side cap keeps direct
    // callers deterministic as well.
    static constexpr std::size_t maxCollisionProxies{256};

    float height{1.8f};       // eye height above ground (meters)
    float speed{5.0f};        // movement speed (m/s)
    float turnSpeed{1.5f};    // keyboard yaw speed (rad/s)
    float mouseSens{0.003f};  // mouse sensitivity (rad/px)
    float collisionRadius{0.30f}; // horizontal player-cylinder radius (m)

    [[nodiscard]] bool  isActive() const { return active_; }
    [[nodiscard]] float posX() const { return posX_; }
    [[nodiscard]] float posY() const { return posY_; }
    [[nodiscard]] float posZ() const { return posZ_; }
    [[nodiscard]] float yaw() const { return yaw_; }
    [[nodiscard]] float pitch() const { return pitch_; }

    // The (target, yaw, pitch, distance) an Editor::EditorCamera should be
    // restored to after walk mode ends -- mirrors the pre-extraction
    // exitWalkMode()'s "no jarring jump" restoration math exactly (note
    // the pitch sign flip: orbit-camera pitch = -walk pitch).
    struct ExitCameraState {
        Microsoft::Xna::Framework::Vector3 target;
        float yaw{0.0f};
        float pitch{0.0f};
        float distance{5.0f};
    };

    // Enters walk mode, placing the walk camera at the given (orbit)
    // camera position/yaw, ground-relative (mirrors enterWalkMode()'s
    // pre-extraction seeding exactly).
    void enter(Microsoft::Xna::Framework::Vector3 cameraPos, float cameraYaw);

    // Explicitly leaves walk mode (e.g. a menu toggle), returning the
    // camera state to restore to. isActive() is false afterward.
    ExitCameraState exit();

    // Advances the movement/look simulation by dt given the current
    // keyboard state and raw mouse delta since last frame. If Escape is
    // held, exits walk mode internally (equivalent to calling exit())
    // and returns its result instead of advancing; otherwise returns
    // std::nullopt and isActive() stays true.
    std::optional<ExitCameraState> update(
        float dt,
        const Microsoft::Xna::Framework::Input::KeyboardState& ks,
        int mouseDx, int mouseDy,
        std::span<const WalkCollider> colliders = {});

    // First-person view matrix from the current position/yaw/pitch/height.
    // The projection matrix is deliberately NOT owned here -- walk mode
    // reuses the orbit camera's own fovDegrees/nearPlane/farPlane
    // unchanged (MeshCraftApplication::Draw()'s own long-standing
    // behavior), so there is nothing walk-mode-specific to compute there.
    [[nodiscard]] Microsoft::Xna::Framework::Matrix viewMatrix() const;

private:
    bool  active_{false};
    float posX_{0.0f}, posY_{0.0f}, posZ_{5.0f};
    float yaw_{0.0f};    // radians, horizontal look
    float pitch_{0.0f};  // radians, vertical look (clamped +-85 deg)
    float velY_{0.0f};   // vertical velocity (gravity / jump)
    bool  onGround_{true};
};

} // namespace MeshCraft::Editor
