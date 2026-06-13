# NEXT.md — MeshCraft Handoff Document

---

## 1. Project Summary

**MeshCraft** is a 3D scene editor for the `.mc3.xml` format — a custom XML-based scene
description used by the OpenEggbert project. Scenes contain primitives, extrusion shapes,
CSG operations, groups, instances, lights, cameras, textures, and environment settings.
The output pipeline exports to `.glb` via the `mc3togltf` converter.

**Main goal:** A fully usable desktop editor where a developer can build, edit, and export
`.mc3.xml` scene files without hand-editing XML.

**Current phase:** All planned features implemented, including keyframe animation. All object
types, all extrude paths/cross-sections, full light/camera/material/texture/definitions editors,
drag-and-drop hierarchy, pivot rendering, multi-selection gizmo, CSG boolean evaluation, mesh
viewport preview, and XML round-trip tests are complete.

**Key architectural decisions:**
- Built on **CNA** — an XNA-like C++ framework (SDL3 + OpenGL ES 3.2 via EasyGL backend).
  CNA lives at `../cna/` as a sibling repo included via `add_subdirectory`.
- All UI is **Dear ImGui** (v1.91.6, SDL3 + OpenGL ES 3 backends, `#version 300 es`).
  ImGui replaced the previous hand-drawn SpriteBatch UI in commit `8856fba`.
- `BeginDraw()` / `EndDraw()` are virtual overrides on `Game`; ImGui frame wraps them.
- SDL events forwarded to ImGui via `SDL_AddEventWatch` → `ImGui_ImplSDL3_ProcessEvent`.
- Scene data (`Mc3Document`) is pure C++ in the `mc3/` sublibrary — no graphics dependency.
- Undo/redo uses deep-copy snapshots of the entire `Mc3Document` (20-step stack).

---

## 2. Current Status

### Build
- **Builds cleanly** with `cmake --build cmake-build-debug --target MeshCraft -- -j$(nproc)`.
- **Workaround required before each build** (CNA / SHARP_RUNTIME source sync):
  ```bash
  find cmake-build-debug/CNA_dep/CMakeFiles -name "*.o" -exec touch {} \;
  find cmake-build-debug/CNA_dep/SHARP_RUNTIME/CMakeFiles -name "*.o" -exec touch {} \;
  find /rv/data/development/github.com/openeggbert/cna/include -name "*.hpp" -exec touch {} \;
  find /rv/data/development/github.com/openeggbert/sharp-runtime/include -name "*.hpp" -exec touch {} \;
  find /rv/data/development/github.com/openeggbert/sharp-runtime/src -name "*.cpp" -exec touch {} \;
  touch cmake-build-debug/CNA_dep/SHARP_RUNTIME/libSHARP_RUNTIME.a cmake-build-debug/CNA_dep/libCNA.a
  ```
  Must touch all `.o` files, all CNA/SHARP_RUNTIME headers, and SHARP_RUNTIME `.cpp` sources.
  Both CNA and SHARP_RUNTIME have private-member / NOXNA errors in recent commits that break
  recompilation — the workaround makes all build artifacts appear newer than the sources.
- **tinyxml2 duplicate target fix** in `mc3/CMakeLists.txt`: guards against `sharp-runtime`
  also bundling tinyxml2; creates `tinyxml2::tinyxml2` alias when only the bare target exists.

### Tests
- **2/2 tests pass** (`ctest --test-dir cmake-build-debug -V`):
  1. `smoke_test` — launches editor, takes screenshot, checks ≥ 1 MB
  2. `mc3_roundtrip` — 114 XML round-trip checks (all field types + animation)

### Available binaries
- `cmake-build-debug/MeshCraft` — the editor (desktop, debug)
- `mc3togltf/build/mc3togltf` — XML→GLB converter (standalone build; required for File > Export GLB)
- `cmake-build-web/MeshCraft.html` — editor for the browser (requires Emscripten build)

---

## 3. mc3 Format Coverage

### ✅ Fully covered

| Category | What's implemented |
|---|---|
| **File ops** | Load / Save / Save As / Export GLB |
| **Primitives** | Box, Sphere, Cylinder, Cone, Plane — all params (size/radius/height/segments/axis) |
| **Extrude** | All 5 path types (Line/Arc/Helix/Polyline/Bezier), all 4 cross-sections (Rect/Circle/Polygon/Custom), twist, segments, caps, innerRadius hollow tubes (UI + rendering + serialization) |
| **CSG** | Union / Difference / Intersection + isCutter flag; viewport gizmo overlays |
| **Group** | Create, expand/collapse, drag-and-drop reparenting, Ungroup |
| **Instance** | Definition picker, material override |
| **Definitions** | Create/rename/remove via Defs tab; rename propagates to all Instance references |
| **Mesh** | URI field (viewport shows placeholder box) |
| **Area** | Exists in scene graph; no special viewport representation |
| **Transform** | Position / Rotation (XYZ Euler °) / Scale / Pivot — all DragFloat3; pivot applied as T(-pivot)*S*R*T(pos+pivot) |
| **Deform** | Non-uniform geometry scale (separate from transform.scale) |
| **visible / collision / tags** | All editable in Properties panel |
| **Lights** | Ambient / Directional / Point / Spot — all params incl. castShadows, falloff |
| **Cameras** | Perspective + Orthographic — position/target/rotation override/near/far/fov/orthoSize |
| **Environment** | backgroundColor, backgroundTexture, fog (Linear/Exponential + all params) |
| **Textures** | URI, wrapU/V (Repeat/Clamp/Mirror), filter (Linear/Nearest), colorSpace, mipMaps |
| **Materials** | Full inline editor: baseColor RGBA, metallic, roughness, emissive, alphaMode, alphaCutoff, normalScale, occlusionStrength + 5 texture slots |
| **Multi-selection** | Move/Scale/Rotate gizmo applies delta to all selected objects simultaneously |
| **Serialization** | All fields listed above round-trip through XML (verified by 54 ctests) |

### ✅ Fully covered

All originally-planned mc3 features are now implemented and rendered in the viewport.

### ✅ Recently completed

| Feature | Detail |
|---|---|
| **Texture rendering in viewport** | Objects with `baseColorTexture` in their material render with the texture applied. UV+normal geometry built for all unit shapes; lazy texture cache via `loadOrGetTexture()`. Fallback to flat color when no texture. |

### ✅ Recently completed

| Feature | Detail |
|---|---|
| **CSG boolean mesh evaluation** | Union/Difference/Intersection now evaluate actual boolean geometry via the manifold v3 library. Result cached per `Mc3Object*`, invalidated on `pushUndo()`. Falls back to individual child rendering if manifold returns empty. |

### ✅ Done

**F — Keyframe animation** (completed)
Full keyframe animation system: `Mc3Action`, `Mc3Channel`, `Mc3Keyframe` data model in `mc3/`.
Three interpolation modes: **Step**, **Linear**, **CubicBezier** (with per-keyframe tangent handles).
Animated properties: position.x/y/z, rotation.x/y/z, scale.x/y/z, visible (+ deform + material
properties stored/serialised for future renderer wiring).
XML round-trip: `<actions><action><channel><keyframe>` parsed and written by `Mc3XmlParser` /
`Mc3XmlWriter`.
Editor: **Timeline panel** (Ctrl+T / View→Timeline) at the bottom of the viewport — action
dropdown with create/delete, duration, loop toggle, play/pause/stop, time scrubber; per-channel
rows with keyframe circles (click = seek, right-click = delete). **[K] buttons** in the
Properties panel insert keyframes at the current time for Position / Rotation / Scale / Visible.
Space bar = play/pause when an action is selected.
Renderer: `AnimOverride` map evaluated each frame via `evaluateAndPushAnimOverrides()`; injected
into `SceneRenderer` which overrides the effective `Mc3Transform` and visibility per named object.
**Files:** `Mc3Animation.hpp/cpp`, `Mc3Document.hpp`, `Mc3XmlParser.cpp`, `Mc3XmlWriter.cpp`,
`mc3/CMakeLists.txt`, `SceneRenderer.hpp/cpp`, `MeshCraftApplication.hpp/cpp`.
**Test file:** `test/animation_test.mc3.xml` — Bounce (cubic), Spin (linear), Flash (step), Pulse (cubic).
**Schema:** `mc3.xsd` v0.3 — `interpType`, `bezierHandleType`, `mc3KeyframeType`,
`mc3ChannelType`, `mc3ActionType`, `mc3ActionsType`.

---

## 4. Recent Changes

| Commit | Change |
|-----------|-------------------------------------------------------------------------|
| (pending) | Viewport stats overlay: FPS (exp. smoothed), Objects/Visible/Locked counts, Selected count, camera dist+target; top-right of viewport; toggle via View > Stats Overlay |
| `ce64962` | Object locking: Ctrl+L / Edit menu / hierarchy context menu; [L] prefix in hierarchy; locked objects immune to gizmo, nudge and delete; stored as set<id> in editor |
| `d003c49` | Grid cell size control: "Grid" toolbar button; right-click popup with presets 0.25/0.5/1/2/5/10 u + DragFloat; GridRenderer.setSpacing() rebuilds VB keeping 20 u extent |
| `09282bc` | Hide selected (H) / Show all hidden (Alt+H): keyboard shortcuts + Edit menu items; clears selection on hide; both push undo; documented in Keyboard Shortcuts dialog |
| `1b3ce1c` | Snap interval configurator: right-click Snap button → popup with Move/Rotate/Scale presets (0.1–2 u, 5–90°, 0.05–1) + DragFloat custom inputs; active preset highlighted green |
| `e0f27c7` | Unsaved-changes guard: New/Open/Open Recent/Exit prompt "Save / Don't Save / Cancel" when scene is modified |
| `f1320f4` | Auto-save: every 60 s when modified, writes <file>.autosave; cleared on manual save; startup warns if autosave is newer than saved file |
| `421ae9c` | Help > Keyboard Shortcuts dialog: scrollable modal table of all shortcuts grouped by category (File/Edit/Add/View/Tools/etc.) |
| `1a7d06a` | Snap to grid: toolbar Snap button + View menu toggle; Move snaps 0.5 u, Rotate 15°, Scale 0.25 — applied during gizmo drag |
| `332ff97` | Hierarchy search: "Search..." filter box in Scene tab; case-insensitive, hides non-matching subtrees, auto-expands nodes while active, Escape clears |
| `8d0bf97` | Status bar notifications: timed coloured messages for save/open/export results (green success, red error, 2-3 s auto-dismiss) |
| `ba431ca` | Inline rename: double-click or right-click > Rename in hierarchy to edit object name in-place; Enter commits, Escape cancels |
| `d8d1b97` | Recent Files: File > Open Recent submenu, persisted to ~/.config/meshcraft/recent.txt, max 10 entries with tooltips and Clear Recent |
| `b50e2b8` | Web build: build-web.sh + cmake/web/pre.js (IDBFS persistent storage); NEXT.md cleanup |
| `9ab45e5` | exportGltf() binary discovery: add mc3togltf/build/mc3togltf as primary candidate; register mc3togltf_gltf in root ctest pointing to standalone binary |
| (pending) | features.mc3.xml: bump to v0.3, add DemoSpin action (linear rotation + cubic bezier translation); roundtrip test extended to 114 checks |
| `64b59ad` | mc3togltf test suite: gltf_test.py (38 checks — basic gltf/glb conversion, animation export structure, accessor types, dense sampling) registered as ctest mc3togltf_gltf |
| `bc24fd7` | MC3_FORMAT.md updated to v0.3: full `<actions>` reference, animatable properties table with glTF export notes, proposed improvements renumbered |
| `3fb9673` | Animation round-trip tests: 43 new checks covering linear/cubic-bezier/step XML round-trip, multi-action round-trip, and evaluateChannel correctness (97 total) |
| `d8cc59e` | mc3togltf animation export: parse `<actions>` in Mc3XmlParser, emit glTF `animations[]` from Mc3Action channels (translation/rotation/scale with LINEAR/STEP interpolation, cubic bezier sampled at 30fps) |
| `bfa8b19` | Keyframe animation: Mc3Action/Channel/Keyframe data model, XML round-trip, timeline panel, playback engine, [K] buttons in Properties panel |
| `246a80e` | Mesh viewport preview: tinyobjloader FetchContent, loadObjMesh + loadOrGetMesh cache, lit VPNT path; fix manifold 32-bit IB |
| `97a47a4` | Invalidate CSG mesh cache on undo/redo (`clearCsgCache` in `pushUndo`) |
| `eef446e` | CSG boolean mesh evaluation: manifold v3 FetchContent, buildManifoldTree + manifoldToRenderMesh, cache in SceneRenderer |
| `e4ea36b` | mc3 schema v0.2: IDREF refs, rotation_units/euler_order, metadata, uv_mapping child elements |
| `2c3606d` | Texture rendering in viewport: UV+normal VBs for all unit shapes, per-material texture binding, lazy cache |
| `066ffa9` | Definitions panel: Defs tab with Add/Remove, rename (fixes all Instance refs), Name/Type/Transform editor |
| `23d7f23` | Multi-selection gizmo: Move/Scale/Rotate delta applied to all selected objects; fix tinyxml2 duplicate CMake target |
| `92ce20a` | Extrude innerRadius: hollow tube rendering — outer+inner walls, annular caps, inner edge overlay rings |
| `ce87854` | Pivot rendering: T(-pivot)*S*R*T(pos+pivot) in objectWorldMatrix; Pivot DragFloat3 in Properties panel |
| `23f3534` | Extrude edge overlay: trace actual sweep wireframe (rings + spines) for all path types |
| `8f742db` | XML round-trip unit tests: 54 checks (visible, deform, extrude all paths, CSG, isCutter, groups) |
| `e792e6d` | Drag-and-drop reparenting in hierarchy; drop onto node = last child, drop on footer = root |
| `27eb2b1` | Extrude path rendering: sweep mesh for all 5 path types and 4 cross-section types |
| `77451d6` | Edge overlay: black wireframe lines over all visible objects; Alt+W / View menu / toolbar |
| `7a6057b` | CSG visualization: Add menu, hierarchy badges, properties panel, viewport gizmos |
| `8856fba` | Replace hand-drawn SpriteBatch UI with Dear ImGui |

---

## 5. Architecture Notes

### Main modules

| Module | Location | Role |
|----------------------|---------------------------------------------------|----------------------------------------------------------------------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` | Game loop, all input, scene state, entire ImGui UI |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Renders MC3 objects + gizmos (translate/scale/rotate/lights/cameras/CSG) |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid via `BasicEffect` + `VertexBuffer` |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom/focus; yaw+pitch+distance model |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Tracks selected `shared_ptr<Mc3Object>` (supports multi-select) |
| `TransformGizmo` | `include/MeshCraft/Editor/TransformGizmo.hpp` | `GizmoMode` + `GizmoAxis` + drag state |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; `loadFromFile` / `saveToFile` XML |
| CNA | `../cna/` sibling repo | SDL3 window, GL context, BasicEffect, Input |

### Data flow
1. `LoadContent()` — creates renderers, loads `Mc3Document`, inits ImGui.
2. `Update()` — polls `Keyboard` / `Mouse`; handles shortcuts + mouse input.
3. `Draw()` — clears → 3D scene → gizmos → `drawImGuiUi()`.
4. `BeginDraw()` — calls `ImGui::NewFrame()`.
5. `EndDraw()` — calls `ImGui::Render()` + `ImGui_ImplOpenGL3_RenderDrawData()`,
   then optional auto-screenshot, then `Game::EndDraw()`.

### Important invariants
- `Mc3Document.materials` is `std::map<std::string, Mc3Material>`. Iterate with `const auto& [key, mat]`.
- `Mc3Material` uses `baseColor` (float[4]), **not** `diffuse`.
- ImGui panels are fixed-position windows (`NoMove | NoResize | NoBringToFrontOnFocus`).
- Viewport rect = `[kLeftPanelW, imguiTopH_]` to `[W - kRightPanelW, H - kStatusH]`.
- `imguiTopH_` is updated each frame from actual menu bar + toolbar heights.
- `pendingScreenshot_` flag: set in `Draw()`, consumed in `EndDraw()` after ImGui renders.
- CNA API: use `getCurrentTechniqueProperty()` / `getPassesProperty()`.
- `Color` has no default constructor — always init with 4 args: `Color(r, g, b, a)`.
- Undo: call `pushUndo()` before any mutation of `document_` or object data.
- Pivot transform formula (already applied): `world = T(-pivot) * S * R * T(pos + pivot)`.
- `document_.definitions` is `std::map<std::string, shared_ptr<Mc3Object>>`. When renaming a
  definition key, also walk all scene objects and update `Instance.definition` references.

### Boundaries to preserve
- **No CNA changes without owner permission.** Another Claude Code instance handles CNA.
- Do not add `${meta-gl_SOURCE_DIR}/include` to `CMakeLists.txt` — triggers full CNA recompile.
- Do not change `Mc3Document` public API without checking `mc3togltf` and all test XMLs.
- Do not refactor `MeshCraftApplication.cpp` structurally — it is large but functional.

---

## 6. Useful Commands

```bash
# Configure (first time only — add -DFETCHCONTENT_UPDATES_DISCONNECTED=ON if no network)
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL -DFETCHCONTENT_UPDATES_DISCONNECTED=ON

# Build (touch workaround required each time CNA sources changed)
touch cmake-build-debug/CNA_dep/SHARP_RUNTIME/libSHARP_RUNTIME.a cmake-build-debug/CNA_dep/libCNA.a
cmake --build cmake-build-debug --target MeshCraft -- -j$(nproc)

# Full workaround when CNA .o files are also stale
find cmake-build-debug/CNA_dep/CMakeFiles -name "*.o" -exec touch {} \;
touch cmake-build-debug/CNA_dep/SHARP_RUNTIME/libSHARP_RUNTIME.a cmake-build-debug/CNA_dep/libCNA.a

# Run editor with a scene
./cmake-build-debug/MeshCraft test/house.mc3.xml

# Run all tests
ctest --test-dir cmake-build-debug -V

# Auto-screenshot (non-interactive, exits after ~2 s)
./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/editor.ppm

# Convert MC3 to GLB
./cmake-build-debug/mc3/mc3togltf test/house.mc3.xml test/house.glb

# Build mc3togltf standalone (required for File > Export GLB)
cmake -S mc3togltf -B mc3togltf/build -DCMAKE_BUILD_TYPE=Release
cmake --build mc3togltf/build --parallel

# Web/WASM build (requires emsdk; source emsdk_env.sh first)
./build-web.sh
# Then: cd cmake-build-web && python3 -m http.server 8080
# Open: http://localhost:8080/MeshCraft.html
```

---

## 7. What Remains (priority order)

All originally-planned features are now implemented. Remaining items are optional enhancements.

### ✅ Done

**L — Texture rendering in viewport** (completed)
Objects with a `baseColorTexture` in their material now render with the texture applied.
Implementation: `VertexPositionNormalTexture` VBs (with UVs + normals) added to all unit shapes
alongside the existing colored VBs. `drawMeshTextured()` uses BasicEffect `TextureEnabled` + the
lit-texture shader (stride 32, ambient+directional light). `loadOrGetTexture()` lazily loads and
caches textures by absolute path via `Texture2D(path, device)`. Fallback to flat color when no
texture or load error.
**Files:** `SceneRenderer.hpp`, `SceneRenderer.cpp`.

### ✅ Done

**M — Mesh viewport preview** (completed)
`<mesh src="...obj">` objects now load and render in the viewport via tinyobjloader.
Implementation: `loadObjMesh()` (file scope, SceneRenderer.cpp) reads an OBJ file using
the `ObjReader` API, builds a `VertexPositionColor` VB (triangle soup, for fallback) and a
`VertexPositionNormalTexture` VB (with per-vertex or per-face normals, for lit rendering).
Both use 32-bit sequential index buffers. Result cached in `meshCache_` by absolute path.
`drawMeshTextured()` extended to handle `nullptr` texture (disables texture sampling, keeps
lighting). The Mesh case in `drawObject()` always uses the lit VPNT path for loaded files.
Limit: 300k triangles max (skips to placeholder box if exceeded).
**Files:** `SceneRenderer.hpp`, `SceneRenderer.cpp`, `CMakeLists.txt` (tinyobjloader FetchContent).
**Test file:** `test/mesh_test.mc3.xml` + `test/meshes/teapot_minimal.obj`.

**C — CSG boolean mesh evaluation** (completed)
Union/Difference/Intersection now produce real boolean geometry via the manifold v3 library.
Architecture: `buildManifoldTree()` recursively converts the CSG subtree to `manifold::Manifold`
objects in world space (parentWorld baked in), then `manifoldToRenderMesh()` converts the result
to a `VertexPositionColor` `RenderMesh`. The result is cached in `csgMeshCache_` keyed by
`const Mc3Object*` and cleared by `clearCsgCache()` (called from `pushUndo()`).
Falls back to rendering children individually if the manifold result is empty.
**Files:** `SceneRenderer.hpp`, `SceneRenderer.cpp`, `CMakeLists.txt` (manifold FetchContent).
**Test file:** `test/csg_test.mc3.xml` — difference (box−sphere), union (two spheres), intersection (box∩sphere).

### ✅ Done

**F — Keyframe animation** (completed — see section 4 for full notes)
`Mc3Action` / `Mc3Channel` / `Mc3Keyframe` data model; Step/Linear/CubicBezier interpolation;
timeline panel with play/pause/scrub; [K] keyframe buttons in Properties; glTF animation export
via mc3togltf (cubic bezier densely sampled at 30fps). 114 round-trip test checks.

### ✅ Done

**W — Web / WASM build** (completed)
Emscripten build support via `build-web.sh`. Output: `cmake-build-web/MeshCraft.html`.
IDBFS mount in `cmake/web/pre.js` makes `/home/user` persistent across reloads via IndexedDB.
Test files preloaded at `/test`. Run locally with `python3 -m http.server 8080`.
See section 6 for build commands.

---

## 8. Resume Prompt

```
Read NEXT.md first. Then inspect only the files needed for the chosen task.
Do not refactor unrelated code. Do not modify CNA without permission.
Build with:
  touch cmake-build-debug/CNA_dep/SHARP_RUNTIME/libSHARP_RUNTIME.a cmake-build-debug/CNA_dep/libCNA.a
  cmake --build cmake-build-debug --target MeshCraft -- -j$(nproc)
Test with:
  ctest --test-dir cmake-build-debug -V
Update NEXT.md when done.
```
