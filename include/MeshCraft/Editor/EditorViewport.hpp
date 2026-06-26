#pragma once

#include "MeshCraft/Editor/EditorCamera.hpp"
#include "MeshCraft/Editor/TransformGizmo.hpp"

#include <Microsoft/Xna/Framework/Vector3.hpp>

namespace MeshCraft::Editor {

struct PickRay {
    Microsoft::Xna::Framework::Vector3 origin;
    Microsoft::Xna::Framework::Vector3 direction;
};

// The 3D viewport controller — camera and gizmo state only.
// Rendering is handled by MeshCraftApplication via SceneRenderer + GridRenderer.
class EditorViewport {
public:
    explicit EditorViewport() = default;

    [[nodiscard]] EditorCamera&       camera()       { return camera_; }
    [[nodiscard]] const EditorCamera& camera() const { return camera_; }
    [[nodiscard]] TransformGizmo&     gizmo()        { return gizmo_; }

    // Convert pixel mouse position (mx, my) inside viewport rect (vX, vY, vW, vH)
    // to a world-space pick ray.
    [[nodiscard]] PickRay pickRay(int mx, int my,
                                  int vX, int vY, int vW, int vH) const;

private:
    EditorCamera   camera_;
    TransformGizmo gizmo_;
};

} // namespace MeshCraft::Editor
