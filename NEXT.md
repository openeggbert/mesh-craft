# NEXT.md

_Last updated: 2026-07-17, end of an autonomous session. Branch `develop`
@ commit `f4e00b3`, working tree clean except the same two untracked,
unrelated scratch scene files noted previously (`test/crownspire-citadel.mc3.xml`,
`test/house3.glb` — manually authored demo content, not part of any tracked
task, left as-is). 9 commits ahead of the session's starting point
(`f1900e3`), all pushed to `develop`. See `git log --oneline -20` for
anything newer than this._

**Human decisions obtained at the start of this session** (see `plan.md`'s
`SYS-W1-04`/`SYS-W3-01`/`AUD-036c` entries for the full rationale) — **all
4 are now fully implemented**, not just decided:
1. `EditorViewport` — **deleted** (not finished wiring in). Done.
2. `SYS-W3-01` Phase 2 Preferences — **narrow** scope (theme/panel-open
   only; autosave/snap/grid fields stay put). Extracted as
   `Editor::Preferences`. Done.
3. Duplicate object ids (`SYS-W1-04`) — **warning-level `Mc3Validation`
   diagnostic**, parsing stays permissive. Implemented in
   `Mc3XmlParser.cpp`. Done.
4. Undo/redo selection (`AUD-036c` open item) — **restores** the pre-
   mutation selection (`SYS-W9-03`), not cleared. Implemented in
   `MeshCraftApplication_Commands.cpp`. Done.

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

- **Build:** re-verified at the end of this session, Release config,
  EasyGL graphics backend — clean, zero warnings, exit 0.
  ```bash
  cmake -S . -B b-release && cmake --build b-release -j"$(nproc)"
  ```
- **Tests:** **124 / 124 `ctest` passing** (was 122/123 at session start —
  `field_matrix` now passes, see `SYS-W6-04`; net +1 test, +~50 new
  assertions across extended existing tests, see §3).
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
  list): closed `field_matrix`'s 18-field gate gap for real
  (`Mc3Object::assetMetadata`/`scriptId`, `Mc3Document::library`/`imports`
  now in `mc3.xsd` + MCB, not just XML); duplicate-object-id `Mc3Validation`
  warning; deleted the dead `EditorViewport` stub; extracted narrow
  `Editor::Preferences` (`SYS-W3-01` Phase 2); undo/redo now restores
  selection instead of clearing it (`SYS-W9-03`); `AiAssistant` API-key
  redaction + bounded error-message size (`SYS-W2-04`); closed every
  `MC3_FORMAT.md` documentation gap (`SYS-W5-02`); found and fixed 5 stale
  `plan.md`/`NEXT.md` status markers referencing findings that were
  actually already done in earlier sessions.
- **Known working examples:** `./b-release/MeshCraft test/house.mc3.xml`;
  `./b-release/MeshCraft <scene> --screenshot out.png` (genuinely captures
  the composited ImGui+3D framebuffer, not just the viewport — used
  repeatedly this session as a real smoke test after `MeshCraftApplication`
  changes); `./b-release/mc3togltf/mc3togltf test/features.mc3.xml
  /tmp/out.glb`.
- **What does not work yet / is not verified:**
  - Web (Emscripten) build: blocked by a crash inside `../cna`, not this
    repo (see §4/§5).
  - Windows (MinGW): does not compile — 1 remaining failure, in `../cna`.
  - CI: present and believed correct (`.github_/workflows/ci.yml`) but
    parked under a non-standard directory name and never actually runs
    (owner-gated — needs a workflow-scoped push token).

## 3. Recent changes

**New autonomous session, 2026-07-17** (continues from the session below;
see the top-of-file decisions block for the 4 human-authorized decisions
this session started with):

- Recorded the 4 decisions in `plan.md` (`SYS-W1-04`, `SYS-W3-01`,
  `AUD-036c`, new `SYS-W9-03`) and here.
- **`SYS-W6-04` (new, DONE):** closed `field_matrix`'s 18-field gap for
  real — `Mc3Object::assetMetadata` (R111), `Mc3Object::scriptId` (R103),
  and `Mc3Document::library`/`imports` (R110/R101) were completely absent
  from both `mc3.xsd` (not a lint nitpick — any document actually using
  `<assetMetadata>`/`<library>`/`<imports>` failed XSD validation outright)
  and MCB (silent XML/JSON→MCB→XML data loss for the same features). Added
  the missing XSD types/elements and `McbReader.cpp`/`McbWriter.cpp`
  read/write support; the 4 residual field_matrix rows
  (`max_visibility_distance`/`namespace`/`script`/`tier`) are genuine
  naming-convention asymmetries, now allowlisted with reasons. New fixture
  `test/asset_metadata_library_import.mc3.xml` + 3 new
  `mcb_roundtrip_test` functions (~40 assertions). Full details in
  `plan.md`'s `SYS-W6-04` entry. **123/123 ctest** (was 122/123).
- **Documentation fix:** `AUD-015`'s header line still said "PARTIAL: ...
  27 sibling read* functions remain" despite the entry's own later
  "Resolved"/completion notes showing it was fully finished in a prior
  session (commit `00aee08`, 189 `expectTag` call sites). Corrected the
  header; also flipped `SYS-W6-01` from stale `[TODO]` to `[DONE]` since
  all 3 of its constituent `AUD-###` findings were already independently
  `[DONE]`. No code change from this fix, `readSceneState`'s deliberate
  skip-not-throw exception (see its own plan.md note) was left as-is after
  a brief revert (see git history on this file if curious — not worth its
  own bullet).
- **`SYS-W1-04` (now DONE):** implemented the decided duplicate-object-id
  handling — `checkDuplicateObjectIds()` in `Mc3XmlParser.cpp` (called at
  the end of `buildDocumentFromRoot()`) walks `doc.objects` + recursive
  `.children` (the same scope `flatFindById` searches) and emits one
  warning-level `Mc3Validation` diagnostic per duplicated id. Parsing stays
  fully permissive. 4 new assertions in `mc3_duplicate_ids_test.cpp`.
- **`EditorViewport` deleted:** removed the abandoned-scaffolding stub
  (`include/MeshCraft/Editor/EditorViewport.hpp` +
  `src/MeshCraft/Editor/EditorViewport.cpp`) outright — zero references
  anywhere else, confirmed before deleting. Sources are globbed, so a
  `cmake .` reconfigure (not a `CMakeLists.txt` edit) was needed to pick up
  the removal. `camera_`/`gizmo_` remain separate `MeshCraftApplication`
  members, unchanged.
- **`SYS-W3-01` Phase 2 (narrow) DONE:** extracted `Editor::Preferences`
  (theme + Preferences-dialog-open state + `applyTheme()` only —
  `autoSaveInterval_`/`snap*`/`gridSpacing_` deliberately stay on
  `MeshCraftApplication`, per the narrow-scope decision).
  `loadPrefs()`/`savePrefs()` stay on `MeshCraftApplication` too, since
  they persist the theme together with those cross-domain fields in one
  prefs file. New `preferences_test` (no coverage existed before). Full
  rebuild + **124/124 ctest** (new `preferences` test); manual
  `--screenshot` smoke test confirms the app still boots/renders.
- **`SYS-W9-03` DONE:** `performUndo()`/`performRedo()` now restore the
  pre-mutation selection instead of clearing it. New
  `undoSelectionStack_`/`redoSelectionStack_` kept in lockstep with
  `undoStack_`/`redoStack_` everywhere those are pushed/popped/cleared
  (including the "Undo History" jump-to-step dialog and the 3 file-load
  `.clear()` sites). Restore is by id (`flatFindSharedById()`, a new
  shared-ptr counterpart of `flatFindById()`), silently skipping an id no
  longer present post-swap rather than dangling/crashing. 5 new mirror-
  model assertions in `test/undo_gesture_frame_test.cpp` (the real
  functions are CNA-coupled and not headlessly callable, matching that
  file's existing convention for this class of test). Full rebuild +
  **124/124 ctest**; manual `--screenshot` smoke test confirms the app
  still boots/renders.
- **Doc fix:** `AUD-014` ("join the AiAssistant background thread at
  shutdown") was already fully fixed in an earlier session (commit
  `3cd27d7`, `plan.md` already read `[DONE]`) — only this file's §5/§8
  hadn't caught up. Corrected, no code change. Same class of staleness as
  the `AUD-015`/`SYS-W6-01` fix above.
- **Stale-`TODO` sweep:** after finding `AUD-014`/`AUD-015`/`SYS-W6-01`
  stale earlier this session, did a quick pass over the remaining
  `SYS-###` `TODO` rows and found 3 more: `SYS-W2-03` (its `AUD-009` has
  read `[DONE]` since commit `ca82b0b`), `SYS-W6-03` (its `AUD-018` has
  read `[DONE]` since commit `ba3e73c`), `SYS-W14-01` (duplicate of the
  already-`[DONE]` `SYS-W9-02`, autosave+crash-recovery). All 3 flipped to
  `[DONE]` with a status note, no code change. While re-verifying
  `SYS-W6-03`, found this session's own earlier `SYS-W6-04` work had
  freshly re-broken `MCB_FORMAT.md`'s field-order list (missing the new
  `library`/`imports` fields) — fixed that too.
- **`SYS-W5-02` DONE:** documented every `MC3_FORMAT.md` gap
  `xsd_docs_diff.py` reports (grew from the original 5+12 to 14+25 by the
  time this was picked up, partly from this session's own `SYS-W6-04`
  additions) — new "Library and Imports"/"UV Mapping"/"Asset Metadata"
  sections plus several attribute-table additions. `xsd_docs_diff.py` now
  reports 0 gaps (was 14 elements + 25 attributes).
- **`SYS-W2-04` (now DONE, one sub-point deliberately left open):**
  "extracted-XML size" turned out to already be bounded (`SYS-W1-03`'s
  512MB `checkDocumentByteBudget()` already runs inside the AI-apply
  path's `loadFromString()` call — verified, not re-implemented). Added
  `AiAssistant::redactSecret()`/`boundedForDisplay()` (both exposed as
  public static methods, matching the existing `jsonEscape` convention):
  every caught-exception error message in `sendAsync()` now has the API
  key redacted (defense-in-depth — no current path actually leaked it,
  but nothing structurally prevented a future one from doing so) and any
  embedded HTTP response body capped at 4KB with a truncation note. New
  pure-function tests plus a real mock-`httplib::Server` integration test
  proving a 20000-byte error body produces a ~4KB `errorMsg()`.
  At the time, capping the response size during the actual network *read*
  was deliberately left open as new `SYS-W2-05` — **closed later this same
  session, see below.**
- **`SYS-W2-05` DONE (closes the gap `SYS-W2-04` left open):** replaced
  `sendAsync()`'s `cli.Post(...)` convenience call with a raw
  `httplib::Request` sent via `cli.send(req)`, with a `content_receiver`
  that aborts the read once a response exceeds the new public
  `AiAssistant::maxResponseBytes` field (default 8MB, overridable like the
  existing timeout fields). Response body genuinely never grows past the
  cap in memory now, not just when later displayed. 2 new mock-server
  tests (cap-exceeded aborts cleanly; comfortably-under-cap still
  succeeds); every pre-existing `ai_test.cpp` mock-server test still
  passes unmodified, confirming the rewrite is a faithful `Post()`
  replacement. Full rebuild + 124/124 `ctest`; manual `--screenshot`
  smoke test.

- **`SYS-W1-05` DONE (partial, honestly scoped):** added a 256-deep
  recursion guard (matching `GltfExporter.cpp`'s existing `kMaxNodeDepth`
  precedent) to `deepCopyObjectAlg` and `deepCopyObj`/`deepCopyDoc` — the
  two most frequently-invoked recursive walks over `Mc3Object::children`
  (called directly by Duplicate/Group/undo on every action) — so a cyclic
  children graph (only possible via C++-API misuse; XML parsing can't
  produce one) now throws a clean error instead of stack-overflow-
  crashing. 2 new tests (2-cycle, self-cycle). Several other recursive
  walkers remain unguarded (`Mc3XmlWriter`, `SceneRenderer`, `mc3togltf`'s
  `MeshBuilder`) — tracked as new `SYS-W1-06`, lower priority since a
  cycle there would surface via a less-immediate operation. Full rebuild
  + 124/124 `ctest`; manual `--screenshot`/`--export` smoke tests.

**Prior session (11 commits, oldest first, all on `develop`, all pushed):**

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
`f392d41` (R103, script IDs). These are real and tested, but R111 (plus
R110/R101) left `mc3.xsd`/MCB gaps — closed this session, see §3's
`SYS-W6-04` entry above.

## 4. Current blocker / main problem

**There is no build-breaking or work-stopping blocker.** `SYS-W3-01`
(decompose the `MeshCraftApplication` "god object") is a genuinely
multi-session task, not a bug: research found **280 data members + 113
methods** in that one class (11,544 lines of implementation across 17
`.cpp` files); only 10 subsystems are cleanly extracted so far (the 9
pre-existing ones plus `KeybindingManager`). This isn't blocking anything
else in the repo — it's just large and not close to finished. Full roadmap
in `plan.md`'s `SYS-W3-01` entry; this session decided `EditorViewport`'s
long-open fate (delete) and Phase 2's Preferences scope (narrow) — see
the decisions block at the top of this file and §8 below.

## 5. Known bugs and limitations

- **Confirmed, external, not actionable from this repo:** Web/Emscripten
  build crashes inside `../cna` on the first `SDL_EVENT_WINDOW_RESIZED`
  (`GameWindow::queryClientBoundsFromSDL()` calls `SDL_GetWindowSize()`
  after the video subsystem reports uninitialized) — blocks web
  live-verification entirely. Windows/MinGW: 1 remaining compile failure,
  also in `../cna`.
- **Confirmed, by design, deferred:** SVG textures parse/edit but are never
  rasterized; `embed:` mesh references parse/edit but aren't resolved on
  export; scripts/triggers are data-model + editing only, no runtime
  execution; `rotation_units="radians"` / non-default `euler_order` are
  honored on export but not in live editor interaction (won't-fix, tracked
  as `STAB-0701`); no native file-browse dialog (drag-and-drop works).
- **Resolved:** `Editor::EditorViewport`'s long-open finish-or-delete
  decision (bundled a camera + gizmo + `pickRay()`, added as an explicit
  "stub" in commit `580105d`, never wired in) — **deleted 2026-07-17**
  (see top-of-file decisions block). `camera_`/`gizmo_` remain separate
  `MeshCraftApplication` members.
- **Resolved:** duplicate object IDs are proven safe at the `mc3` library
  level (no crash/data loss) and now surface a warning-level
  `Mc3Validation` diagnostic (decided + implemented 2026-07-17,
  `SYS-W1-04`, now `DONE`); parsing stays permissive.
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
(cd b-release && ctest -R field_matrix --output-on-failure)   # SYS-W6-04's gate, now green

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

_(Everything from the previous revision of this list is done — see §3 for
the full list of what landed this session. Before starting any task below,
re-run `git log --oneline -20` and re-check the cited `plan.md` row's
status yourself: this session found and fixed **6** stale `[TODO]`/status
markers that referenced findings already resolved in earlier sessions
(`AUD-014`, `AUD-015`/`SYS-W6-01`, `SYS-W2-03`, `SYS-W6-03`, `SYS-W14-01`,
`SYS-W6-02`) — don't assume any remaining `TODO` below is still accurate
without looking.)_

1. **`plan.md`'s remaining `TODO` `SYS-###` rows**, in no particular
   priority order (pick the highest-value one that fits available time):
   `SYS-W1-06` (new this session — extend `SYS-W1-05`'s cycle-guard
   pattern to the remaining unguarded recursive walks: `Mc3XmlWriter`,
   `SceneRenderer`, `mc3togltf`'s `MeshBuilder`), `SYS-W5-03` (MC3
   versioning + unknown element/attribute policy), `SYS-W5-04` (central
   document index/reference resolver), `SYS-W5-05` (property-based
   round-trip tests + parser fuzz target), `SYS-W7-01` (truthful glTF
   export matrix per model feature), `SYS-W11-07` (package-first
   discovery + offline, partial), `SYS-W12-01` (benchmark scenes +
   baselines).
   `SYS-W11-01`/`SYS-W11-03`/`AUD-042`/`AUD-052`/`AUD-053`/`AUD-057` stay
   correctly blocked/owner-gated (CI parked, no Android NDK in this
   environment) — don't attempt those without the missing external
   resource.

2. **`SYS-W3-01` Phase 3+** (`MeshCraftApplication` decomposition
   continues): per the roadmap in `plan.md`'s `SYS-W3-01` entry, remaining
   candidates are Macro recorder (deliberately deferred at Phase 1 — needs
   a `PropertiesPanel`-style callback-DI struct since `executeMacroStep()`
   calls 8+ other `MeshCraftApplication` methods), undo/redo stack
   ownership, animation, file dialogs, post-processing, audio/walk-mode.
   No decision has been made on which is next — that's itself worth a
   quick research pass (read the actual member/method list for each
   candidate, matching how Phase 1/2 were scoped) before picking, not an
   assumption.

3. **`AUD-036c`'s remaining open item (2 of 2):** "locked objects untouched
   by undo/redo" is well-tested at the per-command level but not
   independently tested as its own whole-document-snapshot guarantee (see
   `AUD-036c`'s status note, second open item — the first, selection
   restore, is what this session's `SYS-W9-03` closed). Low priority,
   by-design not a gap, but flagged as untested.

Do a **repo-wide staleness spot-check early in the next session** — this
one found real value in it (5 status corrections, one doc actively
re-broken by this session's own earlier commit) — before assuming any
`[TODO]` row represents real remaining work.

## 9. Do not do yet

- **No broad `MeshCraftApplication` refactor in one pass.** `SYS-W3-01` is
  explicitly phased (research this session sized it at 280 members/113
  methods); do one subsystem at a time, verify, commit.
- **No mass `clang-format -i` across the existing 18.5k LOC.** The config
  added this session (`.clang-format`) was deliberately not applied
  tree-wide — that's a separate, much larger, not-yet-decided change.
- **No changes to `../cna` or `../sharp-runtime`** without explicit owner
  permission.
- **No `Mc3Document` public API changes** without checking `mc3togltf`,
  `mc3tomcb`, the editor, and all test fixtures first — additive only.
- **No attempt to unpark CI** (`.github_/workflows/ci.yml` → `.github/`) —
  owner-gated, needs a workflow-scoped push token nobody in this session
  has.
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
