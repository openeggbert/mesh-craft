#include "MeshCraft/Editor/CameraBookmarks.hpp"
#include "MeshCraft/Editor/EditorCamera.hpp"

#include <cmath>
#include <cstdio>

using MeshCraft::Editor::CameraBookmarks;
using MeshCraft::Editor::EditorCamera;

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (condition) std::printf("PASS: %s\n", message);
    else { std::printf("FAIL: %s\n", message); ++failures; }
}

bool near(float left, float right) {
    return std::abs(left - right) < 1e-5f;
}
} // namespace

int main() {
    CameraBookmarks bookmarks;
    EditorCamera camera;

    check(bookmarks.get(0) && !bookmarks.get(0)->valid,
          "slots begin empty");
    check(!bookmarks.restore(0, camera), "empty slot cannot restore a camera");
    check(!bookmarks.save(-1, camera) && !bookmarks.save(CameraBookmarks::kSlotCount, camera),
          "out-of-range save is rejected");
    check(!bookmarks.get(-1) && !bookmarks.get(CameraBookmarks::kSlotCount),
          "out-of-range lookup is rejected");

    camera.yaw = 1.25f;
    camera.pitch = -0.45f;
    camera.distance = 27.0f;
    camera.target = {3.0f, -2.0f, 9.0f};
    check(bookmarks.save(2, camera), "valid slot captures the camera");
    const auto* saved = bookmarks.get(2);
    check(saved && saved->valid && near(saved->targetZ, 9.0f),
          "captured slot exposes its valid snapshot");

    camera.reset();
    check(bookmarks.restore(2, camera), "saved slot restores the camera");
    check(near(camera.yaw, 1.25f) && near(camera.pitch, -0.45f) &&
              near(camera.distance, 27.0f) && near(camera.target.X, 3.0f) &&
              near(camera.target.Y, -2.0f) && near(camera.target.Z, 9.0f),
          "restore recovers all orbit-camera fields");

    return failures == 0 ? 0 : 1;
}
