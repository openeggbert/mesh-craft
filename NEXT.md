# NEXT.md

_Last updated: 2026-07-13. Branch `develop` @ commit `d2264ad`, working tree
clean except two untracked, unrelated scratch scene files
(`test/crownspire-citadel.mc3.xml`, `test/house3.glb` — manually authored
demo content, not part of any tracked task, left as-is). 0 commits ahead/behind
`origin/develop`. See `git log --oneline -20` for anything newer than this._

---

## 1. Project summary

**MeshCraft** is a desktop 3D scene editor (C++23, built on the CNA game
framework — an XNA/FNA-style API over SDL3 + OpenGL ES) for **MC3**, this
project's own scene/model format. A scene is a document (`Mc3Document`) of
primitives, CSG operations, materials, lights, cameras, animation, and
several extension namespaces (scripts, sounds/music, triggers, scene
states). The same in-memory AST has two serialization surfaces: `.mc3.xml`
(original) and `.mc3.json` (added recently, genuinely semantic JSON, not a
mechanical XML mirror). A separate binary format, `.mcb`, and a glTF/GLB
exporter (`mc3togltf`) round out the format family.

**Main goal (current phase):** this project has been in a **stabilization
phase** for several sessions — an evidence-based audit (`AUD-###` findings)
plus a systematic hardening workstream (`SYS-###`), not new user-facing
features. That backlog is now almost fully closed: every `AUD-###` finding
that isn't externally blocked is done, and of the systematic workstream only
one item remains, itself broken into phases (see §4).

**Important architectural decisions:**
- `Mc3Document` (in `mc3/`) is the single canonical in-memory AST. It has no
  CNA/GUI dependency and is shared by both parsers/writers, `mcb`,
  `mc3togltf`, `mc3tomcb`, and the editor.
- The editor (`MeshCraftApplication`) and the exporter (`mc3togltf`) are
  **two independent geometry generators** reading the same `Mc3Document` —
  there is no shared mesh-building code between "what you see in the editor"
  and "what gets exported." This is a deliberate but risky duplication (see
  §6).
- `../cna` and `../sharp-runtime` are **sibling repositories, not part of
  this repo, and not to be modified from here** — a separate process/owner
  handles them. This repo only consumes them via `add_subdirectory`.
- A second, unrelated sibling repository, **`../mesh-world`** (a
  procedurally-generated 3D world explorer built on top of MC3), drives its
  own feature work directly into this repo's `mc3/` library from time to
  time (see §3 and §5) — commits can land here that this repo's own
  `plan.md` never asked for.

## 2. Current status

- **Build:** last verified this session, Release config, EasyGL graphics
  backend — clean, zero warnings, exit 0.
  ```bash
  cmake -S . -B b-release && cmake --build b-release -j"$(nproc)"
  ```
- **Tests:** **122 / 123 `ctest` passing.** The one failure is
  `field_matrix` — see §4, not a regression from anything tracked in this
  repo's own backlog.
- **CLI/tools/apps/libraries currently available:**
  - `MeshCraft` — the interactive editor (`./b-release/MeshCraft
    scene.mc3.xml`, or `--screenshot out.png` / `--export out.glb` for
    headless one-shot runs).
  - `mc3togltf` — MC3 → glTF/GLB exporter (`--stats` prints export +
    pre-export-validation diagnostics).
  - `mc3tomcb` — MC3 XML → MCB binary converter.
  - Standalone libraries `mc3` (format/AST + XML/JSON parse-writer),
    `mcb` (binary format) — both buildable and testable without CNA.
- **Recently implemented** (this session; see §3 for the exact commit
  list): `Mc3Document::validate()` and its wiring into AI-apply/pre-export/
  pre-render/save (`Mc3Validation` diagnostics, previously console-only);
  a "Validation" ImGui panel + status-bar indicator surfacing those
  diagnostics; a checked-in `.clang-format`/`.clang-tidy` config (not
  applied to the existing tree); the first extraction out of the
  `MeshCraftApplication` "god object" (`Editor::KeybindingManager`).
- **Known working examples:** `./b-release/MeshCraft test/house.mc3.xml`;
  `./b-release/MeshCraft <scene> --screenshot out.png` (verified this
  session to genuinely capture the composited ImGui+3D framebuffer, not
  just the viewport); `./b-release/mc3togltf/mc3togltf test/features.mc3.xml
  /tmp/out.glb`.
- **What does not work yet / is not verified:**
  - Web (Emscripten) build: blocked by a crash inside `../cna`, not this
    repo (see §4/§5).
  - Windows (MinGW): does not compile — 1 remaining failure, in `../cna`.
  - CI: present and believed correct (`.github_/workflows/ci.yml`) but
    parked under a non-standard directory name and never actually runs
    (owner-gated — needs a workflow-scoped push token).
  - `field_matrix` ctest gate: currently red (see §4).

## 3. Recent changes

This session (11 commits, oldest first, all on `develop`, all pushed):

- `d9e98d5` — fixed 2 pre-existing XSD-invalid test fixtures (unrelated
  double-hyphen-in-XML-comment and duplicate-`id` bugs found while
  re-verifying the baseline).
- `a119415` — **added** `Mc3Document::validate(Mc3Validation&)`: re-validates
  a document's current in-memory state by round-tripping it through the
  XML writer/parser, for documents that never went through a parsing load
  at all (built programmatically or mutated after loading). New
  `mc3_document_validate_test`.
- `92c6246` — wired the above into the AI-response path
  (`AiResponseAlgorithms.hpp`); extended `ai_test.cpp`.
- `297af3e` — wired it into `mc3togltf`'s `GltfExporter` (new `validation`
  member, printed under `--stats`); new
  `mc3togltf_pre_export_validation_test`.
- `899b486` — wired it into 4 real load call sites (startup, Open File,
  Open Recent File, autosave recovery) and into `saveFile()`.
- `f0b33b1` — docs: closed out `SYS-W1-01`.
- `9c7bfa5` — **added** a "Validation" ImGui panel + status-bar indicator
  (`MeshCraftApplication_UiValidation.cpp`) surfacing the diagnostics above
  in the UI, not just the console.
- `d388529` — docs: closed out `SYS-W14-02`.
- `c9b48a6` — **added** root `.clang-format` / `.clang-tidy` (config only —
  no file in the tree was reformatted; `clang-tidy` actually run and found
  only pre-existing, already-triaged issues).
- `d364712` — docs: closed out `SYS-W11-06`.
- `95aaa90` — **refactor:** extracted `Editor::KeybindingManager`
  (`include/MeshCraft/Editor/KeybindingManager.hpp` +
  `src/MeshCraft/Editor/KeybindingManager.cpp`) out of
  `MeshCraftApplication`; deleted `MeshCraftApplication_Keybindings.cpp`;
  updated ~90 call sites; new `keybinding_manager_test` (no coverage existed
  for this subsystem before).
- `d2264ad` — docs: recorded `SYS-W3-01`'s full scope/roadmap and marked
  Phase 1 done.

**Also present on `develop` but NOT done by this session** — landed from
the sibling `mesh-world` repo's own, separate backlog while this session
was in progress: `e7bed06` (R109, semantic `mc3.json`), `ff63ef5` (R110,
`.mc3lib` library format), `7110ebd` (R111, `Mc3Object::assetMetadata`),
`83819f8` (R101, `<imports>`), `df9d5ea` (R102, composite-object split),
`f392d41` (R103, script IDs). These are real and tested, but **R111 is the
direct cause of the one currently-failing test** (§4).

## 4. Current blocker / main problem

**There is no build-breaking or work-stopping blocker.** The closest things
to one:

**(a) `field_matrix` ctest gate is red.**
- Symptom: `ctest -R field_matrix --output-on-failure` (from `b-release`)
  reports 18 fields "missing from `['xsd_attr', 'mcb_read', 'mcb_write']`"
  (some also missing from `model`): `bounds_min`, `bounds_max`, `category`,
  `subcategory`, `nominal_size`, `collision_proxy`, `clearance_volume`,
  `facing`, `hash`, `license`, `provenance`, `instancing_eligible`,
  `shadow_policy`, `max_visibility_distance`, `selection_weight`,
  `namespace`, `script`, `tier`.
- Affected files: `mc3/include/MeshCraft/Mc3/Mc3AssetMetadata.hpp` (where
  these fields live in the model), `mc3/mc3.xsd`, `mcb/src/McbReader.cpp` /
  `McbWriter.cpp` (where they're absent).
- Suspected cause: commit `7110ebd` (R111, from the sibling `mesh-world`
  repo's own backlog, not this repo's `plan.md`) added these
  `Mc3Object::assetMetadata` fields to the model/XML layers but not yet to
  `xsd_attr`/`mcb_read`/`mcb_write`.
- What's been tried: nothing, deliberately. This session's own `plan.md`/
  `SYS-###` work never touched `Mc3AssetMetadata.hpp`; fixing someone else's
  in-flight cross-repo work risks colliding with their next commit. Before
  touching this, **check `git log` for newer R-series commits** — the gap
  may already be closed.
- Verification once addressed: `(cd b-release && ctest -R field_matrix
  --output-on-failure)` should print no `FAIL:` lines.

**(b) `SYS-W3-01` (decompose the `MeshCraftApplication` "god object") is a
genuinely multi-session task, not a bug.** Research this session found
**280 data members + 113 methods** in that one class (11,544 lines of
implementation across 17 `.cpp` files); only 10 subsystems are cleanly
extracted so far (the 9 pre-existing ones plus this session's
`KeybindingManager`). This isn't blocking anything else in the repo — it's
just large and not close to finished. Full roadmap in `plan.md`'s
`SYS-W3-01` entry.

## 5. Known bugs and limitations

- **Confirmed, external, not actionable from this repo:** Web/Emscripten
  build crashes inside `../cna` on the first `SDL_EVENT_WINDOW_RESIZED`
  (`GameWindow::queryClientBoundsFromSDL()` calls `SDL_GetWindowSize()`
  after the video subsystem reports uninitialized) — blocks web
  live-verification entirely. Windows/MinGW: 1 remaining compile failure,
  also in `../cna`.
- **Confirmed, currently red:** `field_matrix` ctest gate — see §4(a).
- **Confirmed, by design, deferred:** SVG textures parse/edit but are never
  rasterized; `embed:` mesh references parse/edit but aren't resolved on
  export; scripts/triggers are data-model + editing only, no runtime
  execution; `rotation_units="radians"` / non-default `euler_order` are
  honored on export but not in live editor interaction (won't-fix, tracked
  as `STAB-0701`); no native file-browse dialog (drag-and-drop works).
- **Confirmed, open, low-severity:** `AiAssistant`'s detached background
  HTTP thread is never joined at shutdown (`AUD-014`) — doesn't currently
  cause a hang (deterministic shutdown is otherwise handled) but is a loose
  end.
- **Incomplete:** `Editor::EditorViewport` (bundles a camera + gizmo +
  `pickRay()`) is dead code — added as an explicit "stub" in commit
  `580105d`, never included by `MeshCraftApplication.hpp`, never
  instantiated. No decision has been made to finish wiring it in or delete
  it.
- **Unresolved product decision, not a bug:** duplicate object IDs are
  proven safe at the `mc3` library level (no crash/data loss), but whether
  they should be a hard parse error is a product call nobody has made
  (`SYS-W1-04`).
- **Needs verification:** whether the sibling `mesh-world` repo's R-series
  work (R104+) will touch files this repo also cares about — check
  `git log` at the start of any future session, don't assume `plan.md`
  alone reflects everything that has changed.
- **Risky assumption to watch:** the editor (`SceneRenderer`) and the
  exporter (`mc3togltf/MeshBuilder.cpp`) independently generate geometry
  for every primitive/CSG type from the same `Mc3Document` fields. Nothing
  enforces they agree except a differential test that doesn't cover every
  primitive/case (see §6). A change to one without checking the other can
  silently make "what you see" not match "what you export."

## 6. Architecture notes

- **`Mc3Document`** (`mc3/include/MeshCraft/Mc3/Mc3Document.hpp`) — the
  canonical AST. CNA-free. Public API is depended on by `mc3togltf`,
  `mc3tomcb`, the editor, and every test fixture — **additive changes
  only**; check all four before changing an existing signature.
- **Two independent geometry generators, same source data:**
  `mc3togltf/src/MeshBuilder.cpp::buildPrimitive()` (export + CSG) and
  `SceneRenderer`'s primitive dispatch (`SceneRenderer_Builders.cpp`,
  editor viewport). No shared code. **Triangle winding differs
  deliberately between them** — export is CCW-from-outside (glTF/OpenGL
  convention), the editor preview is CW-from-outside (CNA's default
  `RasterizerState`). Do not "fix" one to match the other; getting it
  backwards renders a see-through mirrored interior, not an obvious crash.
- **CSG dual-path invariant:** `mc3togltf/src/CsgEvaluator.cpp` (export) and
  `SceneRenderer`'s CSG preview cache (editor) both key off `isCutter` /
  `role="cutter"`. Keep both in sync if CSG semantics change.
- **`MeshCraftApplication`** (`include/MeshCraft/MeshCraftApplication.hpp`)
  — the main editor class, historically a "god object": 280 data members +
  113 methods, implementation spread across 17 `.cpp` files by *area* (not
  by *ownership*). Being incrementally decomposed (`SYS-W3-01`, in
  progress, see §4b). Ten subsystems are already extracted into owned
  helper objects with narrow interfaces: `SelectionManager`,
  `EditorCamera`, `TransformGizmo`, `SceneRenderer`, `GridRenderer`,
  `SceneHierarchyPanel`, `PropertiesPanel`, `AiAssistant`, `ModelRegistry`,
  `KeybindingManager`. The established idiom for extracting a new one:
  self-contained value member, zero/near-zero-arg constructor, whatever
  document/app state it needs passed per-call rather than stored (see any
  of the above for a template) — a `PropertiesPanel`-style
  context-struct-of-callbacks idiom exists for logic that must call back
  into many private `MeshCraftApplication` members.
- **`Alg` mirror pattern:** pure-logic, CNA-free free functions
  (`include/MeshCraft/EditorAlgorithms.hpp`,
  `src/MeshCraft/AiResponseAlgorithms.hpp`) mirror some production code
  paths so they're headlessly testable. Most are the real production
  implementation (the `.cpp` calls into them); a few are deliberately-kept
  duplicates. At least two have already drifted from production despite
  the "kept in sync" intent (`insertAnimKeyframesAlg`/`loadPrefsAlg`,
  `AUD-030` fixed the first, `AUD-031`/`032`/`033` cover the audit of the
  rest). **Check whether an `Alg` function is actually called from
  production before assuming a fix there takes effect.**
- **Undo/redo:** whole-document snapshot-based (deep copy on every
  mutating command), not command/diff-based. `undoStack_`/`redoStack_` are
  raw `std::vector<Mc3::Mc3Document>` members, not yet extracted.
- **`mc3.xsd` is compiled into the binary at configure time** — editing it
  requires a reconfigure, not just a rebuild.
- **XML comment gotcha:** a literal `--` inside an XML comment is rejected
  by `lxml`/`test/validate_xsd.py`, though `tinyxml2` tolerates it
  silently. Run `test/validate_xsd.py` on any new/edited `.mc3.xml`
  fixture before trusting it.
- **`AiAssistant` threading invariant:** background HTTP runs on a
  detached `std::thread` writing into a `shared_ptr<AiRequestResult>`
  (atomic done flag + mutex). Never switch this to `std::async`/
  `std::future` (reintroduces a destructor-blocking hang-on-close that was
  already fixed once).
- **`CNA_ENABLE_NET` must stay `OFF`** — an unused CNA subsystem that fails
  to compile; re-enabling breaks the default build.
- **Boundaries that must not be broken:** no changes to `../cna` or
  `../sharp-runtime` without owner permission (a separate process handles
  them). No `${meta-gl_SOURCE_DIR}/include` in `CMakeLists.txt` (triggers a
  full CNA recompile). Commits from the sibling `mesh-world` repo's
  R-series backlog can land on this repo's `develop` independent of this
  repo's own `plan.md` — don't assume `git log` only contains commits this
  repo's own backlog asked for.

## 7. Useful commands

```bash
# Configure + build (Release, EasyGL backend — the tree this session verified)
cmake -S . -B b-release
cmake --build b-release -j"$(nproc)"

# Full test suite
(cd b-release && ctest -j"$(nproc)")
(cd b-release && ctest -N)                              # list registered tests + live count
(cd b-release && ctest -R field_matrix --output-on-failure)   # reproduce the current failure

# Plan/doc self-consistency (run before trusting any count in plan.md/NEXT.md)
python3 test/validate_plan_consistency.py . b-release

# XSD validation of a new/changed fixture
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# Lint/format (config checked in this session; clang-format needs
# `pip install clang-format` first if not already on PATH)
clang-format -i path/to/changed/file.cpp
cmake -S . -B b-release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p b-release path/to/changed/file.cpp

# Run / demo
./b-release/MeshCraft test/house.mc3.xml
./b-release/MeshCraft test/house.mc3.xml --screenshot /tmp/out.png   # writes a .ppm despite the name; `convert` reads it
./b-release/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb --stats
./b-release/mc3tomcb/mc3tomcb test/house.mc3.xml /tmp/out.mcb
```

## 8. Next smallest tasks

1. **Re-check `field_matrix` before touching it.**
   Goal: confirm whether the sibling `mesh-world` repo's ongoing R-series
   work has already closed the gap described in §4(a), to avoid duplicate
   or conflicting work.
   Files: none changed — just `git log --oneline -20` and re-run the test.
   Verify: `(cd b-release && ctest -R field_matrix --output-on-failure)`.

2. **If still open, close the `field_matrix` gap for the 18 listed fields.**
   Goal: add `xsd_attr` (mc3.xsd) + `mcb_read`/`mcb_write`
   (`McbReader.cpp`/`McbWriter.cpp`) coverage for `Mc3Object::assetMetadata`'s
   fields, or explicitly allowlist any that are intentionally
   XML/JSON-only, matching `test/field_matrix.py`'s existing allowlist
   pattern.
   Files: `mc3/mc3.xsd`, `mcb/src/McbReader.cpp`, `mcb/src/McbWriter.cpp`,
   `mc3/include/MeshCraft/Mc3/Mc3AssetMetadata.hpp`, `test/field_matrix.py`.
   Verify: `(cd b-release && ctest -R field_matrix)` passes; full `ctest`
   still green.

3. **Decide `EditorViewport`'s fate.**
   Goal: either finish wiring it in (retarget the ~76 `camera_.` + ~12
   `gizmo_.` call sites across 7 files to go through it) or delete it as
   abandoned scaffolding — either way, record the decision in `plan.md`
   (`SYS-W3-01`'s roadmap already names this as Phase 3).
   Files: `include/MeshCraft/Editor/EditorViewport.hpp`,
   `src/MeshCraft/Editor/EditorViewport.cpp`, and (if finishing it)
   `MeshCraftApplication.hpp` + the 7 files referencing `camera_`/`gizmo_`.
   Verify: full rebuild + `ctest`; if finished, a `--screenshot` check that
   the viewport still renders/orbits correctly.

4. **`SYS-W3-01` Phase 2: Preferences ownership decision.**
   Goal: decide whether a new `Preferences`/`AppSettings` class owns only
   `prefTheme_`/`prefsOpen_`/`applyTheme()` (narrow) or also the
   cross-domain fields `loadPrefs()`/`savePrefs()` currently persist
   (`autoSaveInterval_`, `snapTranslate_`/`snapRotate_`/`snapScale_`,
   `gridSpacing_`) — then extract accordingly.
   Files: `include/MeshCraft/MeshCraftApplication.hpp`,
   `src/MeshCraft/MeshCraftApplication_FileOps.cpp` (`loadPrefs`/
   `savePrefs`/`applyTheme`), `include/MeshCraft/EditorAlgorithms.hpp`
   (`PrefsAlg`, `loadPrefsAlg`/`savePrefsAlg`).
   Verify: full rebuild + `ctest`; a manual load/save round-trip test
   (following `keybinding_manager_test.cpp`'s pattern) for the new class.

5. **Investigate `AUD-014`: join the `AiAssistant` background thread at
   shutdown.**
   Goal: confirm whether the detached thread noted in §6 can be safely
   joined (with a bounded timeout, reusing the existing
   `waitForAllInFlight()`) during `MeshCraftApplication`'s destructor,
   closing this long-open loose end.
   Files: `src/MeshCraft/AiAssistant.cpp`, `include/MeshCraft/AiAssistant.hpp`,
   `src/MeshCraft/MeshCraftApplication.cpp` (destructor).
   Verify: `ctest -R ai` (the `ai_test` binary) plus a manual check that
   the app still exits promptly with a request in flight.

## 9. Do not do yet

- **No broad `MeshCraftApplication` refactor in one pass.** `SYS-W3-01` is
  explicitly phased (research this session sized it at 280 members/113
  methods); do one subsystem at a time, verify, commit.
- **No mass `clang-format -i` across the existing 18.5k LOC.** The config
  added this session (`.clang-format`) was deliberately not applied
  tree-wide — that's a separate, much larger, not-yet-decided change.
- **No "fixing" `field_matrix` without first checking `git log`** for newer
  commits from the sibling `mesh-world` repo — it may already be resolved,
  and racing that other work risks a real merge conflict or duplicated
  effort.
- **No changes to `../cna` or `../sharp-runtime`** without explicit owner
  permission.
- **No `Mc3Document` public API changes** without checking `mc3togltf`,
  `mc3tomcb`, the editor, and all test fixtures first — additive only.
- **No attempt to unpark CI** (`.github_/workflows/ci.yml` → `.github/`) —
  owner-gated, needs a workflow-scoped push token nobody in this session
  has.
- **No speculative work on `EditorViewport`** (e.g. partially rewiring it)
  without first making the explicit finish-or-delete decision in task 3
  above — it's already been left half-done once.
- **No new user-facing features** until the current backlog (`SYS-W3-01`
  and its open phases) is closed — this project is still in a
  stabilization phase by its own stated policy (`STABILIZATION.md`).

## 10. Resume prompt

```
Read NEXT.md first, in full. Then work on exactly ONE task from its
"Next smallest tasks" section — start with task 1 unless told otherwise.
Inspect only the files that task names; do not refactor or "clean up"
anything else you notice along the way. Make one small, verified
improvement: implement it, then run the exact verification command the
task lists (and the full `ctest` suite) before considering it done. Do
not start a second task in the same session unless the first is fully
committed and verified. When finished, update NEXT.md: move the completed
task out of "Next smallest tasks", update "Current status"/"Recent
changes" with what actually changed (not what was planned), and re-check
every other section for anything your change made stale.
```
