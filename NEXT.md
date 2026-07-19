# NEXT.md

_Last updated: 2026-07-19 (late night), end of an autonomous continuation
of the same session — the user explicitly asked to keep implementing the
remaining "Next smallest tasks" items without stopping to ask each time
(they were going to sleep until ~06:00). All 4 queued tasks are now
fixed: the AI Assistant thread-creation-failure wedge, the no-op undo
snapshots on locked-object commands, the latent CSG nested-Intersection
null-deref, `AUD-069`'s `includePathWithinRoot()` false-rejection, plus a
low-risk doc cleanup pass. **§8 is now empty — see its own note for what
a future session should do next**, since there is no more pre-queued
work to just pick up. The 2026-07-18 session ran a fresh independent
adversarial audit and fixed 10 of its 12 findings; this file was fully
rewritten (not appended to) at that point — its previous revision had
grown to 1072 lines of session-by-session narrative; see
`git log -- NEXT.md` and `docs/history/` if that history is ever
needed._

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
undocumented findings. All 10 originally-planned fixes are merged
(`AUD-064` through `AUD-073`) — `AUD-069` was the one deliberately
deferred at the time (2026-07-18), then fixed the next day (2026-07-19).
Zero of the audit's own findings remain open. The two separately-noted
gaps found alongside the audit (the AI Assistant thread-creation-failure
wedge and the no-op undo snapshots on locked-object commands) are also
both fixed, 2026-07-19 — see §3. `plan.md`'s own remaining `AUD-###` rows
are now all `DONE`/`DEFERRED`/owner-gated `TODO` (`AUD-042`/`052`/`053`/
`057` — CI/Android/sibling-pin, all blocked, not actionable here); there
is genuinely no small, unblocked, pre-scoped task left queued anywhere —
see §8. The project is in an **ongoing hardening / bug-fixing** phase,
not active new-feature development, though scoped new features have
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

- **Build: clean**, last verified this session at commit `c393ca1` (the
  last commit touching code — `a49021d`'s doc cleanup pass has no code
  changes, verified separately by re-running the full suite unchanged;
  fresh `cmake --build b-release -j4`, zero errors/warnings, EASYGL
  backend on Linux — the only backend buildable in this environment).
- **Tests: 142/142 `ctest` passing**, last verified this session at
  commit `c393ca1` and reconfirmed unchanged after `a49021d` (`ctest
  -j4`). Net +1 since this session started at 141 — only the CSG
  null-child fix (task 3) added a genuinely new `ctest`-registered target
  (`mc3togltf_csg_null_child`); tasks 1, 2, and 4 each only added new
  cases inside existing binaries; task 5 (doc cleanup) added none.
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
- **Also implemented (this session, 2026-07-19):** task 2 from the same
  list — no-op undo snapshots on locked-object commands.
  `deleteSelected()`/`dropSelectedToGroundPlane()`/`groupScaleSelected()`/
  `randomizeTransformSelected()`/`resetPivot()` (all in
  `src/MeshCraft/MeshCraftApplication_Commands.cpp`) each already skipped
  individual locked objects inside their own mutation loop, but all five
  called `pushUndo()`/set `modified_=true` unconditionally, before that
  loop — locking every selected object and invoking any of these commands
  still consumed an undo slot and marked the document modified, even
  though nothing changed (`deleteSelected()` additionally had no
  `hasSelection()` guard at all, so it did this even with zero selection).
  Fixed by adding `anySelectedUnlockedAlg(selected, lockedIds)` to
  `include/MeshCraft/EditorAlgorithms.hpp` (a pure dry-run predicate
  mirroring `findReplaceNames()`'s existing dry-run-then-commit pattern)
  and an early return using it at the top of all five commands, before
  `pushUndo()`. `MeshCraftApplication` itself isn't headlessly
  instantiable (confirmed: no test in this repo constructs the real
  CNA-dependent class directly), so — after discussing this with the
  user and getting confirmation to deviate from the task's suggested
  verify files — the fix was extracted into this already-established
  pure-`Alg` pattern (`groupScaleAlg` already does the same for
  `groupScaleSelected()`) specifically so it could be genuinely
  unit-tested, rather than left inline-only and untestable in the command
  methods. New test (`testAnySelectedUnlockedAlg`,
  `mc3/test/editor_commands_test.cpp` / `mc3_commands` in `ctest`) covers
  no-locks / partial-lock / all-locked / empty-selection /
  lock-references-unselected-object. Verified the test fails to *build*
  against the pre-fix state (`git stash` of just the header addition —
  the predicate didn't exist before this commit) and passes against the
  fix.
- **Also implemented (this session, 2026-07-19):** task 3 — the latent CSG
  nested-Intersection null-deref. `CsgEvaluator.cpp`'s `buildManifoldNode()`
  switch handles CSG nodes nested inside another CSG node's children;
  Union/Difference/Group/Area all guard every child with `if (child)`, but
  nested Intersection dereferenced `*obj.children[0]` unconditionally to
  seed its result. Unreachable via any parser path (confirmed), but a real
  bug if ever reached. Fixed by finding the first non-null child to seed
  from. New test (`mc3togltf/test/csg_null_child_test.cpp`, new
  `mc3togltf_csg_null_child` `ctest` target) builds the `Mc3Object` tree
  programmatically and nests the Intersection under a parent `Union` —
  `evaluateCsgNode()`'s own root-level CSG handling is a SEPARATE,
  already-correctly-guarded code path, so the bug is only reachable via
  this specific nesting. Verified a **real SIGSEGV** against the pre-fix
  code (`git stash`, rebuild, run — exit 139) and a clean pass against the
  fix, plus geometric-equivalence assertions (a null child is inert).
- **Also implemented (this session, 2026-07-19):** task 4 — `AUD-069`'s
  `includePathWithinRoot()` false-rejection. Wrongly rejected a
  same-directory relative resource reference (e.g. `meshSource="model.obj"`)
  as "escaping the root" whenever the target didn't exist on disk AND the
  document was opened via a bare relative filename (empty `sourceDir`).
  Root cause, found empirically (differs from the mechanism NEXT.md/
  `plan.md` originally assumed): `weakly_canonical()` on a non-existent,
  single-component relative path (a bare `"model.obj"`) does NOT resolve
  it against the current directory at all — confirmed via direct debug
  instrumentation that it returns the path unchanged, still relative,
  while the root side always resolves to an absolute path, so `relative()`
  compared an unresolved relative path against an absolute one. Fixed by
  making the candidate absolute via `std::filesystem::absolute()` FIRST
  (never requires existence), then `weakly_canonical`-ing the combined
  absolute path as a unit — deviates from the originally-sketched fix
  direction (joining candidate onto an explicitly-canonicalized root)
  because both call sites already pre-join their own base onto the
  candidate, so re-joining the root a second time would double-prefix.
  Applied identically to both `Mc3XmlParser.cpp` and `Mc3JsonParser.cpp`
  (independently duplicated). Both `load_policy_test.cpp` and
  `json_load_policy_test.cpp` gain a same-directory, non-existent-target
  case; verified both fail against the pre-fix code and pass against the
  fix (`git stash`). This one IS a formal `plan.md` row (unlike tasks 1-3)
  — updated to `[DONE]` there too.
- **Also implemented (this session, 2026-07-19):** task 5, the last
  queued item — a low-risk doc cleanup pass, no code change. Archived
  `AI_TRUNCATION_BUG.md` to `docs/history/` (confirmed via grep all 3 of
  its proposed fixes are implemented and live). Fixed `README.md:254`'s
  stale PPM claim and added a new "Headless one-shot flags" subsection
  documenting `--screenshot`/`--export`/`--benchmark` (previously
  undocumented in `README.md`, only in `--help`'s own usage text).
  Refreshed `missing.md`: verified via grep that `N8`/`N9`/`N10`/`N11`
  and the texture file-browse dialog are now implemented
  (`SYS-W14-10`/`11`/`12`/`13`/`15`); `coordinate_system` and Area's
  properties panel were formally closed as by-design won't-fix /
  confirmed-complete (`SYS-W14-14`/`17`, docs-only decisions); undo/redo
  coverage had 27 further gaps closed (`SYS-W14-16`) but is still
  intentionally a manual discipline, not structural. Updated `render.md`:
  verified via grep that P1 (lighting) and P2 (dynamic edge-overlay push)
  are both implemented — landed via the sibling `mesh-world` repo's own
  R-series work (commits `84b8c1a`/`3c33ba6`), not a `plan.md`-tracked
  task in this repo; P3-P6 confirmed still unimplemented. Verified:
  `ctest -R plan_consistency` passes; full root `ctest -j4` unchanged at
  142/142 (no code touched); confirmed no other file references
  `AI_TRUNCATION_BUG.md` by its old root-level path.
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

**This session (2026-07-19):** tasks 1 and 2 were each individually
confirmed with the user first, per `CLAUDE.md`'s workflow, before
implementing. After task 2 the user explicitly asked to continue
autonomously through the rest of §8's "Next smallest tasks" without
stopping to ask each time (going to sleep, back ~06:00 — waiting for a
confirmation would have cost hours of idle time). From task 3 onward,
each task is still individually implemented, tested, verified, and fully
committed+pushed before the next one starts (same rigor, no more
per-task confirmation gate). All builds/tests this session used `-j4`
(not `-j$(nproc)`), per the user's explicit request mid-session.

Task 1 — AI Assistant thread-creation-failure wedge:
1. Commit `63df1bc` — `fix(ai-assistant): roll back sendAsync() state on
   worker thread-creation failure`. Files modified: `src/MeshCraft/
   AiAssistant.cpp` (try/catch around the worker `std::thread`
   construction; factored the scope guard's decrement+notify into a new
   `endAiWorker()` shared by both the normal-completion and failure
   paths). File extended: `mc3/test/ai_test.cpp` (new
   `testThreadCreationFailureRollsBackState()`, Linux-only, forces a real
   `std::thread` constructor failure via `fork()` + child-scoped
   `RLIMIT_NPROC=0`).
2. Verified: the new test fails against the pre-fix code and passes
   against the fix (`git stash`); `ctest -R mc3_ai` passes; full root
   `ctest -j4` is 141/141 (same total — no new `ctest`-registered target,
   just a new case inside the existing `mc3_ai` binary).
3. This finding was tracked only in `NEXT.md` (never filed as an `AUD-0NN`
   row in `plan.md`, since it was found outside the 2026-07-18 audit's
   formal findings list) — so `plan.md` is unchanged; only `NEXT.md`
   needed updating.

Task 2 — no-op undo snapshots on locked-object commands:
1. Commit `424d027` — `fix(undo): skip no-op undo snapshots when every
   targeted object is locked`. Files modified: `src/MeshCraft/
   MeshCraftApplication_Commands.cpp` (one-line early-return guard added
   to the top of `deleteSelected()`/`dropSelectedToGroundPlane()`/
   `groupScaleSelected()`/`randomizeTransformSelected()`/`resetPivot()`,
   before their existing `pushUndo()` calls), `include/MeshCraft/
   EditorAlgorithms.hpp` (new `anySelectedUnlockedAlg()` pure predicate —
   added because `MeshCraftApplication` isn't headlessly instantiable, so
   this is what made the fix genuinely unit-testable; deviates from the
   task's file list, done with the user's explicit confirmation). File
   extended: `mc3/test/editor_commands_test.cpp`
   (`testAnySelectedUnlockedAlg`, registered in `ctest` as `mc3_commands`).
2. Verified: the new test fails to *build* against the pre-fix state
   (`git stash` of just the `EditorAlgorithms.hpp` addition — the
   predicate didn't exist before this commit) and passes against the fix;
   `ctest -R "commands|undo_manager|undo_gesture_frame"` passes (the
   latter two are the files the task originally suggested — confirmed
   unaffected); full root `ctest -j4` is 141/141 (same total, new case
   inside the existing `mc3_commands` binary).
3. Also tracked only in `NEXT.md`, not `plan.md` (same reasoning as
   task 1).

Task 3 — latent CSG nested-Intersection null-deref (first autonomous task,
no per-task confirmation asked, per the user's explicit go-ahead above):
1. Commit `8ff59c8` — `fix(csg): guard nested Intersection's first child
   against a null pointer`. Files modified: `mc3togltf/src/CsgEvaluator.cpp`
   (find the first non-null child to seed `result` from, instead of
   assuming index 0 is always populated). Files added:
   `mc3togltf/test/csg_null_child_test.cpp` (new
   `mc3togltf_csg_null_child` `ctest` target, registered in
   `mc3togltf/CMakeLists.txt`).
2. Discovered mid-task: `evaluateCsgNode()` (the public entry point) has
   its OWN separate, already-correctly-guarded root-level
   Union/Difference/Intersection handling — the bug in
   `buildManifoldNode()`'s switch is unreachable from a CSG root's direct
   children, only from an Intersection NESTED inside another CSG node's
   children. The test nests accordingly (wraps the Intersection under a
   parent `Union`).
3. Verified: a first test draft (Intersection called directly via
   `evaluateCsgNode()`, not nested) passed even against the pre-fix code —
   caught this via this session's own "verify pre-fix fails" discipline,
   which is exactly what caught the wrong-entry-point mistake. Rewrote
   the test to nest properly; confirmed a **real SIGSEGV** (exit code
   139) against the pre-fix code via `git stash` + rebuild + run, then a
   clean pass against the fix. `ctest -R mc3togltf` (65/65) and full root
   `ctest -j4` (142/142, +1 — a genuinely new `ctest` target this time)
   both pass.
4. Tracked only in `NEXT.md` (same reasoning as tasks 1/2).

Task 4 — `AUD-069`'s `includePathWithinRoot()` false-rejection:
1. Wrote a reproduction test FIRST (against `develop` HEAD, before writing
   any fix) to empirically confirm the bug was still live rather than
   trusting the description — confirmed it failed exactly as described.
2. Investigated the root cause with temporary debug instrumentation
   (`fprintf`/`std::cerr` prints in `includePathWithinRoot()`, removed
   before committing) rather than assuming the mechanism NEXT.md/`plan.md`
   described — found the actual mechanism differs (see the "Also
   implemented" bullet in §2 for the technical detail).
3. Commit `c393ca1` — `fix(AUD-069): make includePathWithinRoot() resolve
   non-existent paths`. Files modified: `mc3/src/Mc3XmlParser.cpp`,
   `mc3/src/Mc3JsonParser.cpp` (both copies of `includePathWithinRoot()`).
   Files extended: `mc3/test/load_policy_test.cpp`,
   `mc3/test/json_load_policy_test.cpp` (also removed a now-stale comment
   in the latter that had explicitly flagged this exact gap as a known,
   deliberately-unfixed follow-up).
4. Verified: both new test cases fail against the pre-fix code and pass
   against the fix (`git stash`); `ctest -R "load_policy"` (2/2) and full
   root `ctest -j4` (142/142, same total — no new `ctest` target) both
   pass.
5. Updated `plan.md`'s `AUD-069` row to `[DONE]` (this task, unlike 1-3,
   has a formal row there since it came from the 2026-07-18 audit
   itself) — updated the row's Outcome/Tests/Resolved/Status-note fields
   and the session log's DONE/TODO tally (9→10 DONE, 5→4 TODO).
   `python3 test/validate_plan_consistency.py . b-release` confirmed
   consistent afterward.

Task 5 — doc cleanup pass (last queued item, no code change):
1. Commit `a49021d` — `docs: doc cleanup pass — archive fixed bug
   report, refresh 3 stale docs`. Every claim was independently verified
   against current source via grep BEFORE editing the doc, rather than
   trusting the task's own description or the doc's existing claims at
   face value (matching §9's own standing rule) — see the "Also
   implemented" bullet in §2 for the full list of what was verified and
   how.
2. Files: `AI_TRUNCATION_BUG.md` moved to `docs/history/` (+ a "FIXED"
   banner + a `docs/history/README.md` index row); `README.md` (PPM
   claim fixed, new "Headless one-shot flags" subsection);
   `missing.md` (new "Resolved since 2026-07-18" section, summary table
   updated, now-empty "total gaps" section trimmed); `render.md` (status
   banner, P1/P2 status notes, recommended-action-order table updated).
3. Verified: `ctest -R plan_consistency` passes; full root `ctest -j4`
   unchanged at 142/142 (no code touched, only confirmed nothing broke);
   grepped the whole repo to confirm no other file still references
   `AI_TRUNCATION_BUG.md` by its old root-level path.
4. This was the last item in §8 — it is now empty. See §8's own note for
   what a future session should do next.

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
- Nothing else is currently blocking forward progress; the only item left
  in §8 is a low-risk doc cleanup pass, which is unblocked.

## 5. Known bugs and limitations

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
  whole algorithm, to keep drift risk low. Some `EditorAlgorithms.hpp`
  functions (`groupScaleAlg`, `countFindReplaceMatches`, and now
  `anySelectedUnlockedAlg`, added 2026-07-19) aren't mirrors at all —
  `MeshCraftApplication_Commands.cpp` calls them directly, specifically
  *because* `MeshCraftApplication` itself can't be instantiated
  headlessly for testing (confirmed: no test in this repo constructs it),
  so this is the only way to make that logic unit-testable. Zero drift
  risk for these three; check for this "directly called, not mirrored"
  variant before assuming every `Alg` function needs a
  called-from-production check.
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

**Empty as of 2026-07-19 (late night).** Every item queued at the start
of this session (5 total: AI Assistant thread-creation-failure wedge,
no-op undo snapshots on locked-object commands, latent CSG
nested-Intersection null-deref, `AUD-069`'s `includePathWithinRoot()`
false-rejection, doc cleanup pass) is now fixed and pushed. There is no
further pre-scoped, ready-to-implement small task sitting anywhere right
now — this is a genuine "ran out of queued work" state, not an oversight:

- `plan.md`'s remaining `AUD-###` rows are all `DONE`/`DEFERRED`, or
  owner-gated `TODO` and explicitly not actionable in this environment
  (`AUD-042` — no Android NDK + CNA-boundary restriction; `AUD-052` — CI
  needs a workflow-scoped push token nobody here has; `AUD-053`/`AUD-057`
  — both downstream of `AUD-052`).
- `SYS-W3-01` (`MeshCraftApplication` decomposition) has 7 phases done;
  its own most recent investigation round explicitly looked at the two
  remaining candidates (file dialogs, post-processing) and declined both
  (no testability win vs. real regression risk with no verification
  tool) — not silently skipped, but also not a ready "next phase" to
  just pick up without fresh investigation first.

**For a future session:** the honest options are (a) run a fresh
independent audit like the 2026-07-18 session did, to surface new
findings from scratch (this is how this session's entire task list
originated) — a bigger undertaking than a "smallest task," needs the
user's own buy-in first, not something to just start; or (b) wait for
the user's own next priority (new feature, specific bug report, etc.).
Per `CLAUDE.md`'s workflow, don't invent and start a new task without
describing it and getting explicit confirmation first — this section
being empty is not license to skip that step.

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
  anything, one item at a time. The 2026-07-18 session's entire 10-fix
  sequence, and this session's tasks 1-2 (each individually confirmed
  before implementing), all followed that pattern. Tasks 3-5 of this
  session were implemented WITHOUT a per-task confirmation — but only
  because the user explicitly, in-session, authorized continuing
  autonomously through the rest of an ALREADY-QUEUED, already-described
  list while they slept; that authorization does not extend to inventing
  NEW tasks not already in §8. §8 is now empty — the next task, whatever
  it is, still needs describing + confirmation first, same as always.
- **Don't trust a stale doc's claims at face value.** The 2026-07-18
  session's own audit found the *prior* session's "backlog exhausted"
  claim was accurate on build/test health but missed 12 real findings —
  re-derive from source when in doubt, especially for anything doc-only.

## 10. Resume prompt

```
Read NEXT.md first, in full. If its "Next smallest tasks" section (§8)
has entries, work on exactly ONE — start with task 1 unless told
otherwise. Inspect only the files that task names; do not refactor or
"clean up" anything else you notice along the way. Confirm the specific
task with the user first (what will change, which files, why) before
implementing, per CLAUDE.md's workflow, UNLESS the user has explicitly,
in THIS conversation, authorized working through the queue
autonomously without asking each time (that authorization covers only
tasks already listed in §8 when given, not new ones invented later).
Make one small, verified improvement: implement it, add/extend a
regression test that fails against the pre-fix code and passes against
the fix (verify this with git stash, matching this session's own
established pattern -- unless the fix genuinely isn't headlessly
testable, e.g. it lives in a class that can't be instantiated without a
GPU/window; if so, say so explicitly and find the nearest testable
seam, matching how this session's undo-snapshot and CSG fixes each
required a similar judgment call), then run the exact verification
command the task lists plus the full `ctest` suite. Do not start a
second task in the same session unless the first is fully committed and
pushed. When finished, update NEXT.md: move the completed task out of
"Next smallest tasks", update "Current status"/"Recent changes" with
what actually changed (not what was planned), and re-check every other
section for anything your change made stale.

If §8 is EMPTY (as it is as of 2026-07-19 late night — see its own
note), do not invent a new task and start implementing it. Read §8's
note for the honest state of what's left (plan.md's remaining rows are
blocked/owner-gated; SYS-W3-01 has no ready next phase without fresh
investigation) and surface that to the user, asking what they'd like
next -- e.g. a fresh audit (the mechanism that generated this session's
entire task list), a specific feature, or something else entirely.
```
