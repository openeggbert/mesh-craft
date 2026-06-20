# NEXT.md — MeshCraft Handoff Document

_Last updated: 2026-06-20 (bloom/fog/grid/theme; STABILIZATION.md added)_

---

## 1. Project Summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` format — a lightweight XML-based scene description used by the OpenEggbert project. The editor provides a Dear ImGui UI with orbit camera, object hierarchy, properties panel, timeline/animation, gizmos, CSG boolean operations, extrude/path geometry, and material editing.

**Main goal:** Full-featured authoring tool for `.mc3.xml` scenes, analogous to a stripped-down Blender for the MC3 format.

**Current phase:** Active feature implementation (~72 of 100 plan.md tasks done) + MC3 pipeline stabilization (see `STABILIZATION.md`).

**Key architectural decisions:**
- Built on **CNA** — XNA-like C++ runtime (SDL3 + OpenGL ES 3.2 via EasyGL). **CNA source must not be modified.**
- All UI is **Dear ImGui** (v1.91.6, SDL3 + OpenGL ES 3 backends, `#version 300 es`). `ImGuiItemFlags_MixedValue` is in `imgui_internal.h` only — do not include internal headers in MeshCraft sources.
- Scene data (`Mc3Document`) is pure C++ in the `mc3/` sublibrary — no graphics dependency. **Public API must not change without auditing `mc3togltf` and all test XMLs.**
- Undo/redo: deep-copy snapshots of entire `Mc3Document` (20-step stack).
- Application split into ~12 `.cpp` files by concern (Mouse, Keyboard, FileOps, Commands, Ui*, Anim, WalkMode). No shared anonymous-namespace helpers across files.
- `mcb/` static library provides binary MCB format; `mc3tomcb` CLI converts between formats.

---

## 2. Current Status

### Build
- **All targets build cleanly** on Linux x86-64 with GCC/Clang.
- `ninja MeshCraft` — OK. `ninja mc3tomcb` — OK. `ninja Mcb` — OK.
- `mc3togltf` builds standalone (`mc3togltf/build/`); **not yet in top-level CMake** (S4 in STABILIZATION.md).

### Tests
- **Smoke test passes:** `bash test/smoke_test.sh ./MeshCraft test/house.mc3.xml` → PASS.
- **MCB round-trip verified:** `mc3tomcb features.mc3.xml features.mcb && mc3tomcb features.mcb out.mc3.xml` — OK.
- No automated unit tests for `MeshCraftApplication` commands (L3 pending).
- mc3togltf tests use hardcoded `mc3togltf/build/` path (S4/S8 in STABILIZATION.md).

### Available binaries (in `cmake-build-debug/`)
- `MeshCraft` — the editor
- `mc3tomcb/mc3tomcb` — CLI converter mc3.xml ↔ .mcb
- `../mc3togltf/build/mc3togltf` — export to GLB/GLTF (standalone build)

---

## 3. Recent Changes

### 2026-06-20
- **Bloom post-process (I6):** FBO+Gaussian blur+additive composite for emissive materials. Pipeline confirmed functional (17 emissive objects drawn/frame on medieval_castle scene); visual result TBD (see bloom bug in `plan.md` I6).
- **Fog rendering (I3):** reads `Mc3Fog` from environment and passes to BasicEffect fog uniforms.
- **Grid:** quadratic alpha fade toward edges, colored axis lines (X=red, Z=blue).
- **UI:** Blender-inspired dark theme applied at startup.
- **medieval_castle.mc3.xml:** new test scene added; `fire_glow` and `charcoal` materials have `emissive_color`.
- **STABILIZATION.md:** added; 12 sub-tasks for MC3 pipeline stabilization.

### 2026-06-14
- **E7** Named layers, **E8** Export subtree as template
- **F2** Mesh source browse, **F3** Export selection, **F4** Merge scene
- **F5** Configurable auto-save, **F6** Rotating backups, **F7** Headless screenshot, **F8** GLB export settings UI
- **G8** Procedural LOD (3 tiers by camera distance for all round primitives)
- **H1** Proportional editing (Gaussian falloff), **H4** Scatter along curve
- **H5** Move Pivot independently from geometry
- **H15** First-person walk mode (F5 toggle, gravity, jump, mouse look)
- **MCB binary format** (`mcb/` library + `mc3tomcb` CLI)

### 2026-06-13
- **A1/A2** Material + deform animation channels wired to renderer via `AnimOverride`
- **B1** Local/World gizmo space toggle
- **B2** Gizmo delta overlay (floating Δ value while dragging)
- **C1** Properties multi-edit (`~` indicator, write to all selected)

---

## 4. Current Priorities

Two parallel tracks:

**Track A — plan.md features** (next candidates from remaining ~28 tasks):
- **I4** — Colored point-light sphere gizmos
- **H7** — Complete Preferences dialog (snap, grid, theme persistence)
- **K2** — CSG reorder children in Properties
- **D5** — Material export/import as XML snippet
- **E5** — Hierarchy filter by multiple criteria

**Track B — STABILIZATION.md pipeline fixes** (see that file for full list):
- **S1** — Fix `source` → `src` in mc3togltf parser _(next)_
- **S3** — Remove duplicate parser from mc3togltf
- **S4** — Add mc3togltf to top-level CMake

---

## 5. Known Bugs and Limitations

| Status | Issue |
|--------|-------|
| suspected bug | Bloom (I6): pipeline runs, 17 emissive objects drawn/frame, but visual effect not visible. See `plan.md` I6 notes. |
| suspected bug | Walk mode Esc exit fails when `ImGui::GetIO().WantCaptureKeyboard` is true. "Exit Walk Mode" HUD button always works. |
| suspected bug | H5 pivot compensation `pos += d·R − d` assumes uniform scale=1. Non-unit scale produces a small visual shift. |
| incomplete | H15 walk mode: mouse look has no frame-rate-independent cap — can spin at high FPS. |
| incomplete | H15 walk mode: floor collision only at y=0; no collision with scene geometry. |
| incomplete | H7 Preferences: only auto-save interval stored. Snap/grid/theme not yet persisted. |
| incomplete | F7 headless screenshot: always writes PPM regardless of extension. |
| incomplete | MCB: compression flag reserved in header but not implemented (throws if set). |
| known | MCB round-trip: field ordering differs from original XML (std::map order). Not a correctness issue. |
| known | No automated UI tests — only XML round-trip and smoke test exist. |
| known | tinyobjloader mesh preview limited to 300k triangles; larger OBJ files fall back to placeholder box. |
| workaround | `Song::GetHashCode()` linker stub — if CNA is recompiled from scratch, inject stub manually (see §7). |
| technical debt | `MeshCraftApplication.hpp` ~500 lines of member declarations. Should be split but deferred. |
| technical debt | `UiMenuBar.cpp` has repeated walk/collect lambdas (L4). |
| pipeline | mc3togltf uses `source` attribute for mesh; core uses `src` — mismatch (S1 in STABILIZATION.md). |
| pipeline | mc3togltf has its own duplicate XML parser diverging from core (S3 in STABILIZATION.md). |
| pipeline | mc3togltf not in top-level CMake build (S4 in STABILIZATION.md). |

---

## 6. Architecture Notes

### Main modules

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` + siblings | Game loop, input, scene state, entire ImGui UI |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer*.cpp` (4 files) | Renders MC3 primitives, gizmos, extrude, builders |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid lines |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom; view+projection matrices |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Tracks selected `Mc3Object` shared_ptrs |
| `TransformGizmo` | `include/MeshCraft/Editor/TransformGizmo.hpp` | GizmoAxis enum + drag state machine |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; XML load/save (no graphics dep) |
| `Mcb` | `mcb/` sublibrary | MCB binary format read/write |
| `mc3tomcb` | `mc3tomcb/` | CLI tool; links `Mc3` + `Mcb` |
| CNA | `../cna/` sibling repo | SDL3, GL ES 3 context, BasicEffect, Input |

### Data flow
1. `Mc3Document::loadFromFile(path)` or `Mcb::loadFromFile(path)` → `document_` (in-memory scene)
2. `MeshCraftApplication::Draw()` → `SceneRenderer::draw(document_, view, proj, selection)` → GL draw calls
3. Mouse/keyboard events → mutate `document_` → push undo stack
4. `document_.saveToFile(path)` or `Mcb::saveToFile(document_, path)` → disk

### Key invariants
- `Mc3Material` fields: `baseColor[4]`, `roughness`, `metallic`, `emissiveColor[3]` (not `emissive`).
- `evaluateAndPushAnimOverrides()` two-pass: pass 1 initialises overrides from document state, pass 2 overwrites with evaluated channel values. Always call both passes.
- `multiLabel(label, mixed)` in UiProperties: shows `~` in orange when values differ across selection.
- `allMatchF3/Str/Bool` lambdas are defined inside `drawPropertiesPanel()` — not available in other files.
- Gizmo local-space axes: `Right = rotM.getRightProperty()`, `Up = getUpProperty()`, `LocalZ = -getForwardProperty()` (XNA Forward = −Z).
- `file(GLOB_RECURSE)` in CMakeLists.txt — **new `.cpp` files require `cmake ..` reconfigure**.
- MCB format version: `MCB_VERSION = 1`. Increment if format changes break backward compat.

### Key constraints
- **No CNA source changes** without owner permission.
- **No `${meta-gl_SOURCE_DIR}/include`** in CMakeLists.txt — triggers full CNA recompile.
- **No `Mc3Document` public API changes** without auditing `mc3togltf` and all test XMLs.
- **CLAUDE.md workflow**: always ask user before implementing a plan.md task, with 2–4 sentence description.
- **Do not push to `master`** directly — work on `develop`, merge only when stable.

---

## 7. Useful Commands

```bash
# Build editor (from cmake-build-debug/)
ninja MeshCraft

# Build everything
ninja

# Run editor
./MeshCraft ../test/house.mc3.xml

# Headless screenshot (no window)
./MeshCraft --screenshot /tmp/out.ppm ../test/house.mc3.xml

# Smoke test
bash ../test/smoke_test.sh ./MeshCraft ../test/house.mc3.xml

# Run all CTests
ctest -V

# Reconfigure (required after adding new .cpp files)
cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON

# MCB round-trip
./mc3tomcb/mc3tomcb ../test/features.mc3.xml /tmp/f.mcb
./mc3tomcb/mc3tomcb /tmp/f.mcb /tmp/f_rt.mc3.xml

# Export to GLB (standalone mc3togltf build)
cd ../mc3togltf && mkdir -p build && cd build
cmake .. && make -j4
./mc3togltf ../../test/house.mc3.xml /tmp/house.glb

# Inject Song::GetHashCode stub (if CNA recompiled from scratch causes link error)
echo 'int _ZNK9Microsoft3Xna9Framework5Media4Song11GetHashCodeEv(const void* s){return 0;}' > /tmp/hashstub.c
gcc -c /tmp/hashstub.c -o /tmp/hashstub.o
ar r cmake-build-debug/CNA_dep/libCNA.a /tmp/hashstub.o
ranlib cmake-build-debug/CNA_dep/libCNA.a
```

---

## 8. Do Not Do

- **No CNA source changes** — owned by a separate Claude Code instance.
- **No `Mc3Document` public API changes** without full audit.
- **No refactor of `MeshCraftApplication.hpp`** — large but stable; splitting risks conflicts.
- **No new `.cpp` files** without `cmake ..` reconfigure first.
- **No speculative features** — only implement tasks confirmed by user from plan.md.
- **No SSAO / shadow maps** (I5, I7) — render-target infrastructure not ready.
- **No MCB compression** — flag reserved; zlib would add a dependency.
- **No keyboard shortcut system** (H9) until more UI features land.
- **No macro recorder** (H12) — requires command-pattern refactor.
- **No mass cleanup of `UiMenuBar.cpp`** (L4) until feature work stabilizes.
- **No switching ImGui version** — v1.91.6 is pinned.

---

## 9. Resume Prompt

```
Read NEXT.md and CLAUDE.md first.
For plan.md feature tasks: ask user before each task, describe what will change (2–4 sentences).
For STABILIZATION.md pipeline tasks: work through S1→S12 in priority order.
Build: cd cmake-build-debug && ninja MeshCraft
Test:  ctest -V
Do not modify CNA. Do not change Mc3Document public API without audit.
```
