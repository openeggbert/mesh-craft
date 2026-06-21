# NEXT.md — MeshCraft Handoff Document

_Last updated: 2026-06-21 (M1/M2/M3 complete; render analysis; 14/14 tests pass)_

---

## 1. Project Summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` format — a lightweight
XML-based scene description used by the OpenEggbert project. The editor provides a
Dear ImGui UI with orbit camera, object hierarchy, properties panel, timeline/animation,
gizmos, CSG boolean operations, extrude/path geometry, material editing, AI-assisted
scene generation, and a local model registry.

**Main goal:** Full-featured authoring tool for `.mc3.xml` scenes, analogous to a
stripped-down Blender for the MC3 format.

**Current phase:** Active feature implementation (~75 of 100 plan.md tasks done).
M1/M2/M3 asset pipeline features are complete and hardened. Renderer quality is the
next major improvement area (see `render.md`).

**Key architectural decisions:**
- Built on **CNA** — XNA-like C++ runtime (SDL3 + OpenGL ES 3.2 via EasyGL).
  **CNA source must not be modified.** CNA will be extended beyond XNA 4.0 — new
  renderer features should be designed to use a richer CNA API in the future.
- All UI is **Dear ImGui** (v1.91.6, SDL3 + OpenGL ES 3 backends).
- Scene data (`Mc3Document`) is pure C++ in the `mc3/` sublibrary — no graphics
  dependency. **Public API must not change without auditing `mc3togltf` and all test XMLs.**
- Undo/redo: deep-copy snapshots of entire `Mc3Document` (20-step stack).
- Application split into ~12 `.cpp` files by concern (Mouse, Keyboard, FileOps,
  Commands, Ui*, Anim, WalkMode, UiAi, UiRegistry).
- Buffer prefill to ImGui `char[]` buffers via `copyToBuf<N>(dst, string_view)` —
  uses `snprintf`, always null-terminates.

---

## 2. Current Status

### Build
- **All targets build cleanly** on Linux x86-64 with GCC/Clang.
- `cmake --build cmake-build-debug --target MeshCraft` — OK.
- `mc3_roundtrip_test`, `mc3_registry_test`, `mc3togltf_*` — all build OK.

### Tests
- **14/14 CTest targets pass** (`ctest` from `cmake-build-debug/`).
  - `smoke_test` — OK
  - `xsd_validation` — OK
  - `mc3_registry` — OK (open/close, save/search/remove, duplicate def id, migration)
  - `mc3_roundtrip` — OK (include, local override, nested include roundtrip, cycle detection)
  - `mc3togltf_*` (9 targets) — all OK

### Available binaries (in `cmake-build-debug/`)
- `MeshCraft` — the editor
- `mc3_roundtrip_test` — XML roundtrip + include system tests
- `mc3_registry_test` — SQLite model registry tests
- `mc3togltf/mc3togltf` — export to GLB/GLTF (linked into editor via `mc3togltf_lib`)

### Recently implemented features (working)
- **M1 — `<include file="…"/>` in mc3.xml:** merge definitions/materials/textures from
  library files; cycle detection; diamond deduplication; local override; roundtrip-safe
  (writer skips included IDs).
- **M2 — Model Registry:** SQLite3 DB at `~/.meshcraft/modelregistry.sqlite3`; save
  definitions with group/name/variant/tags/description/source; search; insert into scene;
  duplicate-id suffix; DB migration.
- **M3 — AI Assistant:** async Claude API (cpp-httplib, `std::async`); auto-validates
  XML response into `aiPendingDoc_`; Apply to Scene; Save to Registry; empty-doc guard;
  scope selection (full scene / selection only); auto-opens registry on first use.

### What does not work yet
- **Renderer lighting:** no directional light, no ambient — all surfaces flat-colored
  (see `render.md` and Section 4 below).
- **Edge overlay quality:** wire cage over flat-shaded geometry looks poor; z-fighting
  on large objects (see `render.md`).
- **Bloom (I6):** pipeline runs, emissive objects drawn, but visual effect not visible
  (suspected CNA BasicEffect limitation).
- **Preferences persistence (H7):** only auto-save interval stored; snap/grid/theme
  not yet persisted across sessions.

---

## 3. Recent Changes

### 2026-06-21 — M1/M2/M3 hardening + render analysis

**Files added:**
- `render.md` — renderer analysis: root causes of flat shading and edge overlay
  problems; six improvement proposals (P1–P6) ordered by effort.
- `mc3/test/mc3_registry_test.cpp` — registry unit tests (5 test cases).
- `test/include_override_{library,scene}.mc3.xml` — local override test fixtures.
- `test/include_lib_{a,b}.mc3.xml`, `test/include_nested_scene.mc3.xml` — nested
  include fixtures.
- `test/include_cycle_{a,b}.mc3.xml` — cycle detection fixtures.

**Files modified:**
- `mc3/src/Mc3XmlParser.cpp` — `processIncludes(recordIncludes)` parameter to fix
  nested include recording; erase local IDs from included* sets after each local
  parse section (fixes local override loss on save).
- `mc3/src/Mc3XmlWriter.cpp` — fix material id written from map key not `mat.name`.
- `mc3/test/roundtrip_test.cpp` — added `testIncludeOverride()`, `testIncludeNested()`
  (with save/reload), `testIncludeCycle()`.
- `include/MeshCraft/ModelRegistry.hpp` — `Entry` struct: added `description`, `source`;
  `entryFromDefinition()` accepts two extra defaulted string params.
- `src/MeshCraft/ModelRegistry.cpp` — description/source columns + ALTER TABLE migration;
  atomic temp filenames (`gRegTmpCounter`).
- `src/MeshCraft/MeshCraftApplication_UiAi.cpp` — full AI lifecycle rewrite:
  auto-populate `aiPendingDoc_` on poll; Apply does not clear it; Save to Registry
  auto-opens DB; Reset closes AI save dialog if `regSaveFromAi_`; no-SQLite3 error.
- `src/MeshCraft/MeshCraftApplication_UiRegistry.cpp` — Definition combo uses
  `aiPendingDoc_->definitions` when `regSaveFromAi_`; auto-open guard checks
  `isOpen()` after `open()` for no-SQLite3 builds; removed orphan `Text("Definition:")`.
- `include/MeshCraft/MeshCraftApplication.hpp` — `copyToBuf<N>` template helper
  (`snprintf`, always null-terminates); added `<cstdio>`, `<string_view>`.
- `m1m2m3.md` — updated architecture diagram, model default, AI lifecycle, Reset flow.
- `CMakeLists.txt` — `find_package(SQLite3)`, `mc3_registry_test` CTest target.

---

## 4. Current Blocker / Main Problem

**Renderer has no lighting — all surfaces are flat-colored.**

### Symptom
Every primitive face (box, sphere, cylinder, etc.) is drawn with one uniform color
regardless of its orientation to the camera or any scene lights. No highlight, no
shadow, no shading gradient. Scene looks like a flat-color paper model.

With the edge overlay enabled (`Show Edges`), black wire cages are drawn on top of
flat-colored faces — there is no shading gradient underneath to give depth, so the
result looks like a painted wireframe.

### Cause (confirmed, see `render.md`)
`BasicEffect` is initialised with `VertexColorEnabled = true` and `LightingEnabled`
is never set to `true` anywhere in the solid draw path
(`SceneRenderer.cpp:288–289`). There is no `EnableDefaultLighting()` call.

The `VertexPositionNormalTexture` buffers (with correct per-face normals) **already
exist** for every unit shape — they are used only in `drawMeshTextured()` when a
material has a `baseColorTexture`. Even there, `LightingEnabled` is still off.

### Affected files
- `src/MeshCraft/Renderer/SceneRenderer.cpp` — `draw()`, `drawMesh()`, `drawMeshTextured()`
- `src/MeshCraft/Renderer/SceneRenderer_Extrude.cpp` — `drawObjectEdges()`, `kPush`

### Not yet tried
- Enabling `LightingEnabled = true` with one directional key light and ambient.
- Routing all solid draws through `drawMeshTextured` (which uses the VPNT path)
  even when `tex == nullptr`.

---

## 5. Known Bugs and Limitations

| Status | Issue |
|--------|-------|
| **confirmed** | Renderer: `LightingEnabled` never true — all surfaces flat-shaded. `SceneRenderer.cpp:288`. |
| **confirmed** | Edge overlay: `kPush = 1.003f` (local space) causes z-fighting on large-scale objects (castle walls etc.). |
| **suspected** | Bloom (I6): pipeline runs (17 emissive objects/frame on medieval_castle), visual effect invisible. Likely BasicEffect cannot do additive FBO composite — needs custom shader. |
| **suspected** | Walk mode: Esc exit fails when `ImGui::WantCaptureKeyboard` is true. HUD "Exit" button always works. |
| **suspected** | H5 pivot compensation assumes scale=1; non-unit scale produces small visual shift. |
| incomplete | H15 walk mode: no frame-rate-independent mouse-look cap; floor collision at y=0 only. |
| incomplete | H7 Preferences: only auto-save interval stored; snap/grid/theme not persisted. |
| incomplete | F7 headless screenshot: always writes PPM regardless of extension. |
| incomplete | MCB: compression flag reserved in header but not implemented (throws if set). |
| incomplete | Mesh objects (GLB) in edge overlay fall through to bounding-box cage (`default:` in `drawObjectEdges`). |
| known | MCB round-trip: field ordering differs from original XML (std::map order). Not a correctness issue. |
| known | No automated UI tests — only XML round-trip, registry, and smoke test. |
| known | tinyobjloader mesh preview limited to 300k triangles; larger OBJ falls back to placeholder box. |
| technical debt | `MeshCraftApplication.hpp` ~535 lines of member declarations. Stable but large. |
| pipeline | mc3togltf uses `source` attribute for mesh; core uses `src` — mismatch (S1 in STABILIZATION.md). |
| pipeline | mc3togltf has its own duplicate XML parser diverging from core (S3 in STABILIZATION.md). |

---

## 6. Architecture Notes

### Main modules

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` + siblings | Game loop, input, scene state, entire ImGui UI |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer*.cpp` (4 files) | Solid + edge + gizmo draw; LOD; BasicEffect |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid lines |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom; view+projection matrices |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | `hasSelection()`, `selection()` (vector of shared_ptrs) |
| `AiAssistant` | `include/MeshCraft/AiAssistant.hpp` + `src/…/AiAssistant.cpp` | Async Claude API; `sendAsync/isDone/result/reset` |
| `ModelRegistry` | `include/MeshCraft/ModelRegistry.hpp` + `src/…/ModelRegistry.cpp` | SQLite3 model DB; guarded by `MESHCRAFT_HAS_SQLITE3` |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; XML load/save; `includes`, `includedDefs/Materials/Textures` |
| `Mcb` | `mcb/` sublibrary | MCB binary format read/write |
| CNA | `../cna/` sibling repo | SDL3, GL ES 3 context, BasicEffect, Input — **do not modify** |

### Renderer draw order (per frame)
1. Grid (depth off)
2. Solid scene — `SceneRenderer::draw()` (depth on)
3. Edge overlay — `SceneRenderer::drawEdgeOverlay()` (depth on, if enabled)
4. Light/camera/CSG gizmos (depth off)
5. Transform gizmo, bounding box, locked outlines
6. Bloom emissive pass (if enabled)
7. Measurement overlay
8. ImGui UI

### Key invariants
- `Mc3Material` fields: `baseColor[4]`, `roughness`, `metallic`, `emissiveColor[3]` (not `emissive`).
- `SelectionManager` API: `hasSelection()` and `selection()` returning `const vector<shared_ptr<Mc3Object>>&`. No `selectedIds()`.
- `copyToBuf<N>(char(&)[N], string_view)` — use this everywhere a `char[]` ImGui buffer is prefilled. Never bare `strncpy`.
- `ModelRegistry::open()` stub (no-SQLite3 build) returns without throwing and leaves `isOpen() == false`. Always check `isOpen()` after `open()` before proceeding.
- AI save dialog: `regSaveFromAi_ = true` means the dialog should read from `aiPendingDoc_->definitions`, not `document_.definitions`. Reset always clears both.
- `file(GLOB_RECURSE)` in CMakeLists.txt — new `.cpp` files require `cmake ..` reconfigure.
- **Do not push to `master`** directly — work on `develop`.

### Key constraints
- **No CNA source changes** without owner permission.
- **No `${meta-gl_SOURCE_DIR}/include`** in CMakeLists.txt — triggers full CNA recompile.
- **No `Mc3Document` public API changes** without auditing `mc3togltf` and all test XMLs.
- **CLAUDE.md workflow**: always ask user before implementing a plan.md task.

---

## 7. Useful Commands

```bash
# Build editor
cmake --build cmake-build-debug --target MeshCraft -- -j$(nproc)

# Build everything
cmake --build cmake-build-debug -- -j$(nproc)

# Run editor
./cmake-build-debug/MeshCraft test/house.mc3.xml

# Headless screenshot (no window)
./cmake-build-debug/MeshCraft --screenshot /tmp/out.ppm test/house.mc3.xml

# Run all CTests
cd cmake-build-debug && ctest --output-on-failure

# Run specific test groups
ctest -R "xsd_validation|mc3_roundtrip|mc3_registry"

# Reconfigure (required after adding new .cpp files)
cd cmake-build-debug && cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON

# MCB round-trip
./cmake-build-debug/mc3tomcb/mc3tomcb test/features.mc3.xml /tmp/f.mcb
./cmake-build-debug/mc3tomcb/mc3tomcb /tmp/f.mcb /tmp/f_rt.mc3.xml

# Export to GLB
./cmake-build-debug/mc3togltf/mc3togltf test/house.mc3.xml /tmp/house.glb
```

---

## 8. Next Smallest Tasks

Ordered by priority. Each is one focused coding session.

### R1 — Enable directional light + ambient in solid draw pass
**Goal:** Make faces shade differently based on their normal vs. light direction.
**Approach:** In `SceneRenderer::draw()`, before the object loop, call
`EnableDefaultLighting()` (or set DirectionalLight0 + ambient manually) and
`setLightingEnabledProperty(true)`. Change `drawAuto` lambda to always use
`drawMeshTextured` (VPNT path) even when `tex == nullptr` — the VPNT path already
handles `tex == nullptr` by skipping `setTextureProperty`. After the loop, restore
`LightingEnabled = false` so edge/gizmo passes are unaffected.
**Files:** `src/MeshCraft/Renderer/SceneRenderer.cpp`
**Verify:** `cmake --build cmake-build-debug --target MeshCraft && ctest`; visually
confirm faces of a box show distinct top/side/bottom tones.

### R2 — Fix edge overlay z-fighting (world-space push)
**Goal:** Eliminate black flicker on large-scale objects with edge overlay enabled.
**Approach:** Replace `constexpr float kPush = 1.003f` with a per-object value
derived from the maximum world-space scale axis, targeting a fixed ~0.02-unit push
regardless of object scale.
**Files:** `src/MeshCraft/Renderer/SceneRenderer_Extrude.cpp`
**Verify:** Load medieval_castle scene, enable edges, confirm no flicker on walls.

### S1 — Fix `source` → `src` attribute mismatch in mc3togltf
**Goal:** `mc3togltf` uses `source` for mesh paths; the core parser uses `src`.
**Files:** `mc3togltf/src/` (parser), `mc3/src/Mc3XmlParser.cpp` (reference).
**Verify:** Export a scene with mesh objects; confirm GLB contains geometry.

### S3 — Remove duplicate parser from mc3togltf
**Goal:** mc3togltf has its own XML parser diverging from the core `Mc3XmlParser`.
Replace it with a dependency on the `Mc3` library.
**Files:** `mc3togltf/src/`, `mc3togltf/CMakeLists.txt`.
**Verify:** All 9 mc3togltf CTest targets still pass.

### S4 — Add mc3togltf to top-level CMake
**Goal:** `cmake --build cmake-build-debug` should build mc3togltf without a
separate build step.
**Files:** `CMakeLists.txt`, `mc3togltf/CMakeLists.txt`.
**Verify:** Clean build produces `cmake-build-debug/mc3togltf/mc3togltf`.

### I4 — Colored point-light sphere gizmos
**Goal:** Point lights rendered as small colored spheres in the viewport (color
matches light color).
**Files:** `src/MeshCraft/Renderer/SceneRenderer_Gizmos.cpp`,
`src/MeshCraft/Renderer/SceneRenderer.hpp`.
**Verify:** Add a point light to a scene, confirm colored sphere gizmo appears.

---

## 9. Do Not Do Yet

- **No CNA source changes** — owned by a separate Claude Code instance.
- **No `Mc3Document` public API changes** without full audit.
- **No custom OpenGL shaders** yet — wait for CNA to expose a shader upload API
  (planned beyond XNA 4.0). `render.md` P5/P6 describe the target architecture.
- **No screen-space effects (SSAO, shadow maps, bloom rewrite)** until CNA provides
  FBO read/write and fragment shader API.
- **No refactor of `MeshCraftApplication.hpp`** — large but stable.
- **No new `.cpp` files** without `cmake ..` reconfigure first.
- **No speculative features** — only implement plan.md tasks confirmed by user.
- **No mass cleanup** of any large file until relevant feature work stabilizes.
- **No MCB compression** — zlib dependency not desired yet.
- **No switching ImGui version** — v1.91.6 is pinned.

---

## 10. Resume Prompt

```
Read NEXT.md and CLAUDE.md first. Do not read other files until you know which
task you are working on.

For plan.md feature tasks: ask the user before implementing each task,
with a 2–4 sentence description of what will change.

For renderer tasks: start with R1 (enable directional light) in
src/MeshCraft/Renderer/SceneRenderer.cpp — the VPNT buffers and drawMeshTextured
path already exist, lighting just needs to be enabled.

Build:  cmake --build cmake-build-debug --target MeshCraft -- -j$(nproc)
Test:   cd cmake-build-debug && ctest --output-on-failure

Do not modify CNA. Do not change Mc3Document public API without audit.
Do not refactor unrelated code. Make one small verified improvement, then
update NEXT.md.
```
