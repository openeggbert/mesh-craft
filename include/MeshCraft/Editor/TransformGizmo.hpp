#pragma once

namespace MeshCraft::Editor {

enum class GizmoMode {
    Translate,
    Rotate,
    Scale,
};

// Transform gizmo shown in the 3D viewport for the active selection.
class TransformGizmo {
public:
    [[nodiscard]] GizmoMode mode() const { return mode_; }
    void setMode(GizmoMode mode)         { mode_ = mode; }

    // TODO: render gizmo into the viewport
    // TODO: hit testing for dragging

private:
    GizmoMode mode_{GizmoMode::Translate};
};

} // namespace MeshCraft::Editor
