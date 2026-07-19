# NEXT.md

_Last updated: 2026-07-19, after fixing task 1 of the previous session's
"Next smallest tasks" list (the AI Assistant thread-creation-failure
wedge). The 2026-07-18 session ran a fresh independent adversarial audit
and fixed 10 of its 12 findings; this file was fully rewritten (not
appended to) at that point — its previous revision had grown to 1072 lines
of session-by-session narrative; see `git log -- NEXT.md` and
`docs/history/` if that history is ever needed._

## 1. Project summary

**MeshCraft** is a desktop 3D scene editor (C++23, built on the CNA game
framework — an XNA/FNA-style API over SDL3 + OpenGL ES) for **MC3**, this
project's own scene/model format. A scene is a document (`Mc3Document`) of
primitives, CSG operations, materials, lights, cameras, animation, and
several extension namespaces (scripts, sounds/music, triggers, scene
states). The same in-memory AST has two serialization surfaces: `.mc3.xml`
(original) and `.mc3.json` (semantic JSON, not a mechanical XML mirror). A
separate binary format, `.mcb`, and a glTF/GLB exporter (`mc3togltf`)
round out the format family.

**Current phase:** the original stabilization backlog (AUD/SYS tasks from
a 2026-07-11 audit) is substantively complete and archived
(`docs/history/STABILIZATION.md`, `docs/history/plan_20260718.md`). The
2026-07-18 session ran a **second, independent, fresh adversarial audit**
(four parallel review agents: build/test verification, core-code bug
hunt, architecture/docs staleness, UX gaps) that found 12 new, previously-
undocumented findings. 10 are fixed and merged (`AUD-064` through
`AUD-073`, skipping the deliberately-not-fixed `AUD-069`); 3 of the
audit's own findings remain open (see §8) plus one separately-noted,
now-fixed gap (the AI Assistant thread-creation-failure wedge, fixed
2026-07-19 — see §3). The project is in an **ongoing hardening /
bug-fixing** phase, not active new-feature development, though scoped new
features have
landed before when explicitly requested (`SYS-W14-##` rows).

**Important architectural decisions:**
- `Mc3Document` (in `mc3/`) is the single canonical in-memory AST. It has
  no CNA/GUI dependency and is shared by both parsers/writers, `mcb`,
  `mc3togltf`, `mc3tomcb`, and the editor.
- The editor (`MeshCraftApplication`/`SceneRenderer`) and the exporter
  (`mc3togltf`) are **two independent geometry generators** reading the
  same `Mc3Document` — no shared mesh-building code. Triangle winding
  differs deliberately (export CCW-from-outside, editor preview
  CW-from-outside) — do not "fix" one to match the other.
- `../cna` and `../sharp-runtime` are **sibling repositories, not part of
  this repo, and not to be modified from here** — a separate
  process/owner handles them. This repo only consumes them via
  `add_subdirectory`.
- A second, unrelated sibling repository, **`../mesh-world`**, drives its
  own feature work directly into this repo's `mc3/` library from time to
  time — commits can land here that this repo's own `plan.md` never
  asked for. Check `git log` at the start of any session.

## 2. Current status

- **Build: clean**, last verified this session at commit `63df1bc` (fresh
  `cmake --build b-release -j4`, zero errors/warnings, EASYGL backend on
  Linux — the only backend buildable in this environment).
- **Tests: 141/141 `ctest` passing**, last verified this session at the
  same commit (`ctest -j4`). Same total as before — this session's fix
  added a new case inside the existing `mc3_ai` binary rather than a new
  `ctest`-registered target, so the registered-test count didn't move.
- **CLI/tools/apps/libraries currently available:**
  - `MeshCraft` — the interactive editor (`./b-release/MeshCraft
    scene.mc3.xml`, or `--screenshot out.png` / `--export out.glb` /
    `--benchmark` for headless one-shot runs).
  - `mc3togltf` — MC3 → glTF/GLB exporter (`--stats` prints diagnostics).
  - `mc3tomcb` — MC3 XML → MCB binary converter.
  - Standalone libraries `mc3` (format/AST + XML/JSON parse-writer),
    `mcb` (binary format) — both buildable and testable without CNA via
    their own `mc3/build`/`mcb/build` trees (no live GPU/GL needed).
- **Recently implemented (this session, 2026-07-19):** task 1 from the
  2026-07-18 session's "Next smallest tasks" list — the AI Assistant
  thread-creation-failure wedge. `AiAssistant::sendAsync()`
  (`src/MeshCraft/AiAssistant.cpp`) used to set `pending_` and increment
  the process-wide in-flight counter *before* constructing the worker
  `std::thread`; if that construction itself threw (a real
  `std::system_error` from `pthread_create()` under thread/resource
  exhaustion), neither was ever rolled back, so `isInFlight()` would
  report `true` forever and the Send button would stay disabled
  permanently with no visible error. Fixed by wrapping the thread
  construction in `try`/`catch` and rolling back both plus surfacing
  `hasError_`/`errorMsg_` on failure. Regression test (`mc3/test/ai_test.cpp`,
  Linux-only) forces a **real** `std::thread` constructor failure via
  `fork()` + `RLIMIT_NPROC=0` scoped to the child process only (zero effect
  on the parent process or anything else on this shared machine); verified
  failing against the pre-fix code and passing against the fix via
  `git stash`, matching this project's established pattern.
- **Recently implemented (previous session, 2026-07-18):** 10 fixes from a
  fresh audit, each with a regression test, each verified both broken
  (via `git stash` of the one-line/few-line fix) and fixed:
  - `AUD-064` — unbounded `<grid>` `subdivisions_x * subdivisions_z`
    froze the editor every frame (`SceneRenderer_Extrude.cpp`).
  - `AUD-065` — `.mc3.json` tessellation fields had none of the XML
    path's clamps (`Mc3JsonParser.cpp`).
  - `AUD-066` — MCB reader `.reserve()`'d a claimed collection count in
    full before validating the stream contained that many elements
    (`McbReader.cpp`, ~824MB VmPeak from a 43-byte hostile file).
  - `AUD-067` — degenerate zero-length Polyline extrude tangent divided
    by zero into NaN, aborting the whole glTF export (`MeshBuilder.cpp`).
  - `AUD-068` — `.mc3.json` load path ignored `Mc3LoadPolicy` entirely,
    so `confineResourcePathsToRoot` was a no-op (`Mc3JsonParser.cpp`).
  - `AUD-070` — CDATA sections in saved XML broke out early on an
    embedded `]]>`, corrupting the document structure (`Mc3XmlWriter.cpp`).
  - `AUD-071` — two MCB fields silently dropped on a tag mismatch instead
    of being rejected (`McbReader.cpp`).
  - `AUD-072` — same class of bug as `AUD-064`, in `drawExtrudeDynamic()`.
  - `AUD-073` — hollow extrude cross-section divided by zero when
    `radius=0` (`SceneRenderer_Extrude.cpp`).
  - Plus: confirmed/documented an unrelated, external `../easy-gl`/
    `../meta-gl` build blocker resolved itself (§4).
  - `AUD-069` was **found, filed, deliberately NOT fixed** (low severity,
    fails safe — see §5).
- **Known working examples:** `./b-release/MeshCraft test/house.mc3.xml`;
  `--screenshot out.png` (real PNG); `--benchmark` (in-process timing);
  `./b-release/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb`.
- **What does not work yet / needs re-verification (not checked this
  session — see README.md's own platform table for the last-recorded
  detail, dated 2026-07-07/09, before re-trusting it):**
  - Web (Emscripten): last known blocked by a `../sharp-runtime`
    regression, separate from an earlier `../cna` crash.
  - Windows (MinGW): last known blocked by a CNA-side header gap +
    `../sharp-runtime` `-Werror` failures; the two CNA-free CLI tools
    (`mc3togltf.exe`, `mc3tomcb.exe`) were last confirmed to build fine.
  - CI: present and believed correct (`.github_/workflows/ci.yml`) but
    parked under a non-standard directory name, owner-gated (`AUD-052`).

## 3. Recent changes

**This session (2026-07-19):**
1. Read `NEXT.md` in full, confirmed with the user, then implemented
   exactly task 1 from §8's "Next smallest tasks" (per this project's
   `CLAUDE.md`/resume-prompt workflow: one confirmed task per session).
2. Commit `63df1bc` — `fix(ai-assistant): roll back sendAsync() state on
   worker thread-creation failure`. Files modified: `src/MeshCraft/
   AiAssistant.cpp` (try/catch around the worker `std::thread`
   construction; factored the scope guard's decrement+notify into a new
   `endAiWorker()` shared by both the normal-completion and failure
   paths). File extended: `mc3/test/ai_test.cpp` (new
   `testThreadCreationFailureRollsBackState()`, Linux-only, forces a real
   `std::thread` constructor failure via `fork()` + child-scoped
   `RLIMIT_NPROC=0`).
3. Verified: the new test fails against the pre-fix code and passes
   against the fix (`git stash`); `ctest -R mc3_ai` passes; full root
   `ctest -j4` is 141/141 (same total — no new `ctest`-registered target,
   just a new case inside the existing `mc3_ai` binary).
4. This finding was tracked only in `NEXT.md` (never filed as an `AUD-0NN`
   row in `plan.md`, since it was found outside the 2026-07-18 audit's
   formal findings list) — so `plan.md` is unchanged this session; only
   `NEXT.md` needed updating.

**Previous session (2026-07-18), in order:**
1. Ran a fresh, independent 4-agent audit (build/test verification,
   core-code bug hunt, architecture/docs staleness, UX gaps) rather than
   trusting the prior session's "backlog exhausted" claim — found 12 new
   findings plus several stale docs.
2. User picked findings to fix one at a time, in severity order. Each of
   the 10 commits below is a `fix(AUD-0NN): ...` + matching
   `docs(AUD-0NN): ...` pair — see `git log --oneline` for the exact
   list, or `plan.md`'s `AUD-064`..`073` rows for full evidence/fix
   writeups with file:line citations and verification commands.
3. Files added: `test/grid_stress.mc3.xml`, `test/extrude_stress.mc3.xml`,
   `mc3/test/json_input_budget_test.cpp`,
   `mc3/test/json_load_policy_test.cpp`, `mcb/test/reserve_bomb_test.cpp`,
   `mcb/test/tag_mismatch_rejection_test.cpp`,
   `mc3/test/cdata_injection_test.cpp`,
   `test/extrude_hollow_zero_radius_test.cpp` — one new regression test
   per fix, each independently confirmed to fail against the pre-fix code
   (via `git stash`) before confirming it passes against the fix.
4. Files modified (production code): `src/MeshCraft/Renderer/
   SceneRenderer_Extrude.cpp` (3 separate fixes — `AUD-064`/`072`/`073`),
   `mc3/src/Mc3JsonParser.cpp` (2 fixes — `AUD-065`/`068`),
   `mcb/src/McbReader.cpp` (2 fixes — `AUD-066`/`071`),
   `mc3togltf/src/MeshBuilder.cpp` (`AUD-067`),
   `mc3/src/Mc3XmlWriter.cpp` (`AUD-070`).
5. `plan.md`: added `AUD-064` through `AUD-073` (10 rows, all `DONE`) and
   `AUD-069` (`TODO`, filed not fixed) to the active backlog table.
6. Confirmed and documented (no code change) that an unrelated,
   in-progress `../easy-gl`/`../meta-gl` edit briefly broke the
   CNA-linked build mid-session; re-checked on request and confirmed
   resolved.

**Prior sessions:** see `docs/history/plan_20260718.md` and
`docs/history/STABILIZATION.md` for the full original stabilization
backlog (650+ STAB tasks, then a 57-finding audit, all archived DONE).

## 4. Current blocker / main problem

**No build-breaking blocker at present.** Build and tests are both green
as of the last verification this session (§2).

The closest thing to a standing blocker is **owner-gated, not a bug**:
- `AUD-052`/`SYS-W11-01` — CI (`.github_/workflows/ci.yml`) is
  permanently parked under a directory GitHub Actions won't pick up
  (trailing underscore), needs a workflow-scoped push token nobody in
  this environment has. This blocks `AUD-053` (editor CI job) and the
  CI-job half of `AUD-057` (sibling-repo pin enforcement — the
  configure-time-assertion half already landed).
- Nothing else is currently blocking forward progress; the next work is
  simply the remaining audit findings (§8), which are unblocked.

## 5. Known bugs and limitations

- **`AUD-069` (confirmed, low severity, TODO):** `includePathWithinRoot()`
  (duplicated identically in `Mc3XmlParser.cpp` and `Mc3JsonParser.cpp`)
  wrongly rejects a same-directory relative resource reference (e.g.
  `meshSource="model.obj"`) as "escaping the root" when the target file
  does not exist on disk AND the document was opened via a bare relative
  filename (empty `sourceDir`) — `weakly_canonical()` only resolves
  paths that exist. **Fails safe** (a false rejection, not a bypass), so
  low severity. See `plan.md`'s `AUD-069` row for the fix direction.
- **No-op undo snapshots on locked objects (found, not yet filed/fixed):**
  several `MeshCraftApplication_Commands.cpp` commands (`deleteSelected`,
  `dropSelectedToGroundPlane`, `groupScaleSelected`,
  `randomizeTransformSelected`, `resetPivot`) call `pushUndo()`
  unconditionally before checking per-object locks inside their loop —
  locking every selected object and invoking the command still consumes
  an undo slot and marks the document modified, even though nothing
  changed. `findReplaceNames()` already shows the correct dry-run-first
  pattern to copy.
- **Latent CSG null-deref (found, not yet filed/fixed, unreachable
  today):** `mc3togltf/src/CsgEvaluator.cpp:304-311`'s nested
  `Intersection` case dereferences `obj.children[0]` without the
  `if (child)` guard every other child in the same loop uses. No current
  parser path produces a null child, so this is a latent, not live, risk.
- **Stale documentation (found, not yet fixed):**
  - `AI_TRUNCATION_BUG.md` (repo root) describes a bug that is fully
    fixed (configurable `maxTokens` slider, `wasTruncated()` check,
    `stop_reason` parsing all exist) — should be archived, not left at
    root looking open.
  - `render.md` — its own P1/P2 proposals are already implemented; P3-P6
    remain open. Needs a status update or archival + backlog entries.
  - `README.md:254` — still claims `--screenshot` "always writes PPM
    regardless of file extension"; `SYS-W14-03` made `.png` paths write
    real PNG months ago. README also never mentions `--benchmark`.
  - `missing.md` — 6 of its "still open" items were already fixed by
    same-day commits after its last edit; needs a fresh pass.
- **Web/Windows build status: needs re-verification**, not checked this
  session — see §2's caveat and README.md's own platform table.
- **`SYS-W3-01` (in progress, not a bug):** `MeshCraftApplication` god
  object, 7 subsystems extracted so far (`KeybindingManager`,
  `Preferences`, `MacroRecorder`, `UndoManager`, animation-override
  computation, `WalkController`, `AudioPreview`). File dialogs and
  post-processing were investigated and explicitly declined as further
  extraction targets (see `plan.md`).
- **By-design, not bugs:** MC3 silently drops unrecognized XML
  attributes/elements on round-trip (`SYS-W5-03`, human-decided,
  documented in `MC3_FORMAT.md`); editor/exporter use different triangle
  winding deliberately; undo history is a bounded 20-entry stack
  (`AUD-038`); `embed:` mesh references aren't resolved on export
  (`AUD-025`, deferred).

## 6. Architecture notes

- **`Mc3Document`** (`mc3/include/MeshCraft/Mc3/Mc3Document.hpp`) — the
  canonical AST. CNA-free. Public API is depended on by `mc3togltf`,
  `mc3tomcb`, the editor, and every test fixture — **additive changes
  only**; check all four before changing an existing signature.
- **Two independent geometry generators, same source data:**
  `mc3togltf/src/MeshBuilder.cpp::buildPrimitive()`/`buildExtrude()`
  (export + CSG) and `SceneRenderer`'s primitive dispatch
  (`SceneRenderer_Builders.cpp`/`SceneRenderer_Extrude.cpp`, editor
  viewport). No shared code. `test/differential_geometry_test.cpp`
  diffs them for all 8 primitive types (not CSG combinations). This
  session found and fixed two cases (`AUD-072`/`073`) where the editor
  side had a bug the export side didn't — when touching one, always
  check the other.
- **`MeshCraftApplication`** — the main editor class, historically a
  "god object" (280 data members + 113 methods originally). Being
  incrementally decomposed (`SYS-W3-01`, see §5). Two idioms coexist for
  extracted subsystems: self-contained value members
  (`Preferences`/`KeybindingManager`/`UndoManager`/`WalkController`/
  `AudioPreview`), or a callback `Context` struct built per call site
  (`MacroRecorder`) — pick based on the actual call-site entanglement,
  don't assume.
- **`Alg` mirror pattern:** pure-logic, CNA-free free functions
  (`include/MeshCraft/EditorAlgorithms.hpp`,
  `src/MeshCraft/AiResponseAlgorithms.hpp`,
  `include/MeshCraft/Renderer/PrimitiveTessellationAlg.hpp`) mirror
  production code so it's headlessly testable. **Drift risk is real** —
  always check whether an `Alg` function is actually called from
  production before assuming a fix there takes effect. This session's
  `AUD-073` test deliberately mirrors a 2-line formula rather than a
  whole algorithm, to keep drift risk low.
- **Resource-path confinement (`Mc3LoadPolicy`):** both `Mc3XmlParser.cpp`
  and `Mc3JsonParser.cpp` now enforce `confineResourcePathsToRoot`
  (as of `AUD-068`) via an identically-named, independently-defined
  `g_confineResourcePaths`/`g_resourceRoot`/`includePathWithinRoot`
  thread_local trio in each file — not shared, deliberately (no existing
  cross-file context-passing mechanism for either parser's free
  functions). Keep both in sync if this policy's semantics change.
  `allowIncludes`/`confineIncludesToRoot`/`maxIncludeDepth` are XML-only
  — `.mc3.json`'s `includes` field is an inert passthrough list with no
  merge behavior to gate.
- **Undo/redo:** whole-document snapshot-based (deep copy on every
  mutating command), not command/diff-based, in `Editor::UndoManager`.
  `pushUndo()`/`performUndo()`/`performRedo()` on
  `MeshCraftApplication` are thin wrappers. 20-entry cap, by design.
- **`mc3.xsd` is compiled into the binary at configure time** — editing
  it requires a reconfigure, not just a rebuild.
- **XML comment gotcha:** a literal `--` inside an XML comment is
  rejected by `lxml`/`test/validate_xsd.py`, though `tinyxml2` tolerates
  it silently. Hit this directly while writing `test/extrude_stress.mc3.xml`'s
  comment this session — run `validate_xsd.py` on any new/edited fixture.
  For CDATA content, note the DIFFERENT (now-fixed, `AUD-070`) gotcha:
  a literal `]]>` breaks a CDATA section, not a comment.
  `Mc3XmlWriter.cpp::appendTextOrCData()` now handles this automatically.
- **`AiAssistant` threading invariant:** background HTTP runs on a
  detached `std::thread` writing into a `shared_ptr<AiRequestResult>`
  (atomic done flag + mutex). Never switch this to `std::async`/
  `std::future` (reintroduces a destructor-blocking hang-on-close
  already fixed once). `sendAsync()` now wraps the worker `std::thread`'s
  own construction in try/catch (2026-07-19 fix) — on a real construction
  failure it rolls back the in-flight counter via `endAiWorker()` and
  leaves `pending_` unset rather than wedging `isInFlight()` true forever;
  keep that rollback if this function is ever restructured further.
- **`CNA_ENABLE_NET` must stay `OFF`** — an unused CNA subsystem that
  fails to compile; re-enabling breaks the default build.
- **Boundaries that must not be broken:** no changes to `../cna` or
  `../sharp-runtime` without explicit owner permission. No
  `${meta-gl_SOURCE_DIR}/include` in `CMakeLists.txt` (triggers a full
  CNA recompile). Commits from the sibling `mesh-world` repo's own
  backlog can land on this repo's `develop` independent of this repo's
  own `plan.md` — check `git log` at the start of a session, don't
  assume `plan.md` alone reflects everything that changed.
- **Standalone CNA-free build trees:** `mc3/build/` and `mcb/build/` are
  independently configurable/buildable subtrees (no CNA/GL dependency) —
  useful for fast iteration on format-layer fixes without paying for a
  full editor rebuild. Both were used this session (`AUD-065`/`066`/`068`
  reconfigured and rebuilt there directly).

## 7. Useful commands

```bash
# Configure + build (Release, EasyGL backend)
cmake -S . -B b-release
cmake --build b-release -j4   # -j4, not -j$(nproc): shared machine, throttle it

# Full test suite
(cd b-release && ctest -j4)
(cd b-release && ctest -N)                                    # list registered tests + live count
(cd b-release && ctest -R "<name>" --output-on-failure)       # one test

# Plan/doc self-consistency (run before trusting any count in plan.md/NEXT.md)
python3 test/validate_plan_consistency.py . b-release

# XSD validation of a new/changed fixture
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# Standalone CNA-free trees (fast iteration on mc3/mcb-only changes)
cmake -S mc3 -B mc3/build && cmake --build mc3/build -j4 && (cd mc3/build && ctest -j4)
cmake -S mcb -B mcb/build && cmake --build mcb/build -j4 && (cd mcb/build && ctest -j4)

# Lint/format (config present; not applied tree-wide, see plan.md §9-equivalent)
clang-format -i path/to/changed/file.cpp
cmake -S . -B b-release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p b-release path/to/changed/file.cpp

# Run / demo
./b-release/MeshCraft test/house.mc3.xml
./b-release/MeshCraft test/house.mc3.xml --screenshot /tmp/out.png
./b-release/MeshCraft test/house.mc3.xml --benchmark
./b-release/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb --stats
./b-release/mc3tomcb/mc3tomcb test/house.mc3.xml /tmp/out.mcb

# Reproduce a specific fixed bug's before/after (pattern used throughout
# this session): stash the one fix, rebuild just that target, run against
# its regression fixture, then `git stash pop` to restore.
git stash push -- <file-with-the-fix>
cmake --build b-release -j4 --target <affected-target>
./b-release/<binary> <its-regression-fixture>
git stash pop && cmake --build b-release -j4 --target <affected-target>
```

## 8. Next smallest tasks

1. **Fix no-op undo snapshots on locked-object commands.**
   - Goal: make `deleteSelected()`, `dropSelectedToGroundPlane()`,
     `groupScaleSelected()`, `randomizeTransformSelected()`,
     `resetPivot()` skip `pushUndo()`/`modified_=true` when every
     targeted object is locked (no actual mutation happens) — mirror
     `findReplaceNames()`'s existing dry-run-first pattern.
   - Files: `src/MeshCraft/MeshCraftApplication_Commands.cpp`.
   - Verify: extend `undo_manager_test`/`undo_gesture_frame_test.cpp`
     with a "all-selected-objects-locked" case asserting no undo push;
     `ctest -R "undo_manager|undo_gesture_frame"`.

2. **Guard the latent CSG null-deref.**
   - Goal: add the same `if (child)` guard `CsgEvaluator.cpp` uses
     everywhere else in the nested-`Intersection` case at line ~306.
   - Files: `mc3togltf/src/CsgEvaluator.cpp`.
   - Verify: no test can reach this today (no parser path produces a
     null child) — a defensive one-line change; confirm
     `mc3togltf_earclip`/CSG-related tests still pass:
     `ctest -R mc3togltf`.

3. **Fix `AUD-069`'s `includePathWithinRoot()` false-rejection.**
   - Goal: resolve the candidate against an explicitly-normalized base
     (`weakly_canonical(rootDir.empty() ? current_path() : rootDir) /
     candidate`, canonicalized as a unit) instead of canonicalizing the
     bare candidate first, so resolution doesn't depend on the target
     file's existence. Fix in BOTH `Mc3XmlParser.cpp` and
     `Mc3JsonParser.cpp` (independently duplicated, not shared).
   - Files: `mc3/src/Mc3XmlParser.cpp`, `mc3/src/Mc3JsonParser.cpp`.
   - Verify: extend `load_policy_test.cpp` and `json_load_policy_test.cpp`
     with a non-existent-file + empty-rootDir case (the JSON test already
     has a same-directory case but only with the file pre-created);
     `ctest -R "load_policy"`.

4. **Doc cleanup pass (low-risk, several small pieces):**
   - Archive `AI_TRUNCATION_BUG.md` to `docs/history/` (bug is fixed).
   - Fix `README.md:254`'s stale PPM claim; add `--benchmark` to its
     flag list.
   - Refresh `missing.md` (6 stale "still open" items).
   - Update or archive `render.md` (P1/P2 done, P3-P6 still open).
   - Files: `AI_TRUNCATION_BUG.md`, `README.md`, `missing.md`, `render.md`,
     `docs/history/`.
   - Verify: no code change, so just confirm `ctest -R plan_consistency`
     (or the equivalent doc-consistency checks) still pass, and that
     nothing else references the archived file by its old root path.

## 9. Do not do yet

- **No broad `MeshCraftApplication` refactor in one pass.** `SYS-W3-01`
  is explicitly phased; do one subsystem at a time, verify, commit.
- **No mass `clang-format -i` across the existing ~19k LOC.** The config
  exists but was deliberately not applied tree-wide — a separate,
  larger, not-yet-decided change.
- **No changes to `../cna` or `../sharp-runtime`** without explicit owner
  permission, even if a fix seems small.
- **No `Mc3Document` public API changes** without checking `mc3togltf`,
  `mc3tomcb`, the editor, and all test fixtures first — additive only.
- **No attempt to unpark CI** (`.github_/workflows/ci.yml` →
  `.github/`) — owner-gated, needs a workflow-scoped push token nobody
  in this environment has.
- **No new features without asking first.** Per `CLAUDE.md`'s workflow:
  describe the task and get explicit confirmation before implementing
  anything, one item at a time — the 2026-07-18 session's entire 10-fix
  sequence and this session's single-task fix both followed that pattern
  and it worked well; don't switch to batching multiple unconfirmed fixes
  at once.
- **Don't trust a stale doc's claims at face value.** The 2026-07-18
  session's own audit found the *prior* session's "backlog exhausted"
  claim was accurate on build/test health but missed 12 real findings —
  re-derive from source when in doubt, especially for anything doc-only.

## 10. Resume prompt

```
Read NEXT.md first, in full. Then work on exactly ONE task from its
"Next smallest tasks" section — start with task 1 unless told otherwise.
Inspect only the files that task names; do not refactor or "clean up"
anything else you notice along the way. Confirm the specific task with
the user first (what will change, which files, why) before implementing,
per CLAUDE.md's workflow. Make one small, verified improvement: implement
it, add/extend a regression test that fails against the pre-fix code and
passes against the fix (verify this with git stash, matching this
session's own established pattern), then run the exact verification
command the task lists plus the full `ctest` suite. Do not start a second
task in the same session unless the first is fully committed and pushed.
When finished, update NEXT.md: move the completed task out of "Next
smallest tasks", update "Current status"/"Recent changes" with what
actually changed (not what was planned), and re-check every other section
for anything your change made stale.
```
