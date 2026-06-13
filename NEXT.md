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
- **Workaround required before each build** (SHARP_RUNTIME `<algorithm>` / CNA source sync):
  ```bash
  find cmake-build-debug/CNA_dep/CMakeFiles -name "*.o" -exec touch {} \;
  touch cmake-build-debug/CNA_dep/SHARP_RUNTIME/libSHARP_RUNTIME.a cmake-build-debug/CNA_dep/libCNA.a
  ```
  (only needed when CNA sources were updated since last build; plain `.a` touch suffices otherwise)
- **tinyxml2 duplicate target fix** in `mc3/CMakeLists.txt`: guards against `sharp-runtime`
  also bundling tinyxml2; creates `tinyxml2::tinyxml2` alias when only the bare target exists.

### Tests
- **2/2 tests pass** (`ctest --test-dir cmake-build-debug -V`):
  1. `smoke_test` — launches editor, takes screenshot, checks ≥ 1 MB
  2. `mc3_roundtrip` — 54 XML round-trip checks (all field types)

### Available binaries
- `cmake-build-debug/MeshCraft` — the editor
- `cmake-build-debug/mc3/mc3togltf` — XML→GLB converter

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
| (pending) | mc3togltf animation export: parse `<actions>` in Mc3XmlParser, emit glTF `animations[]` from Mc3Action channels (translation/rotation/scale with LINEAR/STEP interpolation, cubic bezier sampled at 30fps) |
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

### Large scope

**F — Actions / States animation** (large scope, out of phase)
`Mc3Document` has no actions/states data model yet. Needs data layer design first.

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
