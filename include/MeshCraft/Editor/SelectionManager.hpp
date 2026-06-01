#pragma once

#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <memory>
#include <vector>

namespace MeshCraft::Editor {

class SelectionManager {
public:
    void select(std::shared_ptr<Mc3::Mc3Object> object);
    void deselect(std::shared_ptr<Mc3::Mc3Object> object);
    void clear();

    [[nodiscard]] bool isSelected(const Mc3::Mc3Object* object) const;
    [[nodiscard]] const std::vector<std::shared_ptr<Mc3::Mc3Object>>& selection() const;

    [[nodiscard]] bool hasSelection() const { return !selection_.empty(); }

private:
    std::vector<std::shared_ptr<Mc3::Mc3Object>> selection_;
};

} // namespace MeshCraft::Editor
