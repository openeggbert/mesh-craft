// Editor::WalkController test (SYS-W3-01 Phase 6).
//
// No test existed for walk mode's movement/look physics before this
// extraction. Covers: enter()/exit() camera-state round-trip (including
// the ground-relative position seeding and the pitch sign flip on exit),
// forward/backward movement following the current yaw, keyboard yaw
// turning, mouse look, pitch clamping at +-85 degrees, jump + gravity +
// ground collision settling back at y=0, opt-in box-collider wall sliding,
// platform landing and ceiling collision, Escape triggering an implicit
// exit from update() (matching the pre-extraction updateWalkMode()'s own
// early-return-on-Escape), and a differential check that viewMatrix()
// actually incorporates position/yaw/height (rather than hand-deriving
// CreateLookAt's exact matrix elements, matching preferences_test.cpp's
// own differential-check precedent for a third-party formula).

#include "MeshCraft/Editor/WalkController.hpp"

#include <Microsoft/Xna/Framework/Input/Keys.hpp>
#include <Microsoft/Xna/Framework/Input/KeyboardState.hpp>

#include <array>
#include <cmath>
#include <cstdio>

using namespace MeshCraft::Editor;
using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;

static int failures = 0;
static void check(bool cond, const char* msg) {
    if (cond) std::printf("PASS: %s\n", msg);
    else      { std::printf("FAIL: %s\n", msg); ++failures; }
}
static void checkNear(float a, float b, const char* msg, float eps = 1e-4f) {
    check(std::abs(a - b) < eps, msg);
}

static bool matricesDiffer(const Matrix& a, const Matrix& b) {
    return std::abs(a.M11 - b.M11) > 1e-6f || std::abs(a.M12 - b.M12) > 1e-6f ||
           std::abs(a.M13 - b.M13) > 1e-6f || std::abs(a.M41 - b.M41) > 1e-6f ||
           std::abs(a.M42 - b.M42) > 1e-6f || std::abs(a.M43 - b.M43) > 1e-6f;
}

int main() {
    // Initial state.
    {
        WalkController wc;
        check(!wc.isActive(), "starts inactive");
        checkNear(wc.height, 1.8f, "default eye height is 1.8m");
    }

    // enter(): ground-relative position seeding.
    {
        WalkController wc;
        wc.enter(Vector3(3.0f, 5.0f, -2.0f), 1.2f);
        check(wc.isActive(), "enter() activates walk mode");
        checkNear(wc.posX(), 3.0f, "enter() seeds posX from the camera position");
        checkNear(wc.posY(), 5.0f - wc.height, "enter() seeds posY as cameraY - height (ground-relative)");
        checkNear(wc.posZ(), -2.0f, "enter() seeds posZ from the camera position");
        checkNear(wc.yaw(), 1.2f, "enter() seeds yaw from the camera yaw");
        checkNear(wc.pitch(), 0.0f, "enter() resets pitch to 0 (level look)");
    }

    // enter(): a camera already at/below ground clamps posY to 0, not negative.
    {
        WalkController wc;
        wc.enter(Vector3(0.0f, 0.5f, 0.0f), 0.0f); // cameraY(0.5) - height(1.8) would be negative
        checkNear(wc.posY(), 0.0f, "enter() clamps a below-ground seed position to 0, not negative");
    }

    // exit(): camera-state restoration, including the pitch sign flip.
    {
        WalkController wc;
        wc.enter(Vector3(1.0f, 2.0f, 3.0f), 0.5f);
        // Simulate some pitch via update() (mouse look), then exit.
        KeyboardState noKeys{};
        wc.update(0.016f, noKeys, /*mouseDx=*/0, /*mouseDy=*/-100); // look up
        float pitchBeforeExit = wc.pitch();
        check(pitchBeforeExit > 0.0f, "setup: mouse look produced a positive pitch");

        auto exitState = wc.exit();
        check(!wc.isActive(), "exit() deactivates walk mode");
        checkNear(exitState.pitch, -pitchBeforeExit,
                  "exit() returns the negated pitch (orbit-camera convention vs. walk-camera convention)");
        checkNear(exitState.target.Y, wc.posY() + wc.height * 0.5f,
                  "exit() returns a target height halfway up from ground to full eye height");
        checkNear(exitState.distance, 5.0f, "exit() always restores a fixed 5.0 orbit distance");
    }

    // update(): forward/backward movement follows the current yaw.
    {
        WalkController wc;
        wc.enter(Vector3(0.0f, wc.height, 0.0f), 0.0f); // yaw=0 faces -Z
        KeyboardState fwd{Keys::W};
        KeyboardState noKeys{};
        wc.update(0.1f, fwd, 0, 0);
        checkNear(wc.posX(), 0.0f, "forward movement at yaw=0 does not change X");
        check(wc.posZ() < 0.0f, "forward movement at yaw=0 decreases Z (moves toward -Z)");

        WalkController wc2;
        wc2.enter(Vector3(0.0f, wc2.height, 0.0f), 0.0f);
        KeyboardState back{Keys::S};
        wc2.update(0.1f, back, 0, 0);
        check(wc2.posZ() > 0.0f, "backward movement at yaw=0 increases Z");
    }

    // update(): keyboard yaw turning (A/D and Left/Right).
    {
        WalkController wc;
        wc.enter(Vector3(0.0f, wc.height, 0.0f), 0.0f);
        KeyboardState turnRight{Keys::D};
        wc.update(0.1f, turnRight, 0, 0);
        check(wc.yaw() > 0.0f, "D turns yaw positive");

        WalkController wc2;
        wc2.enter(Vector3(0.0f, wc2.height, 0.0f), 0.0f);
        KeyboardState turnLeft{Keys::Left};
        wc2.update(0.1f, turnLeft, 0, 0);
        check(wc2.yaw() < 0.0f, "Left arrow turns yaw negative");
    }

    // update(): mouse look adjusts yaw/pitch proportional to mouseSens.
    {
        WalkController wc;
        wc.enter(Vector3(0.0f, wc.height, 0.0f), 0.0f);
        KeyboardState noKeys{};
        wc.update(0.016f, noKeys, /*mouseDx=*/50, /*mouseDy=*/20);
        checkNear(wc.yaw(), 50.0f * wc.mouseSens, "mouse dx moves yaw by dx * mouseSens");
        checkNear(wc.pitch(), -20.0f * wc.mouseSens, "mouse dy moves pitch by -dy * mouseSens (inverted look)");
    }

    // update(): pitch is clamped at +-~85 degrees even with a huge mouse delta.
    {
        WalkController wc;
        wc.enter(Vector3(0.0f, wc.height, 0.0f), 0.0f);
        KeyboardState noKeys{};
        wc.update(0.016f, noKeys, 0, /*mouseDy=*/-100000); // look up hard
        check(wc.pitch() <= 1.48f + 1e-4f, "pitch is clamped at the ~85 degree maximum");
        wc.update(0.016f, noKeys, 0, /*mouseDy=*/200000); // look down hard
        check(wc.pitch() >= -1.48f - 1e-4f, "pitch is clamped at the ~85 degree minimum");
    }

    // update(): jump + gravity + ground collision.
    {
        WalkController wc;
        wc.enter(Vector3(0.0f, wc.height, 0.0f), 0.0f); // posY starts at 0 (on ground)
        checkNear(wc.posY(), 0.0f, "setup: starts exactly on the ground");

        KeyboardState jump{Keys::LeftControl};
        wc.update(0.05f, jump, 0, 0);
        check(wc.posY() > 0.0f, "a jump while on the ground lifts posY above 0 on the next update");

        // Let gravity pull it back down over many frames with no keys held.
        KeyboardState noKeys{};
        for (int i = 0; i < 200 && wc.posY() > 0.0f; ++i)
            wc.update(0.05f, noKeys, 0, 0);
        checkNear(wc.posY(), 0.0f, "gravity eventually settles posY back to exactly 0 (ground collision)");

        // Jump again from the ground -- proves onGround_ was correctly
        // re-armed by the ground-collision clamp above, not stuck false.
        wc.update(0.05f, jump, 0, 0);
        check(wc.posY() > 0.0f, "a second jump from the ground works (onGround_ correctly re-armed)");
    }

    // update(): swept player-cylinder collision prevents tunnelling through a
    // thin wall, even with one long frame, and retains tangential motion for
    // natural wall sliding.
    {
        const WalkCollider wall{-10.0f, 0.0f, -2.0f, 10.0f, 3.0f, -1.0f};
        const std::array<WalkCollider, 1> colliders{wall};
        KeyboardState fwd{Keys::W};

        WalkController straight;
        straight.speed = 20.0f;
        straight.enter(Vector3(0.0f, straight.height, 0.0f), 0.0f);
        straight.update(0.2f, fwd, 0, 0, colliders); // would move 4m without sweep
        check(straight.posZ() > -0.71f && straight.posZ() < -0.69f,
              "swept wall collision stops before an expanded wall face (no tunnelling)");

        WalkController sliding;
        sliding.speed = 20.0f;
        sliding.enter(Vector3(0.0f, sliding.height, 0.0f), 0.5f);
        sliding.update(0.2f, fwd, 0, 0, colliders);
        check(sliding.posZ() > -0.71f,
              "diagonal movement is blocked by the wall instead of passing through it");
        check(sliding.posX() > 1.0f,
              "diagonal movement preserves its tangent component and slides along the wall");
    }

    // update(): box colliders are also solid vertically -- the player lands
    // on a platform and a jump cannot pass through a low ceiling.
    {
        const WalkCollider platform{-3.0f, 2.0f, -3.0f, 3.0f, 3.0f, 3.0f};
        const std::array<WalkCollider, 1> colliders{platform};
        KeyboardState noKeys{};
        WalkController falling;
        falling.enter(Vector3(0.0f, 6.8f, 0.0f), 0.0f); // feet at y=5
        for (int i = 0; i < 30 && falling.posY() > 3.0f; ++i)
            falling.update(0.05f, noKeys, 0, 0, colliders);
        checkNear(falling.posY(), 3.0f, "falling lands on the top of a box collider rather than y=0");

        const WalkCollider ceiling{-3.0f, 2.2f, -3.0f, 3.0f, 3.0f, 3.0f};
        const std::array<WalkCollider, 1> ceilingColliders{ceiling};
        KeyboardState jump{Keys::LeftControl};
        WalkController jumping;
        jumping.enter(Vector3(0.0f, jumping.height, 0.0f), 0.0f);
        for (int i = 0; i < 5; ++i)
            jumping.update(0.05f, jump, 0, 0, ceilingColliders);
        check(jumping.posY() <= 0.4001f,
              "jumping is stopped below a box-collider ceiling instead of passing through it");
    }

    // update(): Escape triggers an implicit exit, matching the
    // pre-extraction updateWalkMode()'s own early-return-on-Escape.
    {
        WalkController wc;
        wc.enter(Vector3(0.0f, wc.height, 0.0f), 0.3f);
        KeyboardState esc{Keys::Escape};
        auto result = wc.update(0.016f, esc, 0, 0);
        check(result.has_value(), "update() returns a value when Escape is held");
        check(!wc.isActive(), "update() with Escape held deactivates walk mode");
        if (result) checkNear(result->yaw, 0.3f, "the Escape-triggered exit carries the current yaw");
    }

    // viewMatrix(): differential checks that it actually incorporates
    // position/yaw/height (not hand-deriving CreateLookAt's exact matrix).
    {
        WalkController base;
        base.enter(Vector3(0.0f, base.height, 0.0f), 0.0f);
        Matrix m0 = base.viewMatrix();

        WalkController movedX;
        movedX.enter(Vector3(10.0f, movedX.height, 0.0f), 0.0f);
        check(matricesDiffer(m0, movedX.viewMatrix()), "viewMatrix() changes when position changes");

        WalkController turned;
        turned.enter(Vector3(0.0f, turned.height, 0.0f), 1.0f);
        check(matricesDiffer(m0, turned.viewMatrix()), "viewMatrix() changes when yaw changes");

        WalkController taller;
        taller.height = 3.0f;
        taller.enter(Vector3(0.0f, taller.height, 0.0f), 0.0f);
        check(matricesDiffer(m0, taller.viewMatrix()), "viewMatrix() changes when eye height changes");
    }

    if (failures == 0) std::printf("All WalkController tests passed.\n");
    else                std::printf("%d WalkController test(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
