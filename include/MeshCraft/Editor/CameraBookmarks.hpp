#pragma once

#include <array>

namespace MeshCraft::Editor {

class EditorCamera;

// SYS-W3-01 Phase 8: the five orbit-camera bookmarks are independent editor
// state. Keeping their capture/restore invariant here prevents the main
// application from owning another small state machine and makes it testable
// without constructing MeshCraftApplication or an ImGui frame.
struct CameraBookmark {
    float yaw{0.0f};
    float pitch{0.4f};
    float distance{15.0f};
    float targetX{0.0f};
    float targetY{0.0f};
    float targetZ{0.0f};
    bool valid{false};
};

class CameraBookmarks {
public:
    static constexpr int kSlotCount = 5;

    // Return false for an invalid slot. restore() also returns false when
    // the requested slot has not been saved yet.
    bool save(int slot, const EditorCamera& camera);
    bool restore(int slot, EditorCamera& camera) const;

    [[nodiscard]] const CameraBookmark* get(int slot) const;

private:
    std::array<CameraBookmark, kSlotCount> slots_{};
};

} // namespace MeshCraft::Editor
