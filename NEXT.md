# NEXT.md

_Last updated: 2026-07-06, commit `252f266` (develop, in sync with `origin/develop`)_

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
adding new features. All work is tracked in `plan.md` as 650 `STAB-XXXX`
tasks across sections S0–S20, gated by a Gate 0–6 checklist
(`STABILIZATION.md`).

**Current phase: stabilization is essentially DONE.** **620/650** `plan.md`
rows are ✅, **29** are 🟡 (genuinely flagged — documented real gaps or
corrected premises, not meant to auto-resolve), **0** are 🧪, and exactly
**1** is still 📋 (STAB-0650, blocked on the repo owner rotating a PAT
before CI can even be activated to check its output — nothing more to do
here without that action). Every section S0–S20 is closed except for its
handful of permanently-flagged 🟡 rows. **There is no more open
"stabilization backlog" work left to autonomously pick up** — see §8 for
what's actually left.

**Important architectural decisions:** (unchanged from prior revisions —
see `git log -p -- NEXT.md` for full history) `mc3`/`mcb` CNA-free
standalone libs, CNA sibling repo never modified, `mc3togltf`/`mc3tomcb`
CNA-free CLI+lib, editor pure logic lives in CNA-free `Alg`-suffixed
headers (`EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`,
`Renderer/CsgCacheAlg.hpp`) — **not every mirror is wired back into its
real `.cpp`**, some are deliberate parallel duplicates purely for
testability (see §6). MCB fully documented (`MCB_FORMAT.md`). CSG via
Manifold v3.0.0, `isCutter`/`role="cutter"` semantics matter a lot (a
missing flag silently turns a subtraction into a union). Two CSG code
paths (export vs. editor preview) share the same semantics but are
separate implementations.

---

## 2. Current status

### Build
**Debug**: 66/66 tests pass as of commit `252f266`. Working tree clean.
Also did a **from-scratch Emscripten web build this session** (see §6) —
succeeded cleanly, and was verified running in real headless Chrome
(WebGL2 context creation confirmed, no errors). Release/standalone
builds not separately re-verified.

### What's now fully verified (S3-S16, S20 — across this and prior sessions)
MCB binary format, glTF/GLB export, CSG (both code paths), geometry
reuse/instancing/large scenes, editor save/load/undo/macro/export
workflows, UI robustness, ModelRegistry, **AI integration (S10, including
a real hang-on-close bug fix)**, **materials/textures/visual fidelity
(S11)**, animation (S12, 2 flagged), commands/undo/redo (S13),
rendering/viewport (S14, GL-state-leak check added), import/export
integration (S15), **cross-platform (S16, fully closed — Emscripten/
Blender both verified this session)**, documentation (S17), code quality
(S18), security (S19), and release readiness (S20, down to 1 blocked
row) are all now empirically verified — not just read from code. Full
per-row detail is in `plan.md`; `git log --oneline` has one commit per
closed section/batch.

### Real bugs found and fixed this session (chronological, S10 onward)
1. **`AiAssistant` hang-on-close** (STAB-0387/0388): `reset()`/destructor
   blocked on an in-flight `std::async` future — could hang the app for
   up to 600s on shutdown/reset while an AI request was running. Fixed
   by replacing `std::future`/`std::async` with a detached `std::thread`
   + `shared_ptr<AiRequestResult>` (atomic done-flag, mutex-guarded
   fields) — non-blocking `reset()`, no lifetime issues either way.
2. **Stale `"claude-sonnet-4-6"` model default** in the UI's `aiModelBuf_`
   and its empty-buffer fallback/tooltip — independent of `AiAssistant::
   model`'s already-fixed default, so every request silently used the old
   model name unless manually retyped.
3. **Temp file leak on every malformed AI response** — `parseXmlAlg()`/
   `serializeScene()` only cleaned up their scratch file on the success
   path; malformed XML (the common real-world case) leaked a file every
   time. Fixed with `try/catch`-guaranteed cleanup.
4. **No confirmation before a drastic AI "Apply to Scene" shrink**
   (STAB-0395) — added a two-click "Confirm Replace" gate when the AI
   result has less than half the current scene's object count (on
   scenes ≥10 objects).
5. **No pre-Send byte-count indicator** (STAB-0409) — added, sharing the
   exact serialization helper the Send button itself uses so they can't
   drift apart.
6. **`logError()`/GL-error checking was dead code** (STAB-0521) — a
   `glGetError()` helper existed but had zero call sites anywhere. Added
   a real per-frame check (`checkGlStateLeak()`), wired into the
   `--screenshot` diagnostic output (`[GLCheck] clean`/`error`), with a
   new permanent ctest.
7. **Web GLB export has no browser-download bridge** (STAB-0571,
   flagged not fixed) — `-sFORCE_FILESYSTEM=1` exposes `Module.FS` but
   nothing reads an exported file back out into a `Blob`/download; a
   real gap, new-feature-sized, out of scope during stabilization.
8. **Unescaped literal `%`** in `PropertiesPanel.cpp:1087`'s
   `ImGui::TextDisabled("Inner Radius (0 = 50%)")` — a printf-style
   format-string bug, surfaced as a compiler warning during the
   from-scratch Emscripten rebuild. Fixed to `50%%`.
9. **Accidental duplicate `STAB-0521` row in `plan.md`** — found and
   removed; corrected the plan's total row count back to 650 (the
   "651, S14 has 31 not 30" note in the old summary was describing this
   very duplicate, not a legitimate extra task).

### Real, confirmed-but-unfixed gaps (flagged 🟡 in plan.md — 29 total)
Grouped by recurring cause (see the memory file `project_meshcraft_
stabilization.md` for the full breakdown):
- **Needs a live interactive display session** (mouse-driven ImGui state
  a headless environment can't reach) — most of S12/S14's remaining 🟡
  rows (curve editor visibility, proportional-edit radius indicator,
  locked-object outline, FPS counter, drag-drop OS gesture).
- **CNA-side blocker, out of scope** — Windows/MinGW build (CNA
  hardcodes GLES3 headers unconditionally for EASYGL).
- **Confirmed real gap, feature-moratorium applies** — web GLB download
  bridge (STAB-0571), animation scale-time function (STAB-0460),
  material-duplicate command (STAB-0427/D3), merge-scene object-id
  collision handling (STAB-0289), no first-launch prefs.ini auto-save
  (STAB-0327), no registry-DB-path override (STAB-0360).
- **Corrected premise** — row's literal expectation doesn't match a
  legitimate design choice (STAB-0389: AI panel intentionally visible
  but degrades gracefully in non-AI builds, not hidden; STAB-0423:
  preview sphere has no environment map, so "mirror-like" isn't
  physically achievable; STAB-0386: no `saveToString()` exists, scene
  serialization round-trips through a temp file instead, same result).
- **STAB-0012/STAB-0092**: pre-existing (MinGW CNA gap, an accepted
  `<embeds>`-in-`<include>` limitation).

---

## 3. Recent changes

This session picked up from S10 (AI Integration Stability, 40 rows, 11
done at the start) and closed **S10, S11, S12 (already mostly done),
S13 (already done), S14, S15 (already mostly done), S16, and S20's
remaining rows** — all the way to only 1 open row left in the entire
650-task plan. Key milestones, each its own commit:
- `stab: STAB-0387/0388 — fix AiAssistant hang-on-close via detached thread`
- `stab: close S10 (AI Integration Stability) — STAB-0380..0410`
- `stab: close S11 (Materials, Textures, Visual Fidelity) — STAB-0421..0431`
  (added a real headless-Blender PBR-material import test)
- `stab: STAB-0521 — wire up a real per-frame GL error check; close S14`
  (also found+removed the duplicate STAB-0521 row)
- `stab: close S16/S20 web+platform rows with real browser/Blender evidence`
  (from-scratch Emscripten build + real headless-Chrome verification;
  Blender-based release-sample import test)

A useful investigation pattern from prior sessions kept paying off: when
a row's stated verification method turns out to be infeasible as written
(no tool, no headless path), don't just leave it "manual" — check
whether the actual capability exists in this environment before assuming
it doesn't (Blender and a working Emscripten SDK + real Chrome both
turned out to be available, closing several rows that earlier notes had
marked as blocked).

Full history is in `git log --oneline`; `plan.md` has a per-row writeup
for every `STAB-XXXX` ID.

---

## 4. Current blocker / main problem

**No blocker to local development or testing on Linux** — 66/66 tests
pass as of `252f266`. The only remaining plan.md item (STAB-0650) is
blocked on the repo owner rotating a PAT for the parked-deactivated CI
workflow — nothing to do here without that action. MinGW cross-compile
is blocked on a CNA-side GLES3-header gap (out of scope, needs the CNA
maintainer). The Emscripten build itself is healthy (see §6); its
known pre-existing "blank canvas" visual-usability limitation (from a
prior session, STAB-0553) is unchanged and still not root-caused.

---

## 5. Known bugs and limitations

Unchanged from prior sessions (Emscripten blank-canvas rendering issue,
MinGW GLES3 gap, CI credentials, SVG rasterization, embedded glTF,
`<embeds>`-in-`<include>`, `mc3.xsd` numeric ranges, `mip_maps`, N3-N7
data-only, CSG no real UVs, ~100+ non-material ImGui slider sites still
lacking `ImGuiSliderFlags_AlwaysClamp` — see prior revision for the full
list, `git log -p -- NEXT.md`) **plus this session's findings** (§2's
numbered list — the AI hang-on-close fix, the web GLB-download gap, the
GL-error-check dead-code fix, etc.).

- **Two separate, duplicated material-editing UIs exist** (
  `Scene/PropertiesPanel.cpp` and `MeshCraftApplication_UiLeftPanel.cpp`)
  — a fix in one file's slider doesn't apply to the other's.
- **Not every `Alg` mirror is wired back into its real `.cpp`** — some
  (`PrefsAlg`, `loadRecentFilesAlg`/etc.) are deliberate parallel
  duplicates for testability only. Precedented, not a bug, but check
  both sides when touching one (see §6).
- **No web GLB-download bridge** (STAB-0571) — see §2.

---

## 6. Architecture notes

Unchanged from prior revision (`git log -p -- NEXT.md` for the full
text) — CSG dual-code-path `isCutter` semantics, `Alg` mirror pattern
nuance (some mirrors deliberately unwired), undo/redo snapshot-based
mechanism, GL rendering caution (skybox VAO issue), `mc3.xsd` compiled
at configure time, and the **XML comment gotcha**: `--` anywhere inside
an XML comment is rejected by `lxml`/`validate_xsd.py` even though
`tinyxml2` (the app's own parser) tolerates it — always validate new
`.mc3.xml` fixtures with `test/validate_xsd.py` before trusting them.

**New this session — environment capabilities worth knowing about:**
- **Blender 4.3.2 is installed** (`/usr/bin/blender`) — usable for real
  headless GLB-import verification via `bpy.ops.import_scene.gltf` +
  reading back node values (`bpy.data.materials[...].node_tree.nodes`)
  through Python. Three ctests now use it: `mc3togltf_blender_import`
  (STAB-0252, synthetic scene), `mc3togltf_material_pbr_blender_import`
  (STAB-0431, one box+material), `mc3togltf_release_sample_blender_import`
  (STAB-0642, real authored content — `medieval_castle.mc3.xml`).
- **Emscripten SDK is installed** at `~/Downloads/emsdk` (v5.0.7) but not
  on `PATH` by default — `source ~/Downloads/emsdk/emsdk_env.sh` first,
  then `./build-web.sh` builds into `cmake-build-web/` (gitignored,
  ~315MB, takes a few minutes from scratch).
- **google-chrome is installed** — can run genuinely headless with
  software WebGL2 via `google-chrome --headless=new
  --enable-unsafe-swiftshader --use-gl=angle --use-angle=swiftshader
  --screenshot=out.png <url>` (serve the web build dir first, e.g.
  `python3 -m http.server`). Confirmed real WebGL2 context creation with
  no errors this way. **No argv/URL-param scene loading and no UI-
  automation harness exist for the web target** — toggling runtime UI
  state (SSAO/bloom checkboxes, opening a file) and visually confirming
  effects still needs either a human or future automation investment
  (synthetic canvas mouse-event dispatch).
- AI integration (`AiAssistant.cpp`, `MeshCraftApplication_UiAi.cpp`,
  `AiResponseAlgorithms.hpp`) test-seam infrastructure: `AiAssistant::
  apiBaseUrl` (override for a local mock `httplib::Server`),
  `connectTimeoutSec`/`readTimeoutSec`/`writeTimeoutSec` (override for
  timeout tests), the whole validation pipeline (`extractXmlAlg`/
  `validateAndParseAiResponseAlg`/`isEmptyMc3DocumentAlg`/
  `validateXmlAgainstXsdAlg`) CNA-free in `AiResponseAlgorithms.hpp` for
  direct `ai_test.cpp` unit testing. The background HTTP call now runs
  on a **detached `std::thread`** writing into a `shared_ptr<
  AiRequestResult>` (atomic `done` + mutex-guarded fields) — not
  `std::async`, so destroying `AiAssistant` never blocks (STAB-0387/0388).

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid a system-cmake bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (66)
ctest -N                                                      # lists all 66 tests
ctest --print-labels                                          # format/export/render/registry/ai/commands

# --- Validate an XML fixture (also catches the "--" inside comment bug)
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# --- Run / export / validate / version
./cmake-build-debug/MeshCraft test/house.mc3.xml
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
ctest -R mc3_ai --output-on-failure             # AI integration tests (S10)
ctest -R mc3_registry --output-on-failure       # ModelRegistry tests (S9)
ctest -R gl_state_leak --output-on-failure      # STAB-0521 GL error check (S14)
ctest -R blender --output-on-failure            # all 3 Blender-based tests

# --- Web build (Emscripten) — see §6 for details
source ~/Downloads/emsdk/emsdk_env.sh
./build-web.sh                                  # builds cmake-build-web/MeshCraft.html
cd cmake-build-web && python3 -m http.server 8080
google-chrome --headless=new --enable-unsafe-swiftshader \
    --use-gl=angle --use-angle=swiftshader \
    --screenshot=/tmp/out.png http://localhost:8080/MeshCraft.html

# --- Push
git push origin develop
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

**There is no open stabilization-plan backlog left to autonomously pick
up.** Before starting new work, a future session should:

1. **Check with the user** about direction — the natural next steps are
   either (a) resuming feature work (the moratorium was "no new features
   until stabilization is done" — it now essentially is), or (b) picking
   a different initiative entirely. Don't assume either without asking.
2. If asked to keep closing plan.md rows: the only one left is
   **STAB-0650** (CI report consistency), and it's blocked on the repo
   owner rotating a PAT — nothing to do without that.
3. If asked to revisit the 29 flagged 🟡 rows: most need a live human
   with a real display/mouse (curve editor, radius indicator, FPS
   counter, drag-drop gesture) or are deliberate product decisions
   (STAB-0289/0327/0360/0427/0460/0571) that need scope discussion
   before implementing, not a quick patch — re-read each row's writeup
   in `plan.md` before touching it, since the reasoning for *why* it's
   flagged (not just that it is) matters for judging whether anything
   changed.
4. If asked to do a general code-quality/architecture pass: S18 (Code
   Quality and Architecture) is closed but was scoped to the original
   650-row list, not an open-ended audit — a fresh review might find
   more, but that's a different kind of task than continuing this plan.

---

## 9. Do not do yet

Unchanged from prior revisions (no new scene-format features, no CNA/
SHARP_RUNTIME changes, no `Mc3Document` public API changes without
checking dependents, no SVG rasterization or `embed:`/`<embeds>`
implementation without an explicit decision, no mass refactoring, no
MinGW GLES-header vendoring, no CSG UV-preservation implementation, no
speculative fix for STAB-0289/0327/0360/0427/0460 without discussing
scope first, no blanket slider-clamp sweep across the remaining ~100
non-material sites without a per-site downstream-safety review first).

New from this session: **no web GLB-download-bridge implementation**
(STAB-0571) without discussing scope first — it's real new-feature work
(JS glue, likely a custom HTML shell), not a stabilization fix.

---

## 10. Resume prompt

```
Read NEXT.md first. The 650-task stabilization plan (plan.md) is now
essentially complete: 620 ✅ / 29 🟡 (permanently flagged) / 1 📋
(STAB-0650, blocked on repo-owner PAT rotation) / 0 🧪.

Before doing anything else, check in with the user about direction —
there is no more open stabilization backlog to autonomously continue.
Do not assume a next task; ask what they'd like next (resume feature
work, revisit a specific flagged row with new context, a different
initiative, etc.).

If continuing autonomous work was explicitly re-authorized: the only
legitimately open plan.md row is STAB-0650, which needs the repo owner's
action, not code. Revisiting any of the 29 flagged 🟡 rows requires
re-reading that row's own reasoning in plan.md first (most need a live
human with a display, or are deliberate product decisions needing scope
discussion, not code-only fixes) — see NEXT.md §8 for detail.

Current branch: develop, in sync with origin/develop at commit 252f266.
Build dir: cmake-build-debug/ (Debug, CLion cmake) — last full rebuild +
66/66 ctest was clean at this commit, working tree clean.

Reconfigure cmake-build-debug ONLY with CLion's cmake, not the system
cmake. Editing mc3.xsd or adding a new .cpp file / new add_test()
requires a reconfigure, not just a rebuild. New .mc3.xml fixture
comments must never contain "--" anywhere inside them.

Blender (/usr/bin/blender) and a working Emscripten SDK
(~/Downloads/emsdk, source emsdk_env.sh first) are both available in
this environment — useful for any future web/Blender-adjacent
verification work, see NEXT.md §6.

CI is parked deactivated under .github_/ (credentials issue, needs the
repo owner). Commit after each STAB-XXXX task and push to origin/develop
— standing workflow, but only applicable if there's still a task to do.
```
