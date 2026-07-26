#pragma once

#include "MeshCraft/Mc3/Mc3Document.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace MeshCraft::Editor {

// SYS-W9-04: a review/checkpoint timeline deliberately separate from the
// exact, short UndoManager stacks. It is still snapshot-based (rather than a
// speculative command-log rewrite), but has its own explicit logical-memory
// budget and survives ordinary undo/redo changes. Callers pass an independent
// document snapshot to capture(); restore() returns a new independent copy.
class SceneHistory {
public:
    using SnapshotId = std::uint64_t;

    static constexpr std::size_t kDefaultBudgetBytes = 64u * 1024u * 1024u;
    static constexpr std::size_t kMaxReviewChanges = 256;

    enum class SnapshotKind { Automatic, Checkpoint };
    enum class ChangeKind { Added, Removed, Modified };

    struct SnapshotInfo {
        SnapshotId id{0};
        SnapshotKind kind{SnapshotKind::Automatic};
        std::string label;
        std::size_t estimatedBytes{0};
        std::size_t objectCount{0};
    };

    struct CaptureResult {
        bool stored{false};
        SnapshotId id{0};
        std::size_t evictedCount{0};
        std::string message;
    };

    struct BudgetResult {
        std::size_t evictedCount{0};
        std::size_t usedBytes{0};
    };

    struct RestoredState {
        Mc3::Mc3Document doc;
        std::vector<std::string> selectionIds;
    };

    struct ObjectChange {
        ChangeKind kind{ChangeKind::Modified};
        std::string objectId;
        std::string detail;
    };

    struct Diff {
        std::size_t addedCount{0};
        std::size_t removedCount{0};
        std::size_t modifiedCount{0};
        bool sceneResourcesChanged{false};
        bool truncated{false};
        std::vector<ObjectChange> changes;
    };

    explicit SceneHistory(std::size_t budgetBytes = kDefaultBudgetBytes);

    [[nodiscard]] std::size_t budgetBytes() const { return budgetBytes_; }
    [[nodiscard]] std::size_t usedBytes() const { return usedBytes_; }
    [[nodiscard]] std::size_t snapshotCount() const { return snapshots_.size(); }
    [[nodiscard]] std::vector<SnapshotInfo> snapshots() const;

    // `doc` must already be a deep, independent snapshot. Automatic entries
    // evict only older automatic entries; named checkpoints first evict older
    // automatic entries and, only if necessary, the oldest checkpoint, so the
    // configured budget is never exceeded. A single oversize snapshot is
    // rejected rather than weakening the budget.
    CaptureResult capture(Mc3::Mc3Document doc, std::vector<std::string> selectionIds,
                          std::string label, SnapshotKind kind);
    BudgetResult setBudgetBytes(std::size_t bytes);
    bool remove(SnapshotId id);
    void clear();

    // Returns a fresh deep copy; restoring a checkpoint does not consume it.
    [[nodiscard]] std::optional<RestoredState> restore(SnapshotId id) const;
    [[nodiscard]] std::optional<Diff> diffAgainst(SnapshotId id,
                                                   const Mc3::Mc3Document& current) const;

    [[nodiscard]] static std::size_t estimateDocumentBytes(const Mc3::Mc3Document& doc);
    [[nodiscard]] static std::size_t countObjects(const Mc3::Mc3Document& doc);

private:
    struct Snapshot {
        SnapshotInfo info;
        Mc3::Mc3Document doc;
        std::vector<std::string> selectionIds;
    };

    std::vector<Snapshot> snapshots_;  // oldest first
    std::size_t budgetBytes_{kDefaultBudgetBytes};
    std::size_t usedBytes_{0};
    SnapshotId nextId_{1};

    std::size_t evictFor(std::size_t requiredBytes, SnapshotKind incomingKind);
};

} // namespace MeshCraft::Editor
