# NEXT.md

## 1. Project summary

**MeshCraft** is a 3D scene editor for the `.mc3.xml` format — a custom XML-based scene description that supports primitives (box, sphere, cylinder, cone, plane), extrusion, CSG, instances, lights, cameras, and environment settings.

**Main goal:** a working GUI editor that lets users visually build and edit MC3 scenes, save them back to `.mc3.xml`, and export to `.glb` via the `mc3togltf` converter.

**Key architectural decisions:**
- Built on **CNA** — an XNA-like C++ framework using SDL3 + OpenGL ES 3.2 (EasyGL backend).
- UI panels drawn with `SpriteBatch` + a 1×1 white `Texture2D` (the "white pixel trick").
- Bitmap font: 5×7 pixel glyphs via `BitmapFont.hpp/cpp` — no CNA SpriteFont dependency.
- MC3 scene data lives in `Mc3::Mc3Document` from the `mc3/` sublibrary (pure C++, no graphics).
- CNA is a sibling repo (`../cna`) included via `add_subdirectory`.

---

## 2. Current status

**Build:** succeeds cleanly.
```
cd cmake-build-debug && ninja -j$(nproc)
```

**Tests:** `ctest -V` passes (smoke test: loads scene, screenshots, checks non-empty PPM, ~2.2 s).

**Working features:**
- Load `.mc3.xml` on startup; save (Ctrl+S); save-as (Ctrl+Shift+S); export GLB via `mc3togltf` (Ctrl+E)
- 3D scene rendering: box, sphere, cylinder, cone, plane
- Extrude (Line path): Rect cross-section → box shape; Circle/Polygon → cylinder shape; aligned to x/y/z axis
- CSG (Union/Difference/Intersection) and Group: render children recursively in parent transform space
- Instance objects: resolved from `doc.definitions` map, rendered at instance transform
- Grid with XYZ axis colours
- Orbit (middle-drag), pan (right-drag), zoom (scroll); F focuses camera on selection, resets if nothing selected
- Toolbar: tool buttons (Select/Move/Rotate/Scale) and add-primitive buttons (Box/Sphere/Cylinder/Cone/Plane) respond to mouse clicks and keyboard shortcuts (Q/G/R/S, F1–F5)
- Left hierarchy panel: recursive tree with depth indentation, expand/collapse toggle for groups (click triangle), click to select
- Right properties panel: object name + type colour; POS/ROT/SCL with inline editing (click field → type digits → Enter applies, Escape cancels)
- Material colour swatch in properties panel
- Ray-cast picking (left-click, AABB, recursive through children); Ctrl+click for multi-select
- Transform gizmo: X/Y/Z axis arrows in Move mode (G); drag handle translates along axis
- Ctrl+A selects children of selected group; without group selection selects all top-level objects
- Ctrl+D deep-copies selected object(s) with `_copy` name suffix, inserted after original
- Delete removes selected objects at any depth in the hierarchy
- Arrow keys nudge selected object (Shift = 0.1 step); PageUp/PageDown nudge on Z
- Window title reflects tool / file / modified state; F11 saves `screenshot.ppm`
- Automated smoke test via `ctest`

**Known limitations:**
- No file-open dialog — path typed via console stdin (Ctrl+O)
- Extrude: non-Line paths (Arc, Helix, Polyline, Bezier) and Custom cross-sections show placeholder box
- Mesh objects (external geometry) show placeholder box — loading not implemented
- No undo/redo

---

## 3. Open bugs

| # | Description |
|---|-------------|
| 1 | File-open dialog not available — user types path in terminal stdin |
| 2 | No undo/redo (Ctrl+Z / Ctrl+Y) |
| 3 | Extrude non-Line paths and Custom cross-sections unrendered |
| 4 | Delete only removes top-level children of a group, not deeper descendants when the group itself is not deleted |

---

## 4. Architecture notes

**Main modules:**

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` | Game loop, input, scene state, UI draw |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid lines via `BasicEffect` + `VertexBuffer` |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Renders MC3 scene objects + transform gizmo |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom/focus; produces view+projection matrices |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Tracks selected `Mc3Object` shared_ptrs |
| `TransformGizmo` | `include/MeshCraft/Editor/TransformGizmo.hpp` | GizmoAxis enum + drag state; rendering in SceneRenderer |
| `BitmapFont` | `include/MeshCraft/Ui/BitmapFont.hpp` | 5×7 pixel font, 96 ASCII glyphs, column-major encoding |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; load/save XML |
| CNA | `../cna/` sibling repo | SDL3 window, GL context, SpriteBatch, BasicEffect, Texture2D |

**Data flow:**
1. `LoadContent()` creates renderers; loads `Mc3Document` from XML.
2. `Update()` polls `Keyboard`/`Mouse`; mutates camera, selection, document.
3. `Draw()` clears screen → renders 3D scene → draws gizmo → overlays 2D UI via SpriteBatch.

**Important invariants:**
- `Mc3Document.materials` is `std::map<std::string, Mc3Material>` — iterate with `const auto& [key, mat]`.
- `Mc3Material` uses `baseColor` (4-element float array), not `diffuse`.
- `SpriteBatch::Begin()`/`End()` must bracket all 2D `Draw()` calls; cannot nest.
- CNA API: use `getCurrentTechniqueProperty()` and `getPassesProperty()` (old `CurrentTechnique()`/`Passes()` removed in commit 34ae601).
- `Color` has no default constructor — always initialise with 4 args.
- Do not add `${meta-gl_SOURCE_DIR}/include` to MeshCraft's CMakeLists.txt — triggers full CNA recompile with pre-existing bugs.
- `hierarchyRows_` and `propFieldHits_` are populated in `Draw()` and consumed in `Update()` (1-frame lag — intentional, invisible to user).

---

## 5. Useful commands

```bash
# Configure (first time)
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL

# Build
cd cmake-build-debug && ninja -j$(nproc)

# Run
./cmake-build-debug/MeshCraft test/house.mc3.xml

# Run with auto-screenshot (exits after ~2 s)
./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/editor.ppm

# Run smoke test
ctest --test-dir cmake-build-debug -V

# Convert MC3 to GLB
./cmake-build-debug/mc3/mc3togltf test/house.mc3.xml test/house.glb
```

---

## 6. Next smallest tasks

1. **Object name editing in properties panel**
   - Goal: click the name bar at the top of the properties panel to rename the selected object.
   - Approach: reuse the existing field-edit infrastructure (`fieldActive_`, `fieldBuffer_`), but store/apply a string instead of a float. Add a `nameFieldActive_` bool or extend the section enum with section=-1.
   - Files: `MeshCraftApplication.hpp/cpp` — `drawUi` properties section, `handleMouseInput`, `handleKeyboardShortcuts`.

2. **Status bar info text**
   - Goal: display object count and selection count as text in the status bar (e.g. "12 objects · 2 selected").
   - Files: `MeshCraftApplication.cpp` — `drawUi` status bar section, using `drawBitmapText`.

3. **Undo/redo (Ctrl+Z / Ctrl+Y)**
   - Goal: revert/redo the last document mutation.
   - Approach: before each mutating operation (`addPrimitive`, `deleteSelected`, `duplicateSelected`, field apply, nudge, gizmo drag end) push a copy of `document_` onto an undo stack (max ~20 entries). Ctrl+Z pops and restores; Ctrl+Y re-applies.
   - Files: `MeshCraftApplication.hpp` (undo stack), `MeshCraftApplication.cpp` (push before mutations, handle shortcuts).

---

## 7. Do not do yet

- **No refactor of CNA** — only fix what's blocking a build.
- **No SpriteFont / native text widget** — bitmap font approach is sufficient for now.
- **No GUI file dialog** — out of scope until a basic widget model exists.
- **No API changes in `Mc3Document`** without checking `mc3togltf` and all test scenes.
- **No mass include path changes** in `CMakeLists.txt` — risks triggering a full CNA recompile.

---

## 8. Resume prompt

```
Read NEXT.md first. Then implement the next task from section 6. Do not refactor unrelated code. Build with: cd cmake-build-debug && ninja -j$(nproc). Verify with: ctest --test-dir cmake-build-debug -V. Update NEXT.md when done.
```
