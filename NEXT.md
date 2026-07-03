# NEXT.md

_Last updated: 2026-07-03_

---

## 1. Project summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` scene format.
It uses Dear ImGui for its UI, SDL3 + OpenGL via the **CNA** runtime (a
separate sibling repo at `../cna` — do NOT modify from this repo), and
exports scenes to glTF/GLB via **mc3togltf**.

**Main goal:** reach a fully stabilized, test-covered codebase before
adding new features. All work is tracked in `plan.md` as `STAB-XXXX`
tasks across sections S0–S20, gated by a Gate 0–6 checklist (each gate
requires a specific `STAB-XXXX` ID range to be fully green).

**Current phase:** Stabilization. Gates 0–2 (Build, Format, Export
priority items) are complete. **Gate 3 (Editor safety)'s P1 items are
done** across S7 (Editor Save/Load), S8 (UI Robustness), and S13
(Commands/Undo/Redo) — the gate itself isn't fully green yet because it
requires the *entire* `STAB-0261–0335` range, and P2/P3 rows in that
range remain. **Gate 4 (Registry/AI) is done**, including S9
(ModelRegistry) and the AI mock-test cluster plus its 3 remaining P0
verification items in S10. Gates 5–6 are untouched. Plan-wide totals
(out of 650 `STAB-XXXX` rows): **157 ✅ done, 13 🟡 partial, 143 🧪 has a
plan but not executed, 337 📋 not started.**

**Important architectural decisions:**
- `mc3/` and `mcb/` are pure C++ static libs with **no** CNA/ImGui
  dependency and must stay independently buildable.
- `mc3togltf/` and `mc3tomcb/` are standalone CLI + lib targets, also
  CNA-free.
- CNA is added as a sibling CMake subdirectory
  (`add_subdirectory(../cna ...)`) and must not be modified from this repo.
- The editor (`src/MeshCraft/`) is CNA-coupled, but many of its command/
  parsing functions turn out to have zero actual CNA dependency once
  ImGui rendering and app-state bookkeeping are set aside. Where that's
  true, the pure logic lives in a dedicated CNA-free header (suffixed
  `Alg` for individual functions) that both the real app code and the
  test suite include and call — see §6.

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`, generated with CLion's bundled cmake
  4.2.2): a full from-scratch rebuild (53 targets) succeeded cleanly,
  last verified 2026-07-02 at commit `f37c541`.
- **Release** (`b-release/`): last verified 2026-07-01. Not re-verified
  since — several commits have landed since then; re-verify before
  relying on it.
- **Offline mode** (`-DFETCHCONTENT_UPDATES_DISCONNECTED=ON`): working as
  of 2026-07-01, not re-verified since.
- **Standalone (CNA-free) component builds**, each configuring/building/
  testing without the root project: `mc3`, `mcb`, `mc3togltf`, `mc3tomcb`.
  Last verified 2026-06-30.

### Tests
**19/19 CTest pass** (last full run 2026-07-02, commit `f37c541`):
`smoke_test`, `xsd_validation`, `mc3_registry`, `mc3_ai`, `mc3_roundtrip`,
`mc3_commands`, `mcb_roundtrip`, `mc3tomcb_roundtrip`, `mc3togltf_gltf`,
`mc3togltf_all_primitives`, `mc3togltf_export_verification`,
`mc3togltf_large_scene`, `mc3togltf_csg_strict`, `mc3togltf_csg_export`,
`mc3togltf_csg_unsupported`, `mc3togltf_csg_nested`,
`mc3togltf_instance_deform_cache`, `mc3togltf_float_cache_key`,
`mc3togltf_large_scene_generated`.

- `mc3_commands` (~250 assertions): editor command algorithms (rename/
  find-replace/array-dup/duplicate/group/ungroup), undo/redo round-trips
  for every mutating command, auto-save/backup, Save-As/Export-Selection/
  drag-drop/invalid-file-load workflows, keybinding/preferences/macro
  persistence formats, hierarchy-panel filtering, material-color
  resolution, undo-stack depth capping, AI-panel + unsaved-changes-
  confirmation dialog lifecycles.
- `mc3_registry`: open/save/search/remove/migration, registry-unavailable
  and search-field edge cases, schema introspection.
- `mc3_ai` (new this session, 39 assertions): `AiAssistant`'s JSON
  helpers, the AI-response validation pipeline (extract/repair/parse/
  empty-check), and three end-to-end mock-HTTP-server round-trips
  (success, truncation, HTTP error) — no real network call.

See `plan.md`'s `STAB-XXXX` rows for the exhaustive per-behavior list;
this summary intentionally stays high-level.

### Tools / libraries available
- `MeshCraft` — editor executable. Builds and runs; the 3D viewport is
  not fully integrated into the render loop (see "What does NOT work
  yet").
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb`.
- `mc3tomcb` — bidirectional CLI: `mc3.xml` ↔ `.mcb`, direction chosen by
  file extension.
- `Mc3` — static lib, scene data + XML load/save.
- `Mcb` — static lib, binary serialization (`McbWriter`/`McbReader`).
- `mc3togltf_lib` — static lib, `GltfExporter`/`MeshBuilder`/`CsgEvaluator`.

### What works
- Full XML round-trip for all 10 primitive types, including edge cases.
- MCB binary round-trip for the base scene and all N1–N7 extension types.
- MCB CLI round-trip (`mc3.xml → mcb → mc3.xml`), deterministic and
  lossless.
- XSD validation for all test fixtures, including N3–N7.
- glTF/GLB export: all 12 primitives, animations, CSG (including nested),
  materials, lights, cameras, instances, groups, OBJ mesh import.
- SQLite-backed asset registry: open/save/search/remove/migration; search
  matches group/name/tags/description/source, case-insensitively.
- Editor undo/redo: snapshot-based, round-trips full document state for
  every mutating command (including a recently-fixed gap: `resetPivot()`
  now correctly pushes an undo entry).
- Auto-save (configurable interval, separate `.autosave` file) and backup
  rotation (fixed 2-slot ring buffer) on save.
- AI Assistant: sends scene + prompt to the Claude API, extracts/repairs/
  validates the returned XML (including responses wrapped in markdown
  fences or trailing prose — this was a real bug, fixed this session),
  applies it to the scene or saves definitions to the registry.

### What does NOT work yet
- `EditorViewport` is not integrated into the `MeshCraftApplication`
  render loop.
- SVG texture rasterization is parsed/serialized but not rasterized
  (stub only) — blocked on a library choice (librsvg vs. NanoSVG).
- Embedded glTF is parsed/serialized but not resolved/inlined by
  `GltfExporter`.
- CI workflow exists but is parked deactivated under `.github_/` (see
  §4); there is no automated full-editor (CNA + SDL3) build/test job
  running anywhere.

---

## 3. Recent changes

All changes are committed on `develop`; see §10 for the exact commit the
push was last confirmed in sync at.

**STAB-0377/0378/0379** (S10, P0 — verification-only, no new code):
confirmed all three were already satisfied by existing code, so each was
simply marked ✅ in `plan.md` with a citation, no new test/code added:
- STAB-0377: `testUndoRedoAiApply()` (`mc3/test/editor_commands_test.cpp:974`)
  already covers "Apply to Scene" pushing an undo entry; verified it
  matches the real button code exactly (`MeshCraftApplication_UiAi.cpp:264-266`,
  `pushUndo(); document_ = *aiPendingDoc_;`).
- STAB-0378: `grep` over `AiAssistant.cpp` confirms the API key is only
  ever used to build the `x-api-key` HTTP header — never written to
  `std::cout`/`std::cerr`/`printf`/etc.
- STAB-0379: `MeshCraftApplication_UiAi.cpp:138-143` already pre-fills
  the key input buffer from `std::getenv("ANTHROPIC_API_KEY")` whenever
  the buffer is empty.

Across this session, Gate 3's P1 items (S7/S8/S13) and all of Gate 4
(S9 + S10) were closed out, cluster by cluster, each verified with a
full Debug rebuild + `ctest`. **5 real bugs were found and fixed** along
the way (not just test-coverage additions):

1. **`resetPivot()` never pushed an undo entry** — it mutated selected
   objects' transform and set `modified_ = true`, but didn't call
   `pushUndo()`. Fixed in `MeshCraftApplication_Commands.cpp`.
2. **`SceneHierarchyPanel.cpp`'s hierarchy filter did nothing when only a
   type/layer/tag/material filter was set (no search text)** — the skip
   decision was gated on the text-search flag alone instead of "any
   filter active". Fixed in `drawHierarchy()`.
3. **Bloom/SSAO strength (and SSAO radius) sliders had no
   `ImGuiSliderFlags_AlwaysClamp`** — Ctrl+Click-to-type could set them
   to out-of-range values, including negative, with nothing else
   guarding the shader uniforms. Fixed in `MeshCraftApplication_UiMenuBar.cpp`.
4. **`ModelRegistry::search()`'s SQL omitted `source` from its `WHERE`
   clause** — searching by source silently found nothing. Fixed, plus
   the search hint text was updated.
5. **AI responses wrapped in a markdown code fence or followed by
   trailing prose failed to parse** — `extractXml()` only located the
   *start* of the XML and returned everything to the end of the string;
   tinyxml2 does not tolerate content after the root element closes.
   Fixed by also trimming at the matching `</mc3>`.

Also this session: extended test coverage across S7/S8/S9/S10/S13
(dozens of `STAB-XXXX` items — see `plan.md` for the full list), added
CNA-free "Alg mirror" functions for previously-untestable logic
(hierarchy filtering, material-color resolution, undo-stack depth
capping, keybinding/prefs/macro persistence formats, AI-response
validation), added a test seam (`AiAssistant::apiBaseUrl`) enabling a
real mock-HTTP-server test without touching production behavior, and
created new files `src/MeshCraft/AiResponseAlgorithms.hpp` and
`mc3/test/ai_test.cpp`.

Also found and corrected two stale/outdated docs: `AI_TRUNCATION_BUG.md`
describes a bug (hardcoded `max_tokens=8192`, no `stop_reason` check)
that isn't present in the current code — it was fixed in an earlier
session but the doc was never updated. `plan.md`'s per-section
task-count summary table had also drifted from actual row state over
several prior sessions; it was recomputed directly from the row markers
and is now a derived table, not hand-maintained.

Earlier sessions (summarized): the original Gate 3 "commands are
undoable" cluster, auto-save + 2-slot backup rotation, AI-panel dialog
lifecycle, dirty-flag/unsaved-changes-confirmation, registry search
fields + edge cases, plus 2 more real bugs (7 of 40 `std::strncpy` call
sites missing their null-terminator; the `ModelRegistry::search()` gap
listed above).

---

## 4. Current blocker / main problem

**There is no blocker to local development or testing.** Debug builds
clean (full rebuild, 53/53 targets) and 19/19 tests pass as of the last
verified state (`f37c541`, 2026-07-02). `develop` is pushed and in sync
with `origin/develop`.

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
- Cause: the PAT in `.git/config`'s remote URL (both this repo and the
  sibling `../cna` repo) lacks the `workflow` OAuth scope.
- Workaround already in place: the CI workflow file is committed but
  parked at `.github_/workflows/ci.yml` (GitHub only treats
  `.github/workflows/` as live workflows), so it's versioned but
  inactive. To activate: rename `.github_` → `.github` and push with a
  `workflow`-scoped token.
- Related, separate issue: that same PAT is embedded in plaintext in the
  remote URL in `.git/config` (not in any tracked/committed file — full
  history checked). Recommended fix: revoke it, issue a new token with
  `repo` + `workflow` scopes, and switch the remote to a credential
  helper or SSH instead of embedding the token in the URL.

Nothing has been tried yet to fix this beyond identifying it; it
requires the repo owner to rotate/rescope the token.

---

## 5. Known bugs and limitations

- **PAT exposed in `.git/config` and lacks `workflow` scope** — see §4.
  _status: confirmed (security + operational); needs owner action._
- **CI is partial** — only the CNA-free libs (`mc3`, `mcb`, `mc3togltf`,
  `mc3tomcb`) are covered by the parked workflow; there's no full-editor
  (CNA + SDL3) CI job. _status: incomplete, and currently inactive
  (see §4)._
- **SVG texture rasterization** — parsed/serialized only, not rasterized.
  Blocked on a library choice (librsvg vs. NanoSVG). _status: incomplete._
- **Embedded glTF** — parsed/serialized but not inlined by
  `GltfExporter`. _status: incomplete._
- **`EditorViewport` not integrated** into `MeshCraftApplication`'s
  render loop. _status: incomplete._
- **`mc3` standalone build skips `mc3_commands`** — that test needs
  `EditorAlgorithms.hpp` from the editor tree, which isn't present in a
  standalone `mc3/` checkout. _status: intended, not a bug._
- **`cmake-build-debug/` reconfigure is toolchain-sensitive** — it was
  generated with CLion's bundled cmake 4.2.2; reconfiguring with the
  system cmake (3.31.6) fails during Generate (bogus manifold/
  `boolean3.cpp` sources). _status: confirmed, environmental — always
  use CLion's cmake for this directory._
- **`AI_TRUNCATION_BUG.md` is stale** — describes `max_tokens` hardcoded
  at 8192 with no `stop_reason` check; neither is true of the current
  code (`maxTokens` defaults to 32000, UI-configurable up to 64000, and
  `wasTruncated()` is checked and surfaced). The underlying bug was
  fixed in an earlier session; the doc was never updated.
  _status: doc is stale, not the code — consider deleting or rewriting
  `AI_TRUNCATION_BUG.md`._
- **Gate 3 is not fully green** (P1 items are, but the gate needs the
  full `STAB-0261–0335` range) — S7 28/35, S8 17/40, S13 11/25; the
  remainder of each is P2/P3, untested. _status: needs verification,
  tracked task-by-task in `plan.md`._
- **S10 (AI) P0 items are now all closed** (STAB-0377/0378/0379
  verified this session, no code changes needed); 25 P1/P2/P3 items
  remain untouched. _status: P0 done, rest not started._

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

**Undo/redo:** snapshot-based. `undoStack_`/`redoStack_` (capped at
`kUndoMax = 20`, oldest entries dropped first) hold
`std::vector<Mc3::Mc3Document>`; `pushUndo()` stores a deep copy of
`document_` before each mutating command.

**The "Alg mirror" pattern**, used throughout this codebase's test
suite: editor/AI logic in CNA-coupled `.cpp` files (`MeshCraftApplication_
*.cpp`, `MeshCraftApplication_UiAi.cpp`) often turns out to have zero
actual CNA/ImGui dependency once app-state bookkeeping and rendering
calls are set aside. Two variants, both in use:
- **Single source of truth** (preferred): move the pure logic into a
  CNA-free header (`EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`),
  suffix functions `Alg`, and have the real `.cpp` `#include` and call
  them directly — no duplication. Examples: `duplicateObjectsAlg`,
  `groupObjectsAlg`, `extractXmlAlg`, `validateAndParseAiResponseAlg`.
- **Kept in sync manually**: only when the real function is genuinely
  CNA-coupled and can't be unified (e.g. `deepCopyDoc` vs. the mirror
  `deepCopyObjectAlg`) — the mirror's comment names the real function it
  tracks.

Where a class has no CNA dependency at all (`ModelRegistry`,
`Mc3Document`), tests use the real class directly — no mirror needed.
Where logic is genuinely inseparable from ImGui rendering, verification
is by code inspection only (documented as such in `plan.md`), since no
headless ImGui test harness exists.

**Testing network-dependent code**: `AiAssistant` has a public
`apiBaseUrl` member (default `https://api.anthropic.com`) that tests
override to point `sendAsync()` at a local `httplib::Server` mock —
`httplib::Client` auto-dispatches to SSL for `https://` or a plain
socket for `http://`, so this changes nothing about production
behavior. See `mc3/test/ai_test.cpp` for the pattern.

**Hard constraints / invariants:**
- `Mc3Document` public API: do not change without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` must stay buildable
  standalone (no CNA/ImGui deps). Keep editor-only test dependencies
  guarded (see `mc3/CMakeLists.txt`'s check for `EditorAlgorithms.hpp`'s
  presence).
- Do not register tests referencing the `mc3tomcb` target inside `mcb/`
  (would break the `mcb` standalone build) — `mc3tomcb_roundtrip` lives
  in `mc3tomcb/CMakeLists.txt`.
- Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`
  (triggers a full CNA recompile).
- Do not modify CNA source files from this repo.
- `file(GLOB_RECURSE)` is used for sources — adding a new `.cpp` file
  requires a cmake reconfigure (a new `.py` test file does not; new test
  executables registered explicitly in `CMakeLists.txt`, like
  `ai_test`, need a reconfigure too).
- MCB format version is `MCB_VERSION = 1` in `McbFormat.hpp` — bump on
  any breaking wire-format change.
- XSD root element order: `include → metadata → meta → environment →
  lights → cameras → textures → materials → embeds → scripts → sounds →
  music → triggers → states → definitions → objects → actions`.
- Reconfigure `cmake-build-debug/` only with CLion's bundled cmake 4.2.2.

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid the 3.31.6 bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON   # (re)configure
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (19)
ctest -N                                                      # lists all 19 tests

# --- Release (system cmake 3.31.6 is fine for a fresh dir)
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
done   # mc3 1/1 · mcb 1/1 · mc3togltf 11/11 · mc3tomcb 2/2

# --- Export a scene / validate XML / run a single test
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
python3 test/validate_xsd.py mc3/mc3.xsd test/features.mc3.xml
ctest -R mc3_commands --output-on-failure   # editor algorithms + undo/redo
ctest -R mc3_registry --output-on-failure   # ModelRegistry
ctest -R mc3_ai       --output-on-failure   # AiAssistant + mock HTTP server
ctest -R mc3tomcb_roundtrip --output-on-failure

# --- Push (normal pushes work fine; only CI activation is blocked, see §4)
git push origin develop
# To activate CI: rename .github_ -> .github and push with a workflow-scoped token
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

1. **Move to P2 items** in whichever section is most valuable next —
   with P0/P1 essentially exhausted across S7/S8/S9/S10/S13, the next
   tier by `plan.md`'s own priority scheme is P2 (then P3). Remaining
   P2/P3 counts: S7: 7, S8: 23, S9: 13, S10: 25, S13: 14, plus
   untouched sections S11 (Materials, 30), S12 (Animation, 30), S14
   (Rendering, 30), S15 (Import/export, 25).
   Goal: pick by ID order within the chosen section; for each item,
   check whether the relevant logic is CNA-free (an `Alg`-mirror
   candidate) or inspection-only before assuming a CNA/ImGui test
   harness is needed.
   Files: varies by section — see `plan.md`'s Key File column per row.
   Verify: the relevant `ctest -R <target>`, plus a full
   `ctest --output-on-failure` (expect 19/19 or higher).

2. **(optional) Rotate the PAT and activate CI** — see §4 for the exact
   steps.
   Goal: replace the plaintext, under-scoped PAT in `.git/config` with a
   `repo`+`workflow`-scoped token (or SSH), then rename `.github_` →
   `.github`.
   Files: `.git/config` (not tracked), `.github_/workflows/ci.yml` →
   `.github/workflows/ci.yml`.
   Verify: `git push origin develop` accepted for a
   `.github/workflows/` file, and the workflow runs in GitHub Actions.

---

## 9. Do not do yet

- **No new scene-format features** — N1–N7 are complete; further schema
  additions need design discussion first.
- **No CNA source changes** — a separate Claude Code instance owns the
  `../cna` repo.
- **No `Mc3Document` public API changes** without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- **No `${meta-gl_SOURCE_DIR}/include`** in any `CMakeLists.txt`.
- **No reconfiguring `cmake-build-debug/` with system cmake 3.31.6** —
  use CLion's cmake 4.2.2.
- **No moving `.github_` back to `.github`** until a `workflow`-scoped
  token exists (the push will be rejected).
- **No committing the PAT** anywhere — it must be removed from
  `.git/config`, not copied into any tracked file.
- **No SVG rasterization work** until a library choice is made (librsvg
  vs. NanoSVG).
- **No mass refactoring** of passing code, and no speculative
  architecture changes — this is a stabilization phase; scope each task
  to exactly what its `STAB-XXXX` entry in `plan.md` asks for.
- **No rewriting `AI_TRUNCATION_BUG.md` as a side effect of an unrelated
  task** — it's flagged as stale (§5), but cleaning it up should be its
  own small, deliberate step, not folded into other work.

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect only the files needed for the first
task in section 8. Do not refactor unrelated code. Make one small,
verified improvement. Run the relevant build/test command from section 7
and confirm cmake-build-debug still passes (19/19, or the new total if
you registered a new ctest). Update NEXT.md after finishing.

Current branch: develop; confirm sync with origin/develop before
resuming (last confirmed sync was commit a837492; commits since then
may not be pushed yet — check `git status` / `git log origin/develop..HEAD`).
Build dirs: cmake-build-debug/ (Debug, CLion cmake 4.2.2) — full rebuild
+ 19/19 ctest verified clean 2026-07-02 at commit f37c541 (no code has
changed since — only plan.md/NEXT.md docs — so this should still hold,
but re-run ctest if in doubt). b-release/ (Release) not re-verified
since 2026-07-01; re-verify if touching anything Release-sensitive.
Active plan: plan.md (STAB-XXXX tasks; Gate 3's P1 items, all of Gate 4,
and S10's remaining P0 items are done — S7 28/35, S8 17/40, S9 22/35,
S10 9/40, S13 11/25. Pick the next task from section 8: move to P2
items in whichever section is preferred, or optionally rotate the PAT
to activate CI).
Reconfigure cmake-build-debug ONLY with CLion's cmake 4.2.2, not
/usr/bin/cmake.
CI is parked deactivated under .github_/ (token lacks `workflow` scope,
see section 4).
```
