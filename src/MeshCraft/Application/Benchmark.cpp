// SYS-W12-04: in-process instrumentation for the categories SYS-W12-01's
// CLI-level benchmark harness (test/benchmark.py) couldn't reach without
// either code instrumentation or driving a live MeshCraftApplication --
// mesh-gen, CSG + cache, traversal, picking, undo snapshot, texture
// processing, animation eval, registry, startup, first frame.
//
// Triggered by `MeshCraft <scene> --benchmark` (main.cpp), which constructs
// MeshCraftApplication in benchmark mode: LoadContent() is timed directly
// (the "startup" category), the first BenchmarkProgress real Draw() calls
// are timed (using the app's actual camera_/view/proj -- not a hand-rolled
// stand-in) to get a genuine "first frame" vs "warm frame" comparison, and
// this file's runBenchmarkSuite() then times the remaining categories that
// don't need a render context, prints everything to stdout, and exits.
//
// Mesh generation and CSG evaluation below are direct CPU work samples; the
// texture line is collected around the live renderer's cache-miss decode /
// rasterize / Texture2D-upload paths during the same real frames that supply
// the first-versus-warm timing. None of these values is a CI threshold: they
// are reproducible local baselines and cache/allocation observability.

#include "MeshCraft/Application/MeshCraftApplication.hpp"

#include "CsgEvaluator.hpp"
#include "MeshBuilder.hpp"

#include <chrono>
#include <functional>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

namespace MeshCraft::Application {

namespace {
template <typename Fn>
double timeMs(Fn&& fn) {
    auto t0 = std::chrono::steady_clock::now();
    fn();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void collectUniqueObjects(
    const std::vector<std::shared_ptr<Mc3::Mc3Object>>& roots,
    std::unordered_set<const Mc3::Mc3Object*>& seen,
    std::vector<const Mc3::Mc3Object*>& objects)
{
    for (const auto& object : roots) {
        if (!object || !seen.insert(object.get()).second) continue;
        objects.push_back(object.get());
        collectUniqueObjects(object->children, seen, objects);
    }
}

bool isCsgObject(const Mc3::Mc3Object& object) {
    using Mc3::ObjectType;
    return object.type == ObjectType::Union || object.type == ObjectType::Difference ||
           object.type == ObjectType::Intersection;
}
} // namespace

void MeshCraftApplication::runBenchmarkSuite() {
    std::cout << "[Benchmark] scene: " << currentFile_.string()
              << " (" << document_.objects.size() << " root objects)\n";

    std::cout << "[Benchmark] startup (LoadContent): " << benchmarkProgress_.loadContentMs() << " ms\n";

    const auto& frameTimesMs = benchmarkProgress_.frameTimesMs();
    if (frameTimesMs.size() >= 2) {
        double first = frameTimesMs.front();
        double warmSum = std::accumulate(frameTimesMs.begin() + 1, frameTimesMs.end(), 0.0);
        double warmAvg = warmSum / static_cast<double>(frameTimesMs.size() - 1);
        std::cout << "[Benchmark] first frame: " << first << " ms\n";
        std::cout << "[Benchmark] warm frame (avg of " << (frameTimesMs.size() - 1)
                  << "): " << warmAvg << " ms\n";
        std::cout << "[Benchmark] first-minus-warm frame delta: "
                  << (first - warmAvg) << " ms\n";
    } else {
        std::cout << "[Benchmark] first/warm frame: skipped (need >=2 frames, got "
                  << frameTimesMs.size() << ")\n";
    }

    const auto& gpuBuffers = sceneRenderer_->lastGpuBufferCacheStats();
    std::cout << "[Benchmark] GPU buffers (last warm scene draw): tint created "
              << gpuBuffers.ordinaryTintBufferCreations << ", tint cache hits "
              << gpuBuffers.ordinaryTintCacheHits << ", authored UV created "
              << gpuBuffers.authoredUvBufferCreations << ", authored UV cache hits "
              << gpuBuffers.authoredUvCacheHits << ", authored UV cache entries "
              << sceneRenderer_->authoredUvBufferCacheSize() << "\n";

    // Collect every live authored primitive once. Definitions are included as
    // Instance targets, but pointer identity prevents a shared definition from
    // being counted twice through a hierarchy reference.
    std::unordered_set<const Mc3::Mc3Object*> seenObjects;
    std::vector<const Mc3::Mc3Object*> allObjects;
    collectUniqueObjects(document_.objects, seenObjects, allObjects);
    for (const auto& [id, definition] : document_.definitions) {
        (void)id;
        collectUniqueObjects({definition}, seenObjects, allObjects);
    }

    // Direct CPU primitive generation. This deliberately does not include
    // drawing or GPU upload, so a frame's renderer scheduling does not hide a
    // tessellator regression. Failures are reported rather than aborting the
    // informational benchmark on one unsupported/bad authored primitive.
    {
        int primitiveCount = 0;
        int failures = 0;
        int vertices = 0;
        int triangles = 0;
        const double ms = timeMs([&] {
            for (const auto* object : allObjects) {
                if (!object || !object->primitive) continue;
                try {
                    const auto mesh = mc3togltf::buildPrimitive(*object->primitive);
                    ++primitiveCount;
                    vertices += mesh.vertexCount();
                    triangles += static_cast<int>(mesh.indices.size() / 3);
                } catch (const std::exception&) {
                    ++failures;
                }
            }
        });
        std::cout << "[Benchmark] mesh generation (CPU primitive builder x" << primitiveCount
                  << "): " << ms << " ms (" << vertices << " verts, " << triangles
                  << " tris, " << failures << " failure(s))\n";
    }

    // CSG has a materially different cold path (Manifold evaluation) and
    // warm path (cache lookup). Keep them distinct rather than burying both in
    // the generic first-frame delta. This mirrors the evaluator used by the
    // renderer/exporter but intentionally stays CPU-only: no GPU driver work
    // or ordinary object drawing is folded into either number.
    {
        std::vector<const Mc3::Mc3Object*> csgObjects;
        for (const auto* object : allObjects)
            if (object && isCsgObject(*object)) csgObjects.push_back(object);
        std::unordered_map<const Mc3::Mc3Object*, mc3togltf::CsgMeshData> csgCache;
        int failures = 0;
        const double coldMs = timeMs([&] {
            for (const auto* object : csgObjects) {
                try {
                    csgCache.emplace(object,
                        mc3togltf::evaluateCsgNodeWithMaterials(*object, document_.definitions));
                } catch (const std::exception&) {
                    ++failures;
                }
            }
        });
        int warmHits = 0;
        const double warmMs = timeMs([&] {
            for (const auto* object : csgObjects)
                if (csgCache.find(object) != csgCache.end()) ++warmHits;
        });
        std::cout << "[Benchmark] CSG CPU evaluator: cold " << coldMs << " ms ("
                  << csgCache.size() << "/" << csgObjects.size() << " cached, "
                  << failures << " failure(s)); warm cache lookup " << warmMs
                  << " ms (" << warmHits << " hit(s))\n";
    }

    // The renderer reports exactly its texture cache-miss path during each
    // real benchmark frame. First and warm samples make a decode/rasterize +
    // GPU-upload cost visible without forcing a second artificial draw.
    if (!benchmarkTextureStats_.empty()) {
        const auto& first = benchmarkTextureStats_.front();
        double warmMs = 0.0;
        int warmHits = 0, warmMisses = 0, warmUploads = 0;
        for (std::size_t index = 1; index < benchmarkTextureStats_.size(); ++index) {
            const auto& stats = benchmarkTextureStats_[index];
            warmMs += stats.decodeUploadMs;
            warmHits += stats.cacheHits;
            warmMisses += stats.cacheMisses;
            warmUploads += stats.gpuUploads;
        }
        const std::size_t warmFrames = benchmarkTextureStats_.size() - 1;
        std::cout << "[Benchmark] texture decode/upload: first " << first.decodeUploadMs
                  << " ms (hits " << first.cacheHits << ", misses " << first.cacheMisses
                  << ", uploads " << first.gpuUploads << "); warm avg "
                  << (warmFrames ? warmMs / static_cast<double>(warmFrames) : 0.0)
                  << " ms (" << warmHits << " hits, " << warmMisses << " misses, "
                  << warmUploads << " uploads across " << warmFrames << " frame(s))\n";
    } else {
        std::cout << "[Benchmark] texture decode/upload: skipped (no scene draw sample)\n";
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

    // Undo snapshot: one whole-document deep copy frozen for both undo and
    // automatic history. Pops the undo entry immediately after so this
    // doesn't leave a spurious exact-undo entry (the process exits anyway).
    {
        double ms = timeMs([&] { pushUndo(); });
        undoManager_.popUndoWithoutApplying();
        std::cout << "[Benchmark] undo snapshot (one deep copy, shared history): " << ms << " ms\n";
    }

    // Animation eval: only meaningful if the scene actually has an action.
    if (!document_.actions.empty()) {
        currentActionName_ = document_.actions.begin()->first;
        currentActionClipName_.clear();
        clearAnimationPreviewTransition();
        double ms = timeMs([&] { evaluateAndPushAnimOverrides(); });
        std::cout << "[Benchmark] animation eval (" << currentActionName_ << "): " << ms << " ms\n";
    } else {
        std::cout << "[Benchmark] animation eval: skipped (scene has no actions)\n";
    }

    // Registry: real SQLite open (or reuse if already open) + a query.
    {
        double ms = timeMs([&] {
            registryWorkspace_.benchmarkOpenAndSearch();
        });
        std::cout << "[Benchmark] registry (open + search): " << ms << " ms\n";
    }
}

} // namespace MeshCraft::Application
