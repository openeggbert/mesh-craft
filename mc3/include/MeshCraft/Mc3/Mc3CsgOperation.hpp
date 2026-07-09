#pragma once

namespace MeshCraft::Mc3 {

enum class CsgType {
    Union,
    Difference,
    Intersection,
};

// Attached to an Mc3Object whose type is union/difference/intersection.
// The operation applies to the object's children list.
struct Mc3CsgOperation {
    CsgType csgType{CsgType::Union};

    // No cached mesh result field here by design — this struct stays a pure,
    // CNA-free parameter holder. Result caching happens downstream, in
    // SceneRenderer's own CSG cache (editor preview) and per-export in
    // mc3togltf/src/CsgEvaluator.cpp — see NEXT.md's CSG dual-path note.
};

} // namespace MeshCraft::Mc3
