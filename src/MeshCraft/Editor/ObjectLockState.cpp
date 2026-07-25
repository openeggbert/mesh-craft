#include "MeshCraft/Editor/ObjectLockState.hpp"

namespace MeshCraft::Editor {

bool ObjectLockState::empty() const {
    return ids_.empty();
}

bool ObjectLockState::isLocked(const std::string& id) const {
    return ids_.contains(id);
}

const ObjectLockState::IdSet& ObjectLockState::ids() const {
    return ids_;
}

void ObjectLockState::lock(const std::string& id) {
    ids_.insert(id);
}

void ObjectLockState::unlock(const std::string& id) {
    ids_.erase(id);
}

void ObjectLockState::toggle(const std::string& id) {
    if (isLocked(id)) unlock(id);
    else              lock(id);
}

} // namespace MeshCraft::Editor
