# NEXT.md

_Last updated: 2026-07-06, commit `ee68b5b` (develop, in sync with `origin/develop`)_

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

**Current phase:** Stabilization. **550/651** `plan.md` rows are ✅ (up
from 361 at the start of this session — S3 through S7 all closed or
effectively closed in one sitting, 189 rows). Sections **S0, S1, S2, S3,
S4, S5, S6, S7, S13, S17, S19** are fully closed or closed-except-one-
flagged-row. S16/S18/S20 similarly. **S8 (UI Robustness) is next** — 40
rows, 17 done, 23 open (all 📋, no 🧪) — see §4/§8.

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
  `Alg`, e.g. `EditorAlgorithms.hpp`, `Renderer/CsgCacheAlg.hpp`) that
  both the real app code and the headless test suite `#include` and call
  directly. **Not every Alg mirror is wired back into the real `.cpp`** —
  some (e.g. `PrefsAlg`, the new `loadRecentFilesAlg` family) are
  deliberately left as parallel duplicates purely for testability. This
  is an accepted, precedented pattern in this codebase, not a bug — but
  it does mean the two copies could drift if one side changes without
  the other being checked (see §6).
- MCB (`mcb/`) is a custom tagged binary format, fully documented in
  `MCB_FORMAT.md`. CSG evaluated via **Manifold** (`CsgEvaluator.cpp`),
  pinned v3.0.0 — see prior NEXT.md revisions for full CSG semantics
  (isCutter, world-transform composition, empty-result policy, etc.,
  all verified this effort in S5).

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`): last full rebuild + full `ctest` run
  clean at commit `4b82832` — **63/63 tests passed** (up from 48 at the
  start of this session). Working tree clean as of `ee68b5b` (docs-only
  since).
- **Release**/standalone builds: not re-verified this session — re-run
  §7's commands if you depend on them.
- **MinGW**: ~73%, CNA-side GLES3 gap, out of scope. **Emscripten**:
  builds 100%, blank-canvas rendering issue unchanged — see §4.
- Blender 4.3.2 happens to be installed in this environment — one new
  ctest (`mc3togltf_blender_import`) actually exercises it.

### Tests
**63/63 CTest pass** (up from 48 at session start). New this session: 4
mc3togltf export tests (S4), 8 mc3togltf CSG tests (S5), 8 mc3togltf
geometry-reuse/large-scene tests (S6, including a real headless Blender
import and a >1M-triangle OBJ stress test), 1 new root-level render test
(S5's `csg_cache_eviction_test`), plus extensions to several existing
Python tests and to `mc3_commands_test` (S7's recent-files, merge-scene
ID-collision, and export-subtree-XSD-validation coverage).

### What works (everything from before, plus this session's closures)
- **MCB (S3), glTF/GLB export (S4), CSG — both `mc3togltf`'s and the
  editor's separate preview cache (S5), geometry reuse/instancing/large
  scenes (S6), and editor save/load/undo/macro/export workflows (S7) are
  all now fully verified**, not just read from code. Full details of what
  was tested are in `plan.md`'s per-row writeups; highlights in §3 below.

### What does NOT work yet / known gaps
Unchanged from prior sessions (Emscripten blank canvas, MinGW GLES3 gap,
CI credentials, SVG rasterization, embedded glTF, `<embeds>`-in-
`<include>`, CSG has no real UVs) **plus one new confirmed gap found this
session**:
- **`mergeDocumentsAlg()` (Merge Scene) has no object-id-collision
  handling** (STAB-0289, flagged 🟡). Textures/materials/actions all
  suffix a colliding *key*; objects (a `std::vector`, not a map) are
  unconditionally appended with no id dedup. Not data-loss (nothing gets
  overwritten) but anything that looks an object up by id post-merge
  (`Mc3SceneState`/`Mc3ObjectOverride`) could resolve ambiguously. A real
  fix needs to walk the whole source object subtree (ids exist on every
  nested child) and rename anything colliding with the destination's
  existing tree — meaningfully more work than the flat-map suffix idiom
  used elsewhere, so it's documented and test-proven but **not fixed**.
  Picking this up is good, scoped follow-up work if editor stability
  work continues past S8.

---

## 3. Recent changes

This session closed **S3 (MCB), S4 (glTF/GLB export), S5 (CSG), S6
(Geometry Reuse/Large Scenes) entirely**, and **S7 (Editor Save/Load)
effectively** (34/35, one flagged gap) — 189 rows total. Highlights,
newest first:

- **S7**: confirmed the recent-files list is genuinely implemented and
  added test coverage for it (cap enforcement, dedup, persistence). Found
  and documented (not fixed) the merge-scene object-id-collision gap
  above. Closed a real testing gap in the export-subtree-template test:
  it previously only round-tripped through the lenient `tinyxml2` parser,
  never validated against the actual strict-order `mc3.xsd` — added a
  genuine schema-validation step via the project's own
  `test/validate_xsd.py`.
- **S6**: Blender happened to be installed in this environment, so a
  "needs Blender, may not be completable" row (STAB-0252) became a real,
  passing headless-Blender-import test instead of just an investigation.
  Also added a genuine >1M-triangle OBJ stress test (measured ~350MB
  peak RSS via `resource.getrusage`), confirmed geometry-cache/animation
  independence, material reuse across a large scene, and closed a real
  documentation gap (`buildGeomCacheKey()`'s exact per-`ObjectType` field
  composition was undocumented).
- **S5**: found and fixed a real bug in the *test fixture*, not the
  production code — the nested-CSG test was missing `role="cutter"`,
  silently testing a no-op union instead of a real 3-level difference
  (see prior NEXT.md revision for full details). Extracted the editor's
  CSG cache-invalidation hash to a new CNA-free header
  (`CsgCacheAlg.hpp`) for direct unit testing, and found/fixed a real bug
  where `clearCsgCache()` was only called on one of three document-load
  paths (File→New and the regular File→Open dialog were both missing
  it — a stale-cache-collision risk after switching files).
- **S4/S3**: see prior NEXT.md revisions — real bugs found and fixed
  include `Mc3Action::autoplay` being silently dropped by MCB, and a
  missing `--stats` material-count field in `mc3togltf`.

Full history is in `git log --oneline`; `plan.md` has a per-row writeup
for every `STAB-XXXX` ID.

---

## 4. Current blocker / main problem

**No blocker to local development or testing on Linux** — 63/63 tests
pass as of `4b82832`. The Emscripten blank-canvas issue, MinGW cross-
compile gap, and CI credentials issue are all unchanged from prior
sessions — see earlier NEXT.md revisions (`git log -p -- NEXT.md`) for
full diagnostic history if picking any of these up.

---

## 5. Known bugs and limitations

Unchanged from prior sessions (see earlier revisions for the full list:
Emscripten, MinGW, CI, SVG rasterization, embedded glTF, `<embeds>`-in-
`<include>`, `mc3.xsd` numeric ranges, `mip_maps`, visual-toggle
re-verification, `--screenshot` always PPM, N3-N7 data-only, `mc3`
standalone skipping `mc3_commands`, STAB-0642/0643/0650 externally
blocked) **plus**:
- **CSG has no real UVs/smooth normals** (investigated this session,
  STAB-0224/0225) — a real, non-trivial feature to fix (no UV channel is
  ever fed into Manifold), correctly left as a documented limitation.
- **`mergeDocumentsAlg()` has no object-id-collision handling** — see §2.
- **Not every `Alg` mirror is wired back into its real `.cpp`** — some
  are deliberately parallel duplicates (see §1/§6). Not itself a bug, but
  worth checking both sides when touching one.

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

**Data flow:** `.mc3.xml` → `Mc3XmlParser` → `Mc3Document` → either
`Mc3XmlWriter` (save), `McbWriter` (binary, `MCB_FORMAT.md`), or
`GltfExporter` (glTF/GLB).

**Two independent CSG code paths, same `isCutter` semantics:**
`mc3togltf/src/CsgEvaluator.cpp` (export) and
`src/MeshCraft/Renderer/SceneRenderer.cpp`'s `buildManifoldTree()`
(editor preview cache). Both key `Difference` on `obj.isCutter`
(`role="cutter"` in XML): non-cutter children union into a "base", cutter
children subtract. **A CSG child missing `role="cutter"` doesn't
error — it silently changes a subtraction into a union.** Always validate
new `.mc3.xml` fixtures with `test/validate_xsd.py` and sanity-check the
actual exported geometry (vertex/triangle count), not just "did it
export something" — a whole test session this effort ran on a silently
broken CSG fixture before this was caught.

**`Alg` mirror pattern — important nuance found this session**: the
comment at the top of `EditorAlgorithms.hpp` implies mirrors are called
by both real code and tests, but in practice **several mirrors
(`PrefsAlg`, `loadRecentFilesAlg`/etc.) are left as parallel duplicates**,
never wired into the real `.cpp` (which keeps its own inline copy of the
same logic). This is precedented and accepted in this codebase, but it
means drift is possible if one copy changes without checking the other —
always grep for the real `.cpp`'s equivalent function when touching a
mirror, and decide deliberately whether to wire it or leave it parallel
(matching the pattern the surrounding code already uses).

**Undo/redo:** snapshot-based, whole-`Mc3Document` copies. Any
`pushUndo()`-preceded mutation is restorable by construction.

**GL rendering caution**: a VAO-based vertex-attribute draw path was
found non-functional in this environment in a prior session (skybox
fix: `gl_VertexID`-based procedural approach instead).

**`mc3.xsd` is compiled into `MeshCraft`/`ai_test` at CMake configure
time** — editing it needs a full reconfigure.

**XML comment gotcha (found repeatedly this session)**: `--` anywhere
inside an XML comment (not just at start/end) is rejected by `lxml`
(`test/validate_xsd.py`) even though `tinyxml2` (the app's own parser) is
lenient about it. Always validate new `.mc3.xml` fixtures with
`validate_xsd.py` before trusting them.

**Hard constraints / invariants:** (unchanged from prior revisions)
`Mc3Document` public API stability, `mc3`/`mcb`/`mc3togltf`/`mc3tomcb`
standalone buildability, `mc3.xsd`/`Mc3XmlWriter.cpp` symmetry, no
`${meta-gl_SOURCE_DIR}/include`, no CNA/SHARP_RUNTIME edits, cmake
reconfigure needed for new `.cpp`/`add_test()`/`mc3.xsd` changes,
CLion-cmake-only reconfigure, optional `SQLite3`/`OpenSSL`/`LibXml2`.

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid a system-cmake bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (63)
ctest -N                                                      # lists all 63 tests
ctest --print-labels                                          # format/export/render/registry/ai/commands
ctest -L export --output-on-failure                           # run just one label group
ctest --rerun-failed --output-on-failure

# --- Release / standalone builds — see prior NEXT.md revisions for full commands

# --- Validate an XML fixture (also catches the "--" inside comment bug)
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# --- Run / export / validate / version
./cmake-build-debug/MeshCraft test/house.mc3.xml
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
ctest -R mc3_roundtrip --output-on-failure
ctest -R mcb_roundtrip --output-on-failure
ctest -R "mc3togltf_csg" --output-on-failure    # all 7 CSG export tests (S5)
ctest -R csg_cache --output-on-failure          # editor CSG preview-cache tests (S5)
ctest -R "mc3togltf_large_scene|mc3togltf_blender|mc3togltf_large_obj" --output-on-failure  # S6
ctest -R mc3_commands --output-on-failure       # editor Alg-mirror unit tests (S7)

# --- Push
git push origin develop
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

Continue **S8 (UI Robustness)**, `plan.md` priority order — 40 rows, 17
done, 23 open (all 📋, none pre-marked 🧪). Given this effort's pattern
across S0-S7 (roughly a third of "open" rows turn out to already be
satisfied by existing tests, and several genuine bugs surface only once
you write a *real* empirical check instead of trusting a row's assumed
behavior), the approach that's worked well:

1. Read the full S8 section of `plan.md` first (rows STAB-0296-0335ish —
   check exact numbering) to understand what's being asked.
2. For each row, grep existing test files (`mc3_commands_test`,
   `mc3togltf` Python tests, root-level Python render tests) for whether
   it's already covered before writing anything new.
3. When something needs a genuinely new test and the underlying logic is
   ImGui/CNA-coupled, check whether the actual *mutation* logic can be
   extracted to a CNA-free `Alg` mirror (the established pattern) even if
   the triggering UI gesture itself can't be headlessly driven — this
   effort's `mc3_commands_test` additions (recent files, merge-scene ID
   collision, CSG cache hash) all followed this shape.
4. When a row's literal wording doesn't match the actual, correct
   behavior (found several times this session — STAB-0220/0226/0293),
   correct the row rather than force-testing something that isn't real.
5. Real, confirmed gaps that aren't worth fixing on the spot (scope too
   large for a single verification task) should be flagged 🟡 with a
   clear explanation and a test that proves the gap, not silently
   ignored or force-fixed speculatively — matches STAB-0092/0289.
6. Run the full `ctest` suite (63/63 baseline) after each change; commit
   + push after each task per the standing workflow.

**After S8 closes**, continue to **S9 (ModelRegistry Stability)** in
`plan.md` order (35 rows, 22 done, 13 open, all 📋).

---

## 9. Do not do yet

Unchanged from prior revisions (no new scene-format features, no CNA/
SHARP_RUNTIME changes, no `Mc3Document` public API changes without
checking dependents, no SVG rasterization or `embed:`/`<embeds>`
implementation without an explicit decision, no mass refactoring, no
MinGW GLES-header vendoring, no STAB-0642/0643/0650 without the missing
tool/access, no CSG UV-preservation implementation without an explicit
decision).

One addition: **no speculative fix for STAB-0289** (merge-scene
object-id collisions) without discussing scope first — the row is
flagged, test-proven, and documented; a correct fix is real, non-trivial
feature work (recursive subtree id-walking), not a quick patch.

---

## 10. Resume prompt

```
Read NEXT.md first. Then read the S8 section of plan.md (UI Robustness,
40 rows) before writing any new code — grep mc3_commands_test.cpp and the
mc3togltf/test/*.py files for whether each open row is already satisfied
first. This whole stabilization effort has consistently found that a
meaningful fraction of "open" rows are stale (already covered, never
marked done), and that writing a *real* empirical check (not just
presence/no-crash) sometimes surfaces genuine bugs a superficial test
would miss — this session alone found and fixed 3 real bugs this way
(MCB autoplay drop, a silently-broken nested-CSG test fixture, missing
clearCsgCache() calls) and documented one real gap left unfixed
(merge-scene object-id collisions, STAB-0289, flagged 🟡).

Make one small, verified improvement at a time. Run the relevant build/
test command from section 7 and confirm cmake-build-debug still passes
(63/63, or the new total if you registered a new ctest). Update plan.md's
row for the task you completed, and update NEXT.md's section 3/8 after
finishing a batch (not necessarily after every single row).

Current branch: develop, in sync with origin/develop at commit ee68b5b.
Build dir: cmake-build-debug/ (Debug, CLion cmake 4.2.2) — last full
rebuild + 63/63 ctest was clean at commit 4b82832, working tree clean as
of ee68b5b (docs-only commits since).

Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file / new add_test()
requires a reconfigure, not just a rebuild — see section 6. New
.mc3.xml fixture comments must never contain "--" anywhere inside them
(lxml/validate_xsd.py rejects it even though tinyxml2 doesn't).

The Emscripten blank-canvas issue (section 4) needs a human with a
browser to close the loop — if none is available, stay on the plain S8
backlog work in section 8 instead.

CI is parked deactivated under .github_/ (credentials issue). Commit
after each STAB-XXXX task and push to origin/develop — standing workflow
across this whole stabilization effort.
```
