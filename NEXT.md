# MeshCraft — Current Handoff

_Last updated: 2026-07-27._ Read this before resuming work. The authoritative
active queue is [`plan.md`](plan.md); historical detail is in
[`docs/history/`](docs/history/).

## Product snapshot

MeshCraft is a pre-production C++23 3D scene editor for the MC3 format. Its
canonical `Mc3Document` model is shared by XML/JSON parsing and writing, MCB,
the MC3→MCB and MC3→glTF/GLB tools, and the editor. The editor is built on CNA;
the sibling `cna` and `sharp-runtime` repositories are consumed read-only and
must not be modified here.

The feature set already includes:

- MC3 XML/JSON, MCB, glTF/GLB export, CLI conversion tools, validation, and
  bounded parser/resource limits;
- CSG, dynamic/generated geometry, authored LOD, materials, UV mapping,
  textures, lights, cameras, animation clips, and scene history;
- editable OBJ and self-contained GLB/glTF import, local model registries,
  asset packs, Lua scripts, triggers, states, and explicit event bindings;
- undo/redo, autosave recovery, and automated unit, differential, smoke,
  screenshot, and format round-trip coverage.

The original stabilization work and `SYS-W14-28` through `SYS-W14-39` are
complete. MeshCraft is now roughly 75–80% of the way to a solid 1.0: the work
remaining is concentrated in correctness, performance, documentation, CI, and
distribution rather than missing editor breadth.

## Current priorities

1. Obtain native standalone Windows qualification (`SYS-W11-06`). The
   release-readiness changes are published on `develop`; await the first
   remote `windows-2022` CTest/artifact evidence — see "Session log" below,
   its first real run already landed and found real issues.
2. Autonomous continuation in progress on the freshly-added `[PROPOSED]`
   backlog (`SYS-W9-06/07`, `SYS-W1-08`, `SYS-W2-06`, `SYS-W11-08/09/10`,
   `SYS-W13-03`, `SYS-W3-05`). **8 of 9 DONE as of this update**:
   `SYS-W9-06`, `SYS-W2-06`, `SYS-W1-08`, `SYS-W9-07`, `SYS-W13-03`,
   `SYS-W11-08`, `SYS-W11-09` all landed, each its own commit, build-verified
   and tested before committing.
   **`SYS-W11-09` found a real heap-use-after-free bug** in
   `EventPreviewRunner::execute()`'s RunScript step (`ff62004`) — a stale
   `working.scripts` iterator read again after `LuaScriptRunner::run()`
   already replaced `working` via move-assignment. Fixed. Also fixed 4
   instances of a known-shape LeakSanitizer finding (deliberately cyclic
   `shared_ptr` test fixtures) and guarded `package_consumer_smoke` out
   under `MESHCRAFT_SANITIZE=ON` (structurally incompatible with that test's
   own purpose, not a bug). `build-asan/` is ~5.7 GB on disk, kept for
   incremental reuse rather than deleted.
   Remaining: `SYS-W11-10` (RC process/versioning), `SYS-W3-05`
   (decompose `EditorAlgorithms.hpp`, doing this one last per the original
   review's own recommended ordering: architecture after correctness). See
   `plan.md` for full evidence on everything done so far.

`SYS-W14-40` is complete: the explicit bounded Event Preview/Play mode is
covered by `event_preview_runner`. See `plan.md` for the remaining Windows
qualification evidence and the full release-gate order.

## Session log (2026-07-26, autonomous continuation)

User explicitly authorized an unattended multi-hour session over the
freshly-merged `[PROPOSED]` backlog, with one carved-out exception to
"leave general CI-red alone for now": fixing the Editor (EASYGL/VULKAN) CI
job's missing `cna` submodule checkout (mesh-craft's own workflow file, not
CNA source — `submodules: true` added to `.github/workflows/ci.yml`'s
Checkout CNA step).

**The first real `develop` CI run (before tonight's push) surfaced 6 concrete
regressions, deliberately left alone per the user's instruction, tracked here
for the next pass that picks them up:**
1. Windows: `mc3togltf`'s test suite almost entirely fails with
   `STATUS_DLL_NOT_FOUND (0xC0000135)` — a required runtime DLL isn't next to
   the exe on the CI runner itself, not just in a downloaded artifact.
2. Windows: `mc3_roundtrip` — `std::filesystem::remove()` on a temp file
   throws "cannot remove: ... being used by another process".
3. Windows: `mcb_load_policy` — one assertion's expected rejection-message
   text differs because of POSIX- vs Windows-style absolute-path handling.
4. Windows: `mc3togltf_large_obj_stress` — the Python test harness does
   `import resource` unconditionally; that module doesn't exist on Windows.
5. Linux ASan+UBSan: `mc3_roundtrip`'s cyclic-children-graph test leaks by
   construction (LeakSanitizer correctly flags the test fixture itself).
6. Linux ASan+UBSan: `mc3_json_document_budget` times out at the 30s CTest
   ceiling — confirmed (via `git stash` isolating the change) this is
   pre-existing on unmodified code, caused by `-DCMAKE_BUILD_TYPE=Debug`
   (~50-55s) vs. a `Release`-flagged build (~8-10s), not anything from
   tonight's `SYS-W1-08` work.

Completed and pushed, one commit each, in this order:
- **CI fix**: `cna` submodule checkout in the Editor matrix job (`084af6f`).
- **`SYS-W9-06`** (`edcc677`): one portable atomic-file-replace primitive
  (`Mc3::writeFileAtomically`/`uniqueSiblingTempPath`,
  `mc3/include/MeshCraft/Mc3/Mc3AtomicFileWriter.hpp`) replacing the 4
  duplicated fixed-`.tmp`-name write-then-rename call sites in
  `Mc3XmlWriter`/`Mc3JsonWriter`/`McbWriter`/`GltfExporter`'s GLB path.
- **`SYS-W2-06`** (`a7ecc90`): fixed `budgetedLuaAllocator()`'s new-allocation
  accounting bug (a new object's `oldSize` is Lua's type tag, not a real
  prior size — confirmed against the vendored Lua 5.4 source). Moved to
  header-only `include/MeshCraft/Editor/LuaMemoryBudget.hpp`.
- **`SYS-W1-08`** (`a0b4530`): `Mc3JsonParser` gained the same
  `Mc3Validation`-capturing diagnostics `Mc3XmlParser` already had; wired
  `loadSceneFileDispatched()` and both `.mc3lib` loaders through it.

Each commit built and tested clean (standalone `mc3`/`mcb`/`mc3togltf`, the
full root `MeshCraft` editor target, and the affected root-project test
subset) before committing. The 3 pre-existing `mc3togltf_blender_import`
family failures (missing `numpy` in this sandbox's Blender) are unrelated
environment gaps, not regressions, and were left alone.

Continuing (each its own commit, in this order after the above): **`SYS-W9-07`**
(`bdd3d03`, untitled-scene crash recovery), **`SYS-W13-03`** (`2c2ce4f`, 4
verified doc-drift fixes), **`SYS-W11-08`** (`fbaf683`, clean-room CLI
release artifacts). `SYS-W11-08` found a real functional gap along the
way, not just a testing gap: **neither `mc3tomcb` nor `mc3togltf` supported
`--version` at all** (usage text / "input file not found" respectively) —
fixed for both. Also fixed a self-referential bug in the new SHA-256
manifest generator (a stale manifest from a previous run got its own old
hash included in the new one). Windows got the same manifest/notices staged
additively in CI, without touching the already-deferred DLL-not-found gap.

Remaining: `SYS-W11-09`, `SYS-W11-10`, `SYS-W3-05` in that order (see
`plan.md`'s recommended ordering note on `SYS-W11-06`). If this file wasn't
updated further after this paragraph and the session ended, check `git log`
on `develop` and `plan.md`'s own `[DONE]`/`[IN_PROGRESS]` markers for the
true current state before assuming
anything below this point is stale.

`SYS-W11-09` and `SYS-W11-10` landed (see `plan.md`, both `[DONE]`). Then
**`SYS-W3-05`** (real refactor, not cosmetic): split the 2,514-line
`EditorAlgorithms.hpp` into 7 cohesive, CNA-free headers
(`EditorCommandAlgorithms.hpp`/`EditorSelectionAlgorithms.hpp`/
`EditorTransformAlgorithms.hpp`/`EditorPersistenceAlgorithms.hpp`/
`EditorEventAlgorithms.hpp`/`EditorPreferencesAlgorithms.hpp`/
`EditorUtilityAlgorithms.hpp`), moved `findParentListAlg`/`removeFromListAlg`
into Commands (not Selection as first proposed) once grep proved every call
site is a Commands/Transform mutator, and migrated all 30 flagged consumer
files individually — 14 now need exactly 1 new header, 8 need 2, 1 needs 3,
1 (`Commands.cpp`) needs 4; one (`Macro.cpp`) had a dead include removed
outright; 5 were comment-only false positives needing no change. No facade
left behind; the original header is deleted. `MeshCraft` plus all 13
affected test targets built clean with zero missing-include errors;
`ctest -LE render` is 181 tests / 163 passed, with only the two
already-known pre-existing exceptions (`mc3_json_document_budget` timeout,
3 Blender-`numpy` import tests) plus 14 unrelated `mcb_*`/`mc3togltf_*`
tests that were simply never built in this reused `cmake-build-debug`
(spot-built 3 to confirm — pre-existing gap, not a regression). See
`plan.md`'s `SYS-W3-05` entry for full line-count/fan-out evidence.

All 9 `[PROPOSED]` backlog tasks are now `[DONE]`. User asked what's next;
chose to work through the 6 deferred CI-red regressions from this session's
earlier survey (see the numbered list above), leaving `SYS-W11-11`'s
version-number decision deliberately undecided and keeping `build-asan/` in
place. Progress, each its own commit:
- **#4 + #6** (`fc0fbd4`): `mc3togltf_large_obj_stress`'s Python harness now
  guards `import resource` behind `os.name != "nt"`; `mc3_json_document_budget`'s
  CTest `TIMEOUT` raised 30s→200s (measured ~50-55s Debug / ~96-100s
  ASan+UBSan, pre-existing, not a regression from tonight's work).
- **#5** was already fixed as a side effect of `SYS-W11-09`'s cyclic-leak
  cleanup (same file, same `.clear()` pattern).
- **#2** (`mc3_roundtrip`'s Windows "used by another process" on `remove()`):
  root cause is NOT a transient lock — 6 of the file's helper/test blocks
  opened an `std::ifstream` on the temp file to read its contents back, then
  called `std::filesystem::remove()` on the same path while that `ifstream`
  was still in scope (its destructor runs later, at the end of the
  enclosing block). POSIX allows unlinking an open file; Windows'
  `DeleteFile` does not without `FILE_SHARE_DELETE`, so this fails
  deterministically there, not flakily. Fixed by adding an explicit
  `.close()` immediately before each such `remove()`. Also added a shared
  `removeTestFile()` helper (bounded retry via the non-throwing
  `std::error_code` overload, matching `Mc3::writeFileAtomically`'s
  finalize-step precedent from `SYS-W9-06`) and swapped every bare
  `std::filesystem::remove(...)` call in the file to use it, as a backstop
  against any remaining external transient lock (AV/indexing) this
  diagnosis doesn't explain. Verified: `mc3_roundtrip` and the full
  non-render `mc3_*` suite (29 tests) pass clean under ASan+UBSan
  (`build-asan/`), no new leak/UB findings from the added `.close()` calls.
  Unverified on real Windows (no Wine in this sandbox).
- Remaining: **#3** (`mcb_load_policy` POSIX- vs Windows-style absolute-path
  message text) and **#1** (`mc3togltf.exe` `STATUS_DLL_NOT_FOUND`, needs
  CMake DLL-staging/install-rule changes).

## Known release blockers and decisions

| Area | Live state |
| --- | --- |
| Alternate graphics backends | `SYS-W8-05` is blocked on CNA owner coordination and actual backend evidence. |
| Android | `AUD-042` is blocked on NDK/tooling, sibling-runtime cross-build repair, packaging, and device/emulator testing. |
| Undo retention | `AUD-038` deliberately limits undo to 20 deep-copy entries; `SYS-W9-05` targets duplicate ownership, not unlimited history. |
| Unknown MC3 XML | Current policy is lossy handling of unknown elements/attributes; do not add a preservation bag without an approved compatibility contract. |
| New features | Do not add formats, importers, skinning, Draco/meshopt, or broad modelling tools before the active quality queue. |

## Immediate technical risks

- Existing CI now has Clang ASan+UBSan and bounded XML/JSON/MCB/GLB fuzz
  smoke coverage for standalone code. `Mc3`/`Mcb` CMake packages and a
  relocatable Linux CLI archive are locally qualified; Windows standalone
  qualification has a dedicated `windows-2022` job and successful local
  MinGW 14 builds, but awaits its first runner CTest/artifact evidence.

## Documentation truth

`SYS-W13-02` is complete. [`docs/CAPABILITY_MATRIX.md`](docs/CAPABILITY_MATRIX.md)
is the authoritative short matrix for format, editor, exporter, and platform
scope. `capability_documentation` statically checks the source-backed Walk
Mode proxy, Web IDBFS, autosave-recovery, and ordinary-UV claims. It excludes
volatile test totals and unverified platform-runtime assertions.

`SYS-W2-01` is complete. Lua executes against an isolated document copy with a
16 MiB VM allocator cap and the existing 50-million-instruction ceiling. Only
a fully validated result commits; the editor creates undo/history at that
commit boundary, so a failed script cannot leave a partial edit or false dirty
state.

`SYS-W12-03` is complete. Ordinary tinting uses existing permanent geometry
and effect state, authored UV buffers are persistent/bounded by mesh and
mapping inputs, and the 100-instance benchmark fixture proves a warm draw has
zero ordinary tint/authored-UV creations while reusing the UV cache.

`SYS-W12-04` is complete. The benchmark now splits CPU primitive mesh
generation, CSG cold evaluation/warm lookup, and cache-miss texture
decode/upload from frame time, with allocation/cache counters and no fragile
wall-clock gate. The UV fixture and a CSG fixture both exercised those lines.

`SYS-W3-03` is complete. `RegistryWorkspace` owns the registry database and
all Registry UI workflow state; `MeshCraftApplication` now has a single
workspace member and provides only explicit scene-insert/status callbacks.
The controller lifecycle is CNA-free tested and existing registry-data tests
continue to cover SQLite and no-SQLite behavior.

`SYS-W3-04` is complete. `AutomationWorkspace` owns the shared non-serialized
Scripts/Triggers/States/Events selection, Lua, and Preview/Play state, while
all four renderers use narrow callbacks rather than reaching into
`MeshCraftApplication`. Trigger execution snapshots its steps before a Lua
transaction can swap the document, so later steps remain valid.

`SYS-W9-05` is complete. Matching undo and automatic-history entries share a
single immutable document snapshot, while each restore still produces an
independent writable document. The 1,000-object test proves one retained graph
instead of two and covers selection/undo/history equivalence.

`SYS-W11-04` is complete. CI runs all four CNA-free standalone components
under Clang ASan+UBSan, then runs fixed-seed, bounded libFuzzer smoke tests for
XML, JSON, MCB and self-contained GLB import. The source corpora stay
immutable; generated MCB/GLB seeds and fuzzer working copies live in CI build
directories. The local Clang 19 smoke verification passed for all four input
classes; the existing GLB import CTest also passed.

`SYS-W11-05` is complete. The `release` install component exports
`MeshCraft::Mc3` and `MeshCraft::Mcb`, checks them through an external
`find_package` consumer CTest, and `meshcraft_cli_release` produces a
relocatable CLI `.tar.gz` from the same install rules. The archive includes
the two CLI tools, MC3/MCB headers and libraries, package configs, and the
non-system runtime libraries used by the glTF exporter.

`SYS-W11-06` is in progress. Its Windows CI job remains CNA-independent,
builds and CTests all four standalone components, verifies a fixed
MC3→MCB/GLB fixture before artifact upload, and publishes only the qualified
Windows CLI executables. The release-readiness changes are pushed to
`origin/develop`. This session reconfigured the local MinGW 14 tree with
`BUILD_TESTING=ON`,
confirmed its CTest registration uses the configured cross-test emulator, and
compiled the affected standalone targets. The sandbox blocks Wine itself with
`SIGSYS`, so it cannot provide runtime evidence. The native Linux
`cli_cross_platform_fixture`, package-consumer, capability-documentation, and
plan-consistency CTests passed. Freshly relinked XML, JSON, and MCB
load-policy tests plus the embedded-GLB and SVG safety export tests also
passed. The four path-confinement comparisons use `generic_string()` rather
than Windows-wide `path::native()` strings. Await the first GitHub
`windows-2022` CTest/artifact run before marking this task done.

## Practical commands

```bash
python3 test/validate_plan_consistency.py .
cmake -S . -B b-release -DBUILD_TESTING=ON
cmake --build b-release -j4 --target MeshCraft
ctest --test-dir b-release -LE render --output-on-failure -j4
```

Rendering tests depend on a usable display preflight; their registration or
availability is environment-dependent. Do not write a volatile “all N/N tests
pass” count into handoff documentation unless it was just verified in the same
build tree.

Standalone format/tooling builds are useful when CNA siblings are unavailable:

```bash
cmake -S mc3 -B mc3/build -DBUILD_TESTING=ON
cmake --build mc3/build -j4
ctest --test-dir mc3/build --output-on-failure -j4
cmake -S mcb -B mcb/build -DBUILD_TESTING=ON
cmake --build mcb/build -j4
ctest --test-dir mcb/build --output-on-failure -j4
```

Package and CLI-release verification from the established root build is:

```bash
ctest --test-dir cmake-build-debug -R '^package_consumer_smoke$' --output-on-failure
cmake --build cmake-build-debug --target meshcraft_cli_release -j2
```

## Guardrails

- Preserve `Mc3Document` as the CNA-free canonical AST. Check every consumer
  before changing its public API.
- Keep independent editor/exporter geometry paths independent except for
  deliberate, documented sharing.
- Use four jobs or fewer for local builds/tests.
- Do not modify sibling repositories or push changes without explicit user
  authorization.
- Keep `plan.md` bounded: archive completed narratives instead of letting the
  active queue become a worklog.

## Verification caveat

This session configured the existing root Debug tree and freshly built the
focused standalone/path-confinement targets with `CCACHE_DISABLE=1`; its
configured sibling CNA and sharp-runtime revisions differ from the historical
verified SHAs and must not be treated as a full-editor qualification. The
normal compiler-cache location is read-only in this environment, and Wine is
blocked by sandbox `SIGSYS`, so neither a fresh full suite nor native Windows
runtime tests were completed here. That is not evidence of a source failure.
The next session should start by reviewing the first `standalone-windows`
GitHub Actions run for the current `develop` head; use its CTest/artifact
result to complete or diagnose `SYS-W11-06`.
