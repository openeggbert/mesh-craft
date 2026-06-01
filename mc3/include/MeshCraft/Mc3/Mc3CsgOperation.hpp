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

    // TODO: cached mesh result after CSG evaluation
};

} // namespace MeshCraft::Mc3
