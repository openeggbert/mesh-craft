# MeshCraft — Active Plan

_Last updated: 2026-07-26._ This is the bounded, authoritative work queue.
Detailed evidence for completed work and prior audit sessions is archived in
[`docs/history/`](docs/history/); it is useful provenance, not live planning.

## Current product state

MeshCraft is a pre-production C++23 MC3 scene editor with standalone MC3/MCB
libraries and MC3→MCB / MC3→glTF tooling. It already covers MC3 XML/JSON,
MCB, glTF/GLB export, validation, history, registries and local asset packs,
CSG, authored LOD/materials/UVs, animation clips, Lua scripts, triggers,
states, event bindings, and automated tests. The codebase is approximately
76,800 C/C++ lines.

The stabilization and `SYS-W14-28`…`SYS-W14-39` roadmaps are complete. The
remaining inherited work is intentionally small:

- `SYS-W8-05` is blocked on CNA backend qualification and owner coordination.
- `AUD-042` is blocked on an Android toolchain, sibling-runtime repair, and a
  real device/emulator.
- `AUD-038` is a conscious design limit: undo is a bounded 20-entry,
  whole-document deep-copy stack.

The next release is about semantic correctness, predictable mutation,
performance, truthful documentation, and portability—not widening the feature
surface with further formats, importers, skinning, Draco/meshopt, or broad new
modelling tools.

## Status and priority

- `TODO` — authorized and not started.
- `IN_PROGRESS` — currently being implemented.
- `DONE` — completed; durable evidence is in Git and the archive.
- `BLOCKED` — external dependency or owner action is required.
- `DEFERRED` — intentionally not scheduled.

`P0` is data loss, invalid output, crash, or unsafe-input risk. `P1` is a
wrong semantic result, untruthful capability, or critical test gap. `P2` is
architecture, distribution, or measurable performance work. `P3` is polish.

## Release 1.0 gates

1. The editor and exporters must agree on every supported MC3 transform
   convention, including rotation units and Euler order.
2. Failed script/event execution must not partially mutate a document.
3. Stable rendering of ordinary geometry must not create routine tint or
   authored-UV GPU buffers per frame.
4. User-facing capability documentation must match source and tests.
5. CNA-independent format/tooling code must pass sanitizer and bounded fuzz
   qualification in CI.
6. Package installation, Windows standalone qualification, reproducible
   release artifacts, and Android/alternate-backend work must have either
   evidence or an explicit release-scope decision.

## Priority execution queue

The user authorized this queue on 2026-07-26. Work proceeds one task at a
time; re-evaluate scope and blockers before starting each item.

1. **SYS-W11-06** `P2` — standalone Windows qualification. **In progress:**
   the release-readiness changes are published on `develop`; await the first
   native `windows-2022` CI CTest/artifact run before accepting the release
   gate. **Amended acceptance (2026-07-26 review):** a green in-tree CTest run
   alone does not prove the two published `.exe` files are a usable
   standalone artifact. Before DONE, additionally: copy or download the
   staged artifact into a clean directory outside all build trees and outside
   `PATH`; run both executables with `--version` from there; perform one
   MC3→MCB and one MC3→GLB conversion and verify the fixed fixture SHA-256;
   confirm every third-party runtime DLL the two executables need (the root
   `CMakeLists.txt` allows Manifold/tinyobjloader as shared runtime libraries)
   is staged alongside them.

---

## Active roadmap

### W13 — Project truth and release readiness

- **SYS-W13-01** `[DONE]` `P0` — Restore this file as a bounded active plan.
  The 2,743-line prior plan was copied verbatim to
  [`docs/history/plan_20260726.md`](docs/history/plan_20260726.md); no
  historical evidence was discarded. `plan.md` now retains only current state,
  release gates, blockers, and the executable queue; `NEXT.md` is the compact
  handoff. The plan-consistency check continues to protect task identity and
  queue state.

- **SYS-W13-02** `[DONE]` `P1` — Reconciled `README.md`, `NEXT.md`,
  `missing.md`, `MC3_FORMAT.md`, and `TESTING.md` with source anchors.
  [`docs/CAPABILITY_MATRIX.md`](docs/CAPABILITY_MATRIX.md) is now the concise
  format/editor/exporter/platform authority. The Walk Mode proxy, Web IDBFS,
  autosave recovery, and ordinary-UV claims are covered by the bounded
  `capability_documentation` lint test; volatile CTest totals are no longer
  recorded as product truth.

- **SYS-W13-03** `[DONE]` `P1` — Removed 4 verified documentation-drift
  claims (not the full external-review list — two of its claims did not
  hold up on direct read: the "click/timer event" gap it named is
  `SYS-W14-11`'s pre-existing JSON-only scope, already accurate, and
  `RELEASE.md`'s "95 tests" mention is already self-caveated as historical,
  not asserted as current — left both alone):
  - `CHANGELOG.md` no longer claims automatic trigger/state-switching events
    are unimplemented; credits `SYS-W14-40` (Event Preview/Play) for
    automatic timer/Area/click dispatch.
  - `missing.md` no longer self-contradicts on `coordinate_system` — removed
    the stale "not read anywhere... by design" clause that contradicted its
    own Summary table two sections earlier.
  - `RELEASE.md`'s docs checklist no longer references the retired
    emoji-marker (`✅`/`🟡`/`🧪`/`📋`/`🔴`) summary-table format; points at
    `test/validate_plan_consistency.py` instead, which is the real check.
  - `new.md` gained a status note crediting `SYS-W14-40` for the "event
    bindings for Areas/triggers" feature it recommends, so the recommendation
    reads as historical rather than a live gap despite the file's date.
  Extended `test/validate_capability_documentation.py` with 6 source-backed
  assertions covering all four (2 of the 4 needed both a `forbid` the stale
  text is gone and a `require` the replacement is present). Caught and fixed
  2 vacuous checks along the way — both `forbid` needles for CHANGELOG.md
  and `missing.md` failed to match the actual pre-fix text on the first
  attempt (a missing 2-space Markdown continuation indent, and a spurious
  literal `\n` where the source had a space), which would have made the
  check pass regardless of whether the fix was ever applied; verified each
  needle against `git show HEAD:<file>` before trusting it. All 6 new checks
  plus the pre-existing checks pass.

### W5 — MC3 governance

- **SYS-W5-06** `[DONE]` `P1` — The CNA-free rotation-convention helper is
  shared by `mc3togltf` and the live editor. Rendering/CSG, hierarchy- and
  pivot-aware picking, gizmos, object/world transforms, cameras, Walk Mode and
  animated transforms honor `rotation_units` and all six Euler orders. Scene
  Properties exposes both declarations and a static degrees/XYZ normalizer;
  it refuses animated Euler channels rather than make a lossy motion change.
  Regression tests cover radians, every order, nesting, pivots, cameras and
  animation hand-off; the EASYGL editor and focused tests built successfully.

- **SYS-W5-07** `[DEFERRED]` `P2` — Revisit forward-compatibility policy only
  when a concrete compatibility contract is requested. The current accepted
  policy remains documented lossy handling of unknown XML elements/attributes;
  do not add an extension bag without deciding between strict rejection,
  preservation, and lossiness.

### W1 — Validation and diagnostics

- **SYS-W1-08** `[DONE]` `P1` — Added validation-capturing JSON load APIs.
  `Mc3JsonParser` gained the same `g_validation`/`reportWarning`/`reportError`/
  `reportErrorDoc` thread_local pattern `Mc3XmlParser.cpp` already used
  (`Mc3JsonParser.cpp`), wired into: per-field tessellation clamps
  (`clampTess`, now threading the *owning object's* id/name down into
  `toPrimitive`/`toCrossSection`/`toPath`/`toExtrude` — JSON nests these
  fields under sub-objects with no identity of their own, unlike XML's flat
  attributes), `customPoints`/`points` count-cap rejections, every
  `DocumentBudget::charge*()` budget-exceeded rejection, the document
  byte-budget rejection, resource-path confinement rejections, and a new
  "unknown object type" warning (JSON silently defaults an unrecognized
  `type` to `group` — reported now, though still not rejected outright the
  way XML drops the object entirely; this task is diagnostic parity, not
  behavioral parity, and changing JSON's accept-vs-drop semantics is out of
  scope here). `parse()`/`parseString()` each now install their own
  `ValidationScope` + real-vs-synthetic source-file tag (refactored the
  shared parsing body into an internal `buildDocumentFromJson()` so `parse()`
  no longer delegates to `parseString()` and stomps its source-file tag with
  a synthetic in-memory path). Added `Mc3Document::loadFromJsonFile()`/
  `loadFromJsonString()`/`loadFromLibraryFile()`/`loadFromLibraryJsonFile()`
  `Mc3Validation&` overloads (the last two for BOTH formats — XML's library
  loader had no validation overload either, not just JSON's) and updated
  `loadSceneFileDispatched()` (`FileOps.cpp`) so all four load paths populate
  the editor's validation history consistently.
  New `mc3_json_validation` differentially compares JSON against
  `Mc3XmlParser`'s existing diagnostics for the same tessellation-clamp value
  (identical `suggestedRepair`), plus invalid enum, document budget, resource
  confinement, library identity, a clean document (zero entries), and the
  no-validation-argument non-regression case. All 27 standalone `mc3` tests
  passed, as did the full root `MeshCraft` editor target and the
  registry/undo tests exercising `Mc3Document` load paths transitively.
  **Performance note (found and fixed during this task, not a regression
  from it):** computing an object's id/name identity unconditionally on
  every parsed object — regardless of whether a validation sink was even
  active — added measurable per-object overhead; made it conditional on
  `g_validation != nullptr` so the overwhelmingly common no-validation case
  pays nothing for it. That fix chased what turned out to be a red herring:
  confirmed via `git stash` isolation that `mc3_json_document_budget`'s
  ~150K-object stress scenario already took ~50-55s on the UNMODIFIED,
  pre-this-task code under this repo's `Debug` (`-O0`) build (vs. ~8-10s
  under a `Release`-flagged standalone build) — this is the SAME
  `mc3_json_document_budget ... ***Timeout 30.05 sec` failure already
  present in the "Clang ASan+UBSan and bounded fuzz" CI job (which configures
  `-DCMAKE_BUILD_TYPE=Debug`) from tonight's CI-red survey, not something
  this task introduced or fixed. Left alone per the standing decision to
  leave the general CI-red regressions for a separate pass; the per-object
  laziness fix above is kept anyway since it's a correct, free micro-
  optimization, just not the explanation for the timeout.

### W2 — AI / import sandbox

- **SYS-W2-01** `[DONE]` `P1` — Lua scripts now run on a deep, isolated
  document copy, re-resolve an optional target by a unique stable object ID,
  apply whole-document validation, and commit only after success. The 16 MiB
  Lua allocator cap complements the existing 50-million-instruction limit.
  The editor takes undo/history only at the commit boundary and refreshes
  selection after the swap; failed scripts leave document, selection, dirty
  state and undo/history untouched. Focused runner/trigger tests cover partial
  mutation followed by error, loops, excess allocation, non-finite transforms,
  invalid material references, placement and trigger-driven execution.

- **SYS-W2-06** `[DONE]` `P1` — Made the Lua allocator budget accounting
  exact. `budgetedLuaAllocator()` always treated `oldSize` as the size of a
  previously-owned block, even when `pointer == nullptr` (a genuinely new
  allocation), for which Lua's `lua_Alloc` contract instead passes an
  object-kind tag (confirmed against the vendored Lua 5.4 source: `luaC_newobj`
  → `luaM_malloc_(L, size, tag)` → `frealloc(ud, NULL, tag, size)`, versus
  `luaM_realloc_`/`luaM_free_`'s `lua_assert((osize == 0) == (block == NULL))`,
  which holds only for a real resize/free of an existing block). Subtracting
  that tag from `budget.allocated` on every new allocation silently eroded the
  tracked total below real usage across many small allocations. Moved the
  allocator out of `LuaScriptRunner.cpp`'s anonymous namespace into a new
  header-only `include/MeshCraft/Editor/LuaMemoryBudget.hpp` (no other call
  site changes needed; `LuaScriptRunner.cpp` is compiled directly into 4
  different test targets, not linked as a shared library) and made it branch
  on `pointer == nullptr` before computing `retained`.
  New `test/lua_memory_budget_test.cpp` (`lua_memory_budget`, header-only, no
  Lua/sol2 dependency) calls the allocator directly with hand-crafted
  call sequences matching each of `luaM_malloc_`/`luaM_realloc_`/`luaM_free_`'s
  exact `(pointer, oldSize, newSize)` shapes — deterministic and independent
  of Lua's own GC scheduling, unlike probing this through real script
  execution. Covers many small held table/string-shaped allocations (exact
  tracked-total equality after each), table growth, grow/shrink
  reallocations, collection followed by reallocation reclaiming real
  headroom, failed-allocation/failed-resize rollback leaving the tracked
  total untouched, and a successful transaction at exactly the 16 MiB limit
  plus rejection at limit+1. Verified discriminating: temporarily reverting
  just the `pointer == nullptr` branch reproduces 4 failing assertions
  (the exact-equality checks after many small held allocations, and the
  15 MiB-held-before-collection check), confirming the fix — not just the new
  test file's presence — is what the tests depend on. `lua_memory_budget`,
  `lua_script_runner`, `automation_workspace`, `trigger_fire`, and
  `event_preview_runner` (the 4 targets that compile `LuaScriptRunner.cpp`)
  all built and passed after the move.

### W12 — Performance baselines

- **SYS-W12-03** `[DONE]` `P1` — Ordinary tinting now uses effect/material
  state on the permanent normal/UV buffer; the defensive VPC-only fallback is
  bounded and persistent. Authored-UV buffers are cached by immutable mesh
  identity, projection/mapping values, and geometry scale. `--benchmark`
  reports creation/hit counts for its last warm draw; the 100-instance
  `benchmark_editor` fixture requires zero tint/authored-UV creations and an
  authored-UV cache hit. The focused CTest, UV projection, bloom/emissive, and
  point/spot-gizmo screenshot regressions passed under EasyGL/Xvfb.

- **SYS-W12-04** `[DONE]` `P2` — `--benchmark` now measures direct CPU
  primitive mesh generation, CSG cold evaluation and warm-cache lookup, and
  cache-miss texture decode/rasterization plus GPU upload separately from
  first/warm frame time. It also prints mesh/triangle, CSG cache/failure, and
  texture hit/miss/upload counts. The headless 100-instance UV fixture and a
  real CSG fixture both passed; the former remains an allocation/cache-reuse
  regression check and neither test relies on a machine-specific time limit.
  The local baseline is recorded in `test/BENCHMARK_BASELINE.md`.

### W11 — Build / CI / developer experience

- **SYS-W11-04** `[DONE]` `P1` — CI now configures, builds, and CTests each
  CNA-independent component (`mc3`, `mcb`, `mc3togltf`, `mc3tomcb`) with Clang
  ASan+UBSan and ccache. Corpus-seeded libFuzzer smoke targets cover XML,
  JSON, MCB, and self-contained GLB import, each with a 20-second total-time,
  5-second per-input, 512 MiB RSS, and 1 MiB-input ceiling. MCB and GLB seeds
  are generated from checked-in fixtures, while source corpora remain
  immutable; sanitizer/fuzzer errors fail CI. The GLB importer also exposes a
  byte-input route, so fuzzing does not write hostile files. Local Clang 19
  builds and five-second smoke runs passed for all four formats; the existing
  GLB import CTest passed. An EASYGL editor sanitizer job remains intentionally
  deferred until its dependencies are reliable under sanitizers.

- **SYS-W11-05** `[DONE]` `P2` — `Mc3` and `Mcb` now install headers, static
  libraries, version-compatible `Mc3Config.cmake`/`McbConfig.cmake`, and
  namespaced `MeshCraft::Mc3`/`MeshCraft::Mcb` imported targets. The Mc3
  package bundles the concrete `tinyxml2` static dependency; Mcb locates Mc3
  and zlib through its config. `package_consumer_smoke` installs only the
  `release` component, configures a separate CMake project with
  `find_package`, links both libraries, and executes an MCB round trip.
  `meshcraft_cli_release` depends only on `mc3tomcb`/`mc3togltf`, installs
  their runtime requirements plus the package surface, and writes a
  relocatable `.tar.gz` archive. Local Debug verification passed the package
  smoke plus MC3/MCB and CLI round trips; the installed glTF CLI resolves its
  bundled Manifold runtime through `$ORIGIN/../lib`.

- **SYS-W11-06** `[IN_PROGRESS]` `P2` — Qualify standalone MC3/MCB/tooling builds and
  tests on a current Windows runner, independent of CNA editor backend
  blockers. The new `windows-2022` CI job configures, builds, and CTests all
  four standalone components, verifies a fixed MC3→MCB/GLB SHA-256 fixture,
  then publishes the two Windows CLI executables. Local MinGW 14 cross builds
  compiled the standalone targets, and their CTest registrations correctly
  use the configured emulator. This also exposed and fixed all four
  narrow-string uses of `path::native()` in confinement checks;
  `generic_string()` now works on Windows-wide paths. The local sandbox blocks
  Wine itself with `SIGSYS`, so it cannot supply runtime evidence. The
  release-readiness changes are now pushed to `origin/develop`; await and
  review the first GitHub Windows CTest/artifact run before marking this task
  done.

- **SYS-W11-07** `[DEFERRED]` `P3` — Improve dependency reproducibility with
  immutable revisions or verified archives, third-party notice/SBOM, and an
  offline-cache release-build check. **2026-07-26 review note:** revisit
  deferring this once `SYS-W11-08` exists — a clean-room release artifact
  built from movable FetchContent tags is a weaker reproducibility claim than
  the artifact test alone suggests. Left `DEFERRED` here; changing that is a
  scope decision for the user, not made in this pass.

- **SYS-W11-08** `[PROPOSED]` `P2` — Produce and test clean-room CLI release
  artifacts. Replace the current bare-executable-upload pattern (the
  `windows-2022` job publishes only `mc3tomcb.exe`/`mc3togltf.exe`, no
  runtime libraries or manifest) with one staged install tree containing both
  CLI programs, their required runtime libraries, notices, licenses, package
  metadata, and a manifest of SHA-256 hashes. Test the Linux and Windows
  artifacts after copying them outside the build directory with build paths
  removed from the environment. **Tests:** `--version`, MC3→MCB, MC3→GLB,
  deterministic fixture hashes, missing-runtime detection, and archive
  extraction into a path containing spaces and non-ASCII characters.

- **SYS-W11-09** `[PROPOSED]` `P1` — Add first-party editor sanitizer CI.
  `SYS-W11-04`'s sanitizer/fuzz CI covers `mc3`, `mcb`, `mc3togltf`, and
  `mc3tomcb`; the editor's own `src/`+`include/` (35,682 lines, confirmed via
  `find src include \( -name '*.cpp' -o -name '*.hpp' \) | xargs wc -l`) is
  not in a regular sanitizer job — `SYS-W11-04` explicitly deferred the EASYGL
  editor sanitizer job pending reliable dependencies. Root `CMakeLists.txt`
  already has `-DMESHCRAFT_SANITIZE=ON` wired up (just not MSVC). Use it with
  pinned CNA/sharp-runtime revisions: start with the CNA-free and non-render
  editor tests, then add the smallest reliable EASYGL smoke/render subset.
  **Acceptance:** sanitizer findings fail CI, no source test is silently
  disabled because it was built through the editor root, and the selected
  partition completes with fixed time and memory limits.

- **SYS-W11-10** `[PROPOSED]` `P2` — Define and execute the first release
  candidate process. Decide whether the first public release is `0.1.0`,
  `1.0.0`, or another version; use one authoritative version source for the
  root project, Mc3, Mcb, both CLI tools, package configs, `--version`,
  archive names and the changelog. Produce an RC artifact set, execute
  `RELEASE.md`, record the exact supported platform/backend matrix, and
  distinguish unsupported platforms from temporarily blocked qualification.

### W3 — Architecture decomposition

- **SYS-W3-03** `[DONE]` `P2` — `RegistryWorkspace` now owns the registry
  database, filters, cached results, AI-save lifecycle, save buffers, and
  asset-pack export state. It reduced 20 direct registry fields in
  `MeshCraftApplication` to one workspace member. The workspace's ImGui frame
  receives explicit insertion/status callbacks; the application still owns
  undo, scene-instance placement, import refresh, and notifications. The new
  CNA-free `registry_workspace` test covers AI dialog ownership/lifecycle,
  while the existing insertion/SQLite/no-SQLite tests preserve registry data
  behavior. No application-wide context object was introduced.

- **SYS-W3-04** `[DONE]` `P3` — `AutomationWorkspace` owns the four resource
  tab selections, Lua runner, Preview/Play lifecycle/reports, and scene-state
  mutation semantics. Scripts, Triggers, States, and Events are all rendered
  in `AutomationWorkspaceUi.cpp` through explicit undo, selection, lookup,
  playback, and notification callbacks; no application-wide context was added.
  `LeftPanel.cpp` is reduced from 2,808 to 2,203 lines. The CNA-free
  `automation_workspace` test locks workspace lifecycle/state application;
  `trigger_fire` additionally proves a post-Lua trigger step remains safe
  after the atomic document swap.

- **SYS-W3-05** `[PROPOSED]` `P2` — Decompose `EditorAlgorithms.hpp`.
  Confirmed at 2,514 lines, combining unrelated persistence, selection,
  transform, command, event, preferences and utility algorithms behind one
  header. Split into cohesive CNA-free modules and move non-template
  implementation to `.cpp` files where practical; preserve existing tested
  APIs or migrate call sites mechanically. Measure clean-build time and
  incremental-rebuild fan-out before and after — do not accept a cosmetic
  split that keeps one transitive mega-header.

### W9 — Undo and data-loss

- **SYS-W9-05** `[DONE]` `P2` — Automatic history and exact undo now retain
  one `shared_ptr<const Mc3Document>` frozen from a single deep copy. Each
  undo/redo/history restore still creates a fresh writable deep copy before
  editor mutation, with selection restoration and existing bounds unchanged.
  The 1,000-object unit fixture verifies shared ownership, independent
  restoration, and 2,098,753 logical retained bytes versus the prior
  4,197,506 duplicate-graph model; it also records an informational attach
  time without a fragile wall-clock threshold. Undo/redo/history and registry
  insertion regressions passed.

- **SYS-W9-06** `[DONE]` `P0` — Added one portable atomic-file replacement
  primitive, `MeshCraft::Mc3::writeFileAtomically()` +
  `uniqueSiblingTempPath()` (`mc3/include/MeshCraft/Mc3/Mc3AtomicFileWriter.hpp`,
  `mc3/src/Mc3AtomicFileWriter.cpp`), and replaced the four independently
  duplicated fixed-`<destination>.tmp`-name write-then-rename call sites
  (`Mc3XmlWriter.cpp`, `Mc3JsonWriter.cpp`, `McbWriter.cpp`,
  `GltfExporter.cpp`'s GLB path) with calls to it. Each candidate temp name
  combines a monotonic atomic counter with a steady-clock reading, so
  concurrent saves of the same destination never collide, and a caller
  requesting a fresh name automatically skips past any stale leftover temp
  file rather than failing. The finalizing `rename()` retries up to 5 times
  (20ms apart) to absorb a transient Windows sharing violation; on
  irrecoverable finalize failure it throws `AtomicFinalizeError` (distinct
  from a writer-stage exception, which propagates unchanged) and always
  removes the temp file first, leaving any pre-existing destination
  untouched. New `mc3/test/atomic_write_test.cpp` (registered as
  `mc3_atomic_write`) covers first save, overwrite-existing, a Unicode
  destination path, injected writer failure (with and without a pre-existing
  destination), a genuine finalize-stage failure (renaming onto an existing
  directory, which throws `AtomicFinalizeError` specifically), a stale
  leftover `<dest>.tmp` file not blocking a new save, and two concurrent
  calls producing distinct temp names. All 26 standalone `mc3` tests, all 11
  standalone `mcb` tests, and 75/78 standalone `mc3togltf` tests passed (the
  remaining 3 — `mc3togltf_blender_import` and its two PBR/release-sample
  variants — fail only on a pre-existing, unrelated environment gap: this
  sandbox's Blender lacks the `numpy` module its glTF importer needs; not a
  regression from this change). The full root-project `MeshCraft` editor
  target, plus the registry/undo/obj-export-cleanup tests that exercise
  `Mc3Document::saveToFile()`/registry save paths transitively, also built
  and passed. Native Windows overwrite qualification remains unverified in
  this sandbox (no Wine); left for `SYS-W11-06`'s own Windows CI evidence.
  Editor-side direct-write config files (preferences/keybindings/macros/
  recent-files) were surveyed and found to have the same unprotected-
  direct-write shape, but are intentionally left out of this task's scope —
  regenerable config is a materially lower-severity gap than scene-file data
  loss, and folding them in here would have widened one coherent task into
  an unrelated sweep across the editor.

- **SYS-W9-07** `[DONE]` `P1` — Added crash recovery for never-saved
  ("Untitled") documents. `performUntitledRecoverySave()` writes to one
  bounded slot, `untitledRecoveryPath()` (`MeshCraftPrivate.hpp`,
  `meshcraftConfigDir() / "untitled.recovery.mc3.xml"`), on its own
  `autoSaveTickAlg` countdown (`autoSaveUntitledCountdown_`, `hasCurrentFile`
  inverted) — reuses the existing tested tick function unchanged rather than
  touching `performAutoSave()`'s own tested "no current file never
  auto-saves" gating. `checkForUntitledRecovery()` runs once at startup right
  after a fresh `newScene()` (`Application.cpp`'s `LoadContent()`, the
  no-file-argument branch) and offers a new modal dialog
  ("Recover Unsaved Scene", `Overlays.cpp`) with Recover / Discard / Not Now.
  `recoverUntitledScene()` keeps `currentFile_` empty, sets `modified_ =
  true`, and never calls `addRecentFile()`. The recovery file is removed on:
  successful Save As (both the plain dialog and Save-as-Library, since either
  can be an untitled document's first save), explicit Discard, and ordinary
  (non-crash) shutdown (`~MeshCraftApplication()`, unconditional whenever
  `currentFile_` is still empty — there is no quit-confirmation gate in this
  app, so reaching a clean exit while modified already means the user chose
  not to save); a real crash skips the destructor, which is what leaves the
  file for the next startup to find.
  New `mc3_untitled_recovery` (CNA-free, mirrors `mc3_autosave_recovery`'s
  own established "App-level methods aren't headlessly testable, the
  filesystem+Mc3 mechanism they reduce to is" scope) covers: a real
  document's content round-tripping through the recovery path, discard's
  remove()-succeeds postcondition, a crash-marker simulation (a file left
  behind is detectable), a corrupt recovery file throwing catchably instead
  of crashing, and coexistence with the existing named-file `.autosave`
  sibling (disjoint paths, independent content, verified together in one
  temp directory). The App-level flow itself (dialog wiring, destructor
  timing, `currentFile_`/`modified_` transitions) was verified by full editor
  build success and code review only, not a live screenshot — same
  CNA-coupled-and-not-headlessly-testable class as this session's F20/F21
  precedent, not a lower bar invented for this task.

### W14 — Bounded new work

- **SYS-W14-40** `[DONE]` `P2` — Added explicit bounded Event Preview/Play.
  It alone dispatches timer bindings, Walk Mode Area enter/exit transitions,
  and viewport-picked object clicks; ordinary editing remains inert. The
  CNA-free `EventPreviewRunner` retains enabled/cooldown/one-shot/32-dispatch
  eligibility and computes Area bounds through the shared nested-transform,
  pivot, rotation-unit, and Euler-order path. Trigger steps, states, and Lua
  scripts run on one deep isolated document copy, validate as a batch, and
  publish through one undo/history boundary. A failed script restores runtime
  eligibility, leaves document/selection/dirty/undo/history untouched, and
  suppresses staged action/audio effects. `event_preview_runner` covers Area,
  timer, click, state, successful commit, invalid state, and failed-script
  rollback; the EasyGL Debug editor built successfully.

### Inherited external blockers

- **SYS-W8-05** `[BLOCKED]` `P1` — Broad alternate-backend editor
  qualification requires real backend screenshot/device evidence and CNA owner
  coordination; it is not a backend-name switch.

- **SYS-W8-06** `[PROPOSED]` `P1` — Web editor and IDBFS end-to-end
  qualification, split out from `SYS-W8-05` because it has its own specific
  blocker and persistence contract rather than a generic backend-name gap.
  Confirmed: the Emscripten pre-JS mount/syncfs IDBFS implementation exists
  and is linked (`-lidbfs.js`), but `docs/CAPABILITY_MATRIX.md` and
  `README.md` both already record that real persistence and editor usability
  are unverified because the CNA web resize failure prevents a stable
  session — this task tracks removing that caveat with evidence, not
  re-implementing IDBFS. **Unblock requirements:** a CNA revision that
  survives initial resize; successful Emscripten configure/build; headless-
  browser editor startup; a persisted preference across reload; recovery-file
  persistence; and one GLB export/download smoke test.

---

## Audit-derived carryovers

Net across all 2 AUD-### rows in this active plan: 0 DONE, 0 TODO, 1 DEFERRED,
1 BLOCKED. Completed audit evidence remains in the archives.

### AUD-038 `[DEFERRED]` `P3` `W9` · Bounded whole-document undo history
- **Component:** `UndoManager` and the editor history flow.
- **Evidence:** Undo intentionally keeps 20 whole-document deep-copy entries;
  oldest snapshots are evicted. This is a known memory/retention trade-off,
  not a currently unbounded growth bug.
- **Outcome:** Retain the explicit product limit. `SYS-W9-05` may later reduce
  duplicate ownership without changing user-visible restore semantics.

### AUD-042 `[BLOCKED]` `P2` `W8` · Android qualification
- **Component:** Android CMake/Gradle route, CNA EASYGL backend, and sibling
  `sharp-runtime`.
- **Evidence:** The source path selects GLES/EASYGL, but an Android NDK,
  repaired sibling-runtime cross-build, packaging environment, and device or
  emulator evidence are unavailable in this checkout.
- **Outcome:** Do not claim Android release readiness until configure, build,
  package, and device/emulator smoke verification are reproducible.

## Verification and scope notes

- Run `python3 test/validate_plan_consistency.py .` after planning changes.
- Keep local build/test parallelism at four jobs or fewer.
- Do not change sibling `cna` or `sharp-runtime` repositories here.
- Do not change the public `Mc3Document` API without checking the editor,
  `mc3togltf`, `mc3tomcb`, and all format fixtures.
- A fresh full build was not independently reproduced for this review: the
  editor needs sibling repositories and standalone FetchContent could not
  reach its network dependency. That is an environment limitation, not a
  source-code failure claim.
