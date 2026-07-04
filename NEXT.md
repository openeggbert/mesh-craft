# NEXT.md

_Last updated: 2026-07-04_

---

## 1. Project summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` scene format
— a lightweight XML-based scene description used by the OpenEggbert
project. It uses Dear ImGui for its UI, running on **CNA** (an XNA-style
SDL3 + OpenGL runtime, a separate sibling repo at `../cna` — never
modified from this repo) and **SHARP_RUNTIME** (`../sharp-runtime`, the
math library CNA's backend depends on). Scenes export to glTF/GLB via
**mc3togltf** and to a compact binary format via **mc3tomcb**.

**Main goal:** reach a fully stabilized, test-covered codebase before
adding new features. All work is tracked in `plan.md` as 650 `STAB-XXXX`
tasks across sections S0–S20, gated by a Gate 0–6 checklist (each gate
requires its full `STAB-XXXX` ID range green — not just a smaller
"priority" subset `plan.md` also tracks for getting each gate's biggest
risks closed quickly).

**Current phase:** Stabilization. Every gate's priority-list subset is
done. **Gate 6 (Documentation)**: S17 20/20 ✅ (fully green), S19 15/15
✅ (fully green), S18 20/25 (all reachable done; 1 flagged —
STAB-0617 — 4 tool-blocked, need `include-what-you-use`/`clang-tidy`/
`cppcheck`), S20 12/15 (3 blocked on Blender/browser/CI). No gate is
fully green yet, but nothing more is runnable in Gate 6 without an
external tool or a human with a display. **S11 (Materials/Textures)**
is done: 25/30, remaining 5 all genuinely blocked (live display or
Blender). **S12 (Animation Stability)** is done: 28/30, remaining 2
flagged (STAB-0464 needs a live display; STAB-0460 describes a feature
that was never built, out of scope per the stabilization moratorium).
Both S11 and S12 sweeps found several real bugs, fixed with permanent
regression tests — see §3 for the full list. **S13 (Commands,
Undo/Redo, and Algorithms) is now fully done — all 25 items (STAB-0471
through STAB-0495) ✅.** Highlights: extracted 8 new Alg mirrors
(`convertToDefinitionAlg()`/`breakInstanceAlg()`/`alignToObjectAlg()`/
`scatterAlongCurveAlg()`/`applyProportionalFalloffAlg()`/
`vertexSnapToNearestAlg()`/`applyRotationDragAlg()`/
`flattenDescendantsAlg()`/`groupScaleAlg()`, none existed before;
STAB-0483 found and fixed a real duplicate-id bug in Break Instance;
STAB-0486 found and fixed a real Undo/Redo drift bug between the
keyboard/menu/palette entry points; STAB-0487/0488 confirmed the macro
recorder/playback system already correct; STAB-0491 confirmed the
Ctrl+rotate 45° snap is real but the increment is user-configurable
(default 15°); STAB-0493 found and fixed the session's biggest bug — a
glTF-export path where Instance `variantDefinitions` were silently
ignored outside CSG, fixed via a new shared
`Mc3Object::resolvedInstanceDefinitionKey()`; STAB-0494/0495 closed out
the last 2 P3 items — see §3 for the full list. **Work has moved to
S14 (Rendering and Viewport Stability)**: STAB-0496 done (confirmed
`smoke_test` exercises a genuine render, not a stub), STAB-0497 done
(confirmed the renderer's main switch exhaustively handles all 19
`ObjectType` values plus a safe default fallback; added a permanent
`smoke_test_all_objects` ctest), and STAB-0498 done (added
`test/empty_scene.mc3.xml` + `smoke_test_empty_scene` ctest — no
existing fixture covered a genuinely empty scene). Plan-wide totals:
**300 ✅ done, 6 🟡 partial, 109 🧪 has a plan but not executed,
235 📋 not started** out of 650.

**Important architectural decisions:**
- `mc3/` and `mcb/` are pure C++ static libs with **no** CNA/ImGui
  dependency and must stay independently buildable.
- `mc3togltf/` and `mc3tomcb/` are standalone CLI + lib targets, also
  CNA-free.
- CNA is added as a sibling CMake subdirectory
  (`add_subdirectory(../cna ...)`) and must not be modified from this
  repo.
- The editor (`src/MeshCraft/`) is CNA-coupled, but much of its
  command/parsing/validation logic has zero actual CNA dependency once
  ImGui rendering and app-state bookkeeping are set aside — that pure
  logic lives in CNA-free headers (functions suffixed `Alg`, e.g.
  `EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`) that both the real
  app code and the headless test suite `#include` and call directly.
  See §6 for the full pattern.

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`, generated with CLion's bundled cmake
  4.2.2): full reconfigure + rebuild (53 targets) succeeded cleanly,
  last verified at commit `fca6fc1`.
- **Release** (`b-release/`): freshly built and verified in the same
  session as Debug above — 6.25MB binary (vs. Debug's 59.9MB), confirmed
  via `file`/`objdump` to have zero DWARF debug sections.
- **Offline mode** (`-DFETCHCONTENT_UPDATES_DISCONNECTED=ON`): used for
  every standalone/Release build this session without issue.
- **Standalone (CNA-free) component builds**, each configuring/
  building/testing without the root project: `mc3` (1/1), `mcb` (1/1),
  `mc3togltf` (18/18), `mc3tomcb` (2/2) — `mc3togltf` re-verified fresh
  this session after STAB-0493 added `mc3togltf_instance_variant`
  (Mc3, mc3togltf_lib, and mc3togltf all rebuild+test cleanly outside
  the root project).

### Tests
**28/28 CTest pass** in Debug as of this session (STAB-0498 added
`smoke_test_empty_scene`, STAB-0497 added `smoke_test_all_objects`,
STAB-0493 added `mc3togltf_instance_variant`, STAB-0630 added
`mc3togltf_obj_robustness`, STAB-0417/0418/0420/0416 added
`mc3togltf_texture_sampler`, STAB-0166-0170/0411-0415/0435 added
`mc3togltf_material_pbr`, STAB-0440 added
`mc3togltf_svg_texture_export`, STAB-0447/0448 added
`mc3togltf_animation_unsupported`; started the session at 20/20):
`smoke_test`, `smoke_test_all_objects`, `smoke_test_empty_scene`,
`xsd_validation`, `mc3_registry`, `mc3_ai`, `mc3_roundtrip`,
`mc3_commands`, `mcb_roundtrip`, `mc3tomcb_roundtrip`, `mc3togltf_gltf`,
`mc3togltf_all_primitives`, `mc3togltf_export_verification`,
`mc3togltf_large_scene`, `mc3togltf_csg_strict`, `mc3togltf_csg_export`,
`mc3togltf_csg_unsupported`, `mc3togltf_csg_nested`,
`mc3togltf_instance_deform_cache`, `mc3togltf_float_cache_key`,
`mc3togltf_obj_robustness`, `mc3togltf_texture_sampler`,
`mc3togltf_material_pbr`, `mc3togltf_svg_texture_export`,
`mc3togltf_animation_unsupported`, `mc3togltf_instance_variant`,
`mc3togltf_large_scene_generated`, `mc3togltf_large_scene_500`.

- `mc3_commands` (~419 assertions): editor command algorithms, undo/redo
  for every mutating command, auto-save/backup, Save-As/Export-Selection/
  drag-drop workflows, keybinding/preferences/macro persistence,
  hierarchy-panel filtering, AI-panel + unsaved-changes dialog
  lifecycles.
- `mc3_registry` (~94 assertions): ModelRegistry SQLite CRUD, search
  (name/group/tags/description/source), migration, edge cases.
- `mc3_ai` (~55 assertions): AiAssistant JSON helpers, the full
  AI-response validation pipeline (extract → repair → parse →
  empty-check → XSD-validate), 4 mock-HTTP-server round-trips (success,
  truncation, HTTP error, indefinite-hang timeout — no real network),
  and XSD accept/reject regression tests.
- `mc3_roundtrip` (~299 assertions): full XML parser/writer roundtrip,
  all N1–N7 extensions, edge cases.
- `mcb_roundtrip` (~50 assertions): MCB binary roundtrip, base scene +
  all N1–N7 types.

See `TESTING.md` for the full per-test reference and `plan.md`'s
`STAB-XXXX` rows for the exhaustive per-behavior list.

### Tools / libraries available
- `MeshCraft` — editor executable, version `0.1.0` (`--version` to
  print it, new this session). Builds and runs; the 3D viewport is not
  fully integrated into the render loop (see "What does NOT work yet").
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb`.
- `mc3tomcb` — bidirectional CLI: `mc3.xml` ↔ `.mcb`, direction chosen
  by file extension.
- `Mc3` — static lib, scene data + XML load/save.
- `Mcb` — static lib, binary serialization.
- `mc3togltf_lib` — static lib, `GltfExporter`/`MeshBuilder`/
  `CsgEvaluator`.

### What works
- Full XML round-trip for all primitive types and all N1–N7 extensions.
- MCB binary round-trip, matching the XML feature set.
- XSD validation for every test fixture, and now for AI-generated scene
  XML before it's applied to the live scene (STAB-0391).
- glTF/GLB export: all primitives, animations, CSG (incl. nested),
  materials, lights, cameras, instances, groups, OBJ mesh import.
- SQLite-backed asset registry: open/save/search/remove/migration.
- Editor undo/redo (snapshot-based, every mutating command verified).
- Auto-save + 2-slot rotating backup on save (see `README.md`'s new
  "Backup and Recovery" section for the exact mechanics).
- AI Assistant: sends scene + prompt to the Claude API, validates the
  response (markdown-fence/prose tolerant, structural + XSD schema
  checks), applies it or saves definitions to the registry.
- Real headless smoke-testing verified this session: `house.mc3.xml`,
  `garden_house.mc3.xml`, and `features.mc3.xml` all load cleanly
  through the actual editor binary (`--screenshot` mode), not just the
  standalone parser.

### What does NOT work yet
- `EditorViewport` is not integrated into the `MeshCraftApplication`
  render loop.
- SVG texture rasterization: parsed/serialized/round-tripped, but
  `GltfExporter` never reads the SVG texture map at all — silently
  dropped from glTF export, not just deferred.
- Embedded glTF (`<mesh src="embed:id"/>`): parsed/serialized, but
  `GltfExporter` treats `embed:id` as a literal OBJ path, which fails —
  export continues with an empty (meshless) node, doesn't crash.
- N3–N7 scene data (scripts, sounds, music, triggers, scene states,
  meta): fully round-tripped but not executed at runtime anywhere — no
  Lua interpreter, no audio playback, no trigger-firing, no
  state-switching. Intended at this stage (data model before
  execution), not a bug.
- CI workflow exists but is parked deactivated under `.github_/` (see
  §4); no automated full-editor (CNA + SDL3) build/test job runs
  anywhere currently.

---

## 3. Recent changes

**40 STAB tasks committed as of `cb04f50`** (`53aa75e` through
`cb04f50`, pushed to `origin/develop` — see `git log --oneline` for the
full list). **STAB-0498** below is verified and about to be committed
as of this update. Gate 6 is exhausted (§8); **S11, S12, and S13 are
all fully done** except genuinely blocked/flagged items (S11/S12
only). Work is underway on **S14 (Rendering and Viewport
Stability)**.

This was a long, dense session that closed out essentially all of
**Gate 6 (Documentation)**'s reachable work (plus a from-scratch schema
audit that found real bugs), then finished S11 and S12 entirely (bar
the blocked/flagged items) and started S13. Highlights, most recent
first:

- **STAB-0498 — added an empty-scene fixture + ctest**: no existing
  fixture had a genuinely empty `<objects/>` scene. Added
  `test/empty_scene.mc3.xml` (the minimal possible valid document — no
  environment/materials/definitions either) and confirmed via a real
  `--screenshot` run that it loads and renders cleanly (an empty scene
  still draws its background/grid/skybox), exit 0. Added a permanent
  `smoke_test_empty_scene` ctest (same mechanism as STAB-0496/0497).
- **STAB-0497 — confirmed the renderer handles every object type,
  added `smoke_test_all_objects`**: `drawObject()`'s main switch
  (`SceneRenderer.cpp`) exhaustively handles all 19 `ObjectType` values
  (every primitive, `Group`/`Area`, the 3 CSG ops, `Instance`,
  `Extrude`, `Mesh`) plus a safe `default:` placeholder-box fallback —
  no crash possible by construction, even for a future/unknown type.
  Empirically confirmed via real `--screenshot` runs against
  `all_objects.mc3.xml`, `all_primitives.mc3.xml` (11 primitive
  shapes), and `csg_nested.mc3.xml` (nested CSG) — all exit 0 with
  genuinely varied pixel output (383-757 distinct sampled colors,
  ruling out a stub render). Added a permanent `smoke_test_all_objects`
  ctest (same `smoke_test.sh` mechanism as STAB-0496, pointed at
  `all_objects.mc3.xml`) so this stays verified going forward, not just
  this session.
- **STAB-0496 — verified `smoke_test` exercises a genuine render**:
  the ctest already passed, but to confirm it isn't a trivial stub
  (e.g. a cleared framebuffer that just happens to produce a non-empty
  file), sampled the actual PPM pixel data for all 3 sample scenes
  (`house.mc3.xml`, `garden_house.mc3.xml`, `features.mc3.xml`) and
  found 366/911/839 distinct RGB colors respectively — proving real
  lit 3D geometry is drawn, not a flat/uninitialized buffer (which
  would show ~1-3 colors). No code change needed. **First item of S14
  (Rendering and Viewport Stability) done.**
- **STAB-0495 — extracted `groupScaleAlg()`, closes S13**: confirmed
  exactly correct against the row's own example (2 objects at (0,0,0)
  and (2,0,0), group-scaled 2x, end up at (-1,0,0) and (3,0,0) — each
  pushed twice as far from their shared centroid). No Alg mirror
  existed. Extracted `groupScaleAlg()` into `EditorAlgorithms.hpp`;
  wired the real `groupScaleSelected()` to call it, and as a side
  effect fixed its status-message count to reflect objects *actually*
  scaled rather than the full selection size (same fix shape as
  STAB-0484's `alignToObject`). Added `testGroupScale()` (12
  assertions): the row's own example verified exactly; factor<1
  shrinks toward the centroid; a locked object is excluded from the
  move/scale and the count but still correctly contributes to the
  centroid average; empty selection is a no-op. **This closes S13
  (Commands, Undo/Redo, and Algorithms) — all 25 items done.**
- **STAB-0494 — confirmed negative arrayDuplicate count clamps
  gracefully, doesn't silently no-op**: the row expected "count=-1 ->
  no objects added"; actual (also safe) behavior is
  `arrayDuplicateObjects()`'s existing `count = std::max(2, count)`
  clamp, which produces exactly 1 copy for any too-small count
  (0, 1, or negative) — never a crash, never a silent zero-op. The
  real ImGui slider is clamped to `[2,20]`, so a negative count is
  only reachable via a hand-edited/hostile `.mc3macro` `linear_array`
  step. Added 2 assertions to `testArrayDuplicate()`: `count=-1`
  produces exactly 1 copy, and `count=-1000000` (guarding against an
  extreme negative being misinterpreted as a huge loop count) also
  safely produces exactly 1 copy.
- **STAB-0493 — found and fixed a real glTF-export bug in Instance
  "variants" resolution**: the row's file (`MeshCraftApplication_Commands.cpp`)
  was wrong — "Random variant" isn't an editor command at all, it's a
  data-model feature (`Mc3Object::variantDefinitions` +
  `makeInstanceVariant()`) resolved deterministically per-object by
  hashing the object's own id (stable across runs — the correct design
  for reproducible exports, not a literal per-call coin flip).
  `SceneRenderer.cpp` (live viewport) and `CsgEvaluator.cpp` (CSG
  export) both already resolved this correctly via an identical
  duplicated hash formula, but **`GltfExporter.cpp`'s regular
  (non-CSG) Instance path ignored `variantDefinitions` entirely and
  always used `definition`** (always the first variant) — what you see
  in the editor viewport was not what you got in a regular (non-CSG)
  glTF export. Its mesh-cache key had the same bug compounded (keyed
  off the always-identical `obj.definition` rather than the resolved
  key), which would have caused wrong mesh reuse across differently-
  resolved variant instances once the primary bug was fixed, had the
  cache key not been fixed at the same time. Also found and fixed the
  identical gap in `breakInstanceAlg()` (STAB-0483, this session),
  `SceneRenderer_Extrude.cpp`'s edge-overlay wireframe path (would draw
  the wrong variant's outline vs. its own solid mesh), and the left
  panel's "rename definition key" flow (patched `Instance::definition`
  on rename but not matching entries inside `variantDefinitions`,
  leaving dangling references). Consolidated everything into one new
  shared `Mc3Object::resolvedInstanceDefinitionKey()` method
  (`mc3/include/MeshCraft/Mc3/Mc3Object.hpp`+`.cpp` — additive, not a
  breaking API change) and wired every consumer through it. Added
  `test/instance_variant.mc3.xml` (8 instances, distinct ids, same
  2-entry variant list) + `mc3togltf_instance_variant` ctest: asserts
  real variation occurs (exactly 2 distinct meshes used across the 8
  instances, not a collapse onto one — the regression check that would
  have failed pre-fix) and that resolution is deterministic across
  repeated export runs. Verified via full CMake reconfigure + rebuild
  (0 warnings), 26/26 ctest (up from 25/25), a real `--screenshot`
  smoke test, and a standalone (CNA-free) `mc3togltf` re-verification
  (18/18).
- **STAB-0492 — extracted `flattenDescendantsAlg()`**: `selectChildren()`
  was already correct — its recursive `addAll` lambda walked every
  descendant at any depth, not just direct children — but had no Alg
  mirror. Extracted `flattenDescendantsAlg()` into
  `EditorAlgorithms.hpp`; wired the real function to call it. Added
  `testFlattenDescendants()` (5 assertions) with a 3-level tree
  (children → grandchildren → one great-grandchild): all 5 descendants
  present regardless of depth, pre-order confirmed, childless object
  flattens to an empty list.
- **STAB-0491 — extracted `applyRotationDragAlg()`, confirmed 45° is a
  preset not a default**: the row's phrasing implied a hardcoded 45°
  snap; actual behavior rounds to the configurable `snapRotate_`
  (default 15°, quick-select presets 5/10/15/30/45/90°) — 45° is
  achievable, just not the default. Also confirmed the Ctrl-held
  momentary snap override is **deliberately rotation-only**: Move/Scale
  drags only check the persistent Snap-to-grid toggle, while rotation
  checks `snapEnabled_ || ctrlHeld` — verified intentional via the
  matching "Ctrl or snap grid" status-bar indicator condition in
  `MeshCraftApplication_UiOverlays.cpp`. No Alg mirror existed.
  Extracted `applyRotationDragAlg()` (accumulate delta on the target
  axis, optional round-to-increment, skip locked, return rotated
  count) into `EditorAlgorithms.hpp`; wired the real code to call it.
  Added `testRotationDragSnap()` (12 assertions): delta accumulates on
  only the targeted axis across multiple objects; locked objects are
  skipped and excluded from the count; a post-delta value snaps
  correctly both up and down to the nearest 45° increment; empty
  selection is a no-op.
- **STAB-0490 — extracted `vertexSnapToNearestAlg()`**: the "Vertex
  snap (Shift)" block right next to STAB-0489's falloff code in the
  same `handleMouseInput()` function was already correct — nearest-
  unselected-object search within a camera-distance-scaled threshold
  from the anchor object's position, then a rigid offset applied to
  the whole selection — but had no Alg mirror either. Extracted
  `vertexSnapToNearestAlg()` (search + rigid-offset apply, taking an
  already-resolved reference point since the anchor/threshold
  derivation is mouse/camera-driven) into `EditorAlgorithms.hpp`;
  wired the real code to call it. Added `testVertexSnap()` (11
  assertions): a found target snaps the anchor exactly onto it and
  moves every other selected object by the identical offset (rigid,
  not independently re-centered); no target within threshold correctly
  no-ops and returns `false`; the *nearest* of several candidates wins;
  a locked selected object is excluded from the move even though the
  search itself doesn't consult lock state; empty selection is a
  no-op.
- **STAB-0489 — extracted `applyProportionalFalloffAlg()`**: the
  Gaussian falloff math inside `handleMouseInput()`'s `applyFalloff`
  lambda (weight 1.0 at zero distance, decreasing to 0 at the radius
  edge exclusive, skipping selected/locked objects, centered on the
  selection's average position) was already correct but completely
  untested — the whole function is one giant mouse-drag handler with
  no Alg mirror for this piece. Extracted `proportionalFalloffWeightAlg()`
  (pure weight formula) + `applyProportionalFalloffAlg()` (selection-
  center + recursive walk + apply, taking an already-resolved
  world-space delta since axis resolution is mouse/gizmo-driven) into
  `EditorAlgorithms.hpp`; wired the real code to call it. Added
  `testProportionalFalloff()` (14 assertions): weight function edge
  cases (zero distance, at/beyond radius, strictly decreasing,
  non-positive radius), the apply function's near/far/locked-neighbor
  selectivity, multi-object selection using the *average* position as
  influence center, and empty-selection/zero-radius no-ops.
- **STAB-0487 + STAB-0488 — confirmed the macro recorder/playback
  system is already correct**: STAB-0487 confirmed `batchRenameSelected()`
  unconditionally records a `"batch_rename"` step with the rename
  pattern as its arg, and playback replays it through the exact same
  real function — already exhaustively covered by the existing
  `testMacroSaveLoadRoundTrip()` (STAB-0292), whose fixture already
  includes a `batch_rename` step. STAB-0488's row assumes a
  name-lookup-and-skip mechanism that doesn't exist by design: macro
  steps never store an object name/id, only the verb and its args —
  every step operates on whatever is selected *at playback time*.
  Verified every selection-consuming command
  (`duplicateSelected`/`groupSelected`/`ungroupSelected`/
  `groupScaleSelected`/`arrayDuplicate`/`batchRenameSelected`) already
  guards on empty selection, so replaying a macro recorded on an object
  that's absent at playback time is already a safe no-op for every
  step. No bug, no code change needed for either.
- **STAB-0486 — found and fixed a real Undo/Redo drift bug**: 3
  independent hand-copied implementations of Undo/Redo had silently
  diverged — the keyboard shortcut path (`MeshCraftApplication_Keyboard.cpp`)
  enforced the `kUndoMax` cap on the *opposite* stack (e.g. redo-stack
  growth while undoing) and called `evaluateAndPushAnimOverrides()` to
  refresh animation state after the swap, but the Edit-menu and command-
  palette copies did neither — undoing/redoing via mouse could grow the
  opposite stack past the 20-entry cap and leave stale animation
  overrides applied until something else (e.g. playback) refreshed
  them. Also found "Drop to Ground Plane" duplicated between menu and
  palette with a cosmetic status-message drift (menu reported the
  actual dropped count, palette always said "Dropped to ground plane").
  Fixed both by extracting shared `performUndo()`/`performRedo()`/
  `dropSelectedToGroundPlane()` private methods
  (`MeshCraftApplication_Commands.cpp`) and wiring all 3 (or 2) call
  sites to them — future changes can no longer re-diverge. No new
  headless test possible (CNA-coupled `MeshCraftApplication` methods
  can't be instantiated in `mc3_commands_test`); the shared cap logic
  itself is already covered by `testUndoStackDepthCapped`. Verified via
  full rebuild (0 warnings), 25/25 ctest, and a real `--screenshot`
  smoke test against `house.mc3.xml`.
- **STAB-0485 — extracted `scatterAlongCurveAlg()`**: this row's
  phrasing ("N=10 -> exactly 10 new objects") doesn't match intended
  behavior — the UI itself documents `count-1` new objects
  (`MeshCraftApplication_UiOverlays.cpp:826`, `"(%d new)"` label),
  since `count` is the total number of points along the curve
  *including* the untouched original, not a bug. No Alg mirror existed
  for this command (same gap as STAB-0482/0483/0484). Extracted
  `ScatterCurveParamsAlg` + `scatterCurvePositionAlg()` (pure line/arc
  position math) + `scatterAlongCurveAlg()` (the splice-and-copy loop,
  taking an injectable `jitterRng` for determinism) into
  `EditorAlgorithms.hpp`; wired the real `scatterAlongCurve()` to call
  it. Added `testScatterAlongCurve()` (21 assertions): count=10/1
  source -> exactly 9 new objects and the original untouched; multiple
  sources each independently get `count-1` copies inserted right after
  themselves; line- and arc-mode position math verified against
  `scatterCurvePositionAlg`; a fixed jitter function gives exact,
  deterministic per-axis offsets; `count<2`/empty-selection no-ops.
- **STAB-0484 — extracted `alignToObjectAlg()`**: "Align to Object"
  had no Alg mirror either (same gap as STAB-0482/0483). Confirmed the
  command only aligns *position* (not rotation/scale) of every
  non-target selected object to the first-selected "target" object,
  skipping locked objects. As a side effect of the extraction, fixed
  the status-message object count (previously always reported
  `selectionSize-1`, over-counting when a selected object was locked
  and skipped — now reports the actual aligned count). Added
  `testAlignToObject()` (11 assertions): the align itself, rotation
  left untouched, locked-object skip, and <2-selected/empty no-ops.
- **STAB-0483 — found and fixed a duplicate-id bug in "Break
  Instance"**: the original code only uniquified the *top-level*
  copy's id (a dead `copy->id = inst->id;` assignment was immediately
  overwritten by the real uniquification logic) — every **descendant**
  kept the definition template's original id verbatim. Breaking a
  second Instance of the *same* definition (placing multiple copies of
  a prop is a common workflow) produced exact duplicate child ids,
  which matters since `id` is used elsewhere as a set key (`lockedIds_`,
  43 references across the codebase). Extracted `breakInstanceAlg()` +
  a new `regenerateSubtreeIdsAlg()` helper (recursively assigns a
  fresh, document-unique id to the whole copied subtree) +
  `flatFindByIdAlg()` into `EditorAlgorithms.hpp` (no Alg mirror
  existed for this command before, same gap as STAB-0482). Added
  `testBreakInstance()` (20 assertions) — the key one walks every id in
  two broken-instance copies of the same definition into a `std::set`
  and confirms every insert succeeds (no duplicates anywhere).
- **STAB-0482 — extracted `convertToDefinitionAlg()`**: "Convert to
  Definition" had no Alg mirror yet, unlike duplicate/group/ungroup
  (already covered). Extracted the command's document mutation into
  `EditorAlgorithms.hpp`, wired the real `convertToDefinition()` to
  call it (single source of truth, ~30 fewer duplicated lines). Added
  `testConvertToDefinition()` (20 assertions): the new definition is a
  deep copy with transform reset to identity but type/children
  preserved; the replacement Instance preserves the original
  transform/visibility/tags/id/name and correctly references the new
  key; object count stays the same (in-place replacement); a second
  conversion gets a distinct key with no collision.
- **STAB-0457-0470 — S12 closeout**: STAB-0457-0459/0461-0463/0465-0467/
  0470 all confirmed correct by code inspection or already covered by
  existing tests (deleted/included/registry-inserted objects are
  equally animatable since animation channels resolve by name against
  the whole merged object tree with no origin distinction; the shared
  `evaluateChannel()` interpolation math and its glTF-export/sampler
  consistency were already thoroughly tested; mc3 time units are
  already seconds, no frame-rate conversion exists or is needed — added
  a clarifying note to `MC3_FORMAT.md` and fixed a stale line there
  about SVG textures predating this session's STAB-0440 fix).
  **STAB-0460 and STAB-0464 flagged** (🟡, not resolved): STAB-0460
  is a "scale all keyframe times" feature that was never built at all
  (confirmed by exhaustive grep) — out of scope per the stabilization
  feature moratorium; STAB-0464's curve-editor drawing code exists but
  needs a live display to confirm it's visually correct.
  **STAB-0468 found a real gap and fixed it**: keyframes were sorted
  with plain `std::sort` (not stability-guaranteed for equal keys), so
  two keyframes at an identical time had a formally unspecified
  tie-break order. Switched to `std::stable_sort` for a well-defined
  first-declared-wins policy; also confirmed `evaluateChannel()` was
  already safe against a zero-length time span (explicit guard for
  Linear, inherently safe for CubicBezier via bisection search — no
  NaN/crash risk either way). **STAB-0469 found a real gap and fixed
  it**: `mergeSceneFromFile()` merged textures/materials/objects but
  never touched `src.actions` at all — merging a scene silently
  discarded 100% of its animations. Fixed with the same
  suffix-on-collision pattern already used for materials. Separately
  noted (not fixed, flagged in §5): the same function's *texture*
  merge still uses skip-on-collision rather than the suffix+
  reference-remap pattern STAB-0425 established, so a merged material
  could end up referencing the wrong texture on a name collision.
- **STAB-0449-0456 — S12 timeline UI sweep**: STAB-0449-0453 (timeline
  multi-select, delete, duplicate action ["Dup" button, initially
  missed searching for the literal word "Duplicate"], rename action
  ["Ren" button, not literally double-click as the row assumed, but
  functionally correct with proper name-collision validation],
  copy/paste keyframes) and STAB-0454/0455 (undo-safety for add/delete
  keyframe) all verified correct by code inspection — the mouse/pixel
  hit-testing itself needs a live display, but the underlying data
  operations are directly readable: multi-select is a `std::set`
  toggle, delete uses reverse-sorted erasure to avoid index
  invalidation, copy/paste preserves relative keyframe timing, and
  every mutation is preceded by an unconditional `pushUndo()`.
  **STAB-0456 found a real bug and fixed it**: renaming an object via
  any of its 3 real editor paths (Properties panel, hierarchy panel,
  batch rename) never propagated to that object's animation channels
  (`Mc3Channel::targetObject` is a plain name string, not a stable id)
  — silently orphaning its animations, safe (no crash, both export and
  playback already guard a missing target) but permanently broken with
  no indication why. Added a shared `renameObjectInActionsAlg()` helper
  to `EditorAlgorithms.hpp`, wired into all 3 call sites (`batchRenameObjects()`
  gained a new optional `actions` pointer parameter, nullable so its 5
  existing headless-test call sites kept compiling unchanged). Added
  `testRenameObjectInActionsAlg()` (7 assertions).
- **STAB-0441-0448 — S12 roundtrip + glTF-export animation sweep**:
  STAB-0441 (position)/0442 (rotation)/0444 (material) were already
  covered by existing tests (`testAnimationCubicBezier`/
  `testAnimationLinear`/`testAnimationMultiAction`, and this session's
  own `testMaterialColorAnimationRoundtrip`). STAB-0443 (scale) and
  STAB-0445 (deform) were genuine gaps — no test had ever used a
  `Scale*`/`Deform*` channel — added `testAnimationScaleRoundtrip()`/
  `testAnimationDeformRoundtrip()`. STAB-0446 was already thoroughly
  covered by `gltf_test.py`'s `test_animation()` (position/rotation/
  scale all verified exporting to the correct glTF path with correct
  accessor types). STAB-0447/0448: confirmed `GltfExporter.cpp`
  already warns and excludes any unsupported animated property
  (material/deform channels use the same code path proven for
  visible-only actions) but no fixture had ever exercised a material
  or deform channel specifically — added
  `test/animation_unsupported.mc3.xml` + `mc3togltf_animation_unsupported`
  ctest confirming both warn correctly and are excluded from the
  exported `animations` array.
- **STAB-0437/0438/0439/0440 — S11 closeout**: STAB-0437 confirmed a
  `<material>` with no `id` collides gracefully on `id=""` (last-write-
  wins, no crash) and — critically — the exporter's `!matName.empty()`
  guard means a materialless object never accidentally resolves to an
  unnamed material; added `testUnnamedMaterialHandledGracefully()`.
  STAB-0438 confirmed alpha=0 with no explicit `alpha_mode` exports as
  OPAQUE (no auto-BLEND) — matches STAB-0170's earlier finding that
  `alpha_mode` is explicit, not auto-derived; also checked the live
  editor and found it doesn't alpha-blend either (no `GL_BLEND`
  anywhere in `SceneRenderer.cpp`), so there's no editor-vs-export
  discrepancy — this is correct-by-design, a content-authoring
  responsibility, not a bug. STAB-0439 found the real `baseColorFactor`
  default for an emissive-only material is `[0.8,0.8,0.8,1.0]`, not
  `[1,1,1,1]` as the row assumed — matches `Mc3Material`'s actual
  default, correct behavior, wrong row expectation. **STAB-0440 found
  a real gap and fixed it**: SVG textures were already documented as
  unsupported, but silently — no warning was ever printed. Added a
  `doc.svgTextures` check to `GltfExporter.cpp`'s `buildMaterial()`
  so an unresolvable SVG-sourced texture reference now prints a
  specific warning (naming the material, slot, and texture id) instead
  of vanishing silently; added `test/svg_material.mc3.xml` +
  `mc3togltf_svg_texture_export` ctest.
- **STAB-0432/0433/0434/0435/0436 — final S11 P3 sweep**: STAB-0432
  added `testUvMappingRoundtrip()` (`Mc3UvMapping` had zero prior
  coverage — projection/scale/offset/rotation all confirmed exact).
  STAB-0433 added a "Path resolution" note to `MC3_FORMAT.md`'s
  Textures section — **found texture `uri` resolution is subtly
  different from `<include>`'s own rule**: it always resolves relative
  to the *top-level* scene file's directory, even for `<texture>`
  elements declared inside an included file (verified consistent
  across both real consumers, `GltfExporter.cpp` and
  `SceneRenderer.cpp`). STAB-0434 confirmed the editor's "Embed
  textures" export-dialog checkbox is intentionally disabled/inert
  (tagged "not yet supported") — actual embed-vs-external behavior is
  determined entirely by output extension (`.glb`/`.gltf`), already
  covered by this session's STAB-0416/0420 work. STAB-0435 extended
  `mc3togltf_material_pbr` to verify each test object's exported
  node→mesh→primitive→material index chain (not just that materials
  exist by name). STAB-0436 added
  `testMaterialColorPrecisionRoundtrip()`, confirming colors are never
  quantized to `uint8` anywhere in the pipeline and an 8-bit-derived
  value survives well within the row's 1e-5 tolerance despite `%.6g`
  (6-sig-fig) XML serialization.
- **STAB-0429 + STAB-0430 — closed 2 genuine material test-coverage
  gaps**: STAB-0429 added `testMaterialAllFieldsRoundtrip()` — a
  "kitchen sink" material with all 5 texture slots, non-default
  base/emissive color, roughness/metallic, `alpha_mode="mask"`+cutoff,
  and `double_sided`, all 25 assertions confirming exact save→reload
  survival (no prior test populated every field on one material at
  once). STAB-0430 added `testMaterialColorAnimationRoundtrip()` — 6
  channels (baseColor R/G/B, roughness, metallic, emissiveR), 2
  keyframes each, 30 assertions: confirmed
  `AnimatedProperty::MaterialBaseColorR`-etc. already have a complete
  string mapping the writer/parser correctly call (`Mc3Animation.cpp`),
  but literally no test had ever exercised a material animation channel
  before (only position/rotation/scale/deform/visible were covered) —
  all pass, no code changes needed for either, purely closing untested
  gaps.
- **STAB-0426 + STAB-0427 — verified material search + documented D3's
  skip reason**: STAB-0426 confirmed by code reading (inline ImGui
  code, not extracted to a testable header, but a plain string
  algorithm — verifiable without a display, unlike the preview
  *sphere*): filter and material key are both lowercased before a
  substring match, correctly matching the row's own `"metal"` →
  `"Metal_01"`/`"METAL_rusty"` example. STAB-0427: traced "D3" to the
  pre-650 task list (`"Material: duplicate material in the material
  list"`, a proposed feature, superseded and not in this repo's
  tracked docs) — confirmed via grep there's no "duplicate material"
  code anywhere (genuinely unimplemented, not a half-finished ghost),
  and documented why: it's a new feature, and this project is
  explicitly in a feature moratorium during stabilization (same policy
  that already defers SVG rasterization and embedded-glTF-by-reference).
- **STAB-0425 — registry insert silently kept the wrong material on a
  name collision**: the 4th real S11 bug this session. `insertIntoScene()`
  already gave the imported *definition* id a numeric suffix on
  collision, but materials/textures used silent "skip if already
  present" — two independently-authored registry entries reusing a
  generic name like `"wood"` would leave the second insert's object
  pointing at the *first* entry's unrelated `"wood"` material, wrong
  color and all, no error. Fixed: materials/textures now get the same
  suffix treatment as the definition id, plus a `remapMaterialRefs()`
  walk over the inserted object tree so it always ends up pointing at
  its own (possibly renamed) material. Added `testInsertMaterialNameCollision`
  (8 new assertions in `mc3_registry_test.cpp`).
- **STAB-0424 — material id collision in `<include>`/merge**: already
  fully covered by existing (currently passing) test coverage — no new
  code or test needed. Confirmed the policy in code: `<include>`
  elements process before the local file's own `<materials>` section
  (explicit comment in `Mc3XmlParser.cpp`), and the local parse does a
  plain map overwrite, so **local (including file) always wins** on an
  id collision. `roundtrip_test.cpp`'s existing `testIncludeOverride()`
  already asserts exactly this, including round-trip survival.
- **STAB-0416 — texture URI subdirectory silently stripped on export**:
  the 3rd real S11 export bug this session. tinygltf's *default* image
  writer (`tinygltf::WriteImageData`, vendored) truncates
  external-reference URIs to `GetBaseFilename()` — `uri="textures/a.png"`
  was silently exported as `uri="a.png"`, which would break texture
  loading in any tool expecting the original relative layout (that
  default behavior is designed for tinygltf's own "auto-write image
  bytes next to the gltf" workflow, which mc3togltf doesn't use for
  external references). Fixed by registering a custom
  `writer.SetImageWriter(...)` that preserves the original URI verbatim
  for external references and delegates to the real default for
  embedded images (`--embed`/`.glb`, confirmed unaffected). Extended
  `mc3togltf_texture_sampler` to assert exact `uri` values.
- **STAB-0166-0170 (S2/S3) + STAB-0411-0415 (S11) — verified material
  PBR export** (10 duplicate-pair rows, same underlying check,
  closed together): `base_color` → `baseColorFactor` (all 4 components
  incl. alpha), `metallic` → `metallicFactor`, `roughness` →
  `roughnessFactor`, `emissive_color` → `emissiveFactor`, `alpha_mode`
  → `alphaMode`/`alphaCutoff` (`blend`→BLEND, `mask`+cutoff→MASK) all
  confirmed correct via a new `test/material_pbr.mc3.xml` fixture (4
  materials covering each case) and `mc3togltf_material_pbr` ctest —
  no code changes needed, this was genuinely already correct, just
  untested.
- **STAB-0419 + STAB-0420 — verified texture colorSpace/missing-file
  handling**: `tex.colorSpace` is never read by `GltfExporter.cpp`, but
  this is *correct*, not a gap — glTF core has no per-texture
  colorSpace property; the spec mandates it implicitly by material
  slot (baseColor/emissive = sRGB, normal/metallicRoughness/occlusion
  = linear, non-negotiable), and the exporter never decodes/re-encodes
  pixels anyway (raw PNG bytes copied verbatim). Checked real content
  (`test/human.mc3.xml`): mc3 authors already assign `color_space`
  consistent with glTF's per-slot convention. For STAB-0420: default
  `.gltf` mode correctly doesn't warn on a missing texture file (it's
  just an external URI reference, resolved later by the consuming
  tool); `--embed`/`.glb` mode (which actually reads pixel bytes)
  already warns per-texture and still exits 0 — extended
  `mc3togltf_texture_sampler` to cover the `.glb` path explicitly.
- **STAB-0417 + STAB-0418 — texture sampler wrap/filter export bugs**:
  found 2 more real gaps while starting S11. `mc3.xsd`'s `wrapModeType`
  allows `repeat`/`clamp`/`mirror`, but `GltfExporter.cpp`'s
  `buildTextures()` only checked for `"clamp"` — `wrap_u="mirror"`
  silently exported as `REPEAT` instead of `MIRRORED_REPEAT`. Separately,
  `tex.filter` (`linear`/`nearest`) was **never read at all** — every
  sampler got hardcoded `LINEAR`/`LINEAR_MIPMAP_LINEAR` regardless of
  the mc3 setting. Fixed both; added a permament `mc3togltf_texture_sampler`
  ctest (`test/texture_sampler.mc3.xml`, one texture per wrap
  mode + one `filter="nearest"`) asserting exact glTF sampler enum
  values. **Also noticed but not fixed**: `mc3.xsd` declares a
  `mip_maps` texture attribute (default `true`) that has **zero**
  representation anywhere — no `Mc3Texture` field, no parser read, no
  writer emit, no exporter use. A schema-valid `mip_maps="false"`
  silently does nothing. Flagged in §5, not fixed (out of scope for
  STAB-0417/0418, no assigned STAB-XXXX ID of its own).
- **STAB-0616/0617/0618 — audited the 3 large-source-file items**:
  `MeshCraftApplication.hpp` (613 lines) is internally organized into
  clearly-labeled member groups, with its *implementation* already
  split across ~15 `MeshCraftApplication_*.cpp` files by feature — no
  further split needed. `SceneRenderer.cpp` (1362-line core, alongside
  the already-split `_Builders`/`_Extrude`/`_Gizmos` files) is
  internally organized (mesh primitives → draw dispatch → debug-icon
  gizmos → stats) — confirmed the existing 4-file split is good
  practice, as the row expected, no further split. `PropertiesPanel.cpp`
  is genuinely different: `draw()` is a single ~1770-line function with
  almost no internal section markers — a real smell, and splitting it
  by object-type (as the row suggests) would be a legitimate, low-risk
  mechanical extraction *in principle*, but ImGui code is easy to
  subtly break on extraction (ID-stack/layout ordering) and this
  environment has **no way to visually verify a UI panel still renders
  correctly** (no display, viewport not integrated). Flagged 🟡 (confirmed
  real, deferred pending a human clicking through the live editor) —
  not attempted, unlike the other two which are genuinely fine as-is.
- **STAB-0612 + STAB-0613 — verified `Mc3XmlParser.cpp`'s
  safe-attribute-helper usage and TU-local linkage**: every one of the
  ~30 direct `->Attribute(...)` calls outside the `attr()`/`attrF()`/
  `attrB()`/`attrVec3()` helpers themselves is a deliberate
  presence-check guard, not a bypass. The file uses `static` (not a
  literal `namespace{}` block) for all 37 free functions, giving the
  same TU-local/no-ODR-conflict guarantee — only `Mc3XmlParser::parse`
  itself has external linkage, and it's a properly header-declared
  class method. Both clean — no fix needed. Same commit also closed
  **STAB-0147/STAB-0619** (S3/S18): MCB tag constants were already
  extracted to a shared header, just under the name `McbFormat.hpp`
  rather than the `McbTags.hpp` both rows assumed.
- **STAB-0607 + STAB-0608 — verified `mcb`/`mc3` library hygiene**: no
  raw `new`/`delete`/`malloc`/`free` in `McbWriter.cpp`/`McbReader.cpp`
  (1405 lines combined, 0 hits; ownership via `std::make_shared` +
  RAII containers); no raw `fopen`/`FILE*` in
  `Mc3XmlParser.cpp`/`Mc3XmlWriter.cpp` (all paths are
  `std::filesystem::path`, disk I/O goes through tinyxml2's
  `LoadFile`/`SaveFile`). Both clean — no fix needed.
- **STAB-0606 — checked whether `Mc3XmlParser` needs a non-throwing
  API**: read every real call site of `Mc3Document::loadFromFile` (7
  production sites across the editor, `mc3togltf`, `mc3tomcb`, plus
  extensive test coverage) — every one already wraps the call in
  try/catch and handles the exception correctly, including this
  session's own hostile-input tests (STAB-0625/0628/0629) which
  specifically rely on the throwing behavior. The `plan.md` row itself
  hedges this as "Optional." Adding a parallel non-throwing
  `loadFromFile(path, &error)` variant would be speculative API
  surface with zero current consumer — not done, per project policy
  against adding features beyond what's required.
- **STAB-0605 — verified logging is consistent**: grep-driven audit
  across `mc3/src`, `mc3togltf/src`, `mcb/src`, `mc3tomcb/src`,
  `src/MeshCraft`. 0 raw `printf`/`fprintf` calls anywhere (only
  `std::snprintf` into local string buffers for UI labels/XML number
  formatting — a different concern, not diagnostics). Libraries use
  `std::cerr` with a `"Warning: "`/`"Error: "` prefix and never touch
  stdout; CLI tools (`mc3togltf`, `mc3tomcb`) use `std::cout` for their
  actual output plus `std::cerr` for failures; the editor tags every
  message with a `[Subsystem]` prefix (`[Bloom]`, `[Skybox]`, `[SSAO]`,
  etc.). Consistent throughout — no fix needed, no custom logger
  warranted.
- **STAB-0604 — audited `SceneRenderer_Builders.cpp` vs
  `MeshBuilder.cpp` for duplicate geometry code**: read both files in
  full, compared box + UV-sphere construction line-by-line as
  representative samples. Same underlying parametric math (identical
  ring/sector UV-sphere formula, same 6-face box topology) but
  genuinely different code shapes: the renderer side builds unit-sized
  shapes directly into 2 different interleaved GPU vertex formats via
  `uint16_t`-indexed buffer uploads (CNA-coupled); the export side
  builds arbitrarily-dimensioned shapes into flat position/normal/
  texcoord/`uint32_t`-index vectors for `tinygltf` (CNA-free), with a
  shared `addQuad()` helper and axis-remapping the renderer side
  doesn't need. Extracting a shared utility would only add an
  abstraction layer to bridge the two representations, with no runtime
  benefit and real risk to two independently-tested core paths — **not
  done**, documented as an intentional non-action.
- **STAB-0611 + STAB-0635 — shared `uniqueTempPath()` helper, fixing a
  real tmp-file collision risk**: while investigating STAB-0635 (whose
  `plan.md` row named `AiAssistant.cpp` as the key file — slightly off;
  it has no temp files at all), found the *same* bug pattern
  independently duplicated in 3 places: `ModelRegistry.cpp`'s
  `entryFromDef`/`insertIntoScene`, `AiResponseAlgorithms.hpp`'s
  `parseXmlAlg`, and `MeshCraftApplication_UiAi.cpp`'s `serializeScene`
  each built their tmp filename from a local `std::atomic<int>` counter
  starting at 0 — safe within one process, but two MeshCraft processes
  (or an app instance + a test run) sharing the same OS temp directory
  could collide on the exact same filename. Added
  `include/MeshCraft/TempFile.hpp`'s `uniqueTempPath(prefix, extension)`
  (random 64-bit suffix, `thread_local` RNG seeded from
  `std::random_device`) — this is exactly what the still-open
  STAB-0611 asked for ("shared between AI assistant and any future
  user"), so fixing STAB-0635 properly meant doing STAB-0611 first and
  using it at all 3 sites rather than patching each one differently.
  Added a 1000-iteration no-collision regression test to `ai_test`.
  Verified: 21/21 ctest, plus a direct run of `mc3_registry_test`
  (exercises the changed `ModelRegistry.cpp` paths) — all pass.
- **STAB-0603 — `EditorAlgorithms.hpp` moved to a public include
  directory**: was at `src/MeshCraft/EditorAlgorithms.hpp`, included
  via a fragile `src/`-relative path (`#include "EditorAlgorithms.hpp"`)
  that only worked because `mc3_commands_test` explicitly added
  `src/MeshCraft` to its include dirs. Moved to
  `include/MeshCraft/EditorAlgorithms.hpp`, matching the existing
  public-include convention every other header uses (e.g.
  `AiAssistant.hpp`); updated both real `#include`s and
  `mc3/CMakeLists.txt`'s `mc3_commands_test` guard (now checks
  `include/MeshCraft/EditorAlgorithms.hpp`, still gracefully skipped in
  a standalone `mc3/` checkout where that path doesn't exist — verified
  by rebuilding standalone `mc3` fresh: 1/1, correctly skips
  `mc3_commands`). Root project rebuild: 21/21 ctest still pass.
- **STAB-0633 — AI apply requires undo-capable state**: confirmed by
  code inspection — `document_` is assigned from `aiPendingDoc_` at
  exactly one call site (`MeshCraftApplication_UiAi.cpp:266`, the
  "Apply to Scene" button), unconditionally preceded on the immediately
  prior line by `pushUndo()`. No other code path reaches that
  assignment, so apply-without-undo-push is impossible by construction.
  No code change, no new test (would just re-assert that two adjacent
  lines execute in order, which C++ already guarantees). **This closes
  S19 down to a single remaining P3** (STAB-0635).
- **STAB-0383 + STAB-0631 — AI network timeout**: confirmed
  `AiAssistant`'s `httplib::Client` already had finite connect/read/write
  timeouts (30s/600s/120s) — not infinite. Exposed them as public
  overridable members (`connectTimeoutSec`/`readTimeoutSec`/
  `writeTimeoutSec`, mirroring the existing `apiBaseUrl` test-override
  pattern) instead of hardcoded literals, purely so a fast test could
  verify the *behavior* rather than just read the numbers off the page.
  Added a mock-server test where the handler blocks on a condition
  variable forever (simulating a truly hung server, not just a slow
  one) with a 1s override — confirmed `sendAsync()` still returns with
  `hasError()` within ~1s, not indefinitely. Test runs in ~1.2s wall
  time, no change to production timeout values.
- **STAB-0630 — untrusted OBJ file robustness**: fed `loadObjMesh()`
  three hostile inputs — an out-of-range negative (relative) vertex
  index, a literal `nan` coordinate, and a `1e400`-overflow-to-infinity
  coordinate. The first two were already handled cleanly (tinyobjloader
  itself rejects the bad index; `nan` parses to `0.0` and never
  propagates). **Found a real gap**: infinite coordinates parsed
  successfully and silently produced a spec-invalid GLB (`inf` floats in
  the binary buffer, `null` — not a number — in the JSON accessor
  `min`/`max`, since JSON has no `Infinity` literal). Fixed by rejecting
  non-finite vertex coordinates in `loadObjMesh()` with a clean
  `std::runtime_error`, routed through the exact same catch-and-skip
  path `buildMesh()` already uses for other malformed-mesh cases (prints
  `Warning:`, node exported meshless, export continues — no crash).
  Added a permanent `mc3togltf_obj_robustness` ctest (2 new `.obj`
  fixtures + 1 new `.mc3.xml` scene under `test/`) covering all 3 inputs
  plus a check that no accessor ever serializes a non-finite value.
- **STAB-0610 — `-Wall -Wextra` on `Mc3` and `mc3togltf_lib`**: added
  the flags to both targets' `CMakeLists.txt`. Found and fixed 3 real
  warnings in our own code: `MeshBuilder.cpp`'s `sampleCrossSection()`
  didn't handle `CrossSectionType::Star` at all (silently produced an
  empty polygon for star cross-sections in glTF export — mirrored the
  existing, correct `SceneRenderer_Extrude.cpp` star-sampling logic to
  fix it); an unused `dy` variable in `samplePath()`; and a
  missing-field-initializers warning on `ExportCtx` aggregate init in
  `GltfExporter.cpp` (silenced with explicit `{}`, no behavior change —
  the omitted members already default-initialize correctly). Also
  marked the vendored `tinygltf`/`tinyobjloader` include dirs `SYSTEM`
  in `mc3togltf/CMakeLists.txt` so their (unfixable, third-party)
  warnings don't pollute the build under the new flags. Verified: a
  full from-scratch Debug reconfigure/rebuild is 100% warning-free for
  `Mc3` and `mc3togltf_lib`, 20/20 ctest still pass, and a fresh
  standalone `mc3togltf` build (outside the root project) is also
  warning-free and 12/12 tests pass.
- **S20 (Release Readiness), 12/15 items**: added a project version
  (`0.1.0`) and `--version` CLI flag; actually built and compared
  Debug vs. Release binaries to confirm Release has no debug symbols;
  ran the real editor binary against all 3 sample scene files to
  confirm clean loading; created `CHANGELOG.md`, `THIRD_PARTY.md`
  (every dependency version verified against the real
  `FetchContent_Declare` tags), `docs/USER_GUIDE.md` (every menu
  path/shortcut verified against the real menu code); added "Reporting
  a Crash" (Linux instructions actually tested with a real SIGSEGV) and
  "Backup and Recovery" sections to `README.md`. **Found a real gap**:
  SVG rasterization wasn't mentioned in `README.md`'s limitations list
  at all — added it. 3 items (STAB-0642/0643/0650) are blocked on tools
  not available in this environment (Blender, a browser, running CI).
- **S18 + S19 P0/P1 verification (18 items, no code changed)**: audited
  `PropertiesPanel.cpp` and `SceneRenderer.cpp` for null-pointer risk
  (none found — every optional/map access is properly guarded);
  confirmed the API key is never stored or logged, no
  `std::system`/`popen` anywhere, SQLite is fully parameterized.
  **Actually tested** (not just read) three hostile inputs against the
  real parser: include path traversal to `/etc/passwd` (clean parse
  error, no crash), a 10MB malformed-XML file (throws in ~7ms), and a
  10MB texture URI (parses correctly in ~53ms, no truncation).
- **A full `mc3.xsd` audit** (not tied to a `STAB-XXXX` ID): diffed
  every `Mc3XmlWriter.cpp` attribute/element against the schema and
  found **7 more real gaps** beyond the `id` gap found earlier
  (`layer`, per-object `<state>` — missing for every object type,
  instance `material_override`/`variants`, `uv_mapping`'s real
  attributes, environment `background_texture`/`skybox_texture`,
  texture `name`) — all fixed. **Also found a genuine round-trip bug**
  while auditing: renaming a texture in the editor was silently
  discarded on every save/reload (`Mc3XmlParser::parseTextures` never
  read the `name` attribute back) — fixed and regression-tested.
  Process note: editing `mc3.xsd` requires a full CMake **reconfigure**,
  not just a rebuild — it's embedded into a generated header at
  configure time (`Mc3XsdEmbed.hpp`), and this was caught the hard way
  via a failing test before it could ship silently wrong.
- **Gate 6's full P1 priority-list (S17, 17 items)**: rewrote
  `STABILIZATION.md` (was describing "15 tests" and already-fixed
  gaps), added N3–N7 sections to `MC3_FORMAT.md` (each with an honest
  "not executed at runtime yet" status note), an MCB Binary Format
  section, an Include semantics section (written from reading the
  actual merge/cycle-detection code), created `TESTING.md` and
  `CONTRIBUTING.md`, reviewed `m1m2m3.md` against current code and
  fixed real drift (missing `source` in the registry search
  description; the `AiAssistant` class snippet was missing prompt
  caching, `maxTokens`, truncation handling, and XSD validation — all
  added after that doc was first written).
- **STAB-0391** (earlier the same day): added real XSD validation of
  AI-generated XML via libxml2, embedded `mc3.xsd` into the binary at
  configure time. Found the object `id` gap during this work — the
  first thread that led to the full audit above.
- **STAB-0243/0244** (S6): confirmed the existing large-scene test
  already covers mesh-reuse correctly; added a 500-object variant of
  the same generator test with a `<30s` timing assertion (measured
  ~0.02s).

Earlier sessions (summarized further): Gate 3's "commands are undoable"
cluster, auto-save + 2-slot backup rotation, AI-panel dialog lifecycle,
registry search fields + edge cases, and 5 other real bugs found via
test-driven development (`resetPivot()` missing an undo push, a
hierarchy-filter bug, unclamped bloom/SSAO sliders, a registry search
gap, and the original AI-response XML-extraction trailing-content bug).

---

## 4. Current blocker / main problem

**There is no blocker to local development or testing.** Debug and
Release both build clean and pass 20/20 tests as of commit `fca6fc1`
(pushed; `develop` is in sync with `origin/develop` at `03723b8`,
confirmed just now).

The one open **operational** issue is **CI cannot be activated with the
current git credentials**:

- Symptom / failing command (only when trying to activate CI, not for
  normal pushes):
  ```
  git push origin develop
  ! [remote rejected] develop -> develop (refusing to allow a Personal
    Access Token to create or update workflow `.github/workflows/ci.yml`
    without `workflow` scope)
  ```
- Cause: the PAT embedded in the remote URL (both this repo and the
  sibling `../cna` repo) lacks the `workflow` OAuth scope.
- Workaround already in place: the CI workflow file is committed but
  parked at `.github_/workflows/ci.yml` (GitHub only treats
  `.github/workflows/` as live), so it's versioned but inactive. To
  activate: rename `.github_` → `.github` and push with a
  `workflow`-scoped token.
- Related, separate issue: that same PAT is embedded in **plaintext** in
  the remote URL in `.git/config` (not in any tracked/committed file).
  Recommended fix: revoke it, issue a new token with `repo` + `workflow`
  scopes, and switch the remote to a credential helper or SSH.
- This also blocks STAB-0650 (verify CI produces a consistent test
  report) — can't be verified without CI actually running.

Nothing has been tried beyond identifying and documenting this; it
requires the repo owner to rotate/rescope the token.

---

## 5. Known bugs and limitations

- **PAT exposed in `.git/config` and lacks `workflow` scope** — see §4.
  _status: confirmed (security + operational); needs owner action._
- **CI is partial** — only the CNA-free libs are covered by the parked
  workflow; no full-editor CI job. _status: incomplete, inactive._
- **SVG texture rasterization** — parsed/serialized/round-tripped, but
  `GltfExporter` never rasterizes the SVG texture map. Since STAB-0440
  this session, a material referencing one now prints a warning naming
  the material/slot/texture id, rather than dropping it silently — but
  the texture itself is still omitted from export. Blocked on a
  library choice (librsvg vs. NanoSVG). _status: incomplete, now
  documented in `README.md`/`MC3_FORMAT.md`, warns instead of silent._
- **`mergeSceneFromFile()`'s texture merge can silently pick the wrong
  texture on a name collision** — unlike its material merge (suffix on
  collision, fixed pattern) and unlike `ModelRegistry::insertIntoScene`
  (suffix + reference-remap, fixed this session per STAB-0425), the
  texture merge in `MeshCraftApplication_FileOps.cpp` still uses
  skip-on-collision (existing wins): if the merged scene's texture
  name collides with an unrelated one already in the target scene, a
  material that should reference the merged scene's own texture ends
  up pointing at the wrong one instead. Found while fixing STAB-0469
  (same file, adjacent code) this session. _status: confirmed, not
  fixed — same fix shape as STAB-0425 (suffix + remap referencing
  materials), scoped out of STAB-0469 to keep that task bounded; no
  assigned STAB-XXXX ID of its own yet._
- **No "scale animation time" feature exists** — `plan.md`'s STAB-0460
  describes a feature (scale all of an action's keyframe times by a
  factor) that was never implemented anywhere in the codebase (confirmed
  by exhaustive grep this session). _status: confirmed missing, not a
  bug — flagged 🟡 in `plan.md`, needs a future feature-planning
  decision, out of scope during the current stabilization-only feature
  moratorium._
- **Embedded glTF** — `embed:id` is treated as a literal OBJ path by
  `GltfExporter`, which fails to parse; export continues with an empty
  node rather than crashing. _status: incomplete, documented._
- **`EditorViewport` not integrated** into `MeshCraftApplication`'s
  render loop. _status: incomplete._
- **`mc3` standalone build skips `mc3_commands`** — needs
  `EditorAlgorithms.hpp` from the editor tree, absent in a standalone
  checkout. _status: intended, not a bug._
- **`cmake-build-debug/` reconfigure is toolchain-sensitive** — must use
  CLion's bundled cmake 4.2.2, not the system cmake (fails on manifold
  sources otherwise). _status: confirmed, environmental._
- **`mc3.xsd` audit closed 7 gaps + 1 texture-rename round-trip bug**
  this session (see §3) — the audit covered `Mc3XmlWriter.cpp`
  exhaustively but not the reverse direction (parser accepting
  something the writer never emits, lower priority since that can't
  cause a false-XSD-reject). _status: resolved for the writer
  direction; parser-only direction unaudited._
- **`mc3.xsd` has no numeric range constraints anywhere** (zero
  `minInclusive`/`minExclusive`) — a schema-valid AI response can
  contain a negative `size`/`radius`/etc. and it applies to the scene
  unchanged. _status: confirmed, documented in `README.md`; not fixed —
  would need a per-attribute design decision, bigger than a doc pass._
- **N3–N7 (scripts/sounds/music/triggers/states/meta) are data-only** —
  round-tripped but nothing executes them at runtime. _status:
  intended at this stage, documented per-section in `MC3_FORMAT.md`._
- **`mc3.xsd`'s `mip_maps` texture attribute has zero implementation**
  — the schema declares `mip_maps` (boolean, default `true`) on
  `<texture>`, but there's no `Mc3Texture` field, no parser read, no
  writer emit, and `GltfExporter` never reads it either. A
  schema-valid `mip_maps="false"` parses fine and silently does
  nothing anywhere. Found while fixing STAB-0417/0418 (texture sampler
  export) this session. _status: confirmed, not fixed — no assigned
  STAB-XXXX ID; would need a `Mc3Texture` field + parser/writer support
  + an exporter decision (e.g. non-mipmapped filter enum when false)._
- **Gate 6 is nearly exhausted for this environment** — S17 20/20 ✅,
  S19 15/15 ✅ (fully green), S18 20/25 (everything reachable done; 4
  tool-blocked on `include-what-you-use`/`clang-tidy`/`cppcheck`, none
  installed here; 1 — `PropertiesPanel.cpp`'s split, STAB-0617 —
  flagged but deferred, needs a human visually verifying the live
  ImGui UI), S20 12/15 (3 items blocked — see below). _status: nothing
  left to pick up in Gate 6 without an external tool or a human with a
  display; work has moved to S11 (Materials/Textures) — see §8._
- **3 `plan.md` items cannot be completed in this environment**:
  STAB-0642 (needs Blender), STAB-0643 (needs a browser), STAB-0650
  (needs CI actually running — see §4). _status: flagged, needs either
  a human with the right tools or the PAT rotated first._

---

## 6. Architecture notes

```
MeshCraft (editor exe)
├── CNA (SDL3/OpenGL runtime — sibling repo at ../cna, DO NOT MODIFY)
├── Mc3 (static lib — mc3/): Mc3Document, XML parser/writer, all types
├── Mcb (static lib — mcb/): McbWriter, McbReader, MCB binary format v1
├── mc3togltf_lib (static lib): GltfExporter, MeshBuilder, CsgEvaluator
├── mc3togltf (CLI exe): thin wrapper around mc3togltf_lib
└── mc3tomcb (CLI exe): mc3.xml <-> mcb converter (links Mc3 + Mcb)
```

**Data flow:** `.mc3.xml` → `Mc3XmlParser` → `Mc3Document` (in-memory
model) → either `Mc3XmlWriter` (save), `McbWriter` (binary), or
`GltfExporter` (glTF/GLB).

**Undo/redo:** snapshot-based. `undoStack_`/`redoStack_` (capped at 20,
oldest dropped first) hold `std::vector<Mc3::Mc3Document>`; `pushUndo()`
stores a deep copy of `document_` before each mutating command.

**The "Alg mirror" pattern**: editor/AI logic in CNA-coupled `.cpp`
files often has zero actual CNA/ImGui dependency once app-state
bookkeeping and rendering are set aside. Two variants:
- **Single source of truth** (preferred): pure logic in a CNA-free
  header (`EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`), functions
  suffixed `Alg`, real `.cpp` `#include`s and calls them directly — no
  duplication.
- **Kept in sync manually**: only when the real function is genuinely
  CNA-coupled and can't be unified — the mirror's comment names what it
  tracks.

**Embedding a resource file at compile time**: `mc3.xsd` is compiled
into `MeshCraft`/`ai_test` as a raw string constant
(`MeshCraft::kMc3XsdContent`), generated at CMake **configure** time
(`file(READ)` + `configure_file()` in root `CMakeLists.txt`, template at
`cmake/Mc3XsdEmbed.hpp.in`) — never read from disk at runtime. **Editing
`mc3.xsd` requires a full reconfigure, not just a rebuild**, or the old
schema stays compiled in silently.

**Testing network-dependent code**: `AiAssistant` has a public
`apiBaseUrl` member (default the real Claude API) that tests override to
point at a local `httplib::Server` mock — production behavior is
untouched. See `mc3/test/ai_test.cpp`.

**Hard constraints / invariants:**
- `Mc3Document` public API: don't change without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` must stay buildable
  standalone (no CNA/ImGui deps).
- **`mc3.xsd` must stay symmetric with `Mc3XmlWriter.cpp`** — this
  session found 8 real gaps that had accumulated silently. Any new
  writer attribute/element needs a schema declaration in the same
  change (see `CONTRIBUTING.md`'s "API change policy").
- Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`
  (triggers a full CNA recompile).
- Do not modify CNA/SHARP_RUNTIME source files from this repo.
- `file(GLOB_RECURSE)` collects sources — a new `.cpp` file needs a
  cmake **reconfigure**, not just a rebuild (same for `mc3.xsd`).
- MCB format version is `MCB_VERSION` in `McbFormat.hpp` — bump on any
  breaking wire-format change.
- XSD root element order is strict (`include → metadata → meta →
  environment → ... → objects → actions`) — this now has real runtime
  consequence since AI responses are validated against it.
- Reconfigure `cmake-build-debug/` only with CLion's bundled cmake.

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid a system-cmake bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON   # (re)configure
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (20)
ctest -N                                                      # lists all 20 tests

# --- Release (system cmake is fine for a fresh dir)
cmake -S . -B b-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
      -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build b-release -j4
(cd b-release && ctest --output-on-failure)

# --- Standalone (CNA-free) component builds + tests
for c in mc3 mcb mc3togltf mc3tomcb; do
  cmake -S "$c" -B "$c-build" -G Ninja -DBUILD_TESTING=ON \
        -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
  cmake --build "$c-build" -j4
  (cd "$c-build" && ctest --output-on-failure)
done   # mc3 1/1 · mcb 1/1 · mc3togltf 18/18 · mc3tomcb 2/2

# --- Run / export / validate / version
./cmake-build-debug/MeshCraft test/house.mc3.xml   # open a sample scene
./cmake-build-debug/MeshCraft --version             # MeshCraft 0.1.0
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
python3 test/validate_xsd.py mc3/mc3.xsd test/features.mc3.xml
ctest -R mc3_commands --output-on-failure   # editor algorithms + undo/redo
ctest -R mc3_registry --output-on-failure   # ModelRegistry
ctest -R mc3_ai       --output-on-failure   # AiAssistant + mock HTTP server

# --- Push (normal pushes work fine; only CI activation is blocked, see §4)
git push origin develop
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

**Gate 6 is now essentially exhausted for this environment.** S17
20/20, S19 15/15 (both fully green), S18 20/25 (all reachable items
done; the other 5 are either tool-blocked — STAB-0609/0614/0615/0620,
need `include-what-you-use`/`clang-tidy`/`cppcheck`, none installed
here — or STAB-0617, genuinely flagged 🟡 and deferred since it needs a
human visually verifying a live ImGui panel after a split, which this
headless environment can't do), S20 12/15 (3 blocked on
Blender/browser/CI). **There is nothing left to pick up in Gate 6
without an external tool or a human with a display.**

The next priority tier is P2 items across S6–S13 and the untouched
S14/S15 sections. **S11 (Materials, Textures, and Visual Fidelity) is
done** — 25/30, remaining 5 all genuinely blocked (STAB-0421/0422/0423
material preview sphere + STAB-0428 texture drag-and-drop need a live
display, same as STAB-0617; STAB-0431 needs Blender, same as the S20
items). **S12 (Animation Stability) is done** — 28/30, remaining 2
flagged (STAB-0460 a never-built feature, out of scope per the
stabilization moratorium; STAB-0464 needs a live display).

**S13 (Commands, Undo/Redo, and Algorithms) is fully done — all 25
items ✅** (see the "Recent changes" summary above and §3 for the full
list of extractions/bugs found this session). **S14 (Rendering and
Viewport Stability), 30 items, is underway** — STAB-0496 done (verified
`smoke_test` exercises a genuine render across all 3 sample scenes),
STAB-0497 done (confirmed the renderer handles all 19 `ObjectType`
values by construction; added `smoke_test_all_objects` ctest), and
STAB-0498 done (added `test/empty_scene.mc3.xml` +
`smoke_test_empty_scene` ctest). Next:

1. **STAB-0499 — verify renderer handles missing mesh file
   gracefully** (S14, P1, next-lowest ID). Goal: a `<mesh>` object with
   a nonexistent `src` renders a placeholder (or nothing), not a crash.
   Files: `src/MeshCraft/Renderer/SceneRenderer.cpp`.
   Verify: `SceneRenderer.cpp`'s `ObjectType::Mesh` case already showed
   (during STAB-0497's code reading) a fallback to `unitBox_` if
   `loadOrGetMesh()` returns null — confirm this via a real
   `--screenshot` run against a fixture with a bad mesh path, following
   the same pattern as STAB-0498 (create a small fixture + permanent
   ctest if none exists).

STAB-0500 (missing material reference, same "no crash, sensible
fallback" shape) is likely headlessly verifiable the same way. Beyond
that, S14 gets heavily visual/interactive (gizmos, camera presets,
bloom/SSAO toggles, shadow maps) — expect many items to land like
S11/S12's blocked set (flagged 🟡, needs a live display) rather than
S13's extract-and-test pattern. S15 (Import/Export/Editor Integration)
remains untouched after
S14.

---

## 9. Do not do yet

- **No new scene-format features** — N1–N7 are complete; further schema
  additions need design discussion first.
- **No CNA/SHARP_RUNTIME source changes** — separate repos, out of
  scope for this one.
- **No `Mc3Document` public API changes** without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- **No `${meta-gl_SOURCE_DIR}/include`** in any `CMakeLists.txt`.
- **No reconfiguring `cmake-build-debug/` with system cmake** — use
  CLion's bundled cmake.
- **No moving `.github_` back to `.github`** until a `workflow`-scoped
  token exists.
- **No committing the PAT** anywhere — remove it from `.git/config`,
  don't copy it into any tracked file.
- **No SVG rasterization work** until a library choice is made (librsvg
  vs. NanoSVG) — this is a real, nontrivial feature decision, not a
  quick fix.
- **No mass refactoring** of passing code, no speculative architecture
  changes — stabilization phase; scope each change to exactly what its
  `STAB-XXXX` entry asks for.
- **No typing any `mc3.xsd` attribute as `xs:ID`/`xs:IDREF`** without
  first checking whether the app actually enforces that referential
  constraint — several existing attributes are deliberately `xs:string`
  instead, to avoid inventing document-wide uniqueness rules the app
  doesn't uphold.
- **No attempting STAB-0642/0643/0650** without the missing tool first
  (Blender, a browser, or an active CI run respectively) — see §5.

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect only the files needed for the first
task in section 8. Do not refactor unrelated code. Make one small,
verified improvement. Run the relevant build/test command from section 7
and confirm cmake-build-debug still passes (20/20, or the new total if
you registered a new ctest). Update NEXT.md after finishing.

Current branch: develop, in sync with origin/develop at commit 03723b8.
Build dirs: cmake-build-debug/ (Debug, CLion cmake 4.2.2) and b-release/
(Release) — both full-rebuilt and 20/20 ctest verified clean at commit
fca6fc1. Standalone mc3/mcb/mc3togltf/mc3tomcb builds also re-verified.
Active plan: plan.md (STAB-XXXX tasks; every gate's priority-list subset
is done; Gate 6 is close to fully green — S17 20/20, S18 7/25 (P0/P1
done), S19 11/15 (P0/P1+bonus done), S20 12/15 (3 items blocked on
missing tools). Pick the next task from section 8: S18/S19's remaining
P2/P3 items would fully close Gate 6 except the 3 blocked S20 items.
Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file requires a
reconfigure, not just a rebuild — see section 6.
CI is parked deactivated under .github_/ (PAT lacks `workflow` scope,
see section 4).
```
