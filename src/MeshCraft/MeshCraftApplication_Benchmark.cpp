// SYS-W12-02: in-process instrumentation for the categories SYS-W12-01's
// CLI-level benchmark harness (test/benchmark.py) couldn't reach without
// either code instrumentation or driving a live MeshCraftApplication --
// mesh-gen, CSG + cache, traversal, picking, undo snapshot, texture
// processing, animation eval, registry, startup, first frame.
//
// Triggered by `MeshCraft <scene> --benchmark` (main.cpp), which constructs
// MeshCraftApplication in benchmark mode: LoadContent() is timed directly
// (the "startup" category), the first kBenchmarkFrames real Draw() calls
// are timed (using the app's actual camera_/view/proj -- not a hand-rolled
// stand-in) to get a genuine "first frame" vs "warm frame" comparison, and
// this file's runBenchmarkSuite() then times the remaining categories that
// don't need a render context, prints everything to stdout, and exits.
//
// Honesty note (matching this project's own established convention of not
// silently overstating coverage): mesh-gen, CSG+cache, and texture
// processing are NOT isolated into their own separate timings here --
// they're reflected in the first-vs-warm frame delta, since all three
// populate their respective caches during those same early frames. Fully
// isolating them would need deeper per-subsystem instrumentation (e.g. a
// hook inside SceneRenderer's mesh/texture loaders); left as a smaller
// follow-up rather than claiming a false precision this pass doesn't have.

#include "MeshCraft/MeshCraftApplication.hpp"

#include <chrono>
#include <functional>
#include <iostream>
#include <numeric>

namespace MeshCraft {

namespace {
template <typename Fn>
double timeMs(Fn&& fn) {
    auto t0 = std::chrono::steady_clock::now();
    fn();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}
} // namespace

void MeshCraftApplication::runBenchmarkSuite() {
    std::cout << "[Benchmark] scene: " << currentFile_.string()
              << " (" << document_.objects.size() << " root objects)\n";

    std::cout << "[Benchmark] startup (LoadContent): " << benchmarkLoadContentMs_ << " ms\n";

    if (benchmarkFrameTimesMs_.size() >= 2) {
        double first = benchmarkFrameTimesMs_.front();
        double warmSum = std::accumulate(benchmarkFrameTimesMs_.begin() + 1,
                                          benchmarkFrameTimesMs_.end(), 0.0);
        double warmAvg = warmSum / static_cast<double>(benchmarkFrameTimesMs_.size() - 1);
        std::cout << "[Benchmark] first frame: " << first << " ms\n";
        std::cout << "[Benchmark] warm frame (avg of " << (benchmarkFrameTimesMs_.size() - 1)
                  << "): " << warmAvg << " ms\n";
        std::cout << "[Benchmark] mesh-gen/CSG+cache/texture warm-up delta (first-minus-warm, "
                  << "not isolated per-category -- see this file's header comment): "
                  << (first - warmAvg) << " ms\n";
    } else {
        std::cout << "[Benchmark] first/warm frame: skipped (need >=2 frames, got "
                  << benchmarkFrameTimesMs_.size() << ")\n";
    }

    // Traversal: a full scene poly-stat walk (every object, recursively).
    {
        int verts = 0, tris = 0;
        double ms = timeMs([&] { sceneRenderer_->scenePolyStats(document_, verts, tris); });
        std::cout << "[Benchmark] traversal (scenePolyStats): " << ms << " ms ("
                  << verts << " verts, " << tris << " tris)\n";
    }

    // Picking: computeObjectWorldMatrix for every object in the scene (the
    // same lookup a real click-to-select does for whichever object was hit).
    {
        std::vector<const Mc3::Mc3Object*> allObjs;
        std::function<void(const std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> collect;
        collect = [&](const std::vector<std::shared_ptr<Mc3::Mc3Object>>& list) {
            for (const auto& o : list) {
                if (!o) continue;
                allObjs.push_back(o.get());
                collect(o->children);
            }
        };
        collect(document_.objects);
        double ms = timeMs([&] {
            for (auto* o : allObjs) sceneRenderer_->computeObjectWorldMatrix(*o, document_);
        });
        std::cout << "[Benchmark] picking (computeObjectWorldMatrix x" << allObjs.size()
                  << "): " << ms << " ms\n";
    }

    // Undo snapshot: pushUndo()'s whole-document deep-copy cost. Pops the
    // stacks back off immediately after so this doesn't leave a spurious
    // entry (harmless anyway since the process exits right after, but
    // keeping the invariant clean costs nothing).
    {
        double ms = timeMs([&] { pushUndo(); });
        undoManager_.popUndoWithoutApplying();
        std::cout << "[Benchmark] undo snapshot (pushUndo deep-copy): " << ms << " ms\n";
    }

    // Animation eval: only meaningful if the scene actually has an action.
    if (!document_.actions.empty()) {
        currentActionName_ = document_.actions.begin()->first;
        double ms = timeMs([&] { evaluateAndPushAnimOverrides(); });
        std::cout << "[Benchmark] animation eval (" << currentActionName_ << "): " << ms << " ms\n";
    } else {
        std::cout << "[Benchmark] animation eval: skipped (scene has no actions)\n";
    }

    // Registry: real SQLite open (or reuse if already open) + a query.
    {
        double ms = timeMs([&] {
            if (!registry_.isOpen()) registry_.open(ModelRegistry::defaultPath());
            (void)registry_.search("");
        });
        std::cout << "[Benchmark] registry (open + search): " << ms << " ms\n";
    }
}

} // namespace MeshCraft
