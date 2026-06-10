# NEXT.md — MeshCraft Handoff Document

---

## 1. Project Summary

**MeshCraft** is a 3D scene editor for the `.mc3.xml` format — a custom XML-based scene
description used by the OpenEggbert project. Scenes contain primitives, extrusion shapes,
CSG operations, groups, instances, lights, cameras, textures, and environment settings.
The output pipeline exports to `.glb` via the `mc3togltf` converter.

**Main goal:** A fully usable desktop editor where a developer can build, edit, and export
`.mc3.xml` scene files without hand-editing XML.

**Current phase:** ~95 % of planned features implemented. All object types, all extrude
paths/cross-sections, full light/camera/material/texture editors, drag-and-drop hierarchy,
and XML round-trip tests are complete. Remaining work is pivot rendering, innerRadius
hollow extrude, definitions panel, and CSG boolean evaluation.

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
| **Extrude** | All 5 path types (Line/Arc/Helix/Polyline/Bezier), all 4 cross-sections (Rect/Circle/Polygon/Custom), twist, segments, caps, innerRadius (UI + serialization) |
| **CSG** | Union / Difference / Intersection + isCutter flag; gizmo overlays |
| **Group** | Create, expand/collapse, drag-and-drop reparenting, Ungroup |
| **Instance** | Definition picker, material override |
| **Mesh** | URI field (viewport shows placeholder box) |
| **Area** | Exists in scene graph; no special viewport representation |
| **Transform** | Position / Rotation (XYZ Euler °) / Scale — DragFloat3 |
| **Deform** | Non-uniform geometry scale (separate from transform.scale) |
| **visible / collision / tags** | All editable in Properties panel |
| **Lights** | Ambient / Directional / Point / Spot — all params incl. castShadows, falloff |
| **Cameras** | Perspective + Orthographic — position/target/rotation override/near/far/fov/orthoSize |
| **Environment** | backgroundColor, backgroundTexture, fog (Linear/Exponential + all params) |
| **Textures** | URI, wrapU/V (Repeat/Clamp/Mirror), filter (Linear/Nearest), colorSpace, mipMaps |
| **Materials** | Full inline editor: baseColor RGBA, metallic, roughness, emissive, alphaMode, alphaCutoff, normalScale, occlusionStrength + 5 texture slots |
| **Serialization** | All fields listed above round-trip through XML (verified by 54 ctests) |

### ⚠️ Partially covered / bugs

| Issue | Detail |
|---|---|
| **Extrude innerRadius not rendered** | `Mc3CrossSection.innerRadius` is editable in the UI and serializes correctly, but `drawExtrudeDynamic` sweeps a solid profile — hollow pipes/tubes render solid. Fix: generate two concentric profile rings and connect with quads |
| **Mesh viewport preview** | Mesh objects render as grey placeholder box regardless of `meshSource` URI. Fix would require a runtime OBJ/GLB loader in the editor (large scope) |

### ❌ Not implemented

| Feature | Detail | Effort |
|---|---|---|
| **Definitions panel** | Definitions can be referenced by Instance objects (picker combo works), but there is no panel to create / list / edit / delete definitions. They can only exist in a file that was loaded with definitions already present. | ~3–5 h |
| **CSG boolean mesh evaluation** | Union/Difference/Intersection render children individually. Actual boolean mesh ops need an external library (e.g. manifold or CGAL). | ~20–30 h + library |
| **Actions / States animation** | `Mc3Document.TODO: actions map` and `Mc3Object.TODO: states, actions` — not modelled in the data layer yet. | Large scope |
| **Texture rendering in viewport** | All objects render with flat material color. Textures from `baseColorTexture` etc. are never sampled. | ~10–15 h |
| **Multi-selection transform** | When multiple objects are selected the gizmo and Properties panel show/operate only on the first selected object. | ~3 h |

---

## 4. Recent Changes

| Commit | Change |
|-----------|-------------------------------------------------------------------------|
| pending   | Pivot rendering: apply T(-pivot)*S*R*T(pos+pivot) in objectWorldMatrix; Pivot DragFloat3 in Properties |
| `23f3534` | Extrude edge overlay: traces actual sweep wireframe (rings + spines) for all path types |
| `8f742db` | XML round-trip unit tests: 54 checks (visible, deform, extrude all paths, CSG, isCutter, groups) |
| `e792e6d` | Drag-and-drop reparenting in hierarchy; drop onto node = last child, drop on footer = root |
| `27eb2b1` | Extrude path rendering: Arc, Helix, Polyline, Bezier, Custom cross-section — runtime sweep mesh |
| `77451d6` | Edge overlay: black wireframe lines over all visible objects; Alt+W / View menu / toolbar Edges button |
| `7a6057b` | CSG visualization: Add menu, hierarchy badges, properties panel, viewport gizmos |
| `771e6e1` | Extrude editor in properties panel; fix extrude XML serialization |
| `097f19b` | Textures tab (left panel); fix texture serialization in XML writer |
| `73403cd` | Area / Mesh / Instance types; type-specific properties sections |
| `de89c2b` | Light and camera gizmos in viewport; Deform section in properties |
| `8284603` | Cameras tab in left panel |
| `5080c1b` | Environment tab in left panel |
| `d9b7f7e` | Lights panel in left panel |
| `dad11b0` | Full material editor in properties panel |
| `20a784f` | Geometry section (primitive parameters) in properties panel |
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
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Tracks selected `shared_ptr<Mc3Object>` |
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
- **Pivot transform formula** (not yet applied):
  `world = T(-pivot) * S * R * T(pos + pivot)` — see `mc3togltf/src/GltfExporter.cpp:328`.

### Boundaries to preserve
- **No CNA changes without owner permission.** Another Claude Code instance handles CNA.
- Do not add `${meta-gl_SOURCE_DIR}/include` to `CMakeLists.txt` — triggers full CNA recompile.
- Do not change `Mc3Document` public API without checking `mc3togltf` and all test XMLs.
- Do not refactor `MeshCraftApplication.cpp` structurally — it is large but functional.

---

## 6. Useful Commands

```bash
# Configure (first time only — add -DFETCHCONTENT_UPDATES_DISCONNECTED=ON if no network)
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL

# Build (touch workaround required each time)
touch cmake-build-debug/CNA_dep/SHARP_RUNTIME/libSHARP_RUNTIME.a cmake-build-debug/CNA_dep/libCNA.a
cmake --build cmake-build-debug --target MeshCraft -- -j$(nproc)

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

### Bugs / correctness

~~**G — Pivot rendering** — DONE~~
Applied `world = T(-pivot) * S * R * T(pos + pivot)` in `objectWorldMatrix()`; Pivot DragFloat3 added in Properties panel.

**H — Extrude innerRadius rendering** ⚡ Easy win
`drawExtrudeDynamic` ignores `Mc3CrossSection.innerRadius` — hollow pipes render solid.
Fix: for Circle/Polygon cross-sections with `innerRadius > 0`, generate an inner profile ring
and connect outer/inner rings with quads (no caps, or annular caps).
**Files:** `SceneRenderer.cpp` (`drawExtrudeDynamic`, `makeProfile`)
**Effort:** ~2 h

### New features

**I — Definitions panel**
There's no way to create or edit reusable object definitions from the editor UI.
Add a **Defs** tab (or sub-panel) showing `document_.definitions`, with Add/Remove buttons
and an inline tree editor for the definition's root object.
**Files:** `MeshCraftApplication.cpp` (left panel tabs)
**Effort:** ~3–5 h

**J — Multi-selection transform**
When multiple objects are selected, the move/rotate/scale gizmo operates on the first
selection only. Extend to apply the same delta to all selected objects.
**Files:** `MeshCraftApplication.cpp` (gizmo drag handlers)
**Effort:** ~2–3 h

### Large scope / future

**C — CSG boolean mesh evaluation**
Actual Union / Difference / Intersection mesh computation. Requires an external geometry
library (e.g. manifold or CGAL). Large scope; out of phase for now.
**Effort:** ~20–30 h + library integration

**K — Texture rendering in viewport**
All objects render with flat material color. Requires loading image files referenced by
texture URIs and sampling them in the shader (or tinting by UV). Depends on CNA Texture2D API.
**Effort:** ~10–15 h

**F — Actions / States animation data model and editor**
Out of scope for current phase.

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
