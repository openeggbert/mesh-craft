# Mesh Craft — Active Backlog

**This is the single active backlog.** For a compact baseline and handoff see
[`NEXT.md`](NEXT.md); for quality gates and process see
[`STABILIZATION.md`](STABILIZATION.md); for the registered tests see
[`TESTING.md`](TESTING.md). Superseded plans live in
[`docs/history/`](docs/history/).

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

   **Net across all 6 AUD-### rows remaining in this active backlog (61
   additional rows completed and archived to `docs/history/plan_20260718.md`
   on 2026-07-18 — see that file for their full evidence/resolution text):
   0 DONE, 4 TODO, 2 DEFERRED** — recompute with
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
3. Remaining `TODO` AUD-### rows by severity (AUD-053, downstream of
   AUD-052; AUD-057's CI-job half, same blocker), then SYS-### rows.

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
- **SYS-W9-01** `[TODO, superseded in scope by AUD-036b]` `P0` — Full undo/redo
  correctness: transaction abstraction, pre-mutation capture verification,
  `undo_coverage_audit.py` triage, atomicity, redo invalidation, selection
  restore. See `AUD-036b` for the authoritative task text.

### W11 — Build / CI / DX
- **SYS-W11-01** `[TODO, owner-gated]` `P1` — Un-park CI (`.github_` → `.github`)
  needs a `workflow`-scoped push token (`AUD-052`). Workflow file itself made
  correct/ready in commit `d16c82c`.
- **SYS-W11-03** `[TODO]` `P2` — Editor build+test CI job. (`AUD-053`)

### W12 — Performance baselines
- **SYS-W12-02** `[TODO]` `P3` — Extend `SYS-W12-01`'s benchmark harness to
  the remaining categories that need in-process instrumentation rather
  than CLI-level timing: mesh-gen, CSG + cache, traversal, picking, undo
  snapshot, texture processing (in isolation), animation eval, registry,
  startup, first frame. Likely needs either a small headless benchmark
  executable linking `MeshCraftApplication`'s internals directly, or new
  timing instrumentation exposed through `--stats`-style CLI output.

### W14 — New features (after P0/P1 gates)
- **SYS-W14-03** `[TODO]` `P2` — PNG screenshot / image export.
- **SYS-W14-04** `[DEFERRED]` `P3` — SVG texture rasterization pipeline.
- **SYS-W14-05** `[DEFERRED]` `P3` — Safe `embed:` mesh/resource support end-to-end. (`AUD-025`)
- **SYS-W14-06** `[DEFERRED]` `P3` — Improved CSG output (smooth normals/UVs/materials).
- **SYS-W14-07** `[DEFERRED]` `P3` — Improved walk/navigation collision.
- **SYS-W14-08** `[TODO]` `P2` — AI change preview/diff before destructive replace.
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
- **SYS-W14-16** `[TODO]` `P2` — Undo/redo structural-guarantee audit.
  Currently a manual discipline: 332 `pushUndo()` call sites (fresh count),
  each independently relying on the author remembering to call it before a
  mutation, verified only by grep/spot-check rather than any structural
  enforcement (e.g. a Command-pattern wrapper, a mutation-tracking proxy, or
  a debug-build assertion that `document_` didn't change since the last
  `pushUndo()`). `SYS-W5-04` did related exhaustive-enumeration work but was
  explicitly scoped to a lookup cache, not to this. This needs its own
  scoped research pass first (enumerate every `document_`-mutating call
  site not currently covered, the same discipline `SYS-W5-04`'s own
  research used) before choosing an enforcement mechanism — a real,
  possibly P1-worthy risk area (a missed `pushUndo()` is silent data loss on
  undo), but not a quick pick. Found via `missing.md`'s 2026-07-18 update.
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

---

## Audit-derived tasks (AUD-###)

67 tasks total (see the Session log's "Net across all 67 AUD-### rows" note
for the exact breakdown of where they came from): 57 from the session-1
audit (re-verified against current source in session 2), plus session-2's
adversarial re-open/new-defect findings. Ordered by discovery (AUD-001..057
in original severity order, followed by the session-2 additions). Status is
re-verified per row, not copied from a prior summary — do not trust a DONE
marker without checking its cited commit/verify command.

**Update (2026-07-18):** 61 of these 67 rows are now `DONE` and archived to
[`docs/history/plan_20260718.md`](docs/history/plan_20260718.md) (full
evidence/resolution text preserved there) — only the 6 still-open rows below
remain in this active file.

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
- **Evidence:** CMakeLists.txt:36-37 `if(ANDROID) set(MESH_CRAFT_GRAPHICS_BACKEND_UPPER "SDL_RENDERER")`. Combined with the fact (Finding 1) that the editor's ImGui UI and post-FX only work through ImGui_ImplOpenGL3 + SDL_GL_GetProcAddress (which need an SDL GL context, not an SDL_Renderer), an Android build would compile against a backend the editor cannot render on. README.md:211 correctly marks Android as `never attempted` so it is not falsely claimed working, but the CMake default choice bakes in a non-functional editor for the one platform that is forced onto SDL_RENDERER.
- **Outcome:** If Android support is intended, wire the editor to a backend it can actually render on (e.g. GLES via EASYGL) or gate the GUI editor off on Android with a clear message, rather than auto-selecting SDL_RENDERER which the UI layer cannot drive.
- **Tests:** An Android NDK configure that either selects a GL-capable backend or errors clearly; not currently testable here (no NDK installed).
- **Verify note:** Refinement (does not change the verdict): the editor↔SDL_RENDERER incompatibility is not Android-specific — the identical breakage occurs for ANY build configured with -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER on desktop, since the editor's ImGui path (MeshCraftApplication.cpp:212-213) is hardwired to OpenGL3 with no SDL_Renderer branch. What is Android-specific is that the force at CMakeLists.txt:36-37 makes SDL_RENDERER non-optional there (the user cannot pick EASYGL). So the finding is slightly understated in scope but accurate as stated. Severity P2 stands.
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
