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
- Load `.mc3.xml` on startup; save (Ctrl+S); save-as; export GLB via `mc3togltf` (Ctrl+E)
- 3D scene rendering: box, sphere, cylinder, cone, plane, extrude (as placeholder)
- Grid renderer with XYZ axis colours
- Orbit / pan / zoom camera (middle-drag, right-drag, scroll); reset (F)
- SpriteBatch UI: toolbar, left hierarchy panel, right properties panel, status bar
- Keyboard shortcuts: Q/G/R/S tools, F1–F5 add primitives, Delete, Ctrl+A, arrow nudge, F12 help
- Window title shows tool / file / modified state; F11 saves `screenshot.ppm`
- Ray-cast object picking (left-click, AABB slab method, recursive through children, Ctrl+click multi-select)
- Bitmap font labels in both panels (object names, POS/ROT/SCL values, material name)
- Hierarchy panel: recursive tree, depth indentation, ">" / "v" expand/collapse for groups
- Properties panel: shows selected object's name, POS/ROT/SCL with inline editing (click field → type → Enter to apply)
- Transform gizmo: X/Y/Z arrows when Move tool (G) active; drag handle moves object along axis
- Automated smoke test via `ctest`
- F1–F5 adds primitive as child when a group is selected; Ctrl+D duplicates selected object(s)

**Not working / incomplete:**
- CSG objects (Union, Difference, Intersection) rendered as bounding-box placeholder only
- No file-open dialog — path entered via console stdin
- CSG children not rendered (Union/Difference/Intersection show placeholder box)

---

## 3. Open bugs / limitations

| # | Status | Description |
|---|--------|-------------|
| 1 | **open** | File-open dialog not available — user types path in terminal stdin |
| 2 | **done** | Properties panel is read-only; POS/ROT/SCL cannot be typed in |
| 3 | **open** | CSG Union/Difference/Intersection rendered as placeholder box |
| 4 | **done** | F1–F5 always adds primitive to top-level `document_.objects`, not inside selected group |
| 5 | **open** | No undo/redo (Ctrl+Z / Ctrl+Y) |
| 6 | **done** | No duplicate object (Ctrl+D) |

---

## 4. Architecture notes

**Main modules:**

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` | Game loop, input, scene state, UI draw |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid lines via `BasicEffect` + `VertexBuffer` |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Renders MC3 scene objects + transform gizmo |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom; produces view+projection matrices |
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
- `hierarchyRows_` is populated in `Draw()` and consumed in `Update()` (1-frame lag — intentional, invisible to user).

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

1. **CSG rendering** *(next priority)*
   - Goal: Union/Difference/Intersection display as the union of their children's meshes (even if just rendered children without boolean ops).
   - Files: `SceneRenderer.cpp` — `drawObject()` CSG cases currently fall through to placeholder box.

2. **Undo/redo (Ctrl+Z / Ctrl+Y)**
   - Goal: command pattern to revert the last edit.
   - Deferred until core editing is more stable.

---

## 7. Do not do yet

- **No refactor of CNA** — only fix what's blocking a build.
- **No SpriteFont / native text widget** — bitmap font approach is sufficient for now.
- **No GUI file dialog** — out of scope until a basic widget model exists.
- **No undo/redo** — needs a command pattern; deferred until core editing is stable.
- **No API changes in `Mc3Document`** without checking `mc3togltf` and all test scenes.
- **No mass include path changes** in `CMakeLists.txt` — risks triggering a full CNA recompile.

---

## 8. Resume prompt

```
Read NEXT.md first. Then implement the next task from section 6. Do not refactor unrelated code. Build with: cd cmake-build-debug && ninja -j$(nproc). Verify with: ./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/test.ppm && ffmpeg -i /tmp/test.ppm /tmp/test.png -y && check the PNG. Update NEXT.md after finishing.
```
