// Unit test for the canonical Mc3::ObjectType <-> display-name mapping.
//
// Regression guard for the silent lossy round-trip: objectTypeName() used to
// omit Torus, Capsule, Disk, Grid and IcoSphere (they returned "Object"), and
// objectTypeFromName() fell back to Box for any unknown name. Recording a macro
// "add Torus" therefore replayed as "add Box".
//
// Depends only on ObjectTypeName.hpp + Mc3Object.hpp; no CNA/GL/ImGui.

#include "MeshCraft/Editor/ObjectTypeName.hpp"

#include <iostream>
#include <set>
#include <string>

using MeshCraft::objectTypeName;
using MeshCraft::objectTypeFromName;
using Mc3T = MeshCraft::Mc3::ObjectType;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

int main() {
    // Every enumerator in declaration order (Mc3Object.hpp).
    const Mc3T all[] = {
        Mc3T::Box, Mc3T::Cube, Mc3T::Sphere, Mc3T::Cylinder, Mc3T::Cone,
        Mc3T::Plane, Mc3T::Torus, Mc3T::Capsule, Mc3T::Disk, Mc3T::Grid,
        Mc3T::IcoSphere, Mc3T::Mesh, Mc3T::Extrude, Mc3T::Group, Mc3T::Instance,
        Mc3T::Union, Mc3T::Difference, Mc3T::Intersection, Mc3T::Area,
    };
    const int kEnumCount = 19;
    check(sizeof(all) / sizeof(all[0]) == kEnumCount,
          "test lists all 19 ObjectType enumerators");

    std::set<std::string> names;
    for (Mc3T t : all) {
        const char* n = objectTypeName(t);
        check(n && n[0] != '\0', std::string("non-empty name for enumerator #") +
              std::to_string(static_cast<int>(t)));
        // No type may collapse to the "Object" sentinel any more.
        check(std::string(n) != "Object",
              std::string("enumerator #") + std::to_string(static_cast<int>(t)) +
              " does not fall back to \"Object\"");
        names.insert(n);

        // Exact round-trip: name -> type recovers the original enumerator.
        auto back = objectTypeFromName(n);
        check(back.has_value() && *back == t,
              std::string("round-trip ") + n + " -> type -> " + n);
    }
    check(static_cast<int>(names.size()) == kEnumCount,
          "all 19 names are distinct");

    // Unknown names return nullopt (no silent Box fabrication).
    check(!objectTypeFromName("").has_value(),        "empty name -> nullopt");
    check(!objectTypeFromName("Object").has_value(),  "\"Object\" sentinel -> nullopt");
    check(!objectTypeFromName("Wombat").has_value(),  "garbage name -> nullopt");
    check(!objectTypeFromName("box").has_value(),     "wrong case -> nullopt");

    if (failures == 0) { std::cout << "All ObjectTypeName tests passed.\n"; return 0; }
    std::cerr << failures << " ObjectTypeName test(s) failed.\n";
    return 1;
}
