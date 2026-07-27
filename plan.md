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

This queue is currently empty: the standalone-Windows-qualification task that
occupied it is done (see the "Active roadmap" section below for its full
verification evidence). Every other tracked item is `[BLOCKED]` (external
dependency, sibling-repository owner coordination, or a user decision) or
`[DEFERRED]` (intentional) — including 2 real third-party bugs found while
verifying the Editor CI jobs this same pass, deliberately not fixed here
(sibling repository, outside this session's authorized scope; user chose to
report and defer rather than fix).

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

- **SYS-W11-06** `[DONE]` `P2` — Qualify standalone MC3/MCB/tooling builds and
  tests on a current Windows runner, independent of CNA editor backend
  blockers. The `windows-2022` CI job configures, builds, and CTests all four
  standalone components, verifies a fixed MC3→MCB/GLB SHA-256 fixture, then
  publishes the two Windows CLI executables. This also exposed and fixed all
  four narrow-string uses of `path::native()` in confinement checks;
  `generic_string()` now works on Windows-wide paths.
  **Amended acceptance criteria (2026-07-26 review) verified for real,
  2026-07-27**, against the actual artifact published by the first fully
  green `windows-2022` run — not just an in-tree CTest pass. Correction to
  this file's own prior belief: **Wine is not blocked in this sandbox** for
  plain console/test binaries (only the full GUI editor hits `SIGSYS`), so
  this was verified directly rather than deferred:
  - Downloaded `meshcraft-cli-windows` via `gh run download`, extracted into
    a scratch directory outside every build tree and outside `PATH`.
  - `sha256sum` on all 4 manifest entries matches `SHA256SUMS.txt` exactly
    (the manifest itself uses bare filenames rather than the `tool/tool.exe`
    subpaths the archive actually has — a cosmetic manifest nit, not a hash
    mismatch).
  - Both `mc3tomcb.exe --version`/`mc3togltf.exe --version` succeed under
    Wine with `PATH` forced to `/usr/bin:/bin` only (no build-tree fallback
    possible).
  - A real MC3→MCB and MC3→GLB conversion of `test/house.mc3.xml` from the
    extracted binaries reproduced the exact existing fixture hashes
    (`4157f107e277a4...`/`0ef25953c8bce5...`) byte-for-byte.
  - **First download attempt (pre-existing artifact) failed outright**:
    `mc3togltf.exe` wouldn't even start (`STATUS_DLL_NOT_FOUND` under the
    same Wine repro) — the published artifact had zero DLLs bundled, but
    `mc3togltf.exe` dynamically linked Manifold, tinyxml2, and the MinGW
    runtime itself. Root-caused and fixed by statically linking the entire
    standalone-Windows build (`-DBUILD_SHARED_LIBS=OFF` +
    `-static -static-libgcc -static-libstdc++` in `ci.yml`'s shared
    configure step) rather than trying to enumerate and stage every DLL —
    re-verified after the fix with the same repro, now passing every check
    above with zero DLL dependencies at all. See `NEXT.md` for the full
    diagnostic trail and a CMake gotcha this surfaced
    (`$<TARGET_RUNTIME_DLLS:...>` expanding to nothing broke the existing
    DLL-copy POST_BUILD command outright; fixed with the documented
    `$<IF:$<BOOL:...>,copy_if_different,true>` workaround).

- **SYS-W11-07** `[DEFERRED]` `P3` — Improve dependency reproducibility with
  immutable revisions or verified archives, third-party notice/SBOM, and an
  offline-cache release-build check. **2026-07-26 review note:** revisit
  deferring this once `SYS-W11-08` exists — a clean-room release artifact
  built from movable FetchContent tags is a weaker reproducibility claim than
  the artifact test alone suggests. Left `DEFERRED` here; changing that is a
  scope decision for the user, not made in this pass.

- **SYS-W11-08** `[DONE]` `P2` — Produced and tested clean-room CLI release
  artifacts, on Linux fully, on Windows partially (see below).
  Discovered along the way (neither tool had it at all — a real functional
  gap, not just a testing gap): **`--version` did not exist.**
  `mc3tomcb --version` printed the usage text and exited 1;
  `mc3togltf --version` treated `--version` as an input filename and failed
  with "input file not found". Added it to both (`MESHCRAFT_VERSION`, a new
  per-tool `target_compile_definitions` using each standalone project's own
  `PROJECT_VERSION` — not yet unified across projects; that is `SYS-W11-10`'s
  job). This directly affects `SYS-W11-06`'s own amended acceptance criteria,
  which already assumed `--version` worked.
  `cmake/CreateCliRelease.cmake` (used by both the `meshcraft_cli_release`
  target and the new test below) now additionally: bundles `LICENSE` and
  `THIRD_PARTY.md` into the archive, and writes a `sha256sum -c`-compatible
  `SHA256SUMS.txt` manifest covering every installed file. Found and fixed a
  real bug in the manifest generation itself while building it: `cmake
  --install` never wipes its destination, so a stale `SHA256SUMS.txt` left
  by a previous run of the script was picked up by the file-listing glob
  with its OLD hash, then silently overwritten by the new manifest —
  a self-referential mismatch on every second run. Fixed by removing any
  existing manifest before globbing.
  New `clean_room_cli_release_smoke` CTest (builds the archive fresh,
  extracts it into `build-dir/clean room café` — outside the build tree,
  with a space and a non-ASCII character in the name — then runs
  `test/clean_room_cli_release_test.py` against the extracted copy with
  `PATH` set to `/usr/bin:/bin` only, i.e. no build-tree directory in the
  environment at all) verifies: both notices and the manifest are present;
  every manifest entry's hash matches the actual extracted file; `--version`
  succeeds for both tools; a real MC3→MCB and MC3→GLB conversion from the
  extracted binaries is byte-identical to the existing in-tree fixture
  hashes; and removing `libmanifold`/`libtinyobjloader` from a second copy
  of the extracted tree makes `mc3togltf` fail cleanly (non-zero exit, not a
  hang) rather than silently succeeding or crashing with no diagnostic.
  Verified on both the root-project build and each tool's own standalone
  build (`mc3tomcb/build`, `mc3togltf/build`); all pass, and the pre-existing
  3 Blender/`numpy` failures in `mc3togltf`'s standalone suite are unrelated
  (same environment gap as `SYS-W9-06`/`SYS-W1-08`'s writeups).
  Added a "Stage release manifest and notices" step to the `windows-2022` CI
  job (SHA-256 manifest + notices bundled into the uploaded artifact, same
  shape as the Linux archive). At the time this task was originally done,
  `mc3togltf.exe`'s `STATUS_DLL_NOT_FOUND` failure was deliberately left for
  a separate pass; **that gap (and the Windows clean-room verification this
  task's own writeup called "unverified — no Wine") is now closed by
  `SYS-W11-06`'s later evidence** — Wine does work in this sandbox for
  console binaries, and the underlying DLL problem is fixed by statically
  linking the standalone Windows build. See `SYS-W11-06` for the full
  verification.

- **SYS-W11-09** `[IN_PROGRESS]` `P1` — Added first-party editor sanitizer CI, and it
  immediately found a real bug, confirming the whole point of doing this.
  Root `CMakeLists.txt` already had `-DMESHCRAFT_SANITIZE=ON` wired up, but
  `meshcraft_apply_sanitize()` was only ever called on the main `MeshCraft`
  binary — all 51 first-party editor test executables (every
  `add_executable(..._test ...)` in the root `CMakeLists.txt`) silently built
  *unsanitized* even with the flag on. Added the call to all 51, right after
  each target's existing `target_compile_features(... cxx_std_23)` line.
  **Toolchain finding:** building the full editor under Clang fails
  compiling CNA's own source (`CNA::Internal::JsonValue` used as an
  incomplete type in a `std::pair`/`std::vector` context) — unrelated to
  sanitizers, a plain compile error. The existing plain `editor` CI job
  avoids this by using GCC, not Clang; the new sanitizer job does too
  (`gcc`/`g++`, matching), rather than touching CNA source (out of bounds
  here regardless).
  Locally verified the full build (875 steps) and the complete non-render
  suite under GCC ASan+UBSan (`build-asan/`, `-DCMAKE_BUILD_TYPE=Debug`,
  pinned CNA `d0c21ee6`/sharp-runtime `5cdaafb2` — the same revisions the
  plain `editor` job uses). First run: 10 of 182 tests failed. Triaged every
  one before touching anything:
  - **1 real bug, fixed:** `EventPreviewRunner::execute()`
    (`include/MeshCraft/Editor/EventPreviewRunner.hpp`) captured `script`,
    an iterator into `working.scripts`, then called
    `scriptRunner_.run(script->second.source, working, scriptTarget)` --
    which atomically replaces `working` via move-assignment on success --
    and afterward read `script->second.source.empty()` again, a genuine
    heap-use-after-free ASan caught precisely. The very next lines in the
    same function already re-derive `scriptTarget` after the same call with
    a comment explicitly naming this exact hazard for stale pointers into
    the replaced document graph; this one instance of the same pattern was
    missed. Fixed by capturing the source string by value before the call.
  - **4 known-shape leaks, fixed:** `mc3_roundtrip`, `object_index`,
    `mc3_ai`, `mc3_commands` each deliberately construct a 2-cycle (or, in
    `mc3_commands`, also a 1-cycle self-reference) via `shared_ptr`
    `children` to prove a depth guard throws instead of crashing --
    unfreeable by construction, an accepted by-design LeakSanitizer finding
    for the fixture, not a product bug (the SYS-W1-05/06/07 convention this
    codebase already uses for that class of test). Fixed all 4 occurrences
    the same way: `.clear()` the cyclic `children` right after the
    assertion so the objects are actually freed, rather than a suppression.
  - **1 structural incompatibility, guarded out:** `package_consumer_smoke`
    proves a *plain* external consumer can `find_package()` the installed
    Mc3/Mcb libraries with no special flags -- impossible when those `.a`
    files are sanitizer-instrumented (the plain consumer never links
    `__asan_*`/`__ubsan_*`, so it fails to link). Guarded the test's own
    `add_test()` behind `if(NOT MESHCRAFT_SANITIZE)` in `CMakeLists.txt`,
    with a comment explaining why; registered and required in every other
    configuration.
  - **1 narrow third-party/system suppression:** the one EASYGL render
    smoke test (`smoke_test`, under Xvfb) reports leaks that all trace into
    `libasan.so` itself or an unknown stripped module -- Mesa/llvmpipe's
    software-rasterizer stack, not any first-party/CNA/sharp-runtime symbol
    anywhere in any of the 4 stacks. Confirmed `ASAN_OPTIONS=detect_leaks=0`
    makes it pass cleanly (no other finding hiding underneath); scoped that
    override to only this one CI step, not the job's leak detection overall.
  - **4 already-known, left alone:** the `mc3_roundtrip`/
    `mc3_json_document_budget`/3×`mc3togltf_*_blender_import` findings from
    tonight's earlier CI-red survey (a separate cyclic-graph leak already
    covered above, the pre-existing Debug-build timeout, and the
    missing-`numpy` Blender gap) are unaffected by this task and left for
    the already-agreed separate pass.
  Re-ran the full non-render suite after all fixes: 181/181 pass (down from
  182, `package_consumer_smoke` now correctly not registered in this
  configuration) except the 4 already-known failures above, confirmed
  unrelated. Re-verified the fixed test files build and pass in the
  *non*-sanitizer build too (`cmake-build-debug`, `mc3/build`) so none of
  these fixes regressed the normal configuration.
  New `editor-sanitizer` CI job (`.github/workflows/ci.yml`): checks out the
  same pinned CNA/sharp-runtime revisions as the plain `editor` job, builds
  with `MESHCRAFT_SANITIZE=ON` + EASYGL, runs `ctest -LE render`, then the
  one render smoke test with leak detection scoped off.
  **Note for a future session:** `build-asan/` (a stable, reusable directory
  per the top-level build-rules convention) is ~5.7 GB on disk after this
  verification; left in place for incremental reuse rather than deleted,
  since another session may want to re-verify against it.

  **Correction (2026-07-27, external review caught this):** the paragraph
  above originally claimed the new CI job builds with `gcc`/`g++` "matching"
  the plain `editor` job — false. The job used `clang`/`clang++` from its
  very first commit; only the *local* verification (`build-asan/`, the
  181/181 result above) actually used GCC. This is now fixed for real
  (`CC: gcc-14`/`CXX: g++-14` in `ci.yml`, not just corrected prose), but
  **downgrading this task's status to `[IN_PROGRESS]`**: the `editor-
  sanitizer` CI job itself has never had a fully green run on real CI —
  every run through 2026-07-27 failed, first on unrelated environment gaps
  (fixed separately, see the CI-red session log in `NEXT.md`), now on
  `SYS-W8-08`'s `ShaderEffect` mismatch (originally misattributed to
  `sharp-runtime`; actually CNA — see `SYS-W8-08`'s own entry). **Confirmed
  on the very next CI run after the GCC switch**: configure now succeeds
  with no Clang/CNA incompatibility at all, and the build failed on the
  exact same `ShaderEffect::setWorldProperty`/etc. errors as the plain
  `editor` job's `VULKAN` entry — consistent, compiler-independent evidence
  that this wasn't a toolchain issue. `SYS-W8-08` is now fixed (CNA pin
  bumped) and confirmed gone on real CI, but a *different* CNA-internal
  build failure took its place, specific to this sanitizer job — see
  `SYS-W8-09`. This job remains `[IN_PROGRESS]` until that's resolved too.
  The code fixes described
  above (heap-use-after-free, 4 leak fixtures, `package_consumer_smoke`
  guard, `meshcraft_apply_sanitize()` wiring) are real and independently
  verified locally; only the CI-job-itself claim was wrong. Re-promote to
  `[DONE]` once a real `editor-sanitizer` run is green.

- **SYS-W11-10** `[DONE]` `P2` — Wired one authoritative version source: a
  new root-level `VERSION` file (currently `0.1.0`, the existing value —
  deliberately not changed; deciding the actual first-release version number
  is a product decision, not made here, see `SYS-W11-11` below), read via
  `file(STRINGS ...)` before each of the 5 `project(... VERSION ...)` calls
  (root, `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` — each reads
  `../VERSION` relative to its own `CMAKE_CURRENT_SOURCE_DIR`, which resolves
  correctly whether built standalone or as a root subdirectory).
  Found and fixed a real, previously-undetected drift risk while wiring
  this: `mcb/cmake/McbConfig.cmake.in` hardcoded
  `find_dependency(Mc3 0.1.0 CONFIG)` — a second, independent copy of the
  version number that `configure_package_config_file()` never touched, so
  it would have silently gone stale the next time the version changed. Now
  substitutes `@PROJECT_VERSION@`. Also updated
  `test/cmake/package_consumer/CMakeLists.txt` (an external-consumer smoke
  test that would otherwise hardcode its own pin, same drift risk) to read
  the same shared file. `--version` (both CLI tools and the main editor),
  package configs, and the CLI release archive name all confirmed to
  correctly reflect the shared source with no further changes needed — they
  already read `${PROJECT_VERSION}`/`MESHCRAFT_VERSION` derived from it.
  `CHANGELOG.md`'s existing `## [Unreleased] — 0.1.0` heading already
  matches; left alone (prose, not a second machine-read source, so it
  cannot drift the same way).
  Verified: all 5 `project()` calls configure cleanly (standalone `mc3`/
  `mcb`/`mc3togltf`/`mc3tomcb` and the root project), `mc3tomcb --version`/
  `mc3togltf --version`/`MeshCraft --version` all report `0.1.0`,
  `package_consumer_smoke` and `clean_room_cli_release_smoke` both still
  pass end to end, and the release archive is still named
  `MeshCraft-0.1.0-Linux-cli.tar.gz`.
  **Deliberately not done here — a product decision, not mechanical
  plumbing:** deciding whether the first public release is `0.1.0`, `1.0.0`,
  or another version, and actually executing an RC process end to end. See
  the new `SYS-W11-11` below.

- **SYS-W11-11** `[BLOCKED]` `P2` — Execute the first release candidate,
  once the version-number decision below is made. Produce an RC artifact
  set from the now-unified version source (`SYS-W11-10`), execute
  `RELEASE.md`'s checklist for real, record the exact supported
  platform/backend matrix, and distinguish unsupported platforms from
  temporarily blocked qualification.
  **Blocked on a decision only the user can make:** is the first public
  release `0.1.0` (honest about remaining platform/qualification gaps —
  `SYS-W8-05`, `SYS-W8-06`, `AUD-042`, `SYS-W11-06`'s Windows evidence, this
  session's own left-for-later CI-red survey) or `1.0.0` (implying those
  gaps are closed first)? Bumping the root `VERSION` file once decided
  updates all 5 projects, `--version`, package configs, and archive names
  in one place (`SYS-W11-10`); nothing else needs to change.

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

- **SYS-W3-05** `[DONE]` `P2` — Split `EditorAlgorithms.hpp` (2,514 lines,
  CNA-free) into 7 cohesive headers under `include/MeshCraft/`, grouped by
  verified cross-reference rather than blind guessing: `EditorCommandAlgorithms.hpp`
  (857 lines — rename/duplicate/group/ungroup/convert-to-definition/
  break-instance, F9/F10 reference cleanup, undo-stack cap, coordinate/rotation
  normalization; also absorbs the two tree helpers `findParentListAlg`/
  `removeFromListAlg`, moved here from the originally-proposed Selection group
  once grep showed every one of their 10 call sites is a Commands- or
  Transform-group mutator, never Selection), `EditorSelectionAlgorithms.hpp`
  (326 — select-children, lock-aware dry-run, STAB-0503 ray-cast picking,
  hierarchy filter), `EditorTransformAlgorithms.hpp` (363 — align/scatter/
  falloff/vertex-snap/rotate-drag/group-scale/camera presets; depends on
  Commands for `findParentListAlg`+`deepCopyObjectAlg` via
  `scatterAlongCurveAlg`), `EditorPersistenceAlgorithms.hpp` (479 — autosave/
  backup/merge/Save-As/export-selection-or-template-or-material/drag-drop/
  recent-files; depends on Commands for `deepCopyObjectAlg` via
  `exportSelectionAlg`), `EditorEventAlgorithms.hpp` (308 — AI panel
  lifecycle, keyframe insertion, SYS-W3-01 Phase 5 anim-override eval/blend;
  depends on Utility for `resolveObjectPropertyValueAlg` via
  `insertAnimKeyframesAlg`), `EditorPreferencesAlgorithms.hpp` (238 —
  keybind/prefs/macro persistence formats; the one group with zero Mc3
  dependency at all), `EditorUtilityAlgorithms.hpp` (108 — live
  object-property resolution, material-color fallback). Every function/struct
  body is byte-identical to the original (verified with a line-range diff
  against each new file before deleting it — only intentional blank-line
  reformatting differs); 2,679 total lines vs. 2,514 (+165, entirely the 7x
  duplicated `#pragma once`/doc-comment/`#include` preamble, not duplicated
  logic).
  Fan-out evidence: of the 30 files the initial grep flagged, 5 were
  comment-only false positives needing no change (`ObjectTypeName.hpp`,
  `ObjectIndex.cpp`, `AiResponseAlgorithms.hpp`, `registry_insert_undo_test.cpp`,
  `MeshCraftPrivate.hpp` — each only *mentions* `EditorAlgorithms.hpp` in a
  comment, never `#include`s it) and `Macro.cpp`'s include was entirely dead
  (zero symbols used) and removed with no replacement. Of the 25 genuine
  consumers: 14 need exactly 1 new header, 8 need 2, 1 (`coordinate_system_test`)
  needs 3, and 1 (`Commands.cpp`, the largest consumer) needs 4 — so 60% of
  real consumers now pull one narrow ~100–860 line module instead of the
  former 2,514-line monolith, and even the heaviest consumer pulls at most
  4 of 7. No facade was left behind; `include/MeshCraft/EditorAlgorithms.hpp`
  is deleted.
  Compiler-driven verification: the by-call-site symbol grep against every
  candidate header was accurate on the first try — `MeshCraft` (30 sources)
  built with zero missing-include errors, as did all 13 affected test
  targets (`coordinate_system_test`, `texture_from_path_test`,
  `animation_preview_algorithms_test`, `delete_reference_integrity_test`,
  `registry_insert_undo_test`, `macro_recorder_test`, `undo_manager_test`,
  `scene_history_test`, `automation_workspace_test`, `camera_bookmarks_test`,
  `lua_script_runner_test`, `trigger_fire_test`, `event_preview_runner_test`).
  `ctest -LE render`: 181 tests, 163 passed. Failures are exactly the two
  pre-authorized, unrelated exceptions (`mc3_json_document_budget` Debug-build
  timeout; the 3 `mc3togltf_*blender_import` tests, blocked on this sandbox's
  Blender lacking `numpy`) plus 14 `mcb_*`/`mc3togltf_*` tests reported
  "Not Run" solely because their binaries had never been built in this reused
  `cmake-build-debug` — spot-built 3 of them (`mc3_invalid_utf8`,
  `mcb_corruption`, `mc3togltf_csg_null_child`) and all passed unchanged with
  no other recompilation triggered, confirming this is a pre-existing
  build-directory gap (none of the 14 consume any `Editor*Algorithms.hpp`
  header), not a regression; left unbuilt per this task's explicit
  mc3togltf/mc3tomcb-out-of-scope instruction. Zero real test regressions.

- **SYS-W3-06** `[PROPOSED]` `P2` — A single-source-of-truth "primitive
  descriptor" registry (display name, tessellation clamp, MCB enum-count
  bound) for each `ObjectType`/`PrimitiveType`, replacing today's ~4
  independently-duplicated switch/magic-number sites. Found 2026-07-27 while
  researching how much code a brand-new primitive type touches: adding one
  today requires edits across ~25 sites in 8 subsystems (enum/struct, XML/
  JSON/MCB parser+writer, 2 independent mesh tessellators — the exporter's
  `MeshBuilder.cpp` and the live-viewport's `SceneRenderer_Builders.cpp` —
  CSG, 6+ editor-UI surfaces, and docs). This is not a hypothetical
  concern: `include/MeshCraft/Editor/ObjectTypeName.hpp`'s own header
  comment documents a real incident where two divergent copies of
  `objectTypeName()` both silently mishandled 5 primitive types (Torus,
  Capsule, Disk, Grid, IcoSphere), so macro record/replay silently turned a
  Torus into a Box. **Deliberately scoped:** a registry for the
  serialization-adjacent *metadata* (name/clamp/enum-bound) would eliminate
  that class of bug; it would NOT and should not try to unify the two
  independent mesh-tessellation implementations, which genuinely differ per
  rendering backend and would need a much larger, separately-justified
  refactor to merge. Not started — needs the usual per-task confirmation
  before implementation.

- **SYS-W14-41** `[PROPOSED]` `P3` — `MenuBar::drawFileImportObj`/
  `drawFileImportGlb` (`src/MeshCraft/Application/UI/MenuBar.cpp:651-656`)
  open a dialog with a plain ImGui text buffer for the file path
  (`importObjDialogBuf_`/`importGlbDialogBuf_`) — no native OS file-picker
  "Browse..." button, unlike the material-texture Browse flow elsewhere in
  the editor (AUD-era F9's native-dialog integration). OS-level drag-and-
  drop (`SDL_EVENT_DROP_FILE`) already works as the low-friction path; this
  task is only about the fallback dialog for when drag-drop isn't
  convenient (e.g. the file isn't visible in an open file-manager window).
  Low-risk, UI-only change — reuse the existing native-dialog helper rather
  than adding a new file-picker dependency. Not started.

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
  and passed. **Update (2026-07-27): native Windows overwrite qualification
  is no longer unverified** — Wine does run console binaries in this
  sandbox after all, and a real MinGW+Wine repro of `mc3_atomic_write`
  passed cleanly (see `SYS-W11-06`'s later evidence), plus this primitive's
  finalize-retry budget was separately widened after a real Windows CI run
  hit a slower-than-expected transient lock (unrelated Unicode-path bug,
  also fixed — see `NEXT.md`).
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

- **SYS-W8-07** `[BLOCKED]` `P2` — `sharp-runtime`'s own source fails to
  compile under the Editor (VULKAN) and Editor (EASYGL) (non-sanitizer) CI
  jobs: `sharp-runtime/src/System/Environment.cpp:307` ignores `chdir()`'s
  return value, which `-Werror=unused-result` (enabled by these jobs'
  compiler flags) turns into a hard build failure. Confirmed via a real
  2026-07-27 CI run, after fixing 5 unrelated CI-environment gaps in this
  same session let the Editor jobs progress this far for the first time
  (missing X11/XTEST/FFmpeg dev packages, and missing `easy-gl`/`meta-gl`
  sibling checkouts — all fixed in `.github/workflows/ci.yml`, see `NEXT.md`
  for the full trail). **Blocked: the fix is one line inside
  `sharp-runtime`'s own source** (check `chdir()`'s return value, or cast to
  `void` if truly don't-care), a sibling repository outside this session's
  authorized scope (`CLAUDE.md`: no changes to CNA/sharp-runtime without
  owner permission) — user explicitly chose to report and defer, not fix,
  when asked 2026-07-27.

- **SYS-W8-08** `[DONE]` `P2` — The Editor (EASYGL) ASan+UBSan CI job failed
  to build for a different reason than `SYS-W8-07` above: mesh-craft's own
  `src/MeshCraft/Renderer/SceneRenderer.cpp` (multiple call sites, e.g. line
  875) calls `ShaderEffect::setWorldProperty`/`setViewProperty`/
  `setProjectionProperty`/`SetTexture`, surfaced for the first time by the
  same 2026-07-27 CI run as `SYS-W8-07`.
  **Corrected misdiagnosis:** the initial writeup blamed `sharp-runtime` for
  this (matching the `Microsoft::Xna::Framework` namespace convention) — that
  was wrong. `ShaderEffect` is actually implemented in **CNA**
  (`cna/include/Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp`), not
  `sharp-runtime`. Confirmed with `git show <ref>:<path>` directly against
  the CI-pinned CNA commit (`d0c21ee613ebd8f8b0055f7868103dd5c5c30fc7`): none
  of the 4 methods existed there at all. Further confirmed that pin was
  **not even an ancestor of CNA's current `develop` branch**
  (`git merge-base --is-ancestor d0c21ee6 <current-HEAD>` → false) — its
  history was rewritten after that commit was recorded (the same class of
  issue `SYS-W13-03`'s AUD-064..073 hash-orphaning finding hit earlier this
  project). The methods were added by CNA's own Task 1079 (commit
  `b08c7aa88f6ccbdf9afae3a25d9bd25bb9620f4d`, "wire ShaderEffect into
  GraphicsDevice's 3D draw path", 2026-07-16) — well after the stale pin's
  2026-07-11 recording.
  **Fix:** bumped the CI-pinned CNA revision (both editor jobs in
  `.github/workflows/ci.yml`) from `d0c21ee6` to CNA's current `develop` tip
  (`ac3aaaeb2a5ba27dbd9e22e782c7041e6e40947c`) — a mesh-craft-side CI
  configuration change, not a CNA source edit, so within this session's
  authorized scope even under the no-CNA-changes-without-permission rule.
  User explicitly confirmed this pin bump before it was made. Verified
  locally before pushing: full `MeshCraft` editor rebuilt clean against this
  CNA revision (already the sandbox's own local `../cna` checkout), and the
  non-render suite passed 176/180 (the 4 failures are the already-known
  unrelated Blender/`numpy` gap). **Confirmed on the real CI run right
  after pushing**: the `ShaderEffect` error is completely gone from both
  `Editor (EASYGL) ASan+UBSan` and `Editor (VULKAN)` — this task is
  genuinely done, not just locally verified.

- **SYS-W8-09** `[BLOCKED]` `P2` — Bumping the CNA pin for `SYS-W8-08` above
  unmasked a *different* CNA-internal build failure, only on
  `Editor (EASYGL) ASan+UBSan`:
  `cna/include/CNA/Internal/Xnb/DecimalDateTimeContentTypeReaders.hpp:32`
  calls `ContentReader::ReadDecimal()`, which the runner's compiler rejects
  as not existing. **Deliberately not chased further — this is not
  locally reproducible despite genuinely trying:** an isolated single-file
  compile, a full `CNA` target rebuild under the exact same flags
  (`MESHCRAFT_SANITIZE=ON`, `g++-14`, same CNA revision), and a
  ccache-cleared from-scratch rebuild of that target all succeeded cleanly
  in this sandbox. The method call itself is real (`DecimalReader::Read()`
  in the same header, guarded `#if !defined(_MSC_VER)`, matching this
  GCC-only job), so this looks like a genuine GCC-version-sensitive
  difference between this sandbox's `g++-14` and the GitHub Actions
  `ubuntu-24.04` runner's `g++-14` (or some other runner-specific factor
  this session couldn't isolate) rather than a simple missing-method bug.
  `Editor (VULKAN)`/`Editor (EASYGL)` (non-sanitizer) don't hit this at
  all — they still fail earlier, on `SYS-W8-07`'s unrelated `chdir()`
  issue. Needs a future session with either CI-side debug instrumentation
  (print the exact GCC version, dump preprocessed output as a build
  artifact) or owner-side CNA investigation — deliberately not guessed at
  further here.

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
