# NEXT.md

_Last updated: 2026-07-06, commit `ff4455a` (develop, in sync with `origin/develop`)_

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
adding new features. All work is tracked in `plan.md` as ~651 `STAB-XXXX`
tasks across sections S0–S20, gated by a Gate 0–6 checklist
(`STABILIZATION.md`).

**Current phase:** Stabilization, deep in the S0–S15 backlog. **453/651**
`plan.md` rows are ✅. Sections **S0, S1, S2, S13, S17, S19** are fully
closed. S16/S18/S20 are closed except for rows genuinely blocked on a
missing tool or a live human session. **S3 (MCB Binary Format Stability)
is in progress** — currently the active section (see §4/§8).

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
- MCB (`mcb/`) is a custom tagged binary format (magic `MCB\0`, version
  byte, then key/tag/value pairs — see `mcb/include/MeshCraft/Mcb/McbFormat.hpp`)
  mirroring `Mc3Document`'s full field set, used as a faster-load
  alternative to XML. It is **not** an authoring format.

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`, generated with CLion's bundled cmake
  4.2.2): last full rebuild + full `ctest` run was clean at commit
  `ff4455a` — **48/48 tests passed**, 0 warnings from MeshCraft's own
  sources. No source changes since that commit (working tree clean).
- **Release** (`b-release/`): last verified earlier in this stabilization
  effort; not re-verified against the current commit — re-run the
  commands in §7 before relying on it.
- **Standalone (CNA-free) component builds** (`mc3`, `mcb`, `mc3togltf`,
  `mc3tomcb`): each configures/builds/tests independently of the root
  project. `mcb`'s standalone build was re-verified at commit `22b1c5d`
  (1/1 passing, includes the MCB include/metadata fix). The other three
  were last confirmed passing earlier in this effort — re-verify with
  §7's commands if you depend on them.
- **MinGW (Windows) cross-compile**: configure succeeds; build reaches
  ~73% (328/449 objects) before failing on a CNA-side issue (missing
  GLES3 headers for a Windows target). See §4. Windows-specific fixes
  already landed on the MeshCraft side this effort: SDL runtime DLL
  copying and static libgcc/libstdc++ linking are both confirmed correct
  via the generated `build.ninja` (STAB-0566/0567), and config-dir
  resolution now has a real `%APPDATA%` branch (STAB-0558) — none of
  this can be exercised past the 73% mark until CNA's GLES3 gap is
  resolved.
- **Emscripten (web) build**: configure and build both succeed 100%
  (449/449, exit 0) and the compiled JS/WASM genuinely executes (verified
  via Node). Loaded in a real browser: initializes correctly, but renders
  a blank canvas — the 3D editor is not yet usable on web. See §4.

### Tests
**48/48 CTest pass** in Debug as of commit `ff4455a`. `ctest -N` lists
all 48 by name; `ctest --print-labels` groups them into `format`/
`export`/`render`/`registry`/`ai`/`commands` (STAB-0027). Notable
binaries and their real measured assertion counts (see `TESTING.md`):
- `mc3_commands` (489 `PASS:` assertions): editor command algorithms
  (rename incl. edge cases), undo/redo, auto-save/backup,
  keybinding/macro persistence, hierarchy filtering, AI-panel lifecycle,
  viewport picking, material-color resolution.
- `mc3_registry` (109 assertions): ModelRegistry SQLite CRUD/search,
  including a real legacy-schema `ALTER TABLE ADD COLUMN` migration test
  and a corrupted-database-file test.
- `mc3_ai` (57 assertions): AiAssistant JSON + AI-response validation
  pipeline, mock-HTTP-server round-trips.
- `mc3_roundtrip` (413+ assertions): full XML parser/writer roundtrip —
  every primitive/structural object type (including Union/Intersection/
  Instance/Area, previously untested), all N1–N7 extensions, `<include>`
  edge cases (spaces in path, nonexistent file, nested, cross-directory),
  malformed-input robustness, a golden-file byte-for-byte writer-output
  test, and a genuine two-cycle save/reload fixpoint test.
- `mcb_roundtrip` (2 CTest entries: `mcb_roundtrip` C++ binary + `mc3tomcb_roundtrip`
  CLI test): MCB binary roundtrip, now including `doc.includes`/skip-sets/
  legacy `metadata`/CSG/extrude/deform (all added this effort — see §3).
- `mc3togltf_*` (~24 tests): CSG, materials, textures (real GLB image
  embedding), animation, instance variants, large scenes, a full-GLB
  determinism test, a golden-JSON structural test, help text,
  no-partial-output-on-error.
- Several real-render smoke tests (`smoke_test*`, `fog_linear_test`,
  gizmo tests, `csg_cache_test`, background/skybox texture, `lod_test`,
  `editor_export_test`) that run the actual `MeshCraft` binary in
  `--screenshot` headless mode and verify genuine pixel output.

### Tools / libraries available
- `MeshCraft` — editor executable, version `0.1.0` (`--version` to print
  it). Builds and runs on Linux; the 3D viewport is not fully integrated
  into the render loop in interactive mode, and the Emscripten build
  doesn't visually render yet (see "What does NOT work yet").
  - `--screenshot <path>` — render scene headlessly to a PPM file.
  - `--export <path>` — export the loaded scene to `.glb`/`.gltf`
    non-interactively and exit.
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb` (`--help`/`-h`,
  `--allow-approximate-csg`, `--stats`).
- `mc3tomcb` — bidirectional CLI: `mc3.xml` ↔ `.mcb`.
- `Mc3` / `Mcb` / `mc3togltf_lib` — static libs (scene data, binary
  serialization, glTF export).

### What works
- Full XML round-trip for all object types (primitives, Group, Union/
  Difference/Intersection, Instance, Area, Mesh, Extrude) and all N1–N7
  extensions, including `<include>` across directories, with spaces in
  the path, and with a clear error on a missing include file.
- MCB binary round-trip, now matching the XML feature set including
  the include structure (fixed this effort — see §3) and legacy
  `metadata` map.
- XSD validation for every test fixture and for AI-generated scene XML.
- glTF/GLB export: primitives, animations, CSG, materials, lights,
  cameras, instances, groups, OBJ mesh import, `<include>`d definitions;
  a genuine byte-for-byte determinism test and a golden-JSON structural
  test both pass.
- SQLite-backed asset registry: open/save/search/remove/migration
  (including from a genuinely pre-migration legacy schema), gracefully
  disabled if SQLite3 isn't available at configure time.
- Editor undo/redo, auto-save + rotating backup, AI Assistant, viewport
  ray-cast click-to-select, material/subtree export-import — all as
  documented in prior sessions, unchanged this effort.

### What does NOT work yet
- **The Emscripten web build doesn't visually render the 3D editor** —
  see §4, the main open problem, unchanged since it was first found.
- `EditorViewport` is not integrated into the `MeshCraftApplication`
  render loop (interactive mode).
- SVG texture rasterization: parsed/serialized/round-tripped, but
  `GltfExporter` never reads the SVG texture map — silently dropped from
  export (a warning is printed).
- Embedded glTF (`<mesh src="embed:id"/>`): parsed/serialized, but
  `GltfExporter` treats `embed:id` as a literal OBJ path, which fails —
  export continues with an empty (meshless) node. Deliberate, documented
  limitation (real fix = parsing external GLB/base64 data), not a bug.
- `<embeds>` inside an `<include>`d file is never merged (only the main
  document's own top-level `<embeds>` is parsed) — a narrow, accepted
  limitation, documented in `MC3_FORMAT.md` (STAB-0092).
- N3–N7 scene data (scripts, sounds, music, triggers, scene states,
  meta): fully round-tripped but not executed at runtime anywhere.
- CI workflow exists but is parked deactivated under `.github_/`.
- MinGW (Windows) cross-compile builds to ~73% then fails on a CNA-side
  GLES3 header issue — out of scope for this repo to fix.
- A cluster of visual toggles (bounding-box overlay, SSAO, bloom,
  wireframe mode, gizmos, shadow-map debug, etc.) are confirmed correct
  by code reading but can't be re-verified fresh in this headless
  environment — see §5.

---

## 3. Recent changes

Most recent work closed out **S0, S1, S2 entirely** and made significant
progress into **S3 (MCB Binary Format Stability)**. Highlights, newest
first (all on `develop`, all pushed):

- **`mcb/test/mcb_roundtrip_test.cpp`**: added CSG/extrude/deform
  roundtrip tests (STAB-0141/0142/0143) — no MCB test exercised any of
  these before, despite the writer/reader already supporting them; all
  pass cleanly, confirming genuine end-to-end support.
- **`mcb/src/McbWriter.cpp` / `McbReader.cpp`**: **real bug fixed** —
  `doc.includes` and its skip-sets (`includedDefs`/`includedMaterials`/
  `includedTextures`) plus the legacy `doc.metadata` map were **never
  written or read by MCB at all**. Converting an XML scene using
  `<include>` to MCB and back to XML would silently inline everything
  the included library contributed into the main scene, losing the
  include structure completely. Fixed by adding write/read support for
  all 5 fields (STAB-0131/0144).
- **`mc3/src/Mc3XmlParser.cpp`**: **real bug fixed** — `attrF()`/`attrI()`
  used raw `std::stof`/`std::stoi` with no `try/catch`; a single
  malformed float attribute anywhere in a scene file (e.g. `radius="abc"`)
  failed the **entire file load** with an unhelpful `"Failed to load
  file: stof"` message. Fixed 3 call sites (STAB-0080).
- **`mc3/src/Mc3XmlParser.cpp` / `Mc3XmlWriter.cpp`**: **real bug fixed**
  — `areaType`'s `size` attribute (declared in `mc3.xsd`) was never
  implemented on either side (STAB-0031). Fixing it surfaced a **second**
  bug in `mc3togltf/src/GltfExporter.cpp`: `buildMesh()` built a real
  default-Box-shaped mesh for any object with `primitive.has_value()`,
  with no `ObjectType::Area` exclusion — harmless before since Area's
  primitive was always null, but triggered immediately by the parser fix.
  Both fixed.
- **`mc3/src/Mc3XmlWriter.cpp`**: **real bug fixed** — the SVG-texture
  writer loop was missing the `includedTextures` skip check the adjacent
  regular-texture loop already had; an SVG texture merged from an
  `<include>` file was silently re-inlined into the main file on every
  save (STAB-0091).
- **`src/MeshCraft/ModelRegistry.cpp`**: **real bug fixed** —
  `ModelRegistry::open()` left `db_` non-null when `createSchema()`
  threw (e.g. opening a corrupted database file), so `isOpen()`
  incorrectly reported `true` after a failed open (STAB-0065).
- **`CMakeLists.txt`**: added an actionable diagnostic when `lxml` is
  missing instead of letting `xsd_validation` fail with a raw Python
  traceback (STAB-0015); added CTest `LABELS` to all registered tests
  for `-L format`/`-L export`/etc. grouping (STAB-0027).
- Extensive new test coverage added across `mc3/test/roundtrip_test.cpp`
  and `mcb/test/mcb_roundtrip_test.cpp` for previously-untested object
  types/fields (Union/Intersection/Instance/Area, `Mc3Light`/`Mc3Camera`/
  `Mc3Environment` — none referenced anywhere before this effort — a
  50-object scene, multi-channel-kind actions, and a genuine two-cycle
  save/reload fixpoint test).
- `plan.md`'s summary table was recomputed (it had drifted badly from
  actual row state across several prior sessions) and `TESTING.md`'s
  stale test counts ("21 tests") were corrected to the real numbers.

Full history is in `git log --oneline`; `plan.md` has a per-row writeup
for every `STAB-XXXX` ID mentioned above.

---

## 4. Current blocker / main problem

**No blocker to local development or testing on Linux** — Debug builds
and passes 48/48 tests as of `ff4455a`.

The most important **open problem**, unchanged across several sessions,
is the Emscripten web build's blank canvas:

- **Symptom**: `MeshCraft.html`, served locally and opened in a real
  browser, loads and initializes correctly — `SDL_CreateWindow`
  succeeds, `EasyGLGraphicsBackend` initializes over WebGL 2.0, no
  crash, no console error — but the `<canvas>` stays a blank/black
  rectangle.
- **Failing command**: none technically fails; the failure is purely
  visual. To reproduce: build with Emscripten (§7), serve the output
  directory over HTTP, open `MeshCraft.html`, check devtools console.
- **Affected files/modules**: likely `MeshCraftApplication::Draw()`/
  `EndDraw()` (`src/MeshCraft/MeshCraftApplication.cpp`), ImGui's
  font-atlas upload path, or a GL call that's silently a no-op under
  WebGL2/GLES3.
- **What has already been tried**: confirmed CNA's `Game::RunLoop()`
  correctly branches on `__EMSCRIPTEN__` and keeps the render loop
  running frame after frame (ruled out "fell out of main()"). Also
  confirmed all 7 of CNA's EasyGL 3D shader programs are valid
  `#version 300 es` and compile/link without error (no shader-compile
  failure in the console). The actual per-frame cause was not
  root-caused — needs temporary frame-counter/`glGetError()` diagnostic
  logging added to `Draw()`/`EndDraw()`, a rebuild, and a human
  browser-retest. Deferred at the user's request as a lower priority
  than the plain backlog work.

A second, lower-priority open item: **MinGW (Windows) cross-compile
builds to ~73%** then fails with `imgui_impl_opengl3.cpp: fatal error:
GLES3/gl3.h: No such file or directory` — a CNA-side backend
configuration decision, out of scope to fix from this repo.

A third, purely **operational** issue: CI is parked deactivated under
`.github_/workflows/ci.yml` because the git remote's credentials lack
the `workflow` scope. `git push` has occasionally been denied then
started working again unprompted — if push fails, don't touch git/
remote config, just retry.

---

## 5. Known bugs and limitations

- **Emscripten web build renders a blank canvas** — see §4. _confirmed,
  not fixed, needs frame-level diagnostic logging + a browser retest._
- **MinGW cross-compile fails at ~73% on a CNA-side GLES3 header
  issue** — see §4. _confirmed, blocked on CNA, not fixable from here._
- **CI cannot be activated with the current git credentials** —
  _confirmed; needs owner action on the git remote/credentials._
- **SVG texture rasterization** not implemented in `GltfExporter`.
  _incomplete, documented, blocked on a library choice (librsvg vs.
  NanoSVG)._
- **Embedded glTF** (`embed:id`) fails to parse as an OBJ path during
  glTF export; doesn't crash, exports a meshless node. _incomplete,
  deliberately documented, real fix is a new feature._
- **`<embeds>` inside an `<include>`d file is never merged** — a narrow,
  accepted limitation (STAB-0092), documented in `MC3_FORMAT.md`.
  _incomplete, not implemented, no reported real-world use case yet._
- **`mc3.xsd` has no numeric range constraints** — a schema-valid AI
  response can contain a negative `size`/`radius` and it applies
  unchanged. _confirmed, documented in `README.md`, not fixed._
- **`mc3.xsd`'s `mip_maps` texture attribute has zero implementation.**
  _confirmed, not fixed, no assigned STAB-XXXX ID._
- **A cluster of visual toggles need a live display to re-verify**
  (bounding-box overlay, SSAO, bloom, wireframe, gizmos, shadow-map
  debug, etc.) — confirmed correct by code reading, none reachable via
  headless `--screenshot`. _needs verification by a human with a live
  display._
- **`--screenshot` always writes raw PPM** regardless of the output
  path's extension. _confirmed, deliberate, documented, relied on by
  dozens of existing test scripts._
- **N3–N7 are data-only** — round-tripped but nothing executes them at
  runtime. _intended at this stage, not a bug._
- **`mc3` standalone build skips `mc3_commands`** — needs
  `EditorAlgorithms.hpp` from the editor tree, absent standalone.
  _intended, not a bug._
- **3 `plan.md` items cannot be completed in this environment**:
  STAB-0642 (needs Blender), STAB-0643 (needs a browser — partially
  reachable), STAB-0650 (needs CI actually running). _flagged, needs
  external tooling/action._
- **S3 (MCB) is only partially audited** — several rows (truncated/
  corrupted-input error handling, UTF-8 strings, large strings, file
  size vs. XML, determinism, magic-byte identifiability, endianness)
  haven't been checked yet this effort. _needs verification — see §8._

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

**MCB format** (`mcb/include/MeshCraft/Mcb/McbFormat.hpp`): magic `MCB\0`
+ version byte + flags byte, then a flat sequence of
`key(len-prefixed) + tag-byte + value` pairs terminated by a zero-length
key, recursing for `TAG_OBJ`/`TAG_ARR`/`TAG_MAP`. `skipValue()` in
`McbReader.cpp` recursively skips any tag type it doesn't recognize by
key name, which is what makes forward-compatible unknown-key handling
work — **this was verified only by code reading so far this effort, not
yet by an empirical test with a hand-constructed unknown-key stream**
(STAB-0132/0145, next up — see §8).

**`doc.sourcePath` is a single directory per document.** Everything
(texture URIs, OBJ mesh sources, etc.) resolves relative to it. When a
document pulls in an `<include>`d file from a *different* directory,
paths from that file are rebased via `Mc3XmlParser.cpp`'s
`rebaseRelativePath()`/`rebaseDefinitionMeshSources()`.

**Undo/redo:** snapshot-based. `undoStack_`/`redoStack_` (capped at 20)
hold `std::vector<Mc3::Mc3Document>`.

**The "Alg mirror" pattern**: editor/AI logic in CNA-coupled `.cpp` files
often has zero actual CNA/ImGui dependency once app-state bookkeeping and
rendering are set aside. Pure logic lives in a CNA-free header
(`include/MeshCraft/EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`),
functions suffixed `Alg`, and the real `.cpp` `#include`s and calls them
directly. The headless test suite (`mc3/test/editor_commands_test.cpp`)
calls the same functions directly with no CNA/SDL3/ImGui dependency.
**Caution**: prior sessions found real `.cpp` files with their own inline
duplicate of an `Alg` function instead of calling it, silently drifted
out of sync — when touching one side of an Alg-mirrored pair, always
check the other side is actually wired up and matches.

**GL rendering caution**: a VAO-based vertex-attribute draw path was
found completely non-functional in this environment's GL setup in a
prior session (the skybox — fixed by switching to a `gl_VertexID`-based
procedural approach). If a new raw-GL draw call renders nothing with no
error, consider this failure mode.

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
  attribute/element needs a schema declaration in the same change (the
  STAB-0031 Area/`size` bug was exactly this symmetry breaking down).
- Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`
  (triggers a full CNA recompile).
- Do not modify CNA/SHARP_RUNTIME source files from this repo.
- `file(GLOB_RECURSE)` collects sources — a new `.cpp` file (or a new
  `add_test()`, or an edit to `mc3.xsd`) needs a cmake **reconfigure**,
  not just a rebuild.
- MCB format version is `MCB_VERSION` in `McbFormat.hpp` — bump on any
  breaking wire-format change. Adding a new optional key (as done this
  effort for `includes`/`metadata`) is additive and doesn't need a bump,
  since `skipValue()` makes unknown keys forward-compatible either way.
- XSD root element order is strict (`include → metadata → meta →
  environment → textures → materials → embeds → ... → definitions →
  objects → actions`).
- Reconfigure `cmake-build-debug/` only with CLion's bundled cmake, not
  the system cmake (a documented system-cmake bug for this project).
- `find_package(SQLite3)`/`OpenSSL`/`LibXml2` are all intentionally
  optional on desktop builds — don't make any of them `REQUIRED` again.

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid a system-cmake bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5   # (re)configure — flag needed since CNA vendors ENet with an old cmake_minimum_required
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (48)
ctest -N                                                      # lists all 48 tests
ctest --print-labels                                          # format/export/render/registry/ai/commands
ctest -L export --output-on-failure                           # run just one label group
ctest --rerun-failed --output-on-failure                      # re-run only what failed last time

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
  rm -rf "$c-build"   # these are scratch dirs, not committed
done

# --- MinGW (Windows) cross-compile — builds to ~73%, see §4 for the blocker
cat > /tmp/mingw-toolchain.cmake <<'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF
cmake -S . -B b-mingw -G Ninja -DCMAKE_TOOLCHAIN_FILE=/tmp/mingw-toolchain.cmake \
      -DBUILD_TESTING=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build b-mingw -j$(nproc)   # fails at ~328/449 objects, see §4

# --- Emscripten (web) build — builds 100%, see §4 for the rendering issue
source /home/robertvokac/Downloads/emsdk/emsdk_env.sh
emcmake cmake -S . -B b-web -G Ninja -DBUILD_TESTING=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build b-web -j$(nproc)
cd b-web && python3 -m http.server 8765 --bind 127.0.0.1   # then open http://127.0.0.1:8765/MeshCraft.html

# --- Run / export / validate / version
./cmake-build-debug/MeshCraft test/house.mc3.xml
./cmake-build-debug/MeshCraft --version
./cmake-build-debug/MeshCraft test/fog_linear.mc3.xml --screenshot /tmp/out.ppm
./cmake-build-debug/MeshCraft test/house.mc3.xml --export /tmp/out.glb
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
python3 test/validate_xsd.py mc3/mc3.xsd test/features.mc3.xml
ctest -R mc3_roundtrip --output-on-failure   # mc3 XML parser/writer
ctest -R mcb_roundtrip --output-on-failure   # MCB binary format
ctest -R mc3_commands  --output-on-failure   # editor algorithms + undo/redo

# --- Push (normal pushes have worked throughout; only CI activation is blocked, see §4)
git push origin develop
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

Continue **S3 (MCB Binary Format Stability)**, `plan.md` priority order.
Remaining open rows (all in `mcb/`):

1. **STAB-0132/0145 — verify MCB unknown-key skipping empirically.**
   Goal: prove (not just read code) that `McbReader`'s `skipValue()`
   correctly and safely skips a key it doesn't recognize, including a
   forward-compat "future version added a new section" scenario.
   Files: `mcb/test/mcb_roundtrip_test.cpp` (new test), possibly
   `mcb/src/McbWriter.cpp`/`McbReader.cpp` if a gap is found. Approach:
   hand-construct a valid MCB byte stream (magic `MCB_MAGIC` + `MCB_VERSION`
   from `McbFormat.hpp`, root `TAG_OBJ`, a couple of known fields, one
   deliberately unknown key with an arbitrary tag/value, then the rest of
   a normal document) and confirm `loadFromBinary()` doesn't throw and
   correctly reads everything after the unknown key. Verification:
   `ctest -R mcb_roundtrip --output-on-failure`.

2. **STAB-0133/0134/0135 — MCB malformed-input error handling.** Goal:
   confirm (or fix) that a truncated file, an all-zeros header, and a
   single-byte input all fail cleanly (throw or return an error) rather
   than crash/UB. Files: `mcb/src/McbReader.cpp`, new tests in
   `mcb/test/mcb_roundtrip_test.cpp`. This is a good candidate to check
   empirically first (feed the bad bytes, see what actually happens)
   before assuming either outcome. Verification: same as above.

3. **STAB-0148 — verify UTF-8 strings survive MCB roundtrip.** Files:
   `mcb/test/mcb_roundtrip_test.cpp`. Quick, should mirror the existing
   `mc3_roundtrip` UTF-8-filename test's spirit but for an object id/name
   field through MCB specifically (`wRawStr`/`rRawStr` are just
   length-prefixed byte copies, so this is very likely already correct
   — confirm with a real test rather than assuming).

4. **STAB-0140/0149/0150 — determinism, magic bytes, endianness.**
   Files: `mcb/src/McbWriter.cpp`. STAB-0149 (magic bytes) and STAB-0150
   (endianness, already commented as "little-endian"/"IEEE 754" in
   `McbFormat.hpp`) are likely quick documentation-only confirmations.
   STAB-0140 (writer determinism) needs an actual byte-compare test —
   check whether `Mc3Document`'s map iteration order (`std::map` is
   naturally sorted, so this is likely fine) could ever make two writes
   of the same document differ.

5. **STAB-0136/0146 — document the MCB format.** `MC3_FORMAT.md` or a
   new `MCB_FORMAT.md`: header layout, `TAG_*` constants (already
   defined with inline comments in `McbFormat.hpp` — STAB-0146 may
   already be satisfied, check before writing more), key ordering.

Remaining lower-priority S3 rows (STAB-0137/0138/0139) are P1/P2 checks
for animation-keyframe completeness, large-string handling, and file-size
comparison — worth a quick look after the above, likely mostly
confirmations given how thoroughly writer/reader already cover other
field types.

**After S3 closes**, continue to **S4 (glTF/GLB Exporter Correctness)**
in `plan.md` order — it's the next-largest section with real open rows
(13 `📋` + 16 `🧪` remaining).

---

## 9. Do not do yet

- **No new scene-format features** — N1–N7 are complete; further schema
  additions need design discussion first.
- **No CNA/SHARP_RUNTIME source changes** — separate repos, out of
  scope for this one.
- **No `Mc3Document` public API changes** without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- **No `${meta-gl_SOURCE_DIR}/include`** in any `CMakeLists.txt`.
- **No reconfiguring `cmake-build-debug/` with system cmake** — use
  CLion's bundled cmake.
- **No moving `.github_` back to `.github`** until CI credentials are
  sorted out.
- **No SVG rasterization work** until a library choice is made — a real
  feature decision, not a quick fix.
- **No implementing `embed:` mesh-source resolution in `GltfExporter`**
  or **`<embeds>`-inside-`<include>` merging** without an explicit
  decision to take either on — both are deliberately documented
  limitations, not bugs, and both have been deferred multiple times now.
- **No mass refactoring** of passing code, no speculative architecture
  changes — stabilization phase; scope each change to exactly what its
  `STAB-XXXX` entry asks for.
- **No attempting to "fix" the MinGW build by vendoring GLES headers
  ourselves** — that's CNA's call, not something to patch around from
  the MeshCraft side.
- **No attempting STAB-0642/0643/0650** without the missing tool/access
  first (Blender, a browser, an active CI run respectively).
- **Don't assume a `plan.md` row needs new work without checking first**
  — this effort found roughly a third of the "open" rows in S0–S2 were
  already fully satisfied by existing tests/infrastructure, just never
  marked done (stale rows from the original 650-task plan generation).
  A quick `grep`/`ctest -R` check before writing new code has
  consistently paid off.

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect only the files needed for the first
task in section 8 (STAB-0132/0145 — MCB unknown-key skipping). Do not
refactor unrelated code. Before writing a new test, grep for whether it
already exists (this codebase has had many stale plan.md rows). Make one
small, verified improvement. Run the relevant build/test command from
section 7 and confirm cmake-build-debug still passes (48/48, or the new
total if you registered a new ctest). Update plan.md's row for the task
you completed, and update NEXT.md's section 3/8 after finishing.

Current branch: develop, in sync with origin/develop at commit ff4455a.
Build dir: cmake-build-debug/ (Debug, CLion cmake 4.2.2) — last full
rebuild + 48/48 ctest was clean at this commit, working tree clean.
Release (b-release/) and the standalone mc3/mcb/mc3togltf/mc3tomcb
builds were verified earlier in this project's history (mcb/ re-verified
at 22b1c5d) but should be re-checked if you depend on them.

Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file / new add_test()
requires a reconfigure, not just a rebuild — see section 6.

The Emscripten blank-canvas issue (section 4) needs a human with a
browser to close the loop — if none is available, stay on the plain S3
backlog work in section 8 instead.

CI is parked deactivated under .github_/ (credentials issue, see
section 4). Commit after each STAB-XXXX task and push to origin/develop
— this has been the standing workflow across this whole stabilization
effort.
```
