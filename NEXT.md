# NEXT.md

_Last updated: 2026-07-06, commit `ad7aeca` (develop, in sync with `origin/develop`)_

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

**Current phase:** Stabilization. **526/651** `plan.md` rows are ✅ (up
from 361 at the start of this session — S3, S4, and S5 all closed in one
sitting). Sections **S0, S1, S2, S3, S4, S5, S13, S17, S19** are fully
closed. S16/S18/S20 are closed except for rows genuinely blocked on a
missing tool or a live human session. **S6 (Geometry Reuse, Instancing,
Large Scenes) is next** — 25 rows, 7 done, 18 open — see §4/§8.

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
  `Alg`, e.g. `EditorAlgorithms.hpp`, and now also
  `Renderer/CsgCacheAlg.hpp`) that both the real app code and the
  headless test suite `#include` and call directly. See §6.
- MCB (`mcb/`) is a custom tagged binary format, fully documented in
  `MCB_FORMAT.md`, used as a faster-load alternative to XML. Not an
  authoring format.
- CSG (union/difference/intersection) is evaluated via the **Manifold**
  library (`mc3togltf/src/CsgEvaluator.cpp`), pinned at v3.0.0. Strict
  mode (default) requires real boolean evaluation to succeed; approximate
  mode (`--allow-approximate-csg`) exports CSG children as separate
  meshes instead, for debugging/preview only. **`isCutter`/`role="cutter"`
  semantics matter a lot** — a `Difference` treats non-cutter children as
  a second base (unioned in), not subtracted; a missing `role="cutter"`
  silently degrades a real cut into a no-op union (found and fixed a real
  bug in the S5 test fixture from exactly this mistake — see §3).
- The editor's own CSG preview (`SceneRenderer.cpp`, separate code path
  from `mc3togltf`'s exporter) caches boolean results by a content hash
  (`csgSubtreeHashAlg()`, `CsgCacheAlg.hpp`) folded with the accumulated
  parent world matrix; the cache is bounded by clearing entirely once it
  would exceed 128 entries (not LRU-to-128).

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`, generated with CLion's bundled cmake
  4.2.2): last full rebuild + full `ctest` run was clean at commit
  `7941a75` — **56/56 tests passed** (up from 48 at the start of this
  session). Working tree clean as of `ad7aeca` (docs-only since).
- **Release** (`b-release/`): not re-verified this effort — re-run the
  commands in §7 before relying on it.
- **Standalone (CNA-free) component builds** (`mc3`, `mcb`, `mc3togltf`,
  `mc3tomcb`): not re-verified this session — re-run §7's commands if you
  depend on them. Note: `CsgCacheAlg.hpp` is CNA-free by construction
  (only depends on `Mc3Document`/`Mc3Object`), so it doesn't affect
  standalone buildability.
- **MinGW (Windows) cross-compile**: configure succeeds; build reaches
  ~73% before failing on a CNA-side GLES3 header issue. Out of scope —
  see §4.
- **Emscripten (web) build**: configure and build both succeed 100% and
  the compiled JS/WASM genuinely executes. Loaded in a real browser:
  initializes correctly but renders a blank canvas — see §4.

### Tests
**56/56 CTest pass** in Debug as of commit `7941a75` (up from 48 at the
start of this session). New this session: 4 mc3togltf export tests (S4),
6 mc3togltf CSG tests (S5: `csg_semantics`, `csg_mesh_child`, `csg_stress`
+ extensions to `csg_export`/`csg_nested`), and 1 new root-level render
test (`csg_cache_eviction_test`) plus an extension to `csg_cache_test`.

### Tools / libraries available
- `MeshCraft` — editor executable, version `0.1.0`. `--screenshot <path>`
  now also prints `[CsgCache] size: N` and `[CsgTriCount] count=N`
  diagnostics (STAB-0213/0216) alongside the existing `[CsgCache]
  evaluations:`/`[LOD]` lines.
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb` (`--help`/`-h`,
  `--allow-approximate-csg`, `--stats`, now including a `Materials:` count).
- `mc3tomcb` — bidirectional CLI: `mc3.xml` ↔ `.mcb`.

### What works
- Full XML round-trip; **MCB binary round-trip fully verified** (S3
  closed) including a real bug fix (`Mc3Action::autoplay` was silently
  dropped by MCB); format fully documented in `MCB_FORMAT.md`.
- **glTF/GLB export fully verified** (S4 closed): every primitive,
  material field, texture, static node transform, camera/light, group
  hierarchy, CLI extension handling, export statistics (added a
  previously-nonexistent material-count stat), empty-scene export.
- **CSG fully verified** (S5 closed): isCutter semantics, child
  world-transform composition, empty-result policy, material-on-CSG-node,
  3-level nesting (found and fixed a real silent-no-op bug in the nested
  fixture — see §3), Mesh-child rejection, 10/150-node stress tests, and
  the editor's separate preview-cache mechanism (triangle-count
  diagnostic, cache-key invalidation on child/parent move, 128-entry
  eviction bound) — all empirically verified, not just read from code.
- XSD validation, SQLite-backed asset registry, editor undo/redo,
  auto-save + rotating backup, AI Assistant — unchanged from prior
  sessions.

### What does NOT work yet
- **The Emscripten web build doesn't visually render the 3D editor** —
  see §4, unchanged since it was first found.
- `EditorViewport` is not integrated into the `MeshCraftApplication`
  render loop (interactive mode).
- SVG texture rasterization and embedded glTF (`embed:id`): both
  documented, deliberate limitations (warn + degrade gracefully).
- `<embeds>` inside an `<include>`d file is never merged — narrow,
  accepted limitation (STAB-0092).
- CSG output has no real UVs (hardcoded placeholder) and flat-only
  normals — documented as a real, investigated-but-not-implemented
  limitation (STAB-0224/0225): Manifold never receives a UV channel to
  carry through the boolean op in the first place.
- N3–N7 scene data: fully round-tripped but not executed at runtime.
- CI workflow exists but is parked deactivated under `.github_/`.
- MinGW (Windows) cross-compile builds to ~73% then fails on a CNA-side
  GLES3 header issue.
- A cluster of visual toggles can't be re-verified fresh in this headless
  environment — see §5.

---

## 3. Recent changes

This session closed **S3 (MCB Binary Format Stability)**, **S4 (glTF/GLB
Exporter Correctness)**, and **S5 (CSG Stability) entirely** — all now
100% ✅ (165 rows closed total). Highlights, newest first:

- **S5 close-out**: the editor's separate CSG preview cache
  (`SceneRenderer.cpp`) was verified by extracting its content-hash logic
  (`csgSubtreeHash`) into a new CNA-free header
  (`include/MeshCraft/Renderer/CsgCacheAlg.hpp`) so it's unit-testable
  without a live `GraphicsDevice` — confirmed child-move and parent-move
  both invalidate the cache key. Added `csgCachedTriCount`/
  `csgMeshCacheSize` diagnostics to `--screenshot` mode. **Real bug found
  and fixed**: the existing 2-level nested-CSG test fixture
  (`test/csg_nested.mc3.xml`) was missing `role="cutter"` on its cutter
  child, so `Difference` silently treated it as a second base child
  (unioned in) instead of subtracted — the fixture's claimed "36 verts"
  hollowed-box result was actually just a bare box (no cavity at all),
  and the old test's presence-only assertions passed regardless. Fixed
  and extended to a genuine 3-level nesting (4380 verts, proving a real
  cut). Also added `mc3togltf_csg_semantics` (isCutter/world-transform/
  empty-result/material-on-node/identical-sphere checks, all using
  calibrated vertex-count comparisons rather than presence-only checks)
  and `mc3togltf_csg_stress` (10-box union, 5-level/32-box union <5s).
- **S4 close-out**: found and closed two real gaps — no test verified a
  *static* object's position/rotation/scale export to glTF node TRS
  (only animated transforms were tested), and no `--stats` material-count
  field existed at all (`ExportStats::materialCount` added). Also closed
  a real hierarchy gap: `<group>` children weren't checked to actually
  appear in the glTF node's `children` array. ~15 other rows turned out
  to already be satisfied by existing tests — stale rows, marked done
  with references.
- **S3 close-out**: **real bug found and fixed** — `Mc3Action::autoplay`
  was never written/read by MCB at all (same silent-drop class as the
  earlier STAB-0131 `includes` bug). Also verified unknown-key skipping,
  malformed-input handling, UTF-8 strings, writer determinism, and the
  little-endian wire format. Wrote `MCB_FORMAT.md`.

Full history is in `git log --oneline`; `plan.md` has a per-row writeup
for every `STAB-XXXX` ID mentioned above.

---

## 4. Current blocker / main problem

**No blocker to local development or testing on Linux** — Debug builds
and passes 56/56 tests as of `7941a75`.

The most important **open problem**, unchanged across several sessions,
is the Emscripten web build's blank canvas (see prior NEXT.md revisions
for full diagnostic history — unchanged this session, deferred as lower
priority than backlog work).

A second, lower-priority item: **MinGW (Windows) cross-compile builds to
~73%** then fails on a CNA-side GLES3 header gap, out of scope for this
repo.

A third, purely **operational** issue: CI is parked deactivated under
`.github_/workflows/ci.yml` because the git remote's credentials lack the
`workflow` scope.

---

## 5. Known bugs and limitations

Unchanged from before this session except as noted in §2/§3 above
(CSG UV/normal limitation now has a deeper investigated explanation; the
nested-CSG test-fixture bug is fixed). See prior revisions for the full
list: Emscripten blank canvas, MinGW cross-compile gap, CI credentials,
SVG rasterization, embedded glTF, `<embeds>`-in-`<include>`, `mc3.xsd`
numeric range constraints, `mip_maps` unimplemented, visual-toggle
re-verification needing a live display, `--screenshot` always PPM, N3-N7
data-only, `mc3` standalone skipping `mc3_commands`, and the 3
externally-blocked `plan.md` items (STAB-0642/0643/0650).

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

**Two independent CSG code paths, same `isCutter` semantics:**
`mc3togltf/src/CsgEvaluator.cpp` (export-time, strict/approximate modes)
and `src/MeshCraft/Renderer/SceneRenderer.cpp`'s `buildManifoldTree()`
(editor preview, always-on content-hash cache). Both key `Difference` on
`obj.isCutter` (`role="cutter"` in XML) identically: non-cutter children
union into a "base", cutter children subtract from it. **A CSG child
missing `role="cutter"` doesn't error — it silently changes a subtraction
into a union**, which is easy to get wrong when hand-writing test
fixtures (see §3's found-and-fixed bug).

**`doc.sourcePath` is a single directory per document.** Everything
resolves relative to it; `<include>`d files from a different directory
are rebased via `Mc3XmlParser.cpp`.

**Undo/redo:** snapshot-based. `undoStack_`/`redoStack_` (capped at 20)
hold `std::vector<Mc3::Mc3Document>`. Any `pushUndo()`-preceded mutation
is restorable by construction (whole-document copy preserves everything,
including child-vector order) — no per-mutation-type undo logic needed.

**The "Alg mirror" pattern**: pure editor/renderer logic lives in a
CNA-free header (`EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`,
and now `Renderer/CsgCacheAlg.hpp`), functions suffixed `Alg`, called by
both the real `.cpp` files and the headless test suite directly.
**Caution**: check both sides stay wired up when touching one side of a
mirrored pair.

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
- Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`.
- Do not modify CNA/SHARP_RUNTIME source files from this repo.
- `file(GLOB_RECURSE)` collects sources — a new `.cpp` file (or a new
  `add_test()`, or an edit to `mc3.xsd`) needs a cmake **reconfigure**.
- **XML comments must never contain `--` anywhere inside them** (not just
  at start/end) — `tinyxml2` (the app's own parser) is lenient about
  this, but `lxml` (`test/validate_xsd.py`) rejects it outright. Found
  this the hard way multiple times this session writing new fixture
  comments — check new `.mc3.xml` fixtures with `validate_xsd.py` before
  trusting them.
- Reconfigure `cmake-build-debug/` only with CLion's bundled cmake.
- `find_package(SQLite3)`/`OpenSSL`/`LibXml2` are all intentionally
  optional on desktop builds.

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid a system-cmake bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (56)
ctest -N                                                      # lists all 56 tests
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
  rm -rf "$c-build"
done

# --- Validate an XML fixture (also catches the "--" inside comment bug)
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# --- MinGW / Emscripten builds — see prior NEXT.md revisions for full commands

# --- Run / export / validate / version
./cmake-build-debug/MeshCraft test/house.mc3.xml
./cmake-build-debug/MeshCraft --version
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
ctest -R mc3_roundtrip --output-on-failure
ctest -R mcb_roundtrip --output-on-failure
ctest -R "mc3togltf_csg" --output-on-failure   # all 7 CSG export tests (S5)
ctest -R csg_cache --output-on-failure         # editor CSG preview-cache tests (S5)

# --- Push
git push origin develop
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

Continue **S6 (Geometry Reuse, Instancing, Large Scenes)**, `plan.md`
priority order — 25 rows, 7 done, 18 open (5 🧪 + 13 📋). Existing test
files: `float_cache_key_test.py`, `instance_deform_cache_test.py`,
`large_scene_test.py`, `large_scene_generated_test.py`. Before writing new
tests, grep these for whether a row is already satisfied — this whole
effort found roughly a third of "open" rows in S0-S5 were stale (already
satisfied, just never marked done).

Notable open rows, roughly in priority order:

1. **STAB-0240** — material-specific cache key (same primitive, different
   material → 2 unique meshes). Check `float_cache_key_test.py`/
   `instance_deform_cache_test.py` first; likely needs a small new
   fixture+test if not covered.
2. **STAB-0241** — repeated OBJ mesh `src` reuses the same glTF mesh
   (2 objects, same `src=`, confirm 1 mesh / 2 nodes). Check
   `ctx.geomMeshCache`/`buildGeomCacheKey()` in `GltfExporter.cpp` — the
   cache key likely already includes the mesh source path; verify with a
   new small test.
3. **STAB-0246/0247** — verify existing test assertions
   (`large_scene_test.py` node count, empty-definition instance no-crash)
   — check first, likely stale/already covered given this effort's
   pattern.
4. **STAB-0248** — multi-child definition instanced 10x → 10 nodes,
   1 mesh *set* (not just 1 mesh — a definition with multiple children
   produces multiple meshes, all of which should be shared across
   instances).
5. **STAB-0249/0250** — editor CSG cache correctness after
   undo/document-load. STAB-0249 (content-hash not raw-pointer keyed) is
   already provably true by construction (`csgSubtreeHashAlg` hashes
   content, not addresses — see `CsgCacheAlg.hpp`, verified this session
   for STAB-0214/0215) — likely just needs a reference added, not new
   tests. STAB-0250 (`clearCsgCache()` called on document load) — grep
   `MeshCraftApplication_FileOps.cpp` for the actual call site first.
6. **STAB-0251/0254/0255/0256/0260** — GLB file-size sanity, cache
   transform-leak check, cache-key documentation, animation/cache
   interaction, material-reuse-count in a large scene. Mix of
   quick-to-verify and needs-new-test.
7. **STAB-0245/0252/0253/0257/0258** — lower-priority (P2/P3) stress/
   research tasks: 1000-object test, Blender import (needs Blender, may
   not be completable in this environment), profiling, 1M-triangle OBJ,
   streaming-export investigation.

**After S6 closes**, continue to **S7 (Editor Save/Load/Data-Loss
Workflows)** in `plan.md` order (35 rows, 28 done, 7 open — all 📋, no 🧪,
likely a mix of stale rows and small real gaps).

---

## 9. Do not do yet

Unchanged from prior revisions — see NEXT.md history for the full list
(no new scene-format features, no CNA/SHARP_RUNTIME changes, no
`Mc3Document` public API changes without checking dependents, no SVG
rasterization or `embed:`/`<embeds>` implementation without an explicit
decision, no mass refactoring, no MinGW GLES-header vendoring, no
STAB-0642/0643/0650 without the missing tool/access).

One addition from this session: **no CSG UV-preservation implementation**
(STAB-0225) — investigated and correctly concluded to be a real,
non-trivial feature (rebuilding every CSG-eligible primitive with
UV-carrying `MeshGL` data), not a quick fix; documented as an explicit
limitation instead.

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect the 4 existing S6 test files
(mc3togltf/test/float_cache_key_test.py, instance_deform_cache_test.py,
large_scene_test.py, large_scene_generated_test.py) plus the S6 section
of plan.md (STAB-0236-0260) before writing any new code — grep for
whether each open row is already satisfied first. This codebase has had
many stale plan.md rows; this effort alone found and closed ~165 rows
across S3-S5, a good fraction of which needed no new code, just a
reference to an existing test. Make one small, verified improvement at a
time. Run the relevant build/test command from section 7 and confirm
cmake-build-debug still passes (56/56, or the new total if you registered
a new ctest). Update plan.md's row for the task you completed, and update
NEXT.md's section 3/8 after finishing a batch (not necessarily after
every single row).

Current branch: develop, in sync with origin/develop at commit ad7aeca.
Build dir: cmake-build-debug/ (Debug, CLion cmake 4.2.2) — last full
rebuild + 56/56 ctest was clean at commit 7941a75, working tree clean as
of ad7aeca (docs-only commits since).

Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file / new add_test()
requires a reconfigure, not just a rebuild — see section 6. New .mc3.xml
fixture comments must never contain "--" anywhere inside them (lxml/
validate_xsd.py rejects it even though tinyxml2 doesn't) — validate new
fixtures with test/validate_xsd.py before trusting them.

The Emscripten blank-canvas issue (section 4) needs a human with a
browser to close the loop — if none is available, stay on the plain S6
backlog work in section 8 instead.

CI is parked deactivated under .github_/ (credentials issue, see
section 4). Commit after each STAB-XXXX task and push to origin/develop
— this has been the standing workflow across this whole stabilization
effort.
```
