# Mesh Craft — Active Backlog

**This is the single active backlog.** For a compact baseline and handoff see
[`NEXT.md`](NEXT.md); for the registered tests see
[`TESTING.md`](TESTING.md). Superseded plans live in
[`docs/history/`](docs/history/), including the former
[`STABILIZATION.md`](docs/history/STABILIZATION.md) (moved there
2026-07-18 — the stabilization phase is now substantively complete; see
`NEXT.md` for current status and boundary rules now live in `CLAUDE.md`).

## Status legend

- `TODO` — not started.
- `IN_PROGRESS` — being worked now.
- `DONE` — implemented, tested, and verified (verification command recorded).
- `BLOCKED` — cannot proceed; blocker recorded.
- `DEFERRED` — intentionally postponed with a reason.

Priority: `P0` (UB / crash / data-loss / unsafe-input / invalid-output-as-success),
`P1` (wrong behavior, missing validation, untruthful capability, critical test gap),
`P2` (maintainability / duplication / perf), `P3` (polish).

Workstreams: `W0` immediate defects · `W1` validation & untrusted input ·
`W2` AI/import sandbox · `W3` architecture decomposition · `W4` de-duplication ·
`W5` MC3 governance · `W6` MCB hardening · `W7` glTF fidelity · `W8` backend truth ·
`W9` undo & data-loss · `W10` UI testability · `W11` build/CI/DX ·
`W12` performance · `W13` documentation · `W14` new features.

## How this backlog was built

The `AUD-###` tasks come from a 12-dimension evidence-based audit of the
checkout (2026-07-11, session 1). Every finding was **adversarially
re-verified** against the real source by a second independent pass; 18
candidate findings were refuted and dropped, leaving **57 confirmed**. A
follow-up adversarial re-review (2026-07-11, session 2) re-opened several
"done" claims against current source, found 3 of them only partially fixed,
and surfaced 2 new defects — these are tracked as `AUD-006b`/`AUD-036b`/
`AUD-039b` (the unfinished remainder of a partial fix) and `AUD-058`/`AUD-059`
(newly found), for **62 AUD tasks total**. Each cites exact `file:line`
evidence. The `SYS-###` tasks are the mandated systematic workstream items
(matrices, sanitizers, benchmarks, features) not tied to a single defect.
**Do not trust a DONE marker at face value** — every DONE row cites an exact
commit and an exact verification command; re-run the command if in doubt.
`test/validate_plan_consistency.py` (wired into CTest as `plan_consistency`)
mechanically checks this file stays internally consistent — run it before
trusting any count quoted below.

---

## Session log

### Session 1 (2026-07-11) — initial audit + P0/P1 fix pass

Fixed all 2 confirmed P0s and most confirmed P1s. Commits (oldest first):
`afb1159` (ActiveTool OOB), `838eefc` (ObjectType mapping), `dae910f`
(NaN/Inf rejection), `fd606d2` (input budgets + instance-cycle + finiteness
gate), `737af77` (undo dead-pattern), `0dfcd4f` (docs consolidation),
`40643a4` (Mc3LoadPolicy), `22b7129` (glTF lights), `dcd0d33` (anim mirror
Gate B), `2d126bf` (deterministic shutdown), `e53af49` (backend-truth
warning), `ac75eb6` (concave extrude caps), `901965f` (resource-path
confinement), `d16c82c` (CI file readiness).

**Self-reported end-of-session status was inaccurate** — see Session 2.

### Session 2 (2026-07-11, continued) — adversarial re-open + doc reconciliation

The session-1 end summary claimed "every P0 and every identified P1
addressed" and reported inconsistent test counts (93/93 in one place, 95/95
in another) with only 4 of 57 AUD tasks marked DONE in this file despite 16+
P1s already being fixed in git history. This session:

1. Reconciled `plan.md`/`NEXT.md` against actual `git log` + `ctest -N`
   (this section, `test/validate_plan_consistency.py`).
2. Re-verified all 57 session-1 AUD findings against current source
   (not against the session-1 summary) — see the AUD-### table below for
   the corrected per-finding status: **28 DONE, 27 TODO, 2 DEFERRED** among
   the original 57 (not 4 DONE).
3. Re-opened 3 findings that were reported fixed but were only partially
   fixed on closer inspection (`AUD-006`→`AUD-006b`, `AUD-036`→`AUD-036b`,
   `AUD-039`/`AUD-040`→`AUD-039b`) and filed the unfinished remainder as new
   `TODO` tasks.
4. Found 2 new defects via direct adversarial code reading
   (`AUD-058` BloomGL::cleanup() never called, `AUD-059` tessellation budget
   is per-field only) not part of the original audit, filed as new `TODO`
   tasks.

   **Net across all 24 AUD-### rows remaining in this active backlog (61
   additional rows completed and archived to `docs/history/plan_20260718.md`
   on 2026-07-18 — see that file for their full evidence/resolution text):
   18 DONE, 4 TODO, 2 DEFERRED** — 10 of the 18 DONE (`AUD-064` through
   `AUD-073`) are fresh findings from a 2026-07-18 (later same day)
   independent re-audit, not part of the original 6 (`AUD-069` itself fixed
   2026-07-19, the day after it was filed); the other 8 (`AUD-074` through
   `AUD-081`) are from a third independent audit on 2026-07-20 (later the
   same day as this session's SYS-W14-18..27 work) — the last two
   (`AUD-080`, `AUD-081`) are documentation-only fixes with no code change.
   Recompute with
   `python3 test/validate_plan_consistency.py . <build-dir>` rather than
   trusting this number as time passes.
5. Archived `plan_deep_audit.md` (all 57 of its own tasks were already
   completed) and fixed `RELEASE.md`'s stale 66/66 test count.
6. Removed machine-specific absolute source paths from this file's evidence
   text.

Work continues past this reconciliation into the newly-opened tasks — see
**Priority execution queue** below. This log entry itself will go stale; the
authoritative live state is always the AUD/SYS task table plus
`git log`/`ctest -N`, not this narrative.

### Session 3 (2026-07-18) — archive completed rows out of the active backlog

`plan.md` had grown to 1778 lines, almost all of it DONE rows kept only for
provenance. Moved every `DONE`-status `AUD-###`/`SYS-###` row (61 AUD + the
matching SYS rows) out to
[`docs/history/plan_20260718.md`](docs/history/plan_20260718.md) verbatim
(full evidence/resolution text preserved, nothing summarized away), leaving
this file with only the still-open rows. `NEXT.md`/code comments citing an
archived `AUD-###`/`SYS-###` id still resolve — see that file, matching the
existing `plan_deep_audit.md` precedent (`docs/history/README.md`). Re-ran
`test/validate_plan_consistency.py` after the move to confirm the backlog is
still internally consistent.

---

## Priority execution queue (next up, in order)

1. **AUD-052 (P1/W11)** — CI is permanently parked under `.github_/`; GitHub
   Actions never runs. This is the root blocker for AUD-053 (a CI-hardening
   task that depends on CI actually running first) and the CI-job half of
   AUD-057 — long documented elsewhere as owner-gated (enabling Actions on
   the repo isn't something available in this environment), so treat as
   blocked-pending-owner-action rather than something to force through.
   AUD-057's configure-time-assertion half (recording/checking the sibling
   repos' current git SHA) landed independently in commit `d2943e3` — only
   the CI-job half is still open, and it's blocked here, not actionable.
2. **AUD-042 (P2/W8)** — Android build path forces SDL_RENDERER; blocked
   (no Android NDK in this environment; also intersects CNA backend
   behavior, out of scope per CLAUDE.md's "no CNA changes without owner
   permission").
3. Remaining `TODO` AUD-### rows are all downstream of the two blockers
   above (AUD-053 needs AUD-052; AUD-057's CI-job half needs the same) —
   none are independently actionable right now.
4. All 10 of the mc3-format-vs-editor gaps found 2026-07-20 (user
   request: "co mc3 nabízí, ale MeshCraft to ještě neumí" -- "what does
   the mc3 format offer that MeshCraft doesn't yet handle") are now
   done: the two P1 gaps (trigger event-firing, Lua scripting execution),
   both P2 gaps (Scene States runtime switching, wiring
   `Mc3ImportResolver` into the editor), and all six P3 completeness gaps
   (`mipMaps` unused downstream -- exporter half done, live-viewport half
   documented as blocked on a CNA API gap; `colorSpace` unused at export;
   UV box/sphere projection; MCB compression; light-brightness unit
   conversion; ambient-light export). Nothing left queued from that
   research pass.

---

## Systematic workstream tasks (SYS-###)

Mandated workstream items not tied to a single audit finding.

### W1 — Validation subsystem
_All items in this workstream are DONE — archived to [`docs/history/plan_20260718.md`](docs/history/plan_20260718.md)._

### W2 — AI / import sandbox
_All items in this workstream are DONE — archived to [`docs/history/plan_20260718.md`](docs/history/plan_20260718.md)._

### W3 — Architecture decomposition
- **SYS-W3-01** `[IN_PROGRESS]` `P2` — Extract from `MeshCraftApplication` (a god
  object split across .cpp files, not by responsibility): document/session,
  command/undo, selection, transform/gizmo, camera, animation, import/export,
  file/autosave, preferences, render pipeline, AI, registry, audio,
  dialog/status, platform bridge — with narrow interfaces, not another god
  object. **Confirmed a genuinely multi-session task, not a one-sitting fix:**
  a 3-way research pass (full header + all 17 implementation files + every
  already-extracted subsystem) found **280 data members + 113 methods** in the
  class (692-line header body, 11,544 lines of implementation across 17
  `.cpp` files), of which only **9 are already delegated** to an owned
  helper (`camera_`/`selection_`/`gizmo_`/`sceneRenderer_`/`gridRenderer_`/
  `hierarchyPanel_`/`propertiesPanel_`/`aiAssistant_`/`registry_`) — the
  remaining ~270 raw members are the actual "god object" surface. Also found
  a previously-abandoned partial attempt at exactly this task:
  `Editor::EditorViewport` (bundling `camera_`+`gizmo_`+a `pickRay()`
  helper) was added in commit `580105d` as an explicit "stub" and never
  wired into `MeshCraftApplication` — still dead code today. Full research
  writeup + phased roadmap (Phase 2 onward: Preferences/Macro, then
  `EditorViewport`'s fate, undo/redo, animation, file dialogs,
  post-processing, audio/walk-mode) recorded in this session's plan file for
  continuity, condensed here:
  **Phase 1 DONE (commit `95aaa90`):** extracted `Editor::KeybindingManager`
  (`include/MeshCraft/Editor/KeybindingManager.hpp` /
  `src/MeshCraft/Editor/KeybindingManager.cpp`) — the `keybindings_` map,
  `KeyBind` struct, default-seeding, load/save (ini format unchanged), and
  `shortcutFired()`'s modifier+just-pressed matching, all moved out of
  `MeshCraftApplication` verbatim; `MeshCraftApplication_Keybindings.cpp`
  deleted. Chosen first because it was the one of the three originally
  bundled candidates (Preferences + Macro recorder + Keybindings) with
  genuinely no cross-domain entanglement — reading the actual
  implementation (not just member counts) showed Macro's
  `executeMacroStep()` calls 8 other `MeshCraftApplication` methods plus
  direct field access (needs a `PropertiesPanel`-style callback-DI struct,
  deferred to a future phase alongside a broader Commands extraction), and
  Preferences' `loadPrefs()`/`savePrefs()` persist a cross-cutting struct
  spanning autosave/gizmo-snap/render-grid fields that aren't Preferences'
  own (needs a deliberate ownership decision, also deferred). New
  `keybinding_manager_test` (root `CMakeLists.txt`, needs real CNA
  `Keys`/`KeyboardState` so it links `CNA`/`SHARP_RUNTIME` unlike the
  header-only `active_tool_test` next to it) — no test existed for this
  subsystem before the extraction; covers default-seeding,
  `shortcutFired()`'s just-pressed/modifier-matching edge cases, and the
  save/load round-trip (a user rebind survives while untouched ids still
  re-seed to their default). Full tree rebuilt + 122/123 ctest (the 1
  pre-existing, out-of-scope `field_matrix` failure, unrelated).
  **Decisions (2026-07-17, human-authorized), unblocking Phase 2 onward:**
  (1) `EditorViewport` — **delete**, not finish. It's abandoned scaffolding
  (stub added `580105d`, never wired in, zero call sites); removing it is
  lower-risk than retargeting ~76 `camera_.`/~12 `gizmo_.` call sites across
  7 files during a stabilization phase. (2) Preferences (Phase 2) —
  **narrow**: the new class owns only `prefTheme_`/`prefsOpen_`/
  `applyTheme()`. `autoSaveInterval_`/`snapTranslate_`/`snapRotate_`/
  `snapScale_`/`gridSpacing_` stay on `MeshCraftApplication` for now,
  matching the single-domain-extraction idiom `KeybindingManager`
  established, to be picked up by their own future subsystem extractions
  rather than folded into Preferences.
  **`EditorViewport` deletion executed (2026-07-17):** removed
  `include/MeshCraft/Editor/EditorViewport.hpp` and
  `src/MeshCraft/Editor/EditorViewport.cpp` outright (`git rm`) — confirmed
  zero references anywhere else in the tree first
  (`grep -rln EditorViewport`). Sources are `file(GLOB_RECURSE ...)`-picked
  up in `CMakeLists.txt`, so no `CMakeLists.txt` edit was needed, just a
  reconfigure (`cmake .`) before the next build. Full rebuild + 123/123
  `ctest`, zero new warnings.
  **Phase 2 (narrow) DONE (2026-07-17):** extracted `Editor::Preferences`
  (`include/MeshCraft/Editor/Preferences.hpp` +
  `src/MeshCraft/Editor/Preferences.cpp`) owning exactly `prefTheme_`
  (renamed `theme_`, via `theme()`/`setTheme()`), `prefsOpen_` (renamed
  `windowOpen_`, via `windowOpen()`/`setWindowOpen()`), and `applyTheme()` —
  and nothing else, per the narrow-scope decision. `MeshCraftApplication`'s
  `loadPrefs()`/`savePrefs()` stay put (they persist `prefs_.theme()`
  together with `autoSaveInterval_`/`snapTranslate_`/`snapRotate_`/
  `snapScale_`/`gridSpacing_` in one prefs file via `PrefsAlg` — splitting
  that shared load/save mechanism was explicitly out of scope for this
  narrow extraction). Updated all 3 call sites
  (`MeshCraftApplication_UiOverlays.cpp`'s Preferences dialog + theme
  buttons, `MeshCraftApplication_UiMenuBar.cpp`'s Help menu item,
  `MeshCraftApplication_FileOps.cpp`'s `loadPrefs()`/`savePrefs()`). New
  `preferences_test` (no coverage existed for this subsystem before):
  default theme/windowOpen values, accessor round-trips, and — since
  `applyTheme()`'s ImGui side effect can't be asserted by inspecting a
  return value — a differential check that `theme()` 0/1/2 each apply a
  visibly different `ImGuiCol_WindowBg`, plus an out-of-range value falling
  back to Dark instead of crashing. Full rebuild + 124/124 `ctest` (new
  `preferences` test registered); manual `--screenshot` smoke test confirms
  the app still boots, loads prefs, and renders after the refactor.
  **Phase 3 DONE (2026-07-18):** extracted `Editor::MacroRecorder`
  (`include/MeshCraft/Editor/MacroRecorder.hpp` +
  `src/MeshCraft/Editor/MacroRecorder.cpp`) — the `isRecording_`/
  `macroSteps_` state, `recordStep()`/`playMacro()`/`executeMacroStep()`/
  `saveMacro()`/`loadMacro()`. This was the harder of the two originally-
  bundled Phase-1 candidates (Preferences' own entanglement turned out
  narrow and was resolved in Phase 2; Macro's was real):
  `executeMacroStep()` calls 8 other `MeshCraftApplication` methods
  (`addPrimitive`/`deleteSelected`/`duplicateSelected`/`groupSelected`/
  `ungroupSelected`/`groupScaleSelected`/`batchRenameSelected`/
  `arrayDuplicate`) plus inline direct mutation of `document_.objects`/
  `selection_`/`lockedIds_`/`modified_` for the `hide`/`show_all`/`lock`/
  `unlock` verbs — resolved with a `MacroRecorder::Context` callback
  struct (a `PropertiesContext`-style DI struct), built fresh at each
  `play()`/`save()`/`load()` call site by a new
  `MeshCraftApplication::macroContext()` factory method (in the now much
  smaller `MeshCraftApplication_Macro.cpp`, which contains only that
  method), rather than storing a back-reference to `MeshCraftApplication`
  — matching `ObjectIndex`'s "dependencies passed as parameters, not
  held" idiom, since play/save/load are on-demand actions with no
  per-frame staleness risk (unlike `PropertiesContext` itself, which is
  rebuilt every frame for that reason). `MacroStep`/`MacroStepAlg`'s
  pre-existing AUD-031 duplication (a real type vs. a CNA-free test
  mirror) is unchanged — `MacroRecorder` is itself now fully CNA-free, but
  keeping the two types separate still lets `EditorAlgorithms.hpp`'s
  `saveMacroAlg`/`loadMacroAlg` be unit-tested standalone without linking
  the new class, so the conversion at the save/load boundary was kept
  rather than merged. Updated all direct-field-access call sites
  (`MeshCraftApplication_Commands.cpp`'s 8 `recordStep()` calls,
  `MeshCraftApplication_UiMenuBar.cpp`'s Record/Stop/Play menu items,
  `MeshCraftApplication_UiOverlays.cpp`'s Macro Editor dialog). New
  `macro_recorder_test` (no coverage existed for this subsystem before):
  a `Spy` context logging every callback invocation, covering
  `recordStep()`'s no-op-while-not-recording guard, start/stop/clear,
  every one of the 12 verbs dispatching to the right callback with
  correctly-parsed args (including `group_scale`/`linear_array`'s numeric
  parsing), playback not re-recording itself and restoring the prior
  `isRecording()` state, the unknown-`add`-type error path, a real
  save/load round-trip through a temp file, and the empty-path/missing-
  file error messages. Full rebuild + 128/128 `ctest` (new `macro_recorder`
  test registered, CNA-free like `object_index_test`); manual
  `--screenshot` smoke test confirms the app still boots and renders
  cleanly after the refactor.
  **Phase 4 DONE (2026-07-18):** extracted `Editor::UndoManager`
  (`include/MeshCraft/Editor/UndoManager.hpp` +
  `src/MeshCraft/Editor/UndoManager.cpp`) — `undoStack_`/`redoStack_` plus
  `SYS-W9-03`'s `undoSelectionStack_`/`redoSelectionStack_` (kept in
  lockstep exactly as before, now returned together as one `Entry`).
  Investigated the call-site surface first rather than assuming
  `pushUndo()`/`performUndo()`/`performRedo()` were the whole story: a
  direct grep found a 4th real consumer — the Undo History dialog
  (`MeshCraftApplication_UiOverlays.cpp`) has its own "jump straight to
  step N" logic (`AUDIT-0056`'s own redo-stack-cap fix included), not just
  push/undo/redo — plus 3 "clear both stacks" sites (`FileOps.cpp`'s Open/
  autosave-recovery, `UiOverlays.cpp`'s Open dialog) and one direct-pop
  site (`SYS-W12-02`'s benchmark, measuring `pushUndo()`'s cost then
  undoing the push). All 4 shapes got a matching method
  (`push`/`undo`/`redo`/`jumpTo`/`popUndoWithoutApplying`/`clear`), so no
  call site needed a workaround. Unlike `MacroRecorder`, this needed no
  callback `Context` at all: every side effect beyond stack bookkeeping
  (`deepCopyDoc()`, `objectIndex_.invalidate()`, `restoreSelectionByIds()`,
  `modified_`/`updateWindowTitle()`, `evaluateAndPushAnimOverrides()`)
  stays the caller's responsibility before/after calling in — `deepCopyDoc()`
  in particular is a `MeshCraftPrivate.hpp` helper, and keeping
  `UndoManager` from depending on it (the caller always passes an
  already-independent copy in) matches `KeybindingManager`'s established
  precedent of keeping extracted classes' dependency footprint minimal.
  `AUD-031`'s `pushWithCapAlg` moved with it (reused directly, header-only,
  no duplication). New `undo_manager_test` (no coverage existed for this
  subsystem before — `undo_gesture_frame_test.cpp` covers the same
  invariants against hand-rolled stand-ins since `MeshCraftApplication`'s
  real methods aren't headlessly callable, and stays as a second,
  independent check; this new test exercises the real class directly):
  41 assertions covering push/undo/redo semantics, redo invalidation on a
  new push, the `kMax` depth cap on both stacks, selection ids traveling
  in lockstep, `jumpTo()`'s multi-step redo-stack rebuild (including its
  own cap and out-of-range rejection), and
  `popUndoWithoutApplying()`/`clear()`. Full rebuild + 129/129 `ctest`
  (new `undo_manager` test registered, CNA-free like `object_index_test`/
  `macro_recorder_test`); manual `--screenshot` smoke test plus a real
  `--benchmark` run confirming the live app's actual `pushUndo()` →
  `undoManager_.push()` → `popUndoWithoutApplying()` path executes cleanly
  end-to-end with a plausible timing, not just the unit test's mocks.
  **Phase 5 (narrowed) DONE (2026-07-18):** the original roadmap named
  "animation" as a whole future phase, but a closer look showed the same
  kind of over-broad framing Preferences' Phase 2 already corrected once:
  `MeshCraftApplication_Anim.cpp` is 975 lines, but only 1 of its 3
  top-level functions (`evaluateAndPushAnimOverrides()`) is a genuinely
  self-contained pure computation — `drawTimelinePanel()` is a ~800-line
  ImGui widget (drag/box-select/multi-select/3 dialogs/keyframe clipboard)
  tightly coupled to playback-control state (`animTime_`/
  `currentActionName_`/`animPlaying_`) referenced from 8 other files
  (`_Keyboard.cpp`, `_Mouse.cpp`, `_UiLeftPanel.cpp`, `_UiMenuBar.cpp`,
  `_UiOverlays.cpp`, `_UiProperties.cpp`, `_FileOps.cpp`,
  `_Benchmark.cpp`) — untangling that now would mean redoing the same
  work `SYS-W14-16` just finished auditing, for no proportionate benefit.
  **Narrowed scope, matching the Preferences precedent:** extracted only
  `evaluateAndPushAnimOverrides()`'s core computation — a two-pass
  seed-then-override calculation over `document_.actions`/materials that
  had **zero existing test coverage** (unlike the neighboring
  `insertAnimKeyframesAlg()`, already tested) — as a new
  `computeAnimOverridesAlg()` in `EditorAlgorithms.hpp`, mirroring the
  file's own established `resolveObjectPropertyValueAlg()`/STAB-0715
  precedent for exactly this kind of production/test-mirror sharing. Uses
  a new CNA-free `AnimOverrideAlg` mirror struct (field-for-field
  identical to `Renderer::AnimOverride`, which can't be included from a
  CNA-free header without pulling in `SceneRenderer.hpp`'s unrelated
  CNA-coupled dependencies — same `MacroStep`/`MacroStepAlg` duplication-
  for-testability idiom). `evaluateAndPushAnimOverrides()` itself is now a
  thin wrapper: clear a stale `currentActionName_`/`animPlaying_` (a
  stateful side effect that stays here), call the pure function, convert
  `AnimOverrideAlg` → `Renderer::AnimOverride`, push to `SceneRenderer`.
  New tests in `mc3/test/editor_commands_test.cpp` (18 assertions):
  transform channel override at a mid-timeline value while untouched
  transform fields stay seeded from the object's live state, material
  channel override (roughness) with base color correctly seeded-not-
  touched, deform defaulting to (1,1,1) with no `Mc3Deform`, an
  unresolvable/renamed target object and an empty `targetObject` both
  silently skipped (matching pre-extraction behavior exactly, not a
  behavior change), and multiple distinct target objects each getting
  their own keyed entry. Full rebuild + 130/130 `ctest`; manually ran
  `--benchmark` against `animation_demo.mc3.xml` (confirms `animation
  eval (Showcase)` fires with the real action name and a plausible
  timing) and `--screenshot` against the same scene (visually confirmed
  via direct image read — SpinPole/GlowCube/PulseSphere etc. render
  correctly with no visual corruption), not just "compiles and doesn't
  crash."
  **Remaining roadmap after Phase 5:** the timeline UI/dialogs/clipboard
  state (deliberately NOT extracted in Phase 5, per the narrowing above),
  file dialogs, post-processing, audio/walk-mode — each its own future
  phase.
  **Phase 6 DONE (2026-07-18) — walk mode:** investigated "file dialogs"
  first and declined it — the 3 dialogs (Open/Save-As/Import-OBJ) are
  already well-factored thin UI glue over existing, already-tested Alg
  functions (`resolveSaveAsPathAlg`, `loadSceneFileDispatched`,
  `addPrimitive`); pulling their handful of booleans/char-buffers into a
  class would have had zero testability payoff, unlike every other phase
  so far. Walk mode (H15), by contrast, turned out to be the cleanest
  extraction yet: `MeshCraftApplication_WalkMode.cpp` is only 168 lines,
  and `updateWalkMode()` is a genuinely self-contained physics/look
  simulation — no `document_`/`selection_` dependency at all, unlike
  every other subsystem extracted so far. Extracted `Editor::WalkController`
  (`include/`+`src/MeshCraft/Editor/WalkController.hpp`/`.cpp`): owns the
  position/yaw/pitch/velocity/on-ground state plus the tunable
  height/speed/turnSpeed/mouseSens settings (public fields, mirroring
  `EditorCamera`'s own yaw/pitch/distance/fovDegrees style so the walk-
  settings ImGui sliders can still bind directly to them). No callback DI
  needed (unlike `MacroRecorder`) — genuinely self-contained, like
  `Preferences`/`KeybindingManager`, and like `KeybindingManager` it's
  unavoidably CNA-coupled (needs a real `KeyboardState` for `update()`).
  `enter()`/`exit()`/`update()` mirror the pre-extraction
  `enterWalkMode()`/`exitWalkMode()`/`updateWalkMode()` logic exactly,
  returning an `ExitCameraState` (target/yaw/pitch/distance) for the
  caller to apply to `Editor::EditorCamera` rather than reaching into it
  directly. `viewMatrix()` computes the first-person view from its own
  state; the projection matrix deliberately stays in
  `MeshCraftApplication::Draw()` since walk mode has never had its own
  FOV/clip-plane settings, only borrowed `camera_`'s. Found and removed
  one piece of genuinely dead code while touching this state:
  `walkSettingsOpen_` was declared but never read or written anywhere.
  New `walk_controller_test` (CNA-coupled like `keybinding_manager_test`
  — needs a real `KeyboardState`; no coverage existed for this subsystem
  before): 32 assertions covering `enter()`'s ground-relative position
  seeding (including clamping a below-ground seed to 0), `exit()`'s
  camera-state restoration (including the pitch sign flip between the
  walk-camera and orbit-camera conventions), forward/backward movement
  following yaw, keyboard yaw turning, mouse look, pitch clamping at
  ~±85°, jump+gravity+ground-collision settling back at y=0 with
  on-ground correctly re-armed for a second jump, `update()`'s implicit
  exit on Escape (matching the pre-extraction early-return exactly), and
  a differential check that `viewMatrix()` actually incorporates
  position/yaw/height (not hand-deriving `CreateLookAt`'s exact matrix,
  matching `preferences_test.cpp`'s own differential-check precedent for
  a third-party formula). Full rebuild + 131/131 `ctest` (new
  `walk_controller` test registered); manual `--screenshot` smoke test
  confirms the normal (non-walk) orbit-camera rendering path is unaffected.
  Walk mode itself needs live F5+WASD input with no CLI-triggerable path
  to exercise it end-to-end in this sandbox (same limitation as
  `SYS-W14-08`'s AI-panel diff UI) — confidence here comes from the 32
  real-class unit tests plus a clean compile/link/smoke-boot, not an
  interactive screenshot of walk mode itself.
  **Phase 7 DONE (2026-07-18) — audio preview:** extracted
  `Editor::AudioPreview` (STAB-0706's sound/music preview playback) —
  another small, self-contained subsystem like walk mode: 3 fields + 2
  methods, all living in one file (`MeshCraftApplication_UiLeftPanel.cpp`,
  both implementation and every call site), with zero `document_`/
  `selection_` coupling beyond an already-resolved `srcPath` string passed
  in by the caller. Unlike file dialogs, this had real behavior worth
  encapsulating: a stop-before-play invariant, error-clearing-on-each-
  attempt, and a genuine correctness constraint (`setIsLoopedProperty()`
  must be called before `Play()` — it throws afterward) that's now a
  documented, enforced-by-construction invariant of the class rather than
  an implicit ordering a future edit could accidentally break. Also
  deduplicated a real repeat: the "is this key currently playing" boolean
  expression (`key == audioPreviewKey_ && audioPreviewInstance_ && ...
  == SoundState::Playing`) was hand-copied at both the sound-list and
  music-list call sites; now one `isPlaying()` method. Added a
  `currentKey()` accessor distinct from `isPlaying()` for the
  remove-button guard, which cares about "is a live instance associated
  with this key at all" regardless of playback state, not "is it actively
  Playing" specifically. Since both the implementation and every call
  site lived in the same file with no other consumer, `playAudioPreview()`/
  `stopAudioPreview()` were removed outright rather than kept as thin
  wrappers (unlike walk mode, where 3 other files needed the existing
  method names preserved). No callback DI needed — self-contained, CNA-
  coupled where unavoidable (real `SoundEffect`/`SoundEffectInstance`),
  matching `WalkController`/`KeybindingManager`'s precedent. New
  `audio_preview_test` (CNA-coupled; no coverage existed for this
  subsystem before): 13 assertions covering the error path exhaustively —
  a nonexistent source path exercises the exact same catch block a
  missing-audio-device error would, so this is meaningful even without a
  working audio backend in this sandbox — initial state, `play()` failure
  clearing state and reporting `error()`, `stop()` as a safe no-op,
  `error()` not accumulating across repeated failed attempts, and
  `isPlaying()`/`currentKey()`'s distinct semantics. **Honesty note:** the
  real-playback success path (a valid audio file actually reaching the
  Playing state) is NOT covered — would need a real audio device/fixture
  this headless sandbox can't guarantee, matching the same limitation
  already noted for walk mode's live-input path and the native file
  dialog. Full rebuild + 132/132 `ctest` (new `audio_preview` test
  registered); manual `--screenshot` smoke test confirms the app still
  boots and renders cleanly (the Audio tab itself needs live interaction
  to exercise beyond compile/link correctness, same limitation).
  **Post-processing (bloom/skybox/SSAO) investigated, declined
  (2026-07-18):** unlike every phase above, this has no pure/testable
  logic to isolate — `initBloom()`/`applyBloom()`/`initSkybox()`/
  `drawSkybox()`/`initSsao()` are 100% side-effecting raw GL calls
  sharing one combined GL-resource bundle (`s_bloom`: function pointers,
  shader programs, FBOs, textures for all three effects at once, not
  three separate boundaries) with a documented history of real fragility
  — a prior fix (see `project_bloom_bug` memory / this file's own
  earlier bloom-effect history) resolved a silent VAO/VBO draw failure
  plus CNA leaking `GL_CULL_FACE`/`GL_STENCIL_TEST`/`GL_SCISSOR_TEST`
  state that had to be explicitly disabled. Its correctness can only be
  verified by actually rendering and pixel-comparing, not by unit tests
  (unlike `WalkController`'s differential matrix checks or
  `computeAnimOverridesAlg`'s direct assertions) — extracting it now
  would carry real regression risk with no proportionate de-risking tool
  available. Left untouched; would need a dedicated visual-regression
  harness (before/after pixel comparison against a real emissive-
  material scene) built first if ever revisited.
  **SYS-W3-01 roadmap status after this session's investigation round:**
  Phases 1–7 done (Keybindings, Preferences, MacroRecorder, UndoManager,
  animation-override computation, WalkController, AudioPreview,
  in-that-order). File dialogs and post-processing were each
  investigated and explicitly declined for different reasons (no
  testability win vs. real regression risk with no verification tool) —
  not silently skipped. `MeshCraftApplication` itself is still a large
  class (the god-object surface named at the top of this entry hasn't
  been reduced to zero), but every remaining piece has now been looked
  at directly rather than assumed extractable.

### W5 — MC3 governance
- **SYS-W5-03** `[DEFERRED, human-authorized decision]` `P2` — MC3
  versioning + unknown element/attribute policy; round-trip must not
  silently drop unknown data unless policy says so.
  **Decision (2026-07-17, human-authorized): leave current behavior as-is
  and document it as an accepted limitation.** No implementation work —
  silently dropping unrecognized XML attributes/elements on round-trip
  stays the documented, intentional behavior; not revisited unless a
  concrete need for forward/backward compatibility arises later.
  **Investigation (2026-07-17): confirmed, with real evidence (this was
  a genuinely-unverified one-line claim before): a fresh
  `<mc3 ... totally_unknown_root_attr="keep-me">` root attribute, an
  unrecognized `<box ... totally_unknown_obj_attr="also-keep-me">` object
  attribute, and an unrecognized `<totally_unknown_element foo="bar"/>`
  root child are **all silently dropped** on any load+save round-trip.
  Root cause, by design (not a bug to patch): `Mc3XmlParser.cpp` reads
  every field via explicit, named `attr(el, "known_name")` calls (21+
  call sites just for root/object-level attributes) with no
  "collect everything I didn't recognize" fallback, and `Mc3XmlWriter.cpp`
  builds a brand-new `XMLElement` from scratch on save, emitting only
  known fields via named `SetAttribute()` calls — there is no
  attribute-preserving codepath anywhere for either layer to lose data
  from in the first place. (`doc.metadata`/`doc.meta` are a deliberate,
  narrow, opt-in passthrough for the specific `<metadata>`/`<meta>`
  elements only — not a generic "preserve anything I don't recognize"
  mechanism.) **Why this stays `BLOCKED`, not implemented unilaterally:**
  the task's own title says "policy" for a reason — there are several
  materially different ways to actually solve this (a generic per-object/
  per-document "extra attributes" bag akin to `metadata`; raw-XML-node
  preservation; a hard schema-version gate that rejects unrecognized
  constructs instead of silently accepting+dropping them; simply
  documenting current behavior as an accepted limitation and closing this
  as DEFERRED) with real, different tradeoffs for every future format
  extension (this project's own `mc3.xsd`, and any external tool/consumer
  built against MC3) — exactly the kind of "could substantially affect
  architecture... format data" decision this session's own operating
  instructions say requires a human, not a unilateral implementation
  choice. Needs a human answer to: **should MC3 preserve unrecognized
  XML data on round-trip at all, and if so, via which mechanism?**

### W6 — MCB hardening
_All items in this workstream are DONE — archived to [`docs/history/plan_20260718.md`](docs/history/plan_20260718.md)._

### W7 — glTF fidelity
_All items in this workstream are DONE — archived to [`docs/history/plan_20260718.md`](docs/history/plan_20260718.md)._

### W8 — Backend truth
_All items in this workstream are DONE — archived to [`docs/history/plan_20260718.md`](docs/history/plan_20260718.md)._

### W9 — Undo & data-loss
- **SYS-W9-01** `[DONE, via AUD-036b + SYS-W9-03 + SYS-W14-16]` `P0` — Full
  undo/redo correctness: transaction abstraction, pre-mutation capture
  verification, `undo_coverage_audit.py` triage, atomicity, redo
  invalidation, selection restore. This row was always just a pointer
  ("See `AUD-036b` for the authoritative task text") rather than
  independent scope, and had gone stale relative to work already done
  elsewhere in this same file: `AUD-036b` (`docs/history/plan_20260718.md`)
  is `DONE` — all 81 `undo_coverage_audit.py` candidates individually
  triaged (0 REAL_MUTATION, the rest TRANSIENT_PREVIEW/FALSE_POSITIVE/
  INTENTIONALLY_NON_UNDOABLE), frame-driven behavioral coverage extended
  to Checkbox/Combo/InputText/multi-object batch edits, and every
  guarantee from its own Outcome verified except "selection restored
  after undo/redo" — which was an explicit open product-question at the
  time, resolved human-authorized **yes** and implemented same-day as
  `SYS-W9-03` (`performUndo()`/`performRedo()` restore the pre-mutation
  selection by id instead of clearing it). This session's own
  `SYS-W14-16` (2026-07-18) then ran a fresh, independent undo-coverage
  audit (two parallel research agents) and found + fixed 27 further real
  gaps (6 missing `pushUndo()` calls, 21 `AUD-036`-style dead patterns)
  that had accumulated since `AUD-036b` closed — i.e. this row's actual
  intent (find and close real undo-coverage gaps) has now been executed
  twice, not left undone. **Not done, by explicit prior scope:** a formal
  "transaction abstraction" class (Command pattern / mutation-tracking
  proxy) was never built — the manual `pushUndo()`-before-every-mutation
  discipline remains, now backed by three rounds of audit rather than a
  structural guarantee. If this bug class recurs a third time, that
  escalation (not a fourth manual audit) would be the right next step.

### W11 — Build / CI / DX
- **SYS-W11-01** `[TODO, owner-gated]` `P1` — Un-park CI (`.github_` → `.github`)
  needs a `workflow`-scoped push token (`AUD-052`). Workflow file itself made
  correct/ready in commit `d16c82c`.
- **SYS-W11-03** `[TODO]` `P2` — Editor build+test CI job. (`AUD-053`)

### W12 — Performance baselines
- **SYS-W12-02** `[DONE, 7 of 10 categories isolated — see honesty note]`
  `P3` — Extend `SYS-W12-01`'s benchmark harness to the remaining
  categories that need in-process instrumentation rather than CLI-level
  timing: mesh-gen, CSG + cache, traversal, picking, undo snapshot,
  texture processing, animation eval, registry, startup, first frame.
  **Implementation (2026-07-18):** new `MeshCraft <scene> --benchmark`
  headless CLI mode (`MeshCraftApplication_Benchmark.cpp`, a new
  constructor overload + `main.cpp` flag, following the exact same
  countdown/`pendingX_`/`EndDraw()`-triggers-`Exit()` pattern
  `--screenshot`/`--export` already established). `startup` times
  `LoadContent()` directly; `first frame`/`warm frame` wrap the first 10
  REAL `Draw()` calls (genuine `camera_`/view/proj, not a hand-rolled
  stand-in, so cold-vs-warm cache costs are real); `traversal`
  (`scenePolyStats`), `picking` (`computeObjectWorldMatrix` over every
  object), `undo snapshot` (`pushUndo()`'s deep-copy cost), `animation
  eval` (`evaluateAndPushAnimOverrides()`, skipped with a clear message if
  the scene has no actions), and `registry` (real SQLite open + `search`)
  are all timed directly, no render context needed.
  **Honesty note (3 of 10 NOT isolated, matching this session's own
  established convention of not overstating coverage):** `mesh-gen`,
  `CSG + cache`, and `texture processing` are reflected only in the
  combined first-vs-warm-frame delta, since all three populate their
  respective caches during those same early frames — isolating each
  individually would need deeper per-subsystem hooks (e.g. inside
  `SceneRenderer`'s own mesh/texture loaders), left as a smaller
  follow-up, not claimed as done.
  **Verify:** full rebuild + `ctest -j"$(nproc)"` (127/127, +1 new
  `benchmark_editor` test — a smoke check that every expected category
  line appears and the process exits 0, same "informational, not a
  timing gate" philosophy `SYS-W12-01`'s own `benchmark` test already
  established, not a new precedent). Manually ran `--benchmark` against
  3 real fixtures (`house.mc3.xml`, `animation_demo.mc3.xml` — confirms
  animation eval fires with a real action name, `csg_cache.mc3.xml` —
  confirms a real warm-up delta from CSG cache population) and confirmed
  every number is plausible, not just "didn't crash." `test/BENCHMARK_BASELINE.md`
  updated with a sample reading.

### W14 — New features (after P0/P1 gates)
- **SYS-W14-03** `[DONE]` `P2` — PNG screenshot / image export.
  `main.cpp`'s own `--help` text and usage example have always promised
  `scene.mc3.xml --screenshot output.png` renders "to PNG", but
  `saveScreenshot()` unconditionally wrote raw PPM (P6) bytes regardless
  of the requested extension — a real `.png` path got PPM data with a
  misleading extension, not a decodable PNG (confirmed directly: this
  session's own earlier `--screenshot foo.png` calls needed `convert` to
  become viewable). **Implementation (2026-07-18):** `saveScreenshot()`
  now dispatches on the path's extension — an explicit `.png` path is
  encoded with `stbi_write_png()` (tinygltf's vendored
  `stb_image_write.h`); every other extension (in particular every
  `.ppm` path this test suite's own screenshot-based tests use) keeps
  writing the exact same raw PPM bytes as before, byte-for-byte
  unchanged. No new dependency or `CMakeLists.txt` change was needed:
  `GltfExporter.cpp` (`mc3togltf_lib`, which `MeshCraft` already links)
  already defines `STB_IMAGE_WRITE_IMPLEMENTATION` for its own glTF
  texture embedding, so the encoder is already compiled into the binary —
  this only adds a declarations-only `#include <stb_image_write.h>` (the
  header is already on `MeshCraft`'s include path via `mc3togltf_lib`'s
  `SYSTEM PUBLIC` include dir). The GL-readback buffer is bottom-up; PNG
  (like the existing PPM writer) expects top-down, so the fix flips into
  a second buffer explicitly rather than using `stb_image_write`'s global
  `stbi_flip_vertically_on_write()` flag, which would also affect
  `mc3togltf`'s own unrelated `stbi_write_png()` calls. New
  `png_screenshot_test.py`: a `.png` path decodes as a real PNG (magic
  bytes + `IHDR` width/height/bit-depth/color-type match the viewport,
  hand-parsed rather than adding a Pillow dependency this test suite
  doesn't otherwise have), and a `.ppm` path is confirmed still exactly
  raw PPM (P6) — a regression guard for the ~15 existing screenshot-based
  tests (`csg_*_cutter_test.py`, `background_texture_test.py`,
  `skybox_texture_test.py`, `*_gizmo_test.py`, etc.) that all pass `.ppm`
  explicitly and were confirmed unaffected. Full rebuild + 130/130
  `ctest` (new `png_screenshot_test` registered); manually ran
  `--screenshot foo.png` for real and visually confirmed (via a direct
  image read) it renders correctly and matches the equivalent `.ppm`
  capture pixel-for-pixel in composition, not just "decodes without
  erroring."
- **SYS-W14-04** `[DEFERRED]` `P3` — SVG texture rasterization pipeline.
- **SYS-W14-05** `[DEFERRED]` `P3` — Safe `embed:` mesh/resource support end-to-end. (`AUD-025`)
- **SYS-W14-06** `[DEFERRED]` `P3` — Improved CSG output (smooth normals/UVs/materials).
- **SYS-W14-07** `[DEFERRED]` `P3` — Improved walk/navigation collision.
- **SYS-W14-08** `[DONE]` `P2` — AI change preview/diff before destructive
  replace. Before this, "Apply to Scene" replaced `document_` wholesale
  with only two safety nets: full Undo, and `STAB-0395`'s
  `aiApplyNeedsConfirmationAlg()` — a single heuristic that only warns on
  a drastic top-level *object-count* drop (e.g. 50→3). Nothing told the
  user WHICH specific objects an AI response would add, remove, or change
  — only a truncated 280-char raw-text preview of the response body.
  **Implementation (2026-07-18):** `computeAiChangeSummaryAlg()`
  (`src/MeshCraft/AiResponseAlgorithms.hpp`) flattens both the current and
  pending documents' object trees by id (first-match-in-document-order for
  duplicates, matching `Editor::ObjectIndex`'s established convention;
  same 256-depth cycle guard as `deepCopyObjectAlg` — AI responses are
  untrusted content) and reports three id-keyed lists: added, removed,
  modified. **Scope, stated plainly in the code (not implied):**
  "modified" compares only name/type/visible/material/transform
  (position/rotation/scale) — the highest-signal fields for "did the AI
  move/rename/hide/reassign my object" at a glance — NOT a full
  field-by-field structural diff (primitive params, extrude,
  csgOperation, tags, states, uvMapping, metadata, assetMetadata,
  scriptId are not compared; an object keeping its core fields but
  gaining, say, a changed primitive radius is reported as unchanged
  here). A full exhaustive diff would need its own dedicated pass — this
  is intentionally the high-signal subset. Wired into
  `MeshCraftApplication_UiAi.cpp`: right after "Response validated.", the
  AI panel now shows an "N added, M removed, K modified*" summary line
  (color-coded) with an optional expandable details tree listing each
  changed object's name/id (capped at 20 per category with an explicit
  "...and N more" rather than silently truncating), shown before the
  Apply/Confirm Replace buttons — informing the decision, not gating it
  (Undo remains the actual safety net). New tests in `mc3/test/ai_test.cpp`
  (10 assertions): added/removed detection, modified detection (name
  change and transform change, both independently), unchanged objects
  with an out-of-scope field difference (tags) correctly NOT flagged,
  nested children walked (not just top-level objects), duplicate-id
  first-match-wins semantics, and the cyclic-children throw guard. Full
  rebuild + 130/130 `ctest`; manual `--screenshot` smoke test confirms no
  regression to the overall app (the new code only runs inside the
  already-existing `aiPendingDoc_.has_value()` branch, which needs a live
  AI response to populate — not independently triggerable via CLI, so
  this task's confidence comes from the dedicated unit tests plus a clean
  compile/link/smoke-boot, not an interactive screenshot of the panel
  itself).
- **SYS-W14-09** `[BLOCKED]` `P3` — Web persistence & export verification (blocked on
  CNA/browser — see NEXT.md).
- **SYS-W14-10** `[DONE]` `P3` — Editor UI to attach a script to an object
  (`Mc3Object::scriptId`, `mc3/include/MeshCraft/Mc3/Mc3Object.hpp`). Scripts
  themselves are fully editable (`doc.scripts`, the "Scripts" tab,
  `STAB-0705`), but no UI anywhere sets an object's `scriptId` to attach one
  — confirmed via `grep -rn "scriptId" src/MeshCraft/` (zero hits). Found via
  `missing.md`'s 2026-07-18 update.
  **Implementation (2026-07-18):** added a "Script" combo box in
  `PropertiesPanel.cpp`, directly mirroring the existing Material combo's
  structure (a `(none)` sentinel + every `doc.scripts` key, first-match
  mixed-selection handling via the file's existing `allMatchStr` helper,
  `pushUndo()`/`markModified()` on change) — minus the color swatch,
  plus a `(type)` suffix per entry read from `Mc3Script::type`. Placed
  right after the Material block. No new call sites elsewhere needed since
  this is a pure UI addition over an existing, already-parsed/written
  field. Verify: full rebuild + `ctest -j"$(nproc)"` (126/126, unchanged
  count — no new test registered, this is UI-only over an already-tested
  field), manual `--screenshot` smoke test (clean GL state, no crash).
- **SYS-W14-11** `[DONE]` `P2` — Editor support for opening/saving `.mc3.json`
  (`Mc3Document::loadFromJsonFile`/`saveToJsonFile`, `mc3/src/Mc3Document.cpp`,
  R109's semantic-JSON format). These already worked at the library level,
  but `MeshCraftApplication_FileOps.cpp`'s Open/Save/Save As exclusively
  called the XML load/save path. Found via `missing.md`'s 2026-07-18 update.
  **Implementation (2026-07-18):** `EditorAlgorithms.hpp`'s
  `resolveSaveAsPathAlg()` (used by Save As) now returns a 3-way
  `SaveAsFormat{Xml,Mcb,Json}` instead of an `isMcb` bool — `.json`/
  `.mc3.json` route to the JSON writer, same as `.mcb` already did to the
  MCB writer; updated its 6 call sites (1 production, 5 test) accordingly,
  plus 2 new test cases. New shared `MeshCraftApplication::loadSceneFileDispatched()`
  (extension dispatch: `.mcb`→MCB reader, `.json`→JSON parser, else→XML
  parser) replaces 3 independent copies of this same dispatch that had
  drifted apart (startup load, "Open Recent File", and the Open File
  dialog, which previously only special-cased `.mcb` and would have
  mis-parsed a `.json` file as XML) — extracted per this codebase's own
  established de-duplication idiom (`AUD-031`'s precedent). `saveFile()`
  (File > Save, re-saving to the already-open path) also gained the same
  3-way dispatch. **Found and fixed a related pre-existing bug while doing
  this:** `saveFile()` previously called the XML writer *unconditionally*
  regardless of `currentFile_`'s actual extension — so re-saving a file
  originally opened as `.mcb` (via File > Save, not Save As) silently
  overwrote it with XML content under the same `.mcb` filename. Fixed as
  part of the same 3-way dispatch, not filed separately, since it's the
  exact same root cause this task was already fixing.
  **Verify:** full rebuild + `ctest -j"$(nproc)"` (126/126, +2 assertions
  in the existing `mc3_commands`/`editor_commands_test.cpp` test, no new
  test binary). Real end-to-end check (not just unit-level path
  resolution): converted `test/house.mc3.xml` to `.mc3.json` via the
  `mc3` library directly, loaded it through the app's real command-line
  startup path (`./MeshCraft house.mc3.json --screenshot`), and confirmed
  the rendered screenshot is **byte-identical** to loading the original
  `.mc3.xml` — proves the dispatch and the underlying R109 JSON
  round-trip are both correct together, not just in isolation.
- **SYS-W14-12** `[DONE]` `P3` — Editor UI for asset metadata
  (`Mc3Object::assetMetadata`, `Mc3AssetMetadata` struct,
  `mc3/include/MeshCraft/Mc3/Mc3AssetMetadata.hpp` — 23 fields across 6
  shapes: strings, string vectors, `array<float,3>`, two different maps,
  a bool, two floats). Zero editor UI before this (confirmed via
  `grep -rn "assetMetadata\|AssetMetadata" src/MeshCraft/`, zero hits).
  Found via `missing.md`'s 2026-07-18 update.
  **Implementation (2026-07-18):** `assetMetadata`'s own header comment
  says it's "present only on definitions where authored/known" — so the
  right home is the existing "Defs" tab
  (`MeshCraftApplication_UiLeftPanel.cpp`, which already edits
  `document_.definitions[selectedDefId_]`), not the main Properties panel
  (which edits placed scene objects/Instances, not the definitions they
  reference). New collapsible "Asset Metadata (R111)" tree node there,
  gated behind a checkbox since `assetMetadata` is optional. All 23
  fields editable via 3 local field-editor lambdas covering the 3
  most-repeated shapes (plain string, `array<float,3>`, comma-separated
  string-vector) plus 2 explicit map editors (`sockets`, `lods`) mirroring
  the existing Meta editor's key-rename pattern
  (`PropertiesPanel.cpp`) adapted for non-string values. Tag lists
  (`semanticTags`/`styleTags`/`regionTags`/`periodTags`/`materialSlots`)
  use a single comma-separated line rather than 5 separate full add/
  remove list UIs — proportionate given there are 5 of them, still fully
  editable, not read-only.
  **Verify:** full rebuild + `ctest -j"$(nproc)"` (126/126, unchanged —
  UI-only over an already-parsed/written field, no new library-level
  test needed). Manual `--screenshot` smoke test loading the pre-existing
  `test/asset_metadata_library_import.mc3.xml` fixture (which already
  populates every field this UI edits, including non-empty `sockets`/
  `lods` maps and tag lists — a real exercise of the exact data shapes,
  not a hand-picked trivial case) — clean GL state, no crash.
- **SYS-W14-13** `[DONE, data-field editing only — see scope note]` `P3` —
  Editor UI for library metadata and imports (`Mc3Document::library`
  (`Mc3LibraryInfo`), `Mc3Document::imports` (`Mc3Import`), the `.mc3lib`
  reusable-library file format). Zero editor UI — no panel showed/edited
  the library namespace/version, no way to add/remove an `<imports>`
  entry. Found via `missing.md`'s 2026-07-18 update.
  **Readiness investigation (2026-07-18):** this row's own caution asked
  whether the format is still evolving upstream before building UI against
  it. Checked the sibling `mesh-world` repo's own tracking directly (not
  assumed): `git log` there is already at `R114` (well past `R101`/`R110`),
  and both rows are marked `[x]` DONE in `mesh-world/plan.md` — `R110`'s
  own note explicitly says "dependency pruning" was a deliberate, separate,
  already-decided scope cut, not an open design question. `R112`+ build
  new *consuming* features (facade modules, street layout) on top of
  `R101`/`R110`, not changes to the `library`/`imports` schema itself. So
  the underlying fields are stable, not still moving — the original
  caution doesn't apply; proceeded with implementation.
  **Implementation:** new "Library (.mc3lib)" section in the Scene
  Properties panel (`PropertiesPanel.cpp`, shown when nothing is
  selected, same place `model`/`unit`/`coordinate_system` are already
  edited) — a checkbox creates/clears the optional `doc.library`, then
  namespace/version text fields plus a "Recompute" button calling the
  existing `Mc3Document::computeLibraryContentHash()` (never auto-computed
  implicitly, matching that method's own documented contract). New
  "Imports" tab (`MeshCraftApplication_UiLeftPanel.cpp`) for
  `doc.imports` — an ordered `std::vector`, not a map like
  scripts/triggers/sounds, so a direct index-based row editor (namespace/
  source/hash per row + Remove), mirroring the existing Triggers tab's
  per-step editor (`trigger.steps`, the closest existing "vector of small
  multi-field structs" precedent in this file) rather than the map-keyed
  pattern the other new-tab additions used.
  **Scope, deliberately narrowed:** only the DATA fields are editable.
  The separate `saveToLibraryFile()`/`saveToLibraryJsonFile()`/
  `loadFromLibraryFile()`/`loadFromLibraryJsonFile()` file-I/O surface
  (a distinct "Save/Open as .mc3lib" feature, with its own contract —
  throws if `library` is unset) is NOT wired into the File menu in this
  pass; a user can populate `library`/`imports` and save via the regular
  Save/Save As (XML/JSON) path, but there's no dedicated "Save As
  Library" action yet. Left as a smaller, separate follow-up rather than
  conflating two different features in one pass.
  **Verify:** full rebuild + `ctest -j"$(nproc)"` (126/126, unchanged —
  UI-only over already-round-trip-tested R101/R110 fields, no new library-
  level test needed). `test/validate_xsd.py` against a hand-built fixture
  with `<library>`/`<imports>` populated; manual `--screenshot` smoke test
  loading it (no crash, clean GL state).
- **SYS-W14-14** `[DEFERRED, human-authorized-precedent decision — see note]`
  `P2` — Wire up `coordinate_system` so it actually affects something. The
  invalid `"left_handed_y_up"` combo option was already removed
  (`STAB-0713`, combo now only offers the 2 XSD-valid values), but
  `Mc3Document::coordinateSystem` is still write-only — stored and
  editable, but never read anywhere in `mc3togltf/` or `src/MeshCraft/`
  (confirmed by direct grep, no read sites found). A user can correctly set
  this field through the GUI and it has zero effect on rendering or export.
  Found via `missing.md`'s 2026-07-18 update.
  **Investigation + decision (2026-07-18):** this row's own text offered
  two paths — implement the real conversion, or document as declarative-
  only (matching `SYS-W5-03`'s precedent). Investigated what "implement
  the real conversion" would actually require: `right_handed_z_up` vs.
  `right_handed_y_up` differ only in which axis is "up," so a single
  root-transform (axis swap) applied once at the scene root would in
  principle be enough — mechanically simple. But that root transform would
  need to be applied **consistently across every independent root-matrix
  call site**: `SceneRenderer::draw()`, `drawEmissivePass()`,
  `drawCsgGizmos()`, and `computeObjectWorldMatrix()` (used by picking and
  the Properties panel's world-position display) in the editor, plus
  `GltfExporter.cpp`'s own root node construction — matching this
  project's own already-documented "two independent geometry generators"
  risk (`NEXT.md`'s architecture notes: the editor and exporter build
  geometry independently with no shared code, and are known to require
  careful manual sync, e.g. deliberately-opposite triangle winding).
  Missing even one of these call sites would make gizmos/picking silently
  disagree with rendered geometry — a worse, harder-to-notice bug than
  today's "field does nothing" gap. Chose the same resolution as
  `SYS-W5-03`'s precedent: **documented as a known, by-design limitation
  rather than implemented**, since a correct implementation needs the same
  careful multi-site consistency discipline `SYS-W1-05`/`06`/`07` used for
  cycle-guards, not a single-file quick fix, and a "why not just do it"
  investigation belongs in this file even when the answer is "not yet."
  **What DID ship, matching the `rotation_units`/`euler_order` precedent
  exactly (`STAB-0701`):** `checkRotationConventionNotice()`
  (`MeshCraftApplication_FileOps.cpp`) now also checks `coordinateSystem`
  and includes it in the combined load-time status-bar notice (was
  rotation-only, now covers all 3 declarative-but-unhonored conventions in
  one combined message when more than one applies). `MC3_FORMAT.md`
  updated: `coordinate_system`'s value table was missing
  `right_handed_z_up` entirely (a pre-existing doc bug, fixed in passing),
  plus a new paragraph stating plainly that this field is honored
  **nowhere**, not even by the exporter (a weaker guarantee than
  `rotation_units`/`euler_order`, which are at least export-honored), with
  the same reasoning as above for why. Verify: full rebuild +
  `ctest -j"$(nproc)"` (126/126, unchanged), `test/validate_xsd.py` against
  a hand-built `coordinate_system="right_handed_z_up"` fixture (valid),
  manual `--screenshot` smoke test loading it (no crash, clean GL state).
- **SYS-W14-15** `[DONE, material texture slots only — see scope note]` `P3`
  — Native file-browse dialog for texture/mesh path fields (currently plain
  `ImGui::InputText` boxes everywhere, including the Import OBJ dialog and
  material texture-slot fields). Notably, the CNA dependency already ships
  a `FileDialog` device (SDL backend) that was never called from anywhere
  in `src/MeshCraft/` or `mc3togltf/` — the capability existed one layer
  down and was simply unused. Found via `missing.md`'s 2026-07-18 update.
  **Implementation (2026-07-18):** enabled CNA's `CNA_DEVICES` CMake option
  (a mesh-craft-side `CMakeLists.txt` consumer choice, off by default in
  CNA itself — not a CNA edit) to compile in `CNA::Devices::FileDialog`.
  Added a "..." Browse button next to each of the 5 material texture-slot
  fields in `PropertiesPanel.cpp`'s `texField` lambda, gated on
  `FileDialog::getIsSupportedProperty()` (false on Web/iOS, where the
  manual-entry-only field is kept instead of a dead button).
  `FileDialog::ShowOpenFile()` is asynchronous and its callback may run on
  a different thread per its own documented contract — handled with a new
  `PendingFileBrowse` result box (`MeshCraftApplication.hpp`) mirroring
  `AiRequestResult`'s existing threading pattern (mutex + atomic done
  flag), drained once per frame in `Update()` by feeding the resolved path
  into the *existing* `pendingDropTexture_`/`hoveredTexSlot_`/
  `hoveredTexMatId_` pipeline the OS drag-and-drop path (D6) already used
  — full reuse of that consumption logic, no duplication.
  **Scope, deliberately narrowed:** only the 5 material texture slots are
  wired up (the single highest-value, most-repeated integration point).
  The Import OBJ dialog's path field and other manual-path fields (SVG/
  sound/music/embed src, mesh source) still use manual entry only — left
  as a smaller follow-up rather than wiring every path field in one pass,
  now that the `PendingFileBrowse` plumbing exists to reuse. Verify: full
  rebuild + `ctest -j"$(nproc)"` (126/126, unchanged count), manual
  `--screenshot` smoke test (clean GL state). The actual native dialog
  itself was **not** interactively triggered during verification (would
  spawn a real, hanging `zenity`-class process in this sandbox, per
  `FileDialog`'s own doc comment about why its test-swap hook exists) —
  confidence instead comes from reusing already-tested consumption logic
  verbatim and mirroring `AiAssistant`'s already-proven threading idiom.
- **SYS-W14-16** `[DONE]` `P2` — Undo/redo structural-guarantee audit.
  Ran two parallel research agents over the full ~332 `pushUndo()`-family
  call-site population (one covering command/input/macro files, one
  covering panel/UI files) to exhaustively enumerate real undo-coverage
  gaps, rather than choosing a structural-enforcement mechanism (Command
  pattern, mutation-tracking proxy, debug-build snapshot-diff assertion)
  up front with no evidence of where it would actually pay off. The audit
  found two distinct bug classes, no full-blown enforcement mechanism, and
  fixed every confirmed instance of both:
  1. **Missing `pushUndo()` entirely** (6 confirmed): `toggleIsolate()`
     (`MeshCraftApplication_Commands.cpp`) had no snapshot on either the
     activate or deactivate path; the Loop and Autoplay checkboxes
     (`MeshCraftApplication_Anim.cpp`) mutated `act.loop`/`act.autoplay`
     directly via `Checkbox(label, bool*)` with `pushUndo()` called only
     *after* the in-place mutation already happened (`ImGui::Checkbox`
     writes `*v` and returns `true` in the same call, so a `pushUndo()`
     inside `if (Checkbox(...))` always snapshots the *new* value, not the
     old one — fixed via the standard local-copy-then-writeback pattern,
     which needs no `IsItemActivated()` gate since Checkbox only fires
     once per click); the macro `show_all` verb
     (`MeshCraftApplication_Macro.cpp`) was a copy/paste omission relative
     to its sibling `hide` verb just above it; the Default Camera combo
     and the States tab's Position/Rotation/Scale `DragFloat3` fields plus
     the UV tab's Rotation `DragFloat` (all in
     `src/MeshCraft/Scene/PropertiesPanel.cpp`) had no `pushUndo()` call
     at all on any path.
  2. **AUD-036-style dead pattern** (21 confirmed: 15 in
     `PropertiesPanel.cpp`, 6 in `MeshCraftApplication_UiLeftPanel.cpp`) —
     `if (ImGui::SliderInt/SliderFloat(...)) { if (IsItemActivated())
     pushUndo(); ... }`, i.e. the activation check nested *inside* the
     changed-check. `SliderInt`/`SliderFloat` fire `changed=true` every
     frame during a drag but `IsItemActivated()` is only true on the
     first frame, so nesting them means `pushUndo()` can silently never
     fire if the two don't land on the same frame. Covered: every
     primitive segment/subdivision slider (Sphere, Cylinder/Cone, Disk,
     Capsule, Grid X/Z, IcoSphere, Torus), Extrude path-segments and all
     3 CrossSection-type segment/side sliders, both Material-tab
     Roughness/Metallic/Alpha-Cutoff sliders (Properties panel and
     left-panel copy), and the left-panel's Light Spot Angle, Light
     Falloff, and Camera FOV sliders. Fix applied uniformly: hoist the
     widget's return value into a locally-scoped `bool`, check
     `IsItemActivated()` unconditionally right after (outside the
     changed-check), and only run the mutation/`markModified()` inside
     `if (thatBool)`.
  Verified: full rebuild + `ctest -j"$(nproc)"` (127/127, unchanged
  count — pure bug fixes to existing UI, no new tests needed), manual
  `--screenshot` smoke test against `test/house.mc3.xml` (clean GL state,
  UI renders correctly including the SYS-W14-13 Library section and
  Default Camera combo). A mechanical grep-based check confirmed zero
  remaining instances of `IsItemActivated()` nested inside a
  widget-changed `if`-block anywhere in either audited file. Honesty
  note: this pass fixed every gap the two audits actually found in the
  files they covered; it did **not** re-derive a fresh call-site count
  across the *entire* codebase from scratch, so it should not be read as
  a proof that all 332+ `pushUndo()` sites are now individually correct —
  only that the two recognized bug classes (missing call, dead
  activation-gate pattern) are eliminated everywhere the audits looked.
  No structural enforcement mechanism (Command pattern, mutation-tracking
  proxy, debug assertion) was added; if this class of bug recurs, that
  would be the next escalation, not another manual audit pass.
- **SYS-W14-17** `[DONE, no further UI needed — see investigation]` `P3` —
  Dedicated Area object properties panel. `STAB-0721` added an `"Area
  (trigger zone)"` label when editing an Area object, but the editor still
  falls through to the same generic Box size fields immediately below (by
  design at the time, per that commit's own comment — `ObjectType::Area`
  structurally carries a Box-shaped primitive with no parser case of its
  own). Found via `missing.md`'s 2026-07-18 update.
  **Investigation (2026-07-18):** checked whether Area objects have any
  distinct data to edit beyond size, and whether `doc.triggers`
  (`Mc3Trigger`) links back to a specific object. Confirmed neither:
  `Mc3XmlParser.cpp` genuinely has no `ObjectType::Area` case in
  `parsePrimitive`'s switch (Area really is just a Box-shaped primitive
  with a different `type` tag, nothing more), and `Mc3Trigger` itself
  (`id` + a list of `{type, ref}` steps) carries no field referencing an
  `Mc3Object` at all — an Area and a same-named Trigger are correlated
  only by convention for whatever runtime consumes the scene, not by any
  schema-level link this editor could surface. So there is no hidden field
  or cross-reference to wire up: the labeled-Box-editor added by
  `STAB-0721` (which resolved the original "cosmetically unclear, looks
  like a plain box" complaint) is the complete, correct fix given the
  current data model — not a partial one. No further UI work needed;
  closing without new code, matching this backlog's own precedent for
  findings that turn out to already be fully resolved on investigation
  (e.g. `SYS-W5-05`, `SYS-W7-01` in the archived history).

- **SYS-W14-18** `[DONE]` `P1` — Lua scripting execution engine.
  `Mc3Object::scriptId`/`doc.scripts` (`Mc3Script`, `type="lua"`) parse,
  serialize, round-trip (XML/JSON/MCB), and are fully editable in the
  "Scripts" tab and per-object "Script" field — but nothing in this
  codebase ever interprets a script's `source` text. Grepped: no Lua
  library is vendored/linked anywhere in `CMakeLists.txt`/`mc3togltf/`.
  `Mc3Object.hpp:101-108`'s own doc comment on `scriptId` frames the
  intended use precisely: "compose-time" scripting for placing imported
  definitions into a definition's own `assetMetadata.sockets` (R103/R104,
  `mesh_world_revival.md` §6/§7) — "nothing in mesh-craft itself executes
  it, that's each consumer's own choice of Lua binding/sandbox." Found
  2026-07-20 during a "what does the format support that the editor
  doesn't" review (user request).
  **Open design questions (asked before implementing), and the user's
  answers:** library -- user said "look at how ../mesh-world does it"
  rather than pick blind; investigated via a research fork and found
  mesh-world already has a complete, working, sandboxed reference
  implementation (`Mc3ScriptRunner.cpp`/`LuaRuntime.cpp`: **Lua 5.4.7 +
  sol2 v3.3.0**, `sol::lib::base/math/string/table` only, `io`/`os`/
  `debug`/`package`/`dofile`/`loadfile`/`load`/`collectgarbage` all
  nil'd, `require` overridden to throw) -- mirrored exactly rather than
  inventing a different one. `Mc3ScriptRunner.hpp`'s own header comment
  explicitly frames this as "R104 asks for the Lua binding to live in
  MeshCraft itself... deliberately deferred for v1" -- this task
  completes that deferral. API surface -- user chose **broader**: not
  just R103/R104's original "socket placement" scope, but read/write
  object properties too. Execution trigger -- user chose **explicit
  button + trigger step**, no automatic compose-time execution.
  **Implementation:** new `Editor::LuaScriptRunner`
  (`include/MeshCraft/Editor/LuaScriptRunner.hpp` +
  `src/.../LuaScriptRunner.cpp`), CNA-free. Two globals bound per run
  (fresh `sol::state` each time, no persistent state, matching
  mesh-world's own convention): `def` -- mesh-world's own
  `PlacementApi` unchanged (`place`/`place_at`/`has_socket`), except
  `target` is nullable here (mesh-world's compose-time caller always
  has one; MeshCraft's own call sites don't always); `scene` -- this
  repo's own addition for the broader ask: `scene:find(idOrName)`
  (two-pass id-then-name recursive search) returns a handle with
  `get/set_position/rotation/scale`, `get/set_visible`,
  `get/set_material`, read-only `.name`/`.id`. **Safety addition beyond
  mesh-world's own reference** (which has none -- acceptable for an
  offline/CLI tool, not for an interactive editor): an instruction-
  count execution budget (`lua_sethook`, `LUA_MASKCOUNT`, 50M
  instructions) aborts a genuine infinite loop instead of hanging the
  whole editor UI thread forever -- empirically confirmed against a
  real `while true do end` script in the test below (aborts in well
  under a second, not hung).
  Lua/sol2 vendored directly into the `MeshCraft` editor target's own
  `CMakeLists.txt` (`GIT_REPOSITORY`/`GIT_TAG` convention, matching
  this repo's other FetchContent deps) -- NOT into the shared `mc3/`
  library, since mesh-world's own "materially bigger commitment"
  concern was specifically about forcing the dependency onto every
  `mc3/` consumer (CNA/mc3togltf/mc3tomcb too); that constraint doesn't
  apply to the editor target itself, which already vendors much
  heavier dependencies (Manifold/ImGui). Excludes `onelua.c`/`ltests.c`
  from the compiled sources -- the git mirror's checkout (unlike
  mesh-world's own lua.org release-tarball fetch) includes both, and
  `onelua.c` `#include`s every other `.c` file into one translation
  unit, which would duplicate-define every Lua symbol once anything
  actually links against both it and the individual `.c` files.
  **UI wiring:** Scripts tab gained a "▶ Run Script" button
  (`target` = current selection's first object if any, else `nullptr`);
  the Triggers tab's `run-script` step (`SYS-W14-19`'s `fireTrigger()`)
  now actually calls `luaScriptRunner_.run()` with the same
  selection-based target convention, instead of reporting "not
  implemented." `pushUndo()`/`modified_` are now called by
  `fireTrigger()` -- but ONLY when the trigger has at least one
  `run-script` step (checked up front, snapshot taken before the loop
  so Ctrl+Z covers a script's mutation even if a later step then
  errors) -- a `play-action`/`play-sound`/`play-music`-only trigger
  still pushes no snapshot at all, matching `SYS-W14-19`'s own original
  behavior for those 3 step types (they never touched `document_`).
  **Tests:** new `test/lua_script_runner_test.cpp` (`lua_script_runner`
  ctest) -- CNA-free, so this exercises REAL Lua execution directly
  (not a mirror): empty-source no-op, plain-script success, syntax/
  runtime error reporting, sandbox verification (`os`/`io` are nil,
  `require` blocked), the infinite-loop budget actually aborting a real
  `while true do end`, `scene:find()` by id/name/not-found, real
  property read/write mutating the actual `Mc3Object`, `def:place()`/
  `place_at()`/`has_socket()` matching mesh-world's own contract
  exactly (including the no-target and unresolved-definitionRef error
  cases). `test/trigger_fire_test.cpp` (extended, mirrors
  `fireTrigger()`'s control flow like before, but now uses the REAL
  `LuaScriptRunner` for its `RunScript` case instead of a mock, since
  that class itself is CNA-free) adds: a real script mutating a real
  object end-to-end through the trigger path, a Lua syntax error
  correctly distinguished from a missing-ref skip in the aggregated
  status message, and the conditional-pushUndo() behavior (only for a
  trigger containing a `run-script` step). Manual smoke test: a real
  scene fixture with a script + a `run-script` trigger loads and
  renders via `--screenshot` with no crash and a clean GL state.
  Full rebuild + 157/157 `ctest` (was 155 after `SYS-W14-19`; 2 new
  registrations, `lua_script_runner` and the `trigger_fire_test`
  extension needed no new registration).
- **SYS-W14-19** `[DONE]` `P1` — Trigger event-firing system.
  `doc.triggers` (`Mc3Trigger`: `id` + ordered `{type, ref}` steps —
  `play-action`/`play-sound`/`run-script`/`play-music`) parse/serialize/
  round-trip and are editable in the "Triggers" tab, but nothing anywhere
  calls a trigger's steps — there is no event system (collision, click,
  timer, or otherwise) that would fire one, and no explicit "run this
  trigger now" action either. `MC3_FORMAT.md` documents this as a
  deliberate "data model first" limitation (N5), not a bug. Found
  2026-07-20 (same review as `SYS-W14-18`).
  **Tractable independent of `SYS-W14-18`:** `play-sound`/`play-music`
  steps only need to call into real playback (the "Audio" tab's
  `AudioPreview` already does real `SoundEffect`/`SoundEffectInstance`
  playback manually — reusable); `play-action` only needs to call the
  existing action-playback system the Timeline already drives. Only
  `run-script` steps depend on `SYS-W14-18`. Smallest useful slice: an
  explicit "Fire" button per trigger in the Triggers tab that executes
  all its non-script steps for real, so triggers become testable/usable
  in the editor even before any live in-scene event system exists.
  **Implementation:** `MeshCraftApplication_UiLeftPanel.cpp`'s Triggers
  tab gained a `fireTrigger(const Mc3::Mc3Trigger&)` lambda, a `▶` button
  per trigger row, and a prominent "▶ Fire Trigger" button in the
  selected-trigger detail view. `PlayAction` sets
  `currentActionName_`/`animTime_`/`animPlaying_` — the exact same trio
  the load-time `autoplay="true"` logic already uses
  (`MeshCraftApplication.cpp:279-284`), so a fired action drives the same
  Timeline playback a user pressing Play would see. `PlaySound`/
  `PlayMusic` call `audioPreview_.play()`, reusing the Audio tab's own
  real `SoundEffectInstance` playback. `RunScript` originally reported
  "scripting not implemented yet" via `setStatusMsg` -- superseded the
  same day by `SYS-W14-18` (see its own entry above), after which
  `RunScript` actually runs the referenced script via
  `luaScriptRunner_.run()`, with the current selection's first object
  (if any) as the `def` target; `pushUndo()`/`modified_` are now called
  conditionally (only when the trigger has a `run-script` step -- see
  `SYS-W14-18`'s entry for why). A missing `step.ref` (dangling
  reference to a deleted sound/action/track/script) is also reported,
  not silently skipped. **Known limitation,
  inherited from the editor's existing architecture, not introduced
  here:** there is only ever one "current action" and one
  `AudioPreview`-shared audio slot in this editor (see `AudioPreview`'s
  own "one-shared-preview-at-a-time" doc comment) — a trigger with
  multiple `play-action` (or multiple `play-sound`/`play-music`) steps
  has each later one replace the previous rather than layering, since
  there is no multi-track mixer to layer into.
  **Tests:** new `test/trigger_fire_test.cpp` (`trigger_fire` ctest) --
  `MeshCraftApplication` is CNA-coupled and not headlessly instantiable,
  so this mirrors `fireTrigger()`'s exact control-flow shape against
  plain `Mc3Document` data and mock playback-state/audio-call trackers.
  Covers: `PlayAction` sets/doesn't-set the mock playback trio for a
  valid/missing ref; `PlaySound`/`PlayMusic` record the right resolved
  src (relative-vs-absolute) and loop flag; `RunScript` is reported, not
  silently dropped; a 4-step trigger mixing all cases aggregates its
  fired/missing/unimplemented counts correctly in one status message.
  Full rebuild + 156/156 `ctest` (was 155; `trigger_fire` is the only
  new registration).
- **SYS-W14-20** `[DONE]` `P2` — Scene States runtime switching.
  `doc.sceneStates` (`Mc3SceneState`: named visibility/position/rotation/
  material overrides — e.g. "day"/"night" variants) parse/serialize/
  round-trip and are editable in the "States" tab, but selecting/applying
  a state does nothing to the live `document_` objects in the editor —
  there is no code path that actually applies a state's overrides.
  `MC3_FORMAT.md` documents this as a deliberate "data model first"
  limitation (N6), not a bug. Found 2026-07-20 (same review).
  **Implementation:** a "▶ Apply State" button in the States tab's
  selected-state detail view (`MeshCraftApplication_UiLeftPanel.cpp`).
  Uses the existing `flatFindById()` (same lookup `def:place()`-style
  code elsewhere in the app already relies on) to resolve each
  override's `id` against the live document, then writes only the
  override's SET fields (`visible`/`position`/`rotation`/`material`) --
  an unset field on an override leaves the object's existing value
  untouched, matching `Mc3ObjectOverride`'s own documented "only present
  attributes are overridden" contract exactly. `pushUndo()`/`modified_`
  are conditional on at least one override actually resolving to a real
  object -- a state whose overrides ALL reference missing/deleted
  object ids (or an empty-overrides state) pushes no undo snapshot at
  all, matching this session's own established no-op-undo discipline
  (same pattern as `SYS-W14-18/19`'s `fireTrigger()`). A missing object
  id is reported in the status message, not silently skipped, and
  doesn't block the other overrides in the same state from applying.
  **Tests:** new `test/scene_state_apply_test.cpp`
  (`scene_state_apply` ctest) -- CNA-free mirror (same rationale as
  `trigger_fire_test`). Covers: multiple overrides actually mutating
  the real objects; only-the-set-fields-applied (an unset field is
  left alone); a missing object id reported without blocking a sibling
  override in the same state; the no-op-undo cases (all-missing,
  empty-overrides) pushing zero undo snapshots.
  Full rebuild + 158/158 `ctest` (was 157).
- **SYS-W14-21** `[DONE]` `P2` — Wire `Mc3ImportResolver` into the editor.
  `mc3/src/Mc3ImportResolver.cpp` (R101, `resolve()`/`resolveAndMergeInto()`)
  is a complete, tested, standalone implementation that resolves a
  document's `<imports>`/`mc3lib://name@version` references against a
  search-directory list and merges namespace-qualified definitions in —
  this session used/extended it directly (`F18`, import-chain depth cap).
  But grepped `src/MeshCraft/`: zero references to `Mc3ImportResolver` or
  `resolveAndMergeInto` anywhere. `SYS-W14-13`'s own scope note only
  covers the separate "Save As .mc3lib" file-I/O surface being unwired
  from the File menu — it does not mention import RESOLUTION at all, and
  neither does any other tracked row (checked `plan.md`/`NEXT.md`/
  `missing.md`). An `<instance definition="ns:id">` referencing an
  imported (not locally-defined) definition therefore renders as nothing
  in the live editor — the data round-trips correctly, but composing a
  scene from a shared imported library doesn't actually work
  interactively, only the metadata describing that it should.
  **Implementation (per the user's confirmed 3-part plan):**
  new `MeshCraftApplication::resolveImports()`
  (`MeshCraftApplication_FileOps.cpp`) — a no-op if `document_.imports`
  is empty; otherwise constructs `Mc3::Mc3ImportResolver` with
  `document_.sourcePath` (the loaded document's own directory, matching
  how both `Mc3XmlParser`/`Mc3JsonParser` already set `sourcePath`) as
  the sole search directory, calls `resolveAndMergeInto(document_)`, and
  reports success/failure via `setStatusMsg()`. A resolution failure
  (missing library file, hash mismatch, import cycle, or the resolver's
  own `F18` chain-depth cap) is caught and reported, not rethrown — it
  does not fail an otherwise-loadable document, matching this session's
  established "a recoverable data issue shouldn't make an otherwise-
  loadable document unopenable" precedent (same shape as `SYS-W14-18`'s
  script-error handling). (1) Called automatically right after every
  load, alongside `checkRotationConventionNotice()`, at all 4 load call
  sites: initial launch-with-file-argument (`MeshCraftApplication.cpp`),
  the Open-file-dialog confirm handler (`MeshCraftApplication_UiOverlays.cpp`),
  `OpenRecentFile` (`MeshCraftApplication_FileOps.cpp`), and autosave
  recovery (`MeshCraftApplication_FileOps.cpp`). (2) An explicit
  "Resolve Imports" button added to the Imports tab
  (`MeshCraftApplication_UiLeftPanel.cpp`) for re-resolving after
  editing the import rows without a full reload.
  **Tests:** new `test/resolve_imports_test.cpp` (`resolve_imports`
  ctest) — CNA-free mirror of `resolveImports()`'s exact control flow
  (`MeshCraftApplication` is not headlessly instantiable, same rationale
  as `trigger_fire_test`/`scene_state_apply_test`), against the REAL
  `Mc3ImportResolver` and real `.mc3lib.xml` fixture files on disk (not
  a mock — the resolver itself is already exhaustively covered by
  `mc3/test/import_resolver_test.cpp`, so this test only needs to prove
  the thin wrapper's own contract). Covers: empty-imports no-op (no
  status message, no resolver constructed); successful resolution
  merging the imported definition into `document.definitions` with the
  correct singular/plural status message; a missing-library failure
  caught and reported via the error-status shape without propagating,
  leaving `document.definitions` untouched (not a partial merge).
  Full rebuild + 159/159 `ctest` (was 158).
- **SYS-W14-22** `[DONE]` `P3` — `Mc3Texture::mipMaps` unused downstream.
  Round-trips (XML/JSON/MCB) and now has editor UI (added this session,
  `F21`, 2026-07-20 audit) but grepped `SceneRenderer.cpp`/
  `GltfExporter.cpp`: zero reads of `.mipMaps` in either. The live
  viewport never generates/uses mipmaps for any texture regardless of
  this flag, and the glTF exporter's sampler `minFilter` selection
  ignores it too. **Outcome:** generate mipmaps for live-viewport
  textures when set (or document why not, e.g. a real performance
  tradeoff); have the exporter choose a mipmapped `minFilter`
  (`LINEAR_MIPMAP_LINEAR` etc.) instead of its current unconditional
  choice when `mipMaps` is true.
  **Implementation (exporter half):** `GltfExporter.cpp`'s `buildTextures()`
  now only requests the `*_MIPMAP_*` sampler `minFilter` variant when
  `tex.mipMaps` is actually true; when false it emits the plain
  `LINEAR`/`NEAREST` filter instead of unconditionally requesting a
  mipmapped one regardless of the author's choice.
  **Live-viewport half: investigated, documented as blocked on a CNA API
  gap, not implemented.** `SceneRenderer::loadOrGetTexture()` constructs
  a `Texture2D` via CNA's file-loading constructor
  (`Texture2D(assetName, graphicsDevice)`), which does not expose a
  mipmap parameter — only the raw-pixel constructor
  (`Texture2D(device, w, h, bool mipMap, format)`) does, and it requires
  the caller to already have decoded pixel data + a manual `SetData()`
  call, not a drop-in replacement for the file-loading path. Confirmed
  in `cna/src/.../EasyGLGraphicsBackend.cpp`: the backend's own comment
  states outright that "CNA does not generate mipmaps by default" for
  the Linear filter this path uses, and no public
  `Texture2D::GenerateMipmap()`/equivalent exists to retrofit onto an
  already-loaded texture (`SamplerState`'s mipmap knobs only bias/clamp
  *existing* mip levels, they don't generate new ones). Making the live
  viewport actually honor `mipMaps=true` therefore requires extending
  CNA's own `Texture2D` API — out of scope per `CLAUDE.md`'s "no CNA
  changes without owner permission" (same class of boundary as
  `AUD-042`'s Android/SDL_RENDERER gap). Left as a legitimate, explicitly
  documented gap rather than an unexamined oversight; a future CNA-side
  change (owner-authorized) could add a `mipMap` bool to the asset-loading
  `Texture2D` constructor(s) to close it.
  **Tests:** extended `test/texture_sampler.mc3.xml` with a 5th texture
  (`tex_no_mipmaps`, `mip_maps="false"`) and `mc3togltf/test/
  texture_sampler_test.py`'s `EXPECTED`/`EXPECTED_URI` tables to assert
  it exports with the plain (non-mipmap) `LINEAR` `minFilter`, and bumped
  the missing-texture-warning count assertion from 4 to 5. Full rebuild +
  159/159 `ctest` (unchanged count -- extended an existing test, not a
  new one).
- **SYS-W14-23** `[DONE]` `P3` — `Mc3Texture::colorSpace` unused at export.
  `srgb`/`linear` round-trips and is editable, but grepped
  `GltfExporter.cpp`: zero hits for `colorSpace` -- no conversion or
  even a `KHR_texture_transform`-style hint is ever applied at export
  regardless of the declared value. **Outcome:** at minimum, apply the
  correct glTF `sRGB`/linear encoding convention per texture slot
  (baseColor/emissive are sRGB by glTF convention; normal/metallic-
  roughness/occlusion are linear) using this field rather than a fixed
  per-slot assumption, so an author's explicit override is honored.
  **Implementation:** glTF 2.0 has no per-texture color-space override —
  the encoding convention per slot is fixed by the spec itself, not
  something an exported file can honor per-author-intent. So instead of
  a silent "fixed per-slot assumption" with the field simply doing
  nothing, `GltfExporter.cpp`'s `buildMaterial()` now takes `doc.textures`
  and, for each of the 5 PBR texture slots it resolves
  (`base_color_texture`/`emissive_texture` = sRGB;
  `normal_texture`/`metallic_roughness_texture`/`occlusion_texture` =
  linear), checks the referenced texture's own declared `color_space`
  against that slot's mandated encoding and emits an explicit warning
  (naming the material, texture id, slot, declared value, and required
  value) when they conflict — surfacing likely-mistaken authoring intent
  (e.g. a normal map declared `color_space="srgb"`) instead of silently
  discarding the signal. Does not fail the export, matching this
  session's established warn-don't-fail precedent for informational
  mismatches (SVG-texture-unresolved, missing-texture-file).
  **Tests:** new `test/color_space_export.mc3.xml` (2 materials — one
  with correctly-declared color_space on its `base_color_texture`/
  `normal_texture`, one with both deliberately wrong) + new
  `mc3togltf/test/color_space_export_test.py` (`mc3togltf_color_space_export`
  ctest) — asserts the matched material produces no color-space warning
  and the mismatched material's warning text names the right slot,
  declared value, and required value, for both mismatch directions
  (sRGB-slot-given-linear and linear-slot-given-sRGB). Full rebuild +
  160/160 `ctest` (was 159).
- **SYS-W14-24** `[DONE]` `P3` — UV mapping box/sphere projection not
  implemented. `Mc3UvMapping.projection` (`planar`/`box`/`sphere`) only
  ever produces the primitive's default planar unwrap in both the live
  viewport and `mc3togltf` -- confirmed honestly warned about, not
  silently dropped (`GltfExporter.cpp:656-673`, `AUD-024`'s own comment:
  "Box/Sphere projection genuinely isn't implemented anywhere"), but the
  feature itself is still missing. **Outcome:** implement triplanar/box
  and spherical UV generation for at least the exporter (editor-viewport
  parity is a nice-to-have, not required, since `mc3togltf` is the
  ground truth for exported appearance).
  **Implementation:** two new `MeshData` methods
  (`mc3togltf/src/MeshBuilder.hpp`/`.cpp`), called from `buildMesh()`
  before `applyUvMapping()`'s existing scale/offset/rotation so both
  compose (matching how scale/offset/rotation already layer on top of
  the default planar unwrap): `applyBoxProjectionUv()` -- per vertex,
  picks the dominant axis from that vertex's own normal (or, if normals
  are absent, the direction from the mesh's local bounding-box center)
  and projects onto the other two axes using raw, non-normalized
  local-space coordinates (deliberate -- ties apparent texture scale to
  object size, the standard box/triplanar convention in DCC tools, and
  lets the existing `scale_u`/`scale_v` remain the tiling control rather
  than adding a second competing scale concept). `applySphereProjectionUv()`
  -- equirectangular mapping (`atan2`/`asin` of the position relative to
  the mesh's local bounding-box center) normalized to `[0,1]`, degenerating
  to `(0.5, 0.5)` at zero radius instead of NaN. The old "not implemented"
  warning in `GltfExporter.cpp`'s `buildMesh()` is gone -- both
  projections now actually run.
  **Tests:** extended `test/uv_mapping_export.mc3.xml` (added
  `SphereProjectedBox`, kept `BoxProjectedBox`) and `mc3togltf/test/
  uv_mapping_export_test.py` to assert real geometry instead of the old
  "still the default unwrap + warns" behavior: `BoxProjectedBox`'s
  projected U/V now span `[-0.5, 0.5]` (raw local coords, unit box);
  `SphereProjectedBox`'s U/V stay within `[0,1]` and vary across the
  box's 8 distinct corner directions (not degenerate/constant); neither
  emits the old warning. **Caught during verification:** the fixture's
  first draft used `<!-- ... -- ... -->`-style XML comments, which is
  invalid XML (a comment body may not contain `--` anywhere, not just at
  the close) -- `xsd_validation` failed on it immediately; fixed by
  rewording the comments, not by suppressing the check. Full rebuild +
  160/160 `ctest` (unchanged count -- extended existing tests, not new
  ones).
- **SYS-W14-25** `[DONE]` `P3` — MCB compression reserved but never
  implemented. `McbWriter.cpp:640` always writes the compression byte as
  `0` (no compression); `McbReader.cpp:1412` throws "compressed format
  not yet supported" if it's ever nonzero -- so the reader's own
  rejection path is permanently dead code against this codebase's own
  writer, only reachable via a hypothetical future/foreign writer.
  **Outcome:** implement real compression (e.g. zlib/deflate, matching
  the sanity-limit philosophy already used elsewhere in this reader) or
  remove the reserved byte/exception entirely if compression is not
  actually planned, rather than leaving a half-declared feature.
  User picked "implement real compression (zlib)".
  **Implementation:** `saveToBinary`/`saveToFile` (`McbWriter.hpp`/`.cpp`)
  gained an opt-in `bool compress = false` parameter (default preserves
  every existing caller's output byte-for-byte) -- when true, the document
  payload is serialized into an in-memory buffer, zlib-`compress2()`'d at
  `Z_BEST_COMPRESSION`, and written as `flags=MCB_FLAG_COMPRESSED` +
  `reserved(2)` + `uncompressedSize(u32)` + `compressedSize(u32)` +
  compressed bytes, instead of the plain `flags=0` + `TAG_OBJ` + document
  bytes an unset flag means -- the 8-byte magic/version/flags/reserved
  prefix itself is byte-identical either way, so old readers still
  correctly recognize a compressed file by its flags byte rather than
  misparsing it. `McbReader.cpp`'s `loadFromBinaryImpl()` reads the two
  size fields (each validated against a 512MB sanity ceiling BEFORE being
  used to size any buffer -- the same zip-bomb defense philosophy as
  `kMcbMaxStringLen`/`kMcbMaxCollectionCount` elsewhere in this reader,
  applied to the new claimed-uncompressed/claimed-compressed-size fields),
  reads exactly that many compressed bytes, `uncompress()`s them (erroring
  if the actual decompressed size doesn't match what was claimed -- not
  just trusting zlib's return code alone), and parses the document from an
  `istringstream` over the result via the exact same rootTag+`readDocument()`
  path the uncompressed branch already used. `mcb/CMakeLists.txt` gained
  `find_package(ZLIB)`, guarded exactly like the root CMakeLists.txt's own
  SQLite3/OpenSSL/LibXml2 blocks (optional, not `REQUIRED` -- a toolchain
  without zlib still builds `Mcb` fine, `compress=true` throws a clear
  "this build was compiled without zlib support" error instead, and
  reading a compressed file throws a distinct "requires zlib" error
  instead of misparsing it). No editor UI wiring (a compression toggle in
  the Save-As-MCB dialog) -- out of scope; the task's own outcome text
  was about the reader/writer capability itself, not UI exposure.
  **Tests:** new `mcb/test/compression_test.cpp` (`mcb_compression`
  ctest, guarded `#ifdef MESHCRAFT_HAS_ZLIB` with a SKIP fallback,
  matching `mc3_registry_test.cpp`'s established pattern for optional
  system deps) -- round-trip correctness, the flags byte actually
  reflecting compression, real measured size reduction on a repetitive
  document, and 4 zip-bomb/corruption defenses (oversized claimed
  uncompressed size, oversized claimed compressed size, truncated
  payload, corrupted deflate bytes). Updated the pre-existing
  `mcb_roundtrip_test.cpp` `testCompressedFlagRejected` (AUD-019) -- its
  hand-crafted "compressed flag set" fixture predates the new
  size-prefix fields the real format now requires, so it's genuinely
  malformed now rather than merely "unimplemented"; split into a
  zlib-available variant (still fails safely, now via "unexpected end of
  stream" while reading the missing size fields, plus a new sibling test
  proving a PROPERLY-shaped compressed document loads successfully) and
  a no-zlib variant (fails via a distinct "requires zlib" message) so
  AUD-019's "clear error, not silent misread" guarantee is verified in
  both build configurations. Full rebuild + 161/161 `ctest` (was 160).
- **SYS-W14-26** `[DONE]` `P3` — Light brightness sent to glTF without
  physical unit conversion. `GltfExporter.cpp:1040` sets
  `intensity = light.brightness` identically for Directional/Point/Spot,
  but glTF's `KHR_lights_punctual` spec defines directional intensity in
  lux and point/spot intensity in candela -- different physical units --
  so the same raw authored number produces a physically-inconsistent
  result depending on light type, and may look different in a
  glTF-conformant viewer than in MeshCraft's own live preview.
  **Outcome:** either apply a documented, deliberate conversion (or
  scale factor) per light type, or clearly document that `brightness` is
  an arbitrary non-physical unit not meant to round-trip physically
  through glTF (a legitimate design choice too, if made explicitly
  rather than left as an unexamined gap).
  User picked "implement conversion to physical units".
  **Implementation:** confirmed `brightness` has no real-world meaning
  anywhere in this codebase today (the live viewport only uses it to
  color a light's gizmo icon, `SceneRenderer.cpp`'s `drawLightGizmos()`
  -- never for actual illumination, since the viewport has no real-time
  lighting pass at all), so this adopts an established, documented
  external convention rather than inventing an arbitrary one: the same
  Watts-to-photometric formula Blender's own glTF exporter uses (the de
  facto reference other glTF pipelines already expect). New
  `lightIntensityForExport()` (`GltfExporter.cpp`): directional passes
  `brightness` through unconverted (matches Blender's Sun-lamp
  convention -- W/m^2 usable directly as lux); point/spot convert via
  `brightness / (4*pi) * 683` (`kPbrWattsToLumens = 683.0`, the CIE
  photometric luminous-efficacy constant at the 555nm peak-sensitivity
  wavelength, matching Blender's own Point/Spot-lamp formula). A
  deliberate, documented per-type scale factor, not a claim that
  `brightness` is now a fully physically-calibrated quantity -- no
  editor-side lux/candela input mode was added, and the live viewport's
  gizmo-only use of `brightness` is unaffected (out of scope, matching
  this session's established "exporter is the ground truth for exported
  appearance" precedent for the other P3 gaps).
  **Tests:** extended `mc3togltf/test/light_export_test.py` (existing
  `light_export_all_types.mc3.xml` fixture: Sun brightness=2.0,
  Bulb brightness=5.0, Torch brightness=3.0) with intensity-value
  assertions for all 3 exported light types, checking each against the
  documented formula. No existing test asserted on `intensity` before
  this, so nothing needed correcting for the new values -- only new
  coverage was added. Full rebuild + 161/161 `ctest` (unchanged count --
  extended an existing test, not a new one).
- **SYS-W14-27** `[DONE]` `P3` — Ambient light dropped on glTF export.
  `GltfExporter.cpp:1023-1027` (`STAB-0696`) explicitly warns and drops
  any `LightType::Ambient` light -- glTF 2.0 core + `KHR_lights_punctual`
  genuinely has no ambient-light equivalent, so this is a real spec gap,
  not an oversight, and the current behavior (warn, don't silently drop)
  is already honest. Filed as an open task, not closed as won't-fix like
  `coordinate_system`/`rotation_units` (`STAB-0701`/`SYS-W14-14`),
  because unlike those two a *lossy but useful* approximation is
  possible here (e.g. bake the ambient contribution into every affected
  material's emissive channel at export time) -- worth a real
  scope/priority decision rather than defaulting to won't-fix.
  **Outcome:** either implement an approximation (emissive-bake being
  the most tractable) or formally close as won't-fix with the same
  rigor `STAB-0701` got, instead of leaving it an implicit, undecided gap.
  User picked "implement emissive-bake approximation".
  **Implementation:** `addLights()` (`GltfExporter.cpp`) sums every
  `<ambient>` light's `color × brightness` in the document into one
  combined RGB contribution (multiple ambients combine the same way
  multiple real fill lights would), then bakes it into every material in
  `model.materials`' `emissiveFactor`, tinted by that material's own
  `baseColorFactor` (so the approximation still reflects each material's
  own albedo instead of washing every material to the same flat color)
  and clamped to `[0,1]` (matching glTF core's `emissiveFactor` range --
  no `KHR_materials_emissive_strength` extension used). Applied even when
  every light in the document is ambient-only (the function's existing
  `if (lightsArray.empty()) return` early exit -- meaning "no
  `KHR_lights_punctual` lights to add" -- sits AFTER the bake, not
  before it, since the two are independent concerns). The per-ambient
  warning text was updated to explain it was baked, not just dropped.
  **Tests:** extended the existing `test/light_export_all_types.mc3.xml`
  fixture with a `mat_box` material (`base_color="0.8 0.6 0.4 1.0"`)
  assigned to its one object, and `mc3togltf/test/light_export_test.py`
  with an assertion that `mat_box`'s exported `emissiveFactor` exactly
  equals `(ambient.color × ambient.brightness) × mat_box.base_color`
  (`Fill`'s `color=0.2/0.2/0.2, brightness=0.5` → expected
  `[0.08, 0.06, 0.04]`) -- verified this matches the real exported bytes,
  not just that some warning fired. Full rebuild + 161/161 `ctest`
  (unchanged count -- extended an existing test, not a new one).

---

## Audit-derived tasks (AUD-###)

67 tasks were created in total originally (see the Session log's "Net
across all 16 AUD-### rows remaining in this active backlog" note for the
exact breakdown of where they came from): 57 from the session-1 audit
(re-verified against current source in session 2), plus session-2's
adversarial re-open/new-defect findings. Ordered by discovery (AUD-001..057
in original severity order, followed by the session-2 additions). Status is
re-verified per row, not copied from a prior summary — do not trust a DONE
marker without checking its cited commit/verify command.

**Update (2026-07-18):** 61 of those original 67 rows are now `DONE` and
archived to [`docs/history/plan_20260718.md`](docs/history/plan_20260718.md)
(full evidence/resolution text preserved there). **The active row count
below is NOT "67 minus 61 = 6"** — a later-same-day independent re-audit
(2026-07-18) added 10 more rows, `AUD-064` through `AUD-073` (all now
`DONE`; see the Session log note above), bringing this file's current
total to **16 AUD-### rows** (the 6 remaining from the original 67, plus
those 10). `test/validate_plan_consistency.py` is authoritative for this
count — re-run it rather than trusting this paragraph.

**Update (2026-07-20, later same day, after SYS-W14-18..27):** a third
independent fresh audit (4 parallel agents: build/test health, core-code
bug hunt, docs/architecture staleness, editor UX/wiring gaps) added
`AUD-074` through `AUD-081` (all `DONE`), bringing the total to
**24 AUD-### rows** — every finding from this audit round, including the
two documentation-only staleness findings (`AUD-080` MCB_FORMAT.md,
`AUD-081` TESTING.md), is now closed. `AUD-078` (fog) was downgraded from
the audit's own initial P1/P2 "visibly double-applied" framing to P2
"dead/incorrect code" after direct empirical investigation found zero
actual pixel difference — see its own row for the full story.

### AUD-025 `[DEFERRED]` `P2` `W7` · embed: mesh source is treated as a literal OBJ path — node exports with no mesh while export exits 0 'Written'
- **Component:** mc3togltf/src/GltfExporter.cpp buildMesh()
- **Evidence:** GltfExporter.cpp:586-593: `if (obj.type == ObjectType::Mesh && !obj.meshSource.empty()) { try { md = loadObjMesh(ctx.basePath, obj.meshSource); } catch (...) { std::cerr << Warning ...; ctx.stats.warnings++; return -1; } }`. The exporter never checks for the `embed:<id>` form the mc3 parser/writer round-trip (Mc3XmlParser.cpp:743-749). `loadObjMesh` tries to open a file literally named 'embed:tree', fails, warning printed, node gets no mesh. main.cpp:93 then prints 'Written:' and returns 0. Documented (MC3_FORMAT.md, STAB-0194/0549) and a warning + stats.warnings signal it, so not fully silent — but the tool still reports success with dropped geometry.
- **Outcome:** Resolve embed:<id> against doc.embeds (parse the referenced/inline GLB and merge its meshes), or make the missing-geometry case a non-zero exit / clearer failure rather than 'Written' success.
- **Tests:** The existing embed_mesh_source_test.py locks in the degraded behavior; add resolution or assert a distinct exit/status when geometry is dropped.
- **Verify note:** Evidence is accurate; no correction needed. Severity P2 is appropriate: the geometry loss is signalled by a stderr Warning and the stats.warnings counter (only shown with --show-stats), and the behavior is documented in MC3_FORMAT.md and locked in by a passing test (mc3togltf/test/embed_mesh_source_test.py, STAB-0549) that asserts exit 0 + warning + empty node is the intended, accepted limitation. It is therefore a low-severity known limitation rather than a silent data-loss bug, but the core claim (tool reports 'Written'/exit 0 while dropping the embed:-referenced mesh) is factually correct.
- **Status note:** Existing embed_mesh_source_test.py locks in the current documented-limitation behavior (exit 0 + warning + empty node); the adversarial re-verify pass judged this an accepted limitation, not a defect requiring a code change. Left TODO-eligible for W14 embed: resolution work (SYS-W14-05).

### AUD-038 `[DEFERRED]` `P3` `W9` · Undo history is a bounded 20-entry whole-document deep-copy stack; oldest entries are silently dropped (informational — answers the audit question, by-design)
- **Component:** include/MeshCraft/MeshCraftApplication.hpp, src/MeshCraft/MeshCraftApplication_Commands.cpp
- **Evidence:** include/MeshCraft/MeshCraftApplication.hpp:629 `static constexpr int kUndoMax = 20;`. pushUndo() at Commands.cpp:331-336 does `undoStack_.push_back(deepCopyDoc(document_)); if (undoStack_.size() > kUndoMax) undoStack_.erase(undoStack_.begin()); redoStack_.clear();` — so beyond 20 operations the oldest snapshot is silently discarded (no user notice), and each snapshot is a full deep copy of the entire document (all objects/materials/textures/actions), which for large scenes is 20 full-scene copies of RAM. This is standard bounded-history behavior, not a correctness bug; noting because the audit explicitly asked whether history is silently dropped (yes) and because it is the constraint that makes finding #1's whole-doc-snapshot model expensive.
- **Outcome:** No code change required for correctness. Optionally document the 20-op limit in the UI and/or consider a command-delta model if memory becomes a concern.
- **Tests:** n/a (behavioral note).
- **Verify note:** The `static constexpr int kUndoMax = 20;` is at include/MeshCraft/MeshCraftApplication.hpp:632, NOT line 629 (line 629 is the unrelated `void checkRotationConventionNotice();` declaration). The pushUndo() body spans Commands.cpp:331-337 and uses `static_cast<int>(undoStack_.size()) > kUndoMax` (the paraphrase omitted the cast, but the semantics match). Severity P3/informational is appropriate.
- **Status note:** By-design bounded history; audit's own correction confirms no code change is required for correctness.

### AUD-042 `[TODO]` `P2` `W8` · Android build path force-selects SDL_RENDERER, guaranteeing the editor UI would not render if built for Android
- **Component:** CMakeLists.txt (Android backend auto-select)
- **Evidence:** CMakeLists.txt:101-102 `if(ANDROID) set(MESH_CRAFT_GRAPHICS_BACKEND_UPPER "SDL_RENDERER")`. Combined with the fact (Finding 1) that the editor's ImGui UI and post-FX only work through ImGui_ImplOpenGL3 + SDL_GL_GetProcAddress (which need an SDL GL context, not an SDL_Renderer), an Android build would compile against a backend the editor cannot render on. README.md:211 correctly marks Android as `never attempted` so it is not falsely claimed working, but the CMake default choice bakes in a non-functional editor for the one platform that is forced onto SDL_RENDERER.
- **Outcome:** If Android support is intended, wire the editor to a backend it can actually render on (e.g. GLES via EASYGL) or gate the GUI editor off on Android with a clear message, rather than auto-selecting SDL_RENDERER which the UI layer cannot drive.
- **Tests:** An Android NDK configure that either selects a GL-capable backend or errors clearly; not currently testable here (no NDK installed).
- **Verify note:** Refinement (does not change the verdict): the editor↔SDL_RENDERER incompatibility is not Android-specific — the identical breakage occurs for ANY build configured with -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER on desktop, since the editor's ImGui path (MeshCraftApplication.cpp:212-213) is hardwired to OpenGL3 with no SDL_Renderer branch. What is Android-specific is that the force at CMakeLists.txt:101-102 makes SDL_RENDERER non-optional there (the user cannot pick EASYGL). So the finding is slightly understated in scope but accurate as stated. Severity P2 stands.
- **Blocked:** No Android NDK in this environment; also intersects CNA backend behavior (out of scope).
- **Status note:** `AUD-039b`'s runtime check (commit `58a7f03`) now means an Android build (if one were attempted) would refuse to launch the editor UI with a clear error, rather than silently opening a non-functional window -- so the "renders nothing with no indication why" consequence this finding warns about is closed. The CMake-level force-select itself (`if(ANDROID) set(...SDL_RENDERER)`) is unchanged; this task stays `TODO` because the root cause (Android has no path to a GL-capable backend at all) is still open and untestable here (no NDK).
- **Status note:** Android force-selects SDL_RENDERER (a backend the editor cannot render on); no NDK available to test in this environment. Depends on the AUD-039b decision (hard failure vs. real backend) for a principled fix.

### AUD-052 `[TODO]` `P1` `W11` · CI is permanently parked under `.github_/` — GitHub Actions never runs; the repo has zero CI
- **Component:** .github_/workflows/ci.yml
- **Evidence:** .github_/workflows/ci.yml:1-7 self-documents the deactivation: "DEACTIVATED: this file lives under `.github_/` (note the trailing underscore), not `.github/`, so GitHub does NOT run it." Confirmed: `ls -d .github` -> "No such file or directory"; the only workflow file tracked in git is `.github_/workflows/ci.yml`; `git log --all --diff-filter=D -- .github/workflows/ci.yml` is empty, so an active workflow dir never existed. Net effect: no automated build/test has ever gated any push or PR on master/develop. NOTE the stated activation blocker is also questionable — the comment claims the push token "lacks the `workflow` scope", but `git remote -v` shows origin is an SSH remote (`git@github-openeggbert:openeggbert/mesh-craft.git`), and the workflow-scope restriction is an OAuth/PAT-over-HTTPS guard that does not apply to SSH key pushes; the file can most likely be un-parked by renaming `.github_` -> `.github` and pushing over SSH.
- **Outcome:** Rename `.github_` -> `.github` (over the SSH remote) so the workflow actually runs, or explicitly document why CI must remain disabled; the current state means every correctness/build regression ships unverified.
- **Tests:** After renaming, confirm a run appears under Actions for a test push to develop and that all matrix jobs go green; verify branch protection requires the checks.
- **Verify note:** Core finding fully verified; evidence is accurate. Two refinements: (1) Severity is arguably P2 rather than P1 — the deactivation is intentional, honestly self-documented in the file itself, and tracked as a known owner-blocked task (STAB-0650, referenced in NEXT.md:263-264 and MEMORY as "blocked on owner"/PAT-rotation). It is a documented capability GAP, not a hidden defect or an untruthful capability claim (the file is candid that CI is off). Under the P1 rubric "test gap on critical path" it is defensible, but a reader should know it is a deliberate, disclosed state. (2) The secondary NOTE — that the stated `workflow`-scope blocker is questionable because origin is SSH — is technically plausible (the workflow-scope gate applies to OAuth/HTTPS-PAT pushes; personal SSH keys authenticate as the full user and are generally exempt) but it is SPECULATIVE and not provable from the repo: the local SSH remote does not establish how the maintainer or any automation actually pushes, and the maintainer's notes explicitly cite a "PAT-rotation blocker." Treat that NOTE as a hypothesis, not an established fact.
- **Status note:** OWNER-GATED, correctly not counted as done. .github_/workflows/ci.yml stays parked pending a workflow-scoped push token; documented in NEXT.md and the file itself. Do not mark this DONE without an actual git history entry moving .github_ -> .github.

### AUD-053 `[TODO]` `P2` `W11` · Even if un-parked, CI never builds or tests the actual editor (18.5k LOC) — only 4 standalone CLI/format libs
- **Component:** .github_/workflows/ci.yml
- **Evidence:** ci.yml:39-46 restricts the matrix to `component: [ mc3, mcb, mc3togltf, mc3tomcb ]` and configures each standalone (`cmake -S ${{ matrix.component }}`). ci.yml:19-24 admits the gap: "TODO (not yet wired up): a full editor build job. The root project pulls in the CNA sibling repo ... That covers the remaining root-level tests (smoke_test, xsd_validation, mc3_registry, mc3_ai, mc3_commands, and the render-labeled tests — 20 tests...)". The uncovered `src/MeshCraft/**` editor is 32 .cpp files / 18,529 LOC — the largest first-party surface — plus all render/smoke/AI/registry/XSD tests registered in CMakeLists.txt:408-798 run in no automation.
- **Outcome:** Add an editor build+test job (checkout ../cna + ../sharp-runtime, install SDL3/GL/xvfb) so the smoke/render/xsd/registry/ai CTest suite is exercised, or accept and document that the editor is validated only by hand.
- **Tests:** New CI job runs `ctest` at the repo root under xvfb-run and passes the render-labeled tests.
- **Verify note:** Minor line-range imprecision only: the root-level test registrations actually span lines 408–807 (the final add_test, mc3_ai at line 807, and set_tests_properties through ~808), not 408–798 — CMakeLists.txt is 810 lines total. All other numbers (matrix components, 32 files / 18,529 LOC, the TODO quote at 19-24) are exact. Substance of the finding is unchanged.

### AUD-057 `[TODO]` `P2` `W11` · Editor build pulls sibling repos via add_subdirectory(../cna) / SHARP_RUNTIME with no version pin — non-reproducible and unguarded by CI
- **Component:** CMakeLists.txt
- **Evidence:** CMakeLists.txt:107 `add_subdirectory(../cna CNA_dep)` and the link lines at CMakeLists.txt:338/354/365 (`CNA ... SHARP_RUNTIME ...`) consume two sibling repos purely by relative path, with no GIT_TAG, commit, or version check — whatever happens to be checked out at ../cna and ../sharp-runtime is used. README.md:48-50 documents the checkout requirement but not any pinned revision, and README.md:211 records that this is actively fragile: "a *fresh* rebuild now fails — `../sharp-runtime` gained a new Emscripten-only regression ... (16 `-Werror` failures + 1 hard `std::chrono::clock_cast` compile error)." With CI parked (finding 1) and the editor uncovered even if un-parked (finding 2), nothing detects such sibling-repo breakage.
- **Outcome:** Pin the sibling repos to explicit commits/tags (submodule or a recorded SHA + a configure-time check), and gate the editor build in CI against those pinned revisions so cross-repo regressions are caught.
- **Tests:** Add a configure-time assertion that ../cna and ../sharp-runtime are at expected revisions; add a CI editor-build job that fails when they drift/break.
- **Blocked:** Fixing the sibling-repo build regressions themselves is out of scope (../cna and ../sharp-runtime are owned elsewhere); only mesh-craft's pinning/CI wiring is in scope here. **Partially resolved below** — the configure-time-assertion half is done; the CI-job half remains blocked on AUD-052 (CI itself is parked).
- **Resolved (partial):** commit `d2943e3` — verify: `cmake -S . -B <build-dir> 2>&1 | grep AUD-057` (expect no output when siblings are at the recorded SHA; a `-DMESHCRAFT_CNA_VERIFIED_SHA=0...0` override reproduces the warning path)
- **Status note:** Added a non-fatal configure-time check (`meshcraft_check_sibling_revision`, CMakeLists.txt) that `git rev-parse HEAD`s `../cna` and `../sharp-runtime` and prints `message(WARNING ...)` — not `FATAL_ERROR` — when either has drifted from the two `MESHCRAFT_*_VERIFIED_SHA` values recorded in the same file (currently `../cna` @ `d0c21ee6`, `../sharp-runtime` @ `5cdaafb2`, both re-verified against a passing 101/101 `ctest` run just before recording). Deliberately non-fatal: CNA is developed by a separate process (CLAUDE.md — "No CNA changes without owner permission. A separate Claude Code instance handles CNA."), so it legitimately moves ahead of this project's last-verified pin; a hard `FATAL_ERROR` would block that work every time it advances. Verified both the silent-when-matching path and the warning-when-drifted path (temporarily overwrote `MESHCRAFT_CNA_VERIFIED_SHA` with a bogus SHA, confirmed the warning fires and configure still exits 0, then restored the correct value and re-verified a clean reconfigure + full rebuild + 101/101 ctest). **Remaining (not done):** the CI-job half (gate the editor build in CI against these pins) is blocked on AUD-052 — there is no running CI to wire it into. The pinned SHAs are a manually-updated marker, not automation; they need a human/agent to re-run `ctest` and bump them after intentionally picking up new sibling commits, which is not enforced by anything.

### AUD-064 `[DONE]` `P0` `W1` · Grid subdivisions_x × subdivisions_z are each individually capped but their PRODUCT is not — freezes the live editor every frame
- **Component:** src/MeshCraft/Renderer/SceneRenderer_Extrude.cpp (drawGridDynamic)
- **Evidence:** Found via a fresh, independent adversarial re-audit of the parser/renderer/exporter surface (not part of the original 2026-07-11 audit). `Mc3XmlParser.cpp:677-678` clamps `subdivisions_x`/`subdivisions_z` each to `kMaxTessellation=4096` individually via `attrCountBudgeted`. `AUD-059`'s `DocumentBudget.totalTessellationWeight` (Mc3XmlParser.cpp) only sums these two fields' raw values (max +8192 to the running total, far under the 500,000 ceiling) — it bounds "many objects each near the per-field cap summed across a document", not one object's two capped dimensions multiplied together. `drawGridDynamic` (SceneRenderer_Extrude.cpp:691, called every frame from SceneRenderer.cpp:840 for any visible Grid object) builds `cols*rows` vertices and `subX*subZ*6` indices from scratch with no cache, unlike every other primitive (Capsule/Torus use `getOrBuildXMesh()` caches). A single, XSD-valid `<grid subdivisions_x="4096" subdivisions_z="4096"/>` therefore requests ~16.8M vertices per frame and silently wraps the `uint16_t` index buffer.
- **Outcome:** Bound `cols*rows` before allocating, matching the existing `drawExtrudeDynamic`'s own `numVerts>65535` placeholder-fallback convention in the same file.
- **Tests:** New `test/grid_stress.mc3.xml` fixture (`subdivisions_x="4096" subdivisions_z="4096"`) + `smoke_test_grid_stress` ctest (TIMEOUT 30, `test/smoke_test.sh`).
- **Resolved:** commit `cd4f310` — verify: `ctest -R smoke_test_grid_stress`
- **Status note:** Empirically confirmed as a real freeze, not just a theoretical risk, before writing the fix: `MeshCraft grid_stress.mc3.xml --screenshot out.png` did not complete within a 20s timeout on the unpatched binary (reproduced via `git stash` of the one-line fix); with the fix it completes in ~3s, the same as normal startup. Severity assessed as P0 (unsafe/untrusted-input-driven crash-or-freeze on a live, interactive path — an AI-generated or hand-edited `.mc3.xml`/`.mc3.json` can trigger it with no warning), one step above `AUD-059`'s own P1, since that finding's fix does not cover this exact multiplicative case. Scope was deliberately kept to the one confirmed, reproduced instance (`drawGridDynamic`); a closely related but unconfirmed-severity issue in the same file (`drawExtrudeDynamic` builds its full buffer before its own existing `numVerts>65535` bailout, wasting allocation but not looping past it) was found by the same audit pass and is a good candidate for a small follow-up, not folded into this fix to keep it minimal and independently verifiable.

### AUD-065 `[DONE]` `P1` `W1` · .mc3.json load path applies none of the XML path's per-field tessellation clamps — bypasses AUD-005/AUD-064 hardening entirely
- **Component:** mc3/src/Mc3JsonParser.cpp (toPrimitive/toCrossSection/toExtrude)
- **Evidence:** Found via the same fresh adversarial re-audit as `AUD-064`. `Mc3XmlParser.cpp` clamps every `segments`/`sides`/`subdivisions_x`/`subdivisions_z` attribute to `kMaxTessellation=4096` via `attrCountBudgeted` (`AUD-005`). `Mc3JsonParser.cpp:79,83-84,101-102,143` (pre-fix) read the equivalent JSON keys (`segments`, `subdivisionsX`, `subdivisionsZ`, `sides`, extrude `segments`) with a raw `j["..."].get<int>()` and no clamp whatsoever — a `.mc3.json` with `"segments": 100000000` or `"subdivisionsX": 4096, "subdivisionsZ": 4096` (the exact `AUD-064` trigger) passed straight through, reaching `SceneRenderer`/`mc3togltf` unclamped. Since `.mc3.json` is a fully supported, documented second serialization of the same `Mc3Document` AST (not a debug/internal format), this is a real, reachable bypass of both `AUD-005`'s and `AUD-064`'s protections, not a hypothetical one.
- **Outcome:** Apply the same per-field clamps (mirroring `Mc3XmlParser.cpp`'s bounds exactly) on the JSON load path.
- **Tests:** New `mc3/test/json_input_budget_test.cpp` (`mc3_json_input_budget` ctest, 13 assertions) — mirrors `mc3_input_budget`'s XML fixture shape: hostile `segments`/`subdivisionsX`/`subdivisionsZ`/extrude `sides`/`segments` values are clamped to `<=4096`, negative `segments` floors at `0`, legitimate values pass through unchanged.
- **Resolved:** commit `4416bd9` — verify: `ctest -R mc3_json_input_budget`
- **Status note:** Fixed with a small `clampTess()` helper (`Mc3JsonParser.cpp`, mirrors `Mc3XmlParser.cpp`'s `kMaxTessellation`/per-field minimums) applied at all 6 read sites. Full CNA-free `mc3` standalone suite: 20/20 `ctest` (was 19); also built and verified from the root `b-release` tree directly (target + `ctest -R` both pass there too). **Not covered by this fix, deliberately (separate, larger follow-ups):** (1) the JSON path is still not wired into `Mc3XmlParser.cpp`'s document-wide `DocumentBudget`/total-tessellation-weight tracking (`AUD-059`) — that budget is a `thread_local` file-static with no shared header, and covers materials/textures/embeds/actions/etc. too, not just tessellation; (2) `Mc3JsonParser::parseString`'s `Mc3LoadPolicy` parameter is still entirely unused (a separate, pre-existing finding from the same audit pass, not folded in here). **Could not run the full root `ctest` suite end-to-end** while verifying this — see §4's new blocker note in `NEXT.md` (an unrelated, external `../easy-gl`/`../meta-gl` mid-edit breaking the CNA-linked build, discovered incidentally, out of this repo's bounds to fix).

### AUD-066 `[DONE]` `P1` `W6` · McbReader.cpp reserves the FULL claimed collection count up front, before validating the stream actually contains that many elements
- **Component:** mcb/src/McbReader.cpp (rU32Bounded / 13 `.reserve(n)` call sites)
- **Evidence:** Found via the same fresh adversarial re-audit as `AUD-064`/`AUD-065`. `rU32Bounded()` (`McbReader.cpp:167-176`) validates a claimed collection count against `kMcbMaxCollectionCount=10,000,000` — a real bound, but only proof the count is under the sanity ceiling, not that the stream actually contains that many elements. 13 call sites (`doc.lights`/`doc.cameras`/`doc.objects`/`obj->children`/`ch.keyframes`/`act.channels`/`doc.imports`/`doc.includes`/`obj->tags`/`obj->variantDefinitions`/extrude cross-section `customPoints`/path `points`/a generic array helper) then called `.reserve(n)` with that full claimed count, before reading a single element. A tiny, corrupted/malicious file (valid header/fields up to one oversized-but-legal count, then EOF) could therefore force a large up-front allocation.
- **Outcome:** Cap the up-front `.reserve()` to a small constant hint regardless of the claimed count; let a genuinely large legitimate file grow the vector via normal amortized reallocation as elements are actually read.
- **Tests:** New `mcb/test/reserve_bomb_test.cpp` (`mcb_reserve_bomb` ctest, 11 assertions).
- **Resolved:** commit `8b8b7a6` — verify: `ctest -R mcb_reserve_bomb`
- **Status note:** Fixed with `reserveHint(n) = min(n, 4096)` applied at all 13 sites. **Empirically measured, not just reasoned about** (matching `AUD-064`'s own before/after-timing precedent): the regression test hand-crafts a 43-byte file whose `lights` array count is patched to 9,000,000 then truncated immediately after, and measures `/proc/self/status` VmPeak (virtual-memory high-water mark) around the load attempt — **NOT RSS**: a `vector<Mc3Light>::reserve(9000000)` was confirmed to leave RSS essentially flat (Linux lazily commits pages; `reserve()` never touches/constructs elements) while jumping VmPeak from ~6MB to ~850MB, so an RSS/`getrusage`-based check would have silently passed regardless of the bug. Verified both directions: the unpatched reader (checked via `git stash` of the one-line-per-site fix) grows VmPeak by ~824MB and the test correctly FAILS against it; the patched reader grows VmPeak by only ~350KB and the test passes. Also confirms a legitimate 50-light document still round-trips all 50 lights (the hint doesn't truncate real data). Full root `ctest`: 135/135 (was 134). Left a pointer comment in `mcb_corruption_test.cpp` (whose own header comment previously implied the reader was already fully memory-safe against arbitrary corruption) noting this refinement — that file's crash-safety/rejection-correctness sweeps were never positioned to catch an allocation-SIZE issue like this one.

### AUD-067 `[DONE]` `P1` `W7` · frameAxes() has no zero-length tangent guard — a degenerate polyline point produces NaN that aborts the whole export
- **Component:** mc3togltf/src/MeshBuilder.cpp (frameAxes)
- **Evidence:** Found via the same fresh adversarial re-audit as `AUD-064`/`065`/`066`. `frameAxes()` (`MeshBuilder.cpp:660-671`) computes a binormal `b` then divides by its own length `bl` with no guard, unlike `norm3()` (`:654-657`, a few lines above in the same file) which does guard (`if (l > 1e-6f)`). `samplePath`'s `Polyline` case (`:769-793`) computes each segment's tangent as `{dx,dy,dz} * tang`; two consecutive identical `<point>` elements make `dx=dy=dz=0`, so the tangent is `{0,0,0}` regardless of the `tang` fallback value (a plausible authoring/AI mistake — duplicate points in a hand-edited or generated path). Unlike the neighboring `Bezier` path (`:830-832`, which explicitly falls back to `{0,1,0}` for the same degenerate case), `Polyline` has no such fallback and feeds `{0,0,0}` straight into `frameAxes`, dividing `0/0` into NaN. That NaN reaches `GltfExporter.cpp`'s finiteness gate and aborts the ENTIRE export with a generic "non-finite vertex position" error, not just a confusing diagnostic — a full, otherwise-valid extrude mesh becomes unexportable because of one duplicate point anywhere in its path.
- **Outcome:** Guard the `bl` division in `frameAxes()`, matching `norm3()`'s existing convention in the same file.
- **Tests:** New `mc3togltf/test/degenerate_polyline_extrude_test.py` (`mc3togltf_degenerate_polyline_extrude` ctest) — mirrors `degenerate_cone_test.py`'s pattern (export must succeed; parse the GLB's POSITION/NORMAL accessor data AND declared min/max, assert every float is finite).
- **Resolved:** commit `02255b6` — verify: `ctest -R mc3togltf_degenerate_polyline_extrude`
- **Status note:** Fixed by guarding only the `bl` division (`if (bl > 1e-6f) { ... }`, else `b` stays `{0,0,0}`) — sufficient, not just "doesn't crash": `n` is `cross(b,t)`, which is always `{0,0,0}` when `t=={0,0,0}` regardless of what `b` is, so no fallback direction for `b` would avoid a degenerate frame anyway; the honest result of a zero-length path segment is a single pinched (zero-radius but finite) ring at that one point, matching how a genuinely zero-length input segment should look, not a NaN-corrupted mesh. **Empirically verified both directions** (`git stash` of the one-line fix, matching this session's own established before/after convention): unpatched, the fixture (a polyline with points `(0,0,0)→(0,1,0)→(0,1,0)→(1,2,0)`, the middle pair duplicated) fails export with exit 2 / "Error: non-finite vertex position generated"; patched, it succeeds (312 vertices, 164 triangles, exit 0), and the new test's own GLB accessor scan confirms every POSITION/NORMAL float (data and declared min/max) is finite. Full root `ctest`: 136/136 (was 135). Both `frameAxes()` call sites (`MeshBuilder.cpp:878` and `:1113`) are covered by the single shared-function fix — no second call site needed a separate change.

### AUD-068 `[DONE]` `P2` `W1` · Mc3JsonParser::parseString() entirely ignores its own Mc3LoadPolicy — confineResourcePathsToRoot is a no-op on the .mc3.json load path
- **Component:** mc3/src/Mc3JsonParser.cpp (parseString)
- **Evidence:** Found via the same fresh adversarial re-audit as `AUD-064`..`067`. `Mc3JsonParser.cpp:358` (pre-fix): `const Mc3LoadPolicy& /*policy*/` — the parameter is received but never read anywhere in the function body. `Mc3XmlParser.cpp`'s equivalent path enforces `confineResourcePathsToRoot` (`AUD-006b`) at 6 resource-path fields (mesh source, texture uri, SVG src, embed src, sound src, music src); the JSON path had none of that — an untrusted `.mc3.json` naming an absolute or `..`-escaping path in any of those 6 fields passed straight through unvalidated. No production call site currently passes `untrusted()` to this parser, so this was latent, not yet exploited, but a silent bypass one future JSON-based untrusted caller away (e.g. a JSON-emitting AI response path).
- **Outcome:** Thread `policy.confineResourcePathsToRoot` through to the same 6 resource-path fields the XML parser already validates, matching its behavior exactly.
- **Tests:** New `mc3/test/json_load_policy_test.cpp` (`mc3_json_load_policy` ctest, 12 assertions) — mirrors `load_policy_test.cpp`'s resource-confinement coverage.
- **Resolved:** commit `16ebcaa` — verify: `ctest -R mc3_json_load_policy`
- **Status note:** Implementation mirrors `Mc3XmlParser.cpp`'s own `g_confineResourcePaths`/`g_resourceRoot`/`includePathWithinRoot` exactly (same names, same logic, same `thread_local` rationale — `toObject()` is called recursively for children with no existing policy parameter to thread through). **Deliberately scoped to `confineResourcePathsToRoot` only** — `allowIncludes`/`confineIncludesToRoot`/`maxIncludeDepth` are NOT wired in, because unlike XML, `.mc3.json`'s `includes` field is parsed as an inert flat string list and never resolved/merged into another file's content anywhere in this codebase (confirmed via grep: only this file's own read and `Mc3JsonWriter.cpp`'s matching write touch `doc.includes` — no merge logic exists for the JSON path at all, so there's no local-file-inclusion vector here to gate). **Empirically verified both directions** (`git stash`): unpatched, the 7 confinement-specific assertions correctly FAIL; patched, all 12 pass. Full root `ctest`: 137/137 (was 136); standalone CNA-free `mc3/build` tree: 21/21 (was 20).
- **New finding while testing (filed separately, not fixed here):** see `AUD-069` — a pre-existing latent bug shared with the XML parser's `includePathWithinRoot`, found while writing this fix's regression test.

### AUD-069 `[DONE]` `P3` `W1` · includePathWithinRoot() wrongly rejects a same-directory relative resource reference when the target doesn't exist on disk and the document was opened via a bare relative filename
- **Component:** mc3/src/Mc3XmlParser.cpp and mc3/src/Mc3JsonParser.cpp (both independently define `includePathWithinRoot`, identical logic)
- **Evidence:** Found while writing `AUD-068`'s regression test. `includePathWithinRoot()` resolves the candidate path via `std::filesystem::weakly_canonical()`. `weakly_canonical()` only canonicalizes the longest EXISTING prefix of a path; when a relative candidate refers to a file that does not exist anywhere on disk (a very ordinary case — e.g. an AI-generated document referencing a mesh/texture that hasn't been created yet, or simply checking confinement before checking existence), `weakly_canonical()` returns the candidate unchanged (still relative, not resolved against any base). When `rootDir` is also empty (a document opened via a bare relative filename, e.g. `mc3togltf scene.mc3.xml out.glb` run from the scene's own directory — normalized to `"."` and then canonicalized to an ABSOLUTE cwd), `std::filesystem::relative(relative_candidate, absolute_root)` produces an empty/degenerate result, which the function's own `rel.empty()` check then treats as "escapes the root" and rejects. Reproduced directly (see `mc3_json_load_policy_test.cpp`'s own comment on this): a same-directory `meshSource="model.obj"` under an untrusted policy with an empty `sourceDir` is wrongly rejected UNLESS the referenced file is first created on disk at that exact location.
- **Outcome:** This is a false REJECTION (fails safe — an over-strict false positive), not a bypass (no escaping path is ever wrongly accepted), so severity is low. Fixed by making `candidate` absolute via `std::filesystem::absolute()` FIRST (never requires existence — unconditionally prepends `current_path()` to a relative path), then `weakly_canonical`-ing that combined absolute path as a single unit, instead of canonicalizing the bare candidate on its own.
- **Tests:** Extended both `load_policy_test.cpp` (XML) and `json_load_policy_test.cpp` (JSON, already had a same-directory case, but only with the file pre-created — added a non-existent-file variant) to cover the non-existent-file + empty-rootDir combination.
- **Resolved:** commit `c393ca1` — verify: `ctest -R "load_policy"`
- **Status note:** The fix direction sketched above (join `candidate` onto an explicitly-canonicalized root) turned out to be subtly wrong once actually attempted: both call sites already pre-join their own base onto `candidate` before calling this function (`g_resourceRoot / p`, `selfPath.parent_path() / fileAttr`), so re-joining the root a second time inside the function would double-prefix whenever `rootDir` is non-empty. `std::filesystem::absolute()` avoids this — it only touches `candidate`, never re-introduces `rootDir`. Root cause also empirically re-derived rather than assumed: direct debug instrumentation showed `weakly_canonical("model.obj")` returning the literal string `"model.obj"` (still relative, not resolved against `current_path()` at all) for a non-existent single-component relative path, confirming the mechanism described above. Applied identically to both parsers' independently duplicated copies. **Empirically verified both directions** (`git stash`): unpatched, both new non-existent-file assertions correctly FAIL with the "escapes the document root" error; patched, both pass, alongside every pre-existing load-policy assertion (including the `..`-escape rejection cases, confirming the fix didn't loosen real confinement). Full root `ctest`: 142/142 (unchanged — no new `ctest`-registered target, new cases inside the existing `mc3_load_policy`/`mc3_json_load_policy` binaries).

### AUD-070 `[DONE]` `P2` `W6` · Mc3XmlWriter emits CDATA sections with no scan for an embedded "]]>", letting attacker-controlled content break out of the CDATA section on save
- **Component:** mc3/src/Mc3XmlWriter.cpp (SVG inlineContent, script source, embed base64Content)
- **Evidence:** Found via the same fresh adversarial re-audit as `AUD-064`..`069`. Three call sites (SVG `inlineContent`, script `source`, embed `base64Content`) called `t->SetCData(true)` unconditionally with no check that the text doesn't itself contain the CDATA terminator `]]>`. A single CDATA section cannot contain its own closing delimiter — content containing `]]>` followed by attacker-chosen text (a plausible AI-generated `<script>` or a pasted SVG whose own markup contains a CDATA section) closes the CDATA early, and whatever follows in the string is then parsed as literal XML markup on the next load.
- **Outcome:** Detect an embedded `]]>` before choosing CDATA; fall back to normal escaped text (safe, since `]]>` has no special meaning outside a CDATA section) rather than emitting a structurally-broken section.
- **Tests:** New `mc3/test/cdata_injection_test.cpp` (`mc3_cdata_injection` ctest, 7 assertions).
- **Resolved:** commit `a349163` — verify: `ctest -R mc3_cdata_injection`
- **Status note:** Empirically confirmed MORE severe than "garbled content" while writing the fix: the unpatched writer's output for a crafted script/SVG payload is XML so structurally broken it doesn't even parse back on reload (`XML_ERROR_MISMATCHED_ELEMENT`), crashing the (pre-fix) test process on an uncaught exception — verified via `git stash` of the fix. Fixed with `appendTextOrCData()`: CDATA when safe, entity-escaped plain text when the content contains `]]>`. Deliberately NOT the other standard technique (splitting into multiple adjacent CDATA sections) — `Mc3XmlParser.cpp` reads these fields via `GetText()`, which returns only the FIRST child text node's value and would silently truncate anything after the first split point; the single-node fallback used instead has no such risk. Applied at all 3 `SetCData` sites, including embed `base64Content` (safe by construction — base64's alphabet can never contain `]` or `>` — applied anyway for consistency/defense in depth rather than trusting that invariant forever). Full root `ctest`: 138/138 (was 137); standalone CNA-free `mc3/build` tree: 22/22 (was 21).

### AUD-071 `[DONE]` `P2` `W6` · McbReader silently drops the overrides/steps field on a tag mismatch instead of rejecting, unlike every other known key
- **Component:** mcb/src/McbReader.cpp (readSceneState, readTrigger)
- **Evidence:** Found via the same fresh adversarial re-audit as `AUD-064`..`070`. `readSceneState()`'s `overrides` key and `readTrigger()`'s `steps` key were each read with `if (k == "X" && tag == TAG_ARR) { ... } else { skipValue(in, tag); }` — the only two remaining call sites in the file using this pattern (confirmed via grep for `&& tag == TAG_`). Every other known key uses `expectTag()` (`AUD-015`'s own established policy), which throws a clear "type mismatch" error when a known key's tag doesn't match its expected decoder. These two instead fell straight into the unknown-key fallback on a mismatch, silently dropping the field — a corrupted/malicious file with the right key name but the wrong tag byte produced a valid-looking document quietly missing its scene-state overrides or trigger steps, no error or diagnostic at all.
- **Outcome:** Switch both sites to the same `expectTag()`-then-read pattern every other known key already uses.
- **Tests:** New `mcb/test/tag_mismatch_rejection_test.cpp` (`mcb_tag_mismatch_rejection` ctest, 12 assertions).
- **Resolved:** commit `6cc673f` — verify: `ctest -R mcb_tag_mismatch_rejection`
- **Status note:** Fixed by adding `expectTag(tag, TAG_ARR, "overrides")`/`expectTag(tag, TAG_ARR, "steps")` right before each field's existing read logic, matching the pattern used everywhere else in the file. **Empirically verified both directions** (`git stash`): the test patches the tag byte immediately following each key's exact, unambiguously-located byte pattern from `TAG_ARR` to `TAG_STR` — unpatched, both mismatch-detection assertions correctly FAIL (the field is silently dropped, no exception); patched, loading throws `"MCB: type mismatch for key 'overrides' (expected tag 8, got 4)"` / the `steps` equivalent, and all 12 assertions pass. Also confirms a legitimate document with both fields present still round-trips correctly. Full root `ctest`: 139/139 (was 138).

### AUD-072 `[DONE]` `P0` `W1` · drawExtrudeDynamic builds its full vertex buffer before its own size bailout, freezing the editor every frame
- **Component:** src/MeshCraft/Renderer/SceneRenderer_Extrude.cpp (drawExtrudeDynamic)
- **Evidence:** Found via the same fresh adversarial re-audit as `AUD-064`..`071` — the same class of bug as `AUD-064`'s `drawGridDynamic`, in the neighboring function in the same file. Path segments (`M`, from `ex.segments`) and cross-section points (`N`, from `ex.crossSection.segments`/`.sides`) are each individually capped at parse time (`kMaxTessellation=4096`), but nothing bounded their PRODUCT before allocating. `ex.segments="4096"` with a circular cross-section `segments="4096"` trig-computed and allocated the full `hollow ? 2*M*N : M*N+2` vertex buffer (~16.8M vertices) every single frame — the existing `numVerts>65535` bailout (previously at the very end of the function) only discarded the result AFTER all that work was already done, not before.
- **Outcome:** Compute the vertex-count formula up front (right after `M`/`N`/`hollow` are known) and bail to a placeholder mesh before any allocation or trig work, matching `AUD-064`'s fix to `drawGridDynamic` in the same file.
- **Tests:** New `test/extrude_stress.mc3.xml` fixture (`segments="4096"` path, `segments="4096"` circular cross-section) + `smoke_test_extrude_stress` ctest (TIMEOUT 30, `test/smoke_test.sh`).
- **Resolved:** commit `ff7cad4` — verify: `ctest -R smoke_test_extrude_stress`
- **Status note:** Empirically confirmed as a real freeze before writing the fix, matching `AUD-064`'s own verification approach: `MeshCraft extrude_stress.mc3.xml --screenshot out.png` did not complete within a 25s timeout on the unpatched binary (`git stash` of the fix); with the fix it completes in ~2.3s, the same as normal startup. The old end-of-function `numVerts>65535` check is now provably unreachable given the new early bailout (both use the exact same formula the ring-building loops actually produce) — left in place as a cheap defensive backstop against the `uint16_t` index buffer wrapping, with a comment explaining it should never fire rather than silently removed. Full root `ctest`: 140/140 (was 139).

### AUD-073 `[DONE]` `P2` `W1` · Extrude hollow cross-section divides by zero when radius=0, producing NaN vertex positions
- **Component:** src/MeshCraft/Renderer/SceneRenderer_Extrude.cpp (drawExtrudeDynamic, drawObjectEdges)
- **Evidence:** Found via the same fresh adversarial re-audit as `AUD-064`..`072`. Both `drawExtrudeDynamic()` and `drawObjectEdges()` independently compute `bool hollow = (cs.innerRadius > 0.0f) && (type == Circle || Polygon);` then `float innerScale = hollow ? (cs.innerRadius / cs.radius) : 0.0f;` with no check that `cs.radius` is nonzero. `radius==0` is a legal document value (the parsers only reject negatives); a hollow cross-section with `radius=0, innerRadius>0` (already a nonsensical shape — the "inner" radius would be larger than the "outer" one) made `innerScale` evaluate to `+inf`, propagating NaN vertex positions into both the live solid viewport and the edge-overlay wireframe. `mc3togltf`'s own export-side `buildExtrude()` was already safe — confirmed its hollow condition requires `innerRadius < radius`, which `radius=0` can never satisfy — so this was an editor-only gap, not shared with the export path.
- **Outcome:** Require `radius > 1e-6f` (matching `norm3()`'s/`AUD-067`'s existing epsilon convention in the same file) before treating a cross-section as hollow, at both call sites.
- **Tests:** New `test/extrude_hollow_zero_radius_test.cpp` (`extrude_hollow_zero_radius` ctest, 12 assertions).
- **Resolved:** commit `7889fd0` — verify: `ctest -R extrude_hollow_zero_radius`
- **Status note:** `SceneRenderer` is CNA-coupled and can't be unit-tested directly, so the new test mirrors the exact two-line formula (both call sites are byte-for-byte identical) rather than calling the real function — same idiom as `differential_geometry_test.cpp`. The test concretely reproduces the bug first (the pre-fix formula genuinely computes `+inf` for `0.3f/0.0f`, not just asserted as a hypothesis) before proving the fix (finite `0.0f` for both Circle and Polygon, and for a near-zero — not just exact-zero — radius; a legitimate hollow shape and a solid `radius=0` cross-section are both confirmed unaffected). **A live-render screenshot comparison was also tried and deliberately NOT kept as a committed test**, documented honestly rather than silently omitted: a `radius=0` cross-section produces byte-identical output whether or not this fix is applied, because `makeProfile()`'s Circle case already collapses every profile point to `(0,0)` when `radius==0` — the whole extruded tube's OUTER ring is already degenerate/invisible for a reason unrelated to this fix, so pixel comparison cannot discriminate it; only the formula-level test can. Full root `ctest`: 141/141 (was 140).

### AUD-074 `[DONE]` `P0` `W1` · McbReader has no document-wide aggregate budget, unlike the XML/JSON parsers' DocumentBudget
- **Component:** mcb/src/McbReader.cpp (readObject, readTexture, readMaterial, readEmbed, readAction/readChannel/readKeyframe, definitions map, loadFromBinaryImpl)
- **Evidence:** Found via a fresh 4-agent independent audit run 2026-07-20 (same day as this session's SYS-W14-18..27 feature work), specifically the core-code-correctness-bug-hunt dimension. `Mc3XmlParser.cpp`/`Mc3JsonParser.cpp` both implement a `DocumentBudget` struct (`kMaxTotalObjects=100'000`, `kMaxTotalMaterials=20'000`, `kMaxTotalTessellationWeight=500'000`, etc., charged at every read site) specifically because a per-field/per-collection cap (`kMcbMaxCollectionCount=10'000'000`, `McbReader.cpp`) bounds any *single* array/map but not the AGGREGATE across the whole document. `McbReader.cpp` never got this protection — confirmed via grep, zero occurrences of "Budget"/"charge" before this fix. Concretely exploitable with REAL, present data (not just a false/truncated claim, which `mcb_reserve_bomb_test.cpp` already covered a different variant of): the `"objects"` array accepts up to `kMcbMaxCollectionCount` entries, and each entry can be encoded as just 2 bytes (`TAG_OBJ` + a zero-length key immediately ending `readObject()`) — a file well under 1MB could claim and actually deliver enough entries to force construction of far more heap `shared_ptr<Mc3Object>` instances than any legitimate scene needs, reachable directly from the editor's Open-file dialog.
- **Outcome:** Port `Mc3XmlParser.cpp`'s `DocumentBudget` to `McbReader.cpp` — same constants, same charge-site placement (inside `readObject()` for the recursive object-tree dimension since children re-enter the same function; at each top-level map/array loop in `readDocument()` for the flat collections: textures/materials/embeds/definitions/actions; inside `readChannel()`/`readAction()` for the nested channel/keyframe dimensions; a new `clampTessBudgeted()` wrapping the existing `clampTess()` at all 6 tessellation-field sites) — reusing the exact same reasoning/comments as the XML parser's copy rather than inventing a different set of limits for the same format. `g_budget.reset()` added at the top of `loadFromBinaryImpl()` (MCB has no `<include>`-equivalent recursive re-entry into this function, so unlike the XML parser's reset-once-at-top-level-only care, there's no nested-call subtlety to preserve here).
- **Tests:** New `mcb/test/document_budget_test.cpp` (`mcb_document_budget` ctest, 11 assertions) — deliberately constructs REAL (not truncated) minimal-but-numerous data crossing 3 different budget dimensions (objects: 100,001 real 2-byte entries over the 100,000 cap; tessellation: 200 spheres at segments=3000 each, individually under the 4096 per-field cap but summing to 600,000 over the 500,000 aggregate cap; materials: 20,001 real empty materials over the 20,000 cap), confirms the exact boundary (100,000 objects, not 100,001) still loads successfully with no false-positive, and confirms an ordinary small document is completely unaffected. All 10 pre-existing `mcb_*` tests re-run and confirmed still passing (no false-positive against any existing legitimate fixture).
- **Resolved:** commit `75c9cbc` — verify: `ctest -R mcb_document_budget`
- **Status note:** Caught a real bug in the test's OWN construction while writing it (not the reader): `Mc3XmlParser.cpp`'s `rKey()`-style object-field keys use a 1-byte length prefix, but `TAG_MAP` entry keys use `rRawStr()`'s 4-byte length prefix — a different wire encoding for the same-looking "string key" concept. The materials-budget test case initially used the 1-byte encoding for a `TAG_MAP` key, which misparsed as garbage and threw a `"string length exceeds sanity limit"` error instead of the expected `"material budget"` error; fixed by adding a distinct `appendRawStr()` test helper and using it specifically for `TAG_MAP` keys, `appendKey()` (1-byte) only for `TAG_OBJ` field keys. Full rebuild + 162/162 `ctest` (was 161).

### AUD-075 `[DONE]` `P1` `W1` · Several primitive tessellators divide by an unclamped `segments` inside a `<= segments`-bounded loop, producing NaN vertices at `segments` 0 or 1
- **Component:** mc3togltf/src/MeshBuilder.cpp (buildSphere, buildCylinder, buildCone, buildTorus, buildCapsule, buildDisk); include/MeshCraft/Renderer/PrimitiveTessellationAlg.hpp (tessellateUnitSphereAlg, tessellateUnitTorusAlg)
- **Evidence:** Found via the same fresh 4-agent independent audit as `AUD-074`, the core-code-correctness-bug-hunt dimension. `mc3.xsd`/all three parsers only reject a negative `segments` value, so `segments=0` (or `1`, for Sphere specifically — `rings=segments/2` truncates to 0 there too) is a legal document value. Several tessellators use a `for (i = 0; i <= segments; ++i)` rim/pole loop that still executes once at `i==0` even when the divisor is 0, computing e.g. `2*pi*0/0` — an IEEE-754 NaN, not caught by any existing check. `buildSphere`'s `rings`/`sectors`, `buildTorus`'s `rings` (its sibling `sides` was already guarded, `std::max(4, segments/2)`), and `buildCapsule`'s `sectors` (its sibling `rings` was already guarded, `std::max(2, segments/4)`) were all unclamped entirely; `buildCylinder`/`buildCone`'s side loops were already safe (`< segments`, so segments=0 just emits nothing there), but their cap-rim loops used the vulnerable `<= segments` pattern; `buildDisk`'s both branches (solid and ring) used it too. **Independently re-verified during investigation that the live editor viewport has an identical, independently-implemented bug** in `PrimitiveTessellationAlg.hpp` (a second, CNA-free tessellator used by `SceneRenderer`, differential-tested against `MeshBuilder.cpp` by `differential_geometry_test.cpp`) — `tessellateUnitSphereAlg`'s `rings`/`sectors` and `tessellateUnitTorusAlg`'s `ringSeg`/`tubeSeg` have the exact same unclamped-divisor-in-a-`<=`-loop shape; `tessellateUnitCylinderAlg`/`ConeAlg`/`CapsuleAlg` were already safe (strict `<` bounds throughout, confirmed by direct reading, not just by analogy) and `drawDiskDynamic()` (`SceneRenderer_Extrude.cpp`, the live viewport's own dynamic Disk renderer) was ALREADY correctly guarded (`std::max(3, segments)`) — only the export-side `buildDisk` lacked the equivalent guard. Scope of this fix therefore ended up covering both independent geometry generators, not just the one the audit finding named.
- **Outcome:** Clamp `segments` (and any value derived from it that's used as a loop-bound divisor) to a safe non-zero minimum at the top of each affected function, matching the `std::max(N, ...)` convention already used by this file's own already-guarded sibling dimensions (`buildCapsule`'s `rings`, `buildTorus`'s `sides`) and by `drawDiskDynamic()`'s own `std::max(3, segments)` for the same primitive.
- **Tests:** New `test/primitive_zero_segments_test.cpp` (`primitive_zero_segments` ctest, CNA-free, links `mc3togltf_lib` — same idiom as `differential_geometry_test.cpp`) — constructs every one of the 6 shapes (Sphere/Cylinder/Cone/Torus/Capsule/Disk, the last in both solid and ring form) plus the 5 live-viewport tessellators at `segments` 0 and 1, asserting every position/normal/texcoord component is finite and every index is in-bounds. **Empirically verified against the unpatched source via `git stash`**: 23 of the test's assertions genuinely FAIL pre-fix (real NaN reproduced, not just a hypothesized risk) across all 6 shapes and both `tessellateUnitSphereAlg`/`tessellateUnitTorusAlg`; all pass again once the fix is restored. Also manually smoke-tested end-to-end past the unit-test level: a 7-object `segments=0`/`1` scene loads and renders in the real `MeshCraft` binary via `--screenshot` with a clean `[GLCheck]` (no GL errors, no crash), and the same scene exports via the real `mc3togltf` binary to a glTF whose 28 accessors all have finite `min`/`max` bounds (verified by loading the actual JSON output, not just re-running the unit test). Full rebuild + 163/163 `ctest` (was 162).
- **Resolved:** commit `c99bfc5` — verify: `ctest -R primitive_zero_segments`

### AUD-076 `[DONE]` `P1` `W1` · New instances of the AUD-036 "Checkbox variant" no-op-undo bug across 9 real call sites
- **Component:** src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp (Light castShadows, Texture mipMaps, Material doubleSided, Sound loop, Music loop, AssetMetadata instancingEligible), src/MeshCraft/Scene/PropertiesPanel.cpp (Extrude smooth, Extrude caps, Material doubleSided inline editor)
- **Evidence:** Found via the same fresh 4-agent independent audit as `AUD-074`/`AUD-075`, the core-code-correctness-bug-hunt dimension. `ImGui::Checkbox(label, bool* v)` writes `*v` in place and returns `true` on the SAME call — so `if (ImGui::Checkbox("X", &live.field)) { pushUndo(); ... }` (the "unconditional pushUndo() on Checkbox" convention this codebase otherwise correctly uses for Combo/InputText, per `PropertiesPanel.cpp`'s own STAB-0719 comment) runs `pushUndo()` AFTER `live.field` has already been mutated, so the `deepCopyDoc()` snapshot captures the NEW value, not the pre-click one — Ctrl+Z is a silent no-op. This is the exact same bug class `AUD-036`/`AUD-036b`/`SYS-W14-16` already fixed for `Slider`/`Drag`-nested-`IsItemActivated()` and a first Checkbox instance (`Mc3Action::loop`/`autoplay`, `MeshCraftApplication_Anim.cpp`), but 9 more Checkbox call sites added since then had drifted back into the buggy shape (one, `instancingEligible`, via a different gating call, `IsItemDeactivatedAfterEdit()`, which fires on the same click frame for a Checkbox and has the identical timing problem). **A full independent sweep of every `ImGui::Checkbox` call site in `src/MeshCraft/` (47 total) was run beyond the audit's own reported list**, confirming: the 9 found here are the only genuinely-buggy ones; the remaining ~30 in these two files already correctly bind to a derived local `bool` (e.g. `bool hasP = st.position.has_value();`) with the live write-back happening after `pushUndo()` (mostly from `SYS-W14-16`'s earlier systematic pass); and every Checkbox in `MeshCraftApplication_UiOverlays.cpp` is bound to app-level UI-preference state (e.g. `findCaseSensitive_`, `glbAllowApproxCSG_`), not `document_` content, so it correctly has no undo coverage at all.
- **Outcome:** Local-copy-then-writeback at all 9 sites — bind `Checkbox` to a fresh local `bool` initialized from the live field, and only assign the live field (after `pushUndo()`) inside the `if` block once the widget reports a change — the exact fix `MeshCraftApplication_Anim.cpp`'s `act.loop`/`autoplay` already established.
- **Tests:** Extended `test/undo_gesture_frame_test.cpp` (real ImGui frame-driving, no CNA/GL context needed) with a new pair of blocks using a mock "live field" + "snapshot" pair — one exercising the BUGGY direct-bind shape (proves the snapshot captures the wrong, post-click value, reproducing the bug's actual mechanism, not just asserting it exists) and one exercising the FIXED local-copy shape (proves the snapshot correctly captures the pre-click value). The pre-existing `driveCheckboxGesture()` helper in this file only asserted `pushUndo()` was *called* once per click, never *what value* it would have captured — which is why it never caught this class of bug despite testing Checkbox already; the new blocks close that specific gap. Also corrected the stale/incomplete `PropertiesPanel.cpp:1686` comment ("Checkbox/Combo/InputText get an unconditional pushUndo()") to caveat why Checkbox specifically needs the local-copy shape while Combo/InputText don't (their normal usage shape already requires an intermediate variable). Full rebuild + 163/163 `ctest` (unchanged count — extended an existing test, not a new one). Manually smoke-tested via `--screenshot` after the fix (clean `[GLCheck]`, no crash).
- **Resolved:** commit `43e6838` — verify: `ctest -R undo_gesture_frame`

### AUD-077 `[DONE]` `P1` `W1` · Authored lights have zero effect on live-viewport shading — only a gizmo icon
- **Component:** src/MeshCraft/Renderer/SceneRenderer.cpp / SceneRenderer.hpp (constructor, `draw()`, new `applyDocumentLighting()`)
- **Evidence:** Found via the same fresh 4-agent independent audit as `AUD-074`..`AUD-076`, the editor-UX/wiring-gaps dimension. `SceneRenderer`'s constructor calls `effect_->EnableDefaultLighting()` (CNA `BasicEffect`'s fixed built-in 3-directional-light rig) exactly once at init, and `doc.lights` was never read anywhere in the shading pipeline — grepped the whole renderer: `doc.lights` was used in exactly one place, `drawLightGizmos()` (icon overlay), plus separately a first-`castShadows`-directional-light-drives-a-debug-shadow-map path. A user could add a red point light, crank brightness to 50, and the object it's supposedly lighting looked exactly the same — only the small gizmo icon changed — while `mc3togltf`'s export (`SYS-W14-26`/`27`, this same session) correctly exports and unit-converts every light type. Undisclosed in the UI anywhere.
- **Outcome:** Investigated CNA's `BasicEffect` API before implementing (`cna/include/Microsoft/Xna/Framework/Graphics/BasicEffect.hpp`/`DirectionalLight.hpp`): a faithful port of real XNA's fixed-function lighting model, exposing exactly 3 `DirectionalLight0/1/2` slots (`Direction`/`DiffuseColor`/`SpecularColor`/`Enabled`) plus one `AmbientLightColor` — no position/attenuation API at all, so `point`/`spot` genuinely cannot be represented without a custom shader (a materially larger change than this finding's scope). **User confirmed the resulting scoped proposal** (map up to 3 `directional` lights + the first `ambient` light onto BasicEffect's real API; `point`/`spot` stay gizmo-only, documented) before implementation began. New `SceneRenderer::applyDocumentLighting(doc)`, called once per `draw()` (cheap — a handful of float writes, no allocation, so Lights-tab edits are reflected immediately): maps up to the first 3 `Directional` lights onto `DirectionalLight0-2` (`direction` normalized with a zero-length guard matching `drawLightGizmos()`'s own convention, `color × brightness` clamped to `[0,1]`, applied as both diffuse and specular) and the first `Ambient` light onto `AmbientLightColor`; falls back untouched to the constructor's original default rig when the document has no `Directional`/`Ambient` lights to represent (including documents with only `Point`/`Spot` lights, or none at all) — preserving today's look for the common unlit-by-design case.
- **Tests:** New `test/light_shading_test.py` (`light_shading_test` ctest, real headless `--screenshot` pixel sampling) + two new fixtures: `light_shading.mc3.xml` (a white sphere lit by one strong pure-red directional light aimed at the camera-facing hemisphere) and `light_shading_control.mc3.xml` (the identical scene with NO lights, for the default rig). Asserts the lit scene's sphere has a substantial red-dominant pixel cluster (>200 samples) AND the control scene has essentially none (<10) — proving the redness is actually caused by the authored light, not a default-rig coincidence. **Empirically verified via `git stash`**: the test genuinely fails pre-fix (0 red-dominant samples, real assertion failure reproduced, not just a hypothesized risk) and passes post-fix (1020 samples). Also manually verified visually — rendered both scenes and inspected the actual PNG output before writing the automated test: the lit scene's sphere is unmistakably red-shaded from the light's direction, the control sphere is warm-white as before. Full rebuild + 164/164 `ctest` (was 163). **Caught and fixed the same XML-comment double-hyphen bug as `SYS-W14-24`** while writing the fixtures (invalid XML, caught immediately by `xsd_validation`) — also had to fix the fixtures' element ordering (`<cameras>` must precede `<materials>`/`<objects>` per `mc3.xsd`), both before committing.
- **Resolved:** commit `8a80d55` — verify: `ctest -R light_shading_test`

### AUD-078 `[DONE]` `P2` `W1` · BasicEffect's own GPU fog was enabled redundantly and mode-blind alongside the already-complete CPU-side per-object fog blend
- **Component:** src/MeshCraft/Renderer/SceneRenderer.cpp (`draw()`)
- **Evidence:** Found via the same fresh 4-agent independent audit as `AUD-074`..`AUD-077`, the editor-UX/wiring-gaps dimension (initially reported as a P1/P2 "fog is visibly double-applied" finding). `drawObject()`'s per-object CPU-side blend ("I3", further down the same file) is a complete, correct implementation honoring both `mode="linear"` (`start`/`end`) and `mode="exponential"` (`density`). `draw()` additionally called `effect_->setFogEnabledProperty(true)` + `setFogStartProperty`/`setFogEndProperty` (CNA `BasicEffect`'s own built-in GPU fog) unconditionally whenever `doc.environment->fog` existed, regardless of `mode` — wrong for Exponential mode (`BasicEffect` has no exponential-fog concept at all, so it always used `start`/`end` even then) and redundant even for Linear mode (the CPU blend already produces the correct color; a second GPU-side blend on top is at best a no-op, at worst a real double-application). **Downgraded from the audit's own initial P1/P2 "visibly over-fogged" framing to P2 "dead/incorrect code, not a currently-visible defect" after direct empirical investigation while writing the fix**: rendered the exact same fixture with and without the GPU-fog-enabling code (via `git stash`, saved binaries from both) and diffed the output pixel-for-pixel — **zero difference**, in either the fogged or un-fogged case. The GPU-fog branch was not visibly reachable in this renderer's actual effect/shader configuration (likely `VertexColorEnabled=true` routes to a shader permutation where CNA's fog uniform is never consumed — a CNA-side question, out of scope here per the "no CNA changes" boundary, and not necessary to resolve since the fix doesn't depend on knowing why). This is a real code-correctness fix (removes logic that is unconditionally wrong for one fog mode and redundant for the other, and could become visibly double-applying under a different CNA build/backend/effect configuration) — just not a fix for a defect users could currently see.
- **Outcome:** Remove the `BasicEffect`-fog-enabling block from `draw()` entirely; rely solely on the already-correct, already-complete CPU-side per-object blend.
- **Tests:** New `test/fog_exponential_a.mc3.xml`/`fog_exponential_nofog.mc3.xml` fixtures + `test/fog_exponential_test.py` (`fog_exponential_test` ctest) — (1) a static source check that `setFogEnabledProperty(true)` does not appear in `SceneRenderer.cpp` (the actual regression guard: empirically confirmed via `git stash` that this alone correctly fails when the removed pattern is reintroduced, without even needing a rebuild), and (2) a real `--screenshot` render check that `mode="exponential"` fog (previously entirely untested — the pre-existing `fog_linear_test.py` only covers Linear mode) produces a real, measurable, PARTIAL blend toward the fog color via the CPU path alone (qualitative bounds, not an exact numeric prediction — an earlier draft tried predicting the precise blended color via the documented CPU formula and hand-computed camera distance, but hit persistent, unexplained numeric discrepancies against the actual renderer; abandoned in favor of the simpler, still-meaningful directional/partial-blend check, documented honestly in the test file itself rather than silently papered over). Full rebuild + 165/165 `ctest` (was 164). **Caught the same XML-comment double-hyphen bug (3rd time this session)** while writing the fixtures — fixed before committing.
- **Resolved:** commit `f7361fa` — verify: `ctest -R fog_exponential_test`

### AUD-079 `[DONE]` `P1` `W1` · Mc3Camera::rotation ("alternative to target") ignored by the camera gizmo and Look-Through-Camera mode
- **Component:** src/MeshCraft/Renderer/SceneRenderer.cpp/.hpp (new `cameraForwardFromRotation()`, `drawCameraGizmos()`), src/MeshCraft/MeshCraftApplication.cpp (Look-Through-Camera mode)
- **Evidence:** Found via the same fresh 4-agent independent audit as `AUD-074`..`AUD-078`, the editor-UX/wiring-gaps dimension. `Mc3Camera::rotation` (`std::optional<std::array<float,3>>`, "alternative to target") has full editor UI (`MeshCraftApplication_UiLeftPanel.cpp`'s "Override Rotation" checkbox + `DragFloat3`, correctly wired with undo). But `drawCameraGizmos()` (`SceneRenderer.cpp`) always built the frustum gizmo's direction from `cam.target - cam.position`, and Look-Through-Camera mode (`MeshCraftApplication.cpp`) always called `Matrix::CreateLookAt(camPos, cam.target, up)` — neither checked `cam.rotation.has_value()`. So a camera authored with only `rotation` (`target` left at its `{0,0,0}` default) silently showed a gizmo/live-preview pointing at the origin instead of the authored direction. `mc3togltf`'s `GltfExporter.cpp` (`STAB-0694`-era code) already correctly checked `cam.rotation.has_value()` and quaternion-converted it — so export was right and only the two live-viewport consumers were wrong, undisclosed anywhere in the UI.
- **Outcome:** New `SceneRenderer::cameraForwardFromRotation(rotationDegrees)` (static, so both consumers can call it) converts a rotation triple into a forward direction using `Matrix::CreateFromYawPitchRoll(rotation[1], rotation[0], rotation[2])` (this codebase's own established convention for every other rotation field, e.g. `objectWorldMatrix()`) applied to a base forward of `(0,0,-1)` (this project's right_handed_y_up "looks down -Z at identity rotation" convention). `drawCameraGizmos()` and Look-Through-Camera mode both now check `cam.rotation.has_value()` first and use this helper, falling back to the existing `target`-based direction only when `rotation` is absent — matching the exporter's own priority order.
- **Tests:** New `test/camera_rotation.mc3.xml` (a camera with `rotation="0 0 0"` and no `target` authored, so `target` defaults to `{0,0,0}` — a point far outside this camera's frame; a box is placed exactly at `camera_position + (0,0,-distance)`, dead-center in frame if and only if `rotation` is honored) + `test/camera_rotation_test.py` (`camera_rotation_test` ctest, real `--screenshot` pixel sampling, same code path Look-Through-Camera mode uses). **Empirically verified via `git stash`**: rendered the identical fixture with the pre-fix and post-fix binaries — pre-fix, the box is completely off-screen (camera looking at the unused `target` default); post-fix, 1120 bright center-region samples confirm the box is dead-center. Also documented the previously entirely-undocumented `rotation` attribute in `MC3_FORMAT.md`'s Cameras section (a pre-existing gap, not something this fix introduced). Full rebuild + 166/166 `ctest` (was 165). **Caught the same XML-comment double-hyphen bug (4th time this session)** while writing the fixture — fixed before committing.
- **Resolved:** commit `c66d618` — verify: `ctest -R camera_rotation_test`

### AUD-080 `[DONE]` `P3` `W1` · MCB_FORMAT.md still claimed compression was "not yet implemented"
- **Component:** MCB_FORMAT.md (documentation only, no code change)
- **Evidence:** Found via the same fresh 4-agent independent audit as `AUD-074`..`AUD-079`, the docs-staleness dimension. `SYS-W14-25` (implemented earlier the same day) added real zlib-based MCB compression (`saveToBinary(doc, out, /*compress=*/true)`, `MCB_FLAG_COMPRESSED`), but `MCB_FORMAT.md`'s header-flags table still said the compression bit was "defined, not yet implemented" and its validation-error section still described the now-superseded `"MCB: compressed format not yet supported"` rejection message — actively wrong documentation for a feature that existed and had its own passing tests.
- **Outcome:** Rewrote the Flags row to point at a new "Compression (`SYS-W14-25`, 2026-07-20)" subsection; replaced the stale error-message list with the current real ones (`"MCB: invalid magic"`, `"MCB: unsupported version N"`, `"MCB: root is not an object"`, `"MCB: compressed format requires zlib, but this build was compiled without it"`); documented both the uncompressed layout (offset 8 = root tag) and compressed layout (offset 8 = 4-byte uncompressed size, 12 = 4-byte compressed size, 16+ = zlib-deflated payload), the 512MB zip-bomb sanity ceiling on both claimed sizes, actual-decompressed-size verification against the claim, and the optional `find_package(ZLIB)` graceful-degradation behavior — matching the already-correct description in `MC3_FORMAT.md`.
- **Tests:** n/a (doc-only correction). Verified by direct comparison against `mcb/src/McbWriter.cpp`/`McbReader.cpp`'s actual current error strings and header-layout code, not carried over from the stale prior text. Full `ctest` re-run (166/166) confirms the doc change touched no code path.
- **Resolved:** commit `6cc5702` — verify: manual diff of `MCB_FORMAT.md` against `mcb/src/McbReader.cpp`/`McbWriter.cpp`

### AUD-081 `[DONE]` `P3` `W1` · TESTING.md test counts stale within hours of its own "re-verified" revision
- **Component:** TESTING.md (documentation only, no code change)
- **Evidence:** Found via the same fresh 4-agent independent audit as `AUD-074`..`AUD-080`, the docs-staleness dimension. `TESTING.md`'s intro claimed "151 tests today" with a 9-label breakdown (`export` 68, `format` 33, `render` 24, `unit` 18, etc.) and its "Running the tests" section claimed "151/151 Passed" — both stale, since the 10 SYS-W14-18..27 feature-commit tests and the 5 AUD-074..079 fix-commit tests (`mcb_document_budget_test`, `primitive_zero_segments_test`, `light_shading_test`, `fog_exponential_test`, `camera_rotation_test`) landed without a doc follow-up. The audit also flagged a secondary, more granular staleness: the C++ assertion-count table claimed `mcb_roundtrip_test` prints 234 `PASS:` lines, when `SYS-W14-25`'s new compression-roundtrip cases actually brought it to 236.
- **Outcome:** Re-derived every count from a live build rather than trusting the audit report or the prior doc revision: `ctest -L <label> -N` ("Total Tests:" line) per label gives `ai` 1, `commands` 1, `export` 69, `format` 37, `lint` 3, `perf` 2, `registry` 1, `render` 27, `unit` 25 = 166 total (matching `ctest -N`'s own count). Re-ran all 5 C++ assertion binaries directly and counted `^PASS:` lines: `mc3_registry` 156, `mc3_ai` 122, `mc3_roundtrip` 577, `mc3_commands` 558 (all already correct, no change needed) and `mcb_roundtrip_test` 236 (was documented as 234, now fixed). Updated the intro paragraph, the "Expected result" line, and the `mcb_roundtrip` table row; added an honest note that the file's own prior same-day revision had already gone stale within hours, and why.
- **Tests:** n/a (doc-only correction). Verified by actually running `ctest -L <label> -N` and each C++ binary directly and counting real output lines, not by assumption. `python3 test/validate_plan_consistency.py . b-release --run-tests` passes, including its own live `ctest -N` cross-check (166) and a full live `ctest` run (166/166 passed, 0 failed).
- **Resolved:** commit `6cc5702` — verify: manual diff of `TESTING.md` counts against a live `ctest -L <label> -N`/binary run
