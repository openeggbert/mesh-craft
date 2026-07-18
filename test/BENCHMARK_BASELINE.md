# Benchmark baseline (SYS-W12-01 Phase 1 + SYS-W12-02 Phase 2)

Reference numbers from `test/benchmark.py`, for a human (or a future
session) to eyeball against a fresh run and notice anything wildly off —
**not** an automated pass/fail gate (see the script's own docstring for
why: wall-clock timing on a shared/virtualized/loaded machine is too noisy
across runs and machines for a hard threshold to be a real signal rather
than flakiness).

Run with:

```bash
python3 test/benchmark.py <build-dir>/mc3tomcb/mc3tomcb <build-dir>/mc3togltf/mc3togltf . --runs 7
```

## Recorded baseline

Machine: this sandbox, 2026-07-17, Release build (`cmake -S . -B b-release`
default flags), median of 7 runs per measurement.

| Fixture | Size | XML open + MCB save | MCB load + XML save | XML open + glTF export |
|---|---|---|---|---|
| `house.mc3.xml` (small) | 2.0 KB | 5.1 ms | 8.1 ms | 15.4 ms |
| `features.mc3.xml` (medium) | 5.0 KB | 9.7 ms | 9.4 ms | 19.7 ms |
| `medieval_castle.mc3.xml` (large) | 89.3 KB | 22.2 ms | 20.9 ms | 237.4 ms |

The large fixture's export time (237 ms) is disproportionately higher than
its XML/MCB convert times (~20 ms) relative to the small/medium fixtures —
expected, since export is the only measurement here that also does mesh
generation, CSG evaluation, and texture/material processing, not just
parse+serialize. Not investigated further as part of this task (no
regression to investigate — this is the first baseline); a future session
correlating this against `--stats`' own per-phase breakdown could quantify
which part of export dominates, if that becomes useful.

## Scope note — Phase 1 of `SYS-W12-01`, not the full 13-category ask

This covers 3 of the categories `SYS-W12-01`'s original description named
(XML open/save, MCB convert/load, export — combined, not broken out
per-phase). The remaining 10 (mesh-gen, CSG + cache, traversal, picking,
undo snapshot, texture processing, animation eval, registry, startup,
first frame) needed either code instrumentation inside the editor/library
or driving a live `MeshCraftApplication` — done in `SYS-W12-02` below.

## `SYS-W12-02` Phase 2 — in-process editor benchmarks (2026-07-18)

`MeshCraft <scene> --benchmark` (`MeshCraftApplication_Benchmark.cpp`) runs
headlessly, times each category using the app's real internals (not a
CLI-level proxy), and prints to stdout. Also informational, not a gate —
see `test/benchmark_editor_test.sh` (the registered `benchmark_editor`
ctest) for the smoke-level check this project actually runs (every
category line appears, process exits 0 — not a timing assertion).

```bash
./b-release/MeshCraft test/house.mc3.xml --benchmark
```

Sample reading, this sandbox, 2026-07-18, `test/house.mc3.xml` (5 root
objects, 713 verts / 1290 tris), Release build:

| Category | Reading |
|---|---|
| startup (`LoadContent()`) | ~14 ms |
| first frame | ~2.1 ms |
| warm frame (avg of 9) | ~0.5 ms |
| traversal (`scenePolyStats`) | ~0.001 ms |
| picking (`computeObjectWorldMatrix` × object count) | ~0.005 ms |
| undo snapshot (`pushUndo()` deep-copy) | ~0.025 ms |
| animation eval | skipped (scene has no actions — see `test/animation_demo.mc3.xml` for a populated reading) |
| registry (open + search) | ~0.9 ms |

**Honesty note:** mesh-gen, CSG + cache, and texture processing are NOT
isolated into their own separate timings — they're reflected in the
first-vs-warm frame delta (~1.6 ms here), since all three populate their
respective caches during those same early frames. Fully isolating them
would need deeper per-subsystem instrumentation (e.g. a hook inside
`SceneRenderer`'s own mesh/texture loaders); left as a smaller follow-up,
not claimed as done. Traversal/picking/undo-snapshot/registry numbers on
this tiny fixture are near the timer's own resolution floor — meaningful
relative comparisons need a larger fixture (`test/medieval_castle.mc3.xml`
or similar), not these absolute values.
