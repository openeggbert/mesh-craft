# NEXT.md — MeshCraft Handoff Document

_Last updated: 2026-06-14 (session implementing E7–E8, F2–F8, G8, H1, H4, H5, H15, MCB format)_

---

## 1. Project Summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` format — a lightweight XML-based scene description used by the OpenEggbert project. The editor provides a Dear ImGui UI with orbit camera, object hierarchy, properties panel, timeline/animation, gizmos, CSG boolean operations, extrude/path geometry, and material editing.

**Main goal:** Full-featured authoring tool for `.mc3.xml` scenes, analogous to a stripped-down Blender for the MC3 format.

**Current phase:** Active feature implementation — ~70 of 100 planned tasks done; working through the backlog in plan.md.

**Key architectural decisions:**
- Built on CNA (XNA-like C++ runtime: SDL3 + OpenGL ES 3). **CNA source must not be modified.**
- `Mc3Document` (in `mc3/` sublibrary) is the data model. **Its public API must not change without checking `mc3togltf` and all test XMLs.**
- `MeshCraftApplication` is split across ~12 `.cpp` files by concern (Mouse, Keyboard, FileOps, Commands, Ui*, Anim, WalkMode).
- New `.cpp` files require cmake reconfigure (`file(GLOB_RECURSE)` is used).
- A new `mcb/` static library provides binary MCB format serialization; `mc3tomcb` CLI converts between formats.

---

## 2. Current Status

### Build
- **All targets build cleanly** on Linux x86-64 with GCC/Clang.
- `ninja MeshCraft` — OK (no errors, no warnings treated as errors).
- `ninja mc3tomcb` — OK.
- `ninja Mcb` — OK.

### Tests
- **Smoke test passes:** `bash test/smoke_test.sh ./MeshCraft test/house.mc3.xml` → PASS (headless screenshot, ~2.4 MB PPM).
- **MCB round-trip verified:** `mc3tomcb features.mc3.xml features.mcb && mc3tomcb features.mcb out.mc3.xml` — OK. MCB is ~27% smaller than XML on the features scene.
- No automated unit test suite for `MeshCraftApplication` commands yet (L3 is pending).

### Available binaries (in `cmake-build-debug/`)
- `MeshCraft` — the editor
- `mc3tomcb/mc3tomcb` — CLI converter mc3.xml ↔ .mcb
- `mc3/mc3togltf/mc3togltf` (separate build) — export to GLB/GLTF

### Recently implemented (this session, on branch `develop`)
- **E7** Named layers, **E8** Export subtree as template
- **F2** Mesh source browse, **F3** Export selection, **F4** Merge scene
- **F5** Configurable auto-save, **F6** Rotating backups, **F7** Headless screenshot, **F8** GLB export settings UI
- **G8** Procedural LOD (3 LOD tiers by camera distance for all round primitives)
- **H1** Proportional editing (Gaussian falloff), **H4** Scatter along curve
- **H5** Move Pivot independently from geometry (gizmo at pos+pivot, pos compensated with d·R−d)
- **H15** First-person walk mode (F5 toggle, gravity, jump with Ctrl, mouse look)
- **MCB binary format** (`mcb/` library + `mc3tomcb` CLI): self-describing keyed binary, smaller than XML, compression flag reserved

### What does not work yet
- No HDRI/fog/background texture rendering in viewport (I1–I3 pending)
- No shadow maps, SSAO, bloom (I5–I7)
- No customizable keyboard shortcuts (H9)
- No macro recorder (H12)
- Preferences dialog (H7) is partially done (auto-save interval only, via Help menu)
- Material preview sphere (D7) not implemented
- Walk mode floor collision is flat y=0 only — no collision with scene objects
- Walk mode mouse sensitivity can feel off at high frame rates (no frame-rate-independent mouse cap)
- `H15` walk mode entry from the View menu shows the toggle correctly but does not exit via Esc if ImGui captures keyboard

---

## 3. Recent Changes

### Files added
- `mcb/CMakeLists.txt`, `mcb/include/MeshCraft/Mcb/McbFormat.hpp`, `McbWriter.hpp`, `McbReader.hpp`
- `mcb/src/McbWriter.cpp`, `mcb/src/McbReader.cpp`
- `mc3tomcb/CMakeLists.txt`, `mc3tomcb/src/main.cpp`
- `src/MeshCraft/MeshCraftApplication_WalkMode.cpp`

### Files modified (significant)
- `CMakeLists.txt` — added `add_subdirectory(mcb)`, `add_subdirectory(mc3tomcb)`, linked `Mcb` to MeshCraft
- `include/MeshCraft/MeshCraftApplication.hpp` — added walk mode state, pivot edit mode, `drawWalkModeHud`, `resetPivot`
- `src/MeshCraft/MeshCraftApplication.cpp` — walk mode Update/Draw integration, pivot proxy for gizmo
- `src/MeshCraft/MeshCraftApplication_Mouse.cpp` — H1 proportional editing, pivot edit mode gizmo drag
- `src/MeshCraft/MeshCraftApplication_Commands.cpp` — H4 scatter along curve, H5 resetPivot, E8 export subtree
- `src/MeshCraft/MeshCraftApplication_UiProperties.cpp` — H5 Pivot toggle + Reset button
- `src/MeshCraft/MeshCraftApplication_UiMenuBar.cpp` — Walk Mode menu item (F5)
- `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` — MCB open/save support in file dialogs
- `src/MeshCraft/MeshCraftApplication_FileOps.cpp` — F3/F4/F6/F8, MCB route

### Bugs fixed
- MCB writer previously wrote all fields including defaults → file 3× larger than XML. Fixed by skipping fields equal to struct defaults; MCB is now smaller than XML.
- G8 LOD: sed substitutions failed silently (literal `\1` in output). Fixed with direct Edit tool.
- F7 most-vexing parse: `App(path(s))` parsed as function declaration. Fixed with `path p{s}`.

---

## 4. Current Blocker / Main Problem

**No hard blocker.** Build is clean, smoke test passes, MCB round-trip passes.

Nearest friction point: the `plan.md` priority list only goes to item 10 (all done). The remaining ~28 tasks have no explicit priority ordering. The natural next candidates are:

- **H7** — Preferences dialog (auto-save, snap, grid, theme) — partially exists (auto-save only)
- **I3** — Fog visualization in viewport (data exists in `Mc3Fog`, not rendered)
- **I4** — Colored point-light sphere gizmos
- **K2** — CSG reorder children (drag-reorder in Properties)
- **D5** — Material export/import as XML snippet

None of these are blocked by external dependencies.

---

## 5. Known Bugs and Limitations

| Status | Issue |
|--------|-------|
| suspected bug | Walk mode Esc exit does not work when `ImGui::GetIO().WantCaptureKeyboard` is true (e.g. when a panel has focus). The "Exit Walk Mode" button in the HUD always works. |
| suspected bug | H5 pivot compensation formula `pos += d·R − d` assumes uniform scale=1. For non-uniform scale the formula differs; objects with non-unit scale will have a small visual shift when moving the pivot. |
| incomplete | H15 walk mode: mouse look uses raw pixel delta without capping — at very high FPS the camera can spin uncontrollably on fast mouse moves |
| incomplete | H15 walk mode: floor collision is only at y=0. No collision with scene geometry. |
| incomplete | H7 Preferences dialog: only auto-save interval is stored. Snap defaults, grid defaults, and theme are not yet persisted. |
| incomplete | F7 headless screenshot: always writes PPM. No PNG/JPEG output despite the `--screenshot` flag name suggesting image formats. |
| incomplete | MCB format: compression flag (`MCB_FLAG_COMPRESSED = 0x01`) is reserved in the header but throws if set — not implemented. |
| needs verification | MCB round-trip with `features.mc3.xml` produces correct XML but field ordering differs from original (maps use `std::map` insertion order). Not a correctness issue but makes diff noisy. |
| technical debt | `MeshCraftApplication.hpp` has grown large (~500 lines of member declarations). Should be split, but see §9. |
| technical debt | `UiMenuBar.cpp` has repeated `walk`/`collect` lambdas (L4). |
| unknown | Walk mode interaction with "Look through camera" mode — both can be active simultaneously which would conflict on view matrix. |

---

## 6. Architecture Notes

### Main modules

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` + siblings | Game loop, input routing, scene state, all UI |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer*.cpp` (4 files) | Renders MC3 primitives, gizmos, extrude, builders |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit camera; view+projection matrices |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Tracks selected `Mc3Object` shared_ptrs |
| `TransformGizmo` | `include/MeshCraft/Editor/TransformGizmo.hpp` | Axis drag state machine |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; XML load/save (no graphics dep) |
| `Mcb` | `mcb/` sublibrary | MCB binary format read/write; depends only on `Mc3` |
| `mc3tomcb` | `mc3tomcb/` | CLI tool; links `Mc3` + `Mcb` |
| CNA | `../cna/` sibling repo | SDL3 window, GL ES 3 context, SpriteBatch, BasicEffect, XNA-style API |

### Data flow
1. `Mc3Document::loadFromFile(path)` or `Mcb::loadFromFile(path)` → `document_` (in-memory scene)
2. `MeshCraftApplication::Draw()` → `SceneRenderer::draw(document_, view, proj, selection)` → GL draw calls
3. Mouse/keyboard events → `handleMouseInput` / `handleKeyboardShortcuts` → mutate `document_` → push undo stack
4. `document_.saveToFile(path)` or `Mcb::saveToFile(document_, path)` → disk

### Transform math (pivot)
`world = T(−pivot) × Scale × Rotate × T(position + pivot)`

World position of local origin = `position + pivot − Rotate(Scale(pivot))`.
For Scale=I: `position + pivot − Rotate(pivot)`.
Moving pivot by `d` while keeping geometry fixed: `position += d·R − d` (row-major, R = rotation matrix).

### Key invariants / boundaries
- **No CNA source changes** — CNA is a shared dependency owned by another instance.
- **No `${meta-gl_SOURCE_DIR}/include` in CMakeLists** — triggers full CNA recompile.
- **No `Mc3Document` public API changes** without auditing `mc3togltf` and all `test/*.mc3.xml` files.
- New `.cpp` files: always run `cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON` before building.
- Anonymous namespaces: helpers in one `.cpp` are invisible to all other TUs.
- MCB format versioning: current `MCB_VERSION = 1`. Increment if format changes break backward compat.

---

## 7. Useful Commands

```bash
# Configure (after adding new .cpp files or first time)
cd cmake-build-debug
cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON

# Build editor
ninja MeshCraft

# Build MCB CLI
ninja mc3tomcb

# Build all
ninja

# Run editor
./MeshCraft ../test/house.mc3.xml

# Headless screenshot (no window)
./MeshCraft --screenshot /tmp/out.ppm ../test/house.mc3.xml

# Smoke test
bash ../test/smoke_test.sh ./MeshCraft ../test/house.mc3.xml

# MCB round-trip
./mc3tomcb/mc3tomcb ../test/features.mc3.xml /tmp/f.mcb
./mc3tomcb/mc3tomcb /tmp/f.mcb /tmp/f_rt.mc3.xml

# Export to GLB (separate build)
cd ../mc3togltf && mkdir -p build && cd build
cmake .. && make -j4
./mc3togltf ../../test/house.mc3.xml /tmp/house.glb
```

---

## 8. Next Smallest Tasks

In approximate priority order (no formal ordering beyond this):

1. **I3 — Fog visualization in viewport**
   - Goal: render `Mc3Fog` (linear/exponential) in the GLSL shader as a post-effect or per-fragment blend
   - Files: `SceneRenderer.cpp`, possibly shader strings embedded in `SceneRenderer`
   - Verify: open `features.mc3.xml` (which has fog), see fog gradient in viewport

2. **I4 — Colored point-light sphere gizmos**
   - Goal: draw a small sphere at each `Mc3Light::position` colored by `Mc3Light::color`
   - Files: `SceneRenderer.cpp` (`drawLightGizmos`), `SceneRenderer_Gizmos.cpp`
   - Verify: add a point light in the scene, see colored sphere gizmo

3. **H7 — Complete Preferences dialog**
   - Goal: expose snap translate/rotate/scale defaults, grid cell size, and a dark/light theme toggle in a persistent preferences file (`~/.config/meshcraft/prefs.json` or similar)
   - Files: new `MeshCraftApplication_Prefs.cpp`, `MeshCraftApplication.hpp`
   - Verify: change snap distance, restart editor, setting persists

4. **K2 — CSG reorder children in Properties**
   - Goal: drag-reorder child objects inside a CSG node's Properties tab (affects boolean operation order)
   - Files: `MeshCraftApplication_UiProperties.cpp`
   - Verify: create Union with 3 children, drag-reorder them, scene updates

5. **D5 — Material export/import as XML snippet**
   - Goal: right-click material → "Export as XML…" saves a `<material>` element; "Import from XML…" merges it
   - Files: `MeshCraftApplication_Commands.cpp`, `UiLeftPanel.cpp`
   - Verify: export material from one scene, import into another

6. **E5 — Hierarchy filter by multiple criteria**
   - Goal: extend the existing hierarchy search bar to support filtering by type (Box, Sphere…), tag, and material simultaneously
   - Files: `MeshCraftApplication_UiLeftPanel.cpp`
   - Verify: filter by `type=Sphere AND tag=red`, only matching objects visible

7. **H14 — Proportional scale from group center**
   - Goal: when scaling multiple selected objects, use the group bounding-box center as the pivot instead of each object's own pivot
   - Files: `MeshCraftApplication_Mouse.cpp` (scale gizmo drag section)
   - Verify: select 3 objects, scale them, they converge/diverge from their shared center

8. **L3 — Unit tests for commands**
   - Goal: add a headless test binary that exercises batch rename, array duplicate, and find-replace without a display
   - Files: new `test/commands_test.cpp`, update `CMakeLists.txt`
   - Verify: `ninja commands_test && ./commands_test` exits 0

---

## 9. Do Not Do Yet

- **No refactor of `MeshCraftApplication.hpp`** — it is large but splitting it touches every `.cpp` file and risks merge conflicts with ongoing feature work.
- **No CNA source changes** — owned by a separate Claude Code instance; any change there could break the shared dependency.
- **No `Mc3Document` public API changes** — would require auditing `mc3togltf` and all test XMLs; do separately with full audit.
- **No SSAO / bloom / shadow maps** (I5–I7) yet — require render-target infrastructure that does not exist; scope is large.
- **No MCB compression implementation** — the flag is reserved in the header; implementing zlib would add a dependency. Defer until needed.
- **No mass cleanup of `UiMenuBar.cpp`** (L4) until feature work stabilizes — the duplicated lambda pattern is verbose but harmless.
- **No keyboard shortcut system** (H9) until at least a few more UI features land — adding it now would require retrofitting all existing shortcuts.
- **No macro recorder** (H12) — requires a command-pattern architecture refactor; do after L1/L2 extract tasks.
- **Do not push to `master`** directly — work on `develop`, merge only when stable.

---

## 10. Resume Prompt

```
Read NEXT.md first to understand the project state and constraints.
Then read only the files relevant to the first task listed in section 8.
Do not refactor unrelated code and do not change Mc3Document's public API.
Make one small, verified improvement — implement exactly one task from section 8.
After building successfully (ninja MeshCraft), confirm the change works as described in the "Verify" line of that task.
Finally, update NEXT.md: mark the task done in section 8, update section 2 (Current Status) and section 3 (Recent Changes), and adjust section 4 (Current Blocker) if needed.
```
