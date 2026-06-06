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
cmake-build-debug/ninja → [250/251] Linking CXX executable MeshCraft  ✓
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

**Not working:**
- `saveScreenshot()` — `dlsym(RTLD_DEFAULT, "glReadPixels")` returns null (GL functions loaded via EGL, not in default dlopen namespace). Screenshot file is never written.
- No text rendered in panels — all labels are colour-coded shapes only.
- No ray-cast picking — clicking in the 3D viewport does not select objects.
- No transform gizmo (stub `TransformGizmo` class exists but does nothing).
- No file-open dialog — file path must be entered via console stdin.
- Viewport restriction for 3D rendering does not work: `GraphicsDevice::setViewportProperty()` only updates CPU state; `EasyGLGraphicsBackend::Clear()` always resets the GL viewport to full window.

---

## 3. Recent changes

- **`src/MeshCraft/MeshCraftApplication.cpp`** — Added SpriteBatch UI panels (toolbar, hierarchy, properties, status bar), auto-screenshot constructor, `drawRect()`, `drawUi()`, `objectTypeColor()`, `saveScreenshot()` (currently broken).
- **`include/MeshCraft/MeshCraftApplication.hpp`** — Added SpriteBatch, Texture2D, panel layout constants, new constructors, new private methods.
- **`src/MeshCraft/main.cpp`** — Added `--screenshot <path>` argument parsing; second constructor used for auto-screenshot mode.
- **`src/MeshCraft/Renderer/GridRenderer.cpp`** — Updated CNA API: `CurrentTechnique()` → `getCurrentTechniqueProperty()`, `Passes()` → `getPassesProperty()`.
- **`src/MeshCraft/Renderer/SceneRenderer.cpp`** — Same CNA API update at two call sites.
- **`CMakeLists.txt`** — metagl include was added then reverted (caused full CNA recompile with pre-existing bugs).

---

## 4. Current blocker / main problem

**`saveScreenshot()` cannot obtain `glReadPixels`.**

Symptom:
```
[Screenshot] glReadPixels not available
```

Affected file: `src/MeshCraft/MeshCraftApplication.cpp`, function `saveScreenshot()` (~line 797).

Cause: OpenGL ES functions are loaded by SDL3 via EGL/DRI into a private namespace. `dlsym(RTLD_DEFAULT, "glReadPixels")` searches only the main program's symbol table and returns null.

**What has been tried:**
- `dlsym(RTLD_DEFAULT, "glReadPixels")` — returns null.
- `extern "C" void* SDL_GL_GetProcAddress(const char*)` forward declaration — conflicts with SDL3's actual signature (`SDL_FunctionPointer`, not `void*`).
- Adding `${meta-gl_SOURCE_DIR}/include` to CMakeLists.txt to use metagl's `glReadPixels` — triggered full CNA recompilation with pre-existing CNA bugs; reverted.

**Correct fix:** include `<SDL3/SDL.h>` properly (SDL3 headers are at `../cna/third_party/SDL/include/` or `../cna/.sdl-prebuilt/install/include/`) and call `SDL_GL_GetProcAddress("glReadPixels")`, casting the result (`SDL_FunctionPointer`) to the GL function pointer type. This requires either adding SDL3 to MeshCraft's include path without triggering a CNA rebuild cascade, or using a helper function inside CNA/EasyGL that exposes the GL proc address lookup.

---

## 5. Known bugs and limitations

| # | Status | Description |
|---|--------|-------------|
| 1 | **confirmed bug** | `saveScreenshot()` broken — `glReadPixels` unavailable via `dlsym` |
| 2 | **incomplete** | Viewport restriction for 3D rendering does not work — `EasyGLGraphicsBackend::Clear()` resets GL viewport to full window, overriding CPU-side `setViewportProperty()` |
| 3 | **incomplete** | No text rendered in panels — labels are colour shapes only; no SpriteFont in CNA |
| 4 | **incomplete** | Ray-cast picking not implemented — click in 3D viewport deselects, doesn't pick |
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

# Reproduce screenshot bug
./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/test.ppm
# Expected output: [Screenshot] glReadPixels not available

# Convert MC3 to GLB
./cmake-build-debug/mc3/mc3togltf test/house.mc3.xml test/house.glb
```

---

## 8. Next smallest tasks

1. **Fix `saveScreenshot()` — use SDL_GL_GetProcAddress**
   - Goal: obtain `glReadPixels` via SDL3's proc address lookup instead of `dlsym`.
   - Files: `src/MeshCraft/MeshCraftApplication.cpp`, possibly `CMakeLists.txt`.
   - Approach: find SDL3 include path already exposed by CNA (e.g. via `target_include_directories` of `SDL3::SDL3` transitively), include `<SDL3/SDL.h>`, call `SDL_GL_GetProcAddress("glReadPixels")` and cast `SDL_FunctionPointer` to the GL function type.
   - Verify: `./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/test.ppm` produces a readable PPM file.

2. **Verify screenshot output is visually correct**
   - Goal: confirm PPM content shows the editor panels and scene, not a black frame.
   - Verify: `ppmtopng /tmp/test.ppm /tmp/test.png && xdg-open /tmp/test.png` (or inspect with `ffmpeg -i /tmp/test.ppm /tmp/test.png`).

3. **Fix viewport restriction for 3D rendering**
   - Goal: only the centre area (between panels) clears to the scene background colour; panel areas should stay dark.
   - Files: `src/MeshCraft/MeshCraftApplication.cpp` (`Draw()`), EasyGL backend in `../cna/`.
   - Approach: instead of using `setViewportProperty()` + `Clear()`, use a scissor test (`glScissor` + `glEnable(GL_SCISSOR_TEST)`) around the scene clear — or restrict only the `gd.Clear()` call, not the render viewport.
   - Verify: screenshot shows dark panel areas and coloured scene background only in the 3D viewport region.

4. **Ray-cast object picking**
   - Goal: left-click in 3D viewport selects the object under the cursor.
   - Files: `src/MeshCraft/MeshCraftApplication.cpp` (`handleMouseInput()`), `EditorCamera`, `SceneRenderer`.
   - Approach: unproject click position using inverse view-projection, test ray against each object's bounding box.
   - Verify: clicking a visible cube in the house scene selects it and highlights its row in the hierarchy panel.

5. **Display object names as coloured pixel characters (simple bitmap font)**
   - Goal: show object names in the hierarchy panel without requiring SpriteFont.
   - Files: new `src/MeshCraft/Ui/BitmapFont.cpp`, `drawUi()`.
   - Approach: embed a minimal 5×7 or 8×8 bitmap font; render each glyph as small `drawRect` calls.
   - Verify: hierarchy panel rows show object names in the running editor.

6. **Add automated smoke test**
   - Goal: CI can verify the binary loads a scene and exits cleanly.
   - Files: `CMakeLists.txt`, new `test/smoke_test.sh`.
   - Approach: run `MeshCraft test/house.mc3.xml --screenshot /tmp/smoke.ppm` in a virtual framebuffer (`xvfb-run`), check exit code 0 and that `/tmp/smoke.ppm` is non-empty.
   - Verify: `ctest` passes.

---

## 9. Do not do yet

- **No refactor of CNA** — CNA has pre-existing build bugs that appear when its objects are recompiled; avoid changes that trigger CNA rebuild.
- **No new features** until screenshot capture works and viewport restriction is fixed — those are the basic diagnostic tools needed to verify everything else.
- **No SpriteFont integration** until a simpler bitmap font approach is validated or CNA gains native text support.
- **No CSG rendering** until the basic picking and gizmo are working.
- **No API changes in `Mc3Document`** without checking `mc3togltf` and all test scenes.
- **No mass include path changes** in `CMakeLists.txt` — any change to `target_include_directories` that touches CNA's dependencies risks triggering a full CNA recompile with pre-existing bugs.
- **No GUI file dialog** — out of scope until panels have text and a basic widget model.

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect only the files needed for the first task (fix saveScreenshot in src/MeshCraft/MeshCraftApplication.cpp to use SDL_GL_GetProcAddress instead of dlsym). Do not refactor unrelated code. Make one small verified improvement. Build with: cd cmake-build-debug && ninja -j$(nproc). Verify with: ./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot /tmp/test.ppm && file /tmp/test.ppm. Update NEXT.md after finishing.
```
