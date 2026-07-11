// Unit test for MeshCraft::ActiveTool and its bounds-safe name mapping.
//
// Regression guard for the P0 out-of-bounds defect: updateWindowTitle() used
// to index a 9-element `toolNames[]` array with the 10-value ActiveTool enum,
// so selecting the Measure ("Ruler") tool read `toolNames[9]` past the end of
// the array (undefined behavior). activeToolName() now covers every
// enumerator via an exhaustive switch.
//
// Deliberately depends on NOTHING but the standalone ActiveTool.hpp header, so
// it links with no CNA/ImGui/OpenGL/SDL dependencies.

#include "MeshCraft/Editor/ActiveTool.hpp"

#include <cstring>
#include <iostream>
#include <set>
#include <string>

using MeshCraft::ActiveTool;
using MeshCraft::activeToolName;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) {
        std::cout << "PASS: " << msg << "\n";
    } else {
        std::cerr << "FAIL: " << msg << "\n";
        ++failures;
    }
}

int main() {
    // Every enumerator, including the previously-unmapped Measure (index 9).
    const ActiveTool all[] = {
        ActiveTool::Select,  ActiveTool::Move,    ActiveTool::Rotate,
        ActiveTool::Scale,   ActiveTool::AddBox,  ActiveTool::AddSphere,
        ActiveTool::AddCylinder, ActiveTool::AddCone, ActiveTool::AddPlane,
        ActiveTool::Measure,
    };

    std::set<std::string> names;
    for (ActiveTool t : all) {
        const char* n = activeToolName(t);
        check(n != nullptr, "name is non-null");
        check(n != nullptr && n[0] != '\0', "name is non-empty");
        if (n) names.insert(n);
    }

    // 10 enumerators -> 10 distinct, human-readable names (no collisions,
    // no fallthrough to a shared "Unknown").
    check(names.size() == 10, "all 10 tools map to distinct names");

    // The exact defect: Measure must resolve to a real label, not garbage.
    check(std::strcmp(activeToolName(ActiveTool::Measure), "Measure") == 0,
          "Measure maps to \"Measure\"");
    check(std::strcmp(activeToolName(ActiveTool::Select), "Select") == 0,
          "Select maps to \"Select\"");
    check(std::strcmp(activeToolName(ActiveTool::AddPlane), "Add Plane") == 0,
          "AddPlane maps to \"Add Plane\"");

    // constexpr-usable (compile-time evaluation path exercised).
    static_assert(activeToolName(ActiveTool::Measure)[0] == 'M',
                  "activeToolName is usable in constant expressions");

    if (failures == 0) {
        std::cout << "All ActiveTool tests passed.\n";
        return 0;
    }
    std::cerr << failures << " ActiveTool test(s) failed.\n";
    return 1;
}
