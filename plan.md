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

- **SYS-W13-03** `[PROPOSED]` `P1` — Remove documentation drift the
  `capability_documentation` lint does not yet cover, confirmed by direct
  read (not the full review list — two of its claims did not hold up: the
  "click/timer event" gap it named is `SYS-W14-11`'s pre-existing JSON-only
  scope, already accurate, and `RELEASE.md`'s "95 tests" mention is already
  self-caveated as historical, not asserted as current):
  - `CHANGELOG.md:58-59` still states "Automatic collision/click/timer
    trigger events and automatic state switching remain unimplemented,"
    which `SYS-W14-40` (`[DONE]`, Event Preview/Play) has since superseded.
  - `missing.md` self-contradicts on `coordinate_system`: line 53 and the
    matrix at line 177 say it is fully implemented and honored in the editor
    and glTF export, while line 148 still says it is "not read anywhere
    (`coordinate_system`, by design)".
  - `RELEASE.md`'s docs-checklist references a `plan.md` "summary table"
    recomputed from `✅`/`🟡`/`🧪`/`📋`/`🔴` row markers; `plan.md` no longer
    uses that emoji-marker format (`SYS-W13-01` moved it to
    `[DONE]`/`[IN_PROGRESS]`/`[BLOCKED]`/`[DEFERRED]` text markers), so that
    checklist step no longer describes a real check.
  - `new.md` (headed "prepared 2026-07-26 from the current source tree")
    recommends "event bindings for Areas/triggers" as a next feature; that is
    what `SYS-W14-40` already shipped, so the recommendation is stale despite
    the file's date.
  Extend the documentation validator with a small number of source-backed
  negative assertions covering these four, rather than trying to make every
  prose sentence a brittle static test.

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

- **SYS-W1-08** `[PROPOSED]` `P1` — Add validation-capturing JSON load APIs.
  `FileOps.cpp:179-182` documents that `Mc3JsonParser` has no
  `Mc3Validation`-capturing overload, so the `.json` and `.mc3lib.json`
  branches of `loadSceneFileDispatched()` silently return empty validation
  (not populated, not an error) while the XML and MCB branches populate it.
  Add `Mc3Validation&` overloads for JSON string/file loading and for
  `.mc3lib.json`, routing parser clamps, defaults and hard rejections through
  the same structured diagnostic surface XML and MCB already use, then update
  `loadSceneFileDispatched()` so all four load paths populate the editor
  validation history consistently. **Tests:** differential XML/JSON fixtures
  for non-finite values, clamps, missing references, invalid enums, excessive
  limits, library identity, hard rejection, and clean valid documents.

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

- **SYS-W2-06** `[PROPOSED]` `P1` — Make the Lua allocator budget accounting
  exact. `budgetedLuaAllocator()` (`LuaScriptRunner.cpp:33-48`) always treats
  `oldSize` as the size of a previously-owned block:
  `retained = oldSize <= budget.allocated ? budget.allocated - oldSize : 0`.
  Per the Lua 5.4 `lua_Alloc` contract, when `pointer == nullptr` (a genuinely
  new allocation) `oldSize` does not encode a real prior block size — it
  encodes an object-kind tag. The current code still subtracts that tag value
  from `budget.allocated` on every new allocation, so the aggregate budget
  silently drifts low across many small allocations instead of tracking real
  usage. Distinguish the new-allocation case from a resize before interpreting
  `oldSize`, and prove peak Lua-owned memory cannot exceed the configured
  16 MiB budget under realistic allocation patterns. **Tests:** one oversized
  allocation (existing), many small tables, many short strings, table growth,
  grow/shrink reallocations, collection followed by reallocation, failed
  allocation rollback, and a successful transaction immediately below the
  limit.

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

- **SYS-W9-06** `[PROPOSED]` `P0` — Introduce one portable atomic-file
  replacement primitive. `AUDIT-0019` (commit `cec267b`) already made
  `Mc3XmlWriter`, `Mc3JsonWriter`, `McbWriter`, and the GLB path in
  `GltfExporter` write-then-rename instead of writing the destination
  directly; confirmed identical in all four
  (`mc3/src/Mc3XmlWriter.cpp:933-949`, `mc3/src/Mc3JsonWriter.cpp:658-677`,
  `mcb/src/McbWriter.cpp:747-770`, `mc3togltf/src/GltfExporter.cpp:2304-2320`).
  The remaining gap is not "atomic vs. not" but portability and
  collision-safety of that shared pattern, independently duplicated four
  times: a fixed `<destination>.tmp` name can collide between two concurrent
  saves of the same path (e.g. autosave racing a manual save); and
  `std::filesystem::rename()` replacing an *existing* destination is not
  proven equally reliable on Windows as on POSIX in this codebase — the
  existing Windows CI job (`SYS-W11-06`) does not exercise a second save to
  the same path. Replace the four duplicated call sites with one CNA-free
  utility that: creates a unique sibling temporary file; never collides with
  another concurrent save; fully closes and flushes the temporary output
  before replacement; replaces an existing destination correctly on Linux and
  Windows; preserves the old destination if writing or replacement fails;
  removes temporary files after every handled failure; and returns a
  diagnostic distinguishing write failure from finalization failure.
  **Tests:** first save, overwrite existing destination, Unicode path,
  injected writer failure, injected replacement failure, pre-existing
  temporary file, two distinct concurrent temporary names, and a native
  Windows XML/JSON/MCB/GLB overwrite qualification.

- **SYS-W9-07** `[PROPOSED]` `P1` — Recover never-saved Untitled scenes.
  Confirmed at `FileOps.cpp:93`: `performAutoSave()` opens with
  `if (currentFile_.empty()) return;`, so a document that has never been
  saved once (new scene, worked on, never given a path via Save As) has no
  autosave and no recovery path if the editor or system crashes. The existing
  `.autosave` sibling-file recovery (`SYS-W9-02`) only covers documents that
  already have a path. Add a bounded session-recovery file in the MeshCraft
  configuration directory for a modified document that has never been saved.
  On next start, offer an explicit Recover / Discard decision. Recovery must
  keep the document untitled and modified, must not add a synthetic path to
  Recent Files, and must not overwrite an unrelated session; successful
  Save As or explicit discard removes the recovery entry. **Tests:** modified
  untitled recovery, clean shutdown cleanup, crash-marker simulation,
  successful Save As cleanup, discard, corrupt recovery file, and coexistence
  with the existing sibling `.autosave` recovery.

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
