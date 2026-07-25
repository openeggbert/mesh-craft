#include "MeshCraft/Editor/CameraBookmarks.hpp"

#include "MeshCraft/Editor/EditorCamera.hpp"

namespace MeshCraft::Editor {

const CameraBookmark* CameraBookmarks::get(int slot) const {
    if (slot < 0 || slot >= kSlotCount) return nullptr;
    return &slots_[static_cast<std::size_t>(slot)];
}

bool CameraBookmarks::save(int slot, const EditorCamera& camera) {
    if (slot < 0 || slot >= kSlotCount) return false;
    auto& bookmark = slots_[static_cast<std::size_t>(slot)];
    bookmark.yaw = camera.yaw;
    bookmark.pitch = camera.pitch;
    bookmark.distance = camera.distance;
    bookmark.targetX = camera.target.X;
    bookmark.targetY = camera.target.Y;
    bookmark.targetZ = camera.target.Z;
    bookmark.valid = true;
    return true;
}

bool CameraBookmarks::restore(int slot, EditorCamera& camera) const {
    const auto* bookmark = get(slot);
    if (!bookmark || !bookmark->valid) return false;
    camera.yaw = bookmark->yaw;
    camera.pitch = bookmark->pitch;
    camera.distance = bookmark->distance;
    camera.target = {bookmark->targetX, bookmark->targetY, bookmark->targetZ};
    return true;
}

} // namespace MeshCraft::Editor
