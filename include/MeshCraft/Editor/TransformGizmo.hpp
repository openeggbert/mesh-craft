#pragma once

namespace MeshCraft::Editor {

enum class GizmoMode { Translate, Rotate, Scale };
enum class GizmoAxis { None = 0, X = 1, Y = 2, Z = 3 };

class TransformGizmo {
public:
    [[nodiscard]] GizmoMode mode()     const { return mode_; }
    void setMode(GizmoMode mode)             { mode_ = mode; }

    [[nodiscard]] GizmoAxis dragAxis() const { return dragAxis_; }
    [[nodiscard]] bool isDragging()    const { return dragAxis_ != GizmoAxis::None; }
    void startDrag(GizmoAxis axis)           { dragAxis_ = axis; }
    void endDrag()                           { dragAxis_ = GizmoAxis::None; }

private:
    GizmoMode mode_{GizmoMode::Translate};
    GizmoAxis dragAxis_{GizmoAxis::None};
};

} // namespace MeshCraft::Editor
