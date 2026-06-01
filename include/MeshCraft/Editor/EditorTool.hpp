#pragma once

#include <string>

namespace MeshCraft::Editor {

// Base class for all editor tools (select, move, rotate, scale, primitive creation, CSG, etc.).
class EditorTool {
public:
    virtual ~EditorTool() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    virtual void activate()   {}
    virtual void deactivate() {}

    // TODO: mouse/keyboard event handling
    // virtual void onMouseDown(int x, int y, int button) {}
    // virtual void onMouseUp(int x, int y, int button)   {}
    // virtual void onMouseMove(int x, int y)             {}
    // virtual void onKeyDown(int key)                    {}
};

} // namespace MeshCraft::Editor
