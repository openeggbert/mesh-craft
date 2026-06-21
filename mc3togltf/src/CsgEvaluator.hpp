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
// Supported child geometry (real CSG mode):
//   Box, Cube, Sphere, Cylinder, Cone — Manifold analytic shapes.
//   Torus, Capsule, IcoSphere         — triangulated via MeshBuilder and imported into Manifold.
//   Nested CSG, Group, Area, Instance — evaluated recursively (only if their resolved
//                                       children are also supported).
//
// Unsupported child geometry causes std::runtime_error in real CSG mode:
//   Plane, Disk, Grid                 — not watertight; hard failure.
//   Mesh (OBJ), Extrude               — unsupported topology; hard failure.
//   Unresolved Instance definition    — hard failure.
//   Empty or non-manifold geometry    — hard failure.
//
// Pass --allow-approximate-csg (CLI) / enable the editor checkbox to bypass
// Manifold evaluation entirely and export children as separate meshes instead
// (geometrically incorrect — debug fallback only).
//
// Throws std::runtime_error on any evaluation failure.
// Returns empty MeshData when the boolean result is geometrically empty
// (e.g., intersection of non-overlapping shapes); a warning is printed to stderr.
MeshData evaluateCsgNode(
    const MeshCraft::Mc3::Mc3Object& csgObj,
    const std::map<std::string, std::shared_ptr<MeshCraft::Mc3::Mc3Object>>& definitions);

} // namespace mc3togltf
