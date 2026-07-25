#pragma once

#include <set>
#include <string>

namespace MeshCraft::Editor {

// SYS-W3-01 Phase 11: object locks are editor-session state. The document,
// selection, undo policy, and UI presentation stay with the application;
// this class owns only the set of locked object identifiers.
class ObjectLockState {
public:
    using IdSet = std::set<std::string>;

    [[nodiscard]] bool empty() const;
    [[nodiscard]] bool isLocked(const std::string& id) const;
    [[nodiscard]] const IdSet& ids() const;

    void lock(const std::string& id);
    void unlock(const std::string& id);
    void toggle(const std::string& id);

private:
    IdSet ids_;
};

} // namespace MeshCraft::Editor
