# NEXT.md

_Last updated: 2026-07-05 (STAB-0548/0549, prior commit `0f36ab1`, pushed)_

---

## 1. Project summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` scene format —
a lightweight XML-based scene description used by the OpenEggbert project.
It uses Dear ImGui for its UI, running on **CNA** (an XNA-style SDL3 +
OpenGL runtime, a separate sibling repo at `../cna` — never modified from
this repo) and **SHARP_RUNTIME** (`../sharp-runtime`, the math library
CNA's backend depends on). Scenes export to glTF/GLB via **mc3togltf** and
to a compact binary format via **mc3tomcb**.

**Main goal:** reach a fully stabilized, test-covered codebase before
adding new features. All work is tracked in `plan.md` as 650 `STAB-XXXX`
tasks across sections S0–S20, gated by a Gate 0–6 checklist.

**Current phase:** Stabilization. Gates 0–5 are fully closed. **Gate 6
(Documentation)** is exhausted for this environment — everything reachable
without an external tool or a live display is done. Sections S0–S14 are
fully closed (bar a handful of genuinely blocked/flagged items — S14: 17
done, 13 flagged 🟡, 0 remaining). **S15 (Import/Export/Editor
Integration), 30 items, is in progress**: 22 done (STAB-0526–0531,
0533–0538, 0540–0549), 2 flagged 🟡 (STAB-0532, 0539), 6 not yet
started.

**Important architectural decisions:**
- `mc3/` and `mcb/` are pure C++ static libs with **no** CNA/ImGui
  dependency and must stay independently buildable.
- `mc3togltf/` and `mc3tomcb/` are standalone CLI + lib targets, also
  CNA-free.
- CNA is added as a sibling CMake subdirectory
  (`add_subdirectory(../cna ...)`) and must not be modified from this
  repo.
- The editor (`src/MeshCraft/`) is CNA-coupled, but much of its
  command/parsing/validation/render-math logic has zero actual CNA
  dependency once ImGui rendering and app-state bookkeeping are set
  aside — that pure logic lives in CNA-free headers (functions suffixed
  `Alg`, e.g. `EditorAlgorithms.hpp`) that both the real app code and the
  headless test suite `#include` and call directly. See §6.

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`, generated with CLion's bundled cmake
  4.2.2): full reconfigure + rebuild succeeds cleanly, 0 warnings from
  MeshCraft's own sources, as of STAB-0513. **New as of this update:**
  the reconfigure now requires `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` — the
  sibling CNA repo added a vendored ENet dependency
  (`../cna/third_party/enet/CMakeLists.txt`) whose `cmake_minimum_required`
  is too old for CMake 4.2.2 (CMake ≥4 removed compat with policy versions
  <3.5). This is a CNA-side issue (out of scope for this repo, see
  CLAUDE.md); the flag is a harmless top-level workaround, not a CNA file
  change. See §7 for the updated reconfigure command and §4 for details.
- **Release** (`b-release/`): directory exists from an earlier point in
  this session's history; **not re-verified as part of this update** —
  re-run the commands in §7 before relying on it.
- **Standalone (CNA-free) component builds** (each configures/builds/
  tests independently of the root project): `mc3`, `mcb`, `mc3togltf`,
  `mc3tomcb` — last confirmed passing earlier this session; re-verify
  with §7's commands if you depend on this.

### Tests
**46/46 CTest pass** in Debug (`ctest -N` lists all 46 by name). Notably:
- `mc3_commands` (~450 assertions): editor command algorithms, undo/redo,
  auto-save/backup, keybinding/macro persistence, hierarchy filtering,
  AI-panel lifecycle, viewport ray-cast picking, click-selection
  resolution, camera view presets.
- `mc3_registry` (~94 assertions): ModelRegistry SQLite CRUD/search.
- `mc3_ai` (~55 assertions): AiAssistant JSON + AI-response validation
  pipeline, mock-HTTP-server round-trips.
- `mc3_roundtrip` (~299 assertions): full XML parser/writer roundtrip.
- `mcb_roundtrip` (~50 assertions): MCB binary roundtrip.
- Several real-render smoke tests (`smoke_test*`, `fog_linear_test`,
  `point_light_gizmo_test`, `spot_light_gizmo_test`,
  `look_through_camera_test`, `csg_cache_test`,
  `background_texture_test`, `skybox_texture_test`, `lod_test`) that run
  the actual `MeshCraft` binary in `--screenshot` headless mode and
  verify genuine pixel output (or, for `csg_cache_test`/`lod_test`, a
  printed internal counter), not a stub render.
- `editor_export_test`: exercises the editor's own export codepath (not
  just the standalone `mc3togltf` CLI) via a new `--export <path>` flag
  on the `MeshCraft` binary — GLB magic bytes, glTF JSON validity,
  invalid-extension error + non-zero exit, and the exporter stats
  message.
- A dozen `mc3togltf_*` tests covering CSG, materials, textures,
  animation, instance variants, large scenes.

See `TESTING.md` for the full per-test reference.

### Tools / libraries available
- `MeshCraft` — editor executable, version `0.1.0` (`--version` to print
  it). Builds and runs; the 3D viewport is not fully integrated into the
  render loop in interactive mode (see "What does NOT work yet"). New
  this session: `--export <path>` (new, STAB-0526) exports the loaded
  scene to `.glb`/`.gltf` and exits non-interactively (same codepath as
  the editor's File → Export menu), alongside the existing
  `--screenshot <path>`.
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb`.
- `mc3tomcb` — bidirectional CLI: `mc3.xml` ↔ `.mcb`.
- `Mc3` / `Mcb` / `mc3togltf_lib` — static libs (scene data, binary
  serialization, glTF export).

### What works
- Full XML round-trip for all primitive types and all N1–N7 extensions.
- MCB binary round-trip, matching the XML feature set.
- XSD validation for every test fixture and for AI-generated scene XML
  before it's applied to the live scene.
- glTF/GLB export: primitives, animations, CSG, materials, lights,
  cameras, instances, groups, OBJ mesh import.
- SQLite-backed asset registry: open/save/search/remove/migration.
- Editor undo/redo (snapshot-based, capped at 20).
- Auto-save + 2-slot rotating backup on save.
- AI Assistant: sends scene + prompt to the Claude API, validates the
  response, applies it or saves definitions to the registry.
- Viewport ray-cast click-to-select works for **every** object type
  (fixed this session — see §3), not just primitives.
- Real headless rendering verified via `--screenshot`: sample scenes,
  missing-mesh/missing-material fallbacks, orthographic camera, linear
  fog, point-light gizmo, spot-light gizmo, look-through-camera override,
  background texture, **equirectangular skybox (fixed this session — was
  completely invisible before, see §3)** — all confirmed to produce
  genuine, non-stub pixel output.

### What does NOT work yet
- `EditorViewport` is not integrated into the `MeshCraftApplication`
  render loop.
- SVG texture rasterization: parsed/serialized/round-tripped, but
  `GltfExporter` never reads the SVG texture map — silently dropped from
  export (a warning is printed, at least).
- Embedded glTF (`<mesh src="embed:id"/>`): parsed/serialized, but
  `GltfExporter` treats `embed:id` as a literal OBJ path, which fails —
  export continues with an empty (meshless) node, doesn't crash.
- N3–N7 scene data (scripts, sounds, music, triggers, scene states,
  meta): fully round-tripped but not executed at runtime anywhere.
- CI workflow exists but is parked deactivated under `.github_/` (see
  §4); no automated full-editor (CNA + SDL3) build/test job runs
  anywhere currently.
- A cluster of visual toggles (bounding-box overlay, SSAO, bloom,
  wireframe mode, translate/rotate gizmos) are confirmed correct by code
  reading but can't be re-verified fresh in this headless environment —
  see §5.

---

## 3. Recent changes

- **STAB-0548/0549** — STAB-0548 confirmed correct by tracing `Instance`
  resolution (`GltfExporter.cpp:552-587`): definitions from `<include>`
  and the scene's own `<definitions>` are unified into `doc.definitions`
  at parse time, so no special-casing is needed at export time.
  Manually confirmed via `--stats`: 0 warnings, real geometry, and the
  two `pillar` instances correctly sharing one glTF mesh. Previously
  untested by `mc3togltf`'s own suite (only XML round-tripping was
  covered). **STAB-0549 confirmed as a documented, deliberate
  limitation, matching the row's own anticipated resolution** ("marked
  as limitation") — `buildMesh()` treats `embed:<id>` as a literal OBJ
  path with no special-case, so it fails to open, warns, and produces
  an empty node; actually resolving it would mean parsing external
  GLB/base64 data, a real new feature not attempted here. Locked in the
  safe degradation (exit 0, warning, empty node — not a crash) with a
  regression test. Added `mc3togltf/test/include_export_test.py` +
  `test/embed_mesh_source.mc3.xml` + `mc3togltf/test/embed_mesh_source_test.py`
  + 2 permanent ctests. 46/46 ctest passing (up from 44/44).
- **STAB-0546/0547** — STAB-0546 confirmed correct by tracing
  `GltfExporter::exportDocument()`: the whole `tinygltf::Model` is
  built fully in memory and `WriteGltfSceneToFile()` runs exactly
  once, as the last step, so no partial output file is ever left
  behind on either a malformed-input or write-protected-output
  failure (manually confirmed both). **STAB-0547 found and fixed a
  real gap directly contradicting its own verification method**:
  `mc3togltf --help` had no handling at all — it was parsed as a
  *positional input filename* that doesn't exist, producing "Error:
  input file not found: --help" and exit 1, never showing usage text.
  Fixed by recognizing `--help`/`-h` before the positional-argument
  fallback (matching the `MeshCraft` editor's own convention). Added
  `mc3togltf/test/no_partial_output_test.py` +
  `mc3togltf/test/help_text_test.py` + 2 permanent ctests. 44/44 ctest
  passing (up from 42/42).
- **STAB-0544/0545** — **found and fixed a real bug, the 3rd instance
  of this session's "export drops textures" pattern** (after
  STAB-0536): the Material Export dialog copied only the material
  itself into the exported `.mc3mat.xml` — no referenced textures —
  and the Import dialog never looked at a file's `<textures>` section
  at all, so even a fixed export would still lose its texture on
  reimport. Fixed both together: added `exportMaterialAlg()` (same
  texture-collection pattern as `exportSelectionAlg`/
  `exportSubtreeTemplateAlg`) and `importMaterialsAlg()` (imports
  referenced textures, suffixing + re-pointing the material's
  reference field on a genuine collision, reusing an existing texture
  only if its `uri` actually matches). Found the pre-existing
  material-suffix logic was already correct, but **texture-reference
  remapping on collision was the real STAB-0545 gap** — without it, an
  imported material could silently end up pointing at a pre-existing,
  unrelated texture. Added 22 new assertions across 4 test functions.
  42/42 ctest passing (same count, more assertions inside the existing
  `mc3_commands` ctest).
- **STAB-0543** — no "Embed textures" setting exists anywhere (image
  embedding is fully automatic, tied to output format:
  `GltfExporter.cpp:1109-1111`, `embedImagesNow = format == GLB`), but
  the underlying concern — does GLB actually embed real texture pixel
  data? — was genuinely untested (the existing `texture_sampler_test.py`
  deliberately uses nonexistent texture files to test the
  missing-texture-warning path). **Also found the row's own
  verification method was wrong**: GLB embedding here isn't via
  `bufferView`s — tinygltf's `embedImages=true` path encodes images as
  base64 `data:image/png;base64,...` URIs instead (my own first test
  attempt assumed `bufferView` and failed against real output before I
  checked). Added `test/glb_texture_embed.mc3.xml` (references the
  real, committed `textures/solid_green.png`) +
  `mc3togltf/test/glb_texture_embed_test.py` (decodes the embedded
  base64 data URI, confirms byte-identical to the source PNG) + a
  permanent `mc3togltf_glb_texture_embed` ctest. 42/42 ctest passing
  (up from 41/41).
- **STAB-0540/0541** — both already fully covered, no new work needed.
  The editor's `saveFile()`/`saveFileAs()` call `document_.saveToFile()`
  directly — the exact function `mc3/test/roundtrip_test.cpp`'s
  `testInclude()` (`<include>` paths + definitions/materials survive a
  real save-to-file + reload cycle, not inlined) and `testEmbedGltf()`
  (external-GLB and inline-base64 embeds, plus `meshSource="embed:<id>"`
  linkage, via the shared `roundtrip()` helper — a genuine
  `saveToFile()`/`loadFromFile()` round-trip through a real file, same
  calls the editor's Save/Open commands use) already exercise.
- **STAB-0542** — confirmed the row's expectation doesn't match
  long-standing, deliberate behavior: `--screenshot out.png` always
  writes raw PPM (manually verified: `b'P6\n800 480\n255\n...'` header),
  never real PNG — `stb_image_write.h` exists only inside tinygltf's
  fetched deps, never linked into the `MeshCraft` editor target. This
  is the exact mechanism **8 existing `test/*.py` scripts** (all of
  S14/S15's new pixel-sampling tests plus pre-existing ones) already
  rely on; implementing real PNG would mean rewriting all of them and
  is a genuine new-feature decision, not attempted without an explicit
  go-ahead. Marked ✅ (the screenshot mechanism itself produces a
  genuine valid image, extensively proven this session) — documented
  the PNG/PPM naming mismatch in `plan.md` for whoever revisits it.
- **STAB-0539** — traced editor drag-drop's full path (SDL
  `SDL_EVENT_DROP_FILE` watcher → `pendingDropFile_` → `Update()`).
  **Found and fixed an unwired duplicate**: the real drop-consumption
  code (`MeshCraftApplication.cpp:360`) duplicated
  `isDroppableScenePathAlg()`'s exact logic inline instead of calling
  it — rewired it to call the Alg function directly (needed a new
  `#include`). Routing logic confirmed correct, already covered
  headlessly by `testDroppableScenePathDetection`. The actual SDL drop
  *event* delivery itself needs a live drag-and-drop gesture. 41/41
  ctest passing (no regressions). Flagged 🟡.
- **STAB-0537/0538** — confirmed `mc3tomcb`'s existing catch-all
  (`main.cpp:28-47`) already handles both a missing input file and a
  write-protected output path correctly (exit 1, clear stderr message,
  no output file) — no code change needed, just previously-missing
  test coverage (only the success round-trip was tested before). Added
  `mc3tomcb/test/mc3tomcb_error_test.py` (uses a temp dir with its
  write bit stripped rather than a hardcoded `/root/...` path, so it
  doesn't depend on the runner's privilege level) + a permanent
  `mc3tomcb_error_handling` ctest. 41/41 ctest passing (up from 40/40).
- **STAB-0536** — **found and fixed a real bug**: `exportSubtreeAsTemplate()`'s
  file-save path (`MeshCraftApplication_Commands.cpp:531-536`) wrote
  only the definition object — no materials/textures it referenced —
  so a template exported to its own standalone file (the exact shape
  used for `<include>` libraries, e.g. `test/mc3_library.mc3.xml`)
  silently lost its appearance (fell back to default gray, STAB-0500)
  when loaded elsewhere. XSD structural validity was never actually at
  risk (a dangling material reference is still schema-valid) — this
  was a real correctness bug the row's literal ask wouldn't have
  caught. Added `exportSubtreeTemplateAlg()` (mirrors
  `exportSelectionAlg`'s material/texture collection, targets
  `.definitions` instead of `.objects`) and wired it into the real
  method. Added a 9-assertion test including a full save+reload
  round-trip through the real XML writer/parser. 40/40 ctest passing.
- **STAB-0534/0535** — **found a real stale-mirror gap**: the
  pre-existing `mergeDocumentsAlg()` (`EditorAlgorithms.hpp:1120-1163`,
  a CNA-free mirror of `mergeSceneFromFile()` used by
  `testMergeSceneCollisionHandling`) predates STAB-0469, which added
  action-merging with suffix-on-collision to the *real* method — the
  mirror was never updated and silently merged textures/materials/
  objects only, dropping `actions` from the test's view entirely (the
  real method was still correct; only the test mirror had drifted).
  Fixed `mergeDocumentsAlg()` to also merge `actions` with the same
  suffix logic as materials, and added 6 new assertions mirroring the
  existing material-collision test shape. Object-append (STAB-0534) was
  already fully covered. Pure `mc3`-lib test, CNA-free. 40/40 ctest
  passing (same count, more assertions inside the existing test).
- **STAB-0532** — traced the OBJ-import Browse button's full path:
  `PropertiesPanel.cpp:1258-1268` → `MeshCraftApplication_UiProperties.cpp:47-52`
  → the "Mesh Source" popup (`MeshCraftApplication_UiOverlays.cpp:1906-1937`,
  F2). Confirmed it's a plain ImGui `InputText` popup (no native OS file
  dialog) that assigns straight to `meshSource` — the same field
  STAB-0533's round-trip test already confirmed persists correctly. No
  unique logic beyond that; genuinely needs a live UI session to click
  through the popup itself. Flagged 🟡.
- **STAB-0533** — **found a real coverage gap**: the existing
  `meshSource` round-trip test (`mc3/test/roundtrip_test.cpp:1200-1218`)
  only exercised the `"embed:<id>"` special case, never a plain
  imported OBJ file path (the actual common case this row is about).
  Added a new assertion pair (`"models/chair.obj"` → round-trip →
  confirm unchanged) — pure `mc3`-lib test, CNA-free, extends the
  existing `mc3_roundtrip` ctest rather than adding a new fixture.
  40/40 ctest passing (same count — more assertions inside the same
  test).
- **STAB-0526/0527/0528/0529** — first S15 items. The editor's File →
  Export dialog (`exportGltf()`/`runGltfExport()`,
  `MeshCraftApplication_FileOps.cpp`) is menu/dialog-driven; confirmed
  `runGltfExport()` is structurally identical to the standalone
  `mc3togltf` CLI's own `main.cpp` (same `outputFormatFromPath()` →
  `GltfExporter::exportDocument()` sequence, already covered by a dozen
  `mc3togltf_*` ctest entries) — only the editor-specific glue was
  untested. With user go-ahead, added a genuine, permanent `--export
  <path>` CLI flag to the `MeshCraft` editor binary (new 3-arg
  `MeshCraftApplication` constructor, 2-frame countdown since export
  needs no rendering warm-up unlike `--screenshot`'s 120) that runs the
  same `runGltfExport()` codepath a menu click would, then exits — also
  usable for real scripted/batch export, not just testing. Added
  `exportFailed()` so `main()` can return a non-zero exit code on
  export failure (STAB-0528), and a `std::cout` print of the exporter
  stats message alongside the existing `setStatusMsg()` call
  (STAB-0529). Added `test/editor_export_test.py` (all 4 rows in one
  script, reusing `test/house.mc3.xml`) + a permanent
  `editor_export_test` ctest. 40/40 ctest passing (also confirmed
  `--screenshot` mode unaffected).
- **STAB-0525** — verified the LOD tier (`SceneRenderer.cpp:633-639`,
  G8) actually changes with camera distance, closing out S14. No
  accessor existed to observe which tier was used, so added
  `lodLevelMap_`/`lastLodLevel(objId)` (same pattern as
  `csgCachedTriCount()`/K4 and STAB-0522's `csgCacheEvaluationCount()`),
  printed as `[LOD] level=N` at the end of `--screenshot` mode. Added
  `test/lod_near.mc3.xml` (sphere 3 units away, expect tier 0) +
  `test/lod_far.mc3.xml` (same sphere 60 units away, expect tier 2) +
  `test/lod_test.py` (runs both scenes, asserts the exact expected tier
  for each, not just that levels differ) + a permanent `lod_test` ctest.
  Manually confirmed level 0 near / level 2 far. 39/39 ctest passing.
  **S14 (Rendering and Viewport Stability) is now fully closed**: 17
  done, 13 flagged 🟡 (need a live display/tool), 0 remaining.
- **STAB-0524** — **found and fixed a real, previously-invisible bug**:
  the equirectangular skybox never rendered anything, despite every
  surface-level check (shader compiles/links, texture loads, correct
  GL state, zero `glGetError()`, correct viewport/scissor) passing —
  confirmed via a temporarily hardcoded solid-color fragment shader that
  *still* produced zero visible pixels. Root cause: `drawSkybox()` built
  and bound a dedicated `quadVAO`/`quadVBO` with an `a_pos` vertex
  attribute, but that VAO-based path silently produces a degenerate,
  invisible draw in this environment (OpenGL ES 3.2 via Mesa) — while
  the bloom passes' `gl_VertexID`-based procedural quad generation (no
  VAO needed at all) is confirmed working. Fixed by switching
  `kSkyboxVS` to the same `gl_VertexID` pattern, binding VAO 0, and
  removing the now-dead VAO/VBO construction from `initSkybox()`. Also
  fixed a related bug in the same function: `drawSkybox()` always used
  the *default editor camera's* FOV (60°) instead of the actual active
  camera's FOV in look-through-camera/walk mode — added an
  `effectiveFovDegrees` local tracking the real active FOV. Added
  `test/skybox_texture.mc3.xml` + a new committed test asset
  `test/textures/solid_blue_equirect.png` + `test/skybox_texture_test.py`
  (pixel sampling in a safe viewport-interior rectangle) + a permanent
  `skybox_texture_test` ctest. Manually confirmed 96% blue coverage after
  the fix vs. 0% before. 38/38 ctest passing.
- **STAB-0523** — verified `background_texture` renders unconditionally
  (document-driven, no UI toggle), stretched to fill the viewport,
  before the 3D scene (`MeshCraftApplication.cpp:436-457`). Added
  `test/background_texture.mc3.xml` (a solid green background behind a
  red box) + a new committed test asset `test/textures/solid_green.png`
  + `test/background_texture_test.py` (real pixel sampling: asserts both
  the green background cluster and the red foreground box cluster are
  present, proving correct draw order) + a permanent
  `background_texture_test` ctest. Manually confirmed 21,774 green +
  7,084 red samples. 37/37 ctest passing.
- **STAB-0522** — verified the content-hash CSG cache (K1,
  `SceneRenderer.cpp:756-778`) actually prevents re-evaluating a static
  CSG object across repeated draws. No headless way existed to observe
  hit/miss behavior, so added a small counter
  (`csgCacheEvaluations_`/`csgCacheEvaluationCount()`, same pattern as
  the existing `csgCachedTriCount()`/K4) incremented only on a cache
  miss, printed as `[CsgCache] evaluations: N` at the end of
  `--screenshot` mode. Added `test/csg_cache.mc3.xml` (one static CSG
  union) + `test/csg_cache_test.py` (parses stdout, asserts the count
  stays at 1–3 across the ~120-frame screenshot warm-up loop rather than
  climbing toward ~120) + a permanent `csg_cache_test` ctest. Manually
  confirmed exactly 1 evaluation. 36/36 ctest passing.
- **STAB-0521** — audited every FBO-based render pass and GL state
  toggle in `MeshCraftApplication.cpp` (bloom, SSAO, shadow-map debug,
  material preview, main scene draw): all framebuffer binds properly
  restore to `0`, all `Enable`/`Disable` state toggles are consistently
  paired — matches the previously-fixed bloom leak (STAB-0509), no new
  leak found. No hook exists to reliably query `glGetError()` after a
  full frame (GL function pointers are only loaded lazily per-subsystem)
  and a GL error is a weak proxy for state leakage anyway (invalid state
  produces no error, just wrong pixels — the pixel-sampling tests added
  this session already exercise the real render path). Flagged 🟡
  pending a live GL-debugger session.
- **STAB-0520** — confirmed `test/large_scene.mc3.xml` exists; the stats
  overlay's `displayFps_` is an exponentially-smoothed rolling average
  across many frames of a sustained interactive loop, conceptually
  incompatible with the single-frame `--screenshot` model. Needs a human
  running the editor interactively. Flagged 🟡.
- **STAB-0519** — **found and fixed a real gap**: proportional editing's
  falloff radius had no viewport visual indicator anywhere (only a
  toolbar slider/tooltip showed the number), even though the row's own
  success criterion calls for a "radius sphere drawn in viewport" and
  the falloff math itself (`applyProportionalFalloffAlg`) was already
  correct and tested (STAB-0489). Added `SceneRenderer::drawWireSphereAt()`
  (reuses the existing `wireShapeSphere_` geometry) and wired it into
  `MeshCraftApplication.cpp`'s draw loop: an orange wireframe sphere
  centered on the selection's average position (same center the falloff
  algorithm uses) with radius `propEditRadius_`, shown continuously
  whenever proportional editing is enabled with a selection — not just
  mid-drag. 35/35 ctest still passing (no regressions). Like
  STAB-0508/0509/0510, `propEditEnabled_` has no CLI/document/prefs
  hook, so the new indicator can't be pixel-tested headlessly — flagged
  🟡 for a live-display confirmation.
- **STAB-0518** — audited the locked-object outline (red wireframe,
  `MeshCraftApplication.cpp:582-595`): correctly recurses through all
  objects/children, draws the outline for every locked id — no bug
  found. `lockedIds_` is a pure in-memory editor set with no persisted
  `mc3.xml` attribute (confirmed: no `locked` attribute anywhere in
  `mc3.xsd`/`Mc3Document`), only settable via a live keyboard shortcut or
  menu action. Same no-headless-hook wall as STAB-0505/0514/0515/0516.
  Flagged 🟡.
- **STAB-0517** — verified the "look through camera" override with a
  real pixel-sampling test (`test/look_through_camera.mc3.xml` +
  `look_through_camera_test.py`, 34,645 yellow samples confirmed): a box
  placed entirely outside the default editor camera's fixed default view
  is only visible if the render truly switches to the scene's own
  `default_camera` (the same `--screenshot`-mode auto-activation
  mechanism STAB-0507 already exercises, but with a genuine pixel
  assertion this time instead of just a non-empty-PPM smoke check).
- **STAB-0516** — audited the shadow-map debug overlay
  (`MeshCraftApplication.cpp:505-524,1535-1551`,
  `MeshCraftApplication_UiOverlays.cpp:2077-2104`): correctly finds the
  first shadow-casting directional light, renders the scene into a
  dedicated FBO from light-space, restores framebuffer/viewport state
  afterward (no leak), and displays it in a "Shadow Frustum" window — no
  bug found. Gated on `shadowDebugEnabled_` (default `false`, menu-item
  only), same no-headless-hook wall as STAB-0508/0509/0510. Flagged 🟡.
- **STAB-0515** — audited the per-selected-object poly-stats display:
  `plan.md`'s file citation was imprecise (the real feature is
  `PropertiesPanel.cpp:1413-1422`'s "Poly stats (C6)", not the
  document-wide stats overlay in `MeshCraftApplication_UiOverlays.cpp`).
  `sel0` is captured fresh every frame with no caching, so it's
  structurally guaranteed to reflect the current selection — no bug
  possible. Confirmed no CLI/document way to pre-select an object at
  load, so — like STAB-0505/0514 — a live mouse click or menu action is
  required to re-verify visually. Flagged 🟡.
- **STAB-0514** — audited the gizmo-drag delta overlay
  (`MeshCraftApplication_UiOverlays.cpp:95-155`): confirmed correct —
  `gizmoDragAxisIdx_`/`gizmoDragStartVal_` are always set together with
  `gizmo_.startDrag()` at all 3 drag-start sites in
  `MeshCraftApplication_Mouse.cpp`, no stale-read bug possible. Needs a
  live mouse-down-drag gesture over a gizmo handle to visually re-verify
  (same class as STAB-0501/0502) — no headless hook, flagged 🟡.
- **STAB-0513** — verified the spot-light gizmo (small sphere at the apex
  + an 8-segment cone along its direction) with a real pixel-sampling
  test (`test/spot_light_gizmo.mc3.xml` + `spot_light_gizmo_test.py`,
  522 cyan-gizmo samples confirmed); same unconditional draw path as
  STAB-0512. Also discovered (not fixed, out of scope): the sibling CNA
  repo now needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` to reconfigure —
  see §4.

Prior 14 commits (`27bef12` through `b0c56f6`), all part of the S14
(Rendering and Viewport Stability) sweep:

- **STAB-0512** — verified the point-light gizmo (sphere + rays) with a
  real pixel-sampling test (`test/point_light_gizmo.mc3.xml` +
  `point_light_gizmo_test.py`); confirmed `drawLightGizmos()` is called
  unconditionally in the main draw loop, not selection-gated.
- **STAB-0511** — verified linear fog visualization: unlike the runtime
  toggles below, fog is a document-level `<environment><fog>` setting
  applied unconditionally. Added `test/fog_linear.mc3.xml` +
  `fog_linear_test.py` (asserts a fully-fogged object renders
  fog-colored, not its authored base color).
- **STAB-0510** — audited `drawObjectEdges()` (wireframe mode): correctly
  handles all 19 `ObjectType` values, no bug found. Flagged 🟡 —
  `showWireframeMode_` has no headless activation hook.
- **STAB-0509** — bloom toggle flagged 🟡 (no headless hook); confirmed a
  prior real fix (VAO/VBO silent-failure + CNA GL-state leakage) is still
  intact in the current code.
- **STAB-0508** — SSAO toggle flagged 🟡; `applySsao()` confirmed correct
  and safe, but `ssaoEnabled_` has no headless activation hook.
- **STAB-0507** — verified the ortho/perspective toggle via the shared
  `CreateOrthographic`/`CreatePerspectiveFieldOfView` mechanism (added
  `test/orthographic_camera.mc3.xml` + `smoke_test_orthographic_camera`).
- **STAB-0506** — extracted camera view presets (`cameraPresetsAlg()` +
  `cameraOrbitPositionAlg()`) into `EditorAlgorithms.hpp`; `EditorCamera::
  position()` now delegates to it. 15 new headless assertions.
- **STAB-0505** — selected-object bounding-box overlay flagged 🟡 (needs
  live selection).
- **STAB-0504** — extracted `resolveClickSelectionAlg()`; confirmed
  click-to-deselect was already correct. 4 new headless assertions.
- **STAB-0503** — **found and fixed a real bug**: viewport click-to-select
  only ray-tested objects with `obj->primitive` set (11 primitive types),
  silently making every Instance/Mesh/Group/Extrude/CSG object unclickable
  in the 3D viewport. Fixed via a new `pickObjectByRayAlg()`
  (`EditorAlgorithms.hpp`), wired into `MeshCraftApplication_Mouse.cpp`.
  8 new headless assertions.
- **STAB-0501/0502** — translate/rotate gizmo rendering confirmed correct
  by code reading; flagged 🟡 (needs live selection + tool-mode switch).
- **STAB-0500** — confirmed a nonexistent `material=` reference falls
  back to a default gray color, no crash.
- **STAB-0499** — **found and fixed a real gap**: a `<mesh>` with a
  missing file loaded silently with no warning. Fixed `loadOrGetMesh()`
  to print a warning.

Earlier sessions (summarized): Gate 6's documentation pass, a full
`mc3.xsd` audit (found and fixed 8 schema/writer gaps), S11 (Materials/
Textures) and S12 (Animation Stability) closed out with several real bugs
found and fixed, S13 (Commands/Undo/Redo/Algorithms) fully closed with 9
new `Alg` extractions. Full history is in `git log --oneline`.

---

## 4. Current blocker / main problem

**No blocker to local development or testing** — Debug builds and passes
40/40 tests as of STAB-0533 (same count as STAB-0529 — STAB-0533 added
assertions to the existing `mc3_roundtrip` test, not a new ctest entry).
**New reconfigure requirement:** the CLion
cmake command in §7 now needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` added,
because the sibling CNA repo (commit `00e0cea`, "add ENet 1.3.17 as
vendored third-party dependency", 2026-07-04) added
`../cna/third_party/enet/CMakeLists.txt` with a `cmake_minimum_required`
too old for CMake 4.2.2 (CMake ≥4 dropped compatibility with policy
versions <3.5 — hard error, not a warning). The flag is a harmless
top-level workaround (doesn't touch any CNA file) and CNA is out of scope
for this repo per CLAUDE.md, so it was not fixed at the source.

The recurring theme in S14 is that several visual features (bounding-box
overlay, SSAO, bloom, wireframe mode, translate/rotate gizmos) are gated
by pure runtime UI booleans with **no CLI flag, document setting, or
persisted-prefs hook** — so they can't be exercised through the headless
`--screenshot` path used for the rest of this test suite. Each has been
confirmed correct and safe by code reading, but a fresh visual
confirmation needs a human with a live display. This is not a bug, just
an environment limitation — see §5 for the full list.

Genuine **operational** issues, unrelated to current work:

- **`git push origin develop` was denied earlier this session, then
  started working again without any local config change.** The remote is
  now SSH (`git@github.com:openeggbert/mesh-craft.git`, not the
  HTTPS+PAT URL previously documented here). It failed with
  `Permission to openeggbert/mesh-craft.git denied to robertvokac` for
  commits `da3a8d6` through `f45ad40` (STAB-0513–0517), then a later
  retry of the same `git push origin develop` succeeded and pushed all 5
  — so access is likely SSH-agent/key-forwarding related and may recur.
  If push is denied again: don't touch remote/credential config, just
  keep committing locally and retry later (or ask the user).
- **CI cannot be activated with the current git credentials** (separate,
  older issue). The workflow file is committed but parked at
  `.github_/workflows/ci.yml` (GitHub only treats `.github/workflows/` as
  live) because the previous HTTPS remote's embedded PAT lacked the
  `workflow` OAuth scope. Fix requires the repo owner to rotate/rescope
  credentials — may now be moot if the remote has fully switched to SSH.

---

## 5. Known bugs and limitations

- **PAT exposed in `.git/config`, lacks `workflow` scope** — see §4.
  _confirmed; needs owner action._
- **CI is partial** — only CNA-free libs are covered by the parked
  workflow; no full-editor CI job exists. _incomplete, inactive._
- **SVG texture rasterization** not implemented in `GltfExporter` — a
  material referencing an SVG texture prints a warning and omits it.
  _incomplete, documented, blocked on a library choice (librsvg vs.
  NanoSVG)._
- **`mergeSceneFromFile()`'s texture merge** can silently pick the wrong
  texture on a name collision (unlike its material merge, which suffixes
  on collision). _confirmed, not fixed, no assigned STAB-XXXX ID._
- **Embedded glTF** (`embed:id`) fails to parse as an OBJ path; export
  continues with an empty node. _incomplete, documented._
- **`EditorViewport` not integrated** into `MeshCraftApplication`'s
  render loop. _incomplete._
- **`mc3.xsd` has no numeric range constraints** — a schema-valid AI
  response can contain a negative `size`/`radius` and it applies
  unchanged. _confirmed, documented in `README.md`, not fixed._
- **`mc3.xsd`'s `mip_maps` texture attribute has zero implementation** —
  parses fine, does nothing anywhere. _confirmed, not fixed, no assigned
  STAB-XXXX ID._
- **CNA's vendored ENet needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`** to
  reconfigure with CMake 4.2.2 — see §4. _confirmed, workaround in place,
  not a CNA file change._
- **S14 visual toggles need a live display to re-verify**:
  `showBoundingBox_` (STAB-0505), `ssaoEnabled_` (STAB-0508),
  `bloomEnabled_` (STAB-0509), `showWireframeMode_` (STAB-0510), the
  translate/rotate gizmo visibility (STAB-0501/0502), the gizmo-drag
  delta overlay (STAB-0514), the per-selected-object poly-stats display
  (STAB-0515), the shadow-map debug overlay (STAB-0516), the
  locked-object outline (STAB-0518), the proportional-editing falloff
  sphere (STAB-0519, newly implemented this session), and large-scene
  live FPS (STAB-0520) — all confirmed correct and safe by code reading,
  none reachable via the headless `--screenshot` path (no
  CLI/document/prefs hook exists to force them on/pre-select or
  pre-lock an object, and FPS itself requires a sustained interactive
  loop). _needs verification by a human with a live display._
- **GL state-leak instrumentation (STAB-0521)** doesn't have an
  automated `glGetError()` check wired up — every FBO/state-toggle
  site was audited by code reading and matches the previously-fixed
  bloom leak pattern, but a definitive check needs a live GL-debugger
  session (RenderDoc/apitrace). _confirmed correct by reading, no
  automated instrumentation exists._
- **N3–N7 (scripts/sounds/music/triggers/states/meta) are data-only** —
  round-tripped but nothing executes them at runtime. _intended at this
  stage, not a bug._
- **`mc3` standalone build skips `mc3_commands`** — needs
  `EditorAlgorithms.hpp` from the editor tree, absent in a standalone
  checkout. _intended, not a bug._
- **3 `plan.md` items cannot be completed in this environment**:
  STAB-0642 (needs Blender), STAB-0643 (needs a browser), STAB-0650
  (needs CI actually running). _flagged, needs external tooling._

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

**Undo/redo:** snapshot-based. `undoStack_`/`redoStack_` (capped at 20)
hold `std::vector<Mc3::Mc3Document>`; `pushUndo()` stores a deep copy of
`document_` before each mutating command.

**The "Alg mirror" pattern**: editor/AI logic in CNA-coupled `.cpp` files
often has zero actual CNA/ImGui dependency once app-state bookkeeping and
rendering are set aside. Pure logic lives in a CNA-free header
(`include/MeshCraft/EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`),
functions suffixed `Alg`, and the real `.cpp` `#include`s and calls them
directly — single source of truth, no duplication. The headless test
suite (`mc3/test/editor_commands_test.cpp`) calls the same functions
directly with no CNA/SDL3/ImGui dependency. Recent examples:
`pickObjectByRayAlg()`, `resolveClickSelectionAlg()`,
`cameraOrbitPositionAlg()`.

**Embedding a resource file at compile time**: `mc3.xsd` is compiled into
`MeshCraft`/`ai_test` as a raw string constant, generated at CMake
**configure** time (`cmake/Mc3XsdEmbed.hpp.in`). **Editing `mc3.xsd`
requires a full reconfigure, not just a rebuild.**

**Hard constraints / invariants:**
- `Mc3Document` public API: don't change without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` must stay buildable standalone
  (no CNA/ImGui deps).
- `mc3.xsd` must stay symmetric with `Mc3XmlWriter.cpp` — any new writer
  attribute/element needs a schema declaration in the same change.
- Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`
  (triggers a full CNA recompile).
- Do not modify CNA/SHARP_RUNTIME source files from this repo.
- `file(GLOB_RECURSE)` collects sources — a new `.cpp` file (or a new
  `add_test()`, or an edit to `mc3.xsd`) needs a cmake **reconfigure**,
  not just a rebuild.
- MCB format version is `MCB_VERSION` in `McbFormat.hpp` — bump on any
  breaking wire-format change.
- XSD root element order is strict (`include → metadata → meta →
  environment → ... → objects → actions`).
- Reconfigure `cmake-build-debug/` only with CLion's bundled cmake.

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid a system-cmake bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5                      # (re)configure — flag needed since CNA added vendored ENet, see §4
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (39)
ctest -N                                                      # lists all 40 tests

# --- Release (system cmake is fine for a fresh dir)
cmake -S . -B b-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
      -DFETCHCONTENT_UPDATES_DISCONNECTED=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build b-release -j4
(cd b-release && ctest --output-on-failure)

# --- Standalone (CNA-free) component builds + tests
for c in mc3 mcb mc3togltf mc3tomcb; do
  cmake -S "$c" -B "$c-build" -G Ninja -DBUILD_TESTING=ON \
        -DFETCHCONTENT_UPDATES_DISCONNECTED=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  cmake --build "$c-build" -j4
  (cd "$c-build" && ctest --output-on-failure)
done

# --- Run / export / validate / version
./cmake-build-debug/MeshCraft test/house.mc3.xml   # open a sample scene
./cmake-build-debug/MeshCraft --version             # MeshCraft 0.1.0
./cmake-build-debug/MeshCraft test/fog_linear.mc3.xml --screenshot /tmp/out.ppm
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

## 8a. STAB-0530/0531 — already done, no new work

While setting up section 8, found that `mc3tomcb_roundtrip`
(`mc3tomcb/test/mc3tomcb_roundtrip_test.py`, from earlier STAB-0058)
already fully covers both rows — it does a stronger check (MCB magic
bytes + byte-identical determinism + no dropped content) than either
row asks for ("file created; non-zero size"). Marked both ✅ in
`plan.md` with no code changes.

## 8. Next smallest tasks

**S14 is fully closed.** **S15 (Import/Export/Editor Integration)** is
almost done: STAB-0526–0531 + 0533–0538 + 0540–0549 done (22),
STAB-0532/0539 flagged 🟡, **only STAB-0550 left**. **S16
(Cross-Platform Stability)**, 30 items, starts right after — its first
item (STAB-0551, P0, "Linux desktop build and run") is already flagged
🟡 in `plan.md`.

1. **STAB-0550 — verify subtree template import resolves relative
   paths (the last S15 item).** Files:
   `src/MeshCraft/MeshCraftApplication_Commands.cpp`. The row asks:
   when a `.mc3.xml` template exported by `exportSubtreeAsTemplate()`
   (STAB-0536, just fixed to carry its own materials/textures) is
   *imported elsewhere*, do its texture `uri`s resolve relative to the
   *template file's own directory*, not the importing scene's
   directory? Find the template-import code path (look near
   `exportSubtreeAsTemplate`/`convertToDefinitionAlg` in
   `MeshCraftApplication_Commands.cpp`, or check whether "import" here
   just means `Mc3Document::loadFromFile()` + merge, in which case this
   may already be correct by construction since `doc.sourcePath` is
   always set from whichever file was actually loaded).

2. **S16 (Cross-Platform Stability)** starts at STAB-0551. Read its
   `plan.md` rows before starting — several (MinGW/Emscripten builds,
   OS-specific config dirs, clipboard) look like they may need tools or
   platforms not available in this environment, similar to the
   STAB-0642/0643/0650 items already flagged as blocked. Don't assume 🧪
   means "needs a live UI" without checking the actual code path first
   — this session found `runGltfExport()` (STAB-0526), the pre-existing
   `mc3tomcb_roundtrip` test (STAB-0530/0531), `mc3tomcb`'s error
   handling (STAB-0537/0538), STAB-0540/0541, STAB-0546, and STAB-0548
   all turned out to already be correct/headlessly testable, needing
   only new test coverage — while STAB-0534/0535/0536/0543/0544/0545/0547
   had real bugs or wrong assumptions hiding behind plausible-looking
   code/row wording (three of them the *exact same* "export drops
   referenced textures" pattern — check any remaining export-adjacent
   row for it), and STAB-0539 had an unwired duplicate. Read before
   assuming either way.

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
  vs. NanoSVG) — a real feature decision, not a quick fix.
- **No mass refactoring** of passing code, no speculative architecture
  changes — stabilization phase; scope each change to exactly what its
  `STAB-XXXX` entry asks for.
- **No adding a debug-only CLI flag purely to force a UI toggle on for
  testing** (e.g. bloom/SSAO/wireframe) — this has been considered and
  rejected each time as scope creep beyond what the verification task
  asks for; flag 🟡 instead, same as STAB-0505/0508/0509/0510.
- **No attempting STAB-0642/0643/0650** without the missing tool first
  (Blender, a browser, or an active CI run respectively).

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect only the files needed for the first
task in section 8. Do not refactor unrelated code. Make one small,
verified improvement. Run the relevant build/test command from section 7
and confirm cmake-build-debug still passes (46/46, or the new total if
you registered a new ctest). Update NEXT.md after finishing.

Current branch: develop, prior commit `0f36ab1` (STAB-0546/0547) — the
STAB-0548/0549 commit lands right after this NEXT.md update (git push
was denied earlier in a prior session, then started working again
unprompted — see section 4; if it happens again, keep committing
locally and retry later). Build dir: cmake-build-debug/ (Debug, CLion
cmake 4.2.2) — full rebuilt and 46/46 ctest verified clean (includes a
new `--export <path>` CLI flag on the `MeshCraft` binary itself, see
§2/§3). Reconfigure now REQUIRES the extra flag
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5` — see section 4/7 for why (CNA
sibling repo added a vendored ENet dep incompatible with CMake 4.2.2
without it). Release (b-release/) and the standalone
mc3/mcb/mc3togltf/mc3tomcb builds were verified earlier in this
project's history but not re-checked as part of this update — re-verify
(with the same new flag) before relying on them.

Active plan: plan.md (STAB-XXXX tasks). Gates 0–5 closed, Gate 6
exhausted for this environment. S0–S14 fully closed (S14: 17 done, 13
flagged, 0 remaining). S15 (Import/Export/Editor Integration) is
almost done: STAB-0526–0531 + 0533–0538 + 0540–0549 done (22),
STAB-0532/0539 flagged 🟡, only STAB-0550 left — pick it up from
section 8. S16 (Cross-Platform Stability) starts right after.

Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file / new add_test()
requires a reconfigure, not just a rebuild — see section 6.

CI is parked deactivated under .github_/ (PAT lacks `workflow` scope,
see section 4).
```
