#include "MeshCraft/Mc3/Mc3CsgOperation.hpp"

namespace MeshCraft::Mc3 {
// Mc3CsgOperation is a pure parameter struct (see Mc3CsgOperation.hpp) — this
// library deliberately has no CNA/Manifold dependency. Actual CSG evaluation
// happens in two separate, semantically-matched implementations: export-time
// (mc3togltf/src/CsgEvaluator.cpp) and editor-preview-time (SceneRenderer's
// CSG cache) — see NEXT.md's "CSG dual-path invariant" architecture note.
} // namespace MeshCraft::Mc3
