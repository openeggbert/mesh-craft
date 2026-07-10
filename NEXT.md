# NEXT.md

_Last updated: 2026-07-10, prior commit `6db484c` (branch `develop`). Native Linux build/tests re-verified from a genuinely empty scratch build directory the same day, after fixing a live rendering-correctness bug in `SceneRenderer`'s live-preview geometry builders and closing out the `mc3togltf` export audit's remaining `needs_human` items (see §3) — see `STABILIZATION_WORKLOG.md` for earlier command traces._

---

## 1. Project summary

**MeshCraft** is a C++23 3D scene editor for **`.mc3.xml`**, a custom XML scene-description format used by the OpenEggbert project. The editor UI is built on **Dear ImGui**, running on **CNA** (an XNA-style SDL3+OpenGL runtime — sibling repo at `../cna`, never modified from this repo) and **SHARP_RUNTIME** (`../sharp-runtime`, the .NET-BCL-style math/collections library CNA depends on). Scenes export to **glTF/GLB** via `mc3togltf` and to a compact **binary format (MCB)** via `mc3tomcb`.

**Main goal**: reach a fully stabilized, test-covered codebase before adding new product features. All stabilization work is tracked in `plan.md` as `STAB-XXXX` tasks across sections S0–S25 (`STABILIZATION.md` holds the gate policy). The original S0–S20 (650 tasks) was later extended with 5 audit-driven sections: S21 (mc3 format spec-vs-impl), S22 (MCB binary coverage), S23 (mc3togltf export quality), S24 (editor UI coverage of the mc3 format — driven by `missing.md`), and S25 (live-preview rendering correctness) — 722 tasks total.

**Current phase**: stabilization is functionally complete but not formally "all green." `plan.md`: **676/722 rows ✅, 44 🟡 (mostly implemented-but-pending-live-visual-verification — this headless environment can't drive ImGui click/drag interaction; a smaller number are permanently-flagged real gaps or product decisions), 0 needs_human, 2 📋 (one blocked on an external action, one explicitly skipped), 0 🔴**. Per this project's own policy, **new feature work is not yet authorized** without explicit per-task owner approval — the S24 UI-coverage work (§3) was all individually approved task-by-task per `CLAUDE.md`'s `plan.md` review workflow, not a blanket go-ahead.

**Important architectural decisions**:
- `mc3` (data model + XML parser/writer) and `mcb` (binary serializer) are **CNA-free standalone libraries** — must build and test independently of CNA/ImGui.
- `mc3togltf` and `mc3tomcb` are **CNA-free CLI tools + libraries** built on top of `mc3`/`mcb`.
- CNA (`../cna`) and SHARP_RUNTIME (`../sharp-runtime`) are **sibling repos this project must not modify without explicit permission**.
- Editor logic that needs headless unit testing is extracted into CNA-free `*Alg`-suffixed header files (`EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`, `Renderer/CsgCacheAlg.hpp`). Not every `Alg` mirror is wired back into its real `.cpp` call site — a few (`PrefsAlg`, `loadRecentFilesAlg`) are deliberate parallel duplicates kept only for testability. Check both sides before changing one.
- CSG (boolean mesh ops) uses Manifold v3.0.0 via **two independent code paths** — export-time (`mc3togltf/src/CsgEvaluator.cpp`) and editor-preview-time (`SceneRenderer`) — that must stay semantically consistent but are separate implementations.

---

## 2. Current status

### Build
Clean from an **absolute-zero** build directory (not just incremental): `rm -rf cmake-build-debug` → reconfigure → `ninja` → **exit 0**. This was specifically verified this way after discovering that an incrementally-updated build directory had been silently masking a real clean-build break for at least two days (see §4).

### Tests
**86/86 CTest tests pass** (re-verified 2026-07-10, multiple times, after every commit in the S23/S24/S25 work below). Labels: `ai` 1, `commands` 1, `export` 61, `format` 3, `registry` 1, `render` 19. Standalone (CNA-free) subproject builds also verified independently as part of the same suite.

### Tools/binaries currently available (after a build)
- `MeshCraft` — the GUI editor (Linux native; also runs headless via `--screenshot scene.mc3.xml out.ppm` for CI-style pixel checks).
- `mc3togltf` — CLI, `.mc3.xml` → `.gltf`/`.glb`.
- `mc3tomcb` — CLI, `.mc3.xml` ↔ `.mcb` (binary format).
- 5 C++ test binaries (`ai_test`, `mc3_registry_test`, `mc3_roundtrip_test`, `mc3_commands_test`, `mcb_roundtrip_test`), each runnable standalone for fast iteration.

### Recently implemented / working features
- Full `.mc3.xml` parse/write roundtrip, including `<include>` (with cycle detection), N1–N7 schema extensions.
- glTF/GLB export with geometry-reuse caching, CSG (union/difference/intersection, strict + approximate-fallback modes), material/texture/animation export.
- MCB binary format, fully documented (`MCB_FORMAT.md`).
- SQLite-backed `ModelRegistry` (asset library: save/search/insert-into-scene, with graceful stub when SQLite3 isn't available).
- AI Assistant integration (Claude API): mock-server-tested end to end, no real network call in tests; background HTTP work runs on a detached thread (not `std::async`), so closing the app or resetting mid-request never hangs.
- Editor: undo/redo, autosave/backup, save/load, keybindings/macros/preferences persistence, drag-and-drop scene loading.
- **Editor UI for every N1–N7 mc3 extension** (added 2026-07-10, `plan.md` §S24): SVG Textures, Embeds, Scripts, Audio (Sounds/Music), Triggers, and Scene States each now have a full list+editor tab, matching the established `+`/`-`/list/ID+copy pattern. Audio has **real playback** via CNA's `SoundEffect`/`SoundEffectInstance` (▶/■ buttons), not just data editing.
- OBJ import/export (`plan.md` STAB-0717/0718): OBJ export reuses the existing `GltfExporter` (export to temp `.glb`, re-read via `tinygltf`, walk the flattened node graph) rather than reimplementing scene traversal.
- `test/undo_coverage_audit.py`: a permanent (non-CI-gate) heuristic scanner that flags ImGui mutator calls with no nearby `pushUndo()` — used to find and fix 3 genuine undo-coverage gaps (Extrude checkboxes, the whole inline Material tab, Environment/Fog fields).
- Real headless-Blender-based tests confirm exported GLBs (including a real authored scene, `medieval_castle.mc3.xml`) import correctly with matching PBR material values.
- Web (Emscripten) build: the **2026-07-06 build artifacts** (`cmake-build-web/MeshCraft.{html,js,wasm}`, still present) run in a real headless-Chrome session with a working WebGL2 context (no console/GPU errors). **A fresh rebuild currently fails** — see §4, unchanged this session.

### What does NOT work yet
- **Windows GUI build (MinGW)**: does not compile — see §4. Not touched this session.
- **Emscripten web build**: does not rebuild from scratch — see §4. Not touched this session.
- **Web GLB export download**: exporting a GLB in the web build writes to Emscripten's in-browser virtual filesystem only — there is no JS bridge to actually download the file to the user's real filesystem.
- SVG texture rasterization: parsed/serialized but never rasterized (stub only) — now has a full editor UI (§S24) but rasterization itself is unchanged.
- Embedded glTF references (`<mesh src="embed:id"/>`): parsed/serialized but not resolved by the exporter — now has a full editor UI (§S24) but exporter-side resolution is unchanged.
- Scripts/Triggers: now fully editable via UI (§S24), but there is still no runtime *execution* — no Lua interpreter, no trigger event-binding/dispatch. Data-model + editing only, by design.
- `rotation_units="radians"` / non-default `euler_order`: correctly handled on **export** (`STAB-0691`, earlier session), but the editor's own live rendering/gizmos/mouse-drag interaction do not honor them — resolved 2026-07-10 as a deliberate **won't-fix** (`plan.md` STAB-0701: real risk across 8-10 rendering/interaction call sites, zero real content depends on it — `castle.mc3.xml`/`blupi_car.mc3.xml`/`speedy_blupi_world.mc3.xml` all use the default `degrees`/`XYZ`). The editor now shows a status-bar warning on load instead of silently rendering such a file wrong.
- Native file-browse dialog for texture import: still text-field/drag-drop only (`STAB-0716`, user explicitly chose to skip 2026-07-10 — `CNA_DEVICES` is an all-or-nothing CMake flag bundling 7 unrelated device features, and Linux `FileDialog` needs an XDG portal unavailable in this sandboxed environment).
- Android build: never attempted (no Android NDK in this environment).

---

## 3. Recent changes

**2026-07-10 — Editor UI coverage audit (`missing.md` → `plan.md` §S24, `STAB-0703`–`0721`, 17 done, 2 explicitly deferred/skipped) + a live rendering-correctness bug fix (§S25) + closing out the last 3 `needs_human` items from §S23.** Three separate pieces of work in one extended session, each driven by explicit user request/approval per task:
- **`missing.md`**: a 5-parallel-fork audit of editor-UI-vs-mc3-format coverage (distinct from §S23's export-fidelity audit). Found the N1–N7 schema extensions (SVG textures, embeds, scripts, sounds/music, triggers, scene states) had zero editor UI, plus several smaller partial gaps (IcoSphere subdivision UI showed a hardcoded-wrong "320 triangles" label; `Mc3Primitive::axis` had no UI for the 4 primitive types that consume it; `coordinate_system` combo offered an XSD-invalid 3rd option; no whole-scene OBJ import/export; a real undo-coverage gap risk). Turned into 19 `STAB-0703`–`0721` tasks in a new `plan.md` §S24 section.
- **§S24 implementation**: went through all 19 tasks one at a time per `CLAUDE.md`'s ask-before-implementing workflow. 17 completed (editors for all 6 N-extension types incl. real CNA-backed audio playback, primitive `axis` UI, IcoSphere subdivision fix, `coordinate_system` fix, Scene `meta` editor, OBJ import/export, `test/undo_coverage_audit.py` + 3 real undo-coverage fixes it found, "Group" added to the Add menu, an "Area (trigger zone)" label + a fix so freshly-created Areas actually get an editable/persistable size). `STAB-0716` (native file-browse dialog) explicitly skipped by the user (`CNA_DEVICES` is an all-or-nothing flag; Linux needs an XDG portal unavailable here). `STAB-0710` (rotation-unit UI) was sequenced to wait on `STAB-0701`, then closed won't-fix alongside it (see below).
- **§S25 — live-preview rendering bug (`STAB-0722`, the user's own bug report)**: user reported the castle scene rendering with "3 of 6 walls transparent, seeing through boxes." Root cause: `SceneRenderer_Builders.cpp` (the editor's live-preview geometry, separate from `mc3togltf`'s export-side builders) wound several shapes' triangles CCW-from-outside (the glTF/OpenGL convention) instead of CW-from-outside, which is what CNA's actual, correctly-implemented default `RasterizerState` (`CullCounterClockwiseFace`, true XNA/D3D9 semantics) requires — every affected face got backface-culled and you'd see the mirrored interior of the opposite face instead. Fixed 5 of 7 shape builders (Box, Cone-side, Torus, Capsule-mid-rings, IcoSphere — Cylinder/Sphere/Cone-cap/Capsule-poles were already correct); each fix is a 2-of-3 index swap per triangle, no vertex/normal/UV changes. Verified numerically (cross-product-vs-normal re-check, all now CW-correct) and visually (a close-up single-box A/B screenshot comparison — the broken state visibly shows the box's hollow interior).
- **§S23 close-out**: the 3 previously-`needs_human` rows are now all resolved. `STAB-0672`: added `SceneRenderer::csgWarning()` + a Properties-panel "⚠ Preview incomplete" line so an author can tell when a CSG preview silently dropped unsupported Mesh/Extrude content. `STAB-0695`: added `Mc3Camera::orthoAspect` (default 1.0, purely additive) wired through XSD/parser/writer/MCB/glTF-export/UI, so orthographic cameras can export a non-square view volume — user confirmed this is wanted. `STAB-0701`: investigated fully wiring `rotation_units`/`euler_order` into the editor's own rendering, found the real scope is 8-10 call sites (several interactive, unverifiable headlessly) for a feature **no real content uses** (only a synthetic export-test fixture) — resolved as won't-fix, added a load-time status-bar warning instead. `STAB-0710` closed as moot in the same decision.
- Full 86/86 ctest green after every single commit in this batch (one task = one commit = one push, per this session's established convention).

**2026-07-10 — MCB binary-format coverage audit (`plan.md` §S22, `STAB-0658`–`0661`, all 4 completed).** Systematic comparison of `mcb/src/McbWriter.cpp`/`McbReader.cpp` against every field in every `mc3/include/MeshCraft/Mc3/*.hpp` header, to find silent data loss on `mc3 -> MCB -> mc3` round-trips. Overall finding: coverage is very good — all 19 object types, CSG, extrude, all N1-N7 extension sections, materials, lights, cameras, animation, and include-bookkeeping all round-trip correctly. 4 gaps found and fixed, all previously-untested combinations (this batch added the first mcb test coverage for `Mc3Environment` and `Mc3Texture` as whole structs, not just individual pre-existing fields):
- **`STAB-0658`** (P0): `doc.rotationUnits`/`doc.eulerOrder` were entirely missing — silently reverted every rotation's interpretation to degrees/XYZ after any MCB round-trip if the document used radians or a non-default euler order.
- **`STAB-0659`** (P0): `Mc3Environment::skyboxTexture` missing (its sibling `backgroundTexture` was covered, masking the gap).
- **`STAB-0660`** (P1): `Mc3Texture::mipMaps` missing — the exact bug class `STAB-0654` (§S21) just fixed on the XML side, found fresh on the MCB side because that fix didn't touch `mcb/`.
- **`STAB-0661`** (P1): per-object `Mc3Object::metadata` missing (document-level `metadata`/`meta` were covered — this is the per-object one).
- Full 66/66 ctest after every fix; a genuinely-fresh clean build (548/548 objects) re-verified after the whole batch.
- **Opposite-direction follow-up** (same day, after the 4 fixes above): does the reader correctly consume everything the writer emits — matching key, field, and wire type, not just "is every field covered somewhere"? Checked mechanically via a script (not a fork read) across all 29 `writeXxx`/`readXxx` function pairs: key-name symmetry (186/186 match), field-assignment correctness (155 checked, all correct), and wire-type consistency (0 mismatches). **No further gaps found** — see `plan.md` §S22's addendum for the full method and the handful of script false-positives that were manually ruled out.

**2026-07-10 — `mc3` format spec-vs-implementation audit (`plan.md` §S21, `STAB-0651`–`0657`, all 7 completed).** Systematic comparison of `mc3/mc3.xsd` and `MC3_FORMAT.md` against the actual parser/writer/data model. Overall finding: coverage is strong (no data-loss bugs of the class previously fixed in this project); 7 narrower gaps, all fixed:
- **`STAB-0651`/`0652`**: `mc3.xsd` was missing the `sides` attribute (`crossSectionType`) and `cx`/`cy`/`cz` attributes (`pointType`) that the writer already emitted for polygon/star cross-sections and bezier paths — both fully-implemented, documented features that silently failed `xsd_validation`. New fixture `test/extrude_sides_bezier.mc3.xml`.
- **`STAB-0653`**: the initial plan was to delete the root `<mc3 default_camera="...">` attribute as dead code — **that plan was wrong**, caught before committing. A repo-wide grep found 13 real fixtures using it (`blupi_car.mc3.xml`, `speedy_blupi_world.mc3.xml`, etc.), and in the two multi-camera ones it was only working by *coincidence* (the named camera happened to be first in document order, matching the parser's empty-default fallback) — a real latent bug. Implemented it properly instead: root attribute sets the default first, `<cameras default="...">` overrides if present, first-camera fallback only if still empty.
- **`STAB-0654`**: `mip_maps` texture attribute (schema-declared, documented) had no field anywhere in the data model — real, permanent data loss on every load. Added and round-trip tested.
- **`STAB-0655`**: `version` attribute default was `"0.1"` in the parser/model but `"0.3"` in the XSD — a genuine 3-way inconsistency (also self-contradicting within `MC3_FORMAT.md` itself). Fixed to `"0.3"`; caught by the existing golden-byte test, which needed its own fixture updated (correctly — verified it was the only byte diff).
- **`STAB-0656`**: `<uv_mapping>` writer wasn't gated by object type, so a Group/Instance/Area object with `uvMapping` set (not reachable via the editor UI today, but not structurally prevented) would produce schema-invalid XML. Gated it.
- **`STAB-0657`**: deleted `mc3/MC3_FORMAT.md`, an orphaned, stale (3-week-old) duplicate of the root `MC3_FORMAT.md` that actively misled (documented `mip_maps` as working, contradicted the current doc). Added the missing `star` cross-section type to the canonical doc.
- Full 66/66 ctest after every fix; a genuinely-fresh clean build (548/548 objects) re-verified after the whole batch.

**2026-07-09 — `plan_deep_audit.md` follow-up audit phase** (new plan file, superseding `plan.md` for new work since that 650-task plan's own backlog was exhausted): a fresh repository-wide audit (5 parallel review passes + direct investigation) produced 51 real, file:line-verified tasks; 48 completed, 5 `needs_human` (product/policy decisions, not attempted), 2 `blocked` (external — see below). Highlights:
- **Real bugs fixed**: CSG preview cache key omitted `csgOperation`/`extrude`/`meshSource`, causing a stale/wrong preview after certain edits (the highest-risk finding — a *visibly plausible but wrong* rendering bug). `mc3`/`mcb`/GLB save paths were not atomic (crash mid-write could corrupt the destination file) — now write-to-temp-then-rename. Editor silent-failure paths (autosave, recent-file-open, `ModelRegistry::save()`'s `sqlite3_step` result) now surface real errors instead of failing silently. Pivot-reset button was double-pushing an undo snapshot.
- **Hardening**: MCB reader's count-prefixed collection reads (32 sites, not just the 11 originally scoped `.reserve()` call sites) now have the same sanity-ceiling guard the 2026-07 string-length fix established.
- **Completed the ~127-site ImGui slider-clamp gap** across all 8 files flagged since the S8 stabilization pass (`ImGuiSliderFlags_AlwaysClamp`, with deliberately-unbounded fields like position/rotation/keyframe-value correctly left alone).
- **New build regression found and fixed**: the native Linux build was silently broken by upstream `../cna` API drift (`Viewport::X`/`Y` changed from public fields to property accessors, landed in CNA 2026-07-05) — masked by this session's incrementally-updated build dir until a full rebuild was forced; the same "stale build dir hides a real break" failure mode `NEXT.md` already warned about, recurring for a third time.
- **Docs**: `MC3_FORMAT.md`'s Torus/Capsule/Disk/Grid/Icosphere section expanded from one line to full attribute tables; `MCB_FORMAT.md` documents the no-version-migration limitation; a new `test/xsd_docs_diff.py` tool checks XSD-vs-docs coverage on demand; a primitive-geometry cross-check test (`mc3togltf/test/all_primitives_export_test.py`) now guards against silent divergence between the export-time and editor-preview-time primitive-geometry implementations, the same risk class as the CSG dual-path invariant.
- **Corrected several false positives** found by the initial audit fork sweep on manual re-verification (documented in `plan_deep_audit.md` itself rather than silently dropped): the fog divide-by-zero was already guarded; `<actions>` not merging from includes is a deliberate design choice (per `mergeInclude()`'s own comment), not an accidental gap; `MeshCraftApplication_UiMenuBar.cpp`'s sliders were already clamped in an earlier session.
- **New blocked item**: `AUDIT-0050` (implement real IDBFS mount for Emscripten config persistence) — code could be written without touching CNA/sharp-runtime, but can't be compiled/verified until the sharp-runtime Emscripten regression (below) is fixed upstream.

**2026-07-09 — round 2** (3 more targeted audit passes after round 1's 51 tasks closed out, run during an extended unattended session): 6 more tasks (`AUDIT-0056`–`0059`, plus this round's own doc entries), all completed. Highlights:
- **Real bug fixed**: the "Undo History" jump-to-arbitrary-step dialog didn't cap `redoStack_` the way `performUndo()`/`performRedo()` already do — jumping to the oldest of a full undo history could temporarily push it past the documented 20-entry cap.
- **Significant real gap found and fixed**: ~29 ImGui property-editing widgets (concentrated in `MeshCraftApplication_UiLeftPanel.cpp`'s light/environment-fog/material editors, and `MeshCraftApplication_Anim.cpp`'s action-duration/loop/keyframe-editing fields) set the "unsaved changes" dirty flag with **no corresponding `pushUndo()`** — the inverse of the AUDIT-0014 double-push bug: the user would see an unsaved-changes indicator but Ctrl+Z couldn't actually revert the edit. Every fix was individually verified against the file's own established correct pattern (`IsItemActivated()`-gated `pushUndo()` for continuous-drag widgets, to avoid pushing one snapshot per drag-frame) before being applied — several other flagged candidates turned out to be false positives (already correctly paired, just further apart in a long function than the audit's own mechanical lookback check covered) and were left untouched after verification.
- **Two clean, honest "no action needed" results**, documented rather than padded: a proactive sweep for further CNA/SHARP_RUNTIME API drift risk (mirroring the `Viewport` break) found no other real forward-risk sites in this repo's own source; a test-assertion-quality audit found the existing CTest suite's assertions are already ~95%+ "strong" (exact-value/structural), not a real gap area.
- **Re-verified no new CNA drift**: `../cna` landed another commit mid-session (`c4aa6f72`) — re-ran a full fresh clean build immediately after and confirmed still 548/548, 66/66, no new break.

**2026-07-10 — `plan.md` archival split + 4 owner decisions resolved.** `plan.md`'s 620 ✅-completed `STAB-XXXX` rows were moved out to a new `plan_20260710.md` archive (same S0-S20 section structure), leaving `plan.md` with just the 30 still-open rows (29 🟡 + 1 📋) plus all meta sections (Legend/Policy/Gates/Priority Order/Architecture Reference) — verified no id lost or duplicated (650 unique STAB-IDs, 620+30=650). The project owner then resolved 4 of `plan_deep_audit.md`'s 5 `needs_human` rows:
- **`AUDIT-0037`** (id-collision-across-includes policy): decided **warning + last-write-wins**. Implemented in `mc3/src/Mc3XmlParser.cpp`'s `mergeInclude()` — collisions between two different `<include>`d files now print a `Warning:` to stderr naming the id; the common main-doc-overriding-an-include pattern still doesn't warn (that's deliberate authoring, not a mistake). Test coverage added (`mc3_roundtrip`'s `testMaterialIdCollisionAcrossIncludes` now captures stderr and asserts the warning fires).
- **`AUDIT-0055`** (texture slots in the left-panel material editor): decided **intentional, comment only** — added a comment in `MeshCraftApplication_UiLeftPanel.cpp` confirming the scalars-only scope; no new UI.
- **`AUDIT-0039`/`0040`** (CI matrix scope): decided **keep current scope, no change** — not worth deciding `../cna` checkout policy or adding permanently-red jobs until `STAB-0650`'s PAT-rotation blocker clears anyway.
- **Still open**: `AUDIT-0038` (MCB version-migration policy) — not yet asked. The ~24 `plan.md` rows needing a live interactive display session are explicitly **deferred** — owner is not at a PC yet; revisit once they are (§8 item 4 below).
Full 66/66 ctest verified after the `AUDIT-0037` code change.

Full history: `git log --oneline`. Per-task detail for the audit phase (both rounds): `plan_deep_audit.md`. Per-task detail for the original 650-task plan: `plan.md` (open rows) and its archive `plan_20260710.md` (620 completed rows as of the split) — **note**: `plan.md` has since grown again as S21–S25 were added directly to it (102 `STAB-` rows in `plan.md` as of 2026-07-10, most now ✅/🟡; the 620-row archive split was a one-time housekeeping pass, not an ongoing policy — nothing was moved back out after S21-S25 landed). Plus `STABILIZATION_WORKLOG.md` (narrative + exact commands run, up to the archival split).

---

## 4. Current blocker / main problem

**Nothing blocks Linux development, building, or testing** — the build is clean and 86/86 tests pass. If forced to name the single most significant *known, unresolved* problem in the project right now, it is:

**The Windows (MinGW cross-compile) build of the full GUI editor does not complete.**

- **Failing command**: a MinGW cross-compile build (`x86_64-w64-mingw32-g++` toolchain) of the `MeshCraft` target.
- **Exact symptom / first failure**:
  ```
  /rv/data/development/github.com/openeggbert/cna/.../imgui_impl_opengl3.cpp:159:10:
  fatal error: GLES3/gl3.h: No such file or directory
  ```
- **Affected files/modules**: `../cna` (configures `-DIMGUI_IMPL_OPENGL_ES3` unconditionally for its `EASYGL` backend, regardless of target platform) — **outside this repo**, must not be modified without permission.
- **Additional failures found when building past the first one** (`ninja -k 0`), all also outside this repo, in `../sharp-runtime`:
  - `System/Net/Sockets/Socket.cpp:361` — `-Werror=unused-function`.
  - `System/Net/Sockets/UnixDomainSocketEndPoint.cpp` — `afunix.h`'s `ADDRESS_FAMILY` type not visible (MinGW header/include-order issue).
  - `System/Xml/XmlConvert.cpp` (via `CharUnicodeInfo.hpp:162`) — `-Werror=sign-compare`.
- **Suspected cause**: cross-repo API drift (CNA's Windows GL backend was never wired to a real GLES3-on-Windows solution; sharp-runtime's Windows networking code has 3 unrelated warnings-as-errors under this specific MinGW version).
- **What's already been tried / confirmed**: `CNA_ENABLE_NET=OFF` (this project's own fix) removes an unrelated, previously-masking failure but does not touch any of the above. The two **CNA-free** CLI tools, `mc3togltf.exe` and `mc3tomcb.exe`, **do** build and link successfully as real Windows PE32+ executables — confirmed via `file mc3togltf.exe`. Only the full ImGui/CNA-dependent GUI editor is blocked.
- **Not something to fix here**: all 4 failures are in `../cna` or `../sharp-runtime`. Fixing them needs the maintainer(s) of those repos.

**New blocker found 2026-07-09: the Emscripten (web) build no longer completes from scratch, either.**

- **Failing command**: `cmake --build cmake-build-web` (or a fresh `./build-web.sh`) against current `../sharp-runtime`.
- **Exact symptoms**: build fails deep in `../sharp-runtime` sources before ever reaching MeshCraft's own code. Two categories, both 100% inside `../sharp-runtime`:
  1. **16 distinct `-Werror` failures** under Emscripten's clang (unused-private-field, unused-parameter, unused-const-variable, `[[nodiscard]]`-ignored, non-virtual-dtor-delete) — spread across `System/Runtime/InteropServices/RuntimeInformation.cpp`, `System/Net/NetworkInformation/NetworkInterface.hpp`, `System/Net/Dns.cpp`, `System/Xml/XPath/XPathNavigator.cpp` (×6), `System/IO/FileStream.hpp`, `System/Net/Sockets/UnixDomainSocketEndPoint.cpp`, `System/Xml/XPath/XmlDocumentNavigator.hpp`. None of these appear under the native Linux GCC build (different compiler/warning set), and none were present as of the 2026-07-06 verified web build.
  2. **A hard compile error, not a warning**: `System/IO/FileSystemInfo.cpp:31` and `:90` — `error: no member named 'clock_cast' in namespace 'std::chrono'`. This is a genuine libc++/Emscripten-toolchain standard-library gap, not fixable by relaxing `-Werror`.
- **Cause**: `../sharp-runtime` gained new commits between the last verified web build (2026-07-06) and now — HEAD moved to `e5e38db` (2026-07-07), including a `System.Xml.XPath` merge that introduced most of the new warnings.
- **What still works**: the **last-known-good** `cmake-build-web/MeshCraft.{html,js,wasm}` from 2026-07-06 are untouched on disk and still load/run correctly in headless Chrome (used to investigate the canvas-sizing bug above) — only a *fresh* rebuild is broken.
- **Not something to fix here**: both failure categories are 100% inside `../sharp-runtime`. Fixing them needs that repo's maintainer(s) — same handoff as the MinGW failures above.

---

## 5. Known bugs and limitations

- **Confirmed, unfixed (out of this repo's scope)**: Windows GUI build fails (§4). Emscripten web build no longer builds from scratch (§4, new). Emscripten canvas renders blank — root cause now identified (canvas is 0×0, §2) but fix blocked on the build regression above.
- **Confirmed, unfixed (in scope, deliberately deferred — needs a product-scope decision, not a quick patch)**:
  - Merge Scene has no object-id collision handling (materials/textures/actions get suffixed on collision, objects don't) — `STAB-0289`.
  - No first-launch `prefs.ini` auto-creation (only written when the Preferences dialog is explicitly closed) — `STAB-0327`.
  - No registry-DB-path override mechanism (env var/prefs) — `STAB-0360`.
  - `<embeds>` map not merged from `<include>`d files (only from the main document) — `STAB-0092`.
  - No animation scale-time function exists — `STAB-0460`.
  - Web GLB export has no browser-download bridge (§2) — `STAB-0571`.
- **Confirmed, by design (not a bug)**: SVG texture rasterization stub-only; embedded-glTF references not resolved; Scripts/Triggers have full editor UI now (§3) but no runtime execution (no Lua interpreter, no event dispatch); two independent material-editing UIs exist (`Scene/PropertiesPanel.cpp` and `MeshCraftApplication_UiLeftPanel.cpp`) — a fix in one doesn't apply to the other (verified 2026-07-09 via `plan_deep_audit.md` AUDIT-0013: read both side by side, no undocumented drift found beyond the already-known design). `<actions>` also deliberately not merged from `<include>`d files (confirmed via `mergeInclude()`'s own comment — distinct from the genuinely-accidental `<embeds>` gap below). `rotation_units`/`euler_order` are export-only, resolved as won't-fix in the editor's own rendering 2026-07-10 (§3, `STAB-0701`) — a load-time warning covers the gap instead.
- **Needs verification (blocked on tooling, not known-bad)**: **44 `plan.md` rows** (up from ~15+, mostly the new §S24 items from 2026-07-10) need a live interactive display/mouse session this headless environment can't provide (curve-editor visibility, proportional-edit radius indicator, FPS counter, live drag-and-drop gesture, clicking through the new N1–N7 editor tabs to confirm they actually render/interact correctly, etc.) — genuinely untested either way, not confirmed broken; each was implemented with high code-review confidence and mirrors an already-working pattern elsewhere in the file.
- **Fixed 2026-07-10** (this session, §3): a real, user-reported live-preview rendering bug — `SceneRenderer_Builders.cpp` wound 5 of 7 unit-shape builders (Box/Cone/Torus/Capsule/IcoSphere) backwards for CNA's actual culling convention, causing backface-culled faces to show the mirrored interior of the opposite side ("3 of 6 walls transparent" on box-heavy scenes like the castle). Also: CSG preview now warns when it silently drops unsupported content; orthographic cameras can export a non-square view volume (`Mc3Camera::orthoAspect`, new field).
- **Fixed 2026-07-09** (`plan_deep_audit.md` follow-up audit — see §3): all ~127 ImGui slider/drag call sites across 8 files now have `ImGuiSliderFlags_AlwaysClamp` where meaningful (deliberately-unbounded position/rotation/keyframe-value fields correctly excluded); CSG preview cache-key bug fixed (cache omitted `csgOperation`/`extrude`/`meshSource`, causing stale previews after certain edits); MCB reader hardened against unbounded collection counts (32 sites, not just the original 11 `.reserve()` call sites); `mc3`/`mcb`/GLB save paths made atomic (temp-file + rename); several editor silent-failure paths fixed (autosave, recent-file-open, ModelRegistry `sqlite3_step`); a real, previously-unnoticed native-Linux build break from upstream CNA API drift was found and fixed (`Viewport::X`/`Y` field→property change). Full detail: `plan_deep_audit.md` (56 tasks completed, 1 `needs_human`, 2 `blocked`, as of 2026-07-10 — see §3).

---

## 6. Architecture notes

- **CSG dual-path invariant**: `mc3togltf/src/CsgEvaluator.cpp` (export) and `SceneRenderer`'s CSG preview cache (editor) both key off `isCutter`/`role="cutter"` on child objects. A missing cutter flag silently turns a subtraction into a union — this has bitten real test fixtures before. Both paths must be kept in sync if CSG semantics change.
- **Primitive dual-path invariant** (same risk class as CSG, found in `plan_deep_audit.md` AUDIT-0011): `mc3togltf/src/MeshBuilder.cpp::buildPrimitive()` (exact per-type triangulation, used for export and CSG evaluation) and `SceneRenderer`'s primitive dispatch (`SceneRenderer.cpp:635-720`, unit-mesh + scale + LOD) are two independent geometry-generation implementations of all 11 `PrimitiveType` values (Box/Cube/Sphere/Cylinder/Cone/Plane/Torus/Capsule/Disk/Grid/IcoSphere). Both currently agree on which types exist and their basic shape, but there is no automated cross-check that they stay visually/dimensionally consistent as either is changed independently — see `plan_deep_audit.md` AUDIT-0012 for a proposed bounding-box invariant test.
- **Triangle winding differs by convention between the two geometry paths above — do not "fix" one to match the other without checking which convention it actually needs.** `mc3togltf/src/MeshBuilder.cpp` (glTF export) correctly targets **CCW-from-outside** (the glTF/OpenGL-textbook convention). `src/MeshCraft/Renderer/SceneRenderer_Builders.cpp` (editor live-preview) needs **CW-from-outside**, because CNA's real default `RasterizerState` is `CullCounterClockwiseFace` (true XNA/D3D9 semantics — confirmed via CNA's own test suite). Getting this backwards doesn't crash or look "obviously wrong" from a distance — it backface-culls the correct face and shows the mirrored interior of the opposite face instead, which looks like solid geometry gone see-through/transparent (exactly the castle bug fixed 2026-07-10, §3/§5). Verify winding with: face normal = `cross(p1-p0, p2-p0)`, dot against the shape's known outward normal — positive = CCW-from-outside, negative = CW-from-outside.
- **`Alg` mirror pattern**: pure logic extracted into CNA-free headers so it's headlessly unit-testable. Most mirrors are the single source of truth their real `.cpp` calls into — but a few (documented in the headers themselves) are intentionally-unwired parallel duplicates. Always check whether a given `Alg` function is actually called by the real code before assuming a fix there takes effect in the app.
- **Undo/redo**: snapshot-based, not command-diff-based.
- **`mc3.xsd` is compiled into the binary at CMake configure time** (embedded header generation) — editing the XSD requires a reconfigure, not just a rebuild.
- **XML comment gotcha**: a literal `--` anywhere inside an XML comment is rejected by `lxml`/`test/validate_xsd.py`, even though the app's own parser (`tinyxml2`) tolerates it. Always run `test/validate_xsd.py` on new `.mc3.xml` fixtures before trusting them.
- **`AiAssistant` threading invariant**: background HTTP work runs on a **detached `std::thread`** writing into a `shared_ptr<AiRequestResult>` (atomic `done` flag + mutex-guarded fields) — never `std::async`/`std::future`, whose destructor blocking behavior previously caused an app-hang-on-close bug. Do not reintroduce `std::async` here.
- **`CNA_ENABLE_NET` must stay `OFF`** in this project's `CMakeLists.txt` (see §4) — it disables an entirely unused CNA subsystem (`CNA_GamerServices`/`CNA_Net`, never linked by anything in this repo) that currently fails to compile on the CNA side. Re-enabling it will break the default `ninja` build again.
- **API/compatibility boundaries that must remain stable**: `Mc3Document`'s public API is depended on by `mc3togltf` and every test fixture — check both before changing it. CNA and SHARP_RUNTIME source must not be modified without explicit owner permission.

---

## 7. Useful commands

```bash
# --- Configure + build (Debug). MUST use CLion's bundled cmake, not system cmake
# (system cmake has a documented reconfigure bug for this project).
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -G Ninja
cd cmake-build-debug && ninja -j$(nproc)

# --- Test
ctest --output-on-failure          # full suite (86)
ctest -N                           # list all registered tests
ctest --print-labels                # ai / commands / export / format / registry / render
ctest -R mc3_ai --output-on-failure           # AI integration
ctest -R mc3_registry --output-on-failure     # ModelRegistry
ctest -R gl_state_leak --output-on-failure    # GL error-leak check
ctest -R blender --output-on-failure          # 3 real-headless-Blender tests

# --- XSD validation of a new/changed fixture
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# --- Run / demo
./cmake-build-debug/MeshCraft test/house.mc3.xml                     # interactive
./cmake-build-debug/MeshCraft test/house.mc3.xml --screenshot out.ppm  # headless smoke
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
./cmake-build-debug/mc3tomcb/mc3tomcb test/house.mc3.xml /tmp/out.mcb

# --- No linter/formatter is configured for this project.

# --- Reproduce the truly-clean-build check (see §4/§6 for why this matters)
rm -rf cmake-build-debug
# ...then reconfigure+build as above; should exit 0.
```

---

## 8. Next smallest tasks

Ordered, each scoped to one focused session:

1. **Verify whether the repo owner has rotated the CI PAT yet; if so, activate CI.**
   Files: `.github_/workflows/ci.yml` → rename to `.github/workflows/ci.yml`.
   Verify: push a trivial commit and confirm the Actions tab actually runs and reports a consistent result (`STAB-0650`).

2. **Report the cross-repo build failures (§4) to the CNA/sharp-runtime maintainer(s)** — now 2 separate issues: the original 4 MinGW failures, plus the newly-found Emscripten/sharp-runtime regression (16 `-Werror` + 1 hard `clock_cast` error, `../sharp-runtime` HEAD `e5e38db`).
   Files: none in this repo — this is a communication/handoff task, not code. Include the exact errors from §4.
   Verify: N/A (external action).

3. **Once `../sharp-runtime`'s Emscripten build regression (§4) is fixed upstream, retry the canvas-sizing fix.** The blank-canvas root cause is now known (`<canvas width="0" height="0">`, §2/§5, `plan.md` STAB-0553) — next step is a fresh `./build-web.sh`, then trace why SDL3's Emscripten backend isn't applying CNA's requested 1024×768 to the DOM canvas (likely needs an explicit `emscripten_set_canvas_element_size()` call or SDL hint from MeshCraft's own init code — CNA/SDL3 itself must not be modified without permission).
   Files: likely `src/MeshCraft/MeshCraftApplication.cpp` or `main.cpp`, if a mesh-craft-side workaround is possible without touching CNA/SDL3.
   Verify: `./build-web.sh`, serve via `python3 -m http.server`, load in `google-chrome --headless=new --enable-unsafe-swiftshader --use-gl=angle --use-angle=swiftshader --dump-dom <url>`, confirm `<canvas>` has nonzero `width`/`height`; then screenshot and inspect for non-black 3D content.

4. **Likely the highest-value next task if the owner is at a PC: a live interactive editor session to verify the 44 `plan.md` rows flagged 🟡** (implemented, code-reviewed, but never click-tested — this headless environment can't drive ImGui mouse/drag interaction). The bulk (14 rows) are the brand-new §S24 N1–N7 editor tabs added 2026-07-10 (SVG Textures/Embeds/Scripts/Audio/Triggers/Scene States, IcoSphere subdivision slider, primitive Axis combo, coordinate_system combo) — these are the most likely to have a real ImGui-interaction bug (ID-stack collision, `SameLine()` layout break, etc.) simply because they're newest and never seen a real display. The rest are older rows: gizmos, SSAO/bloom/wireframe toggles, drag-drop, curve editor, etc. Plan: owner runs the editor, Claude walks through each check step by step and records the result directly in `plan.md`.
   Files: `plan.md` — search for `🟡` to find all 44 rows; the newest batch is `STAB-0703`–`0721` (minus `0715`/`0720`/`0721` which are already ✅) plus `STAB-0672`.
   Verify: N/A until the session happens.

5. **Pick one of the remaining flagged product-decision rows (`STAB-0092`/`0289`/`0327`/`0360`/`0460`) and get an explicit scope decision from the project owner**, then implement only that one. (`STAB-0571` needs the Emscripten build fixed first — see item 3.)
   Files: varies per row — see `plan.md` for the specific row's "Key File(s)" column.
   Verify: whatever test the chosen row's own `plan.md` entry specifies.

6. **Once `../sharp-runtime`'s Emscripten regression clears (see item 3), also implement `plan_deep_audit.md` AUDIT-0050** (real IDBFS mount/syncfs for web config persistence — currently `-lidbfs.js` is linked but never actually invoked, so prefs/recent-files/keybindings silently vanish on every web reload). Same blocker as item 3, different fix.
   Files: `src/MeshCraft/MeshCraftPrivate.hpp`, web init path, `CMakeLists.txt:347`.
   Verify: see `plan_deep_audit.md` AUDIT-0050 for the exact steps.

7. **Get the project owner's decision on `plan_deep_audit.md`'s last remaining `needs_human` row, `AUDIT-0038`** (MCB version-migration policy — currently hard-fails on any version mismatch, by design; decide whether to ever invest in a migration path). `AUDIT-0037`/`0039`/`0040`/`0055` were already resolved 2026-07-10 (see §3).
   Files: none — communication/handoff task.
   Verify: N/A (external decision).

---

## 9. Do not do yet

- **No new product features** until the project owner explicitly says the current stabilization state (676/722, rest flagged 🟡/📋) is sufficient to resume feature work. Per-task-approved stabilization/coverage work (like §S24, all individually approved via `CLAUDE.md`'s workflow) is not the same as unrequested feature work — don't conflate the two, but also don't start a new §S24-style batch without the same per-task approval discipline.
- **No CNA or SHARP_RUNTIME source changes** without explicit owner permission — this includes the GLES3 header gap and the 3 sharp-runtime `-Werror` issues in §4, even though the fixes are individually easy to guess at.
- **No `Mc3Document` public API changes** without checking `mc3togltf`, `mc3tomcb`, and every test fixture that touches it.
- **No mass refactor or blanket fix** for the ~100+ un-clamped ImGui slider sites (§5) — each needs its own downstream-safety check first.
- **No speculative implementation** of any of the 6 flagged product-decision rows (§8 item 5) without first getting the scope decision — guessing at scope risks building the wrong thing.
- **No SVG rasterization or `embed:`/`<embeds>` resolution work** without an explicit decision on which library/approach to use.
- **No re-running the full "delete cmake-build-debug and rebuild from scratch" verification** unless a meaningful amount of new work has landed since the last clean-build check — it would just re-confirm the same pass with no new information (incremental `ninja` + `ctest` after each commit is sufficient; this was done after every commit in the 2026-07-10 §S23/§S24/§S25 batch).

---

## 10. Resume prompt

```
Read NEXT.md first, in full. Then inspect only the files needed for the
one task you're picking up — do not read or touch unrelated parts of the
codebase, and do not refactor anything you weren't asked to change.

Pick the next smallest task from NEXT.md section 8 (or, if none of those
fit what the user actually asked for, scope a new task down to something
similarly small and testable before starting).

Make one small, verified improvement. After making it, run the relevant
build/test command from NEXT.md section 7 and confirm it passes before
considering the task done. Do not mark anything as fixed/done without
that verification.

When finished, update NEXT.md: refresh section 2 (current status) and
section 3 (recent changes) with what actually changed, move the
completed task out of section 8, and update the commit hash in the
header. Keep the update factual and concise — do not invent progress
that wasn't actually verified.

Current branch: develop, in sync with origin/develop at commit 6db484c.
No new feature work without explicit owner authorization (see section 9).
```
