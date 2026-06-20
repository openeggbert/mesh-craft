# MeshCraft — Task Plan (100 tasks)

Generated from full codebase audit on 2026-06-13.
Tasks are ordered roughly by impact / dependency within each category.

Legend: ✅ done · 🔧 partial · 📋 planned

---

## A — Animation (critical gaps)

| # | Task | Files |
|---|------|-------|
| A1 ✅ | Wire material property animations to renderer (material.baseColor, roughness, metallic, emissive are serialized and evaluated but not passed to renderer) | MeshCraftApplication_Anim.cpp:61, SceneRenderer.cpp |
| A2 ✅ | Wire Deform animation to renderer (deform.x/y/z channel evaluated but not forwarded to geometry) | MeshCraftApplication_Anim.cpp, SceneRenderer.cpp |
| A3 ✅ | Timeline: multi-select keyframes (Shift+click, drag-box, move/delete group) | MeshCraftApplication_Anim.cpp |
| A4 ✅ | Timeline: mini curve editor — show interpolation curve inside each channel row | MeshCraftApplication_Anim.cpp |
| A5 ✅ | Timeline: duplicate action (copy entire action under a new name) | MeshCraftApplication_Anim.cpp |
| A6 ✅ | Timeline: rename action inline (double-click on name in dropdown) | MeshCraftApplication_Anim.cpp |
| A7 ✅ | Timeline: copy/paste selected keyframes (Ctrl+C/V inside timeline) | MeshCraftApplication_Anim.cpp |
| A8 ✅ | Timeline: scale time (stretch/compress all keyframes of a channel) | MeshCraftApplication_Anim.cpp |

---

## B — Viewport & Gizmo

| # | Task | Files |
|---|------|-------|
| B1 ✅ | Gizmo: Local/World space toggle (toolbar button or shortcut) | SceneRenderer_Gizmos.cpp, TransformGizmo.hpp |
| B2 ✅ | Gizmo: show delta value overlay while dragging ("Δ +2.50 m") | MeshCraftApplication_Mouse.cpp, UiOverlays.cpp |
| B3 ✅ | Viewport: overlay buttons for camera presets (Front/Top/Right clickable icons in viewport corner) | UiOverlays.cpp |
| B4 ✅ | Viewport: one-click perspective/orthographic toggle (button in overlay) | MeshCraftApplication.cpp, EditorCamera |
| B5 ✅ | Viewport: bounding box toggle (show AABB around selected objects) | SceneRenderer.cpp |
| B6 ✅ | Viewport: "Look through selected camera" (use selected Mc3Camera as viewport view) | MeshCraftApplication.cpp |
| B7 ✅ | Snapping: vertex snap (hold Shift while dragging → snap to another object's position) | MeshCraftApplication_Mouse.cpp |
| B8 ✅ | Snapping: surface snap (snap to surface of another object) | MeshCraftApplication_Mouse.cpp |
| B9 ✅ | Viewport: render locked objects with a distinct outline (red/grey border) | SceneRenderer.cpp |
| B10 ✅ | Viewport: polygon statistics (vertex/face count) in stats overlay | UiOverlays.cpp, SceneRenderer.hpp |

---

## C — Properties Panel / Objects

| # | Task | Files |
|---|------|-------|
| C1 ✅ | Properties: multi-edit — when N objects selected, show "—" for differing values; editing writes to all | UiProperties.cpp |
| C2 ✅ | Properties: pivot/origin offset UI (Mc3Transform pivot field or equivalent) | Mc3Transform.hpp, UiProperties.cpp |
| C3 ✅ | Properties: quick-assign material button from Material List to selected objects | UiLeftPanel.cpp |
| C4 ✅ | States: UI editor for Mc3Object states (TODO in Mc3Object.hpp) | Mc3Object.hpp, UiProperties.cpp |
| C5 ✅ | UV Mapping: basic UI (planar/box/sphere projection, scale/offset) | Mc3Object.hpp, UiProperties.cpp |
| C6 ✅ | Properties: show vertex/face count for Mesh objects (from SceneRenderer cache) | UiProperties.cpp, SceneRenderer.hpp |
| C7 ✅ | Properties: show world-space position (computed matrix result) beside local transform | UiProperties.cpp |
| C8 ✅ | Properties: extend K keyframe button to all animatable properties (material, deform) | UiProperties.cpp |
| C9 ✅ | Properties: tabs to separate Transform / Geometry / Material / Animation sections | UiProperties.cpp |
| C10 ✅ | Object: per-object action/state system (Mc3Object TODO) | Mc3Object.hpp |

---

## D — Materials & Textures

| # | Task | Files |
|---|------|-------|
| D1 ✅ | Material list: color swatch (small baseColor square) next to each material name | UiLeftPanel.cpp |
| D2 ✅ | Texture tab: make fields editable (wrapU/V, filter, colorSpace as editable combos, not read-only) | UiLeftPanel.cpp |
| D3 ~~skipped~~ | Material: duplicate material (copy in material list) | UiLeftPanel.cpp |
| D4 ✅ | Material: apply material from list to selection (button "Apply to selection") | UiLeftPanel.cpp |
| D5 | Material: export/import as standalone JSON or XML snippet | MeshCraftApplication_Commands.cpp |
| D6 ✅ | Texture: drag-and-drop URI file from OS into texture field | UiLeftPanel.cpp |
| D7 | Material: material preview (small UV sphere with applied material) | UiLeftPanel.cpp, SceneRenderer |
| D8 ✅ | Material: search filter in material list | UiLeftPanel.cpp |

---

## E — Scene & Hierarchy

| # | Task | Files |
|---|------|-------|
| E1 ✅ | Hierarchy: enrich right-click context menu (Rename, Duplicate, Delete, Group, Set as Root) | UiLeftPanel.cpp |
| E2 ✅ | Hierarchy: type icons next to each row (small Box/Sphere/Group/Light symbols) | UiLeftPanel.cpp |
| E3 ✅ | Hierarchy: color row by first tag | UiLeftPanel.cpp |
| E4 ✅ | Hierarchy: show lock icon on locked objects | UiLeftPanel.cpp |
| E5 | Hierarchy: filter by multiple criteria (tag AND/OR, type, material) | UiLeftPanel.cpp |
| E6 ✅ | Hierarchy: Select Children command (complement to Select Parent P) | MeshCraftApplication_Commands.cpp |
| E7 ✅ | Scene: named layers (visibility groups, without changing hierarchy) | Mc3Document, UiLeftPanel |
| E8 ✅ | Scene: export/import subtree as template (save subtree as new definition) | MeshCraftApplication_Commands.cpp |
| E9 ✅ | Scene: total polycount statistic for entire scene | UiOverlays.cpp or Scene Properties |

---

## F — File I/O & Import/Export

| # | Task | Files |
|---|------|-------|
| F1 ~~skipped~~ | Native file picker — replace text buffers with system dialog (zenity/kdialog or imgui-filebrowser) | MeshCraftApplication_FileOps.cpp |
| F2 ✅ | OBJ import via Mesh object (Browse button for meshSource field) | UiProperties.cpp |
| F3 ✅ | Export selection (export only selected objects to a new MC3 XML file) | MeshCraftApplication_FileOps.cpp |
| F4 ✅ | Merge scene (open second MC3 XML and insert its objects into current scene) | MeshCraftApplication_FileOps.cpp |
| F5 ✅ | Auto-save interval configurable in UI (currently hardcoded 60 s) | MeshCraftApplication.hpp, Settings |
| F6 ✅ | Backups (backup.1, backup.2 on each Save) | MeshCraftApplication_FileOps.cpp |
| F7 ✅ | Export PNG screenshot from command line (headless, no GUI) | main.cpp |
| F8 ✅ | GLB export settings UI (textures embedded/external, quantization) | MeshCraftApplication_FileOps.cpp |

---

## G — New Primitives & Geometry

| # | Task | Files |
|---|------|-------|
| G1 ✅ | Torus primitive (Mc3PrimitiveType::Torus — majorRadius, minorRadius, segments) | Mc3Primitive.hpp, SceneRenderer_Builders.cpp |
| G2 ✅ | Capsule primitive (cylinder + hemisphere caps) | Mc3Primitive.hpp, SceneRenderer_Builders.cpp |
| G3 ✅ | Disk primitive (circle/annulus — outerRadius, innerRadius, segments) | Mc3Primitive.hpp, SceneRenderer_Builders.cpp |
| G4 ✅ | Grid/Ground primitive (flat plane with N×M subdivisions) | Mc3Primitive.hpp, SceneRenderer_Builders.cpp |
| G5 ✅ | Ico-sphere (uniform triangle subdivision) | SceneRenderer_Builders.cpp |
| G6 ✅ | Extrude: spiral path with pitch UI (currently Helix exists but pitch UI is missing) | UiProperties.cpp |
| G7 ✅ | Extrude: star polygon cross-section (N-pointed star) | Mc3Extrude.hpp, SceneRenderer_Extrude.cpp |
| G8 ✅ | Procedural LOD (level-of-detail segments for primitives based on camera distance) | SceneRenderer.cpp |

---

## H — Tools & Workflow

| # | Task | Files |
|---|------|-------|
| H1 ✅ | Proportional editing (transform falloff — influence nearby objects) | MeshCraftApplication_Mouse.cpp |
| H15 ✅ | Walk mode: explore scene in first-person (WASD/arrows = move, Left/Right = yaw, Ctrl = jump, PageUp/Down or mouse = pitch, gravity, configurable person height) | MeshCraftApplication_WalkMode.cpp, MeshCraftApplication.hpp |
| H2 ✅ | Measurement tool (click two points → show distance in overlay) | UiOverlays.cpp, new tool state |
| H3 ✅ | Align to object (align to a specific target object, not just selection average) | MeshCraftApplication_Commands.cpp |
| H4 ✅ | Scatter/Place: distribute copies along a curve | MeshCraftApplication_Commands.cpp |
| H5 ✅ | Pivot: UI to move pivot independently from geometry | UiProperties.cpp |
| H6 ✅ | Panel resize: drag left/right panel width | MeshCraftApplication.cpp |
| H7 | Preferences dialog (autosave interval, snap defaults, grid defaults, theme) | new file |
| H8 ✅ | Undo: show undo stack as list (UI in Edit menu) | MeshCraftApplication.hpp |
| H9 | Keyboard shortcuts: customizable bindings | new file |
| H10 ✅ | Drag-and-drop MC3 file from OS into app window (SDL drop event) | MeshCraftApplication.cpp |
| H11 ✅ | Command palette: search object names in addition to commands | UiOverlays.cpp |
| H12 | Macro recorder (record a sequence of actions, replay) | new file |
| H13 ✅ | Gizmo: snap to fixed angles (15°/45°/90° during rotate with Ctrl) | MeshCraftApplication_Mouse.cpp |
| H14 | Proportional scale from group center (scale around group center, not each object's own pivot) | MeshCraftApplication_Commands.cpp |

---

## I — Environment & Rendering

| # | Task | Files |
|---|------|-------|
| I1 | Environment: render background texture in viewport | MeshCraftApplication.cpp, SceneRenderer |
| I2 | Environment: HDRI skybox (cubemap or equirectangular) | SceneRenderer.cpp |
| I3 | Environment: fog visualization in viewport (data exists, not rendered) | SceneRenderer.cpp |
| I4 | Lighting: colored point-light sphere gizmos (sphere in light color) | SceneRenderer.cpp |
| I5 | Rendering: SSAO (ambient occlusion post-process) | SceneRenderer.cpp |
| I6 🔧 | Rendering: bloom — pipeline běží (FBO+blur+composite, 17 emissive objektů kresleno), ale efekt není viditelný. Prozkoumáno: depth test, FBO format (GL_RGB→GL_RGBA8), lighting disable, blur krok/iterace/strength. Pravděpodobná příčina zatím neodhalena. | MeshCraftApplication.cpp, SceneRenderer.cpp |
| I7 | Rendering: shadow map debug overlay | SceneRenderer.cpp |
| I8 ✅ | Rendering: full wireframe mode toggle for entire scene | SceneRenderer.cpp |

---

## J — Definitions & Instances

| # | Task | Files |
|---|------|-------|
| J1 ✅ | Convert to Definition (selected objects → new Definition + Instance in place) | MeshCraftApplication_Commands.cpp |
| J2 ✅ | Instance: preview definition content in Properties panel | UiProperties.cpp |
| J3 ✅ | Instance: break instance (expand Instance into a copy of its content, removing the link) | MeshCraftApplication_Commands.cpp |
| J4 ✅ | Instance: random variant (each instance randomly picks from N definitions) | Mc3Object, MeshCraftApplication_Commands |

---

## K — CSG Improvements

| # | Task | Files |
|---|------|-------|
| K1 | CSG: real-time preview cache (invalidation works; evaluation can be slow for large trees) | SceneRenderer.cpp |
| K2 | CSG: reorder children in Properties (drag-reorder children of a CSG node) | UiProperties.cpp |
| K3 | CSG: export resulting mesh to OBJ/GLB | MeshCraftApplication_Commands.cpp |
| K4 | CSG: show triangle count of resulting mesh | UiProperties.cpp |

---

## L — Code Quality & Architecture

| # | Task | Files |
|---|------|-------|
| L1 | Extract SceneHierarchyPanel into its own class (TODO in Scene/SceneHierarchyPanel.cpp) | SceneHierarchyPanel.cpp |
| L2 | Extract PropertiesPanel into its own class (TODO in Scene/PropertiesPanel.cpp) | PropertiesPanel.cpp |
| L3 | Unit tests for MeshCraftApplication commands (batch rename, array dup, find-replace) | tests/ |
| L4 | Reduce code duplication in UiMenuBar.cpp (walk/collect lambdas repeated many times) | UiMenuBar.cpp |
| L5 | EditorViewport.cpp: implement TODO stub | EditorViewport.cpp |

---

## Priority order for upcoming sessions

1. **A1** ✅ — Wire material animation to renderer
2. **A2** ✅ — Wire Deform animation to renderer
3. **B2** ✅ — Delta overlay while gizmo dragging
4. **C1** ✅ — Multi-edit properties
4. **B1** ✅ — Local/World gizmo space toggle
5. **D1** ✅ — Material color swatch in list
6. **E1** ✅ — Enrich hierarchy right-click menu
7. **B3** ✅ — Viewport camera preset overlay buttons
8. **G1** ✅ — Torus primitive
9. **H2** ✅ — Measurement tool
10. **F1** ~~skipped~~ — Native file picker

---

## Architecture reference

### Modules

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` | Game loop, input, scene state, UI draw |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid lines |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer*.cpp` | Renders MC3 scene objects + gizmos (split into 4 files) |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom/focus; view+projection matrices |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Tracks selected `Mc3Object` shared_ptrs |
| `TransformGizmo` | `include/MeshCraft/Editor/TransformGizmo.hpp` | GizmoAxis enum + drag state |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; load/save XML |
| CNA | sibling repo | SDL3 window, GL context, SpriteBatch, BasicEffect |

### Key constraints

- **No CNA source changes** without owner permission.
- **No `${meta-gl_SOURCE_DIR}/include`** in CMakeLists.txt — triggers full CNA recompile.
- **No `Mc3Document` public API changes** without checking `mc3togltf` and all test XMLs.
- `file(GLOB_RECURSE)` in CMakeLists.txt — new `.cpp` files require cmake reconfigure (`cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON`).
- Anonymous namespaces are TU-local — helpers in one `.cpp` are not accessible from another even if linked together.

### Useful commands

```bash
# Build
cd cmake-build-debug && ninja MeshCraft

# Run
./MeshCraft path/to/scene.mc3.xml

# Re-configure (needed after adding new .cpp files)
cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON

# Export to GLB
./mc3/mc3togltf scene.mc3.xml scene.glb
```
