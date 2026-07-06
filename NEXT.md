# NEXT.md

_Last updated: 2026-07-06, commit `0c504ea` + this session's S16 work (develop)_

---

## 1. Project summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` scene format —
a lightweight XML-based scene description used by the OpenEggbert project.
It uses Dear ImGui for its UI, running on **CNA** (an XNA-style SDL3 +
OpenGL runtime, a separate sibling repo at `../cna` — never modified from
this repo) and **SHARP_RUNTIME** (`../sharp-runtime`, the math library
CNA's backend depends on). Scenes export to glTF/GLB via **mc3togltf** and
to a compact binary format via **mc3tomcb**.

**Main goal:** reach a fully stabilized, test-covered codebase before
adding new features. All work is tracked in `plan.md` as ~650 `STAB-XXXX`
tasks across sections S0–S20, gated by a Gate 0–6 checklist.

**Current phase:** Stabilization. Gates 0–5 are closed; Gate 6
(Documentation) is exhausted for this environment (everything reachable
without an external tool or a live display is done). Sections S0–S15 are
fully closed. **S16 (Cross-Platform Stability)** is in progress.

**Important architectural decisions:**
- `mc3/` and `mcb/` are pure C++ static libs with **no** CNA/ImGui
  dependency and must stay independently buildable.
- `mc3togltf/` and `mc3tomcb/` are standalone CLI + lib targets, also
  CNA-free.
- CNA is added as a sibling CMake subdirectory
  (`add_subdirectory(../cna ...)`) and must not be modified from this
  repo — a separate Claude Code instance owns it.
- The editor (`src/MeshCraft/`) is CNA-coupled, but much of its
  command/parsing/validation/render-math logic has zero actual CNA
  dependency once ImGui rendering and app-state bookkeeping are set
  aside — that pure logic lives in CNA-free headers (functions suffixed
  `Alg`, e.g. `EditorAlgorithms.hpp`) that both the real app code and the
  headless test suite `#include` and call directly. See §6.

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`, generated with CLion's bundled cmake
  4.2.2): last full reconfigure + rebuild was clean, 0 warnings from
  MeshCraft's own sources, at commit `18b1809`.
- **Release** (`b-release/`): exists from earlier in this project's
  history; not re-verified in the current session — re-run the commands
  in §7 before relying on it.
- **Standalone (CNA-free) component builds** (`mc3`, `mcb`, `mc3togltf`,
  `mc3tomcb`, each configures/builds/tests independently of the root
  project): last confirmed passing earlier in this project's history —
  re-verify with §7's commands if you depend on this.
- **MinGW (Windows) cross-compile**: configure succeeds; build reaches
  ~73% (328/449 objects) before failing on a CNA-side issue. See §4.
- **Emscripten (web) build**: configure and build both succeed 100%
  (449/449, exit 0) and the compiled JS/WASM genuinely executes (verified
  via Node). Loaded in a real browser: initializes correctly, but renders
  a blank canvas — the 3D editor is not yet usable on web. See §4.

### Tests
**46/46 CTest pass** in Debug as of the last run in this session (before
commit `18b1809`, which only changed documentation). `ctest -N` lists
all 46 by name. Notably:
- `mc3_commands` (~450+ assertions): editor command algorithms, undo/redo,
  auto-save/backup, keybinding/macro persistence, hierarchy filtering,
  AI-panel lifecycle, viewport ray-cast picking, click-selection
  resolution, camera view presets, export/import material+texture
  collection, merge-scene collision handling.
- `mc3_registry` (~94 assertions): ModelRegistry SQLite CRUD/search.
- `mc3_ai` (~55 assertions): AiAssistant JSON + AI-response validation
  pipeline, mock-HTTP-server round-trips.
- `mc3_roundtrip` (~300+ assertions): full XML parser/writer roundtrip,
  including `<include>` across directories with real files on disk.
- `mcb_roundtrip` (~50 assertions): MCB binary roundtrip.
- Several real-render smoke tests (`smoke_test*`, `fog_linear_test`,
  `point_light_gizmo_test`, `spot_light_gizmo_test`,
  `look_through_camera_test`, `csg_cache_test`, `background_texture_test`,
  `skybox_texture_test`, `lod_test`, `editor_export_test`) that run the
  actual `MeshCraft` binary in `--screenshot` headless mode and verify
  genuine pixel output or printed internal counters, not a stub render.
- `mc3togltf_*` (~18 tests): CSG, materials, textures (including real
  GLB image embedding and `<include>`d definitions), animation, instance
  variants, large scenes, help text, no-partial-output-on-error.
- `mc3tomcb_roundtrip` + `mc3tomcb_error_handling`: CLI round-trip and
  error paths (missing input, write-protected output).

### Tools / libraries available
- `MeshCraft` — editor executable, version `0.1.0` (`--version` to print
  it). Builds and runs on Linux; the 3D viewport is not fully integrated
  into the render loop in interactive mode, and the Emscripten build
  doesn't visually render yet (see "What does NOT work yet").
  - `--screenshot <path>` — render scene headlessly to a PPM file (always
    PPM regardless of the extension given) and exit.
  - `--export <path>` — export the loaded scene to `.glb`/`.gltf`
    non-interactively and exit (same codepath as the editor's File →
    Export menu; also usable for real scripted/batch export).
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb` (`--help`/`-h` now
  works; `--allow-approximate-csg`, `--stats`).
- `mc3tomcb` — bidirectional CLI: `mc3.xml` ↔ `.mcb`.
- `Mc3` / `Mcb` / `mc3togltf_lib` — static libs (scene data, binary
  serialization, glTF export).

### What works
- Full XML round-trip for all primitive types and all N1–N7 extensions,
  including `<include>` libraries that live in a *different* directory
  than the main scene (fixed this session — previously silently broke).
- MCB binary round-trip, matching the XML feature set.
- XSD validation for every test fixture and for AI-generated scene XML
  before it's applied to the live scene.
- glTF/GLB export: primitives, animations, CSG, materials (with real
  embedded texture data, verified byte-for-byte), lights, cameras,
  instances, groups, OBJ mesh import, `<include>`d definitions.
- SQLite-backed asset registry: open/save/search/remove/migration
  (gracefully disabled if SQLite3 isn't available at configure time —
  fixed this session, was previously a hard `REQUIRED` dependency).
- Editor undo/redo (snapshot-based, capped at 20).
- Auto-save + 2-slot rotating backup on save.
- AI Assistant: sends scene + prompt to the Claude API, validates the
  response, applies it or saves definitions to the registry.
- Viewport ray-cast click-to-select works for every object type.
- Material export/import (`.mc3mat.xml`) now correctly carries its
  referenced textures both ways, with correct collision handling on
  import (fixed this session — previously silently dropped textures).
- Export Subtree as Template now correctly carries its referenced
  materials/textures (fixed this session — previously silently dropped
  them, same bug class as material export).
- Real headless rendering verified via `--screenshot` for: sample
  scenes, missing-mesh/missing-material fallbacks, orthographic camera,
  linear fog, point/spot light gizmos, look-through-camera override,
  background texture, skybox (fixed this session — was completely
  invisible before, a VAO-based draw call silently produced nothing
  under this environment's GL setup).
- Emscripten and MinGW builds are both partially proven: Emscripten
  builds and runs (WASM genuinely executes); MinGW builds to ~73% before
  a CNA-side blocker (see §4).

### What does NOT work yet
- `EditorViewport` is not integrated into the `MeshCraftApplication`
  render loop.
- **The Emscripten web build doesn't visually render the 3D editor** —
  see §4, this is the main open problem.
- SVG texture rasterization: parsed/serialized/round-tripped, but
  `GltfExporter` never reads the SVG texture map — silently dropped from
  export (a warning is printed, at least).
- Embedded glTF (`<mesh src="embed:id"/>`): parsed/serialized, but
  `GltfExporter` treats `embed:id` as a literal OBJ path, which fails —
  export continues with an empty (meshless) node, doesn't crash. This is
  a deliberate, documented limitation (real fix = parsing external
  GLB/base64 data, a new feature), not a quick bug.
- N3–N7 scene data (scripts, sounds, music, triggers, scene states,
  meta): fully round-tripped but not executed at runtime anywhere.
- CI workflow exists but is parked deactivated under `.github_/` (see
  §4); no automated full-editor (CNA + SDL3) build/test job runs
  anywhere currently.
- MinGW (Windows) cross-compile builds to ~73% then fails on a CNA-side
  GLES3 header issue (see §4) — out of scope for this repo to fix.
- A cluster of visual toggles (bounding-box overlay, SSAO, bloom,
  wireframe mode, translate/rotate gizmos, gizmo-drag delta overlay,
  per-selection poly stats, shadow-map debug, locked-object outline,
  proportional-editing falloff sphere, large-scene live FPS) are
  confirmed correct by code reading but can't be re-verified fresh in
  this headless environment — see §5.

---

## 3. Recent changes

Most recent work (this session) closed out **S14** and **S15** entirely
and started **S16**. Highlights, newest first:

- **Emscripten web build**: verified `emcmake`/build succeed fully
  (449/449) and the compiled code genuinely executes (Node smoke test);
  live-browser testing found the 3D editor renders a blank canvas —
  documented as a new, undiagnosed limitation (§4).
- **`CMakeLists.txt`**: `find_package(SQLite3)` is no longer `REQUIRED` —
  it was blocking any environment/toolchain without a SQLite3 dev
  package (found while attempting the MinGW cross-compile), contradicting
  the codebase's own established "SQLite3 is optional, `ModelRegistry`
  stubs handle its absence" design.
- **`Mc3XmlParser.cpp`**: fixed `<include>` silently breaking relative
  texture/mesh paths when the included file lives in a different
  directory than the main scene — added `rebaseRelativePath()` /
  `rebaseDefinitionMeshSources()`.
- **`MeshCraftApplication_UiOverlays.cpp` / `EditorAlgorithms.hpp`**:
  fixed Material Export/Import silently dropping referenced textures
  both on export and (separately) on import; added
  `exportMaterialAlg()` / `importMaterialsAlg()`.
- **`mc3togltf`**: added `--help`/`-h` (previously absent — `--help` was
  parsed as a literal, nonexistent input filename); verified no partial
  output file is left on disk on error; verified `<include>`d
  definitions and real GLB texture embedding both work correctly.
- **`MeshCraftApplication_Commands.cpp` / `EditorAlgorithms.hpp`**: fixed
  Export Subtree as Template silently dropping referenced
  materials/textures; added `exportSubtreeTemplateAlg()`.
- **`MeshCraftApplication.cpp`**: fixed the equirectangular skybox
  rendering nothing at all (VAO-based draw silently failed under this
  environment's GL setup; switched to the same `gl_VertexID` procedural
  quad pattern the working bloom passes already use); fixed a related
  wrong-FOV bug in the same function; added a `--export <path>` CLI flag
  for non-interactive GLB/glTF export; fixed drag-drop's path-routing
  logic duplicating (instead of calling) an already-tested function.
- **`SceneRenderer.cpp`/`.hpp`**: added a proportional-editing falloff
  radius indicator (previously missing entirely, despite the underlying
  math being correct and tested); added a CSG cache-hit counter to prove
  the content-hash cache actually prevents re-evaluating static CSG
  objects every frame.
- Extensive new headless pixel-sampling tests were added throughout for
  rendering features (fog, gizmos, camera override, background/skybox
  textures, LOD, CSG cache) and CLI behaviors (export flag, help text,
  error handling on both `mc3togltf` and `mc3tomcb`).

Full history is in `git log --oneline`.

---

## 4. Current blocker / main problem

**No blocker to local development or testing on Linux** — Debug builds
and passes 46/46 tests as of `18b1809`.

The most important **open problem** is the Emscripten web build's blank
canvas:

- **Symptom**: `MeshCraft.html`, served locally
  (`python3 -m http.server` from the build dir) and opened in a real
  browser, loads and initializes correctly — `SDL_CreateWindow`
  succeeds, `EasyGLGraphicsBackend` initializes over WebGL 2.0
  (`OpenGL ES 3.0 (WebGL 2.0)`), `[MeshCraft] New scene` prints, no
  crash, no console error — but the `<canvas>` stays a blank/black
  rectangle. The 3D editor is not visually usable.
- **Failing command**: none technically fails; the failure is purely
  visual/behavioral in the browser. To reproduce: build with Emscripten
  (see §7), serve the output directory over HTTP, open
  `MeshCraft.html` in a browser, open its devtools console.
- **Affected files/modules**: likely `MeshCraftApplication::Draw()`/
  `EndDraw()` (`src/MeshCraft/MeshCraftApplication.cpp`), ImGui's
  font-atlas/texture upload path, or a GL call that's silently a no-op
  under WebGL2/GLES3 but fine under desktop GL. Not yet narrowed down
  further than that.
- **Suspected cause**: unconfirmed. Ruled out one major hypothesis (see
  below). Remaining candidates: ImGui font-atlas texture upload failing
  under WebGL2, an unsupported GL call silently no-op'ing every frame,
  or a canvas-sizing/viewport issue specific to the Emscripten shell.
- **What has already been tried**: investigated (read-only; CNA is out
  of scope to modify from this repo) whether CNA's game loop even keeps
  running under Emscripten — a black canvas after one successful frame
  often means the app fell out of `main()`, since Emscripten requires a
  registered callback (`emscripten_set_main_loop`) to keep looping,
  unlike a native build's `while` loop. Confirmed this is **not** the
  cause: CNA's `Game::RunLoop()` (`../cna/src/Microsoft/Xna/Framework/
  Game.cpp:811-825`) correctly branches on `#if defined(__EMSCRIPTEN__)`
  and calls `emscripten_set_main_loop(EmscriptenMainLoopCallback, 0, 1)`
  generically for every backend including EasyGL — so the render loop
  genuinely keeps running frame after frame. The actual per-frame cause
  of the blank canvas was not root-caused; doing so would need temporary
  frame-counter/GL-error diagnostic logging added to MeshCraft's own
  `Draw()`/`EndDraw()`, a rebuild, and another user retest in a real
  browser — deferred at the user's request as the next thing to pick up.

A second, lower-priority open item: **MinGW (Windows) cross-compile
builds to ~73%** (328/449 objects) then fails with
`imgui_impl_opengl3.cpp: fatal error: GLES3/gl3.h: No such file or
directory` — CNA's build configures `-DIMGUI_IMPL_OPENGL_ES3`
unconditionally for the `EASYGL` backend regardless of target platform,
and no GLES-for-Windows headers are vendored. This is a CNA-side
decision, out of scope to fix from this repo — needs the CNA maintainer
to vendor GLES headers for Windows or select a desktop-GL path for
non-Linux targets.

A third, purely **operational** issue, unrelated to the above: CI is
parked deactivated under `.github_/workflows/ci.yml` (GitHub only treats
`.github/workflows/` as live) because the git remote's credentials lack
the scope needed to activate it. `git push` itself has also intermittently
been denied then started working again unprompted during this session,
for reasons outside this repo's control — if push fails, don't touch
git/remote config; just keep committing locally and retry later.

---

## 5. Known bugs and limitations

- **Emscripten web build renders a blank canvas** — see §4. _confirmed
  via live user testing, not fixed, needs frame-level diagnostic
  logging + another browser-retest round-trip to narrow down further._
- **MinGW cross-compile fails at ~73% on a CNA-side GLES3 header
  issue** — see §4. _confirmed, blocked on CNA, not fixable from this
  repo._
- **CI cannot be activated with the current git credentials** — the
  workflow file is committed but parked at `.github_/workflows/ci.yml`.
  _confirmed; needs owner action on the git remote/credentials._
- **SVG texture rasterization** not implemented in `GltfExporter` — a
  material referencing an SVG texture prints a warning and omits it.
  _incomplete, documented, blocked on a library choice (librsvg vs.
  NanoSVG)._
- **Embedded glTF** (`embed:id`) fails to parse as an OBJ path during
  glTF export; export continues with an empty node, doesn't crash.
  _incomplete, deliberately documented as a limitation (real fix is a
  new feature: parsing external GLB/base64 data), not attempted._
- **`mc3.xsd` has no numeric range constraints** — a schema-valid AI
  response can contain a negative `size`/`radius` and it applies
  unchanged. _confirmed, documented in `README.md`, not fixed._
- **`mc3.xsd`'s `mip_maps` texture attribute has zero implementation** —
  parses fine, does nothing anywhere. _confirmed, not fixed, no assigned
  STAB-XXXX ID._
- **A cluster of visual toggles need a live display to re-verify**:
  bounding-box overlay, SSAO, bloom, wireframe mode, translate/rotate
  gizmo visibility, the gizmo-drag delta overlay, per-selected-object
  poly-stats display, shadow-map debug overlay, locked-object outline,
  the proportional-editing falloff sphere (newly implemented this
  session, needs visual confirmation), and large-scene live FPS — all
  confirmed correct and safe by code reading, none reachable via the
  headless `--screenshot` path (no CLI/document/prefs hook exists to
  force them on/pre-select or pre-lock an object, and FPS itself
  requires a sustained interactive loop). _needs verification by a
  human with a live display._
- **The OBJ-import Browse button** (a plain ImGui text-entry popup, no
  native OS file dialog) and **editor drag-drop's actual SDL drop
  event delivery** both need a live UI session to click/drag through —
  their underlying logic is confirmed correct and headlessly tested.
  _needs a live display._
- **`--screenshot` always writes raw PPM**, regardless of the output
  path's extension (e.g. `--screenshot out.png` still writes PPM bytes)
  — this is deliberate and long-standing, relied on by dozens of
  existing test scripts; not a bug, but the naming is misleading if you
  don't know this. _confirmed, deliberate, documented._
- **N3–N7 (scripts/sounds/music/triggers/scene states/meta) are
  data-only** — round-tripped but nothing executes them at runtime.
  _intended at this stage, not a bug._
- **`mc3` standalone build skips `mc3_commands`** — needs
  `EditorAlgorithms.hpp` from the editor tree, absent in a standalone
  checkout. _intended, not a bug._
- **3 `plan.md` items cannot be completed in this environment**:
  STAB-0642 (needs Blender), STAB-0643 (needs a browser — partially
  now possible, see §4's Emscripten findings), STAB-0650 (needs CI
  actually running). _flagged, needs external tooling/action._

---

## 6. Architecture notes

```
MeshCraft (editor exe)
├── CNA (SDL3/OpenGL runtime — sibling repo at ../cna, DO NOT MODIFY)
├── Mc3 (static lib — mc3/): Mc3Document, XML parser/writer, all types
├── Mcb (static lib — mcb/): McbWriter, McbReader, MCB binary format v1
├── mc3togltf_lib (static lib): GltfExporter, MeshBuilder, CsgEvaluator
├── mc3togltf (CLI exe): thin wrapper around mc3togltf_lib
└── mc3tomcb (CLI exe): mc3.xml <-> mcb converter (links Mc3 + Mcb)
```

**Data flow:** `.mc3.xml` → `Mc3XmlParser` → `Mc3Document` (in-memory
model) → either `Mc3XmlWriter` (save), `McbWriter` (binary), or
`GltfExporter` (glTF/GLB).

**`doc.sourcePath` is a single directory per document.** Everything
(texture URIs, OBJ mesh sources, etc.) resolves relative to it. When a
document pulls in an `<include>`d file from a *different* directory,
paths from that file must be rebased to resolve correctly against
`doc.sourcePath` — see `Mc3XmlParser.cpp`'s `rebaseRelativePath()` /
`rebaseDefinitionMeshSources()` (added this session; this had been
silently broken before).

**Undo/redo:** snapshot-based. `undoStack_`/`redoStack_` (capped at 20)
hold `std::vector<Mc3::Mc3Document>`; `pushUndo()` stores a deep copy of
`document_` before each mutating command.

**The "Alg mirror" pattern**: editor/AI logic in CNA-coupled `.cpp` files
often has zero actual CNA/ImGui dependency once app-state bookkeeping and
rendering are set aside. Pure logic lives in a CNA-free header
(`include/MeshCraft/EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`),
functions suffixed `Alg`, and the real `.cpp` `#include`s and calls them
directly — single source of truth, no duplication. The headless test
suite (`mc3/test/editor_commands_test.cpp`) calls the same functions
directly with no CNA/SDL3/ImGui dependency.
**Caution**: this session found multiple cases where a real `.cpp` had
its own inline duplicate of an `Alg` function instead of calling it,
which had silently drifted out of sync (`mergeDocumentsAlg` vs. the real
merge code after a later feature was added to only one of them) — when
touching one side of an Alg-mirrored pair, always check the other side
is actually wired up and matches.

**GL rendering caution**: a VAO-based vertex-attribute draw path was
found completely non-functional in this environment's GL setup this
session (the skybox — fixed by switching to a `gl_VertexID`-based
procedural approach, matching what the working bloom passes already
use). If a new raw-GL draw call renders nothing with no error, consider
this failure mode.

**Embedding a resource file at compile time**: `mc3.xsd` is compiled into
`MeshCraft`/`ai_test` as a raw string constant, generated at CMake
**configure** time (`cmake/Mc3XsdEmbed.hpp.in`). **Editing `mc3.xsd`
requires a full reconfigure, not just a rebuild.**

**Hard constraints / invariants:**
- `Mc3Document` public API: don't change without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` must stay buildable standalone
  (no CNA/ImGui deps).
- `mc3.xsd` must stay symmetric with `Mc3XmlWriter.cpp` — any new writer
  attribute/element needs a schema declaration in the same change.
- Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`
  (triggers a full CNA recompile).
- Do not modify CNA/SHARP_RUNTIME source files from this repo.
- `file(GLOB_RECURSE)` collects sources — a new `.cpp` file (or a new
  `add_test()`, or an edit to `mc3.xsd`) needs a cmake **reconfigure**,
  not just a rebuild.
- MCB format version is `MCB_VERSION` in `McbFormat.hpp` — bump on any
  breaking wire-format change.
- XSD root element order is strict (`include → metadata → meta →
  environment → textures → materials → embeds → ... → definitions →
  objects → actions`).
- Reconfigure `cmake-build-debug/` only with CLion's bundled cmake, not
  the system cmake (a documented system-cmake bug for this project).
- `find_package(SQLite3)`/`OpenSSL`/`LibXml2` are all intentionally
  optional on desktop builds — don't make any of them `REQUIRED` again
  (a past `REQUIRED` on SQLite3 was a real bug, fixed this session).

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid a system-cmake bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5   # (re)configure — flag needed since CNA vendors ENet with an old cmake_minimum_required
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (46)
ctest -N                                                      # lists all 46 tests

# --- Release (system cmake is fine for a fresh dir)
cmake -S . -B b-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
      -DFETCHCONTENT_UPDATES_DISCONNECTED=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build b-release -j4
(cd b-release && ctest --output-on-failure)

# --- Standalone (CNA-free) component builds + tests
for c in mc3 mcb mc3togltf mc3tomcb; do
  cmake -S "$c" -B "$c-build" -G Ninja -DBUILD_TESTING=ON \
        -DFETCHCONTENT_UPDATES_DISCONNECTED=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  cmake --build "$c-build" -j4
  (cd "$c-build" && ctest --output-on-failure)
done

# --- MinGW (Windows) cross-compile — builds to ~73%, see §4 for the blocker
# Needs a toolchain file (CMAKE_SYSTEM_NAME Windows, x86_64-w64-mingw32-gcc/g++);
# mingw-w64 packages are installed on this machine.
cmake -S . -B b-mingw -G Ninja -DCMAKE_TOOLCHAIN_FILE=<path-to-toolchain>.cmake \
      -DBUILD_TESTING=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build b-mingw -j$(nproc)   # fails at ~328/449 objects, see §4

# --- Emscripten (web) build — builds 100%, see §4 for the rendering issue
source /home/robertvokac/Downloads/emsdk/emsdk_env.sh
emcmake cmake -S . -B b-web -G Ninja -DBUILD_TESTING=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build b-web -j$(nproc)
cd b-web && python3 -m http.server 8765 --bind 127.0.0.1   # then open http://127.0.0.1:8765/MeshCraft.html
node mc3togltf/mc3togltf.js --help   # confirms the compiled JS/WASM actually executes

# --- Run / export / validate / version
./cmake-build-debug/MeshCraft test/house.mc3.xml   # open a sample scene
./cmake-build-debug/MeshCraft --version             # MeshCraft 0.1.0
./cmake-build-debug/MeshCraft test/fog_linear.mc3.xml --screenshot /tmp/out.ppm
./cmake-build-debug/MeshCraft test/house.mc3.xml --export /tmp/out.glb --stats
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
./cmake-build-debug/mc3togltf/mc3togltf --help
python3 test/validate_xsd.py mc3/mc3.xsd test/features.mc3.xml
ctest -R mc3_commands --output-on-failure   # editor algorithms + undo/redo
ctest -R mc3_registry --output-on-failure   # ModelRegistry
ctest -R mc3_ai       --output-on-failure   # AiAssistant + mock HTTP server

# --- Push (normal pushes have worked throughout this session; only CI activation is blocked, see §4)
git push origin develop
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

1. **Diagnose the Emscripten blank-canvas rendering issue (§4).**
   Goal: find out why nothing draws to the canvas after the first
   successful frame, even though the main loop is confirmed to keep
   running. Files: `src/MeshCraft/MeshCraftApplication.cpp` (`Draw()`/
   `EndDraw()`), possibly ImGui's font-atlas upload path. Add a small,
   temporary frame counter (e.g. `std::cerr` every 60 frames) and a
   `glGetError()` check after the main draw calls; rebuild with
   Emscripten (§7); ask a human to reload the page and report the new
   console output. Verification: the diagnostic output itself, reported
   back by a human with a browser — this repo alone can't close the
   loop.

2. **STAB-0559 — verify clipboard paste: Ctrl+V from OS clipboard.**
   Files: `src/MeshCraft/MeshCraftApplication_Keyboard.cpp`. Needs a
   live UI session to actually exercise clipboard interaction — check
   whether this is reachable at all headlessly (e.g. does ImGui's own
   clipboard callback plumbing have a headless-testable seam) before
   assuming it's fully blocked.

3. **STAB-0560 — verify SDL drag-drop on Linux.** Files:
   `src/MeshCraft/MeshCraftApplication.cpp`. Likely needs a live
   desktop session (dragging a file from a real file manager) — same
   caveat as clipboard above, check for a testable seam first.

4. **STAB-0569 — verify preloaded web assets: `test/` files accessible.**
   Files: `CMakeLists.txt`. The `--preload-file test@/test` flag is
   already visible at `CMakeLists.txt:319` (confirmed while reading
   the Emscripten link-options block for STAB-0562/0565) — this row is
   likely a quick confirmation that a sample file under `/test` in the
   Emscripten FS is actually readable, e.g. via a small Node smoke
   check against the already-built `MeshCraft.js`/`.wasm` (same
   approach as STAB-0553's `mc3togltf.js --help` check).

Recently closed this session: **STAB-0554** (path separators — confirmed
`std::filesystem::path` used consistently everywhere, no gap),
**STAB-0555/0556/0557** (UTF-8 filenames, paths with spaces, non-ASCII
object names — added 3 new round-trip tests, all pass, see §3),
**STAB-0558** (config dir per OS — found and fixed a real gap:
`meshcraftConfigDir()` in `MeshCraftPrivate.hpp` was unconditionally
Linux-style with no Windows branch at all; added `%APPDATA%` support
under `#if defined(_WIN32)`), **STAB-0561** (file dialogs — confirmed
no native OS file-picker library exists anywhere in the editor; all 8
path entry points use plain `ImGui::InputText`, which has no
OS-conditional code path to diverge), **STAB-0562** (WebGL2 shader
compatibility — confirmed all 7 of CNA's EasyGL 3D shader programs are
`#version 300 es`/GLSL ES 3.00, and the prior STAB-0553 live-browser
console showed no shader-compile-failure diagnostics), **STAB-0563/0564**
(SQLite/AI stubs on Emscripten — both feature-gate macros are correctly
undefined for that target and both stub branches compiled clean as
part of the same 449/449 build), **STAB-0565** (IDBFS persistence —
found and fixed a real gap: `cmake/web/pre.js` mounted IDBFS at
`/home/user`, but Emscripten's actual default `$HOME` is
`/home/web_user`, so everything `meshcraftConfigDir()` writes was
silently landing outside the persistent mount; fixed and rebuilt clean),
**STAB-0566** (Windows SDL DLL copy — actually reconfigured against a
real MinGW toolchain and found a genuine bug via `--trace-expand`:
`cna_copy_sdl_runtime()`'s `SDL3::SDL3`/etc. targets are invisible from
MeshCraft's parent directory scope, so zero DLL-copy commands were ever
generated; fixed on the MeshCraft side only, via a `CNA_SDL_PREBUILT_ROOT`-based
fallback in `CMakeLists.txt`, verified against the regenerated
`build.ninja`), **STAB-0567** (MinGW static libgcc/libstdc++ —
confirmed directly in the same generated `build.ninja`'s `LINK_FLAGS`),
and **STAB-0568** (Android `main()`/shared-lib adapter — confirmed by
code reading; no Android NDK is installed here so it can't be
reconfigured against a real toolchain like STAB-0566/0567 were).

**Lesson from STAB-0566**: don't close a build-system verification row on
code-reading alone when a cheap real reconfigure is possible (the cached
`.sdl-prebuilt-Windows-x86_64`/`-emscripten` dirs make MinGW/Emscripten
reconfigures fast even though a full build is slow/blocked) — the actual
bug here was invisible from source alone and only showed up via
`--trace-expand` + inspecting the generated `build.ninja`.

Continue through the rest of S16 (`plan.md`, STAB-0559 onward) in
`plan.md` order after these — several remaining rows are platform-build
or live-UI items (clipboard, native file dialogs, Android) that may turn
out to be blocked on missing tools/platforms here, same as MinGW/
Emscripten turned out to be partially blocked. Read each one's actual
code path before assuming either way — this session found roughly as
many "confirmed already correct, just needed a test" outcomes as real
bugs, so don't assume a row is trivial or blocked without checking.

---

## 9. Do not do yet

- **No new scene-format features** — N1–N7 are complete; further schema
  additions need design discussion first.
- **No CNA/SHARP_RUNTIME source changes** — separate repos, out of
  scope for this one. The MinGW GLES3-header blocker and the Emscripten
  rendering issue may ultimately need CNA-side changes, but that's a
  different Claude Code instance's / the CNA maintainer's call, not
  something to patch around from here.
- **No `Mc3Document` public API changes** without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- **No `${meta-gl_SOURCE_DIR}/include`** in any `CMakeLists.txt`.
- **No reconfiguring `cmake-build-debug/` with system cmake** — use
  CLion's bundled cmake.
- **No moving `.github_` back to `.github`** until CI credentials are
  sorted out.
- **No SVG rasterization work** until a library choice is made (librsvg
  vs. NanoSVG) — a real feature decision, not a quick fix.
- **No implementing `embed:` mesh-source resolution in `GltfExporter`**
  without a explicit decision to take it on — it's a real new feature
  (parsing external GLB/base64 data), not a bug fix, and has been
  deliberately left as a documented limitation twice now.
- **No mass refactoring** of passing code, no speculative architecture
  changes — stabilization phase; scope each change to exactly what its
  `STAB-XXXX` entry (or the current diagnostic task) asks for.
- **No adding a debug-only CLI flag purely to force a UI toggle on for
  testing** (e.g. bloom/SSAO/wireframe) — considered and rejected
  multiple times as scope creep; flag known-correct-but-unverifiable
  behavior instead.
- **No attempting to "fix" the MinGW build by vendoring GLES headers
  ourselves** — that's CNA's build configuration decision to make, not
  something to patch around from the MeshCraft side.
- **No attempting STAB-0642/0650** without the missing tool/access first
  (Blender, an active CI run respectively). STAB-0643 (browser) is now
  partially reachable — a human can load the Emscripten build locally,
  as already done for the diagnostic session in §4.

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect only the files needed for the first
task in section 8. Do not refactor unrelated code. Make one small,
verified improvement. Run the relevant build/test command from section 7
and confirm cmake-build-debug still passes (46/46, or the new total if
you registered a new ctest). Update NEXT.md after finishing.

Current branch: develop, in sync with origin/develop at commit 18b1809.
Build dir: cmake-build-debug/ (Debug, CLion cmake 4.2.2) — last full
rebuild + 46/46 ctest was clean at this commit. Release (b-release/) and
the standalone mc3/mcb/mc3togltf/mc3tomcb builds were verified earlier
in this project's history but not re-checked recently — re-verify
before relying on them.

Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file / new add_test()
requires a reconfigure, not just a rebuild — see section 6.

The most useful thing to pick up first is diagnosing the Emscripten
blank-canvas rendering issue (section 4/8, item 1) — it needs a human
with a browser to close the loop, so if none is available, move on to
the plain S16 verification tasks (section 8, items 2+) instead.

CI is parked deactivated under .github_/ (credentials issue, see
section 4).
```
