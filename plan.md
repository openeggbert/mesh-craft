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
| D5 ✅ | Material: export/import as standalone XML snippet — Export saves Mc3Document(material only) to .mc3mat.xml; Import loads + merges with suffix-on-collision; both via popup modal dialogs | MeshCraftApplication_UiOverlays.cpp, MeshCraftApplication.hpp |
| D6 ✅ | Texture: drag-and-drop URI file from OS into texture field | UiLeftPanel.cpp |
| D7 ✅ | Material: preview sphere — 128×128 FBO (BloomGL); SDF Blinn-Phong GLSL shader with u_color/u_roughness/u_metallic uniforms; rendered each frame material is selected; centered ImGui::Image in material panel | MeshCraftApplication.cpp, MeshCraftApplication.hpp, UiLeftPanel.cpp |
| D8 ✅ | Material: search filter in material list | UiLeftPanel.cpp |

---

## E — Scene & Hierarchy

| # | Task | Files |
|---|------|-------|
| E1 ✅ | Hierarchy: enrich right-click context menu (Rename, Duplicate, Delete, Group, Set as Root) | UiLeftPanel.cpp |
| E2 ✅ | Hierarchy: type icons next to each row (small Box/Sphere/Group/Light symbols) | UiLeftPanel.cpp |
| E3 ✅ | Hierarchy: color row by first tag | UiLeftPanel.cpp |
| E4 ✅ | Hierarchy: show lock icon on locked objects | UiLeftPanel.cpp |
| E5 ✅ | Hierarchy: filter by multiple criteria — AND/OR toggle button; tag combo (collects all tags); material combo; layer combo (was E7); updated matchesFilter lambda handles both AND (all criteria) and OR (any active criterion) modes | UiLeftPanel.cpp, MeshCraftApplication.hpp |
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
| H7 ✅ | Preferences dialog — autosave/snap/grid already existed; added: theme selector (Dark/Light/Classic via ImGui::StyleColors*); persistence loadPrefs()/savePrefs() to ~/.config/meshcraft/prefs.ini (key=value INI); loadPrefs() called at startup; savePrefs() on dialog Close | MeshCraftApplication_FileOps.cpp, MeshCraftApplication_UiOverlays.cpp, MeshCraftPrivate.hpp |
| H8 ✅ | Undo: show undo stack as list (UI in Edit menu) | MeshCraftApplication.hpp |
| H9 ✅ | Keyboard shortcuts: customizable bindings — KeyBind struct (ctrl/shift/alt/key); 32 default actions; keybind editor dialog with click-to-capture + kImGuiToXna[] table; Reset to Defaults; INI persistence to ~/.config/meshcraft/keybindings.ini; shortcutFired() replaces direct justPressed() checks in Keyboard.cpp | MeshCraftApplication_Keybindings.cpp (new), MeshCraftApplication_Keyboard.cpp, UiOverlays.cpp, MeshCraftApplication.hpp |
| H10 ✅ | Drag-and-drop MC3 file from OS into app window (SDL drop event) | MeshCraftApplication.cpp |
| H11 ✅ | Command palette: search object names in addition to commands | UiOverlays.cpp |
| H12 ✅ | Macro recorder — record/stop/play/clear; recordStep() hooks in addPrimitive/delete/duplicate/group/ungroup/groupScale/batchRename/linearArray; executeMacroStep() dispatch; save/load .mc3macro (tab-separated lines); Macro Editor dialog (step list, file path, Save/Load); Edit menu Record/Stop/Play/Editor items | MeshCraftApplication_Macro.cpp (new), MeshCraftApplication_Commands.cpp, UiOverlays.cpp, UiMenuBar.cpp, MeshCraftApplication.hpp, MeshCraftPrivate.hpp |
| H13 ✅ | Gizmo: snap to fixed angles (15°/45°/90° during rotate with Ctrl) | MeshCraftApplication_Mouse.cpp |
| H14 ✅ | Group Scale — computes selection center (position average); scales each obj's position from center × factor AND scales obj.scale × factor; dialog with DragFloat + ×2/×0.5/×1 shortcuts; Edit menu "Group Scale…" (enabled ≥2 selected); locked objects skipped | MeshCraftApplication_Commands.cpp, UiMenuBar.cpp, UiOverlays.cpp, MeshCraftApplication.hpp |

---

## I — Environment & Rendering

| # | Task | Files |
|---|------|-------|
| I1 ✅ | Environment: render background texture in viewport — SpriteBatch full-viewport quad before 3D scene; texture cached and reloaded on URI change; clear to backgroundColor when no texture | MeshCraftApplication.cpp |
| I2 ✅ | Environment: equirectangular skybox — GLSL panorama shader (gl_VertexID quad, no depth write), texture cached, drawn before scene | MeshCraftApplication.cpp |
| I3 ✅ | Environment: fog visualization in viewport — per-object color blend (linear: start/end, exponential: density) + BasicEffect FogEnabled for solid pass | SceneRenderer.cpp |
| I4 ✅ | Lighting: colored point/spot light sphere gizmos — point: solid sphere + diamond ray lines; spot: small sphere + cone wireframe | SceneRenderer.cpp |
| I5 ✅ | Rendering: SSAO (ambient occlusion post-process) — depth blit via glBlitFramebuffer + view-space reconstruction + 16-sample hemisphere + 5×5 blur + multiplicative composite; strength & radius sliders in UI | MeshCraftApplication.cpp |
| I6 ✅ | Rendering: bloom — gl_VertexID vertex shader + disable CNA residual GL state (cull-face/stencil/scissor) before composite; strength slider in UI | MeshCraftApplication.cpp, SceneRenderer.cpp |
| I7 ✅ | Rendering: shadow map debug overlay — RGBA+depth FBO 256×256; renders scene from first castShadows directional light (orthographic ±50 m); ImGui "Shadow Frustum" window shows light-view color render; menu toggle in View | MeshCraftApplication.cpp, MeshCraftApplication_UiOverlays.cpp |
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
| K1 ✅ | CSG: real-time preview cache — content-hash (FNV-mix over subtree transforms, primitives, parent matrix) replaces raw-pointer key; `pushUndo()` no longer clears cache; auto-evicts at 128 entries; `clearCsgCache()` called on document load to free memory; also fixes bug where moved parent didn't invalidate child CSG | SceneRenderer.cpp, MeshCraftApplication_Commands.cpp, MeshCraftApplication_FileOps.cpp |
| K2 ✅ | CSG: reorder children in Properties (drag-reorder children of a CSG node) — drag handle ":: name" on each child row; all CSG types (Union/Difference/Intersection); undo-able; Difference keeps isCutter checkbox left of name | UiProperties.cpp |
| K3 ✅ | CSG: export resulting mesh to OBJ — "Export OBJ…" button in CSG Properties; popup dialog with path field; SceneRenderer::exportCsgMesh() builds manifold tree + writes v/vn/f OBJ with per-face normals | SceneRenderer.cpp, UiProperties.cpp, UiOverlays.cpp |
| K4 ✅ | CSG: show triangle count — csgTriCountMap_ (obj.id→int) updated each frame when CSG mesh drawn; csgCachedTriCount() public getter; UiProperties shows "Tris: N" or "— (not yet rendered)" | SceneRenderer.hpp, SceneRenderer.cpp, UiProperties.cpp |

---

## L — Code Quality & Architecture

| # | Task | Files |
|---|------|-------|
| L1 ✅ | Extract SceneHierarchyPanel into its own class — HierarchyCallbacks struct (7 std::function slots); draw(selection, lockedIds, cb) method; moved all hierarchy state (searchBuf, typeFilter, layerFilter, tagFilter, matFilter, filterOr, anchorId, renamingId, renameBuf, renameNeedsFocus, scrollToId, flatOrder); scrollToObject() for command palette; UiLeftPanel Scene tab reduced to 10 lines; UiOverlays uses hierarchyPanel_->scrollToObject() | SceneHierarchyPanel.hpp, SceneHierarchyPanel.cpp, UiLeftPanel.cpp, UiOverlays.cpp, MeshCraftApplication.hpp |
| L2 ✅ | Extract PropertiesPanel into its own class — PropertiesContext struct (12 fields + 8 callbacks); full draw() from UiProperties.cpp (~2000 lines); insertAnimKeyframes signature changed initializer_list→vector; thin wrapper in UiProperties.cpp | PropertiesPanel.hpp, PropertiesPanel.cpp, UiProperties.cpp, MeshCraftApplication.hpp, Anim.cpp |
| L3 ✅ | Unit tests for MeshCraftApplication commands — extracted pure algorithms into `EditorAlgorithms.hpp` (CNA-free); 57 tests across applyRenamePattern / batchRenameObjects / countFindReplaceMatches / applyFindReplaceNames / arrayDuplicateObjects / deepCopyObjectAlg; `Commands.cpp` delegates to free functions; `mc3_commands_test` binary registered as CTest `mc3_commands` | src/MeshCraft/EditorAlgorithms.hpp, mc3/test/editor_commands_test.cpp, mc3/CMakeLists.txt, MeshCraftApplication_Commands.cpp |
| L4 ✅ | Reduce code duplication in UiMenuBar.cpp — 8 repeated std::function tree-walk patterns replaced with two C++23 deducing-this local lambdas: `walkAll(list, fn)` and `selectBy(pred)` | UiMenuBar.cpp |
| L5 ✅ | EditorViewport.cpp: implement TODO stub — added EditorCamera member + camera() accessors; PickRay struct; pickRay(mx,my,vX,vY,vW,vH) method converting pixel coords to world-space ray | EditorViewport.hpp, EditorViewport.cpp |

---

## M — AI & Asset Registry (new feature track)

| # | Task | Files |
|---|------|-------|
| M1 ✅ | mc3.xml `<include file="…"/>` support — parser merges definitions + objects from referenced file; XSD extended; roundtrip test; local override fix (parser erases included IDs after local parse); nested include roundtrip fix (recordIncludes=false for recursive calls); cycle detection test; writer fix (material id from map key) | mc3/src/Mc3XmlParser.cpp, mc3/src/Mc3XmlWriter.cpp, mc3/mc3.xsd, mc3/test/roundtrip_test.cpp, test/ |
| M2 ✅ | Model registry — `ModelRegistry` class wrapping SQLite (`modelregistry.sqlite3`); schema: group/name/variant/tags/xml/description/source (with migration); UI panel in editor (search, insert, save with desc/source fields); registry test suite (mc3_registry_test); atomic temp filenames | include/MeshCraft/ModelRegistry.hpp, src/MeshCraft/ModelRegistry.cpp, mc3/test/mc3_registry_test.cpp, src/MeshCraft/MeshCraftApplication_UiRegistry.cpp, CMakeLists.txt |
| M3 ✅ | AI API integration — `AiAssistant` class (cpp-httplib + Claude API); ImGui panel: scope combo (full scene/selection), prompt, API key, async call; validateAiXml() before apply; aiPendingDoc_ for registry integration; "Save AI to Registry" wires AI result to registry save dialog with source="ai_generated"; atomic temp filenames | src/MeshCraft/AiAssistant.cpp+hpp, src/MeshCraft/MeshCraftApplication_UiAi.cpp, include/MeshCraft/MeshCraftApplication.hpp |

---

## N — mc3.xml Schema Extensions

> Each N task covers: data model extension (`Mc3Document` / new structs), parser (`Mc3XmlParser.cpp`), writer (`Mc3XmlWriter.cpp`), XSD schema (`mc3.xsd`), and basic editor UI.

| # | Task | Files |
|---|------|-------|
| N1 ✅ | SVG texture — `Mc3SvgTexture` struct (id, src, inlineContent); `<texture type="svg" src="…"/>` and inline CDATA form; `doc.svgTextures` map + `addSvgTexture()`; parser routes type="svg", writer emits with SetCData; XSD updated (mixed="true", optional uri, new type/src attrs); 10 roundtrip tests all pass | Mc3SvgTexture.hpp, Mc3Document.hpp, Mc3Document.cpp, Mc3XmlParser.cpp, Mc3XmlWriter.cpp, mc3.xsd, roundtrip_test.cpp |
| N2 ✅ | Embedded GLTF — `Mc3EmbedGltf` struct (id, src, base64Content); `<embeds>` top-level section; external `<embed type="gltf" src="…"/>` and inline base64 CDATA form; meshSource convention "embed:\<id\>"; `doc.embeds` map + `addEmbed()`; XSD `embedsType`; MCB writer/reader; 13 roundtrip tests all pass | Mc3EmbedGltf.hpp, Mc3Document.hpp, Mc3Document.cpp, Mc3XmlParser.cpp, Mc3XmlWriter.cpp, mc3.xsd, McbWriter.cpp, McbReader.cpp, roundtrip_test.cpp |
| N3 | Lua scripts — `<script type="lua" id="…">lua source</script>`; new `Mc3Script` struct (`id`, `type`, `source`); parser/writer; editor: new "Scripts" tab in left panel with list and inline editor; script execution is optional at runtime (Lua 5.4 or LuaJIT) | mc3/include/Mc3Document.hpp, Mc3XmlParser.cpp, Mc3XmlWriter.cpp, mc3.xsd |
| N4 | Sound and music — `<sound id="…" src="…" loop="false"/>` and `<music id="…" src="…" loop="true"/>`; new `Mc3Sound` / `Mc3Music` structs; editor: "Audio" tab in left panel with list and attributes; optional playback in editor (SDL_mixer or miniaudio) | mc3/include/Mc3Document.hpp, Mc3XmlParser.cpp, Mc3XmlWriter.cpp, mc3.xsd |
| N5 | `<trigger>` element — named sequence of things to start: `<trigger id="intro"><play-action ref="walk_anim"/><play-sound ref="door_click"/><run-script ref="init_lua"/><play-music ref="bg_music"/></trigger>`; new `Mc3Trigger` struct; editor: "Triggers" tab in left panel | mc3/include/Mc3Document.hpp, Mc3XmlParser.cpp, Mc3XmlWriter.cpp, mc3.xsd |
| N6 | Document-level `<state>` — named scene-wide configuration snapshots (distinct from per-object `Mc3ObjectState`); each `<state name="night">` contains `<object-override id="…" position="…" rotation="…" visible="…" material="…"/>` for any number of objects; switching a state applies all overrides to live objects; editor: "Scene States" tab | mc3/include/Mc3Document.hpp, Mc3XmlParser.cpp, Mc3XmlWriter.cpp, mc3.xsd |
| N7 | `<meta>` element — top-level key-value metadata container for mc3.xml generators; contains `<metaentry key="…" value="…"/>` elements (key and value as attributes, no sub-tags); shown read-only in editor Scene Properties; XSD + roundtrip test | mc3/include/Mc3Document.hpp, Mc3XmlParser.cpp, Mc3XmlWriter.cpp, mc3.xsd, mc3/test/roundtrip_test.cpp |

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
