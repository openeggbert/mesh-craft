#pragma once

#include "MeshCraft/Mc3/Mc3Document.hpp"

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
// deepCopyDoc() (a MeshCraftPrivate.hpp helper) itself -- every Document
// passed in must already be an independent copy the caller made -- so it
// has zero dependency on MeshCraftPrivate.hpp, matching the
// KeybindingManager precedent of keeping extracted classes' dependency
// footprint minimal.
class UndoManager {
public:
    static constexpr int kMax = 20;

    struct Entry {
        Mc3::Mc3Document          doc;
        std::vector<std::string> selectionIds;
    };

    [[nodiscard]] int  undoCount() const { return static_cast<int>(undoStack_.size()); }
    [[nodiscard]] int  redoCount() const { return static_cast<int>(redoStack_.size()); }
    [[nodiscard]] bool canUndo() const { return !undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redoStack_.empty(); }

    // `doc`/`selectionIds` must already be independent copies (the
    // caller's responsibility). Caps at kMax and clears the redo stack --
    // a new action invalidates any previously-undone-past history.
    void push(Mc3::Mc3Document doc, std::vector<std::string> selectionIds);

    // Pops the most recent undo entry, first pushing `currentDoc`/
    // `currentSelectionIds` (already-independent copies) onto the redo
    // stack. std::nullopt (no-op) if canUndo() is false.
    std::optional<Entry> undo(Mc3::Mc3Document currentDoc, std::vector<std::string> currentSelectionIds);
    std::optional<Entry> redo(Mc3::Mc3Document currentDoc, std::vector<std::string> currentSelectionIds);

    // Jumps directly to the undo entry `stepsAgo` positions back from the
    // top of the undo stack (1 = the most recent undo point, same target
    // undo() reaches; the Undo History dialog's rows are exactly
    // stepsAgo == 1..undoCount()). Pushes currentDoc/currentSelectionIds
    // AND every undo entry newer than the target onto the redo stack
    // (capped at kMax, oldest evicted first), then returns the target
    // entry. std::nullopt (no-op) if stepsAgo is out of [1, undoCount()].
    std::optional<Entry> jumpTo(int stepsAgo, Mc3::Mc3Document currentDoc,
                                 std::vector<std::string> currentSelectionIds);

    // Removes the most-recently-pushed undo entry without applying it
    // (SYS-W12-02's benchmark measures push()'s deep-copy cost, then must
    // not leave a stray entry behind). No-op if canUndo() is false.
    void popUndoWithoutApplying();

    // Discards all undo/redo history (Open/New/autosave-recovery: a
    // wholesale document replacement makes old entries refer to an
    // unrelated document).
    void clear();

private:
    std::vector<Mc3::Mc3Document>         undoStack_;
    std::vector<Mc3::Mc3Document>         redoStack_;
    std::vector<std::vector<std::string>> undoSelectionStack_;
    std::vector<std::vector<std::string>> redoSelectionStack_;
};

} // namespace MeshCraft::Editor
