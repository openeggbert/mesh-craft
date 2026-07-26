#pragma once

// Canonical, exhaustive, bidirectional mapping between Mc3::ObjectType and its
// editor-facing display name ("Box", "Sphere", "IcoSphere", ...).
//
// This is the single source of truth. It replaces two divergent copies that
// both silently mishandled five primitive types:
//   * objectTypeName()     in src/MeshCraft/MeshCraftPrivate.hpp
//   * objectTypeNameAlg()  in include/MeshCraft/EditorCommandAlgorithms.hpp
//                          (formerly EditorAlgorithms.hpp before its SYS-W3-05
//                          split into per-concern headers)
// Both used `default: return "Object"` and omitted Torus, Capsule, Disk, Grid
// and IcoSphere, so those types displayed as "Object" in the outliner and,
// critically, the macro recorder wrote `add Object` for them. On replay
// objectTypeFromName("Object") fell through to Box, so recording "add Torus"
// and replaying it produced a Box — a silent, lossy round-trip.
//
// Depends only on Mc3Object.hpp (the enum), so it is safe to include from any
// CNA/ImGui-free Editor*Algorithms.hpp header and unit-testable standalone.

#include <MeshCraft/Mc3/Mc3Object.hpp>

#include <optional>
#include <string_view>

namespace MeshCraft {

// ObjectType -> display name. Exhaustive switch with NO default: adding a new
// enumerator without a case here is a compile-time -Wswitch warning instead of
// a silent "Object" fallback. The trailing return only guards a static_cast of
// an out-of-range integer and is unreachable for any real enumerator.
inline const char* objectTypeName(Mc3::ObjectType t) {
    switch (t) {
        case Mc3::ObjectType::Box:          return "Box";
        case Mc3::ObjectType::Cube:         return "Cube";
        case Mc3::ObjectType::Sphere:       return "Sphere";
        case Mc3::ObjectType::Cylinder:     return "Cylinder";
        case Mc3::ObjectType::Cone:         return "Cone";
        case Mc3::ObjectType::Plane:        return "Plane";
        case Mc3::ObjectType::Torus:        return "Torus";
        case Mc3::ObjectType::Capsule:      return "Capsule";
        case Mc3::ObjectType::Disk:         return "Disk";
        case Mc3::ObjectType::Grid:         return "Grid";
        case Mc3::ObjectType::IcoSphere:    return "IcoSphere";
        case Mc3::ObjectType::Mesh:         return "Mesh";
        case Mc3::ObjectType::Extrude:      return "Extrude";
        case Mc3::ObjectType::Group:        return "Group";
        case Mc3::ObjectType::Instance:     return "Instance";
        case Mc3::ObjectType::Union:        return "Union";
        case Mc3::ObjectType::Difference:   return "Difference";
        case Mc3::ObjectType::Intersection: return "Intersection";
        case Mc3::ObjectType::Area:         return "Area";
    }
    return "Object";
}

// Display name -> ObjectType. Symmetric inverse of objectTypeName(): every name
// objectTypeName() can produce maps back here, so the round-trip is exact for
// all enumerators. Returns std::nullopt for an unknown name instead of silently
// falling back to Box, so callers can reject or skip rather than fabricate a
// wrong-typed object.
inline std::optional<Mc3::ObjectType> objectTypeFromName(std::string_view n) {
    if (n == "Box")          return Mc3::ObjectType::Box;
    if (n == "Cube")         return Mc3::ObjectType::Cube;
    if (n == "Sphere")       return Mc3::ObjectType::Sphere;
    if (n == "Cylinder")     return Mc3::ObjectType::Cylinder;
    if (n == "Cone")         return Mc3::ObjectType::Cone;
    if (n == "Plane")        return Mc3::ObjectType::Plane;
    if (n == "Torus")        return Mc3::ObjectType::Torus;
    if (n == "Capsule")      return Mc3::ObjectType::Capsule;
    if (n == "Disk")         return Mc3::ObjectType::Disk;
    if (n == "Grid")         return Mc3::ObjectType::Grid;
    if (n == "IcoSphere")    return Mc3::ObjectType::IcoSphere;
    if (n == "Mesh")         return Mc3::ObjectType::Mesh;
    if (n == "Extrude")      return Mc3::ObjectType::Extrude;
    if (n == "Group")        return Mc3::ObjectType::Group;
    if (n == "Instance")     return Mc3::ObjectType::Instance;
    if (n == "Union")        return Mc3::ObjectType::Union;
    if (n == "Difference")   return Mc3::ObjectType::Difference;
    if (n == "Intersection") return Mc3::ObjectType::Intersection;
    if (n == "Area")         return Mc3::ObjectType::Area;
    return std::nullopt;
}

} // namespace MeshCraft
