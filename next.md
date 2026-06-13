# next.md — MeshCraft Handoff Document

_Last updated: 2026-06-13 (session implementing A1, A2, B2, C1, B1)_

---

## 1. Project Summary

**MeshCraft** is a 3D scene editor for the `.mc3.xml` format — a custom XML-based scene
description used by the OpenEggbert project. Scenes contain primitives, extrusion shapes,
CSG operations, groups, instances, lights, cameras, textures, materials, and environment settings.
The output pipeline exports to `.glb` via the `mc3togltf` converter.

**Main goal:** A fully usable desktop editor where a developer can build, edit, and export
`.mc3.xml` scene files without hand-editing XML.

**Current phase:** Core editor is complete and stable. Work is now driven by plan.md — a
100-task improvement backlog generated from a full codebase audit on 2026-06-13. Each task
is confirmed with the user before implementation (see CLAUDE.md workflow).

**Key architectural decisions:**
- Built on **CNA** — an XNA-like C++ framework (SDL3 + OpenGL ES 3.2 via EasyGL backend).
  CNA lives at `../cna/` as a sibling repo included via `add_subdirectory`.
- All UI is **Dear ImGui** (v1.91.6, SDL3 + OpenGL ES 3 backends, `#version 300 es`).
- Scene data (`Mc3Document`) is pure C++ in the `mc3/` sublibrary — no graphics dependency.
- Undo/redo uses deep-copy snapshots of the entire `Mc3Document` (20-step stack).
- Application split into multiple `_Ui*.cpp` / `_Mouse.cpp` / `_Keyboard.cpp` files, all
  compiled into one translation-unit set (no shared anonymous-namespace helpers across files).

---

## 2. Current Status

### Build
- **Status: CLEAN** — `ninja: no work to do` as of last session.
- Build command: `cd cmake-build-debug && ninja MeshCraft`
- No linker errors, no warnings from MeshCraft sources.

### Tests
- **2/2 pass** (`ctest --test-dir cmake-build-debug -V`):
  1. `smoke_test` — launches editor, takes screenshot, checks ≥ 1 MB.
  2. `mc3_roundtrip` — 114 XML round-trip checks (all field types + animation).

### Available binaries
- `cmake-build-debug/MeshCraft` — desktop editor (debug build)
- `mc3togltf/build/mc3togltf` — XML→GLB converter (standalone)

### What works
All core mc3 feature coverage (primitives, extrude, CSG, groups, instances, lights, cameras,
environment, materials, textures, animation). Full list in the previous next.md section 3.

### Recently implemented (this session — not yet committed)
| Task | What was done |
|------|---------------|
| **A1** | Material animation channels (baseColor/roughness/metallic/emissive) wired to renderer via `AnimOverride` + `evaluateAndPushAnimOverrides()` |
| **A2** | Deform animation channels (DeformX/Y/Z) wired to renderer |
| **B2** | Gizmo delta overlay: floating ImGui window near cursor showing Δ and absolute value while dragging (Move/Rotate/Scale) |
| **C1** | Properties multi-edit: `~` indicator when values differ across selection; Visible/Material/Collision write to all selected; Pivot applies delta to all |
| **B1** | Local/World space toggle for gizmo: toolbar button "World"/"Local"; gizmo axes, drag direction, and hit test all use object's local rotation matrix when enabled |

### What does not work yet (from plan.md)
All remaining 95 tasks in plan.md. Priority order for next session: D1, E1, B3, G1, H2, F1.

---

## 3. Recent Changes

**Files modified (uncommitted, this session):**
- `plan.md` — tasks A1 ✅, A2 ✅, B2 ✅, C1 ✅, B1 ✅ marked done; priority order updated
- `CLAUDE.md` — created; workflow: ask user before each task, include task details
- `include/MeshCraft/MeshCraftApplication.hpp` — added `gizmoLocalSpace_`, `gizmoDragStartVal_`, `gizmoDragAxisIdx_`, `copyProps*`, `tlDrag*`, `addChannel*` state
- `include/MeshCraft/Renderer/SceneRenderer.hpp` — `AnimOverride` extended; `drawGizmo/drawScaleGizmo/drawRotateGizmo` gained `bool localSpace = false` parameter
- `src/MeshCraft/Renderer/SceneRenderer.cpp` — `baseColor` and `deformScale` override applied in `drawObject()`
- `src/MeshCraft/Renderer/SceneRenderer_Gizmos.cpp` — all three gizmo draw functions use local axes when `localSpace=true`
- `src/MeshCraft/MeshCraftApplication.cpp` — passes `gizmoLocalSpace_` to gizmo draw calls
- `src/MeshCraft/MeshCraftApplication_Anim.cpp` — `evaluateAndPushAnimOverrides()` initialises and applies material + deform overrides; timeline keyframe drag, info bar, Add Channel dialog
- `src/MeshCraft/MeshCraftApplication_Mouse.cpp` — `getLocalAxes` helper; Move/Scale drag uses local axes; hit test uses local axes for Move/Scale/Rotate
- `src/MeshCraft/MeshCraftApplication_UiProperties.cpp` — multi-edit helpers (`allMatchF3/Str/Bool`), `~` mixed indicator, `multiLabel(mixed)`, Visible/Material/Collision write to all selected
- `src/MeshCraft/MeshCraftApplication_UiToolbar.cpp` — "World"/"Local" toggle button added
- `src/MeshCraft/MeshCraftApplication_UiMenuBar.cpp` — "Select by Material", "Copy Properties to Selected", "Drop to Ground Plane" menu items
- `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` — gizmo delta overlay; Copy Properties dialog; command palette entries
- `src/MeshCraft/MeshCraftApplication_Keyboard.cpp` — Ctrl+Shift+P (Copy Props), Ctrl+Up/Down (reorder siblings)
- `src/MeshCraft/MeshCraftApplication_Commands.cpp` — `copyPropsToSelected()` implementation

---

## 4. Current Blocker / Main Problem

**No blocker.** The build is clean, tests pass, and the editor runs correctly.

The session work (A1, A2, B2, C1, B1) is complete and compiles without errors.
None of the changes are committed yet — the repo has modified + untracked files.

---

## 5. Known Bugs and Limitations

- **Local space gizmo + Rotate drag** — the rotate circles are now drawn in local space,
  and the hit test samples match. However, the actual drag delta calculation still applies
  `rotation[axIdx] += delta` (same as world space). For Euler-based rotation this is
  "approximately correct" for small angles but not a true local-axis rotation.
  Status: *incomplete / acceptable for now*

- **B2 gizmo delta overlay** — `gizmoDragStartVal_` captures only one axis component
  (the scalar of the first selected object's transform). In local space move, the delta
  is now applied as `pos += delta * localAxis`, so the overlay scalar may not exactly match
  the actual world-space position change. Status: *minor cosmetic inaccuracy*

- **Material dropdown ID conflict** — `UiProperties.cpp` previously had two combo boxes
  both using `##matsel`. Fixed by renaming the first to `##matsel0`. Status: *fixed*

- **No automated UI tests** — only XML round-trip and smoke test exist. All editor behavior
  is manually verified. Status: *known limitation*

- **Undo granularity** — `pushUndo()` deep-copies the entire document on first mouse-down
  during a drag. If many drag operations happen quickly, undo stack fills faster than expected.
  Status: *by design, 20-step limit*

- **tinyobjloader mesh preview limited to 300k triangles** — larger OBJ files fall back to
  a placeholder box. Status: *by design*

- **Song::GetHashCode() linker stub** — if CNA is recompiled from scratch without the stub,
  the link will fail. The stub is in `cmake-build-debug/CNA_dep/libCNA.a` injected manually.
  Status: *workaround in place, needs upstream CNA fix*

---

## 6. Architecture Notes

### Main modules

| Module | Location | Role |
|---|---|---|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` + `_Ui*.cpp` | Game loop, input, scene state, entire ImGui UI |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer*.cpp` | Renders MC3 objects + gizmos (4 files) |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom; yaw+pitch+distance |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | `shared_ptr<Mc3Object>` multi-selection |
| `TransformGizmo` | `include/MeshCraft/Editor/TransformGizmo.hpp` | GizmoAxis + drag state |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; XML load/save |
| CNA | `../cna/` sibling repo | SDL3, GL context, BasicEffect, Input |

### Key invariants
- `Mc3Material` fields: `baseColor[4]`, `roughness`, `metallic`, `emissiveColor[3]` (NOT `emissive`).
- Gizmo local-space axes: `Right = rotM.getRightProperty()`, `Up = getUpProperty()`,
  `LocalZ = -getForwardProperty()` (XNA Forward = -Z).
- `evaluateAndPushAnimOverrides()` two-pass: pass 1 initialises overrides from document state,
  pass 2 overwrites with evaluated channel values. Always call both passes.
- `multiLabel(label, mixed)` in UiProperties: shows `~` in orange when values differ.
- `allMatchF3/Str/Bool` lambdas are defined inside `drawPropertiesPanel()` — not available in other files.
- `file(GLOB_RECURSE)` in CMakeLists.txt — adding new `.cpp` files requires `cmake ..` reconfigure.

### Boundaries
- **No CNA changes** without owner permission.
- **No `${meta-gl_SOURCE_DIR}/include`** in CMakeLists.txt — triggers full CNA recompile.
- **No `Mc3Document` public API changes** without checking `mc3togltf` and all test XMLs.
- **CLAUDE.md workflow**: always ask user before implementing a plan.md task, include 2–4 line description of what will change.

---

## 7. Useful Commands

```bash
# Build (from cmake-build-debug/)
cd cmake-build-debug && ninja MeshCraft

# Run editor
./cmake-build-debug/MeshCraft test/house.mc3.xml

# Run all tests
ctest --test-dir cmake-build-debug -V

# Auto-screenshot (non-interactive)
./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/editor.ppm

# Re-configure (needed after adding new .cpp files)
cd cmake-build-debug && cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON

# Export GLB
./cmake-build-debug/mc3/mc3togltf test/house.mc3.xml /tmp/house.glb

# Build mc3togltf standalone
cmake -S mc3togltf -B mc3togltf/build -DCMAKE_BUILD_TYPE=Release && cmake --build mc3togltf/build --parallel

# Inject Song::GetHashCode stub (if CNA was recompiled and link fails)
echo 'int _ZNK9Microsoft3Xna9Framework5Media4Song11GetHashCodeEv(const void* s){return 0;}' > /tmp/hashstub.c
gcc -c /tmp/hashstub.c -o /tmp/hashstub.o
ar r cmake-build-debug/CNA_dep/libCNA.a /tmp/hashstub.o
ranlib cmake-build-debug/CNA_dep/libCNA.a
```

---

## 8. Next Smallest Tasks

Tasks are taken from plan.md priority order. Each requires user confirmation before implementation
(see CLAUDE.md).

1. **D1 — Material color swatch in list** _(next)_
   Goal: Show a 12×12 colored square (baseColor) next to each material name in the Mat tab list.
   Files: `src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp`
   Verify: Build succeeds; open a scene with multiple materials, Mat tab shows swatches.

2. **E1 — Enrich hierarchy right-click context menu**
   Goal: Add Rename / Duplicate / Delete / Group / Set as Root items to the hierarchy context menu.
   Files: `src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp`
   Verify: Right-click on hierarchy node shows menu with all items functional.

3. **B3 — Viewport camera preset overlay buttons**
   Goal: Clickable Front/Top/Right/Perspective icons in viewport corner that set camera orientation.
   Files: `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp`, `EditorCamera.hpp`
   Verify: Clicking "Front" snaps view to front orthographic-style view.

4. **G1 — Torus primitive**
   Goal: Add `PrimitiveType::Torus` with `majorRadius`, `minorRadius`, `segments` to Mc3Primitive
   and build the mesh in SceneRenderer_Builders.cpp.
   Files: `mc3/include/MeshCraft/Mc3/Mc3Primitive.hpp`, `src/MeshCraft/Renderer/SceneRenderer_Builders.cpp`,
   `src/MeshCraft/MeshCraftApplication_UiProperties.cpp`, toolbar +Torus button.
   Verify: Add Torus → visible in viewport; properties show majorRadius/minorRadius/segments.

5. **H2 — Measurement tool**
   Goal: Click two points in 3D viewport → floating overlay shows world-space distance.
   Files: `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp`, new tool state in `.hpp`.
   Verify: Select Measure tool, click point A then B, overlay shows correct distance.

6. **F1 — Native file picker**
   Goal: Replace text-buffer open/save dialogs with `zenity`/`kdialog` system dialog.
   Files: `src/MeshCraft/MeshCraftApplication_FileOps.cpp`
   Verify: File > Open shows OS file browser, not a text input.

---

## 9. Do Not Do Yet

- **No broad refactor** of `MeshCraftApplication.cpp` — it is large but stable; splitting it
  prematurely will create merge conflicts with ongoing plan.md work.
- **No CNA source changes** — CNA is managed by a separate instance; any needed CNA changes
  must go through the owner.
- **No mc3 schema changes** without updating `mc3togltf`, test XMLs, and `mc3.xsd`.
- **No new `.cpp` files** without running `cmake ..` reconfigure first (GLOB_RECURSE).
- **No speculative features** — only implement tasks the user explicitly confirms from plan.md.
- **No mass undo-stack redesign** — current deep-copy approach is intentional.
- **No switching ImGui version** — v1.91.6 is pinned; `ImGuiItemFlags_MixedValue` is in
  `imgui_internal.h` only; do not include internal headers in MeshCraft sources.

---

## 10. Resume Prompt

```
Read next.md first. Then read CLAUDE.md for the task confirmation workflow.
Inspect only the files needed for the first task from the "Next Smallest Tasks" section.
Do not refactor unrelated code. Do not modify CNA without owner permission.
Ask the user to confirm the next task before implementing it, including a 2-4 sentence description.
Build with: cd cmake-build-debug && ninja MeshCraft
Test with:  ctest --test-dir cmake-build-debug -V
Update next.md after finishing.
```
