# MeshCraft

A C++23 3D scene editor for the `.mc3.xml` format — a lightweight XML-based scene description used by the OpenEggbert project.

![MeshCraft screenshot](mesh_craft_screenshot.png)

## Description

MeshCraft lets you build and edit 3D scenes from primitive shapes, groups, materials, CSG boolean operations, and external OBJ meshes. Scenes are saved as `.mc3.xml` files and can be exported to glTF/GLB for use in games and real-time applications.

## The MC3 Format

MC3 (MeshCraft 3D) is an XML-based source format (`.mc3.xml`). It describes scenes as editable constructive objects rather than raw triangle meshes.

Key capabilities:

- primitive shapes: box, sphere, cylinder, cone, plane, torus, capsule, disk, grid, icosphere
- hierarchical groups and transforms (position, rotation, scale, pivot)
- materials: PBR (base color, roughness, metallic, emissive, normal/occlusion textures)
- CSG operations: union, difference, intersection (via Manifold)
- extrude along path (line, arc, helix, polyline, bezier)
- animation channels: position, rotation, scale, visibility, material color, deform
- reusable definitions (prefabs / instances)
- layers, tags, collision hints
- fog and environment settings
- export: glTF/GLB via `mc3togltf`; binary MCB via `mc3tomcb`

See [MC3_FORMAT.md](MC3_FORMAT.md) for the full specification.

## Architecture

| Component | Description |
|-----------|-------------|
| `mc3/` | Core library — `Mc3Document` data model, XML parser/writer, no graphics dependency |
| `mcb/` | MCB binary format read/write (fast runtime loading) |
| `mc3tomcb/` | CLI: convert `.mc3.xml` ↔ `.mcb` |
| `mc3togltf/` | CLI: export `.mc3.xml` → `.gltf` / `.glb` |
| `MeshCraft` | Editor application — Dear ImGui UI, orbit camera, gizmos, CSG, timeline |
| `../cna/` | XNA-like C++ runtime (SDL3 + OpenGL ES 3, sibling repo) |

## Building

### Prerequisites

- CMake 3.21 or newer
- C++23-capable compiler (GCC 13+, Clang 16+, MSVC 2022+)
- Python 3 + `lxml` (for XSD validation tests)
- Sibling repositories checked out next to `mesh-craft/`:
  - `cna/`
  - `sharp-runtime/`

### Build (Linux)

```sh
cmake -S . -B cmake-build-debug -G Ninja
ninja -C cmake-build-debug
```

### Graphics backend

`MESH_CRAFT_GRAPHICS_BACKEND` selects the **CNA engine** backend
(`EASYGL` | `SDL_RENDERER` | `BGFX` | `VULKAN` | `WEBGPU`, default `EASYGL`).
The editor's ImGui renderer consumes CNA draw primitives rather than native GL
calls. `EASYGL` is the fully supported source-GLSL route; the current runtime
gate also permits the separately qualified Vulkan editor path, while other
backends remain deliberately rejected until they receive real screenshot
qualification. Android and Emscripten force `EASYGL`: that selects GLES 3.0 /
WebGL 2 rather than the non-qualified SDL renderer. The CNA-free CLI tools
(`mc3togltf`, `mc3tomcb`) do not depend on the backend at all.

Android is a supported target at the source-configuration level, but no APK or
on-device run is claimed yet: this workspace has no Android NDK and CNA's
current Android cross-build is blocked in the sibling `sharp-runtime` before
graphics code compiles. See `plan.md` `AUD-042` for the exact remaining
validation and packaging work.

```sh
# Working editor (default):
cmake -S . -B cmake-build-debug -G Ninja -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL
ninja -C cmake-build-debug
```

> **Note:** sources are collected with `file(GLOB_RECURSE)`, so CMake does
> not automatically notice a newly added `.cpp` file. After adding one
> (or a new CMakeLists.txt-registered test executable), re-run the
> `cmake -S . -B cmake-build-debug` configure step before building —
> `ninja` alone will not pick it up.

### Build (Windows, MinGW cross-compile from Linux)

Requires the `x86_64-w64-mingw32-gcc`/`g++` toolchain (Debian/Ubuntu:
`apt install g++-mingw-w64-x86-64`). Write a toolchain file:

```cmake
# mingw-toolchain.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

```sh
cmake -S . -B b-mingw -G Ninja -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake -DBUILD_TESTING=OFF
cmake --build b-mingw -j$(nproc)
```

**Known blocker (as of this writing):** configure succeeds and the
build progresses through ~73% of the object graph (all of CNA's
XNA-compatibility layer, `Mc3`, Manifold, most of ImGui), then fails on
`imgui_impl_opengl3.cpp: fatal error: GLES3/gl3.h: No such file or
directory` — CNA's build configures `-DIMGUI_IMPL_OPENGL_ES3`
unconditionally for the `EASYGL` backend regardless of target platform,
and no GLES-for-Windows headers are vendored. This is a CNA-side
graphics-backend decision, not something this repo can fix on its own —
see `cna`'s own issue tracker. SQLite3/OpenSSL/LibXml2 all gracefully
disable (stubbed at compile time) rather than blocking configure when
absent for the target, and the produced `build.ninja` correctly copies
`SDL3`/`SDL3_image`/`SDL3_mixer`/`libwinpthread-1` DLLs next to
`MeshCraft.exe` and statically links `libgcc`/`libstdc++`.

### Build (Web, Emscripten)

Requires the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
(`source emsdk_env.sh` puts `emcmake`/`emcc`/`em++` on `PATH`):

```sh
emcmake cmake -S . -B b-web -G Ninja -DBUILD_TESTING=OFF
cmake --build b-web -j$(nproc)
cd b-web && python3 -m http.server 8765   # then open http://localhost:8765/MeshCraft.html
```

Builds and links cleanly, and the page loads and initializes (WebGL 2.0
context, `SDL_CreateWindow` succeeds, scene creation logs) — **but the app then
crashes on the first window-resize event before rendering a frame**, so the
viewport never appears. The crash is a `std::runtime_error` thrown inside CNA
(`GameWindow::queryClientBoundsFromSDL()` → `SDL_GetWindowSize()` reports the
video subsystem uninitialized); it is out of this repo's scope to fix. See
`NEXT.md` §4 for the full traced root cause. CLI tools (`mc3togltf`,
`mc3tomcb`) build the same way and run correctly under Node.

### Offline / vendored build (`SYS-W11-07`)

Every third-party dependency this repo fetches is pinned to an exact
version tag, so a fully offline build is possible without any code
change — CMake's built-in `FETCHCONTENT_SOURCE_DIR_<NAME>` cache
variable (uppercase of the `FetchContent_Declare` name) redirects that
dependency to a local directory instead of cloning it, verified working
in this repo directly (`cmake -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=/path/to/local/checkout ...`
logs "Using the multi-header code from /path/to/local/checkout/include/",
no network access attempted). Pre-populate each directory (a plain `git
clone <repo> --branch <tag>` works) and pass the matching variable at
configure time:

| Dependency | Repository | Tag | CMake variable |
|---|---|---|---|
| tinyxml2 | `leethomason/tinyxml2` | `10.0.0` | `FETCHCONTENT_SOURCE_DIR_TINYXML2` |
| nlohmann/json | `nlohmann/json` | `v3.11.3` | `FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON` |
| Dear ImGui | `ocornut/imgui` | `v1.91.6` | `FETCHCONTENT_SOURCE_DIR_IMGUI` |
| Manifold | `elalish/manifold` | `v3.0.0` | `FETCHCONTENT_SOURCE_DIR_MANIFOLD` |
| tinyobjloader | `tinyobjloader/tinyobjloader` | `v2.0.0rc13` | `FETCHCONTENT_SOURCE_DIR_TINYOBJLOADER` |
| tinygltf | `syoyo/tinygltf` | `v2.9.3` | `FETCHCONTENT_SOURCE_DIR_TINYGLTF` |
| cpp-httplib (optional, AI feature) | `yhirose/cpp-httplib` | `v0.18.3` | `FETCHCONTENT_SOURCE_DIR_HTTPLIB` |
| Lua (scripting, SYS-W14-18) | `lua/lua` | `v5.4.7` | `FETCHCONTENT_SOURCE_DIR_LUA` |
| sol2 (scripting, SYS-W14-18) | `ThePhD/sol2` | `v3.3.0` | `FETCHCONTENT_SOURCE_DIR_SOL2` |

```sh
cmake -S . -B b-offline -G Ninja \
  -DFETCHCONTENT_SOURCE_DIR_TINYXML2=/path/to/tinyxml2 \
  -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=/path/to/json \
  -DFETCHCONTENT_SOURCE_DIR_IMGUI=/path/to/imgui \
  -DFETCHCONTENT_SOURCE_DIR_MANIFOLD=/path/to/manifold \
  -DFETCHCONTENT_SOURCE_DIR_TINYOBJLOADER=/path/to/tinyobjloader \
  -DFETCHCONTENT_SOURCE_DIR_TINYGLTF=/path/to/tinygltf \
  -DFETCHCONTENT_SOURCE_DIR_HTTPLIB=/path/to/cpp-httplib \
  -DFETCHCONTENT_SOURCE_DIR_LUA=/path/to/lua \
  -DFETCHCONTENT_SOURCE_DIR_SOL2=/path/to/sol2
```

Omit any variable for a dependency you're fine fetching normally — they
mix freely. `../cna`/`../sharp-runtime` are never fetched at all (plain
sibling-directory `add_subdirectory`, see Prerequisites above), so they
need no override.

**Not implemented, and not possible from this repo alone:**
"package-first discovery" (trying `find_package(CNA)` before falling
back to `add_subdirectory(../cna)`) needs `cna`'s own `CMakeLists.txt`
to `install()`/export a package config first — it doesn't today (no
`install(TARGETS ...)`, no generated `CNAConfig.cmake` anywhere in that
tree). Adding that is a change to `cna` itself, out of bounds per this
project's own boundary rules (`CLAUDE.md`: "No CNA changes without
owner permission").

### Run

```sh
./cmake-build-debug/MeshCraft path/to/scene.mc3.xml
```

#### Headless one-shot flags

`MeshCraft` also supports three headless (no window shown, no interactive
loop) one-shot modes, each rendering/measuring the scene and exiting:

```sh
./cmake-build-debug/MeshCraft scene.mc3.xml --screenshot out.png   # or out.ppm — see note below
./cmake-build-debug/MeshCraft scene.mc3.xml --export out.glb
./cmake-build-debug/MeshCraft scene.mc3.xml --benchmark            # timing breakdown to stdout
```

`--screenshot`'s output format is chosen by the path's extension: `.png`
writes a real PNG; any other extension (e.g. `.ppm`) writes raw PPM (P6)
bytes regardless of what the extension says.

### Export to glTF

```sh
./cmake-build-debug/mc3togltf/mc3togltf scene.mc3.xml scene.glb
./cmake-build-debug/mc3togltf/mc3togltf --stats scene.mc3.xml scene.glb  # print export statistics
```

For safety with untrusted scenes, `mc3togltf` **rejects texture/mesh paths that
are absolute or escape the input document's directory by default** (so a
malicious `.mc3` can't read arbitrary local files into the output GLB). Pass
`--allow-external-resources` for trusted scenes that legitimately reference
files outside their own directory.

### Convert to MCB

```sh
./cmake-build-debug/mc3tomcb/mc3tomcb scene.mc3.xml scene.mcb
```

### Tests

```sh
ctest -V --test-dir cmake-build-debug
```

Re-run only the tests that failed last time (useful after fixing a
handful of failures without re-running the whole ~40s suite):

```sh
ctest --test-dir cmake-build-debug --rerun-failed --output-on-failure
```

Tests are also grouped by label (`format`/`export`/`render`/`registry`/
`ai`/`commands`) — run just one group with `ctest --test-dir
cmake-build-debug -L export`. See `TESTING.md` for the full test
reference.

## Current Features

- Full `.mc3.xml` scene load/save with XML roundtrip
- All primitive types rendered in 3D viewport
- Transform gizmos (move / rotate / scale), local/world space
- Object hierarchy panel with selection, multi-select, drag-and-drop
- Properties panel (geometry, material, transform), multi-edit
- Material editor (PBR: base color, roughness, metallic, emissive)
- CSG boolean operations (union, difference, intersection via Manifold)
- Extrude along path (arc, helix, polyline, bezier cross-sections)
- Animation timeline: keyframes, channels, cubic/step/linear interpolation
- Named layers, object tags, collision hints
- Fog, environment background, emissive bloom post-process
- First-person walk mode
- GLB export settings UI; headless screenshot mode
- Undo/redo (20-step deep-copy stack)
- Auto-save with rotating backups
- MCB binary format (`mc3tomcb`)
- glTF/GLB export (`mc3togltf`) with geometry reuse — repeated identical primitives, OBJ meshes, extrude shapes, and `<instance>` nodes share one glTF mesh buffer per unique geometry+material combination

## Current Limitations

- Bloom post-process uses CNA `RenderTarget2D` + `ShaderEffect` blur/composite
  passes; visibility still depends on authored emissive brightness. Custom
  text-shader effects are disabled on Vulkan until CNA exposes a complete
  cross-backend effect contract.
- Walk mode always retains the y=0 ground plane and now collides with scene
  primitives explicitly marked `collision="box"` (walls, platforms, and
  ceilings, including parent transforms). Other collision proxy labels remain
  unsupported rather than being approximated silently.
- Preferences dialog: auto-save interval, snap (translate/rotate/scale), grid spacing, and theme are all persisted (`savePrefsAlg`/`loadPrefsAlg`)
- Headless screenshot: a `.png` path writes a real PNG (`stbi_write_png`); every other extension (e.g. `.ppm`) writes raw PPM (P6) bytes regardless of what the extension actually says
- MCB: as of `SYS-W14-25` (2026-07-20), compression is implemented — `MeshCraft::Mcb::saveToBinary`/`saveToFile` take an opt-in `compress` parameter (default `false`, unchanged output) that zlib-deflates the document payload; `loadFromBinary`/`loadFromFile` transparently detect and decompress it. Requires this build to have been compiled with zlib available (system package, optional — see `THIRD_PARTY.md`); degrades to a clear "requires zlib"/"compiled without zlib support" error rather than misparsing a compressed file or silently ignoring the `compress` request. No editor UI toggle — see `MC3_FORMAT.md`'s MCB section for the full header layout
- CSG export to glTF: evaluated by Manifold (union/difference/intersection). Unsupported child types inside a CSG node (Plane, Disk, Grid, Mesh, Extrude) cause the export to fail with a clear error in default mode. Pass `--allow-approximate-csg` (CLI) or enable "Allow approximate CSG export" (editor) to bypass Manifold and export children separately (debug fallback, geometrically incorrect). Real CSG produces smooth normals with sharp creases, generated box/default or root-selected UV projection, and separate glTF primitives for child materials when no CSG-root material overrides them; operand UV unwraps themselves are not preserved through cuts
- No full end-to-end UI-interaction-simulation harness (nothing scripts a sequence of real user clicks/drags through the live application window) — but this understates real coverage: 36 `render`-labeled tests include real pixel-sampling checks against headless `--screenshot` output (fog, light/camera gizmos, look-through-camera, embedded GLB, background/skybox textures, LOD, CSG preview cache, etc. — see `TESTING.md`), and a handful of `unit`-labeled tests drive real ImGui widget frames (drag/click gestures, undo-snapshot timing, scene-hierarchy drag-drop) directly against production widget code with no GL context needed
- AI Assistant: not available on Emscripten or Android builds (`cpp-httplib`/OpenSSL are only fetched/linked when `NOT EMSCRIPTEN AND NOT ANDROID`). The API key is **never persisted to disk** — it lives only in the in-memory UI text buffer for the session, pre-filled from the `ANTHROPIC_API_KEY` environment variable if set, and is not part of the saved preferences file. The **Model** field (default `claude-sonnet-5`) and **Max tokens** (4096–64000, default 32000) are both user-editable in the AI panel and control the outgoing API request directly — there is no fixed/hardcoded model. **Scope** selects what's sent as context: "Full scene" serializes the entire document; "Selection only" serializes just the currently-selected objects (plus all materials/definitions, so references still resolve) and is disabled when nothing is selected. Applying a response that would drastically shrink the scene's object count (more than half, on a scene of 10+ objects) requires an explicit second "Confirm Replace" click rather than applying immediately. Response validation is structural, not semantic: it checks the XML is well-formed, has a `<mc3>` root, isn't empty, and conforms to `mc3.xsd` (element order, attribute types/patterns, ID/IDREF cross-references) — but `mc3.xsd` has no numeric range constraints (no `minInclusive`/`minExclusive` anywhere), so a geometrically nonsensical response (e.g. negative `size`/`radius`) passes validation and applies to the scene as-is
- Model Registry: no thumbnail column (design placeholder only, never implemented — see `m1m2m3.md`); requires the system SQLite3 library on desktop builds (stubbed out, feature disabled, on Emscripten/Android); no sync between multiple registry database files — it's a single local SQLite file at `~/.meshcraft/modelregistry.sqlite3`, not backed up or shared automatically
- SVG textures (`<texture type="svg">`): external `.svg` files and inline CDATA markup are rasterized by NanoSVG (maximum output dimension 2048px) for both the live editor viewport and glTF export. The live cache has compact content-hash keys and automatically re-rasterizes an external SVG when its file changes; malformed input is warned once per unchanged source. `wrap_u`, `wrap_v`, and `filter` round-trip and affect both the viewport sampler and glTF sampler; `mip_maps` affects glTF, while the live CNA texture remains level-zero because its available API cannot generate a mip chain. `.gltf` exports write a generated PNG beside the document; `.glb` embeds it. Unsupported or malformed SVG is skipped with a named warning rather than dropping the material silently.
- Embedded GLB (`<mesh src="embed:id"/>`): external self-contained `.glb` files and inline base64 GLB both resolve in `mc3togltf` and in the live viewport. The asset's default-scene transforms are flattened into the Mesh object's local geometry; MC3 retains authority over the material. The loader rejects loose companion-file `.gltf`, non-triangle primitives, animation/skin/morph data, malformed paths/data, and assets over the documented 64 MiB/300,000-triangle limits with a named warning instead of importing an unsafe partial asset.
- N3–N7 scene data (scripts, sounds, music, triggers, scene states, meta): fully round-tripped (XML/MCB/XSD) and editable. As of `SYS-W14-18`/`SYS-W14-19`/`SYS-W14-20` (2026-07-20), scripts (`type="lua"`) actually run — a real sandboxed Lua interpreter (`LuaScriptRunner`) with `def:place()`/`place_at()`/`has_socket()` (compose-time socket placement) and `scene:find()` (read/write any object's position/rotation/scale/visible/material) — triggers actually fire (an explicit "Fire" action executes `play-action`/`play-sound`/`play-music`/`run-script` steps for real), and scene states actually apply (an explicit "Apply State" action writes a state's overrides onto the matching live objects). What's still data-model-only: there is no in-scene EVENT system (collision/click/timer) that fires a trigger or switches a scene state automatically — only the explicit manual actions above do. See `MC3_FORMAT.md`'s per-section status notes and `plan.md`'s `SYS-W14-##` rows.
- `<library>`/`<imports>` (R101/R110, `Mc3ImportResolver`): as of `SYS-W14-21` (2026-07-20), the editor actually resolves `<imports>` against the loaded document's own directory and merges the imported libraries' definitions into `document_.definitions`, so `<instance definition="namespace:id">` referencing an imported library's definition renders instead of silently resolving to nothing — this now runs automatically right after every load (initial launch, Open dialog, Open Recent, autosave recovery) plus an explicit "Resolve Imports" button in the Imports tab for re-resolving after editing the rows without a full reload. A resolution failure (missing library file, content-hash mismatch, an import cycle/depth-limit) doesn't fail the whole document load — it's reported via the status bar and the affected imports simply stay unresolved.
- `<texture mip_maps="...">`: as of `SYS-W14-22` (2026-07-20), honored by `mc3togltf` — `false` makes the exporter emit a plain (non-mipmap) glTF sampler `minFilter` instead of unconditionally requesting a mipmapped one. **Not honored by the live editor viewport** — CNA's `Texture2D` asset-loading path has no mipmap-generation option (confirmed in CNA's own OpenGL backend: it explicitly does not generate mipmaps by default for the filter that path uses), and closing that would need a CNA-side API change, out of scope per this repo's CNA boundary. See `MC3_FORMAT.md`'s Textures section for the full writeup.
- `<texture color_space="...">`: as of `SYS-W14-23` (2026-07-20), `mc3togltf` still doesn't re-encode pixels at export time (glTF 2.0's per-slot encoding — baseColor/emissive sRGB, normal/metallic-roughness/occlusion linear — is spec-mandated and has no per-texture override), but now warns when a texture's declared `color_space` conflicts with its slot's mandated encoding (e.g. a normal map declared `color_space="srgb"`), naming the material, texture, slot, and both the declared and required values, instead of silently doing nothing with the mismatch.
- `<uv_mapping projection="box"/"sphere">`: as of `SYS-W14-24` (2026-07-20), implemented by `mc3togltf` — box/triplanar picks the dominant axis per vertex (from its normal, or from its direction from the mesh's local bounding-box center if normals are absent) and projects using raw local-space coordinates on the other two axes; sphere is an equirectangular mapping normalized to `[0,1]`. Both used to be a warn-only no-op (silently falling back to the primitive's default planar unwrap). Editor-viewport parity remains out of scope — `SceneRenderer.cpp` never reads `<uv_mapping>` at all (any attribute, not just `projection`), so the live viewport always shows the default planar unwrap; `mc3togltf`'s export is the ground truth for appearance.
- `<light brightness="...">`: as of `SYS-W14-26` (2026-07-20), `mc3togltf` converts per light type instead of writing the same raw number into every type's glTF `intensity` regardless of its physically different mandated unit (directional = lux, point/spot = candela). Directional passes through unconverted (matches Blender's own glTF exporter's Sun-lamp convention); point/spot convert via `brightness / (4π) × 683` (683 lm/W is the CIE luminous-efficacy constant, matching Blender's Point/Spot-lamp formula). A deliberate, documented scale factor per light type, not a claim of full physical calibration — `brightness` still has no editor-side lux/candela input mode. See `MC3_FORMAT.md`'s Lights section for the full writeup.
- `<ambient>` light: glTF 2.0 core + `KHR_lights_punctual` have no ambient-light concept at all (a real spec gap). As of `SYS-W14-27` (2026-07-20), instead of being dropped outright it's approximated — every `<ambient>` light's `color × brightness` in the document is summed and baked into every material's own `emissiveFactor`, tinted by that material's `base_color` and clamped to `[0,1]`, so a glTF-conformant viewer isn't fully unlit wherever an ambient fill was authored. A lossy approximation, not real global illumination — see `MC3_FORMAT.md`'s Lights section for the full formula.
- Live-viewport light shading (`AUD-077`, 2026-07-20): the editor's live preview now actually shades using `doc.lights` — up to 3 `directional` lights map onto CNA `BasicEffect`'s real `DirectionalLight0-2` slots and the first `ambient` light onto `AmbientLightColor` — instead of always using a fixed 3-point default rig regardless of what's authored. `point`/`spot` lights remain gizmo-only in the live preview (`BasicEffect`, a faithful port of XNA's fixed-function lighting, has no position/attenuation API at all — only directional slots + ambient); `mc3togltf`'s export already handles every light type correctly. See `MC3_FORMAT.md`'s Lights section for the full writeup.

## Platform Support Matrix

Status as of the S16 (Cross-Platform Stability) stabilization pass, re-verified
2026-07-07 against a fresh MinGW cross-compile attempt (with the
`CNA_ENABLE_NET=OFF` fix — see `plan.md`'s "Post-650 Follow-Up Findings" — applied).
Web-column rows re-diagnosed 2026-07-11: the blank canvas is a downstream
symptom of a CNA-side crash on the first resize event, not a canvas-sizing
config bug — full trace in `NEXT.md` §4. The earlier sizing theory is archived
in `docs/history/web_issues.md`.
Legend: ✅ verified working &nbsp; 🟡 partially verified / known gap &nbsp;
❌ not available on this platform &nbsp; ❓ not yet attempted (no toolchain
available to test with).

| Feature | Linux | Windows (MinGW) | Web (Emscripten) | Android |
|---|---|---|---|---|
| Full desktop/native build | ✅ | 🟡 the full GUI editor (`MeshCraft.exe`) still fails, but on the same pre-existing CNA-side `GLES3/gl3.h` header gap as before (`imgui_impl_opengl3.cpp` — CNA configures `-DIMGUI_IMPL_OPENGL_ES3` unconditionally for the `EASYGL` backend regardless of target platform), **plus** 3 separate `../sharp-runtime`-side `-Werror` build failures in its `System.Net.Sockets`/`System.Xml` namespaces (`afunix.h`'s `ADDRESS_FAMILY` on this MinGW version, an unused-function warning, a sign-compare warning) — none of these are in this project's own source, all out of scope to fix without CNA/sharp-runtime maintainer involvement. **New finding**: the two CNA-free CLI tools (`mc3togltf.exe`, `mc3tomcb.exe`) build and link successfully as real Windows PE32+ executables, since neither links SHARP_RUNTIME or CNA at all — confirmed by running `file` on the actual output binaries, not just a partial object count | 🟡 **update 2026-07-09**: the *last-known-good* build (2026-07-06 artifacts) still builds/runs fine, but a *fresh* rebuild now fails — `../sharp-runtime` gained a new Emscripten-only regression since then (16 `-Werror` failures + 1 hard `std::chrono::clock_cast` compile error). See `docs/history/web_issues.md` (archived). | 🟡 source selects GLES/EASYGL; no NDK configure, APK package, or on-device test yet. CNA's current NDK cross-build is blocked in sibling `sharp-runtime` before graphics compile |
| App launches / runs | ✅ | ❌ (build doesn't complete) | ❌ loads and initializes (`SDL_CreateWindow`, WebGL2 context, scene creation) but then **crashes on the first resize event**, before any frame renders — see below | ❓ |
| 3D viewport rendering | ✅ | ❌ (build doesn't complete) | ❌ never reached. **Root cause (re-diagnosed 2026-07-11, supersedes the earlier "canvas 0×0" theory)**: an uncaught `std::runtime_error` from CNA's `GameWindow::queryClientBoundsFromSDL()` — `SDL_GetWindowSize()` reports "Video subsystem has not been initialized" on the first `SDL_EVENT_WINDOW_RESIZED`, killing the wasm module. 100% inside CNA; not fixable from this repo. Full trace in `NEXT.md` §4. | ❓ |
| Shaders (GLSL ES 3.00 / WebGL2) | ✅ (desktop GL) | ❌ (build doesn't complete) | ✅ all 7 CNA EasyGL 3D shader programs are `#version 300 es` and compile/link cleanly | ❓ |
| Config/prefs/recent-files/keybindings persistence | ✅ `~/.config/meshcraft` | ✅ `%APPDATA%\meshcraft` (code-verified; no working build to run it against yet) | ❌ **correction 2026-07-09**: not actually persisted. `meshcraftConfigDir()` (`src/MeshCraft/MeshCraftPrivate.hpp`) has only `_WIN32`/POSIX branches, no `__EMSCRIPTEN__` branch, so it resolves into Emscripten's in-memory MEMFS. `-lidbfs.js` is linked (`CMakeLists.txt`) but `FS.mount`/`FS.syncfs` are never called anywhere in this repo or `../cna` — prefs/recent-files/keybindings/macros are silently lost on every reload. See `plan_deep_audit.md` AUDIT-0050 for the tracked implementation (currently blocked on the Emscripten crash — see `NEXT.md` §4). | ❓ (falls through to the same non-Windows `$HOME`-based logic as Linux; not verified on-device) |
| SQLite Model Registry | ✅ (or gracefully stubbed if SQLite3 dev package absent) | 🟡 gracefully stubbed when SQLite3 absent (code-verified; no working build to run it against yet) | ❌ always stubbed (`MESHCRAFT_HAS_SQLITE3` never defined) | ❌ always stubbed (same guard as Web) |
| AI Assistant (Claude API) | ✅ (or gracefully stubbed if OpenSSL absent) | 🟡 same as SQLite3 above | ❌ always stubbed (`MESHCRAFT_HAS_AI` never defined) | ❌ always stubbed (same guard as Web) |
| File dialogs (text-path-field fallback) | ✅ | ✅ (no native OS dialog anywhere — plain `ImGui::InputText`, platform-agnostic by construction) | ✅ | ✅ |
| Path handling (UTF-8, spaces, separators) | ✅ (round-trip tested) | ✅ (`std::filesystem::path` used consistently; not executable-tested on real Windows) | ✅ (`std::filesystem::path`; not executable-tested) | ✅ (same code path) |
| Static runtime linking / DLL bundling | N/A | ✅ `-static-libgcc -static-libstdc++`, plus `SDL3`/`SDL3_image`/`SDL3_mixer`/`libwinpthread-1` DLLs copied next to the `.exe` (build-graph verified) | N/A (single `.wasm`, no separate runtime DLLs) | ❓ |
| wasm exceptions / preloaded test assets | N/A | N/A | ✅ `-fwasm-exceptions` on compile+link; `--preload-file test@/test` packages all `test/` assets into `MeshCraft.data` | N/A |
| CI | ✅ active GitHub Actions: standalone component matrix plus full EASYGL editor build/tests and Vulkan editor configure/build | ❌ no Windows runner | ❌ no Web runner | ❌ no Android runner |

## Reporting a Crash

MeshCraft has no built-in crash reporter — if it crashes, capture a
backtrace with the OS-standard tools below and attach it to a
[GitHub Issue](https://github.com/openeggbert/mesh-craft/issues) along
with the scene file (if any) and the exact command line that triggered
it.

**Linux:**

```sh
# Debug build already has debug info (see "Build (Linux)" above)
ulimit -c unlimited                       # enable core dumps for this shell
./cmake-build-debug/MeshCraft scene.mc3.xml
# after it crashes, find the core file (verified: named core.<pid> by
# default on this system — check `cat /proc/sys/kernel/core_pattern`
# if yours differs) and load it:
gdb ./cmake-build-debug/MeshCraft core.<pid>
(gdb) bt full                              # full backtrace — this is what to attach
```

If it hangs instead of crashing, attach a backtrace from a running
process instead: `gdb -p $(pgrep MeshCraft) -batch -ex "bt full"`.

**Windows:** no build/test verification of this exists in this
environment (Linux-only dev setup) — the standard approach is to let
Windows Error Reporting generate a `.dmp` file (Control Panel → System
→ Advanced → check "Windows Error Reporting" settings, or trigger one
directly via Task Manager → right-click the hung/crashed process →
"Create dump file") and open it in WinDbg or Visual Studio for a
backtrace. Treat this as unverified guidance, not a tested procedure.

## Backup and Recovery

Auto-save and backups live **next to the scene file itself** — there is
no separate cache/config directory for them.

- **Auto-save**: while a file `scene.mc3.xml` is open, the editor
  periodically writes the current in-memory state to
  `scene.mc3.xml.autosave` (interval configurable in Preferences,
  default 60s). This file is deleted automatically on the next explicit
  Save — it's a crash-recovery net, not a permanent artifact.
- **Recovering after a crash**: reopen `scene.mc3.xml` normally. If a
  `.autosave` file exists and is newer than the saved file, the status
  bar shows "Autosave found — may be newer than saved file: ..." for a
  few seconds — this is a **notification only**, not an automatic
  prompt with a restore button. To actually recover: open
  `scene.mc3.xml.autosave` directly (File → Open, or rename it to end
  in `.mc3.xml` first since the app expects that extension), check it
  looks right, then Save As over the original filename.
- **Backup rotation on every explicit Save**: before writing, the
  previous on-disk content is preserved as `scene.mc3.xml.backup.1`
  (most recent prior version); if a `.backup.1` already existed, it's
  first renamed to `scene.mc3.xml.backup.2` (previous-to-that version)
  — a fixed 2-slot ring buffer, nothing older than 2 saves back is
  kept. To recover from a bad save (e.g. accidentally saved over good
  work with something wrong), copy `.backup.1` (or `.backup.2` for one
  save further back) over `scene.mc3.xml`.

## License

MIT — see [LICENSE](LICENSE).
