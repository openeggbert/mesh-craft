# NEXT.md — MeshCraft Handoff Document

---

## 1. Project Summary

**MeshCraft** is a 3D scene editor for the `.mc3.xml` format — a custom XML-based scene
description used by the OpenEggbert project. Scenes contain primitives, extrusion shapes,
CSG operations, groups, instances, lights, cameras, and environment settings. The output
pipeline exports to `.glb` via the `mc3togltf` converter.

**Main goal:** A fully usable desktop editor where a developer can build, edit, and export
`.mc3.xml` scene files without hand-editing XML.

**Current phase:** Core editor loop is complete (~70 % of planned features done). The editor
is interactive and usable for basic scene composition. The next phase focuses on material
editing, light/camera support, and polish.

**Key architectural decisions:**
- Built on **CNA** — an XNA-like C++ framework (SDL3 + OpenGL ES 3.2 via EasyGL backend).
  CNA lives at `../cna/` as a sibling repo included via `add_subdirectory`.
- All 2D UI is drawn with `SpriteBatch` + a 1×1 white `Texture2D` (white-pixel trick).
  No widget framework; all panels are hand-drawn rectangles + bitmap font.
- Bitmap font: 5×7 px glyphs, 96 ASCII chars, `BitmapFont.hpp/cpp`, `Ui::drawBitmapText()`.
- Scene data (`Mc3Document`) is pure C++ in the `mc3/` sublibrary — no graphics dependency.
- Undo/redo uses deep-copy snapshots (`deepCopyDoc`) of the entire `Mc3Document`.

---

## 2. Current Status

### Build
- **Builds cleanly** with `ninja MeshCraft` (targeted build).
- Full `ninja -j$(nproc)` sometimes triggers a CNA partial-recompile bug (see §5).

### Tests
- **1/1 smoke test passes** (`ctest --test-dir cmake-build-debug -V`).
- Smoke test: launches editor with `test/house.mc3.xml --screenshot`, checks output file
  is non-empty (≥ 1 MB). Exits after ~2 s automatically.

### Available binaries
- `cmake-build-debug/MeshCraft` — the editor
- `cmake-build-debug/mc3/mc3togltf` — XML→GLB converter

### What works
- Load/Save/Save-As/Export (all via in-UI modal dialogs — no stdin dependency)
- Add primitives (Box/Sphere/Cylinder/Cone/Plane), Delete, Duplicate (Ctrl+D)
- Cut/Copy/Paste (Ctrl+X/C/V) with clipboard
- Group (Ctrl+G) / Ungroup (Ctrl+Shift+G)
- Undo/Redo (Ctrl+Z/Y, 20 steps, deep-copy snapshots)
- Move/Scale/Rotate gizmos (G/S/R) with per-axis drag
- Arrow-key nudge (Shift = 0.1 step, PageUp/Down = Z axis)
- Orbit/Pan/Zoom camera; F = focus on selection
- Preset views: Num1=Front, Num3=Right, Num5=Back, Num7=Top, Num9=Bottom
- Left-click ray-cast picking; Ctrl+click multi-select; Ctrl+A select-all
- Box drag-select (left-drag in Select tool, Ctrl = additive)
- Hierarchy panel: tree, expand/collapse, eye icon (visibility toggle)
- Properties panel: name, POS/ROT/SCL, material swatch, VIS toggle, COL field, TAG field
- Object rename, inline transform editing (click field → type → Enter)
- Status bar: object count + selection count; window title reflects tool/file/modified
- F11 screenshot, F12 help to console
- Extrude with Line paths rendered (Arc/Helix/Polyline/Bezier show placeholder box)
- CSG Union/Difference/Intersection render children recursively (no boolean evaluation)
- Instance objects resolve from `definitions` and apply transform

### What does not work yet
- Material editor (no create/edit/delete of `Mc3Material` entries)
- Material assignment in properties panel (swatch displays, but can't change)
- Light and camera objects: parsed from XML but not shown in hierarchy or editable
- Extrude with Arc/Helix/Polyline/Bezier/Custom paths (renders as placeholder box)
- Mesh objects (external geometry, renders as placeholder box)
- Actual CSG boolean mesh evaluation (shapes rendered, not evaluated)
- Orthographic camera mode
- Drag-and-drop reparenting in hierarchy
- Actions/States animation data model and editor
- CI pipeline
- Unit tests for `Mc3Document` XML round-trips

---

## 3. Recent Changes

| Commit | Change |
|--------|--------|
| `c63a3ba` | Save As (Ctrl+Shift+S) now uses in-UI modal dialog (was stdin) |
| `0d7826d` | Open file (Ctrl+O) now uses in-UI modal dialog (was stdin) |
| `d426632` | Box drag-select in 3D viewport (Select tool); blue rect overlay |
| `2dc1a1a` | Preset camera views on Numpad 1/3/5/7/9 |
| `4c0b70e` | Group (Ctrl+G) and Ungroup (Ctrl+Shift+G) |
| `7d8f1b0` | Tags field in properties panel (comma-separated, inline edit) |
| `5bbec3c` | Hierarchy eye icon per row (click to toggle visible) |
| `aaf7d52` | Cut/Copy/Paste (Ctrl+X/C/V) with deep-copy clipboard |
| `4671b78` | Visible toggle (VIS) and Collision field (COL) in properties panel |
| `b756bdc` | Rotate gizmo (R tool): 3 coloured circles, tangential drag |

**Modified files (recent session):**
- `src/MeshCraft/MeshCraftApplication.cpp` — all of the above
- `include/MeshCraft/MeshCraftApplication.hpp` — state for new features
- `src/MeshCraft/Renderer/SceneRenderer.cpp` — scale + rotate gizmo draw
- `include/MeshCraft/Renderer/SceneRenderer.hpp` — gizmo declarations
- `PLAN.md` — status table kept up to date
- `NEXT.md` — this file

---

## 4. Current Blocker / Main Problem

**No blocking bug.** The editor builds, tests pass, and all implemented features work.

The next meaningful work is the **material editor** — currently the most impactful missing
feature for practical use. `Mc3Material` entries exist in `document_.materials` (a
`std::map<std::string, Mc3Material>`) but there is no UI to create, edit, or assign them.
Objects reference materials by name string (`sel0->material`) but the only feedback is a
colour swatch in the properties panel when the material already exists in the scene XML.

---

## 5. Known Bugs and Limitations

- **CNA partial-recompile bug** *(confirmed)* — `ninja -j$(nproc)` may trigger recompilation
  of `ModelMeshPart.cpp` / `ModelMeshPartCollection.cpp` in CNA with a missing `NOXNA` macro,
  causing a build failure. Workaround: always use `ninja MeshCraft` (targeted build).
  Do not add `${meta-gl_SOURCE_DIR}/include` to MeshCraft's `CMakeLists.txt`.

- **Rotate gizmo reference point** *(incomplete)* — the tangent drag uses a fixed reference
  point for the screen radius; for very oblique views the sensitivity can feel off.

- **Box drag-select on initial click** *(minor, by design)* — clicking and then dragging
  will briefly select the object under the cursor before the box-select finalises. The box
  result replaces the selection on release. Acceptable UX trade-off.

- **Extrude / Mesh / CSG placeholders** *(incomplete)* — Arc, Helix, Polyline, Bezier,
  Custom cross-section extrusions, and Mesh objects all render as a placeholder grey box.
  Boolean CSG evaluation (actual mesh subtraction) is not implemented.

- **Lights and cameras in hierarchy** *(incomplete)* — `Mc3Light` and `Mc3Camera` are parsed
  from XML and round-trip through save/load, but are not shown in the hierarchy panel and
  have no properties UI.

- **`Mc3Object` visibility not persisted in XML** *(needs verification)* — the `visible`
  field is stored in the C++ struct but it is unknown whether the XML parser/writer
  round-trips it correctly. Should be tested.

- **No CI pipeline** *(planned)* — tests only run locally.

- **Single smoke test only** *(incomplete)* — the smoke test checks that the window opens
  and a screenshot is non-empty. There are no unit tests for XML serialisation, transform
  maths, or scene operations.

---

## 6. Architecture Notes

### Main modules

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` | Game loop, all input, scene state, entire UI draw |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Renders MC3 objects + translate/scale/rotate gizmos |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid via `BasicEffect` + `VertexBuffer` |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom/focus; yaw+pitch+distance model |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Tracks selected `shared_ptr<Mc3Object>` |
| `TransformGizmo` | `include/MeshCraft/Editor/TransformGizmo.hpp` | `GizmoMode` + `GizmoAxis` + drag state |
| `BitmapFont` | `include/MeshCraft/Ui/BitmapFont.hpp` | 5×7 font, 96 ASCII glyphs, column-major |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; `loadFromFile` / `saveToFile` XML |
| CNA | `../cna/` sibling repo | SDL3 window, GL context, SpriteBatch, BasicEffect, Input |

### Data flow
1. `LoadContent()` — creates renderers, loads `Mc3Document` from XML.
2. `Update()` — polls `Keyboard` / `Mouse`; calls `handleKeyboardShortcuts` + `handleMouseInput`.
3. `Draw()` — clears → 3D scene → gizmo → 2D UI overlay via SpriteBatch.

### Important invariants
- `Mc3Document.materials` is `std::map<std::string, Mc3Material>`. Iterate with `const auto& [key, mat]`.
- `Mc3Material` uses `baseColor` (float[4]), **not** `diffuse`.
- `SpriteBatch::Begin()` / `End()` must bracket all 2D draws; cannot nest.
- `hierarchyRows_` and `propFieldHits_` are **populated in `Draw()`** and consumed in `Update()` (1-frame lag — intentional).
- `fieldSection_` encoding: `-3`=TAGS, `-2`=COL, `-1`=NAME, `0`=POS, `1`=ROT, `2`=SCL.
- CNA API: use `getCurrentTechniqueProperty()` / `getPassesProperty()` (not `CurrentTechnique()`/`Passes()`).
- `Color` has no default constructor — always initialise with 4 args: `Color(r, g, b, a)`.
- Undo snapshots require **deep copy** (`deepCopyObj` recursion) because `Mc3Document` uses `shared_ptr<Mc3Object>` trees.

### Boundaries to preserve
- Do not add `${meta-gl_SOURCE_DIR}/include` to `CMakeLists.txt` — triggers full CNA recompile.
- Do not change `Mc3Document` public API without checking `mc3togltf` and all test XMLs.
- Do not refactor CNA unless it is blocking a build.

---

## 7. Useful Commands

```bash
# Configure (first time only)
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL

# Build (always use targeted form to avoid CNA recompile bug)
cd cmake-build-debug && ninja MeshCraft

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

## 8. Next Smallest Tasks

### Task 1 — Material editor: list panel
**Goal:** Show all materials from `document_.materials` in the properties panel (or a
dedicated section) when no object is selected, or always at the bottom of the right panel.
Each entry: coloured swatch + name label.
**Files:** `src/MeshCraft/MeshCraftApplication.cpp` (drawUi, right panel section)
**Verify:** Load `test/house.mc3.xml`, check that material names appear.

### Task 2 — Material editor: create new material
**Goal:** A "+ MTL" button at the bottom of the material list. Clicking it adds a new
`Mc3Material` with a generated name (e.g. `Mat5`) and default `baseColor = {0.8, 0.8, 0.8, 1}`.
**Files:** `src/MeshCraft/MeshCraftApplication.cpp` (handleMouseInput, drawUi)
**Verify:** Click button, confirm new entry appears in the list.

### Task 3 — Material assignment in properties panel
**Goal:** In the properties panel, clicking the MTL row cycles through available materials
and assigns the clicked one to `sel0->material`.
**Files:** `src/MeshCraft/MeshCraftApplication.cpp` (properties panel click handler)
**Verify:** Select object, click MTL row, confirm swatch colour changes.

### Task 4 — Material baseColor RGBA editing
**Goal:** Clicking a material in the material list opens an inline editor showing 4 numeric
fields (R, G, B, A in 0–1 range) using the existing `drawField` / `activateField` pattern.
**Files:** `src/MeshCraft/MeshCraftApplication.cpp`
**Verify:** Edit R value, confirm swatch colour updates live.

### Task 5 — Lights in hierarchy panel
**Goal:** Show `Mc3Light` entries from `document_.lights` in the hierarchy panel with a
yellow type-colour strip. Clicking selects them.
**Files:** `src/MeshCraft/MeshCraftApplication.cpp` (drawObjList equivalent for lights)
**Verify:** Load a scene with lights, confirm they appear in hierarchy.

### Task 6 — XML round-trip unit test for `visible` field
**Goal:** Add a ctest that creates an `Mc3Object` with `visible = false`, saves to XML,
reloads, and asserts `visible == false`.
**Files:** `test/` or new `mc3/test/` directory, `CMakeLists.txt`
**Verify:** `ctest --test-dir cmake-build-debug -V` all pass.

### Task 7 — Drag-and-drop reparenting in hierarchy
**Goal:** Left-press + drag a hierarchy row to a different row; release inserts the dragged
object as a child of the target. Requires drag-pending state and drop-indicator rendering.
**Files:** `src/MeshCraft/MeshCraftApplication.cpp` (handleMouseInput, drawUi)
**Verify:** Drag Box onto Group, confirm Box appears as child.

---

## 9. Do Not Do Yet

- **No CSG boolean mesh evaluation** — needs a geometry library; large scope.
- **No animation / Actions / States** — out of scope for current phase.
- **No SpriteFont / native text widget** — the 5×7 bitmap font is sufficient.
- **No GUI file-open dialog via OS APIs** — the in-UI modal works; OS dialogs need platform code.
- **No refactor of CNA** — only fix what blocks a build.
- **No changes to `CMakeLists.txt` include paths** — risks triggering full CNA recompile.
- **No API changes in `Mc3Document`** without verifying `mc3togltf` and all test scenes still convert correctly.
- **No mass rename or restructure of `MeshCraftApplication`** — the file is large but functional; split only when a clear module boundary is identified.
- **No speculative abstractions** — the UI is drawn inline; do not extract a widget framework prematurely.

---

## 10. Resume Prompt

```
Read NEXT.md first. Then inspect only the files needed for the first task in section 8.
Do not refactor unrelated code. Make one small, verified improvement. Build with:
  cd cmake-build-debug && ninja MeshCraft
Test with:
  ctest --test-dir cmake-build-debug -V
Update NEXT.md when done (mark the task complete, add it to "Recent changes", update "Next smallest tasks").
```
