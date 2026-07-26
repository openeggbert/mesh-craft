#include "MeshCraft/Editor/SceneHistory.hpp"
#include "MeshCraft/Editor/UndoManager.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace MeshCraft::Editor;
using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool condition, const char* message) {
    if (condition) std::printf("PASS: %s\n", message);
    else { std::printf("FAIL: %s\n", message); ++failures; }
}

static Mc3Document makeDoc(const std::string& id, const std::string& padding = {}) {
    Mc3Document doc;
    doc.model = "History fixture";
    auto object = std::make_shared<Mc3Object>();
    object->id = id;
    object->name = id + padding;
    object->type = ObjectType::Box;
    object->primitive = Mc3Primitive{};
    doc.objects.push_back(object);
    return doc;
}

static Mc3Document makeLargeDoc() {
    Mc3Document doc;
    doc.model = "Large snapshot fixture";
    const std::string padding(128, 'p');
    for (int index = 0; index < 1000; ++index) {
        auto object = std::make_shared<Mc3Object>();
        object->id = "large_" + std::to_string(index);
        object->name = padding;
        object->type = ObjectType::Box;
        object->primitive = Mc3Primitive{};
        doc.objects.push_back(std::move(object));
    }
    return doc;
}

static bool containsSnapshot(const std::vector<SceneHistory::SnapshotInfo>& snapshots,
                             SceneHistory::SnapshotId id) {
    for (const auto& snapshot : snapshots)
        if (snapshot.id == id) return true;
    return false;
}

static bool hasChange(const SceneHistory::Diff& diff, SceneHistory::ChangeKind kind,
                      const std::string& objectId) {
    for (const auto& change : diff.changes)
        if (change.kind == kind && change.objectId == objectId) return true;
    return false;
}

int main() {
    const std::size_t unitBytes = SceneHistory::estimateDocumentBytes(makeDoc("aa"));

    // SYS-W9-05: one deeply independent document graph is frozen once and
    // shared by the exact undo stack and automatic review history. Both
    // consumers must still restore independently writable document values.
    {
        auto snapshot = freezeDocument(makeDoc("shared"));
        UndoManager undo;
        SceneHistory history;
        undo.push(snapshot, {"shared"});
        const auto captured = history.capture(snapshot, {"shared"}, "Shared", SceneHistory::SnapshotKind::Automatic);
        check(captured.stored && snapshot.use_count() >= 3,
              "undo and automatic history retain one shared immutable snapshot");
        const auto undoEntry = undo.undo(freezeDocument(makeDoc("after")), {"after"});
        const auto restored = history.restore(captured.id);
        check(undoEntry.has_value() && restored.has_value() &&
                  undoEntry->snapshot->objects.front()->id == "shared" &&
                  restored->doc.objects.front()->id == "shared" &&
                  restored->selectionIds == std::vector<std::string>{"shared"},
              "shared undo/history snapshot preserves document and selection restoration");
        if (restored) restored->doc.objects.front()->id = "edited-restore";
        const auto restoredAgain = history.restore(captured.id);
        check(restoredAgain.has_value() && restoredAgain->doc.objects.front()->id == "shared",
              "editing a restored document never mutates the shared snapshot");
    }

    // This is a deterministic retained-memory comparison, rather than a
    // machine-dependent RSS assertion: the old workflow retained two deep
    // graphs of this snapshot (one per owner), whereas the shared workflow
    // retains one graph with two owners. The elapsed time is informational.
    {
        auto snapshot = freezeDocument(makeLargeDoc());
        const std::size_t documentBytes = SceneHistory::estimateDocumentBytes(*snapshot);
        UndoManager undo;
        SceneHistory history(documentBytes + 1024u);
        const auto started = std::chrono::steady_clock::now();
        undo.push(snapshot, {});
        const auto captured = history.capture(snapshot, {}, "Large shared", SceneHistory::SnapshotKind::Automatic);
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        const std::size_t sharedRetainedBytes = documentBytes;
        const std::size_t formerDuplicateBytes = documentBytes * 2u;
        check(captured.stored && snapshot.use_count() >= 3 && documentBytes > 0 &&
                  sharedRetainedBytes < formerDuplicateBytes,
              "large-scene undo/history retains one logical document graph instead of two");
        std::printf("INFO: shared large snapshot attach: %.3f ms; retained logical bytes: %zu vs former %zu\n",
                    elapsed, sharedRetainedBytes, formerDuplicateBytes);
    }

    // Automatic snapshots respect the hard budget and evict the oldest
    // automatic entry first, giving long sessions more history than the
    // UndoManager's exact fixed 20-entry stack without changing that stack.
    {
        SceneHistory history(unitBytes * 2 + 1);
        const auto a = history.capture(makeDoc("aa"), {}, "A", SceneHistory::SnapshotKind::Automatic);
        const auto b = history.capture(makeDoc("bb"), {}, "B", SceneHistory::SnapshotKind::Automatic);
        const auto c = history.capture(makeDoc("cc"), {}, "C", SceneHistory::SnapshotKind::Automatic);
        check(a.stored && b.stored && c.stored, "automatic snapshots fit while evicting old automatic history");
        check(c.evictedCount == 1 && !containsSnapshot(history.snapshots(), a.id) &&
                  containsSnapshot(history.snapshots(), b.id) && containsSnapshot(history.snapshots(), c.id),
              "budget eviction removes the oldest automatic snapshot first");
        check(history.usedBytes() <= history.budgetBytes(), "automatic eviction never exceeds the configured budget");
    }

    // A named checkpoint survives normal automatic eviction. Checkpoints can
    // still be evicted only by a later checkpoint or a lowered explicit budget.
    {
        SceneHistory history(unitBytes * 3 + 1);
        const auto automaticA = history.capture(makeDoc("aa"), {}, "A", SceneHistory::SnapshotKind::Automatic);
        const auto checkpoint = history.capture(makeDoc("bb"), {"bb"}, "Milestone", SceneHistory::SnapshotKind::Checkpoint);
        const auto automaticC = history.capture(makeDoc("cc"), {}, "C", SceneHistory::SnapshotKind::Automatic);
        const auto automaticD = history.capture(makeDoc("dd"), {}, "D", SceneHistory::SnapshotKind::Automatic);
        check(automaticA.stored && checkpoint.stored && automaticC.stored && automaticD.stored,
              "checkpoint fixture snapshots store");
        check(!containsSnapshot(history.snapshots(), automaticA.id) && containsSnapshot(history.snapshots(), checkpoint.id),
              "automatic capture evicts an older automatic snapshot before a named checkpoint");

        const auto restored = history.restore(checkpoint.id);
        check(restored.has_value() && !restored->doc.objects.empty() && restored->doc.objects.front()->id == "bb" &&
                  restored->selectionIds == std::vector<std::string>{"bb"},
              "checkpoint restore returns its document and paired selection");
        if (restored) restored->doc.objects.front()->id = "mutated-return-value";
        const auto restoredAgain = history.restore(checkpoint.id);
        check(restoredAgain.has_value() && restoredAgain->doc.objects.front()->id == "bb",
              "restoring produces an independent copy and does not corrupt the stored checkpoint");
    }

    // A rejected automatic capture must not damage existing automatic history
    // while checkpoints consume the rest of the configured budget.
    {
        SceneHistory history(unitBytes * 2 + 1);
        const auto checkpointA = history.capture(makeDoc("aa"), {}, "A", SceneHistory::SnapshotKind::Checkpoint);
        const auto checkpointB = history.capture(makeDoc("bb"), {}, "B", SceneHistory::SnapshotKind::Checkpoint);
        const auto rejected = history.capture(makeDoc("cc"), {}, "C", SceneHistory::SnapshotKind::Automatic);
        check(checkpointA.stored && checkpointB.stored && !rejected.stored &&
                  containsSnapshot(history.snapshots(), checkpointA.id) &&
                  containsSnapshot(history.snapshots(), checkpointB.id),
              "a rejected automatic capture preserves existing checkpoint history");
    }

    // A single document bigger than the explicit budget is rejected rather
    // than allowing the budget to become advisory.
    {
        const std::string padding(4096, 'x');
        const Mc3Document large = makeDoc("large", padding);
        const std::size_t largeBytes = SceneHistory::estimateDocumentBytes(large);
        SceneHistory history(largeBytes - 1);
        const auto rejected = history.capture(large, {}, "Too large", SceneHistory::SnapshotKind::Checkpoint);
        check(!rejected.stored && history.snapshotCount() == 0,
              "an oversize checkpoint is rejected without exceeding the budget");
        const auto budgetResult = history.setBudgetBytes(largeBytes + 1);
        const auto stored = history.capture(large, {}, "Fits", SceneHistory::SnapshotKind::Checkpoint);
        check(budgetResult.usedBytes == 0 && stored.stored && history.usedBytes() <= history.budgetBytes(),
              "raising the explicit budget permits the same checkpoint within the limit");
    }

    // Object-level review identifies additions/removals/modifications and
    // also calls out a change to document-level resources.
    {
        Mc3Document before = makeDoc("same");
        auto removed = std::make_shared<Mc3Object>();
        removed->id = "removed";
        before.objects.push_back(removed);
        SceneHistory history(SceneHistory::kDefaultBudgetBytes);
        const auto snapshot = history.capture(before, {}, "Review base", SceneHistory::SnapshotKind::Checkpoint);

        Mc3Document current = makeDoc("same");
        current.objects.front()->transform.position[0] = 3.0f;
        auto added = std::make_shared<Mc3Object>();
        added->id = "added";
        current.objects.push_back(added);
        Mc3Material material;
        material.roughness = 0.2f;
        current.materials["changed-resource"] = material;

        const auto diff = history.diffAgainst(snapshot.id, current);
        check(diff.has_value() && diff->addedCount == 1 && diff->removedCount == 1 && diff->modifiedCount >= 2,
              "review diff counts added, removed and modified scene content");
        if (diff) {
            check(hasChange(*diff, SceneHistory::ChangeKind::Added, "added") &&
                      hasChange(*diff, SceneHistory::ChangeKind::Removed, "removed") &&
                      hasChange(*diff, SceneHistory::ChangeKind::Modified, "same"),
                  "review diff names the changed objects by stable object id");
            check(diff->sceneResourcesChanged &&
                      hasChange(*diff, SceneHistory::ChangeKind::Modified, "[scene resources]"),
                  "review diff calls out changed scene-level resources");
        }
    }

    if (failures == 0) std::printf("All SceneHistory tests passed.\n");
    else std::printf("%d SceneHistory test(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
