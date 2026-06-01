#include "MeshCraft/Editor/SelectionManager.hpp"

#include <algorithm>

namespace MeshCraft::Editor {

void SelectionManager::select(std::shared_ptr<Mc3::Mc3Object> object) {
    if (!isSelected(object.get()))
        selection_.push_back(std::move(object));
}

void SelectionManager::deselect(std::shared_ptr<Mc3::Mc3Object> object) {
    selection_.erase(
        std::remove_if(selection_.begin(), selection_.end(),
            [&](const auto& o) { return o.get() == object.get(); }),
        selection_.end());
}

void SelectionManager::clear() {
    selection_.clear();
}

bool SelectionManager::isSelected(const Mc3::Mc3Object* object) const {
    return std::any_of(selection_.begin(), selection_.end(),
        [object](const auto& o) { return o.get() == object; });
}

const std::vector<std::shared_ptr<Mc3::Mc3Object>>& SelectionManager::selection() const {
    return selection_;
}

} // namespace MeshCraft::Editor
