# NEXT.md

_Last updated: 2026-07-26. `SYS-W14-39` adds the local, dependency-aware
Model Registry v2 asset-pack workflow; `SYS-W9-04` adds a separate,
memory-budgeted local scene-review history with named checkpoints. The
raw-OpenGL(ES)-vs-CNA audit group
`AUD-082` through `AUD-088` is now complete. The final row, `AUD-085`
(SSAO), was implemented as a CNA depth-to-color pre-pass: the renderer
re-draws scene geometry with a 3D `ShaderEffect` into a `RenderTarget2D`,
then performs AO, blur and multiplicative composition through CNA APIs.
This covers static and dynamic (Disk/Grid/Extrude) scene geometry; no
SSAO `glBlitFramebuffer`, raw FBO, or raw shader path remains. `AUD-092` now
adds a pixel-level regression that compares this fixture with SSAO off and
forced on, rather than treating a clean screenshot exit as visual coverage.
The same session activated `.github/workflows/ci.yml` and added a root editor
build and test job alongside the standalone component matrix. See `plan.md`
for full evidence; older session history remains below and in `docs/history/`._

_Source-layout note (2026-07-25): the editor application implementation is
now at `src/MeshCraft/Application/` and `src/MeshCraft/Application/UI/`,
replacing the former flat `src/MeshCraft/MeshCraftApplication_*.cpp` layout.
`MeshCraftApplication` itself is now
`MeshCraft::Application::MeshCraftApplication`; the old public include remains
a compatibility forwarder. Actual UI components currently cover Validation,
Registry results, Toolbar tools/display/snap-surface/proportional/grid controls,
Properties delegation, Camera Preset Overlay, Gizmo Drag Overlay, Stats Overlay, Measurement Overlay, Status Bar, and the View-menu panel/overlay/direction/focus/
Camera-Bookmarks/Walk-Mode/Bloom-SSAO presentation, plus the Add/CSG and Help menu
presentation and the Edit-history, clipboard, object-action, and
selection-action groups, the Select-by-Type/Tag/Material, Align-Selection,
Distribute-Selection, and Mirror-Selection submenus, and the Copy-Properties,
Convert-to-Definition, Export-Subtree, Break-Instance, Drop-to-Ground,
Snap-to-Grid, Group-Scale, Linear-Array, and Scatter-Along-Curve items plus the
Group/Ungroup pair and every File-menu action through Open Recent. The detailed
toolbar Snap interval contents and the remaining MenuBar sections are still
application-owned. The post-menu audit deliberately keeps MenuBar's top-level
shell there as a small composition layer; extracting it would create a broad
context. Camera bookmark, walk,
document, undo, clipboard, selection, grid, and dialog state have **not** moved
again: they remain in their existing editor/application owners; the UI
component only reads presentation state and invokes application-owned
callbacks.
Historical references below intentionally retain their then-current paths.

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
both fixed, 2026-07-19 — see §3. A 2026-07-20 session then ran a
**targeted (not general) audit**: how much raw OpenGL(ES) the editor
calls outside CNA's own API. Found 5 consumers sharing one raw-GL
function table plus 2 smaller standalone spots;
filed as `AUD-082`-`AUD-088`. All 7 are now migrated onto CNA's
`RenderTarget2D`/`ShaderEffect`/`GraphicsDevice` APIs; `AUD-085` uses a
depth-to-color pre-pass. `plan.md`'s remaining
`AUD-###` rows were all `DONE`/`DEFERRED` except `AUD-042` (Android) until
the 2026-07-25 follow-up audit filed `AUD-089` through `AUD-091`: CLI
screenshot failure reporting, render-test environment/labels, and an AI
definitions-only response timeout. Android is a supported product target:
commit `20a5473` now selects its GLES/EASYGL source path, while NDK,
packaging, and device validation remain externally blocked.
The project is in an **ongoing hardening / bug-fixing** phase, not
active new-feature development, though scoped new features have landed
before when explicitly requested (`SYS-W14-##` rows).

**Important architectural decisions:**
- `Mc3Document` (in `mc3/`) is the single canonical in-memory AST. It has
  no CNA/GUI dependency and is shared by both parsers/writers, `mcb`,
  `mc3togltf`, `mc3tomcb`, and the editor.
- The editor (`MeshCraftApplication`/`SceneRenderer`) and the exporter
  (`mc3togltf`) are **two independent geometry generators** reading the
  same `Mc3Document`, with one deliberate exception (`STAB-0670`):
  Torus/Capsule/IcoSphere have no native Manifold primitive for CSG use,
  so `SceneRenderer.cpp` (`:26,122`) directly calls `mc3togltf_lib`'s own
  `buildPrimitive()` for those three shapes' preview mesh instead of
  duplicating a second implementation — see `differential_geometry_test.cpp`,
  which exists specifically to cover this shared path. Every other
  primitive/CSG/extrude build has no shared mesh-building code at all.
  Triangle winding differs deliberately (export CCW-from-outside, editor
  preview CW-from-outside) — do not "fix" one to match the other.
- `../cna` and `../sharp-runtime` are **sibling repositories, not part of
  this repo, and not to be modified from here** — a separate
  process/owner handles them. This repo only consumes them via
  `add_subdirectory`.
- A second, unrelated sibling repository, **`../mesh-world`**, drives its
  own feature work directly into this repo's `mc3/` library from time to
  time — commits can land here that this repo's own `plan.md` never
  asked for. Check `git log` at the start of any session.

## 2. Current status

- **Last full build: clean after `SYS-W9-04` scene history/checkpoints.** Testing is enabled in the current Ninja Release tree, and
  `CCACHE_DISABLE=1 cmake --build b-release -j4 --target MeshCraft` linked
  successfully on EASYGL. Alternate-backend runtime qualification remains
  blocked.
- **Tests:** the current Release tree registers 211 tests. All 171 runnable
  tests pass; the Xvfb preflight deterministically disables 40 render
  registrations on this host and one render display-preflight is skipped.
  `SYS-W9-04` adds a CNA-free scene-history regression covering the logical
  memory budget, eviction policy, named checkpoints, independent restore with
  selection, large documents, and object/resource review diffs.
  `SYS-W14-38` adds pure playback-range/blend tests, XML/JSON/MCB clip
  round-trip coverage, and an actual glTF clip export test for TRS sampling,
  reverse/rate baking, deterministic JSON/binary output, portable policy, and
  ID-bearing unsupported-channel reports. `SYS-W14-37` added structural coverage for `KHR_mesh_quantization`,
  16-bit index/accessor encoding, deterministic output, explicit GLB versus
  textual-glTF image treatment, preflight estimation, and object-ID reports.
  `SYS-W14-36` added direct bounded GLB import, explicit trusted
  external-gltf confinement, rejection, and MC3→GLB round-trip coverage.
  Before that additive test, an Xvfb-qualified host passed all 185 registrations after `AUD-042` in two
  disjoint groups: 149/149 non-render tests and 36/36 render-labelled tests.
  `mc3togltf_csg_shading_materials` reads a real GLB to assert smooth CSG
  normals, generated UVs, and preserved child-material primitives; the CSG
  viewport-cache/cutter render regressions also pass. The earlier embed export
  test creates an external and inline GLB at runtime and checks its flattened
  geometry; the screenshot test proves the viewport loads that same embed
  without falling back to a placeholder. SVG-specific
  verification passes with `-j4`: external and inline SVG export to glTF PNGs,
  bounded/malformed input, cache invalidation, and real headless viewport
  screenshots sampling the rasterized material pixels. This session's own
  `AUD-082`-`092` work added 4 brand-new ctest targets — `bloom_test`,
  `matpreview_test`, `shadowdebug_test`, `ssao_test` — one per migrated feature that
  previously had zero visual-correctness coverage (see §3). The 142
  count this file last recorded (2026-07-19) predates unrelated work not
  narrated here (a further audit pass and the `SYS-W14-18..27`
  mc3-format-vs-editor gap closures — see `plan.md`/memory, not fully
  reflected in this file's own history) — don't treat 142→184 as this
  session's own delta.
  All builds/tests this session used at most `-j4` (never `-j$(nproc)`), per
  the user's standing request (shared machine).
- **CLI/tools/apps/libraries currently available:**
  - `MeshCraft` — the interactive editor (`./b-release/MeshCraft
    scene.mc3.xml`, or `--screenshot out.png` / `--export out.glb` /
    `--benchmark` for headless one-shot runs).
  - `mc3togltf` — MC3 → glTF/GLB exporter (`--stats` prints diagnostics).
  - `mc3tomcb` — MC3 XML → MCB binary converter.
  - Standalone libraries `mc3` (format/AST + XML/JSON parse-writer),
    `mcb` (binary format) — both buildable and testable without CNA via
    their own `mc3/build`/`mcb/build` trees (no live GPU/GL needed).
- **Recently implemented (2026-07-26):** `SYS-W9-04` adds a local
  `Editor::SceneHistory` timeline separate from the exact 20-entry undo/redo
  stack. Every edit can capture a budgeted automatic restore point; **Edit →
  History & Checkpoints** also creates named checkpoints, restores the stored
  selection through an undoable operation, and compares a snapshot with the
  current scene by stable object ID. The default 64 MiB is a documented
  logical ownership estimate, not a resident-memory claim. History is scoped
  to the current session and clears on New/Open/recovery; it is neither
  persisted nor synchronized. Commit `7cc3e8d`.
- **Recently implemented (2026-07-26):** `SYS-W14-39` upgrades the Model
  Registry with structured metadata filters, deterministic cached catalog
  tiles, import-conflict protection, material-health reporting, and filtered
  export of a portable **local** asset pack containing only referenced
  libraries. Accounts, remote storage, synchronization, and collaboration
  remain deliberately out of scope. Commit `6394eae`.
- **Recently implemented (2026-07-26):** `SYS-W14-35` preserves OBJ `usemtl`
  groups end to end. The editor imports each material assignment as a Mesh
  child under one source group, maps the safe MTL PBR subset into generated
  MC3 materials, and clearly warns about lossy MTL fields. The hardened
  shared parser validates hostile indices and is reused by the viewport and
  exporter, whose material-index metadata selector prevents a later flatten.
  Fixtures cover multi-material and missing-MTL input, hostile indices, and
  the exported glTF node/material split. External sources retain the existing
  safe-export rejection rather than being rewritten as traversal paths.
- **Recently implemented (2026-07-26):** `SYS-W14-36` adds File → Import
  GLB / glTF as an editable, portable workflow. A normal self-contained GLB
  becomes one inline source embed plus a native MC3 node/primitive hierarchy,
  PBR materials, embedded image textures, cameras, and punctual lights;
  explicit selectors keep each Mesh from later flattening on preview/export.
  The visible trusted-gltf opt-in reads only same-directory companions under
  strict JSON/payload limits, then converts them to that same inline source —
  no external path remains after import. Skins/morphs/animations are named
  lossy cases and non-triangles reject before mutation. Inline image data now
  also decodes directly in the viewport with encoded-byte and pixel caps.
- **Recently implemented (2026-07-26):** `SYS-W14-33` adds a capability-gated,
  CNA-only point/spot-light preview. On a valid source-GLSL `ShaderEffect`
  backend, the first eight document-order punctual lights affect normal/UV
  viewport geometry with authored color/brightness, inverse-square range
  attenuation, and spotlight cone/falloff; directional/ambient contribution
  remains intact. Unsupported or failed shader paths clearly retain the
  BasicEffect-plus-gizmo fallback. CNA-free boundary/selection tests and
  red/green pixel fixtures cover the change; the screenshot CTest is
  registered but disabled here by the no-Xvfb preflight. A Release build and
  all 160 non-render CTests pass.
- **Recently implemented (2026-07-26):** `SYS-W14-34` gives the live CSG
  preview the exporter’s child-material composition. A cached Manifold mesh
  shares one CNA vertex buffer across material index ranges; a valid explicit
  CSG-root material remains the full-result override. Material fields now
  participate in the content hash, so edits invalidate the range cache.
  CNA-free partition/cache tests and red/blue-versus-root-green pixel fixtures
  cover both rules; the screenshot CTest is registered but disabled here by
  the no-Xvfb preflight. A Release build and all 161 non-render CTests pass.
- **Recently implemented (2026-07-26):** `SYS-W14-30` extends Walk Mode’s
  explicit collision contract from boxes to exact uniform Sphere/IcoSphere
  and upright circular Capsule proxies. Swept side contacts use the rounded
  cross-section and vertical contacts use the real spherical-cap surface;
  the original swept-box behavior remains intact. Active proxies have
  color-coded world-space debug outlines, a 256-proxy document-order budget,
  and a persistent HUD warning for unsupported, incompatible, or over-budget
  proxies. `mesh` and `convex` remain visibly unsupported rather than being
  silently boxed. The Properties panel’s **Generate Simple Proxy** command
  assigns the appropriate proxy to supported selected primitives in one undo
  step. Expanded `walk_controller` coverage verifies rounded wall/floor/
  ceiling behavior and both budget sides. Release build and all 161
  non-render CTests passed; render CTests remain disabled by the no-Xvfb
  preflight on this host.
- **Recently implemented (2026-07-26):** `SYS-W14-32` makes authored ordinary
  object UV mappings visible in the viewport. The new CNA-free helper applies
  the exporter-compatible default/planar, box, and sphere rules and UV
  transform order to the existing texture-capable render meshes, while CSG
  preserves its separately generated mapping cache and both consumers retain
  their intentional winding differences. Differential exporter comparison
  covers all three mappings; the default/box/sphere screenshot test is
  registered but disabled here by the established no-Xvfb preflight. A Release
  build and all 159 non-render CTests pass.
- **Recently implemented (2026-07-26):** `SYS-W3-02` adds the narrow,
  CNA-free `SceneSemanticsAlgorithms.hpp` contract shared by the viewport and
  glTF export: pivot transform parts, material/visibility precedence, stable
  object identity, and resolved instance variant/LOD/definition selection.
  The normal/depth/edge/emissive/CSG editor paths and both glTF paths now use
  it without merging their separate mesh generators or changing `Mc3Document`.
  `scene_semantics` verifies the pure contract; an additional actual-export
  fixture verifies inherited-definition material, instance material, and
  instance `material_override` precedence. Its full then-current non-render
  suite passed; render CTests remain disabled by the no-Xvfb preflight on this
  host.
- **Recently implemented (2026-07-25):** `SYS-W14-04` rasterizes external
  and inline SVG texture entries through pinned NanoSVG code. The resulting
  RGBA pixels are used by the live CNA viewport and generated as PNG images
  for glTF/GLB export; a 2048px dimension cap prevents hostile SVG dimensions
  from allocating unbounded memory (commit `8cb14be`). The viewport cache uses
  compact content hashes rather than retaining inline markup as map keys;
  external SVG changes invalidate both successful and failed rasterizations.
  SVG textures now round-trip and honor `wrap_u`, `wrap_v`, and `filter` in
  the viewport and glTF sampler. `mip_maps` is honored by glTF export; live
  CNA textures remain level-zero only because the available CNA API has no
  mip-chain generation (follow-up commit `026fc2d`).
- **Recently implemented (2026-07-25):** `SYS-W3-01` Phase 8 extracts the
  five-slot `Editor::CameraBookmarks` state from `MeshCraftApplication`.
  Capture/restore and invalid-slot behavior have their own focused test;
  the menu and keyboard shortcuts retain their existing behavior (commit
  `239b43b`).
- **Recently implemented (2026-07-25):** `SYS-W3-01` Phase 9 extracts the
  transform clipboard's exact position/rotation/scale transfer contract. The
  menu and shortcuts retain their undo/selection semantics, while the
  extracted class has a direct regression test that proves it does not copy a
  target's pivot (commit `221b845`).
- **Recently implemented (2026-07-25):** `SYS-W3-01` Phase 10 extracts the
  timed status notification. Message replacement, severity, and expiry are
  now CNA-free and regression-tested; the existing ImGui status-bar rendering
  remains in the application (commit `d287bc8`).
- **Recently implemented (2026-07-25):** `SYS-W3-01` Phase 11 extracts
  editor-session object-lock membership into `Editor::ObjectLockState`.
  Commands, mouse/keyboard input, statistics, and hierarchy lock buttons use
  its narrow lock/query interface; scene, selection, undo, and UI ownership
  remain where they were. The new direct regression test covers idempotent
  lock/unlock and toggle behavior (commit `4bb30ea`).
- **Recently implemented (2026-07-25):** `SYS-W3-01` Phase 12 extracts the
  headless `--benchmark` frame-progress lifecycle into
  `Editor::BenchmarkProgress`. It has a direct CNA-free test for frame
  counting, samples, completion and startup timing; the application retains
  rendering and benchmark-category presentation (commit `2e87524`).
- **In progress (2026-07-25):** `SYS-W3-01` Phase 13 moved the concrete
  application into `MeshCraft::Application`, organized implementation files
  by application/UI ownership, and is extracting UI presentation through
  narrow contexts. Validation, Registry results, Toolbar controls, Properties
  delegation, Camera Preset Overlay, Gizmo Drag Overlay, Stats Overlay, Measurement Overlay, Status Bar, and the View-menu directions/focus/overlays/panels/
  Camera-Bookmarks/Walk-Mode/Bloom-SSAO presentation, plus the Add/CSG and Help menu
  presentation and the Edit-history, clipboard, object-action, and
  selection-action groups, the Select-by-Type/Tag/Material, Align-Selection,
  Distribute-Selection, and Mirror-Selection submenus, and the Copy-Properties,
  Convert-to-Definition, Export-Subtree, Break-Instance, Drop-to-Ground,
  Snap-to-Grid, Group-Scale, Linear-Array, and Scatter-Along-Curve items plus
  the Group/Ungroup pair are now component-owned. Bookmark, walk, document,
  undo, clipboard, selection, grid, preferences, command-palette, and
  shortcut-dialog state remain in their existing owners; the menu receives
  only read-only state plus application-owned callbacks.
- **Recently implemented (2026-07-25):** `SYS-W3-01` Phase 13 moved the
  `File → Open Recent` presentation into `Application::UI::MenuBar` through
  `FileOpenRecentContext`. It receives only current availability, a lazy
  recent-files provider, an open callback, and a clear callback. The component
  preserves the submenu label, numbered hidden IDs, basename labels, full-path
  tooltips, separator, and empty-list disabled state; persistence, unsaved
  document handling, loading, document replacement, and status/error reporting
  remain application-owned. `CCACHE_DISABLE=1 cmake --build b-release -j4
  --target MeshCraft` and `ctest --test-dir b-release -R '^mc3_commands$'
  --output-on-failure` passed; the latter already exercises recent-files'
  load, save, MRU de-duplication, cap, and restart round-trip contract.
- **Recently implemented (2026-07-25):** `SYS-W3-01` Phase 13 moved the
  `View → Bloom/SSAO` controls into `Application::UI::MenuBar` through
  `ViewPostProcessingContext`. It receives the application-computed
  ShaderEffect-capability gate and value snapshots, then reports changes only
  through five setters; renderer capability checks and all post-processing
  state remain application-owned. The supported/unsupported presentation,
  labels, slider ranges, 140px widths, and `AlwaysClamp` safety guards are
  unchanged. `CCACHE_DISABLE=1 cmake --build b-release -j4 --target MeshCraft`
  and the Xvfb-hosted `bloom_test` render regression passed. The test-hook
  comments now also correctly state that `MESHCRAFT_TEST_FORCE_POSTFX` forces
  only Bloom; SSAO has its own `MESHCRAFT_TEST_FORCE_SSAO` hook.
- **Recently implemented (2026-07-25):** `AUD-092` adds `ssao_test`, a
  `render`-labelled headless regression for the CNA SSAO pipeline. It renders
  `light_shading.mc3.xml` normally and with `MESHCRAFT_TEST_FORCE_SSAO=1`, then
  requires more than 1,000 red pixels to darken, more than 2,000 total
  red-channel darkening, and a maximum per-pixel reduction of at least two.
  The calibrated run observed 1,973 darkened pixels, so this detects a missing
  depth pre-pass, AO pass, blur, or multiplicative composite without depending
  on a fragile full-image golden file.
- **Recently implemented (2026-07-25):** `SYS-W3-01` Phase 13 moved the
  top-left viewport camera controls into `Application::UI::CameraPresetOverlay`.
  `CameraPresetOverlayContext` has only the orthographic/look-through
  snapshots, selected-camera name, and reset/orbit/projection/look-through
  callbacks. The application still owns `EditorCamera`, selected-camera bounds
  checks, document access, and render behavior. The existing `mc3_commands`
  camera-preset regression still verifies the shared Front/Top/Right/Persp
  table; the extracted header compiles independently and the Xvfb `smoke_test`
  passed after the move.
- **Recently implemented (2026-07-25):** the follow-up overlay audit moved the
  cursor-following gizmo drag-delta display into
  `Application::UI::GizmoDragOverlay`. Its context receives only axis index,
  current/start transform values, unit, and rotation/snap snapshots. The
  application retains gizmo and selection lifetime, transform inspection, and
  every mutation path. It preserves the X/Y/Z colors, signed delta, units, and
  Ctrl-or-grid rotation snap indicator; the header compiles independently and
  the focused `mc3_commands` plus Xvfb `smoke_test` regressions passed.
- **Recently implemented (2026-07-25):** the next overlay-audit slice moved
  the top-right viewport scene summary into `Application::UI::StatsOverlay`.
  Its value-only context carries layout, FPS/isolation/camera display state,
  object/selection/lock counts, camera values, and renderer-derived geometry
  counts. The application remains responsible for scene traversal, object-lock
  queries, selected-camera bounds, and `SceneRenderer::scenePolyStats()`;
  component code has no document, renderer, or mutating callback. The header
  compiles independently and the focused `mc3_commands` plus Xvfb `smoke_test`
  regressions passed.
- **Recently implemented (2026-07-25):** the measurement-ruler presentation
  is now `Application::UI::MeasurementOverlay`. The application retains the
  active-tool/point state and cached view-projection calculation, then provides
  projected positions, immutable point coordinates, distance, and viewport
  bounds. The component preserves point/line colors, distance label, second
  point hint, and lower-left result panel without access to the camera,
  document, or mutation paths. Its header compiles independently and focused
  `mc3_commands` plus Xvfb `smoke_test` regressions passed.
- **Recently implemented (2026-07-25):** the bottom bar is now
  `Application::UI::StatusBar`. The application prepares either a timed
  notification or a scene-summary snapshot and supplies validation metadata
  plus the sole callback to open the existing panel. Notification lifetime,
  document traversal, selection, and validation-panel state remain in the
  application. The component preserves the colors, selected-object text,
  validation indicator, tooltip, and click action; its header compiles
  independently and focused `mc3_commands` plus Xvfb `smoke_test` passed.
- **Recently implemented (2026-07-20 through 2026-07-25):** all 7
  raw-OpenGL(ES)-vs-CNA migrations, `AUD-082` through `AUD-088` — full
  detail with file:line evidence and
  verification commands lives in `plan.md`'s own rows; condensed summary
  here:
  - `AUD-082` — panel scissor/viewport clip: raw `glViewport`/`glScissor`/
    `glEnable(GL_SCISSOR_TEST)` calls replaced with `RasterizerState` +
    `GraphicsDevice.Viewport`/`ScissorRectangle`.
  - `AUD-083` — `--screenshot` pixel readback: raw `glReadPixels` replaced
    with `GraphicsDevice.GetBackBufferData()`.
  - `AUD-084` — Bloom post-process: hand-rolled FBO ping-pong + raw
    shaders replaced with 2 `RenderTarget2D`s + 2 `ShaderEffect`s (blur,
    composite) + `SpriteBatch`. Found and fixed **two genuine, previously
    undocumented CNA behavioral gotchas** along the way (recorded in
    §6): `RenderTarget2D`'s `DiscardContents` default means
    `SetRenderTarget()` clears the target on *every* bind, so a
    redundant re-bind silently wipes just-rendered content; and
    `SpriteBatch`'s custom-effect draws project to the full window (not
    a custom `Viewport`) when targeting the backbuffer directly, so
    destRects for those must be window-absolute, not viewport-local.
  - `AUD-085` — SSAO: migrated onto CNA via a genuine depth-to-color
    pre-pass: `SceneRenderer::drawDepthPass()` redraws scene geometry into
    a `RenderTarget2D`, then CNA `ShaderEffect`/`SpriteBatch` AO, blur, and
    multiply-composite passes consume that color depth. This is the required
    replacement because CNA exposes no sampleable depth attachment.
  - `AUD-086` — Skybox: raw VAO/shader equirect draw replaced with
    `Texture2D` (mixed-axis `SamplerState`: wrap-U/clamp-V, built by
    mutating a preset) + `ShaderEffect` + `SpriteBatch`, full-screen.
  - `AUD-087` — Material-preview swatch: raw FBO render + manual GL
    texture handle handed to `ImGui::Image()` replaced with
    `RenderTarget2D` + `ShaderEffect`. Its initial native-handle bridge was
    subsequently superseded by `SYS-W8-04`'s opaque
    `ImGuiTextureRegistry`; production UI code no longer consumes a GL
    texture handle (see §6).
  - `AUD-088` — Shadow Map Debug overlay: raw FBO + separate color/depth
    texture pair replaced with one `RenderTarget2D(..., DepthFormat::
    Depth24)`; needs no custom `ShaderEffect` at all since it just
    redirects the already-CNA-based `sceneRenderer_->draw()` call into
    an off-screen target. This was the 5th and last consumer of the
    former raw-GL table. Its now-unused function loader, shutdown check,
    frame-error diagnostic, and two vacuous tests were removed in `2574fc6`;
    `src/` now has no `SDL_GL_GetProcAddress` call.
  - Every migration verified with real `--screenshot` pixel sampling
    (not `Texture2D::GetData()` — confirmed unreliable for reading a
    render target's just-rendered content within the same frame, see
    §6), plus `git stash`-based before/after pixel-diffs where a
    pre-existing baseline existed (`AUD-082`/`083`/`084`/`086`).
  - Commits: `773437f` (`AUD-082`), `0796d62` (`AUD-083`), `c1be563`
    (`AUD-084`), `43d8744` (`AUD-086`), `e47a846` (`AUD-087`), `95327bc`
    (`AUD-088`), `41dd630` (`AUD-085`, SSAO and CI) — each with a matching
    `docs(AUD-0NN): ...` follow-up
    commit in `plan.md`, same two-commit pattern as every prior `AUD-###`
    row.
- **Recently implemented (previous session, 2026-07-19):** task 1 from the
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
  (`SYS-W14-10`/`11`/`12`/`13`/`15`); Area's properties panel was formally
  confirmed complete (`SYS-W14-17`), while `coordinate_system` was later
  implemented end-to-end (`SYS-W14-14`, 2026-07-26); undo/redo
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
  - CI: active at `.github/workflows/ci.yml`; it includes both the
    standalone matrix and a pinned-sibling root editor build/test job.

## 3. Recent changes

**This session (2026-07-20):** started as a pure research request ("how
much raw OpenGL(ES) does the editor call outside CNA's own API?" —
no code change, just an investigation, reported directly to the user).
The user then asked to file `plan.md` tasks for migrating whatever is
mechanically migratable, with any genuine gap going through a future CNA
NOXNA capability request instead of new raw GL. That produced `AUD-082`
through `AUD-087` (later corrected to include an `AUD-088`, a 5th
raw-GL consumer initially missed). The user then authorized
implementing them ("commitni pushni, pote jde na tyto nove ukoly"),
which was carried out one task at a time, same rigor as every prior
`AUD-###` fix in this project (implement, add a regression test that
exercises the migrated path, verify via real `--screenshot` pixel
sampling and/or a `git stash` before/after diff, run the full suite,
commit code then commit `plan.md`'s writeup, push) — see §2's condensed
per-task summary and `plan.md`'s own `AUD-082`-`AUD-088` rows for full
evidence/verification detail. Commits, in order: `773437f`/`940298f`/
`6d5d381` (`AUD-082`), `0796d62`/`f80bdc0` (`AUD-083`), `c1be563`/
`2cdc5e7` (`AUD-084`), `df217c8` (`AUD-085`, filed as `TODO`/deferred,
no code), `43d8744`/`196e52a` (`AUD-086`), `e47a846`/`b134fd2`
(`AUD-087`), `95327bc`/`f07445d` (`AUD-088`). Mid-series, when `AUD-085`
(SSAO) turned out to need a genuine architecture change rather than a
mechanical swap, asked the user how to proceed (`AskUserQuestion`) —
they chose to skip it and continue with `AUD-086` — so `AUD-085` stays
`TODO`/described in `plan.md` and queued (not silently dropped) in §8
here, not implemented. All builds/tests used `-j4` per the user's
mid-session request. `python3 test/validate_plan_consistency.py .
b-release --run-tests` confirmed the then-registered suite passing and `plan.md`
internally consistent after every single commit in this sequence.

**Previous session (2026-07-19):** tasks 1 and 2 were each individually
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

**Session before that (2026-07-18), in order:**
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

**The current `SYS-W14-05` slice has a clean full Release build and complete
test verification.** `AUD-090` preflights `xvfb-run` with a real `xdpyinfo` client.
The execution sandbox blocks the local sockets needed by Xvfb and the
`mc3_ai` loopback mock server, but the permitted host run completed both
partitions cleanly: 147/147 non-render tests (including `mc3_ai` in 1.28
seconds) and 36/36 render tests. CI explicitly installs `xvfb` and
`x11-utils` for the same render path. `AUD-091` therefore remains closed as
a stale-build false positive rather than hidden behind a longer timeout.

The P1 alternate-backend qualification (`SYS-W8-05`) remains blocked by CNA's
missing cross-backend custom-effect contract and a first-frame Vulkan backend
crash outside this repository. Broad backend qualification is now deliberately
postponed by user priority. Android (`AUD-042`) is an intended supported
platform. Commit `20a5473` forces its source build to GLES/EASYGL rather than
SDL_RENDERER; this workspace still has no Android NDK, and validation is
blocked in sibling `sharp-runtime` before CNA graphics compile.

## 5. Known bugs and limitations

- **Web/Windows build status: needs re-verification**, not checked this
  session — see §2's caveat and README.md's own platform table.
- **`SYS-W3-01` (deferred by user priority, not a bug):** `MeshCraftApplication` god
  object, 12 subsystems extracted so far (`KeybindingManager`,
  `Preferences`, `MacroRecorder`, `UndoManager`, animation-override
  computation, `WalkController`, `AudioPreview`, `CameraBookmarks`). File
  dialogs and post-processing were investigated and explicitly declined as
  further extraction targets; `TransformClipboard`, `StatusNotification`,
  `ObjectLockState`, and `BenchmarkProgress` are now also separate (see
  `plan.md`). Phase 13 is reorganizing application/UI ownership in small
  slices; it does not move those subsystem states back into UI code.
- **By-design, not bugs:** MC3 silently drops unrecognized XML
  attributes/elements on round-trip (`SYS-W5-03`, human-decided,
  documented in `MC3_FORMAT.md`); editor/exporter use different triangle
  winding deliberately; exact undo/redo is a bounded 20-entry stack
  (`AUD-038`) while the separate local review history is a 64 MiB-budgeted,
  non-persistent snapshot timeline (`SYS-W9-04`). Embedded self-contained GLB meshes are supported; loose glTF
  companion-file assets and other unsupported embedded-asset features are
  explicitly rejected with a warning (see `SYS-W14-05`).

## 6. Architecture notes

- **CNA `RenderTarget2D`/`ShaderEffect`/`SpriteBatch` gotchas** (found
  during `AUD-084` through `AUD-088`, including the SSAO depth pre-pass):
  - `RenderTarget2D` defaults to `RenderTargetUsage::DiscardContents`,
    and `GraphicsDevice::SetRenderTarget()` unconditionally clears a
    `DiscardContents` target on **every** bind call
    (`GraphicsDevice.cpp:1843-1857` in `../cna`) — a redundant
    "defensive" re-bind of the same target silently wipes out
    just-rendered content, with zero error indication. Bind exactly once
    per pass.
  - `EasyGLSpriteBatchBackend::FlushBatch()` sizes its orthographic
    projection to the bound `RenderTarget2D`'s own dimensions when one
    IS bound, but to the full **window** size when none is bound
    (backbuffer-targeted draws) — it does not honor a custom
    `GraphicsDevice.Viewport` for backbuffer draws. Use window-absolute
    destRect coordinates for any `SpriteBatch` draw targeting the
    backbuffer directly, not RT-local `(0,0,w,h)` coordinates.
  - `Texture2D::GetData()` is unreliable for reading a `RenderTarget2D`'s
    content immediately after rendering to it, within the same frame —
    gave false zero readings during debugging. Trust real
    `--screenshot` output (`GetBackBufferData`) for verification, not
    `GetData()`.
  - CNA textures shown by `ImGui::Image()` now use monotonic opaque tokens
    from `ImGuiTextureRegistry`, resolved by the CNA-backed ImGui renderer.
    Production MeshCraft has no `GetColorGLHandle()` or native GL texture-id
    bridge; `imgui_renderer_portability` guards against reintroducing one.
  - `SamplerState` only ships combined presets (`LinearWrap`,
    `LinearClamp`, etc. — same mode both axes). A mixed-axis mode (e.g.
    wrap-U/clamp-V for an equirect skybox) needs constructing a preset
    then calling `setAddressUProperty()`/`setAddressVProperty()`
    individually.
  - `drawImGuiUi()` only queues ImGui's draw list; actual rasterization
    happens later, when `EndDraw()` calls the MeshCraft-owned
    `imguiRenderer_->render(ImGui::GetDrawData())`. Any test-only "blit on
    top of everything" hook must run after that call, not in `Draw()`, or
    it is silently overwritten.
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
- **Undo/redo and review history:** `Editor::UndoManager` remains the exact
  whole-document snapshot undo/redo mechanism (20 entries, selection paired
  with each state). `Editor::SceneHistory` is intentionally separate: each
  mutation also offers a bounded, local review restore point, with a 64 MiB
  default logical-memory budget, named checkpoints, restore-with-selection,
  and object/resource diff review. It is not a command-pattern rewrite, does
  not affect redo invalidation, is cleared on New/Open/recovery, and is never
  synced or persisted across sessions.
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

No actionable follow-up audit task remains: `AUD-089` through `AUD-092` are
complete, and the user-authorized `SYS-W14-39` and `SYS-W9-04` work is also
complete. No additional user-authorized implementation task is currently
queued. Android (`AUD-042`) now chooses its GLES/EASYGL source path and
remains externally blocked only on NDK/dependency/package/device validation.
`SYS-W3-01` has 12 completed subsystem phases; its Phase 13 application/UI
ownership work is deferred by user priority. The completed Camera Bookmarks, Camera Preset Overlay, Gizmo Drag Overlay, Stats Overlay, Measurement Overlay, Status Bar, Walk Mode, View
Bloom/SSAO, Help, Add/CSG, Edit-history, Edit-clipboard, and Edit-object-actions
menu slices are implemented and verified, together with Edit-selection-actions,
Edit-select-by-type/tag/material, Edit-copy-properties, Edit-grouping,
Edit-convert-to-definition, Edit-export-subtree, Edit-break-instance,
Edit-align-selection, Edit-distribute-selection, Edit-drop-to-ground,
Edit-snap-selection-to-grid, Edit-mirror-selection, Edit-group-scale,
Edit-linear-array, Edit-scatter-along-curve, Edit-batch-rename,
Edit-find-replace-names, Edit-randomize-transform, Edit-macro-recording,
Edit-play-macro, Edit-macro-editor, Edit-lock-selection, Edit-reset-transform,
Edit-transform-clipboard, Edit-isolate-selection, Edit-hide-selection,
Edit-show-all-hidden, File-merge-scene, File-export-selection, File-export-GLB,
File-export-OBJ, File-save, File-save-as, File-import-OBJ, File-new,
File-open, File-open-recent, and File-exit slices.
`EditHistoryContext` exposes only `canUndo`/`canRedo` plus Undo, Redo, and
Open History callbacks; `Editor::UndoManager`, document replacement,
selection restoration, dialog state, and keyboard handling remain
application-owned.
`EditClipboardContext` exposes only Cut, Copy, and Paste callbacks; clipboard
contents, selection, undo, document mutation, and the Ctrl+X/C/V keyboard paths
remain application-owned.
`EditObjectActionsContext` exposes only Duplicate-at-Offset availability plus
Duplicate, Duplicate at Offset, and Delete callbacks. Offset calculation, grid
spacing, selection, undo, status reporting, document mutation, and the
Ctrl+D/Ctrl+Shift+D/Delete keyboard paths remain application-owned.
`EditSelectionActionsContext` exposes only Select All and Invert Selection
callbacks; scene traversal, selection state, window-title updates, and the
Ctrl+A/Ctrl+I keyboard paths remain application-owned.
`EditSelectByTypeContext` exposes only a lazy present-types provider plus one
type-selection callback. The provider runs only while the submenu is open;
scene traversal, selection mutation, and window-title updates remain
application-owned, while `MenuBar` owns the unchanged type labels and
empty-scene presentation. The provider collects unique values directly into
one enum-capacity-reserved vector, avoiding the former set-node allocations and
set-to-vector copy.
`EditSelectByTagContext` exposes only a lazy sorted-tags provider plus one
tag-selection callback. The provider runs only while the submenu is open and
returns its set directly without a conversion copy; scene traversal, selection
mutation, status reporting, and window-title updates remain application-owned,
while `MenuBar` owns tag-item and empty-scene presentation.
`EditSelectByMaterialContext` exposes only a lazy sorted-materials provider
plus one material-selection callback. The provider runs only while the submenu
is open, excludes empty material names, and returns its set directly without a
conversion copy; scene traversal, selection mutation, status reporting, and
window-title updates remain application-owned, while `MenuBar` owns
material-item and empty-scene presentation.
`EditCopyPropertiesContext` exposes only 2+-selection availability plus one
open-dialog callback. Selection state, dialog state and contents, property
copying, and the Ctrl+Shift+P keyboard path remain application-owned;
`MenuBar` owns the unchanged item presentation and following separator.
`EditGroupingContext` exposes only Group and Ungroup callbacks. Selection
state, undo, document mutation, status reporting, and the
Ctrl+G/Ctrl+Shift+G keyboard paths remain application-owned; `MenuBar` owns
only the unchanged labels, shortcuts, order, and click dispatch.
`EditConvertToDefinitionContext` exposes only current-selection availability
and one conversion callback. Selection state, definition/instance mutation,
undo, document mutation, window-title and status reporting, and the
command-palette route remain application-owned; `MenuBar` owns only the
unchanged label, availability, and click dispatch.
`EditExportSubtreeContext` exposes only current-selection availability and one
open-dialog callback. Selected-object inspection, suggested-name calculation,
dialog buffers and state, dialog rendering, file selection, undo, document
mutation, status reporting, and the actual export remain application-owned;
`MenuBar` owns only the unchanged item presentation.
`EditBreakInstanceContext` exposes only instance-selection availability and one
action callback. Selection/type inspection, definition lookup and object
replacement, undo, document and selection mutation, window-title and status
reporting, and the command-palette route remain application-owned; `MenuBar`
owns only the unchanged label, availability, and click dispatch.
`EditAlignSelectionContext` exposes only selection and Align-to-First
availability, one axis/`EditAlignmentTarget` callback, and one Align-to-First
callback. Bounds calculation, selection and lock state, undo, document
mutation, window-title updates, and the command-palette Align-to-First route
remain application-owned; `MenuBar` owns the unchanged labels, order,
separators, availability, and click dispatch. Bounds are now calculated only
for the clicked axis and only after a click, instead of all three axes whenever
the submenu is open.
`EditDistributeSelectionContext` exposes only 2+-selection availability and one
axis callback. Selection copying and sorting, endpoint and spacing calculation,
object-lock checks, undo, document mutation, and window-title updates remain
application-owned; `MenuBar` owns only the unchanged three labels, order,
availability, and click dispatch.
`EditDropToGroundContext` exposes only current-selection availability and one
action callback. Geometry-specific bottom-offset calculation, selection and
object-lock state, undo, document mutation, window-title and status reporting,
and the command-palette route remain application-owned; `MenuBar` owns only the
unchanged label, availability, and click dispatch.
`EditSnapSelectionToGridContext` exposes only current-selection availability
and one action callback. Selection and object-lock state, grid spacing,
rounding, undo, document mutation, window-title updates, and status reporting
remain application-owned; `MenuBar` owns only the unchanged label,
availability, and click dispatch.
`EditMirrorSelectionContext` exposes only current-selection availability and
one axis callback. Selection and object-lock state, scale mutation, undo,
document mutation, window-title updates, and status reporting remain
application-owned; `MenuBar` owns only the unchanged submenu and child labels,
order, availability, and click dispatch.
`EditGroupScaleContext` exposes only 2+-selection availability and one
open-dialog callback. Selection, dialog state and contents, scale factor,
lock-aware transformation, undo, document mutation, window-title and status
reporting, and the macro-execution route remain application-owned; `MenuBar`
owns only the unchanged label, availability, and click dispatch.
`EditLinearArrayContext` exposes only current-selection availability and one
open-dialog callback. Selection, dialog state and array parameters, duplication
algorithm, undo, document and selection mutation, window-title and status
reporting, and the command-palette and macro-execution routes remain
application-owned; `MenuBar` owns only the unchanged label, availability, and
click dispatch.
`EditScatterAlongCurveContext` exposes only current-selection availability and
one open-dialog callback. Selection, dialog state and scatter parameters,
line/arc placement, jitter, duplication, undo, document and selection mutation,
window-title updates, and status reporting remain application-owned; `MenuBar`
owns only the unchanged label, availability, and click dispatch.
`EditBatchRenameContext` exposes only current-selection availability and one
open-dialog callback. Selection, dialog state and pattern buffer, live preview,
rename algorithm, object-lock state, undo, document mutation, window-title and
status reporting, and the keyboard, hierarchy-menu, command-palette, and
macro-execution routes remain in their existing owners; `MenuBar` owns only the
unchanged label, shortcut, availability, and click dispatch.
`EditFindReplaceNamesContext` exposes only one open-dialog callback. Dialog
state, find/replace buffers and options, live preview, scene traversal,
selection and object-lock state, rename algorithm, undo, document mutation,
window-title and status reporting, and the keyboard and command-palette routes
remain in their existing owners; `MenuBar` owns only the unchanged label,
shortcut, always-enabled presentation, and click dispatch.
`EditRandomizeTransformContext` exposes only current-selection availability and
one open-dialog callback. Selection, dialog state and position/rotation/scale
ranges, random-number generation, object-lock state, transform mutation, undo,
document mutation, window-title and status reporting, and the command-palette
route remain in their existing owners; `MenuBar` owns only the unchanged label,
availability, and click dispatch.
`EditMacroRecordingContext` exposes only the recording flag plus Start and Stop
callbacks. Macro step storage, action capture, recording lifecycle, playback,
macro context, macro-editor dialog state, and status reporting remain in their
existing owners; `MenuBar` owns only the unchanged labels, active-recording
presentation, and click dispatch.
`EditPlayMacroContext` exposes only availability and one playback callback.
Macro step storage, playback implementation, macro context, all invoked command
effects, and the macro-editor dialog state remain in their existing owners;
`MenuBar` owns only the unchanged label, availability, and click dispatch.
`EditMacroEditorContext` exposes only one open-dialog callback. Dialog state,
macro steps, recording and playback operations, macro context, file buffer,
save/load behavior, and status reporting remain in their existing owners;
`MenuBar` owns only the unchanged label and click dispatch.
`EditLockSelectionContext` exposes only current-selection availability and one
toggle callback. Selection state, `Editor::ObjectLockState` mutation, and the
Ctrl+L keyboard path remain in their existing owners; `MenuBar` owns only the
unchanged label, shortcut, availability, and click dispatch.
`EditResetTransformContext` exposes only current-selection availability and a
reset-target callback for Position, Rotation, Scale, or All. Selection state,
`Editor::ObjectLockState` checks, undo, transform mutation, document updates,
and the Alt+G/R/S keyboard paths remain in their existing owners; `MenuBar`
owns only the unchanged submenu, labels, shortcuts, separator, availability,
and click dispatch.
`EditTransformClipboardContext` exposes only copy/paste availability and one
callback for each action. `Editor::TransformClipboard` state, source-object
selection, locked-object filtering, undo, transform mutation, document and
title updates, status reporting, and the Ctrl+Shift+C/V keyboard paths remain
in their existing owners; `MenuBar` owns only the unchanged labels, shortcuts,
availability, and click dispatch.
`EditIsolateSelectionContext` exposes only activation state, availability, and
one toggle callback. Isolation state, the saved visibility map, scene
visibility traversal, selection, undo, document and title updates, status
reporting, and the Alt+I keyboard path remain in their existing owners;
`MenuBar` owns only the unchanged dynamic labels, shortcut, availability, and
click dispatch.
`EditHideSelectionContext` exposes only current-selection availability and one
hide callback. Selection state, object visibility mutation, selection clearing,
undo, document and title updates, and the H keyboard path remain in their
existing owners; `MenuBar` owns only the unchanged label, shortcut,
availability, and click dispatch.
`EditShowAllHiddenContext` exposes only one show-all callback. Scene traversal,
object visibility mutation, undo, document and title updates, macro playback,
and the Alt+H keyboard path remain in their existing owners; `MenuBar` owns
only the unchanged label, shortcut, always-enabled presentation, and click
dispatch.
`FileMergeSceneContext` exposes only one open-dialog callback. Dialog state and
buffers, source-file loading, document merging, undo, title and status updates,
and error handling remain in their existing owners; `MenuBar` owns only the
unchanged label and click dispatch.
`FileExportSelectionContext` exposes only current-selection availability and one
open-dialog callback. Selection state, dialog state and buffers, export
implementation, document and error handling remain in their existing owners;
`MenuBar` owns only the unchanged label, availability, and click dispatch.
`FileExportGltfContext` exposes only one open-dialog callback. Saved-file
validation, output-path derivation, GLB/glTF export settings, dialog state and
buffers, export implementation, document and error handling, and the Ctrl+E
keyboard route remain in their existing owners; `MenuBar` owns only the
unchanged label, shortcut, and click dispatch.
`FileExportObjContext` exposes only one open-dialog callback. Saved-file
validation, output-path derivation, dialog state and buffers, OBJ export
implementation, document and error handling remain in their existing owners;
`MenuBar` owns only the unchanged label and click dispatch.
`FileSaveContext` exposes only one save callback. Current-file state, the
Save-As fallback, document validation and persistence, backups, autosave
cleanup, recent-files updates, title and status updates, error handling, and
the Ctrl+S keyboard route remain in their existing owners; `MenuBar` owns only
the unchanged label, shortcut, and click dispatch.
`FileSaveAsContext` exposes only one open-dialog callback. Current-file state,
dialog buffers and state, path selection, document validation and persistence,
title and status updates, error handling, and the Ctrl+Shift+S keyboard route
remain in their existing owners; `MenuBar` owns only the unchanged label,
shortcut, and click dispatch.
`FileImportObjContext` exposes only one open-dialog callback. Dialog buffers
and state, OBJ-file validation, mesh object creation, selection, undo, document
and title updates, status reporting, and error handling remain in their
existing owners; `MenuBar` owns only the unchanged label and click dispatch.
`FileNewContext` exposes only one new-scene callback. Unsaved-change handling,
pending-action state, document replacement, selection and undo initialization,
title and status updates, and the Ctrl+N keyboard route remain in their
existing owners; `MenuBar` owns only the unchanged label, shortcut, and click
dispatch.
`FileOpenContext` exposes only one open-file callback. Unsaved-change handling,
pending-action state, dialog buffers and state, file loading, document
replacement, selection and undo initialization, title and status updates, error
handling, and the Ctrl+O keyboard route remain in their existing owners;
`MenuBar` owns only the unchanged label, shortcut, and click dispatch.
`ViewPostProcessingContext` exposes only the application-computed text-shader
capability, Bloom/SSAO value snapshots, and setters. The application retains
the capability check, effect state, CNA targets/effects, and rendering; the UI
component preserves the supported/unsupported text, labels, ranges, widths,
`AlwaysClamp` flags, and callback dispatch.
`FileOpenRecentContext` exposes only current availability, a lazy recent-files
provider, one open callback, and one clear callback. Recent-file storage and
persistence, unsaved-change handling, pending-action state, file loading,
document replacement, title/status updates, and error handling remain
application-owned; `MenuBar` owns the unchanged submenu label, numbered hidden
IDs, filename labels, full-path tooltips, separator, disabled state, and click
dispatch.
`FileExitContext` exposes only one exit callback. Unsaved-change handling,
pending-action state, application shutdown, all platform lifecycle work, and
the Escape fallback route remain in their existing owners; `MenuBar` owns only
the unchanged label and click dispatch.

### Ordered Phase 13 follow-up queue

The sequence below is planning only. Per `CLAUDE.md`, each item must be
described and explicitly confirmed immediately before implementation; finishing
one item does not authorize the next.

1. **Post-menu boundary audit — complete.** The top-level `MenuBar` remains
   an application-owned compositor intentionally: it constructs the narrow
   contexts and owns only the top-level menu structure, so extracting it would
   create a broad god-context. Its camera-preset, projection-toggle, and
   conditional look-through-camera candidate is now complete as
   `Application::UI::CameraPresetOverlay`; camera mutation, document-camera
   lookup, selection, and rendering remain application-owned.

2. **First post-overlay follow-up — complete.** The cursor-following gizmo
   drag-delta display is now `Application::UI::GizmoDragOverlay`. The
   application computes a read-only transform/snap snapshot; axis colors,
   signed delta, units, and snap presentation are component-owned.

3. **Second post-overlay follow-up — complete.** The top-right scene summary
   is now `Application::UI::StatsOverlay`. The application computes its
   read-only scene, selection, camera, and renderer-statistics snapshot; ImGui
   presentation is component-owned.

4. **Third post-overlay follow-up — complete.** The measurement ruler's 2D
   presentation is now `Application::UI::MeasurementOverlay`. The application
   retains measurement state and world-to-screen projection, passing only
   projected points, values, and viewport bounds to the component.

5. **Fourth post-overlay follow-up — complete.** The bottom bar is now
   `Application::UI::StatusBar`; application code constructs its notification
   or scene-summary snapshot and retains the validation-panel transition.

### Final post-overlay audit (2026-07-25)

No additional safe narrow extraction remains. `drawDialogs()` is composed of
stateful workflows with their own buffers, transitions, undo/document effects,
and keyboard routes; `drawPanelSplitters()` owns global panel geometry; and
`drawShadowDebugOverlay()` carries a live renderer texture token plus shadow
light lookup. Splitting any one without a new, broader ownership design would
create a god-context or relocate application state into UI. The next work should
be a separately scoped subsystem, not another mechanical `Overlays.cpp` slice.

### Current authorized work

- **SYS-W14-05:** complete in commit `7e93b92`: embedded external/inline GLB
  support accepts self-contained triangle GLBs only, with a 64 MiB /
  300,000-triangle ceiling; MC3 materials remain authoritative.
- **SYS-W14-06:** complete in commit `c4665af`: CSG has generated smooth
  normals/UVs, child-material glTF primitives when the root does not override
  them, valid CSG-root XML serialization for `uv_mapping` (the generic
  MCB/JSON object layouts already retained it), and the live preview shares
  the new geometry/UV path. `SYS-W14-34` subsequently added the same
  child-material split to the live preview.
- **AUD-042:** commit `20a5473` replaces Android's forced `SDL_RENDERER` with
  GLES/EASYGL and adds the CMake-level regression test. The full Release build
  and 149/149 non-render + 36/36 render partitions pass; the absent NDK and
  sibling `sharp-runtime` cross-compile failure prevent an APK/device claim.
- **Next autonomous starting point:** install/provide a working Android NDK
  after the sibling `sharp-runtime` Android errors are resolved, then run an
  arm64-v8a configure/build, package through SDLActivity/Gradle, and smoke-test
  the editor on a device/emulator. Keep broad alternate-backend and `SYS-W3-01`
  work deferred by the recorded user priority.

### Tracked work that is not implementation-ready

- **SYS-W8-05:** broad alternate-backend support is intentionally postponed
  while the current W14 feature work has priority; it still needs the
  appropriate backend environment and CNA owner coordination.
- **AUD-042:** Android is a supported product target and now has a viable
  GLES/EASYGL source selection; it remains blocked on the Android NDK,
  sibling `sharp-runtime` cross-build repair, packaging, and device validation.
- **Deferred, decision-dependent work:** `SYS-W5-03` retains its documented
  human decision. `SYS-W14-14` is no longer deferred; it is implemented.
  `SYS-W14-05`/`06` are no longer deferred.

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
- **No new features without asking first.** Per `CLAUDE.md`'s workflow:
  describe the task and get explicit confirmation before implementing
  anything, one item at a time. The 2026-07-18 session's entire 10-fix
  sequence, this session's (2026-07-20) `AUD-082`-`088` sequence, and the
  2026-07-19 session's tasks 1-2 (each individually confirmed before
  implementing), all followed that pattern. The 2026-07-19 session's
  tasks 3-5, and this session's `AUD-082`-`088` implementation itself,
  were each carried out WITHOUT a per-task re-confirmation — but only
  because the user explicitly, in-session, authorized continuing
  autonomously through an ALREADY-DESCRIBED, already-filed list; that
  authorization does not extend to inventing NEW tasks not already
  described and filed.
- **Don't trust a stale doc's claims at face value.** The 2026-07-18
  session's own audit found the *prior* session's "backlog exhausted"
  claim was accurate on build/test health but missed 12 real findings —
  re-derive from source when in doubt, especially for anything doc-only.

## 10. Resume prompt

```
Read NEXT.md first. Review the current status and the user’s latest
priority before changing code. Do not start a newly invented task without
explicit authorization. Keep all local build and test commands at four CPU
jobs or fewer, update the planning documents after material work, and do not
push unless the user explicitly asks.
```
