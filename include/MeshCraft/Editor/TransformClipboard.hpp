#pragma once

#include <array>

namespace MeshCraft::Mc3 {
struct Mc3Object;
}

namespace MeshCraft::Editor {

// SYS-W3-01 Phase 9: stores the intentionally narrow transform clipboard.
// It copies only position/rotation/scale, matching the existing editor
// shortcut/menu behavior; pivot and every other object property stay intact.
class TransformClipboard {
public:
    [[nodiscard]] bool hasValue() const { return hasValue_; }

    void copyFrom(const Mc3::Mc3Object& source);
    // Returns false when the clipboard is empty, leaving target unchanged.
    bool pasteTo(Mc3::Mc3Object& target) const;

private:
    std::array<float, 3> position_{0.0f, 0.0f, 0.0f};
    std::array<float, 3> rotation_{0.0f, 0.0f, 0.0f};
    std::array<float, 3> scale_{1.0f, 1.0f, 1.0f};
    bool hasValue_{false};
};

} // namespace MeshCraft::Editor
