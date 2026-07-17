# Benchmark baseline (SYS-W12-01, Phase 1)

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
per-phase). Deliberately not covered here, since each needs either code
instrumentation inside the editor/library or driving a live
`MeshCraftApplication` (materially more work than CLI-level timing):
mesh-gen, CSG + cache, traversal, picking, undo snapshot, texture
processing (in isolation), animation eval, registry, startup, first frame.
Tracked as `SYS-W12-02` in `plan.md`.
