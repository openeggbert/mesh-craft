# NEXT.md

_Last updated: 2026-07-06, commit `d5b5153` (develop, in sync with `origin/develop`)_

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

**Current phase:** Stabilization, deep in the S0–S15 backlog. **496/651**
`plan.md` rows are ✅. Sections **S0, S1, S2, S3, S4, S13, S17, S19** are
fully closed. S16/S18/S20 are closed except for rows genuinely blocked on a
missing tool or a live human session. **S5 (CSG Stability) is next** — the
largest remaining open section (23 `📋` + 7 `🧪` of 35 rows) — see §4/§8.

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
  byte, then key/tag/value pairs — see `mcb/include/MeshCraft/Mcb/McbFormat.hpp`,
  fully documented in `MCB_FORMAT.md`) mirroring `Mc3Document`'s full
  field set, used as a faster-load alternative to XML. It is **not** an
  authoring format.
- CSG (union/difference/intersection) is evaluated via the **Manifold**
  library (`mc3togltf/src/CsgEvaluator.cpp`), pinned at v3.0.0. Strict
  mode (default) requires real boolean evaluation to succeed; approximate
  mode (`--allow-approximate-csg`) exports CSG children as separate
  meshes instead, for debugging/preview only.

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`, generated with CLion's bundled cmake
  4.2.2): last full rebuild + full `ctest` run was clean at commit
  `9220d75` — **52/52 tests passed**, 0 warnings from MeshCraft's own
  sources. Working tree clean as of `d5b5153` (docs-only since).
- **Release** (`b-release/`): not re-verified this effort — re-run the
  commands in §7 before relying on it.
- **Standalone (CNA-free) component builds** (`mc3`, `mcb`, `mc3togltf`,
  `mc3tomcb`): each configures/builds/tests independently of the root
  project. `mcb`'s standalone build was re-verified at commit `22b1c5d`.
  The other three were last confirmed passing earlier in this effort —
  re-verify with §7's commands if you depend on them.
- **MinGW (Windows) cross-compile**: configure succeeds; build reaches
  ~73% (328/449 objects) before failing on a CNA-side issue (missing
  GLES3 headers for a Windows target). Out of scope for this repo — see §4.
- **Emscripten (web) build**: configure and build both succeed 100%
  (449/449, exit 0) and the compiled JS/WASM genuinely executes (verified
  via Node). Loaded in a real browser: initializes correctly, but renders
  a blank canvas — the 3D editor is not yet usable on web. See §4.

### Tests
**52/52 CTest pass** in Debug as of commit `9220d75` (up from 48 at the
start of this effort — 4 new mc3togltf tests added while closing S4:
`mc3togltf_node_transform`, `mc3togltf_cli_extension`,
`mc3togltf_export_stats`, `mc3togltf_empty_scene`). `ctest -N` lists all
52 by name; `ctest --print-labels` groups them into `format`/`export`/
`render`/`registry`/`ai`/`commands`.

### Tools / libraries available
- `MeshCraft` — editor executable, version `0.1.0`. Builds and runs on
  Linux; `--screenshot <path>` (headless PPM render), `--export <path>`
  (non-interactive glTF/GLB export).
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb` (`--help`/`-h`,
  `--allow-approximate-csg`, `--stats` — now including a `Materials:`
  count, added this effort).
- `mc3tomcb` — bidirectional CLI: `mc3.xml` ↔ `.mcb`.
- `Mc3` / `Mcb` / `mc3togltf_lib` — static libs (scene data, binary
  serialization, glTF export).

### What works
- Full XML round-trip for all object types and all N1–N7 extensions,
  including `<include>` edge cases.
- **MCB binary round-trip is now fully verified (S3 closed this effort)**:
  every field written by `McbWriter` is read back by `McbReader`,
  including `Mc3Action::autoplay` (a real bug found and fixed this
  effort — was silently dropped by MCB despite being a real, schema-
  defined, fully XML-round-tripped field). Unknown-key skipping/forward
  compatibility, malformed-input handling, UTF-8 strings, determinism,
  and endianness are all empirically verified with dedicated tests, and
  the whole format is documented in `MCB_FORMAT.md`.
- **glTF/GLB export is now fully verified (S4 closed this effort)**:
  every primitive, material field, texture (incl. missing-file warnings),
  static node transform (position/rotation/scale — previously untested;
  only *animated* transforms had coverage), camera/light export, group
  hierarchy (parent-child `node.children`, a real gap found and closed),
  unsupported-animation-channel warnings, CLI extension handling
  (`.foo` rejected, `.glb`/`.gltf`/`.GLB` all correct, silent overwrite
  documented), export statistics accuracy (a real gap found: no
  material-count stat existed at all — added `ExportStats::materialCount`
  and the `--stats` `Materials:` line), and empty-scene export.
- XSD validation, SQLite-backed asset registry, editor undo/redo,
  auto-save + rotating backup, AI Assistant, viewport ray-cast
  click-to-select — all as documented in prior sessions, unchanged.

### What does NOT work yet
- **The Emscripten web build doesn't visually render the 3D editor** —
  see §4, unchanged since it was first found.
- `EditorViewport` is not integrated into the `MeshCraftApplication`
  render loop (interactive mode).
- SVG texture rasterization: parsed/serialized/round-tripped, but
  `GltfExporter` never reads the SVG texture map — silently dropped from
  export (a warning is printed, confirmed by test this effort).
- Embedded glTF (`<mesh src="embed:id"/>`): parsed/serialized, but
  `GltfExporter` treats `embed:id` as a literal OBJ path — export
  continues with an empty (meshless) node, warning printed (confirmed
  by test this effort). Deliberate, documented limitation.
- `<embeds>` inside an `<include>`d file is never merged — narrow,
  accepted limitation (STAB-0092), documented in `MC3_FORMAT.md`.
- N3–N7 scene data: fully round-tripped but not executed at runtime.
- CI workflow exists but is parked deactivated under `.github_/`.
- MinGW (Windows) cross-compile builds to ~73% then fails on a CNA-side
  GLES3 header issue.
- A cluster of visual toggles (bounding-box overlay, SSAO, bloom,
  wireframe mode, gizmos, shadow-map debug, etc.) can't be re-verified
  fresh in this headless environment — see §5.

---

## 3. Recent changes

This session closed out **S3 (MCB Binary Format Stability)** and **S4
(glTF/GLB Exporter Correctness) entirely** — both now 100% ✅. Highlights,
newest first (all on `develop`, all pushed):

- **S4 close-out** (`mc3togltf/src/GltfExporter.{hpp,cpp}`, `main.cpp`,
  new tests `node_transform_test.py`/`cli_extension_test.py`/
  `export_stats_test.py`/`empty_scene_test.py`): found and closed two real
  gaps — (1) no test verified a *static* object's position/rotation/scale
  export to `node.translation`/`rotation`/`scale` (only animated
  transforms were tested); (2) no `--stats` material-count field existed
  at all, so `ExportStats::materialCount` was added. Also closed a real
  hierarchy gap: `all_objects_export_test.py` never checked that
  `<group>` children actually appear in the glTF node's `children` array
  (STAB-0197). ~15 other rows turned out to already be satisfied by
  existing tests (camera/light export, SVG/embed warnings, JSON
  validity, unsupported-animation-channel warnings, OBJ loading) — stale
  rows from the original plan generation, marked done with references.
  Investigated and confirmed `CubeA`/`CubeB` correctly do *not* share a
  glTF mesh (different materials — by design, not a bug).
- **S3 close-out** (`mcb/src/McbWriter.cpp`/`McbReader.cpp`, new
  `mcb_roundtrip_test.cpp` tests, new `MCB_FORMAT.md`): **real bug
  found and fixed** — `Mc3Action::autoplay` was never written/read by
  MCB at all (same silent-drop class as the STAB-0131 `includes` bug).
  Also empirically verified unknown-key skipping/forward compatibility,
  malformed-input handling (truncated/all-zero/single-byte inputs all
  fail cleanly), UTF-8 strings, writer determinism, and the wire
  format's little-endian encoding (proven independent of host
  endianness). Wrote `MCB_FORMAT.md` documenting the whole format.

Full history is in `git log --oneline`; `plan.md` has a per-row writeup
for every `STAB-XXXX` ID mentioned above.

---

## 4. Current blocker / main problem

**No blocker to local development or testing on Linux** — Debug builds
and passes 52/52 tests as of `9220d75`.

The most important **open problem**, unchanged across several sessions,
is the Emscripten web build's blank canvas:

- **Symptom**: `MeshCraft.html`, served locally and opened in a real
  browser, loads and initializes correctly but the `<canvas>` stays
  blank/black.
- **What has already been tried**: confirmed CNA's `Game::RunLoop()`
  correctly branches on `__EMSCRIPTEN__` and keeps rendering; confirmed
  all 7 EasyGL 3D shader programs compile/link without error. The
  per-frame cause was not root-caused — needs temporary frame-counter/
  `glGetError()` diagnostic logging in `Draw()`/`EndDraw()`, a rebuild,
  and a human browser-retest. Deferred at the user's request as lower
  priority than the plain backlog work.

A second, lower-priority open item: **MinGW (Windows) cross-compile
builds to ~73%** then fails with a CNA-side GLES3 header gap, out of
scope to fix from this repo.

A third, purely **operational** issue: CI is parked deactivated under
`.github_/workflows/ci.yml` because the git remote's credentials lack
the `workflow` scope.

---

## 5. Known bugs and limitations

- **Emscripten web build renders a blank canvas** — see §4. _confirmed,
  not fixed, needs frame-level diagnostic logging + a browser retest._
- **MinGW cross-compile fails at ~73%** on a CNA-side GLES3 header
  issue — see §4. _confirmed, blocked on CNA, not fixable from here._
- **CI cannot be activated with the current git credentials** —
  _confirmed; needs owner action on the git remote/credentials._
- **SVG texture rasterization** not implemented in `GltfExporter`.
  _incomplete, documented, blocked on a library choice (librsvg vs.
  NanoSVG)._
- **Embedded glTF** (`embed:id`) fails to parse as an OBJ path during
  glTF export; doesn't crash, exports a meshless node. _incomplete,
  deliberately documented, real fix is a new feature._
- **`<embeds>` inside an `<include>`d file is never merged** — narrow,
  accepted limitation (STAB-0092), documented in `MC3_FORMAT.md`.
- **`mc3.xsd` has no numeric range constraints** — a schema-valid AI
  response can contain a negative `size`/`radius` and it applies
  unchanged. _confirmed, documented in `README.md`, not fixed._
- **`mc3.xsd`'s `mip_maps` texture attribute has zero implementation.**
  _confirmed, not fixed, no assigned STAB-XXXX ID._
- **A cluster of visual toggles need a live display to re-verify**
  (bounding-box overlay, SSAO, bloom, wireframe, gizmos, shadow-map
  debug, etc.). _needs verification by a human with a live display._
- **`--screenshot` always writes raw PPM** regardless of the output
  path's extension. _confirmed, deliberate, documented._
- **N3–N7 are data-only** — round-tripped but nothing executes them at
  runtime. _intended at this stage, not a bug._
- **`mc3` standalone build skips `mc3_commands`** — needs
  `EditorAlgorithms.hpp` from the editor tree, absent standalone.
  _intended, not a bug._
- **3 `plan.md` items cannot be completed in this environment**:
  STAB-0642 (needs Blender), STAB-0643 (needs a browser — partially
  reachable), STAB-0650 (needs CI actually running).
- **S5 (CSG) has 30 open/uncertain rows** — the next section to work
  through, see §8.

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
model) → either `Mc3XmlWriter` (save), `McbWriter` (binary, see
`MCB_FORMAT.md`), or `GltfExporter` (glTF/GLB).

**CSG:** `mc3togltf/src/CsgEvaluator.cpp` evaluates union/difference/
intersection via the Manifold library (v3.0.0, pinned in `CMakeLists.txt`,
`MANIFOLD_PAR`/`MANIFOLD_CROSS_SECTION` both OFF). Strict mode (default)
throws on evaluation failure; `--allow-approximate-csg` exports children
as separate meshes instead (geometrically wrong, debug-only). Mesh
sharing for `<instance>` nodes is keyed on definition+material+deform
(`buildDefCacheKey()` in `GltfExporter.cpp`) — two instances of the same
definition with *different* materials correctly do NOT share a mesh.

**`doc.sourcePath` is a single directory per document.** Everything
resolves relative to it; `<include>`d files from a different directory
are rebased via `Mc3XmlParser.cpp`'s `rebaseRelativePath()`/
`rebaseDefinitionMeshSources()`.

**Undo/redo:** snapshot-based. `undoStack_`/`redoStack_` (capped at 20)
hold `std::vector<Mc3::Mc3Document>`.

**The "Alg mirror" pattern**: pure editor/AI logic lives in a CNA-free
header (`EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`), functions
suffixed `Alg`, called by both the real `.cpp` files and the headless
test suite directly. **Caution**: check both sides stay wired up when
touching one.

**GL rendering caution**: a VAO-based vertex-attribute draw path was
found completely non-functional in this environment's GL setup in a
prior session (the skybox — fixed by switching to a `gl_VertexID`-based
procedural approach).

**Embedding a resource file at compile time**: `mc3.xsd` is compiled into
`MeshCraft`/`ai_test` as a raw string constant at CMake **configure**
time. **Editing `mc3.xsd` requires a full reconfigure, not just a
rebuild.**

**Hard constraints / invariants:**
- `Mc3Document` public API: don't change without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` must stay buildable standalone
  (no CNA/ImGui deps).
- `mc3.xsd` must stay symmetric with `Mc3XmlWriter.cpp`.
- Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`
  (triggers a full CNA recompile).
- Do not modify CNA/SHARP_RUNTIME source files from this repo.
- `file(GLOB_RECURSE)` collects sources — a new `.cpp` file (or a new
  `add_test()`, or an edit to `mc3.xsd`) needs a cmake **reconfigure**,
  not just a rebuild.
- MCB format version is `MCB_VERSION` in `McbFormat.hpp` — bump on any
  breaking wire-format change; adding a new optional key is additive
  and doesn't need one (see `MCB_FORMAT.md`'s forward-compatibility
  section).
- XSD root element order is strict.
- Reconfigure `cmake-build-debug/` only with CLion's bundled cmake, not
  the system cmake.
- `find_package(SQLite3)`/`OpenSSL`/`LibXml2` are all intentionally
  optional on desktop builds — don't make any of them `REQUIRED` again.

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid a system-cmake bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5   # (re)configure — flag needed since CNA vendors ENet with an old cmake_minimum_required
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (52)
ctest -N                                                      # lists all 52 tests
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
ctest -R mc3togltf_csg --output-on-failure   # CSG-related exporter tests (next section, S5)

# --- Push (normal pushes have worked throughout; only CI activation is blocked, see §4)
git push origin develop
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

Continue **S5 (CSG Stability)**, `plan.md` priority order — the next
section, 23 `📋` + 7 `🧪` of 35 rows open. Existing CSG test files:
`csg_export_test.py`, `csg_strict_test.py`, `csg_nested_test.py`,
`csg_unsupported_test.py`. Before writing new tests, grep these for
whether a row is already satisfied — this effort found roughly a third
of "open" rows in S0-S4 were stale (already satisfied, just never
marked done).

Notable open rows to check/implement, roughly in priority order:

1. **STAB-0206** — 3-level deep CSG nesting (existing `csg_nested_test.py`
   only tests 2 levels — check before extending).
2. **STAB-0207** — CSG with transforms inside children uses correct
   world transform (verify `CsgEvaluator.cpp` applies child transforms
   before the boolean op, with a real translated-box-minus-sphere test).
3. **STAB-0208** — CSG cutter semantics (`isCutter` flag) — likely
   already covered by `csg_export_test.py`/`csg_unsupported_test.py`,
   check first.
4. **STAB-0209/0219** — empty CSG result policy (non-overlapping
   intersection/difference) — needs investigation of actual Manifold
   behavior first (empty mesh vs. warning), then document + test.
5. **STAB-0210** — non-watertight geometry in CSG — Manifold error
   handling, needs a genuinely degenerate OBJ fixture.
6. **STAB-0211/0231** — `--allow-approximate-csg` mode: each child
   exported as a separate node/mesh, including nested CSG children.
7. **STAB-0213-0218** — CSG preview cache (editor-side,
   `SceneRenderer.cpp`/`PropertiesPanel.cpp`): cached triangle count,
   cache invalidation on child/parent move, 128-entry eviction, undo of
   child reorder. These need the headless `MeshCraft` test harness, not
   just `mc3togltf` Python tests — check `mc3_commands_test.cpp`/
   `editor_commands_test.cpp` for existing SceneRenderer test patterns.
8. **STAB-0222/0232/0235** — Manifold version/parallelism/cross-section
   config — mostly documentation/investigation, not new tests.
9. **STAB-0224/0225** — CSG UV/normal loss in Manifold output — document
   as a limitation (UV preservation would be a real feature, likely out
   of scope for "document" per this stabilization phase).
10. **STAB-0226/0229** — Manifold exception handling, CSG node name
    preservation in export.
11. **STAB-0227/0228** — CSG edge cases: identical-sphere intersection,
    full-hole difference.
12. **STAB-0233** — CSG performance test (5-level nested union of 32
    boxes, <5s) — lower priority (P3), a genuinely new perf test.

**After S5 closes**, continue to **S6 (Geometry Reuse, Instancing, Large
Scenes)** in `plan.md` order (25 rows, 7 done, 5 🧪 + 13 📋 open).

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
- **No SVG rasterization work** until a library choice is made.
- **No implementing `embed:` mesh-source resolution in `GltfExporter`**
  or **`<embeds>`-inside-`<include>` merging** without an explicit
  decision to take either on.
- **No mass refactoring** of passing code, no speculative architecture
  changes — stabilization phase; scope each change to exactly what its
  `STAB-XXXX` entry asks for.
- **No attempting to "fix" the MinGW build by vendoring GLES headers
  ourselves** — that's CNA's call.
- **No attempting STAB-0642/0643/0650** without the missing tool/access
  first (Blender, a browser, an active CI run respectively).
- **Don't assume a `plan.md` row needs new work without checking first**
  — a quick `grep`/`ctest -R` check before writing new code has
  consistently paid off (roughly a third of "open" S0-S4 rows turned out
  to already be satisfied).
- **No implementing CSG UV preservation (STAB-0225)** without an explicit
  decision — likely a documentation-only row given the stabilization
  phase's "no new features" rule, but investigate before assuming.

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect the 4 existing CSG test files
(mc3togltf/test/csg_export_test.py, csg_strict_test.py, csg_nested_test.py,
csg_unsupported_test.py) plus the S5 section of plan.md (STAB-0201-0235)
before writing any new code — grep for whether each open row is already
satisfied first (this codebase has had many stale plan.md rows; roughly a
third of "open" rows in S0-S4 turned out to already be done). Make one
small, verified improvement at a time. Run the relevant build/test command
from section 7 and confirm cmake-build-debug still passes (52/52, or the
new total if you registered a new ctest). Update plan.md's row for the task
you completed, and update NEXT.md's section 3/8 after finishing a batch
(not necessarily after every single row).

Current branch: develop, in sync with origin/develop at commit d5b5153.
Build dir: cmake-build-debug/ (Debug, CLion cmake 4.2.2) — last full
rebuild + 52/52 ctest was clean at commit 9220d75, working tree clean as
of d5b5153 (docs-only commits since).

Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file / new add_test()
requires a reconfigure, not just a rebuild — see section 6.

The Emscripten blank-canvas issue (section 4) needs a human with a
browser to close the loop — if none is available, stay on the plain S5
backlog work in section 8 instead.

CI is parked deactivated under .github_/ (credentials issue, see
section 4). Commit after each STAB-XXXX task and push to origin/develop
— this has been the standing workflow across this whole stabilization
effort.
```
