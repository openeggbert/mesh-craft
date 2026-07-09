#pragma once

#include <string>

namespace MeshCraft::Editor {

// NOTE: this polymorphic tool-hierarchy design is not currently used and has
// no subclasses anywhere in the codebase. The real editor switches tools via
// a plain `enum class ActiveTool` (see MeshCraftApplication.hpp) handled in
// MeshCraftApplication_Keyboard.cpp/_Mouse.cpp, not through EditorTool. Left
// in place as unused scaffolding rather than removed, since it's unclear
// whether it's intentional groundwork for a future refactor — see
// plan_deep_audit.md AUDIT-0007 before deleting it outright.
class EditorTool {
public:
    virtual ~EditorTool() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    virtual void activate()   {}
    virtual void deactivate() {}
};

} // namespace MeshCraft::Editor
