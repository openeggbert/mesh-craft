# PLAN.md — MeshCraft Feature Plan & Reference

## What is MeshCraft

**MeshCraft** is a 3D scene editor for the `.mc3.xml` format — a custom XML-based scene
description supporting primitives, extrusion, CSG, instances, lights, cameras, and
environment settings. Output is `.glb` via the `mc3togltf` converter.

**Architecture:**
- Built on **CNA** — an XNA-like C++ framework (SDL3 + OpenGL ES 3.2, EasyGL backend)
- UI panels: `SpriteBatch` + 1×1 white `Texture2D` (white pixel trick)
- Bitmap font: 5×7 px glyphs, `BitmapFont.hpp/cpp`, 96 ASCII characters
- Scene data: `Mc3::Mc3Document` in the `mc3/` sublibrary (pure C++, no graphics)
- CNA as a sibling repo `../cna` included via `add_subdirectory`

---

## Status legend

| Symbol | Meaning |
|--------|---------|
| ✅ | done and working |
| 🔧 | partial — basics work, parts missing |
| 📋 | planned, not started |

---

## File Operations

| Feature | Status |
|---------|--------|
| Load `.mc3.xml` on startup (path via argv) | ✅ |
| New scene (Ctrl+N) | ✅ |
| Save (Ctrl+S) | ✅ |
| Save as (Ctrl+Shift+S) | ✅ |
| Export to GLB via `mc3togltf` (Ctrl+E) | ✅ |
| File-open dialog (GUI) | 📋 |
| Open file — in-UI modal dialog (Ctrl+O) | ✅ |
| Recent files list | 📋 |

---

## 3D Viewport — Rendering

| Feature | Status |
|---------|--------|
| Box, Sphere, Cylinder, Cone, Plane | ✅ |
| Extrude with Line path (Rect→box, Circle/Polygon→cylinder) | ✅ |
| Extrude with Arc, Helix, Polyline, Bezier paths | 🔧 (placeholder box) |
| Extrude with Custom cross-section | 🔧 (placeholder box) |
| CSG Union/Difference/Intersection — render children recursively | ✅ |
| Group — render children recursively | ✅ |
| Instance — resolve from `definitions`, apply transform | ✅ |
| Mesh objects (external geometry) | 🔧 (placeholder box) |
| XYZ grid with axis colours | ✅ |
| Wireframe selection highlight | ✅ |
| Actual CSG boolean mesh evaluation | 📋 |

---

## Camera

| Feature | Status |
|---------|--------|
| Orbit (middle-drag), Pan (right-drag), Zoom (scroll wheel) | ✅ |
| Focus on selection / reset (F) | ✅ |
| Preset views (top / front / side) | ✅ |
| Orthographic mode | 📋 |

---

## Selection

| Feature | Status |
|---------|--------|
| Left-click ray-cast AABB picking (recursive through groups) | ✅ |
| Ctrl+click multi-select | ✅ |
| Ctrl+A — select all / select children of group | ✅ |
| Box/rectangle drag-select | ✅ |

---

## Transform Tools

| Feature | Status |
|---------|--------|
| Move gizmo (G) — axis arrows, drag = translate | ✅ |
| Scale gizmo (S) — flat-square tips, drag = scale per axis | ✅ |
| Rotate gizmo (R) — arc handles, drag = rotate per axis | ✅ |
| Arrow key nudge (Shift = 0.1 step), PageUp/Down = Z axis | ✅ |
| Pivot point options (object centre / world origin / cursor) | 📋 |

---

## Hierarchy Panel (left)

| Feature | Status |
|---------|--------|
| Recursive tree with depth indentation | ✅ |
| Expand/collapse toggle for group types (click triangle) | ✅ |
| Click = select; Ctrl+click = multi-select | ✅ |
| Type colour strip and icon | ✅ |
| Drag-and-drop reparenting | 📋 |
| Visibility toggle (eye icon) | ✅ |

---

## Properties Panel (right)

| Feature | Status |
|---------|--------|
| Object name — click to rename (Enter applies, Esc cancels) | ✅ |
| POS / ROT / SCL — inline editing (click field, type, Enter) | ✅ |
| Material colour swatch display | ✅ |
| Material assignment (pick from list) | 📋 |
| Collision type field | ✅ |
| Visible toggle | ✅ |
| Tags list | ✅ |

---

## Edit Operations

| Feature | Status |
|---------|--------|
| Add primitive (toolbar or F1–F5) | ✅ |
| Delete — removes selection at any depth | ✅ |
| Ctrl+D — deep-copy with `_copy` name suffix | ✅ |
| Undo/Redo Ctrl+Z/Y — 20 steps, deep-copy snapshots | ✅ |
| Cut / Copy / Paste (Ctrl+X/C/V) | ✅ |
| Group selection into a new group | ✅ |
| Ungroup | ✅ |

---

## Materials & Textures

| Feature | Status |
|---------|--------|
| Material colour swatch in properties panel | ✅ |
| Material editor: create / edit / delete `Mc3Material` entries | 📋 |
| Texture assignment | 📋 |
| PBR parameter editing (metalness, roughness, emissive) | 📋 |

---

## Lights & Cameras (scene data)

| Feature | Status |
|---------|--------|
| `Mc3Light` and `Mc3Camera` parsed from XML | ✅ |
| Lights shown in hierarchy and properties | 📋 |
| Light editing (type, colour, intensity) | 📋 |
| Camera editing and preview | 📋 |

---

## Actions & States (animation)

| Feature | Status |
|---------|--------|
| `Mc3Action` / `Mc3State` data model | 📋 |
| Action editor panel | 📋 |
| State machine visualisation | 📋 |

---

## UI & Workflow

| Feature | Status |
|---------|--------|
| Toolbar: tool switching + primitive adding | ✅ |
| Status bar: object count and selection count | ✅ |
| Window title: tool / file / modified state | ✅ |
| F11 — save screenshot.ppm | ✅ |
| F12 — print keyboard shortcuts to console | ✅ |

---

## Developer & Tooling

| Feature | Status |
|---------|--------|
| Automated smoke test via `ctest` | ✅ |
| Auto-screenshot mode (`--screenshot` flag, exits after ~2 s) | ✅ |
| Unit tests for `Mc3Document` serialisation round-trips | 📋 |
| CI pipeline | 📋 |

---

## Architecture — modules

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` | Game loop, input, scene state, UI draw |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid lines via `BasicEffect` + `VertexBuffer` |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Renders MC3 scene objects + transform gizmos |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom/focus; produces view+projection matrices |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Tracks selected `Mc3Object` shared_ptrs |
| `TransformGizmo` | `include/MeshCraft/Editor/TransformGizmo.hpp` | GizmoAxis enum + drag state |
| `BitmapFont` | `include/MeshCraft/Ui/BitmapFont.hpp` | 5×7 font, 96 ASCII glyphs, column-major encoding |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; load/save XML |
| CNA | `../cna/` sibling repo | SDL3 window, GL context, SpriteBatch, BasicEffect |

**Data flow:**
1. `LoadContent()` creates renderers; loads `Mc3Document` from XML.
2. `Update()` polls `Keyboard`/`Mouse`; mutates camera, selection, document.
3. `Draw()` clears → renders 3D scene → draws gizmo → overlays 2D UI via SpriteBatch.

**Important invariants:**
- `Mc3Document.materials` is `std::map<std::string, Mc3Material>` — iterate with `const auto& [key, mat]`.
- `Mc3Material` uses `baseColor` (4-element float array), not `diffuse`.
- `SpriteBatch::Begin()`/`End()` must bracket all 2D `Draw()` calls; cannot nest.
- CNA API: use `getCurrentTechniqueProperty()` and `getPassesProperty()` (old `CurrentTechnique()`/`Passes()` removed in commit 34ae601).
- `Color` has no default constructor — always initialise with 4 args.
- Do not add `${meta-gl_SOURCE_DIR}/include` to MeshCraft's `CMakeLists.txt` — triggers full CNA recompile with pre-existing bugs.
- `hierarchyRows_` and `propFieldHits_` are populated in `Draw()` and consumed in `Update()` (1-frame lag — intentional, invisible to user).

---

## Useful commands

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

## Constraints — do not do yet

- **No refactor of CNA** — only fix what's blocking a build.
- **No SpriteFont / native text widget** — bitmap font approach is sufficient for now.
- **No GUI file dialog** — out of scope until a basic widget model exists.
- **No API changes in `Mc3Document`** without checking `mc3togltf` and all test scenes.
- **No mass include path changes** in `CMakeLists.txt` — risks triggering a full CNA recompile.
