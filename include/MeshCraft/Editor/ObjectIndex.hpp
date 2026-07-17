#pragma once

#include "MeshCraft/Mc3/Mc3Document.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace MeshCraft::Editor {

// SYS-W5-04: caches id->object / name->object lookups over a Mc3Document's
// object tree (doc.objects), rebuilt lazily the first time a lookup runs
// after invalidate(). Mirrors MeshCraftApplication::flatFindById()/
// flatFindByName()'s existing first-match-in-document-order semantics --
// duplicate ids/names (SYS-W1-04: parsing stays permissive on these)
// resolve to whichever occurs first in a pre-order walk, matching the
// pre-existing linear-walk behavior exactly, so this is a drop-in
// accelerator, not a behavior change.
//
// Scope: only doc.objects (the live tree) -- not doc.definitions (Instance
// templates), matching flatFindById()'s own existing scope. Investigated
// (2026-07-17) which of the ~13+ id/name lookup call sites in the codebase
// are actually hot: only animation playback's per-frame
// evaluateAndPushAnimOverrides() (MeshCraftApplication_Anim.cpp) re-resolves
// the same channel target names every frame -- every other call site is a
// one-shot user action where an O(n) walk is negligible even on a large
// scene. So only flatFindById()/flatFindSharedById()/flatFindByName() (the
// 3 existing accessor methods, already used by that hot path) delegate to
// this cache; nothing else was changed, per this codebase's own
// no-premature-optimization convention.
//
// Invalidation is the caller's responsibility: call invalidate() after any
// operation that adds/removes/renames-id/renames-name a tree object, or
// wholesale-replaces the document. MeshCraftApplication invalidates from
// exactly two kinds of places: pushUndo() (called before virtually every
// mutating command) and each of its ~10 wholesale document_-replacement
// sites (Open/New/Undo/Redo/AI-apply/autosave-recovery/startup load) --
// see plan.md's SYS-W5-04 entry for the exhaustive enumeration this class
// assumes is covered. Reparenting/moving an object without changing its id
// or name does NOT require invalidation (confirmed: no reparent path in
// this codebase touches id/name).
class ObjectIndex {
public:
    void invalidate() { dirty_ = true; }

    [[nodiscard]] Mc3::Mc3Object* findById(const Mc3::Mc3Document& doc, const std::string& id) const;
    [[nodiscard]] std::shared_ptr<Mc3::Mc3Object> findSharedById(const Mc3::Mc3Document& doc, const std::string& id) const;
    [[nodiscard]] Mc3::Mc3Object* findByName(const Mc3::Mc3Document& doc, const std::string& name) const;

private:
    void rebuild(const Mc3::Mc3Document& doc) const;

    mutable bool dirty_ = true;
    mutable std::unordered_map<std::string, std::shared_ptr<Mc3::Mc3Object>> byId_;
    mutable std::unordered_map<std::string, std::shared_ptr<Mc3::Mc3Object>> byName_;
};

} // namespace MeshCraft::Editor
