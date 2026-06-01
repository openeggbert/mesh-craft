#pragma once

#include "MeshCraft/Editor/TransformGizmo.hpp"

namespace MeshCraft::Editor {

// The main 3D viewport.
class EditorViewport {
public:
    explicit EditorViewport() = default;

    // TODO: camera position, orientation, projection
    // TODO: grid rendering
    // TODO: object rendering via Nova3D
    // TODO: mouse picking

    [[nodiscard]] TransformGizmo& gizmo() { return gizmo_; }

private:
    TransformGizmo gizmo_;
};

} // namespace MeshCraft::Editor
