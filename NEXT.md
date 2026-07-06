# NEXT.md

_Last updated: 2026-07-06, commit `ce943e6` (develop, in sync with `origin/develop`)_

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

**Current phase:** Stabilization. **584/651** `plan.md` rows are ✅ (up
from 361 at the start of this session — S3 through S9 all closed or
effectively closed in one sitting, 223 rows). Sections **S0, S1, S2, S3,
S4, S5, S6, S7, S8, S9, S13, S17, S19** are all fully closed or closed
except one flagged row. S16/S18/S20 similarly. **S10 (AI Integration
Stability) is next** — 40 rows, 11 done, 29 open — see §4/§8.

**Important architectural decisions:** (unchanged from prior revisions —
see `git log -p -- NEXT.md` for full history) `mc3`/`mcb` CNA-free
standalone libs, CNA sibling repo never modified, `mc3togltf`/`mc3tomcb`
CNA-free CLI+lib, editor pure logic lives in CNA-free `Alg`-suffixed
headers (`EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`,
`Renderer/CsgCacheAlg.hpp`) — **not every mirror is wired back into its
real `.cpp`**, some are deliberate parallel duplicates purely for
testability (see §6). MCB fully documented (`MCB_FORMAT.md`). CSG via
Manifold v3.0.0, `isCutter`/`role="cutter"` semantics matter a lot (a
missing flag silently turns a subtraction into a union — bit this
session, see prior revision). Two CSG code paths (export vs. editor
preview) share the same semantics but are separate implementations.

---

## 2. Current status

### Build
**Debug**: 63/63 tests pass (up from 48 at session start) as of commit
`c4f3e61`. Working tree clean as of `ce943e6` (docs-only since). Release/
standalone builds not re-verified this session. Blender 4.3.2 happens to
be installed here — one ctest (`mc3togltf_blender_import`) exercises it
for real.

### What's now fully verified (S3-S9, this session)
MCB binary format, glTF/GLB export, CSG (both code paths), geometry
reuse/instancing/large scenes (including a real >1M-triangle stress test
and a genuine headless Blender import), editor save/load/undo/macro/
export workflows, UI robustness (crash-safety across ~35 rows), and the
ModelRegistry (SQLite-backed asset library) are all now empirically
verified — not just read from code. Full per-row detail is in `plan.md`;
`git log --oneline` has one commit per closed section.

### Real bugs found and fixed this session (chronological)
1. MCB silently dropped `Mc3Action::autoplay` (S3).
2. A nested-CSG test fixture was silently testing a no-op union instead
   of a real subtraction — missing `role="cutter"` (S5).
3. `clearCsgCache()` was missing from 2 of 3 document-load paths — stale
   CSG cache collision risk after switching files (S5).
4. `std::clamp(v, lo, hi)` UB in the panel-resize splitter handlers when
   the OS window shrinks below 240px width — no window minimum size is
   set anywhere (S8, STAB-0309).
5. ~6 material PBR sliders (roughness/metallic/alphaCutoff, duplicated
   across two separate material-editing UIs) could go out of [0,1] via
   Ctrl+Click, exporting spec-invalid glTF materials — missing
   `ImGuiSliderFlags_AlwaysClamp` (S8, follow-up to STAB-0325/0326).
   **~100 more slider/drag sites across the UI have the same gap,
   flagged as real follow-up work, not fixed this pass** (see §5).

### Real, confirmed-but-unfixed gaps (flagged 🟡 in plan.md, not bugs to silently ignore)
- **STAB-0289**: `mergeDocumentsAlg()` (Merge Scene) has no object-id
  collision handling — materials/textures/actions all suffix a
  colliding key, objects don't. Not data-loss, but ambiguous for
  anything that looks an object up by id post-merge. Real fix needs
  recursive subtree id-walking, out of scope for a quick patch.
- **STAB-0327**: no "prefs.ini created on first launch" behavior exists
  — it's only written when the Preferences dialog is explicitly closed.
  Whether to add real first-launch auto-save is a product decision.
- **STAB-0360**: no registry-DB-path override mechanism (env var/prefs)
  exists to document. Same kind of product decision as above.
- **STAB-0012/STAB-0092**: pre-existing from before this session
  (MinGW CNA gap, an accepted `<embeds>`-in-`<include>` limitation).

---

## 3. Recent changes

This session (2026-07-06) closed **S3 (MCB), S4 (glTF/GLB export), S5
(CSG), S6 (Geometry Reuse/Large Scenes), S9 (ModelRegistry) entirely**,
and **S7 (Editor Save/Load), S8 (UI Robustness) effectively** (one
flagged row each) — 223 rows total, 5 real bugs fixed (see §2), several
rows corrected where their assumed behavior didn't match the actual,
intentional design (a recurring and valuable pattern this session —
see the list in §2 and cross-reference `plan.md`'s per-row writeups for
STAB-0220/0226/0293/0309/0327/0333/0353/0356/0357/0360/0369).

A useful investigation pattern that worked well twice this session:
delegating a broad "read N files, categorize each open row as bug-found/
test-addable/already-safe/needs-correction" investigation to a forked
background agent, then implementing fixes from its structured report.
Used for S8 (23 rows) and S10 (29 rows, in progress as of this writing).

Full history is in `git log --oneline`; `plan.md` has a per-row writeup
for every `STAB-XXXX` ID.

---

## 4. Current blocker / main problem

**No blocker to local development or testing on Linux** — 63/63 tests
pass as of `c4f3e61`. Emscripten blank-canvas, MinGW cross-compile gap,
and CI credentials issues are all unchanged from prior sessions.

---

## 5. Known bugs and limitations

Unchanged from prior sessions (Emscripten blank canvas, MinGW GLES3 gap,
CI credentials, SVG rasterization, embedded glTF, `<embeds>`-in-
`<include>`, `mc3.xsd` numeric ranges, `mip_maps`, N3-N7 data-only, CSG
no real UVs) **plus this session's findings**:
- **`mergeDocumentsAlg()` object-id collisions** — see §2.
- **~100+ ImGui slider/drag call sites across the UI still lack
  `ImGuiSliderFlags_AlwaysClamp`** — the 6 highest-confidence-impact
  ones (material PBR factors, which would export spec-invalid glTF)
  were fixed this session; the rest (snap/grid settings, light/camera/
  geometry parameter drags, scatter/array-duplicate counts, animation
  keyframe/duration fields, etc.) need a systematic downstream-safety
  review before blanket-fixing — real, scoped follow-up work, documented
  at STAB-0326's row in `plan.md` rather than rushed through
  speculatively.
- **Two separate, duplicated material-editing UIs exist** (
  `Scene/PropertiesPanel.cpp` and `MeshCraftApplication_UiLeftPanel.cpp`
  both edit the same `Mc3Material` fields independently) — found while
  fixing the slider-clamp bug above; not itself broken, but worth
  knowing about if editing material UI logic in the future (a fix in
  one file's slider doesn't apply to the other's).
- **Not every `Alg` mirror is wired back into its real `.cpp`** — some
  (`PrefsAlg`, `loadRecentFilesAlg`/etc.) are deliberate parallel
  duplicates for testability only. Precedented, not a bug, but check
  both sides when touching one (see §6).

---

## 6. Architecture notes

Unchanged from prior revision (`git log -p -- NEXT.md` for the full
text) — CSG dual-code-path `isCutter` semantics, `Alg` mirror pattern
nuance (some mirrors deliberately unwired), undo/redo snapshot-based
mechanism, GL rendering caution (skybox VAO issue), `mc3.xsd` compiled
at configure time, and the **XML comment gotcha**: `--` anywhere inside
an XML comment is rejected by `lxml`/`validate_xsd.py` even though
`tinyxml2` (the app's own parser) tolerates it — always validate new
`.mc3.xml` fixtures with `test/validate_xsd.py` before trusting them
(hit this repeatedly writing new S5/S6 fixtures this session).

**New this session**: the AI integration (`AiAssistant.cpp`,
`MeshCraftApplication_UiAi.cpp`, `AiResponseAlgorithms.hpp`) already has
substantial test-seam infrastructure from a prior session worth knowing
about before touching S10: `AiAssistant::apiBaseUrl` (override for a
local mock `httplib::Server`), `connectTimeoutSec`/`readTimeoutSec`/
`writeTimeoutSec` (override for timeout tests), and the whole validation
pipeline (`extractXmlAlg`/`validateAndParseAiResponseAlg`/
`isEmptyMc3DocumentAlg`/`validateXmlAgainstXsdAlg`) already extracted
CNA-free into `AiResponseAlgorithms.hpp` for direct `ai_test.cpp` unit
testing, separate from `mc3_commands_test`.

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

# --- Validate an XML fixture (also catches the "--" inside comment bug)
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# --- Run / export / validate / version
./cmake-build-debug/MeshCraft test/house.mc3.xml
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
ctest -R mc3_roundtrip --output-on-failure
ctest -R mcb_roundtrip --output-on-failure
ctest -R "mc3togltf_csg" --output-on-failure    # all CSG export tests (S5)
ctest -R csg_cache --output-on-failure          # editor CSG preview-cache tests (S5)
ctest -R "mc3togltf_large_scene|mc3togltf_blender|mc3togltf_large_obj" --output-on-failure  # S6
ctest -R mc3_commands --output-on-failure       # editor Alg-mirror unit tests (S7/S8)
ctest -R mc3_registry --output-on-failure       # ModelRegistry tests (S9)
ctest -R mc3_ai --output-on-failure             # AI integration tests (S10, next)

# --- Push
git push origin develop
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

Continue **S10 (AI Integration Stability)**, `plan.md` priority order —
40 rows, 11 done, 29 open (6 🧪 + 23 📋). A background investigation
of this whole section (using the same "fork investigates, report
findings, then implement" pattern that worked well for S8) may already
be complete or in progress — check for a completed fork report before
re-investigating from scratch. If none exists, investigate using
`mc3/test/ai_test.cpp`'s existing mock-`httplib::Server` pattern and
`AiResponseAlgorithms.hpp`'s extraction pattern (see §6) as the template
for what's addable vs. needs code-inspection-only verification (ImGui
widget rendering like a password-mask flag or spinner has no headless
test, matching the many already-✅ precedents like STAB-0302/0307/0312/
0319/0350/0352 elsewhere in this codebase).

Notable rows worth extra scrutiny (from a quick read, not yet verified):
- **STAB-0387/0388**: async lifetime safety (cancel/reset while a
  request is in flight, document destroyed mid-request) — genuinely
  worth a careful look given `AiAssistant` runs a background thread;
  a real use-after-free here would be a serious bug, not just a
  crash-safety nicety.
- **STAB-0389/0390**: the `MESHCRAFT_HAS_AI` build guard — check whether
  the existing Emscripten build (which already disables SQLite3/LibXml2
  via the same `NOT EMSCRIPTEN AND NOT ANDROID` pattern per `README.md`'s
  platform matrix) already proves the non-AI path compiles, before
  assuming new work is needed.
- **STAB-0397**: full AI→registry pipeline mock test — a good candidate
  to actually write given both halves (mock AI response, ModelRegistry)
  already have solid test infrastructure from S9/prior sessions.

**After S10 closes**, continue to **S11 (Materials, Textures, and Visual
Fidelity)** in `plan.md` order (30 rows, 25 done, 5 open — 2 🧪 + 3 📋).

---

## 9. Do not do yet

Unchanged from prior revisions (no new scene-format features, no CNA/
SHARP_RUNTIME changes, no `Mc3Document` public API changes without
checking dependents, no SVG rasterization or `embed:`/`<embeds>`
implementation without an explicit decision, no mass refactoring, no
MinGW GLES-header vendoring, no STAB-0642/0643/0650 without the missing
tool/access, no CSG UV-preservation implementation, no speculative fix
for STAB-0289 without discussing scope first).

New from this session: **no blanket slider-clamp sweep across the
remaining ~100 sites** without a per-site downstream-safety review first
(see §5) — the material-PBR fix was justified by a *confirmed* downstream
consequence (invalid glTF export); most of the rest don't have one
confirmed yet, so fixing them all mechanically risks papering over
sites where the actual bug (if any) is elsewhere.

---

## 10. Resume prompt

```
Read NEXT.md first. Then check whether a background investigation of S10
(AI Integration Stability) already completed — if so, use its findings
directly; if not, launch one (or investigate directly) covering the 29
open rows in plan.md's S10 section, using mc3/test/ai_test.cpp's existing
mock-httplib::Server pattern and AiResponseAlgorithms.hpp's CNA-free
extraction pattern as the template for what's genuinely testable vs.
ImGui-rendering-only (no headless test possible).

Make one small, verified improvement at a time. Run the relevant build/
test command from section 7 and confirm cmake-build-debug still passes
(63/63, or the new total if you registered a new ctest). Update plan.md's
row for the task you completed, and update NEXT.md's section 3/8 after
finishing a batch (not necessarily after every single row).

Current branch: develop, in sync with origin/develop at commit ce943e6.
Build dir: cmake-build-debug/ (Debug, CLion cmake 4.2.2) — last full
rebuild + 63/63 ctest was clean at commit c4f3e61, working tree clean as
of ce943e6 (docs-only commits since).

Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file / new add_test()
requires a reconfigure, not just a rebuild. New .mc3.xml fixture
comments must never contain "--" anywhere inside them (lxml/
validate_xsd.py rejects it even though tinyxml2 doesn't).

The Emscripten blank-canvas issue (section 4) needs a human with a
browser to close the loop — if none is available, stay on the plain S10
backlog work in section 8 instead.

CI is parked deactivated under .github_/ (credentials issue). Commit
after each STAB-XXXX task and push to origin/develop — standing workflow
across this whole stabilization effort.
```
