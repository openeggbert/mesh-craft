#pragma once
#include "MeshBuilder.hpp"
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <map>
#include <memory>
#include <string>

namespace mc3togltf {

// Evaluate a CSG boolean tree rooted at 'csgObj' (Union/Difference/Intersection)
// using the Manifold library.
//
// The result is in the CSG root's LOCAL coordinate space: the root's own
// position/rotation/scale is NOT baked into the mesh.  The caller attaches it
// as a glTF node TRS in the usual way.
//
// Supported child primitives:
//   Box, Cube, Sphere, Cylinder, Cone — Manifold analytic shapes.
//   Torus, Capsule, IcoSphere         — triangulated and imported into Manifold.
//   Plane, Disk, Grid                 — not watertight; skipped with a warning.
//   Mesh (OBJ), Extrude               — skipped with a warning.
//   Nested CSG, Group, Area           — evaluated / unioned recursively.
//   Instance                          — definition resolved and evaluated.
//
// Throws std::runtime_error on evaluation failure.
// Returns empty MeshData when the boolean result is geometrically empty
// (e.g., intersection of non-overlapping shapes).
MeshData evaluateCsgNode(
    const MeshCraft::Mc3::Mc3Object& csgObj,
    const std::map<std::string, std::shared_ptr<MeshCraft::Mc3::Mc3Object>>& definitions);

} // namespace mc3togltf
