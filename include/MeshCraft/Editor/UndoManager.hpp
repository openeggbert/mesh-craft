#pragma once

#include "MeshCraft/Editor/DocumentSnapshot.hpp"

#include <optional>
#include <string>
#include <vector>

namespace MeshCraft::Editor {

// SYS-W3-01 Phase 4: the undo/redo stack mechanism extracted out of
// MeshCraftApplication (SYS-W9-03's selection-restore stacks move with
// it, kept in lockstep exactly as before). Self-contained like
// KeybindingManager/Preferences -- no callback DI needed, unlike
// MacroRecorder's Context, because every side effect the pre-extraction
// pushUndo()/performUndo()/performRedo() performed beyond the stack
// bookkeeping itself (deepCopyDoc(), objectIndex_.invalidate(),
// restoreSelectionByIds(), modified_/updateWindowTitle(),
// evaluateAndPushAnimOverrides()) stays the CALLER's responsibility,
// before/after calling in here. In particular this class never calls
// deepCopyDoc() (a MeshCraftPrivate.hpp helper) itself -- callers provide an
// already-independent document, frozen by DocumentSnapshot. This permits an
// automatic SceneHistory entry and the matching undo entry to share one
// immutable graph while keeping the manager CNA-free, matching the
// KeybindingManager precedent of keeping extracted classes' dependency
// footprint minimal.
class UndoManager {
public:
    static constexpr int kMax = 20;

    struct Entry {
        DocumentSnapshot          snapshot;
        std::vector<std::string> selectionIds;
    };

    [[nodiscard]] int  undoCount() const { return static_cast<int>(undoStack_.size()); }
    [[nodiscard]] int  redoCount() const { return static_cast<int>(redoStack_.size()); }
    [[nodiscard]] bool canUndo() const { return !undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redoStack_.empty(); }

    // `snapshot` is an immutable document made from an independent deep copy.
    // It may be shared with SceneHistory. Caps at kMax and clears redo.
    void push(DocumentSnapshot snapshot, std::vector<std::string> selectionIds);
    void push(Mc3::Mc3Document independentDocument, std::vector<std::string> selectionIds) {
        push(freezeDocument(std::move(independentDocument)), std::move(selectionIds));
    }

    // Pops the most recent undo entry, first pushing `currentSnapshot` and
    // current selection onto redo. std::nullopt if canUndo() is false.
    std::optional<Entry> undo(DocumentSnapshot currentSnapshot,
                              std::vector<std::string> currentSelectionIds);
    std::optional<Entry> redo(DocumentSnapshot currentSnapshot,
                              std::vector<std::string> currentSelectionIds);
    std::optional<Entry> undo(Mc3::Mc3Document independentDocument,
                              std::vector<std::string> currentSelectionIds) {
        return undo(freezeDocument(std::move(independentDocument)), std::move(currentSelectionIds));
    }
    std::optional<Entry> redo(Mc3::Mc3Document independentDocument,
                              std::vector<std::string> currentSelectionIds) {
        return redo(freezeDocument(std::move(independentDocument)), std::move(currentSelectionIds));
    }

    // Jumps directly to the undo entry `stepsAgo` positions back from the
    // top of the undo stack (1 = the most recent undo point, same target
    // undo() reaches; the Undo History dialog's rows are exactly
    // stepsAgo == 1..undoCount()). Pushes currentDoc/currentSelectionIds
    // AND every undo entry newer than the target onto the redo stack
    // (capped at kMax, oldest evicted first), then returns the target
    // entry. std::nullopt (no-op) if stepsAgo is out of [1, undoCount()].
    std::optional<Entry> jumpTo(int stepsAgo, DocumentSnapshot currentSnapshot,
                                 std::vector<std::string> currentSelectionIds);
    std::optional<Entry> jumpTo(int stepsAgo, Mc3::Mc3Document independentDocument,
                                std::vector<std::string> currentSelectionIds) {
        return jumpTo(stepsAgo, freezeDocument(std::move(independentDocument)),
                      std::move(currentSelectionIds));
    }

    // Removes the most-recently-pushed undo entry without applying it
    // (SYS-W12-02's benchmark measures push()'s deep-copy cost, then must
    // not leave a stray entry behind). No-op if canUndo() is false.
    void popUndoWithoutApplying();

    // Discards all undo/redo history (Open/New/autosave-recovery: a
    // wholesale document replacement makes old entries refer to an
    // unrelated document).
    void clear();

private:
    std::vector<DocumentSnapshot>         undoStack_;
    std::vector<DocumentSnapshot>         redoStack_;
    std::vector<std::vector<std::string>> undoSelectionStack_;
    std::vector<std::vector<std::string>> redoSelectionStack_;
};

} // namespace MeshCraft::Editor
