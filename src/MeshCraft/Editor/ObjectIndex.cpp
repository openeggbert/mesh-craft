#include "MeshCraft/Editor/ObjectIndex.hpp"

#include <stdexcept>
#include <vector>

namespace MeshCraft::Editor {

namespace {

// Matches deepCopyObjectAlg()/findParentListAlg()'s existing 256-level
// cyclic-children guard convention (EditorAlgorithms.hpp) -- this is a new
// recursive walk over Mc3Object::children, so it needs the same guard.
constexpr int kMaxDepth = 256;

void walk(const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list,
          std::unordered_map<std::string, std::shared_ptr<Mc3::Mc3Object>>& byId,
          std::unordered_map<std::string, std::shared_ptr<Mc3::Mc3Object>>& byName,
          int depth)
{
    if (depth > kMaxDepth) {
        throw std::runtime_error(
            "ObjectIndex: object nesting exceeds 256 levels (cyclic "
            "Mc3Object::children graph?)");
    }
    for (const auto& obj : list) {
        if (!obj) continue;
        // try_emplace keeps the first-inserted value on a duplicate key --
        // since this walk is pre-order (self before children, matching
        // flatFindById()'s own recursive order), that reproduces
        // flatFindById()'s "first match in document order" semantics
        // exactly for documents with duplicate ids/names.
        if (!obj->id.empty())   byId.try_emplace(obj->id, obj);
        if (!obj->name.empty()) byName.try_emplace(obj->name, obj);
        if (!obj->children.empty())
            walk(obj->children, byId, byName, depth + 1);
    }
}

} // namespace

void ObjectIndex::rebuild(const Mc3::Mc3Document& doc) const {
    byId_.clear();
    byName_.clear();
    walk(doc.objects, byId_, byName_, 0);
    dirty_ = false;
}

Mc3::Mc3Object* ObjectIndex::findById(const Mc3::Mc3Document& doc, const std::string& id) const {
    if (dirty_) rebuild(doc);
    auto it = byId_.find(id);
    return it != byId_.end() ? it->second.get() : nullptr;
}

std::shared_ptr<Mc3::Mc3Object> ObjectIndex::findSharedById(const Mc3::Mc3Document& doc, const std::string& id) const {
    if (dirty_) rebuild(doc);
    auto it = byId_.find(id);
    return it != byId_.end() ? it->second : nullptr;
}

Mc3::Mc3Object* ObjectIndex::findByName(const Mc3::Mc3Document& doc, const std::string& name) const {
    if (dirty_) rebuild(doc);
    auto it = byName_.find(name);
    return it != byName_.end() ? it->second.get() : nullptr;
}

} // namespace MeshCraft::Editor
