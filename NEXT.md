# NEXT.md

## 1. Project summary

**MeshCraft** is a 3D scene editor for the `.mc3.xml` format — a custom XML-based scene description that supports primitives (box, sphere, cylinder, cone, plane), extrusion, CSG, instances, lights, cameras, and environment settings.

**Main goal:** a working GUI editor that lets users visually build and edit MC3 scenes, save them back to `.mc3.xml`, and export to `.glb` via the `mc3togltf` converter.

**Current phase:** Editor MVP. The application window, 3D viewport, grid, scene rendering, SpriteBatch UI panels, and basic keyboard/mouse input are all implemented. The editor can load and display `.mc3.xml` files and show them with colored UI panels.

**Key architectural decisions:**
- Built on **CNA** — an XNA-like C++ framework using SDL3 + OpenGL ES 3.2 (EasyGL backend).
- UI panels drawn with `SpriteBatch` + a 1×1 white `Texture2D` (the "white pixel trick") — no text rendering yet.
- MC3 scene data lives in `Mc3::Mc3Document` from the `mc3/` sublibrary (pure C++, no graphics dependency).
- CNA is a sibling repo (`../cna`) included via `add_subdirectory`.

---

## 2. Current status

**Build:** succeeds cleanly.
```
cmake-build-debug/ninja → [255/258] Linking CXX executable MeshCraft  ✓
```

**Tests:** no automated tests for the editor itself. The `mc3/` library has its own build, and test assets exist in `test/`.

**Working:**
- Loading `.mc3.xml` files on startup (`./MeshCraft test/house.mc3.xml`)
- 3D scene rendering (primitives: box, sphere, cylinder, cone, plane, extrude)
- Grid renderer with XYZ axis colours
- Orbit/pan/zoom camera (middle-drag, right-drag, scroll)
- SpriteBatch-based UI: toolbar, left hierarchy panel, right properties panel, status bar
- Hierarchy panel: click on a row to select an object
- Keyboard shortcuts: new/open/save/export, tools (Q/G/R/S), add primitives (F1–F5), delete, nudge, camera reset (F), help (F12)
- Window title shows tool / file / modified state
- F11 shortcut triggers screenshot to `screenshot.ppm`
- **`saveScreenshot()` — working**: uses `SDL_GL_GetProcAddress("glReadPixels")` via forward declaration; produces a valid PPM
- **Viewport restriction — working**: 3D scene renders only in the center area bounded by panels; panel areas stay dark. Uses direct `glViewport`/`glScissor` calls via `SDL_GL_GetProcAddress` cached in `LoadContent()`.
- **Window size 1024×768 — working**: `PresentationParameters` default constructor in CNA now uses 1024×768 (was 800×480); SDL window and logical viewport are 1024×768 on startup.

**Working (recent additions):**
- Ray-cast object picking: left-click in 3D viewport selects closest AABB hit; recursive through children; Ctrl+click for multi-select.
- Bitmap font: 5×7 pixel font in hierarchy and properties panels; "SCENE"/"PROPERTIES" headers, object names, POS/ROT/SCL values.

**Not working:**
- No transform gizmo (stub `TransformGizmo` class exists but does nothing).
- No file-open dialog — file path must be entered via console stdin.

---

## 3. Recent changes

- **`../cna/src/Microsoft/Xna/Framework/Graphics/PresentationParameters.cpp`** — Changed `PresentationParameters` default constructor from 800×480 to 1024×768; this sets the initial SDL window size and `virtualWidth_`/`virtualHeight_` for the EasyGL backend.
- **`src/MeshCraft/MeshCraftApplication.cpp`** + **`include/MeshCraft/MeshCraftApplication.hpp`** — Fixed viewport restriction: cache `glViewport`/`glScissor`/`glEnable`/`glDisable` in `LoadContent()` via `SDL_GL_GetProcAddress`; in `Draw()` apply scissor before `gd.Clear(bgColor)` to restrict it to the 3D area, then re-apply GL viewport for 3D rendering, then disable scissor before SpriteBatch.
- **`src/MeshCraft/MeshCraftApplication.cpp`** — Fixed `saveScreenshot()`: replaced `dlsym` with `SDL_GL_GetProcAddress` forward declaration.
- **`CMakeLists.txt`** — Reverted accidental default backend change (VULKAN → EASYGL).
- **`../cna/src/CNA/Internal/Backends/EasyGL/EasyGLGraphicsBackend.cpp`** — Fixed pre-existing CNA bug: moved anonymous namespace helpers before first use.
- **`../cna/include/Microsoft/Xna/Framework/Graphics/EffectParameter.hpp`** + `.cpp` — Fixed pre-existing CNA bug: added `SetValue(Texture2D*)` overload; fixed `GetValueTexture2D()`.
- **`../cna/include/Microsoft/Xna/Framework/Graphics/EffectParameter.hpp`** and **`.cpp`** — Fixed pre-existing CNA bug: added `SetValue(Texture2D*)` overload and `texture2DData_` field; changed `GetValueTexture2D()` to return `texture2DData_` directly (the `dynamic_cast<Texture2D*>(textureData_)` pattern was always returning null since `Texture2D` doesn't inherit `Texture`).
- Earlier (previous session): Added SpriteBatch UI panels, auto-screenshot constructor, CNA API updates.

---

## 4. Current blocker / main problem

No critical blocker. Window size (1024×768), screenshot capture, and viewport restriction are all working.

---

## 5. Known bugs and limitations

| # | Status | Description |
|---|--------|-------------|
| 1 | **fixed** | `saveScreenshot()` — now uses `SDL_GL_GetProcAddress`; verified working |
| 2 | **fixed** | Viewport restriction — 3D scene now confined to center area via `glViewport`/`glScissor` called directly in `Draw()` |
| 3 | **fixed** | Bitmap font: 5×7 pixel glyphs in hierarchy and properties panels; object names, POS/ROT/SCL values rendered |
| 4 | **fixed** | Ray-cast picking — slab-method ray-AABB per object, recursive through children, selects closest hit |
| 5 | **incomplete** | `TransformGizmo` is a stub — no gizmo rendered or draggable |
| 6 | **incomplete** | File-open dialog not available — user types path in terminal stdin |
| 7 | **incomplete** | Hierarchy panel only shows flat top-level objects; groups/children not indented |
| 8 | **suspected bug** | `prevKs_` field declared in header but `KeyboardState` may lack default constructor in older CNA builds |
| 9 | **needs verification** | CNA commit `34ae601` renamed `CurrentTechnique()`/`Passes()` — any other call sites in MeshCraft not yet updated? |
| 10 | **incomplete** | CSG operations (Union, Difference, Intersection) not handled in SceneRenderer |

---

## 6. Architecture notes

**Main modules:**

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` | Game loop, input, scene state, UI draw |
| `GridRenderer` | `src/MeshCraft/Renderer/GridRenderer.cpp` | XYZ grid lines via `BasicEffect` + `VertexBuffer` |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Renders MC3 scene objects via BasicEffect |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom; produces view+projection matrices |
| `SelectionManager` | `include/MeshCraft/Editor/SelectionManager.hpp` | Tracks selected `Mc3Object` shared_ptrs |
| `TransformGizmo` | stub | Not yet implemented |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; load/save XML |
| CNA | `../cna/` sibling repo | SDL3 window, GL context, SpriteBatch, BasicEffect, Texture2D |

**Data flow:**
1. `LoadContent()` creates renderers; loads `Mc3Document` from XML.
2. `Update()` polls `Keyboard`/`Mouse`; mutates camera, selection, document.
3. `Draw()` clears screen, renders 3D scene, then overlays 2D UI via SpriteBatch.

**Important invariants:**
- `Mc3Document.materials` is `std::map<std::string, Mc3Material>` — iterate with structured bindings `const auto& [key, mat]`.
- `Mc3Material` uses `baseColor` (4-element float array), not `diffuse`.
- `SpriteBatch::Begin()`/`End()` must bracket all `Draw()` calls; cannot nest.
- CNA API (post-commit 34ae601): use `getCurrentTechniqueProperty()` and `getPassesProperty()` — old names `CurrentTechnique()`/`Passes()` are gone.
- `Color` has no default constructor — always initialise with 4 args.
- Do not add `${meta-gl_SOURCE_DIR}/include` to MeshCraft's CMakeLists.txt — it triggers full CNA recompilation with pre-existing CNA bugs.

---

## 7. Useful commands

```bash
# Configure (first time)
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL

# Build
cd cmake-build-debug && ninja -j$(nproc)

# Run with a test scene
./cmake-build-debug/MeshCraft test/house.mc3.xml

# Run with auto-screenshot (exits after ~2s)
./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/editor.ppm

# Run with garden house scene
./cmake-build-debug/MeshCraft test/garden_house.mc3.xml

# Screenshot (now working)
./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/test.ppm
# Expected output: [Screenshot] written /tmp/test.ppm  → valid 800×480 PPM

# Convert MC3 to GLB
./cmake-build-debug/mc3/mc3togltf test/house.mc3.xml test/house.glb
```

---

## 8. Next smallest tasks

1. **Ray-cast object picking — DONE**
   - Implemented in `handleMouseInput()`: unprojects mouse NDC via `EditorCamera::screenRayDirection`, tests each object's world-space AABB (slab method), selects closest hit.
   - Recursive through `children` (e.g. boxes inside House group are pickable).
   - Top-level objects (LeftColumn, RightColumn, Ornament, Ground) highlight in hierarchy panel when clicked. Children of groups are selected but not shown in hierarchy panel (flat list only shows top-level).

2. **Bitmap font — DONE**
   - `include/MeshCraft/Ui/BitmapFont.hpp` + `src/MeshCraft/Ui/BitmapFont.cpp`: 5×7 pixel font, 96 printable ASCII glyphs, column-major encoding.
   - Hierarchy panel: "SCENE" header, object name per row, color-coded text for selection state.
   - Properties panel: "PROPERTIES" header, object name on type bar, "POS"/"ROT"/"SCL" section labels, X/Y/Z axis labels with numeric values, material name.

3. **Transform gizmo (stub — not yet implemented)**
   - Goal: show X/Y/Z drag handles on the selected object in the 3D viewport.
   - Files: `include/MeshCraft/Editor/TransformGizmo.hpp`, `src/MeshCraft/Renderer/SceneRenderer.cpp`.
   - Approach: draw three axis lines from the selected object's position; detect mouse drag on a handle and update the transform.
   - Verify: dragging a selected cube's X handle moves it along X in the scene.

4. **Add automated smoke test**
   - Goal: CI can verify the binary loads a scene and exits cleanly.
   - Files: `CMakeLists.txt`, new `test/smoke_test.sh`.
   - Approach: run `MeshCraft test/house.mc3.xml --screenshot /tmp/smoke.ppm` in a virtual framebuffer (`xvfb-run`), check exit code 0 and that `/tmp/smoke.ppm` is non-empty.
   - Verify: `ctest` passes.

---

## 9. Do not do yet

- **No refactor of CNA** — CNA still has other bugs that may surface when objects are recompiled; only fix what's blocking a build. The two pre-existing bugs fixed this session (`ToEasyGLCompareFunc` ordering, `SetValue(Texture2D*)`) were fixed because CNA was already being recompiled due to CMakeLists.txt timestamp change.
- **No new features** until ray-cast picking works — it is the next key interaction needed before gizmo or other editing features make sense.
- **No SpriteFont integration** until a simpler bitmap font approach is validated or CNA gains native text support.
- **No CSG rendering** until the basic picking and gizmo are working.
- **No API changes in `Mc3Document`** without checking `mc3togltf` and all test scenes.
- **No mass include path changes** in `CMakeLists.txt` — any change to `target_include_directories` that touches CNA's dependencies risks triggering a full CNA recompile with pre-existing bugs.
- **No GUI file dialog** — out of scope until panels have text and a basic widget model.

---

## 10. Resume prompt

```
Read NEXT.md first. Then implement the next task (transform gizmo: draw X/Y/Z axis lines from selected object position in 3D viewport; detect mouse drag on handle and update transform). Do not refactor unrelated code. Make one small verified improvement. Build with: cd cmake-build-debug && ninja -j$(nproc). Verify with: ./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/test.ppm && ffmpeg -i /tmp/test.ppm /tmp/test.png -y && check the PNG. Update NEXT.md after finishing.
```
