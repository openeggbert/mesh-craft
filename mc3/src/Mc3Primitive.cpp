#include "MeshCraft/Mc3/Mc3Primitive.hpp"

namespace MeshCraft::Mc3 {
// Mc3Primitive is a pure parameter struct (see Mc3Primitive.hpp) — this
// library deliberately has no CNA/mesh-generation dependency. Actual mesh
// generation from these parameters lives in mc3togltf/src/MeshBuilder.cpp
// (buildPrimitive()), consumed by both the exporter and the CSG evaluator.
} // namespace MeshCraft::Mc3
