# NEXT.md — MeshCraft Handoff Document

---

## 1. Project Summary

**MeshCraft** is a 3D scene editor for the `.mc3.xml` format — a custom XML-based scene
description used by the OpenEggbert project. Scenes contain primitives, extrusion shapes,
CSG operations, groups, instances, lights, cameras, textures, and environment settings.
The output pipeline exports to `.glb` via the `mc3togltf` converter.

**Main goal:** A fully usable desktop editor where a developer can build, edit, and export
`.mc3.xml` scene files without hand-editing XML.

**Current phase:** ~90 % of planned editor features are implemented and working. The editor
is fully interactive. The remaining work is advanced geometry (extrude path variants,
actual CSG boolean evaluation), drag-and-drop hierarchy reparenting, and animation data.

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
- **Workaround required before each build** (SHARP_RUNTIME missing `<algorithm>` issue,
  being fixed separately):
  ```bash
  touch cmake-build-debug/CNA_dep/SHARP_RUNTIME/libSHARP_RUNTIME.a cmake-build-debug/CNA_dep/libCNA.a
  ```

### Tests
- **1/1 smoke test passes** (`ctest --test-dir cmake-build-debug -V`).
- Smoke test: launches editor with `test/house.mc3.xml --screenshot`, checks screenshot
  file is non-empty (≥ 1 MB). Exits after ~2 s automatically.

### Available binaries
- `cmake-build-debug/MeshCraft` — the editor
- `cmake-build-debug/mc3/mc3togltf` — XML→GLB converter

### What works

**File operations**
- Load / Save / Save As / Export to GLB — all via in-UI modal dialogs (no stdin)
- Window title reflects tool / filename / modified state

**Scene editing**
- Add primitives: Box / Sphere / Cylinder / Cone / Plane
- Add other types: Area / Mesh / Instance / Extrude
- Add CSG containers: `Add > CSG > Union / Difference / Intersection`
- Delete, Duplicate (Ctrl+D), Cut/Copy/Paste (Ctrl+X/C/V)
- Group (Ctrl+G) / Ungroup (Ctrl+Shift+G)
- Undo / Redo (Ctrl+Z / Ctrl+Y, 20 steps, deep-copy snapshots)

**3D viewport**
- Orbit / pan / zoom camera (mouse); F = focus on selection
- Preset views: Num1=Front, Num3=Right, Num5=Back, Num7=Top, Num9=Bottom
- Left-click ray-cast picking; Ctrl+click = additive; Ctrl+A = select all
- Box drag-select (left-drag in Select tool; Ctrl = additive)
- Move (G) / Scale (S) / Rotate (R) gizmos with per-axis drag
- Arrow-key nudge (Shift = 0.1 step, PageUp/Down = Z axis)
- Light gizmos: direction arrows (Directional), cross + ring (Point), spoke-cone (Spot)
- Camera gizmos: position-to-target line + frustum pyramid
- CSG gizmos: coloured box outlines (green=Union, red=Difference, blue=Intersection)
- Difference: red wireframe overlay on cutter children
- **Edge overlay** (Alt+W / View menu / toolbar): black wireframe lines over all visible objects;
  slight scale push (1.003×) avoids z-fighting; recurses into Groups, Instances, and CSG containers
- F11 screenshot; F12 help to console

**Left panel — tabbed (Scene / Lights / Env / Cam / Tex)**
- **Scene tab**: object tree with expand/collapse, visibility toggle, context menu
  (Duplicate / Delete / Hide-Show); CSG nodes show coloured `[U]`/`[D]`/`[I]` prefix;
  cutter children show `[cut]`; Group shows `[G]`
- **Lights tab**: list of `Mc3Light` entries; add/remove; full properties per type
  (Directional: color + intensity + direction; Point: color + intensity + range;
  Spot: color + intensity + range + inner/outer angle; Ambient: color + intensity)
- **Env tab**: sky color, fog (enable/color/near/far), ambient intensity
- **Cam tab**: list of `Mc3Camera` entries; add/remove; Perspective (fov + near/far)
  or Orthographic (size + near/far) + position/target/up
- **Tex tab**: list of textures from `document_.textures`; add/remove;
  edit URI, filter (Linear/Nearest), wrap (Repeat/Clamp/Mirror), mip-maps, sRGB

**Right panel — Properties**
- Name, ID (read-only), Tags, Collision
- Transform: Position / Rotation / Scale (DragFloat3)
- Visible checkbox
- **CSG section** (Union / Difference / Intersection): type combo (Union/Difference/
  Intersection); for Difference: children checklist to mark cutters
- **Geometry section** (primitives): type-specific params (Box: size; Sphere: radius+segments;
  Cylinder/Cone: radius+height+segments; Plane: width+depth)
- **Extrude section**: twist, path-segments, smooth, caps; Cross-section TreeNode (Rect/
  Circle/Polygon/Custom with point list); Path TreeNode (Line/Arc/Helix/Polyline/Bezier)
- **Mesh Source** (Mesh type): URI field
- **Instance** (Instance type): definition picker combo + material override combo
- **Deform** checkbox + DragFloat3 (non-uniform geometry-level scale)
- **Material editor**: full inline editor — name picker combo, baseColor RGBA,
  metallic / roughness / emissive / alpha-mode / alpha-cutoff / normal-scale /
  occlusion-strength; texture fields for base / metallic-roughness / emissive /
  normal / occlusion

**Serialization**
- All fields round-trip correctly through XML (including extrude, deform,
  textures block, material textures, emissive_color, alpha_cutoff)

---

## 3. Recent Changes

| Commit | Change |
|-----------|-------------------------------------------------------------------------|
| *(staged)* | Extrude path rendering: Arc, Helix, Polyline, Bezier, Custom cross-section — runtime sweep mesh |
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

## 4. Known Bugs and Limitations

- **SHARP_RUNTIME rebuild workaround** — missing `<algorithm>` in SHARP_RUNTIME causes
  rebuild failures. Workaround: `touch` the `.a` files before building. Being fixed
  separately by another Claude Code instance working on CNA.

- **Extrude path visualization** — All path types (Line, Arc, Helix, Polyline, Bezier) and all
  cross-section types (Rect, Circle, Polygon, Custom) now generate a correct sweep mesh at
  runtime via `drawExtrudeDynamic`. Twist and caps are honoured. Edge overlay still uses
  bounding-box approximation for non-Line paths.

- **CSG boolean evaluation not implemented** — Union/Difference/Intersection containers
  render their children individually. No actual mesh boolean operations are performed.
  The visual is informational only.

- **No drag-and-drop reparenting** — objects can only be moved by deleting and re-adding
  as children. Reparenting requires drag state + drop indicator in the hierarchy.

- **`Mc3Object.visible` XML round-trip** — not verified by a unit test; should be confirmed.

- **No CI pipeline** — tests run locally only.

- **Single smoke test** — checks window opens and screenshot is non-empty. No unit tests
  for XML serialisation, transform maths, or scene operations.

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

### Boundaries to preserve
- **No CNA changes without owner permission.** Another Claude Code instance handles CNA.
- Do not add `${meta-gl_SOURCE_DIR}/include` to `CMakeLists.txt` — triggers full CNA recompile.
- Do not change `Mc3Document` public API without checking `mc3togltf` and all test XMLs.
- Do not refactor `MeshCraftApplication.cpp` structurally — it is large but functional.

---

## 6. Useful Commands

```bash
# Configure (first time only)
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL

# Build (touch workaround required each time)
touch cmake-build-debug/CNA_dep/SHARP_RUNTIME/libSHARP_RUNTIME.a cmake-build-debug/CNA_dep/libCNA.a
cmake --build cmake-build-debug --target MeshCraft -- -j$(nproc)

# Run editor with a scene
./cmake-build-debug/MeshCraft test/house.mc3.xml

# Run smoke test
ctest --test-dir cmake-build-debug -V

# Auto-screenshot (non-interactive, exits after ~2 s)
./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/editor.ppm

# Convert MC3 to GLB
./cmake-build-debug/mc3/mc3togltf test/house.mc3.xml test/house.glb
```

---

## 7. What Remains

### High priority

**A — Extrude path geometry rendering** ✅ DONE
All path types and cross-sections now render via `drawExtrudeDynamic` (sweep mesh generated
each frame). Edge overlay still uses bounding-box approximation for non-Line paths.

**B — Drag-and-drop reparenting in hierarchy**
Left-press + drag a hierarchy row onto another to reparent. Needs drag-pending state,
drop indicator rendering, and tree mutation on release.
**Files:** `src/MeshCraft/MeshCraftApplication.cpp` (Scene tab, hierarchy lambda)
**Effort:** ~3–5 h

### Lower priority / future

**C — CSG boolean mesh evaluation**
Actual Union / Difference / Intersection mesh computation. Requires an external geometry
library (e.g. manifold or CGAL). Large scope; out of phase for now.
**Effort:** ~20–30 h + library integration

**D — XML round-trip unit tests**
Add ctests for `visible`, `deform`, `extrude`, `csgOperation`, `isCutter` field round-trips.
**Files:** `mc3/test/` or `test/`, `CMakeLists.txt`

**E — CI pipeline**
GitHub Actions workflow that builds and runs ctest on push.

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
