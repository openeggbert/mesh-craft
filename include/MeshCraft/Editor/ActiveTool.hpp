#pragma once

// ActiveTool — the editor's current interaction tool.
//
// Deliberately kept in its own header, independent of the CNA/XNA framework,
// so the enum and its name mapping can be unit-tested standalone (see
// test/active_tool_test.cpp) without linking the whole editor/renderer.

namespace MeshCraft {

enum class ActiveTool {
    Select,
    Move,
    Rotate,
    Scale,
    AddBox,
    AddSphere,
    AddCylinder,
    AddCone,
    AddPlane,
    Measure
};

// Exhaustive, bounds-safe display-name mapping.
//
// This intentionally uses a switch with NO `default:` case. If a new
// ActiveTool enumerator is added without a matching case here, the compiler
// emits a -Wswitch warning at build time. That replaces the previous silent
// out-of-bounds defect: updateWindowTitle() indexed a 9-element `toolNames[]`
// array with the 10-value enum, so selecting the Measure ("Ruler") tool read
// `toolNames[9]` past the end of the array — undefined behavior on every title
// update while that tool was active.
//
// Every enumerator MUST return a non-null string literal. The trailing return
// only guards against a static_cast of an out-of-range integer to ActiveTool;
// it is unreachable for any valid enumerator.
constexpr const char* activeToolName(ActiveTool tool) {
    switch (tool) {
        case ActiveTool::Select:      return "Select";
        case ActiveTool::Move:        return "Move";
        case ActiveTool::Rotate:      return "Rotate";
        case ActiveTool::Scale:       return "Scale";
        case ActiveTool::AddBox:      return "Add Box";
        case ActiveTool::AddSphere:   return "Add Sphere";
        case ActiveTool::AddCylinder: return "Add Cylinder";
        case ActiveTool::AddCone:     return "Add Cone";
        case ActiveTool::AddPlane:    return "Add Plane";
        case ActiveTool::Measure:     return "Measure";
    }
    return "Unknown";
}

} // namespace MeshCraft
