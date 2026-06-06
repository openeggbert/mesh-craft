#pragma once

#include "MeshCraft/Editor/TransformGizmo.hpp"

namespace MeshCraft::Editor {

// The 3D viewport controller — camera and gizmo state only.
// Rendering is handled by MeshCraftApplication via SceneRenderer + GridRenderer.
class EditorViewport {
public:
    explicit EditorViewport() = default;

    [[nodiscard]] TransformGizmo& gizmo() { return gizmo_; }

private:
    TransformGizmo gizmo_;
};

} // namespace MeshCraft::Editor
