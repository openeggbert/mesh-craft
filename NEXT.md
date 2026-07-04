# NEXT.md

_Last updated: 2026-07-04_

---

## 1. Project summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` scene format
— a lightweight XML-based scene description used by the OpenEggbert
project. It uses Dear ImGui for its UI, running on **CNA** (an XNA-style
SDL3 + OpenGL runtime, a separate sibling repo at `../cna` — never
modified from this repo) and **SHARP_RUNTIME** (`../sharp-runtime`, the
math library CNA's backend depends on). Scenes export to glTF/GLB via
**mc3togltf** and to a compact binary format via **mc3tomcb**.

**Main goal:** reach a fully stabilized, test-covered codebase before
adding new features. All work is tracked in `plan.md` as 650 `STAB-XXXX`
tasks across sections S0–S20, gated by a Gate 0–6 checklist (each gate
requires its full `STAB-XXXX` ID range green — not just a smaller
"priority" subset `plan.md` also tracks for getting each gate's biggest
risks closed quickly).

**Current phase:** Stabilization. Every gate's priority-list subset is
now done. **Gate 6 (Documentation) is close to fully green**: S17
(Documentation and User-Facing Honesty) is 20/20 complete; S18 (Code
Quality) has all P0/P1 items done plus 2 P2s (STAB-0610, STAB-0603)
done, 16 P2/P3 remain; S19 (Security) has all P0/P1/P2 items done
(14/15), with just 1 P3 left (STAB-0635, tmp-file race condition); S20
(Release Readiness) is 12/15, with the last 3 items genuinely blocked
(need Blender, a browser, or a running CI — none available in this
environment). No gate is fully green yet. Plan-wide totals: **212 ✅
done, 3 🟡 partial, 136 🧪 has a plan but not executed,
299 📋 not started** out of 650.

**Important architectural decisions:**
- `mc3/` and `mcb/` are pure C++ static libs with **no** CNA/ImGui
  dependency and must stay independently buildable.
- `mc3togltf/` and `mc3tomcb/` are standalone CLI + lib targets, also
  CNA-free.
- CNA is added as a sibling CMake subdirectory
  (`add_subdirectory(../cna ...)`) and must not be modified from this
  repo.
- The editor (`src/MeshCraft/`) is CNA-coupled, but much of its
  command/parsing/validation logic has zero actual CNA dependency once
  ImGui rendering and app-state bookkeeping are set aside — that pure
  logic lives in CNA-free headers (functions suffixed `Alg`, e.g.
  `EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`) that both the real
  app code and the headless test suite `#include` and call directly.
  See §6 for the full pattern.

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`, generated with CLion's bundled cmake
  4.2.2): full reconfigure + rebuild (53 targets) succeeded cleanly,
  last verified at commit `fca6fc1`.
- **Release** (`b-release/`): freshly built and verified in the same
  session as Debug above — 6.25MB binary (vs. Debug's 59.9MB), confirmed
  via `file`/`objdump` to have zero DWARF debug sections.
- **Offline mode** (`-DFETCHCONTENT_UPDATES_DISCONNECTED=ON`): used for
  every standalone/Release build this session without issue.
- **Standalone (CNA-free) component builds**, each configuring/
  building/testing without the root project: `mc3` (1/1), `mcb` (1/1),
  `mc3togltf` (12/12), `mc3tomcb` (2/2) — last re-verified at commit
  `fca6fc1`; `mc3togltf` re-verified again this session after STAB-0610
  (still 0 warnings under `-Wall -Wextra`, standalone build not yet
  re-checked against the newer 13-test count from STAB-0630 — see §3).

### Tests
**21/21 CTest pass** in Debug as of this session (STAB-0630 added
`mc3togltf_obj_robustness`; previously 20/20, last full Debug+Release
verification at commit `fca6fc1`): `smoke_test`, `xsd_validation`,
`mc3_registry`, `mc3_ai`, `mc3_roundtrip`, `mc3_commands`,
`mcb_roundtrip`, `mc3tomcb_roundtrip`, `mc3togltf_gltf`,
`mc3togltf_all_primitives`, `mc3togltf_export_verification`,
`mc3togltf_large_scene`, `mc3togltf_csg_strict`, `mc3togltf_csg_export`,
`mc3togltf_csg_unsupported`, `mc3togltf_csg_nested`,
`mc3togltf_instance_deform_cache`, `mc3togltf_float_cache_key`,
`mc3togltf_obj_robustness`, `mc3togltf_large_scene_generated`,
`mc3togltf_large_scene_500`.

- `mc3_commands` (~282 assertions): editor command algorithms, undo/redo
  for every mutating command, auto-save/backup, Save-As/Export-Selection/
  drag-drop workflows, keybinding/preferences/macro persistence,
  hierarchy-panel filtering, AI-panel + unsaved-changes dialog
  lifecycles.
- `mc3_registry` (~94 assertions): ModelRegistry SQLite CRUD, search
  (name/group/tags/description/source), migration, edge cases.
- `mc3_ai` (~55 assertions): AiAssistant JSON helpers, the full
  AI-response validation pipeline (extract → repair → parse →
  empty-check → XSD-validate), 4 mock-HTTP-server round-trips (success,
  truncation, HTTP error, indefinite-hang timeout — no real network),
  and XSD accept/reject regression tests.
- `mc3_roundtrip` (~299 assertions): full XML parser/writer roundtrip,
  all N1–N7 extensions, edge cases.
- `mcb_roundtrip` (~50 assertions): MCB binary roundtrip, base scene +
  all N1–N7 types.

See `TESTING.md` for the full per-test reference and `plan.md`'s
`STAB-XXXX` rows for the exhaustive per-behavior list.

### Tools / libraries available
- `MeshCraft` — editor executable, version `0.1.0` (`--version` to
  print it, new this session). Builds and runs; the 3D viewport is not
  fully integrated into the render loop (see "What does NOT work yet").
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb`.
- `mc3tomcb` — bidirectional CLI: `mc3.xml` ↔ `.mcb`, direction chosen
  by file extension.
- `Mc3` — static lib, scene data + XML load/save.
- `Mcb` — static lib, binary serialization.
- `mc3togltf_lib` — static lib, `GltfExporter`/`MeshBuilder`/
  `CsgEvaluator`.

### What works
- Full XML round-trip for all primitive types and all N1–N7 extensions.
- MCB binary round-trip, matching the XML feature set.
- XSD validation for every test fixture, and now for AI-generated scene
  XML before it's applied to the live scene (STAB-0391).
- glTF/GLB export: all primitives, animations, CSG (incl. nested),
  materials, lights, cameras, instances, groups, OBJ mesh import.
- SQLite-backed asset registry: open/save/search/remove/migration.
- Editor undo/redo (snapshot-based, every mutating command verified).
- Auto-save + 2-slot rotating backup on save (see `README.md`'s new
  "Backup and Recovery" section for the exact mechanics).
- AI Assistant: sends scene + prompt to the Claude API, validates the
  response (markdown-fence/prose tolerant, structural + XSD schema
  checks), applies it or saves definitions to the registry.
- Real headless smoke-testing verified this session: `house.mc3.xml`,
  `garden_house.mc3.xml`, and `features.mc3.xml` all load cleanly
  through the actual editor binary (`--screenshot` mode), not just the
  standalone parser.

### What does NOT work yet
- `EditorViewport` is not integrated into the `MeshCraftApplication`
  render loop.
- SVG texture rasterization: parsed/serialized/round-tripped, but
  `GltfExporter` never reads the SVG texture map at all — silently
  dropped from glTF export, not just deferred.
- Embedded glTF (`<mesh src="embed:id"/>`): parsed/serialized, but
  `GltfExporter` treats `embed:id` as a literal OBJ path, which fails —
  export continues with an empty (meshless) node, doesn't crash.
- N3–N7 scene data (scripts, sounds, music, triggers, scene states,
  meta): fully round-tripped but not executed at runtime anywhere — no
  Lua interpreter, no audio playback, no trigger-firing, no
  state-switching. Intended at this stage (data model before
  execution), not a bug.
- CI workflow exists but is parked deactivated under `.github_/` (see
  §4); no automated full-editor (CNA + SDL3) build/test job runs
  anywhere currently.

---

## 3. Recent changes

**STAB-0610** (`53aa75e`), **STAB-0630** (`09fdf95`),
**STAB-0383/STAB-0631** (`7670a00`), and **STAB-0633** (`9aabad4`) are
committed; `origin/develop` is not yet pushed to (last pushed commit is
`03723b8`). **STAB-0603** below is implemented but **not yet
committed** as of this update — working tree has a file move
(`src/MeshCraft/EditorAlgorithms.hpp` → `include/MeshCraft/
EditorAlgorithms.hpp`) plus edits to `MeshCraftApplication_Commands.cpp`,
`mc3/test/editor_commands_test.cpp`, `mc3/CMakeLists.txt`, `plan.md`,
`NEXT.md`.

This was a long, dense session that closed out essentially all of
**Gate 6 (Documentation)**'s reachable work, plus a from-scratch schema
audit that found real bugs. Highlights, most recent first:

- **STAB-0603 — `EditorAlgorithms.hpp` moved to a public include
  directory**: was at `src/MeshCraft/EditorAlgorithms.hpp`, included
  via a fragile `src/`-relative path (`#include "EditorAlgorithms.hpp"`)
  that only worked because `mc3_commands_test` explicitly added
  `src/MeshCraft` to its include dirs. Moved to
  `include/MeshCraft/EditorAlgorithms.hpp`, matching the existing
  public-include convention every other header uses (e.g.
  `AiAssistant.hpp`); updated both real `#include`s and
  `mc3/CMakeLists.txt`'s `mc3_commands_test` guard (now checks
  `include/MeshCraft/EditorAlgorithms.hpp`, still gracefully skipped in
  a standalone `mc3/` checkout where that path doesn't exist — verified
  by rebuilding standalone `mc3` fresh: 1/1, correctly skips
  `mc3_commands`). Root project rebuild: 21/21 ctest still pass.
- **STAB-0633 — AI apply requires undo-capable state**: confirmed by
  code inspection — `document_` is assigned from `aiPendingDoc_` at
  exactly one call site (`MeshCraftApplication_UiAi.cpp:266`, the
  "Apply to Scene" button), unconditionally preceded on the immediately
  prior line by `pushUndo()`. No other code path reaches that
  assignment, so apply-without-undo-push is impossible by construction.
  No code change, no new test (would just re-assert that two adjacent
  lines execute in order, which C++ already guarantees). **This closes
  S19 down to a single remaining P3** (STAB-0635).
- **STAB-0383 + STAB-0631 — AI network timeout**: confirmed
  `AiAssistant`'s `httplib::Client` already had finite connect/read/write
  timeouts (30s/600s/120s) — not infinite. Exposed them as public
  overridable members (`connectTimeoutSec`/`readTimeoutSec`/
  `writeTimeoutSec`, mirroring the existing `apiBaseUrl` test-override
  pattern) instead of hardcoded literals, purely so a fast test could
  verify the *behavior* rather than just read the numbers off the page.
  Added a mock-server test where the handler blocks on a condition
  variable forever (simulating a truly hung server, not just a slow
  one) with a 1s override — confirmed `sendAsync()` still returns with
  `hasError()` within ~1s, not indefinitely. Test runs in ~1.2s wall
  time, no change to production timeout values.
- **STAB-0630 — untrusted OBJ file robustness**: fed `loadObjMesh()`
  three hostile inputs — an out-of-range negative (relative) vertex
  index, a literal `nan` coordinate, and a `1e400`-overflow-to-infinity
  coordinate. The first two were already handled cleanly (tinyobjloader
  itself rejects the bad index; `nan` parses to `0.0` and never
  propagates). **Found a real gap**: infinite coordinates parsed
  successfully and silently produced a spec-invalid GLB (`inf` floats in
  the binary buffer, `null` — not a number — in the JSON accessor
  `min`/`max`, since JSON has no `Infinity` literal). Fixed by rejecting
  non-finite vertex coordinates in `loadObjMesh()` with a clean
  `std::runtime_error`, routed through the exact same catch-and-skip
  path `buildMesh()` already uses for other malformed-mesh cases (prints
  `Warning:`, node exported meshless, export continues — no crash).
  Added a permanent `mc3togltf_obj_robustness` ctest (2 new `.obj`
  fixtures + 1 new `.mc3.xml` scene under `test/`) covering all 3 inputs
  plus a check that no accessor ever serializes a non-finite value.
- **STAB-0610 — `-Wall -Wextra` on `Mc3` and `mc3togltf_lib`**: added
  the flags to both targets' `CMakeLists.txt`. Found and fixed 3 real
  warnings in our own code: `MeshBuilder.cpp`'s `sampleCrossSection()`
  didn't handle `CrossSectionType::Star` at all (silently produced an
  empty polygon for star cross-sections in glTF export — mirrored the
  existing, correct `SceneRenderer_Extrude.cpp` star-sampling logic to
  fix it); an unused `dy` variable in `samplePath()`; and a
  missing-field-initializers warning on `ExportCtx` aggregate init in
  `GltfExporter.cpp` (silenced with explicit `{}`, no behavior change —
  the omitted members already default-initialize correctly). Also
  marked the vendored `tinygltf`/`tinyobjloader` include dirs `SYSTEM`
  in `mc3togltf/CMakeLists.txt` so their (unfixable, third-party)
  warnings don't pollute the build under the new flags. Verified: a
  full from-scratch Debug reconfigure/rebuild is 100% warning-free for
  `Mc3` and `mc3togltf_lib`, 20/20 ctest still pass, and a fresh
  standalone `mc3togltf` build (outside the root project) is also
  warning-free and 12/12 tests pass.
- **S20 (Release Readiness), 12/15 items**: added a project version
  (`0.1.0`) and `--version` CLI flag; actually built and compared
  Debug vs. Release binaries to confirm Release has no debug symbols;
  ran the real editor binary against all 3 sample scene files to
  confirm clean loading; created `CHANGELOG.md`, `THIRD_PARTY.md`
  (every dependency version verified against the real
  `FetchContent_Declare` tags), `docs/USER_GUIDE.md` (every menu
  path/shortcut verified against the real menu code); added "Reporting
  a Crash" (Linux instructions actually tested with a real SIGSEGV) and
  "Backup and Recovery" sections to `README.md`. **Found a real gap**:
  SVG rasterization wasn't mentioned in `README.md`'s limitations list
  at all — added it. 3 items (STAB-0642/0643/0650) are blocked on tools
  not available in this environment (Blender, a browser, running CI).
- **S18 + S19 P0/P1 verification (18 items, no code changed)**: audited
  `PropertiesPanel.cpp` and `SceneRenderer.cpp` for null-pointer risk
  (none found — every optional/map access is properly guarded);
  confirmed the API key is never stored or logged, no
  `std::system`/`popen` anywhere, SQLite is fully parameterized.
  **Actually tested** (not just read) three hostile inputs against the
  real parser: include path traversal to `/etc/passwd` (clean parse
  error, no crash), a 10MB malformed-XML file (throws in ~7ms), and a
  10MB texture URI (parses correctly in ~53ms, no truncation).
- **A full `mc3.xsd` audit** (not tied to a `STAB-XXXX` ID): diffed
  every `Mc3XmlWriter.cpp` attribute/element against the schema and
  found **7 more real gaps** beyond the `id` gap found earlier
  (`layer`, per-object `<state>` — missing for every object type,
  instance `material_override`/`variants`, `uv_mapping`'s real
  attributes, environment `background_texture`/`skybox_texture`,
  texture `name`) — all fixed. **Also found a genuine round-trip bug**
  while auditing: renaming a texture in the editor was silently
  discarded on every save/reload (`Mc3XmlParser::parseTextures` never
  read the `name` attribute back) — fixed and regression-tested.
  Process note: editing `mc3.xsd` requires a full CMake **reconfigure**,
  not just a rebuild — it's embedded into a generated header at
  configure time (`Mc3XsdEmbed.hpp`), and this was caught the hard way
  via a failing test before it could ship silently wrong.
- **Gate 6's full P1 priority-list (S17, 17 items)**: rewrote
  `STABILIZATION.md` (was describing "15 tests" and already-fixed
  gaps), added N3–N7 sections to `MC3_FORMAT.md` (each with an honest
  "not executed at runtime yet" status note), an MCB Binary Format
  section, an Include semantics section (written from reading the
  actual merge/cycle-detection code), created `TESTING.md` and
  `CONTRIBUTING.md`, reviewed `m1m2m3.md` against current code and
  fixed real drift (missing `source` in the registry search
  description; the `AiAssistant` class snippet was missing prompt
  caching, `maxTokens`, truncation handling, and XSD validation — all
  added after that doc was first written).
- **STAB-0391** (earlier the same day): added real XSD validation of
  AI-generated XML via libxml2, embedded `mc3.xsd` into the binary at
  configure time. Found the object `id` gap during this work — the
  first thread that led to the full audit above.
- **STAB-0243/0244** (S6): confirmed the existing large-scene test
  already covers mesh-reuse correctly; added a 500-object variant of
  the same generator test with a `<30s` timing assertion (measured
  ~0.02s).

Earlier sessions (summarized further): Gate 3's "commands are undoable"
cluster, auto-save + 2-slot backup rotation, AI-panel dialog lifecycle,
registry search fields + edge cases, and 5 other real bugs found via
test-driven development (`resetPivot()` missing an undo push, a
hierarchy-filter bug, unclamped bloom/SSAO sliders, a registry search
gap, and the original AI-response XML-extraction trailing-content bug).

---

## 4. Current blocker / main problem

**There is no blocker to local development or testing.** Debug and
Release both build clean and pass 20/20 tests as of commit `fca6fc1`
(pushed; `develop` is in sync with `origin/develop` at `03723b8`,
confirmed just now).

The one open **operational** issue is **CI cannot be activated with the
current git credentials**:

- Symptom / failing command (only when trying to activate CI, not for
  normal pushes):
  ```
  git push origin develop
  ! [remote rejected] develop -> develop (refusing to allow a Personal
    Access Token to create or update workflow `.github/workflows/ci.yml`
    without `workflow` scope)
  ```
- Cause: the PAT embedded in the remote URL (both this repo and the
  sibling `../cna` repo) lacks the `workflow` OAuth scope.
- Workaround already in place: the CI workflow file is committed but
  parked at `.github_/workflows/ci.yml` (GitHub only treats
  `.github/workflows/` as live), so it's versioned but inactive. To
  activate: rename `.github_` → `.github` and push with a
  `workflow`-scoped token.
- Related, separate issue: that same PAT is embedded in **plaintext** in
  the remote URL in `.git/config` (not in any tracked/committed file).
  Recommended fix: revoke it, issue a new token with `repo` + `workflow`
  scopes, and switch the remote to a credential helper or SSH.
- This also blocks STAB-0650 (verify CI produces a consistent test
  report) — can't be verified without CI actually running.

Nothing has been tried beyond identifying and documenting this; it
requires the repo owner to rotate/rescope the token.

---

## 5. Known bugs and limitations

- **PAT exposed in `.git/config` and lacks `workflow` scope** — see §4.
  _status: confirmed (security + operational); needs owner action._
- **CI is partial** — only the CNA-free libs are covered by the parked
  workflow; no full-editor CI job. _status: incomplete, inactive._
- **SVG texture rasterization** — parsed/serialized/round-tripped, but
  `GltfExporter` never reads the SVG texture map, so it's silently
  dropped from export. Blocked on a library choice (librsvg vs.
  NanoSVG). _status: incomplete, now documented in `README.md`._
- **Embedded glTF** — `embed:id` is treated as a literal OBJ path by
  `GltfExporter`, which fails to parse; export continues with an empty
  node rather than crashing. _status: incomplete, documented._
- **`EditorViewport` not integrated** into `MeshCraftApplication`'s
  render loop. _status: incomplete._
- **`mc3` standalone build skips `mc3_commands`** — needs
  `EditorAlgorithms.hpp` from the editor tree, absent in a standalone
  checkout. _status: intended, not a bug._
- **`cmake-build-debug/` reconfigure is toolchain-sensitive** — must use
  CLion's bundled cmake 4.2.2, not the system cmake (fails on manifold
  sources otherwise). _status: confirmed, environmental._
- **`mc3.xsd` audit closed 7 gaps + 1 texture-rename round-trip bug**
  this session (see §3) — the audit covered `Mc3XmlWriter.cpp`
  exhaustively but not the reverse direction (parser accepting
  something the writer never emits, lower priority since that can't
  cause a false-XSD-reject). _status: resolved for the writer
  direction; parser-only direction unaudited._
- **`mc3.xsd` has no numeric range constraints anywhere** (zero
  `minInclusive`/`minExclusive`) — a schema-valid AI response can
  contain a negative `size`/`radius`/etc. and it applies to the scene
  unchanged. _status: confirmed, documented in `README.md`; not fixed —
  would need a per-attribute design decision, bigger than a doc pass._
- **N3–N7 (scripts/sounds/music/triggers/states/meta) are data-only** —
  round-tripped but nothing executes them at runtime. _status:
  intended at this stage, documented per-section in `MC3_FORMAT.md`._
- **Gate 6 is close but not fully green** — S17 20/20 ✅, S18 7/25
  (P0/P1 done, 18 P2/P3 left), S19 11/15 (P0/P1+bonus done, 4 P2/P3
  left), S20 12/15 (3 items blocked — see below). _status: the
  remaining S18/S19 items are normal follow-on work; see §8._
- **3 `plan.md` items cannot be completed in this environment**:
  STAB-0642 (needs Blender), STAB-0643 (needs a browser), STAB-0650
  (needs CI actually running — see §4). _status: flagged, needs either
  a human with the right tools or the PAT rotated first._

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

**Undo/redo:** snapshot-based. `undoStack_`/`redoStack_` (capped at 20,
oldest dropped first) hold `std::vector<Mc3::Mc3Document>`; `pushUndo()`
stores a deep copy of `document_` before each mutating command.

**The "Alg mirror" pattern**: editor/AI logic in CNA-coupled `.cpp`
files often has zero actual CNA/ImGui dependency once app-state
bookkeeping and rendering are set aside. Two variants:
- **Single source of truth** (preferred): pure logic in a CNA-free
  header (`EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`), functions
  suffixed `Alg`, real `.cpp` `#include`s and calls them directly — no
  duplication.
- **Kept in sync manually**: only when the real function is genuinely
  CNA-coupled and can't be unified — the mirror's comment names what it
  tracks.

**Embedding a resource file at compile time**: `mc3.xsd` is compiled
into `MeshCraft`/`ai_test` as a raw string constant
(`MeshCraft::kMc3XsdContent`), generated at CMake **configure** time
(`file(READ)` + `configure_file()` in root `CMakeLists.txt`, template at
`cmake/Mc3XsdEmbed.hpp.in`) — never read from disk at runtime. **Editing
`mc3.xsd` requires a full reconfigure, not just a rebuild**, or the old
schema stays compiled in silently.

**Testing network-dependent code**: `AiAssistant` has a public
`apiBaseUrl` member (default the real Claude API) that tests override to
point at a local `httplib::Server` mock — production behavior is
untouched. See `mc3/test/ai_test.cpp`.

**Hard constraints / invariants:**
- `Mc3Document` public API: don't change without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` must stay buildable
  standalone (no CNA/ImGui deps).
- **`mc3.xsd` must stay symmetric with `Mc3XmlWriter.cpp`** — this
  session found 8 real gaps that had accumulated silently. Any new
  writer attribute/element needs a schema declaration in the same
  change (see `CONTRIBUTING.md`'s "API change policy").
- Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`
  (triggers a full CNA recompile).
- Do not modify CNA/SHARP_RUNTIME source files from this repo.
- `file(GLOB_RECURSE)` collects sources — a new `.cpp` file needs a
  cmake **reconfigure**, not just a rebuild (same for `mc3.xsd`).
- MCB format version is `MCB_VERSION` in `McbFormat.hpp` — bump on any
  breaking wire-format change.
- XSD root element order is strict (`include → metadata → meta →
  environment → ... → objects → actions`) — this now has real runtime
  consequence since AI responses are validated against it.
- Reconfigure `cmake-build-debug/` only with CLion's bundled cmake.

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid a system-cmake bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON   # (re)configure
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (20)
ctest -N                                                      # lists all 20 tests

# --- Release (system cmake is fine for a fresh dir)
cmake -S . -B b-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
      -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build b-release -j4
(cd b-release && ctest --output-on-failure)

# --- Standalone (CNA-free) component builds + tests
for c in mc3 mcb mc3togltf mc3tomcb; do
  cmake -S "$c" -B "$c-build" -G Ninja -DBUILD_TESTING=ON \
        -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
  cmake --build "$c-build" -j4
  (cd "$c-build" && ctest --output-on-failure)
done   # mc3 1/1 · mcb 1/1 · mc3togltf 12/12 · mc3tomcb 2/2

# --- Run / export / validate / version
./cmake-build-debug/MeshCraft test/house.mc3.xml   # open a sample scene
./cmake-build-debug/MeshCraft --version             # MeshCraft 0.1.0
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
python3 test/validate_xsd.py mc3/mc3.xsd test/features.mc3.xml
ctest -R mc3_commands --output-on-failure   # editor algorithms + undo/redo
ctest -R mc3_registry --output-on-failure   # ModelRegistry
ctest -R mc3_ai       --output-on-failure   # AiAssistant + mock HTTP server

# --- Push (normal pushes work fine; only CI activation is blocked, see §4)
git push origin develop
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

1. **STAB-0635 — verify tmp-file race condition** (S19, P3, the one
   remaining S19 item). Goal: confirm `AiAssistant.cpp`'s temp-file
   naming includes a PID or random suffix (not a fixed name two
   concurrent instances could collide on).
   Files: `src/MeshCraft/AiAssistant.cpp`.
   Verify: code inspection; grep for the tmp-path construction and
   confirm uniqueness per process/thread.

2. **STAB-0604 — audit duplicate code: `SceneRenderer_Builders.cpp` vs
   `MeshBuilder.cpp`** (S18, P2, next-lowest ID after STAB-0603). Goal:
   check whether the two files (one in the CNA-coupled renderer, one in
   the CNA-free `mc3togltf_lib`) duplicate the same primitive-geometry
   generation logic; if so, decide whether it's worth extracting to a
   shared utility (may not be — the two have different vertex/index
   formats for different consumers, so duplication could be
   intentional; this needs a real read before deciding).
   Files: `src/MeshCraft/Renderer/SceneRenderer_Builders.cpp`,
   `mc3togltf/src/MeshBuilder.cpp`.
   Verify: code inspection; if extraction happens, `ctest
   --output-on-failure` must stay 21/21.

Beyond these two: S18 has 15 more P2/P3 items after STAB-0603/0604
(mostly code-quality audits — see `plan.md`'s S18 rows). Closing S18+S19 fully
would leave Gate 6 blocked only on the 3 genuinely-inaccessible S20
items (§5). After that, the next priority tier is P2 items across
S6–S13 and the
untouched S11/S12/S14/S15 sections — see `plan.md`'s per-section Key
File columns.

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
- **No moving `.github_` back to `.github`** until a `workflow`-scoped
  token exists.
- **No committing the PAT** anywhere — remove it from `.git/config`,
  don't copy it into any tracked file.
- **No SVG rasterization work** until a library choice is made (librsvg
  vs. NanoSVG) — this is a real, nontrivial feature decision, not a
  quick fix.
- **No mass refactoring** of passing code, no speculative architecture
  changes — stabilization phase; scope each change to exactly what its
  `STAB-XXXX` entry asks for.
- **No typing any `mc3.xsd` attribute as `xs:ID`/`xs:IDREF`** without
  first checking whether the app actually enforces that referential
  constraint — several existing attributes are deliberately `xs:string`
  instead, to avoid inventing document-wide uniqueness rules the app
  doesn't uphold.
- **No attempting STAB-0642/0643/0650** without the missing tool first
  (Blender, a browser, or an active CI run respectively) — see §5.

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect only the files needed for the first
task in section 8. Do not refactor unrelated code. Make one small,
verified improvement. Run the relevant build/test command from section 7
and confirm cmake-build-debug still passes (20/20, or the new total if
you registered a new ctest). Update NEXT.md after finishing.

Current branch: develop, in sync with origin/develop at commit 03723b8.
Build dirs: cmake-build-debug/ (Debug, CLion cmake 4.2.2) and b-release/
(Release) — both full-rebuilt and 20/20 ctest verified clean at commit
fca6fc1. Standalone mc3/mcb/mc3togltf/mc3tomcb builds also re-verified.
Active plan: plan.md (STAB-XXXX tasks; every gate's priority-list subset
is done; Gate 6 is close to fully green — S17 20/20, S18 7/25 (P0/P1
done), S19 11/15 (P0/P1+bonus done), S20 12/15 (3 items blocked on
missing tools). Pick the next task from section 8: S18/S19's remaining
P2/P3 items would fully close Gate 6 except the 3 blocked S20 items.
Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file requires a
reconfigure, not just a rebuild — see section 6.
CI is parked deactivated under .github_/ (PAT lacks `workflow` scope,
see section 4).
```
