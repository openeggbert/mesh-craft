#include "MeshCraft/Editor/TransformClipboard.hpp"

#include "MeshCraft/Mc3/Mc3Object.hpp"

namespace MeshCraft::Editor {

void TransformClipboard::copyFrom(const Mc3::Mc3Object& source) {
    position_ = source.transform.position;
    rotation_ = source.transform.rotation;
    scale_ = source.transform.scale;
    hasValue_ = true;
}

bool TransformClipboard::pasteTo(Mc3::Mc3Object& target) const {
    if (!hasValue_) return false;
    target.transform.position = position_;
    target.transform.rotation = rotation_;
    target.transform.scale = scale_;
    return true;
}

} // namespace MeshCraft::Editor
