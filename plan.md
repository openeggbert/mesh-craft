# MeshCraft Stabilization Master Plan

_Generated: 2026-06-27 from full codebase + test audit. Replaces previous 100-task feature plan._

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Verified done — test exists, runs, passes |
| 🟡 | Implemented but needs hardening / additional tests |
| 🔴 | Known bug or broken behavior |
| 📋 | Planned — not yet implemented |
| 🧪 | Needs test — code may exist; coverage absent |
| ⛔ | Blocked on dependency |

---

## Current Policy

1. **No new features until stabilization gates S0–S6 are fully green.**
2. Every task that changes behavior must add or update tests.
3. A task may only be marked ✅ after: test exists, test is registered, test was run, result is documented.
4. Claude Code must ask the user before starting any task group (per CLAUDE.md).
5. Do not modify `../cna` — managed by a separate Claude Code instance.
6. Do not add `${meta-gl_SOURCE_DIR}/include` to CMakeLists.txt.
7. Do not change `Mc3Document` public API without checking `mc3togltf` and all test XMLs.

---

## Stabilization Gates

| Gate | Name | Required tasks |
|------|------|----------------|
| **Gate 0** | Build gate | STAB-0001–0025 green |
| **Gate 1** | Format gate | STAB-0066–0150 green |
| **Gate 2** | Export gate | STAB-0151–0260 green |
| **Gate 3** | Editor safety gate | STAB-0261–0335 green |
| **Gate 4** | Registry/AI gate | STAB-0336–0410 green |
| **Gate 5** | Large scene gate | STAB-0411–0470 green (subset) |
| **Gate 6** | Documentation gate | STAB-0576–0650 green |

---

## Known Test Suite (as of 2026-06-27)

15 CTest tests registered, all passing:

| CTest name | What it tests |
|------------|--------------|
| `smoke_test` | MeshCraft binary starts, opens test file, exits cleanly |
| `xsd_validation` | Python lxml validates all `test/*.mc3.xml` against `mc3/mc3.xsd` |
| `mc3_registry` | ModelRegistry SQLite open/save/search/remove/migration |
| `mc3_roundtrip` | Parse → save → reload → compare for all XML features |
| `mc3_commands` | 57 unit tests for EditorAlgorithms (rename, find-replace, array-dup) |
| `mc3togltf_gltf` | Animation + house export, GLB magic bytes, glTF validity |
| `mc3togltf_all_primitives` | All primitive types export without error |
| `mc3togltf_export_verification` | Verify every node has mesh, node names, material names |
| `mc3togltf_large_scene` | Static large_scene.mc3.xml exports correctly |
| `mc3togltf_large_scene_generated` | Python-generated 200-object scene exports |
| `mc3togltf_csg_strict` | CSG without `--allow-approximate-csg` fails hard |
| `mc3togltf_csg_export` | CSG union/difference/intersection export correctly |
| `mc3togltf_csg_unsupported` | Unsupported CSG child type detected and reported |
| `mc3togltf_instance_deform_cache` | Deform-specific cache keys produce separate meshes |
| `mc3togltf_float_cache_key` | Close float dimensions don't collide in cache |

---

> **2026-07-10:** All 620 ✅-completed STAB-XXXX tasks have been moved out to [`plan_20260710.md`](plan_20260710.md) to keep this file focused on the 30 remaining open items (29 🟡 + 1 📋). Section headers below are kept for all S0-S20 even where every task in that section is now closed, so cross-references and the Stabilization Gates table above still resolve. See the archive file for full history of completed work.

---

## S0 — Baseline Build and Reproducibility

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0012 | 🟡 | P2 | Verify MinGW build from Linux (cross-compile) | `CMakeLists.txt` | Duplicate of STAB-0552 (identical ask, predates the S16 section) — see that row for the current, re-verified (2026-07-07) picture: the CNA-side GLES3 header gap plus 3 separate sharp-runtime-side `-Werror` failures block the full `MeshCraft.exe` GUI editor, but the two CNA-free CLI tools (`mc3togltf.exe`/`mc3tomcb.exe`) now confirmed build and link successfully. Runtime-lib-copying specifically (the row's "runtime libs copied" clause) *is* confirmed working — see STAB-0566/0567. |

---

## S1 — Test Infrastructure

_All 40 tasks in this section are ✅ complete — moved to `plan_20260710.md`._

---

## S2 — MC3 XML Schema, Parser, Writer, and Roundtrip

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0092 | 🟡 | P2 | Define include-tracking policy for `embeds` map (N2) | `mc3/src/Mc3XmlParser.cpp` | **Confirmed real, accepted limitation — not fixed.** Unlike textures/materials/definitions, `mergeInclude()`'s own comment explicitly scopes it to "definitions/materials/textures from one included file" — `<embeds>` is never merged from an included file at all (only parsed from the main document's own top-level `<embeds>` section). This means a `<definition>` merged from an include, if it references `<mesh src="embed:xyz"/>` where `xyz` is declared in *that same included file's* `<embeds>` section (not the main document's), would silently fail to resolve — a real, if narrow, gap (embedding glTF data specifically inside a shared/included asset-library file). Not fixed this session: implementing `includedEmbeds` tracking + a `<embeds>` merge block in `mergeInclude()` is a contained but non-trivial addition (mirrors the definitions-merge pattern), and this specific combination (embed-in-an-included-library) has no existing test fixture or reported real-world use — flagged 🟡 as a documented, accepted limitation rather than attempted speculatively. |

---

## S3 — MCB Binary Format Stability

_All 30 tasks in this section are ✅ complete — moved to `plan_20260710.md`._

---

## S4 — glTF/GLB Exporter Correctness

_All 50 tasks in this section are ✅ complete — moved to `plan_20260710.md`._

---

## S5 — CSG Stability

_All 35 tasks in this section are ✅ complete — moved to `plan_20260710.md`._

---

## S6 — Geometry Reuse, Instancing, Large Scenes

_All 25 tasks in this section are ✅ complete — moved to `plan_20260710.md`._

---

## S7 — Editor Save/Load/Data-Loss Workflows

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0289 | 🟡 | P2 | Verify Merge Scene handles ID collision (suffix appended) | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | **Real gap found, documented, not fixed (flagged for follow-up).** `mergeDocumentsAlg()` suffixes colliding *keys* for textures/materials/actions (all maps) but has **no id-collision handling at all for objects** — `src.objects` are unconditionally appended via `push_back`. Added `testMergeSceneObjectIdCollisionNotHandled()` proving this empirically: merging two documents that both use object id `"a"` leaves two objects sharing that id in the destination, silently. Not a data-loss bug (objects live in a `std::vector`, not a map, so nothing gets overwritten) — the risk is anything that looks an object up *by id* post-merge (e.g. `Mc3SceneState`/`Mc3ObjectOverride`) resolving ambiguously. A correct fix needs to walk the entire source object subtree (ids exist on every nested child, not just top-level objects) and rename any id colliding with the destination's existing tree — materially more work than the flat-map suffix idiom, so left as a documented, test-proven gap rather than a speculative fix. |

---

## S8 — UI Robustness

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0327 | 🟡 | P2 | Verify preferences dialog: INI file created on first launch | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | **Row's premise doesn't match reality — corrected, not fixed.** `savePrefs()` has exactly one call site: the Preferences dialog's own "Close" button (`MeshCraftApplication_UiOverlays.cpp`). There is no auto-save on app exit or "first launch" in the sense the row assumes — `prefs.ini` is only ever written when the user explicitly opens Preferences and closes that specific dialog. Flagged 🟡 rather than ✅ since the row asks to *verify* a behavior that doesn't exist; whether "create prefs.ini on first launch regardless of whether Preferences was opened" is desired behavior is a product decision, not something to implement speculatively. |

---

## S9 — ModelRegistry Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0360 | 🟡 | P3 | Verify registry DB path config (alternative path via env var or prefs) | `src/MeshCraft/ModelRegistry.cpp` | **Investigated; nothing to document — no override capability exists.** `defaultPath()` only reads `$HOME`/`$USERPROFILE` to compute the *default* location; there is no env var, prefs field, or CLI flag that lets a user redirect it. `ModelRegistry::open(path)` does accept an explicit path parameter at the C++ API level, but the app itself always calls `registry_.open(ModelRegistry::defaultPath())` — nothing in the UI exposes a custom path. Flagged 🟡 rather than ✅: whether to add a real override mechanism is a product decision, not something to implement speculatively during a documentation-verification task. |

---

## S10 — AI Integration Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0389 | 🟡 | P1 | Verify `MESHCRAFT_HAS_AI` guard: AI panel not shown when undefined | `src/MeshCraft/MeshCraftApplication_UiMenuBar.cpp` | **Corrected premise**: the "AI Assistant" menu item (`MeshCraftApplication_UiMenuBar.cpp:569`) has no `#ifdef MESHCRAFT_HAS_AI` guard at all — it's always compiled and always shown, even in a build without cpp-httplib/OpenSSL (confirmed: no `ifdef`/`ifndef` anywhere in that file). This is intentional, not an oversight: `CMakeLists.txt:220`'s own status message says "AI Assistant will be disabled at runtime" (not "hidden"). Clicking Send in such a build hits `AiAssistant.cpp`'s `#else` stub branch, which throws immediately and — after this session's STAB-0387/0388 refactor — is caught by `sendAsync()`'s `try/catch` and surfaced cleanly via `hasError()`/`errorMsg()` ("AI not available: built without cpp-httplib + OpenSSL..."), not a crash. Leaving this flagged rather than ✅ since it contradicts the row's literal expectation, even though the actual (visible-but-self-explaining) behavior is a reasonable, deliberate design choice. |

---

## S11 — Materials, Textures, and Visual Fidelity

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0423 | 🟡 | P2 | Verify material preview sphere uses correct coordinate system | `src/MeshCraft/MeshCraftApplication.cpp` | **Corrected premise**: the preview shader (`kMatPreviewFS`, `MeshCraftApplication.cpp:1151-1177`) is a single-directional-light SDF Blinn-Phong sphere with **no environment/reflection map at all** — there is nothing for a "mirror" to reflect, so a literal mirror-like appearance (reflecting the surrounding scene/UI) is not physically achievable with this shader design, regardless of metallic/roughness values. What the shader *does* do correctly, confirmed by reading the math: `diffuse = u_color * (1.0 - u_metallic)` correctly goes to 0 at `metallic=1` (metals have no diffuse term), `f0 = mix(0.04, u_color, u_metallic)` correctly tints the specular color by the base color at full metalness (vs. metals's `0.04` grey dielectric F0), and `shininess = mix(128.0, 2.0, u_roughness)` correctly tightens the specular highlight to a small, sharp point at `roughness=0`. So `metallic=1/roughness=0` does produce the closest achievable approximation (a tight, colored specular highlight on a near-black base) — but not a literal mirror reflection, since no IBL/cubemap exists in this simplified preview. Flagging rather than closing since the row's literal expectation isn't met, even though the underlying shading math is directionally correct. |

---

## S12 — Animation Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0460 | 🟡 | P2 | Verify animation scale-time function | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Confirmed genuinely missing, not a bug: grepped `MeshCraftApplication_Anim.cpp` and the whole `src/`/`include/` tree for any "scale time"/"scaleTime"/keyframe-time-scaling feature — no code implements this at all (the row assumed a feature that was never built). Building it now would be new-feature work, out of scope during this project's stabilization-only feature moratorium (`plan.md`'s own hard constraint, same policy already applied to STAB-0427/D3). Flagged, not resolved — needs a future feature-planning decision, not a stabilization fix. |
| STAB-0464 | 🟡 | P1 | Verify curve editor: interpolation curve visible per channel | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Confirmed the drawing code exists (`MeshCraftApplication_Anim.cpp:673-688`, "Mini curve — sample `evaluateChannel` across track width", using the same shared `evaluateChannel()` already verified correct in STAB-0461) — but whether it's actually *visible and correct on screen* is a rendered-pixel question, genuinely blocked in this headless environment (same constraint as STAB-0421-0423/0428/0617). Flagged, not resolved — needs a human at a live display. |

---

## S13 — Commands, Undo/Redo, and Algorithms

_All 25 tasks in this section are ✅ complete — moved to `plan_20260710.md`._

---

## S14 — Rendering and Viewport Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0501 | 🟡 | P1 | Verify gizmo: translate gizmo appears on selected object | `src/MeshCraft/Renderer/SceneRenderer_Gizmos.cpp` | Confirmed by code reading that `drawGizmo()` is correct and safe (null-obj guard, correct per-axis line + drag-handle-cube math at the object's position, X/Y/Z consistently red/green/blue). Its call site (`MeshCraftApplication.cpp`) correctly gates it on `selection_.hasSelection() && activeTool_ == ActiveTool::Move`. But **actually seeing it requires both a live mouse-click selection and switching to the Move tool** (default `activeTool_` is `Select`, not `Move`) — neither is reachable through the headless `--screenshot` CLI path used for S14's other items, and adding a debug-only CLI selection hook would be scope creep beyond what this verification task asks for. Flagged 🟡, same category as S11/S12's blocked visual items (STAB-0421-0423/0428/0464/0617) — needs a human with a live display. |
| STAB-0502 | 🟡 | P1 | Verify gizmo: rotate gizmo appears on right mode | `src/MeshCraft/Renderer/SceneRenderer_Gizmos.cpp` | Confirmed by code reading that `drawRotateGizmo()` is correct and safe (null-obj guard, draws 3 32-segment circles around the object's position, one per axis plane, consistently red/green/blue for X/Y/Z — same color convention as the translate gizmo). Its call site correctly gates the draw on `selection_.hasSelection() && activeTool_ == ActiveTool::Rotate`, and `R` (`tool.rotate` keybinding) correctly sets `activeTool_ = ActiveTool::Rotate`. Same wall as STAB-0501: actually seeing it needs a live selection + the R keypress, neither reachable through the headless `--screenshot` path. Flagged 🟡, same category as STAB-0501/S11/S12's blocked visual items. |
| STAB-0505 | 🟡 | P1 | Verify bounding box toggle: AABB visible for selected | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Confirmed by code reading that `drawObjectWireframe()` is correct and safe: for non-primitive types it defaults to a unit-size box (matching the STAB-0503 fallback convention), and for each primitive type it scales the correct wire shape (box/sphere/cylinder/cone/plane/torus/capsule/disk/grid/icosphere) to the object's actual dimensions. The cyan bbox overlay (`MeshCraftApplication.cpp`) is correctly gated on `showBoundingBox_ && selection_.hasSelection()` — note there's also a *separate*, always-on yellow selection wireframe built into `drawObject()`'s core path (`if (sel) drawObjectWireframe(...)`, independent of this toggle). Both need a live selection to actually see on screen, same wall as STAB-0501/0502 — flagged 🟡. |
| STAB-0508 | 🟡 | P1 | Verify SSAO toggle: off/on changes visual output | `src/MeshCraft/MeshCraftApplication.cpp` | Confirmed by code reading that `applySsao()` is a correct, safe 4-pass GL pipeline (depth blit → SSAO pass → blur → multiplicative composite, with null-function and zero-viewport guards, and proper GL state save/restore afterward). `ssaoEnabled_` defaults to `false` and is only ever flipped via an ImGui menu item (`MeshCraftApplication_UiMenuBar.cpp`) — checked whether it's part of the persisted prefs file (`loadPrefs()`/`savePrefs()`, `MeshCraftApplication_FileOps.cpp`) the way STAB-0507's orthographic-camera workaround was possible, but it isn't (only `autoSaveInterval`/`snapTranslate`/`snapRotate`/`snapScale`/`gridSpacing`/`theme` persist) — no CLI/document/prefs hook exists to force it on for a headless `--screenshot` run, same wall as STAB-0505's `showBoundingBox_`. Flagged 🟡. |
| STAB-0509 | 🟡 | P1 | Verify bloom toggle: off/on changes visual output | `src/MeshCraft/MeshCraftApplication.cpp` | Confirmed by code reading that `applyBloom()` is a correct, safe emissive-glow GL pipeline (guards on `!gl.ready`/zero viewport, proper GL state save/restore). This bloom pipeline was previously broken and fixed in an earlier session (see project memory, 2026-06-26): a VAO/VBO silent-failure bug plus CNA leaving `GL_CULL_FACE`/`GL_STENCIL_TEST`/`GL_SCISSOR_TEST` enabled after scene rendering, both fixed and empirically verified at the time via manual on/off pixel-diff comparison (1802 pixels changed, +57 avg brightness). Confirmed both fixes are still present in the current code (`BindVertexArray(0)` for every bloom draw call, explicit `Disable()` of all 3 states before the composite pass) — the historical verification still holds. `bloomEnabled_` defaults to `false`, only flipped via an ImGui menu item, and (like STAB-0508's SSAO) isn't part of the persisted prefs file — no CLI/document/prefs hook exists to re-verify it fresh in this headless environment. Flagged 🟡. |
| STAB-0510 | 🟡 | P1 | Verify wireframe mode: all objects rendered as wireframe | `src/MeshCraft/Renderer/SceneRenderer.cpp` | The main draw loop (`MeshCraftApplication.cpp`) correctly skips the solid pass (`if (!showWireframeMode_) sceneRenderer_->draw(...)`) and always draws the edge overlay when wireframe mode is on, matching "solid objects become wireframe" exactly. Audited `drawObjectEdges()` (`SceneRenderer_Extrude.cpp`) the same way STAB-0497 audited the solid draw path: exhaustively and safely handles all 19 `ObjectType` values — 11 primitives explicitly sized, 5 container types (Group/Area/Union/Intersection/Difference) recurse into children, Instance resolves via `resolvedInstanceDefinitionKey()` (STAB-0503's fix consistently applied here too), Extrude computes real profile-ring + spine wireframe geometry, and Mesh/any unknown type falls back to a safe placeholder box — no crash possible by construction. `showWireframeMode_` defaults to `false` and is only flipped via ImGui menu/toolbar, not part of the persisted prefs file — same no-headless-hook wall as STAB-0505/0508/0509. Flagged 🟡. |
| STAB-0514 | 🟡 | P2 | Verify delta overlay: shown while dragging gizmo | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Confirmed correct by code reading (`MeshCraftApplication_UiOverlays.cpp:95-155`): gated on `gizmo_.isDragging() && selection_.hasSelection()`, reads the correct transform component (position/rotation/scale) per `activeTool_`, computes `delta = curVal - gizmoDragStartVal_`, renders a colored axis label + signed delta + current value + rotate snap indicator. Traced all 3 drag-start sites in `MeshCraftApplication_Mouse.cpp` (Move/Scale gizmo-handle hit test at line ~360, Rotate gizmo-circle hit test at line ~418): `gizmoDragAxisIdx_`/`gizmoDragStartVal_` are always set together with `gizmo_.startDrag()`, so no stale/uninitialized read is possible — no bug found. Like STAB-0501/0502, this needs a live mouse-down-drag gesture over a gizmo handle; the headless `--screenshot` path renders one static frame with no simulated mouse-button/motion sequence, and no CLI/document hook exists to force `gizmo_.isDragging()` true (adding one purely for this would be scope creep, same as rejected for STAB-0505/0508/0509/0510). Flagged 🟡. |
| STAB-0515 | 🟡 | P2 | Verify render stats: vertex/face count updates on selection change | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Row's file citation is imprecise — `drawStatsOverlay()` in that file only shows whole-document `scenePolyStats()` totals, never selection-specific. The actual per-selected-object stat lives in `PropertiesPanel.cpp:1413-1422` (Geometry tab, "Poly stats (C6)"): `ctx.renderer->objectPolyStats(*sel0, v, t)`, where `sel0 = ctx.selection.selection().front()` is captured fresh at the top of the panel draw (`PropertiesPanel.cpp:37-38`) every single frame with no caching — so it's structurally guaranteed to recompute from the current selection on every draw; no bug is plausible. Confirmed `MeshCraftApplication.cpp`'s startup path (`newScene()`/`loadFromFile()`) never pre-selects anything, and there is no CLI flag or document attribute to force an initial selection — selection can only be set via a live mouse click (STAB-0503/0504) or an interactive menu action (`MeshCraftApplication_UiMenuBar.cpp`'s "Select All" etc.), none reachable from the headless `--screenshot` single-frame path. Same live-interaction blocker as STAB-0505/0514; adding a debug-only auto-select-on-load hook purely to make this testable would be the same scope creep rejected for STAB-0505/0508/0509/0510. Flagged 🟡. |
| STAB-0516 | 🟡 | P2 | Verify shadow map debug overlay: renders from first directional light | `src/MeshCraft/MeshCraftApplication.cpp` | Confirmed by code reading (`MeshCraftApplication.cpp:505-524`, `:1535-1551`, `MeshCraftApplication_UiOverlays.cpp:2077-2104`): correctly finds the first `Directional` light with `castShadows=true`, builds a light-space view/ortho-projection (±50m), renders the scene into a dedicated FBO via `renderShadowDebugFbo()` (which restores the framebuffer binding/viewport to the main screen afterward — no leak), and displays it in a "Shadow Frustum" ImGui window with the color texture image plus light name/direction. Gated on `shadowDebugEnabled_` (default `false`, `MeshCraftApplication.hpp:214`), only flipped via the "Shadow Map Debug" menu item — same no-CLI/document/prefs-hook wall as STAB-0508/0509/0510. Flagged 🟡. |
| STAB-0518 | 🟡 | P2 | Verify locked object outline: locked objects have distinct visual | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Row's file citation is imprecise — the actual draw-decision logic is `MeshCraftApplication.cpp:582-595`, not `SceneRenderer.cpp` (which only supplies the generic `drawObjectWireframe()` helper both this and the selection-bbox overlay call). Confirmed correct: gated on `!lockedIds_.empty()`, recurses through every object including children, draws a red wireframe (`Color(220,60,60,180)`) around each locked object via `sceneRenderer_->drawObjectWireframe()` — no bug found. `lockedIds_` (`MeshCraftApplication.hpp:394`, a `std::set<std::string>`) is a pure in-memory editor set with **no persisted `mc3.xml` attribute** (confirmed: no `locked` attribute anywhere in `mc3.xsd` or `Mc3Document`) — it's only ever populated via a keyboard shortcut (`MeshCraftApplication_Keyboard.cpp:186-187`) or a menu action (`MeshCraftApplication_UiMenuBar.cpp:394-395`), both live-interaction only. Same no-CLI/document-hook wall as STAB-0505/0514/0515/0516. Flagged 🟡. |
| STAB-0519 | 🟡 | P2 | Verify proportional editing falloff: visible sphere indicator | `src/MeshCraft/MeshCraftApplication_Mouse.cpp` | **Found and fixed a real gap**: no viewport visual indicator for `propEditRadius_` existed anywhere (only a toolbar slider/tooltip showing the number) — the underlying falloff math (`applyProportionalFalloffAlg`) was already correct and covered by 4 headless assertions (STAB-0489), but the row's own success criterion ("radius sphere drawn in viewport") was unimplemented. Added `SceneRenderer::drawWireSphereAt()` (`SceneRenderer.hpp`/`.cpp`, reusing the existing `wireShapeSphere_` unit-sphere wire geometry, same pattern as `drawObjectWireframe`) and wired it into `MeshCraftApplication.cpp`'s draw loop: whenever `propEditEnabled_ && propEditRadius_ > 0 && selection_.hasSelection()`, draws an orange wireframe sphere centered on the selection's average position (matching the exact center calculation `applyProportionalFalloffAlg` uses) with radius `propEditRadius_` — shown continuously while the mode is enabled, not just mid-drag, so the influence zone can be previewed before moving anything. Verified via rebuild (0 warnings) and 35/35 ctest (no regressions). Like STAB-0508/0509/0510, `propEditEnabled_` is a pure runtime toggle with no CLI/document/prefs hook (confirmed: not in the persisted prefs file), so the new indicator can't be pixel-tested via `--screenshot` — flagged 🟡 for a human with a live display to do the final visual confirmation. |
| STAB-0520 | 🟡 | P3 | Verify large scene FPS acceptable (200 objects, ≥ 30 FPS) | manual | Confirmed `test/large_scene.mc3.xml` exists and is already used by `mc3togltf_large_scene`. The stats overlay's `displayFps_` (`MeshCraftApplication.cpp:290`) is an exponentially-smoothed rolling average (`displayFps_ * 0.9 + instant * 0.1`) computed across many frames of a sustained interactive loop — conceptually incompatible with the single-frame `--screenshot` headless model (it would never converge, and the one-shot "instant" value is dominated by one-time setup cost, not steady-state throughput). This needs a human running the editor interactively and watching the live FPS counter — flagged 🟡, same as STAB-0505/0508/0509/0510/0514/0515/0516/0518/0519. |

---

## S15 — Import/Export/Editor Integration

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0532 | 🟡 | P1 | Verify OBJ import: Browse button opens file path entry | `src/MeshCraft/Scene/PropertiesPanel.cpp` | Traced the full path: `PropertiesPanel.cpp:1258-1268`'s Browse button calls `ctx.openMeshBrowse(sel0->meshSource)` (wired in `MeshCraftApplication_UiProperties.cpp:47-52`), which copies the current `meshSource` into `meshBrowseBuf_` and opens a modal (`MeshCraftApplication_UiOverlays.cpp:1906-1937`, "Mesh Source" popup, F2). Confirmed correct: no native OS file dialog — a plain ImGui `InputText` pre-filled with the current path; "Set" pushes undo and assigns `selection_.selection().front()->meshSource = meshBrowseBuf_` (the exact field STAB-0533's round-trip test just confirmed persists correctly to XML), guarded by a "No object selected" check (dead in practice — this popup is only reachable from the Properties panel, which only renders with a selection); "Cancel"/Escape closes without changes. No unique logic beyond a plain text-field assignment already covered elsewhere — genuinely needs a live UI session to click through the actual popup open/type/Set interaction, not a coverage gap. Flagged 🟡, same class as STAB-0505/0508/0509/0510/0514/0515/0516/0518/0519/0520/0521. |
| STAB-0539 | 🟡 | P2 | Verify editor drag-drop: drop `.mc3.xml` → loads scene | `src/MeshCraft/MeshCraftApplication.cpp` | Traced the full path: the SDL `SDL_EVENT_DROP_FILE` watcher (`:86-101`) routes image extensions to a texture-drop handler and everything else to `pendingDropFile_`; `Update()` (`:356-364`) then decides whether to actually open it as a scene. **Found and fixed an unwired duplicate** (same class as STAB-0534/0535's stale-mirror finding, but here the logic hadn't drifted, just wasn't reused): the extension check at `:360` duplicated `isDroppableScenePathAlg()`'s exact logic inline instead of calling it — rewired it to call the already-tested Alg function directly (`EditorAlgorithms.hpp` needed a new `#include` in this file). The routing logic itself is confirmed correct and already covered headlessly by `testDroppableScenePathDetection` (`mc3/test/editor_commands_test.cpp:1879-1891`, now the actual code path, not just a parallel mirror). The remaining piece — an actual SDL drag-and-drop gesture delivering a real `SDL_EVENT_DROP_FILE` event — is not something the headless `--screenshot` single-frame path can simulate. Verified via rebuild + 41/41 ctest (no regressions). Flagged 🟡 for the live end-to-end drop gesture, same class as STAB-0505/0508/.../0532. |

---

## S16 — Cross-Platform Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0552 | 🟡 | P1 | Windows (MinGW) build + run | `CMakeLists.txt` | **Substantial real progress, genuinely blocked past a certain point on cross-repo (out-of-scope) dependencies.** MinGW-w64 is installed in this environment. Original attempt: configure initially failed at `find_package(SQLite3 REQUIRED)` — **found and fixed this as a real bug, see STAB-0008** — after which configure succeeded cleanly. The build then reached `imgui_impl_opengl3.cpp: fatal error: GLES3/gl3.h: No such file or directory` (CNA-side: `-DIMGUI_IMPL_OPENGL_ES3` configured unconditionally for `EASYGL` regardless of target platform). **Re-verified 2026-07-07** with this session's `CNA_ENABLE_NET=OFF` fix applied (see "Post-650 Follow-Up Findings"): re-ran the full MinGW cross-compile from scratch. The GLES3 blocker is still present and unchanged (same file, same error) — but with `-k 0` (keep going past failures) three *additional*, separate `../sharp-runtime`-side `-Werror` build failures were found in files that hadn't previously been reached by the build scheduler: `System/Net/Sockets/Socket.cpp` (`-Werror=unused-function` on an internal helper), `System/Net/Sockets/UnixDomainSocketEndPoint.cpp` (`afunix.h`'s `ADDRESS_FAMILY` type not visible — an include-order/MinGW-header-version issue), and `System/Xml/XmlConvert.cpp` (via `CharUnicodeInfo.hpp`, `-Werror=sign-compare`). All three are entirely within `../sharp-runtime`, a sibling repo this project may not modify without permission — same boundary as CNA. **Genuinely new positive finding**: with `-k 0`, the build got far enough to show that the two CNA-free CLI tools — `mc3togltf.exe` and `mc3tomcb.exe` — **build and link successfully as real Windows PE32+ executables** (confirmed via `file mc3togltf.exe` → `PE32+ executable for MS Windows ... x86-64`), since neither links SHARP_RUNTIME or CNA at all. Only the full GUI editor (`MeshCraft.exe`) remains blocked. Flagged 🟡 with these specific, actionable (but out-of-scope) blockers — not a vague "untested". |
| STAB-0560 | 🟡 | P2 | Verify SDL drag-drop on Linux: `.mc3.xml` file drag works | `src/MeshCraft/MeshCraftApplication.cpp` | Confirmed the code path is correct: `sdlEventWatch()` (`:84-104`) classifies `SDL_EVENT_DROP_FILE` by extension, routing image files to `pendingDropTexture_` and everything else to `pendingDropFile_`; `isDroppableScenePathAlg()` (`EditorAlgorithms.hpp:1370-1373`, `path.extension() == ".xml" \|\| path.string().find(".mc3") != npos`) correctly matches `.mc3.xml`, routing it through `confirmIfModified(PendingAction::OpenRecentFile, ...)` — the normal open-file path, so a non-scene `.xml` would still fail gracefully at the real XML/schema validation stage, not silently misbehave. What's genuinely unverifiable headlessly is the actual OS-level drag gesture from a real file manager (Nautilus/Dolphin) generating a real `SDL_EVENT_DROP_FILE` — needs a live X11/Wayland desktop session, same class of gap as STAB-0505/0508/0509/0510/0514-0520/0428(pre-fix). Flagged 🟡, not a code defect. |
| STAB-0571 | 🟡 | P3 | Verify web export: exported GLB downloadable from browser | `CMakeLists.txt` | **Found a real gap.** Confirmed there is no browser-download bridge anywhere: no `EM_ASM`/`EM_JS` glue, no custom `--shell-file` (the build uses Emscripten's stock default shell template — confirmed via `grep`, no `.html` shell exists in the repo). `-sFORCE_FILESYSTEM=1` + `EXPORTED_RUNTIME_METHODS=['FS','ccall','cwrap']` (`CMakeLists.txt:334-335`) expose `Module.FS` to JS, so an exported GLB's bytes *do* land somewhere JS-reachable (Emscripten's virtual MEMFS) — but nothing ever reads them back out and constructs a `Blob`/`<a download>` to hand to the real OS. Practically: clicking "Export GLB" in the web build writes into an in-browser-tab-only virtual filesystem the user has no way to retrieve from. Building the missing download bridge is new-feature-sized work (JS glue + likely a custom HTML shell), out of scope during this stabilization pass's feature moratorium (same policy as STAB-0427/D3, STAB-0460) — flagged, not fixed. |
| STAB-0572 | 🟡 | P3 | Verify web SSAO: works on WebGL2 | `CMakeLists.txt`, `src/MeshCraft/MeshCraftApplication.cpp` | **Real progress beyond "manual"**: did a full from-scratch Emscripten rebuild (`./build-web.sh`, `emcc` 5.0.7 via `~/Downloads/emsdk`) and, going further than STAB-0553/0562's prior Node-only checks, actually served `MeshCraft.html` locally and loaded it in real headless Chrome (`--headless=new --enable-unsafe-swiftshader --use-gl=angle --use-angle=swiftshader`) — confirmed a genuine WebGL2 context initializes (on-page log: `EasyGLGraphicsBackend initialized with OpenGL OpenGL ES 3.0 (WebGL 2.0 (OpenGL ES 3.0 Chromium))`), no GPU/console errors, screenshot captured showing the app running, not crashed. This doesn't by itself confirm SSAO specifically, though: `ssaoEnabled_` is a pure runtime UI toggle (off by default, no CLI/URL-param/document hook — same class as STAB-0519/0520), and the web build has no argv-based scene-loading or UI-automation infrastructure, so actually toggling SSAO on and visually confirming the AO effect over real geometry needs either a live human session or future investment in browser UI automation (synthetic canvas click/drag dispatch). Flagged 🟡 with concrete, positive evidence rather than a blank "manual" placeholder. |
| STAB-0573 | 🟡 | P3 | Verify web bloom: works on WebGL2 | `CMakeLists.txt`, `src/MeshCraft/MeshCraftApplication.cpp` | Same real-browser evidence and same remaining gap as STAB-0572 (`bloomEnabled_` is the equivalent pure runtime toggle, off by default). The Emscripten build succeeds cleanly with the bloom shaders (`kBloomBlurFS`/`kBloomCompositeFS`, `#version 300 es`) compiled in, and a real headless-Chrome session confirms WebGL2 context creation with no errors — but visually confirming the glow effect itself needs bloom-enabled content loaded and the toggle switched on, which isn't reachable without a live session or UI automation. Flagged 🟡, not "manual, untested." |

---

## S17 — Documentation and User-Facing Honesty

_All 20 tasks in this section are ✅ complete — moved to `plan_20260710.md`._

---

## S18 — Code Quality and Architecture

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0617 | 🟡 | P3 | Audit large source files: `PropertiesPanel.cpp` > 2000 lines | `src/MeshCraft/Scene/PropertiesPanel.cpp` | Reviewed: unlike `SceneRenderer.cpp` (STAB-0618, already reasonably split), this file is a genuine smell — `PropertiesPanel::draw()` is a single ~1770-line function (lines 26-1796) with almost no internal section markers (just 2, at lines 997/1123), covering every object-type's property UI inline. Splitting it into per-section helpers (transform/material/geometry/etc., as the row suggests) is a real, low-risk *mechanical* extraction (pure function-boundary move, no logic change) — but ImGui code is exactly the kind of thing that's easy to subtly break on extraction (ID-stack pushes, `SameLine()`/layout ordering) with **no way to visually verify the panel still renders correctly in this environment** (no running display, viewport not integrated per `NEXT.md` §2). Genuinely blocked on the same class of constraint as the 3 S20 items needing a browser/Blender/CI — needs a human clicking through the live editor to verify after the split, not a missing tool. Not attempted this session; flagged 🟡 (confirmed real, deferred) rather than ✅ (confirmed fine) or 📋 (not looked at). |

---

## S19 — Security and Robustness

_All 15 tasks in this section are ✅ complete — moved to `plan_20260710.md`._

---

## S20 — Release Readiness

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0650 | 📋 | P3 | Verify CI produces consistent test report | `.github/workflows/ci.yml` | Blocked — CI is parked deactivated (`.github_/` not `.github/`, see `NEXT.md` §4's PAT issue) and cannot actually be *run* to verify its output without the repo owner rotating the token first. Could review the YAML for whether the *configuration* would produce a consistent report if activated, but that's a materially weaker check than what this row asks for. |

---

## S21 — MC3 Format Spec/Implementation Audit (2026-07-10)

_New section, added 2026-07-10 after a systematic comparative audit of `mc3/mc3.xsd` and `MC3_FORMAT.md` against the actual `mc3` library implementation (parser/writer/data model). Overall finding: coverage is strong and mature (all 19 object/CSG types, all N1-N7 extensions, the include-merge system all round-trip correctly) — no data-loss bugs of the class previously found/fixed in this project. 7 narrower gaps found, all verified directly against source before being added here (file:line cited in each row)._

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0651 | ✅ | P0 | `mc3.xsd`'s `crossSectionType` is missing the `sides` attribute that the writer actually emits | `mc3/mc3.xsd:243-263`, `mc3/src/Mc3XmlWriter.cpp:212,217`, `test/extrude_sides_bezier.mc3.xml` | **Fixed 2026-07-10.** Added `<xs:attribute name="sides" type="xs:positiveInteger"/>` to `crossSectionType`. New fixture `test/extrude_sides_bezier.mc3.xml` (Hexagon/FivePointStar extrudes with explicit `sides`) confirmed passing `test/validate_xsd.py` after the fix (the gap itself was verified directly by reading the pre-fix XSD, which had no `sides` attribute in `crossSectionType`); also confirmed the fixture exports cleanly via `mc3togltf`. Full 66/66 ctest green (fixture auto-discovered by `xsd_validation`'s `file(GLOB test/*.mc3.xml)`). |
| STAB-0652 | ✅ | P0 | `mc3.xsd`'s `pointType` is missing the `cx`/`cy`/`cz` attributes the writer emits for bezier path points | `mc3/mc3.xsd:228-235`, `mc3/src/Mc3XmlWriter.cpp:263-267`, `test/extrude_sides_bezier.mc3.xml` | **Fixed 2026-07-10.** Added 3 optional `xs:float` attributes (`cx`/`cy`/`cz`) to `pointType`. Same new fixture as STAB-0651 (`BezierRibbon` extrude with a 2-point bezier path with control handles) confirmed passing `test/validate_xsd.py` and exporting cleanly via `mc3togltf`. Full 66/66 ctest green. |
| STAB-0653 | ✅ | P1 | Root-level `default_camera` attribute was never parsed — implemented it (NOT removed; original audit finding was wrong about it being unused) | `mc3/mc3.xsd:912` (kept), `mc3/src/Mc3XmlParser.cpp:903-906,448-449`, `MC3_FORMAT.md`, `mc3/test/roundtrip_test.cpp` | **Correction before implementing: the initial plan (delete from XSD) was wrong.** A repo-wide grep found 13 real test fixtures actively using `<mc3 default_camera="...">` (`blupi_car.mc3.xml`, `speedy_blupi_world.mc3.xml`, etc.) — deleting it from the XSD would have broken `xsd_validation` for all of them. Worse: it was working only by *coincidence* — the parser's "no explicit default → use the first `<camera>`" fallback happened to pick the right one because the named camera was always first in document order. In `blupi_car.mc3.xml`/`speedy_blupi_world.mc3.xml` (2 and 3 cameras respectively) this was a real latent bug: reordering cameras or naming a non-first one would have silently ignored the author's explicit `default_camera=`. **Fixed 2026-07-10** by actually parsing it: `doc.defaultCamera` is now set from the root `default_camera` attribute first, then `<cameras default="...">` (parsed later) overrides it only if explicitly present, then the first-camera fallback applies only if still empty. Writer unchanged — always canonicalizes to `<cameras default="...">` on save. New test `testRootDefaultCameraAttribute` (`mc3/test/roundtrip_test.cpp`) proves a non-first named camera is honored, and that `<cameras default>` wins when both spellings are present. `MC3_FORMAT.md`'s Cameras section documents the precedence. Full 66/66 ctest green. |
| STAB-0654 | ✅ | P1 | Implement the `mip_maps` texture attribute — documented and schema-declared, but silently dropped on load (real data loss) | `mc3/include/MeshCraft/Mc3/Mc3Texture.hpp`, `mc3/src/Mc3XmlParser.cpp:493`, `mc3/src/Mc3XmlWriter.cpp:477-478`, `mc3/test/roundtrip_test.cpp` | **Fixed 2026-07-10.** Added `bool mipMaps{true}` to `Mc3Texture`, parsed via `attrB(c, "mip_maps", true)`, written only when `false` (matching the file's omit-default convention). New `testTextureMipMapsAttribute`: confirms `mip_maps="false"` parses correctly (previously silently dropped), survives a full save+reload cycle, and that the default `true` case round-trips without an explicit attribute being written. Full 66/66 ctest green. |
| STAB-0655 | ✅ | P2 | Fix `version` attribute default mismatch — XSD says `"0.3"`, parser/model fall back to `"0.1"` | `mc3/src/Mc3XmlParser.cpp:898`, `mc3/include/MeshCraft/Mc3/Mc3Document.hpp:30`, `MC3_FORMAT.md:11,18`, `mc3/test/golden/basic_scene.mc3.xml` | **Fixed 2026-07-10.** Confirmed no test fixture omits `version` (140 occurrences across all fixtures, all explicit) before changing the fallback, so this only affects programmatically-created documents. Changed both the parser fallback and `Mc3Document.hpp`'s in-class default from `"0.1"` to `"0.3"` (matching the XSD); fixed `MC3_FORMAT.md`'s opening example/table (was self-inconsistent with its own later `0.3` example) and added the previously-undocumented `default_camera` attribute to the same table (STAB-0653 follow-up). **Caught by the existing golden-byte test** (`testGoldenFileBasicScene`, which builds a doc via the bare default constructor): `mc3/test/golden/basic_scene.mc3.xml` legitimately needed its `version="0.1"` updated to `"0.3"` — confirmed this was the only byte diff before updating it. `mc3togltf/test/golden/basic_scene.mc3.xml` is a separate, hand-authored fixture with an explicit `version="0.1"`, unaffected. Full 66/66 ctest green. |
| STAB-0656 | ✅ | P3 | Gate `<uv_mapping>` write by object type to match XSD scoping (group/instance/area objects can't have it per-schema) | `mc3/src/Mc3XmlWriter.cpp:105-113`, `mc3/test/roundtrip_test.cpp` | **Fixed 2026-07-10.** The writer emitted `<uv_mapping>` for ANY object type whenever `obj->uvMapping` was set; `mc3.xsd`'s `instanceType`/`areaType`/`objectsContainerType` (used for group/union/difference/intersection/instance/area) don't declare it, unlike all 11 primitive types + mesh + extrude. Gated the write by `obj->type`, skipping Group/Union/Difference/Intersection/Instance/Area. Parser left unchanged (harmlessly reads it for any type on input; the writer gate prevents it from being perpetuated on save, which is sufficient — no fixture or editor UI path currently produces this combination). New test `testUvMappingNotWrittenForGroup` constructs a Group with `uvMapping` set directly on the model (bypassing the UI) and confirms the saved XML contains no `uv_mapping` element. Full 66/66 ctest green. |
| STAB-0657 | ✅ | P3 | Delete stale duplicate `mc3/MC3_FORMAT.md` (misleading — documents non-working `mip_maps`, contradicts current root `MC3_FORMAT.md`); add missing `star` cross-section mention to the canonical doc | `mc3/MC3_FORMAT.md` (deleted), `MC3_FORMAT.md:515` | **Fixed 2026-07-10.** Confirmed no functional reference to `mc3/MC3_FORMAT.md` anywhere (build/README/code) before deleting — only an incidental narrative mention in `STABILIZATION_WORKLOG.md`'s log of a past check, left as-is (historical record). `git rm mc3/MC3_FORMAT.md`. Added `star` (radius, inner_radius, sides) to the root doc's cross-section type list — the doc already correctly documented bezier path `cx`/`cy`/`cz` (STAB-0652's writer/parser were already correct; only the XSD lagged). Full 66/66 ctest green (docs-only change). |

---

## S22 — MCB Binary Format Coverage Audit (2026-07-10)

_New section, added 2026-07-10 after a systematic audit of whether `mcb` (binary format library) and `mc3tomcb` (CLI) fully and symmetrically cover everything the `Mc3Document` data model can hold, with no silent data loss on an `mc3 -> MCB -> mc3` round-trip. Overall finding: coverage is very good — all 19 object types, CSG, extrude (including S21's new `sides`/bezier fields), all N1-N7 extension sections, materials, lights, cameras, animation, and include-bookkeeping all round-trip correctly. 4 gaps found, all verified directly by reading both `mcb/src/McbWriter.cpp` and `mcb/src/McbReader.cpp` before being added here (no false positives — see STAB-0653 earlier this session for why that verification step matters)._

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0658 | ✅ | P0 | MCB never writes/reads `doc.rotationUnits`/`doc.eulerOrder` — silently reverts every rotation's interpretation after a round-trip | `mcb/src/McbWriter.cpp:452-454`, `mcb/src/McbReader.cpp:753-755`, `mcb/test/mcb_roundtrip_test.cpp` | **Fixed 2026-07-10.** Added `wIfStr`/read-key pairs for both fields, matching the pattern of the other root scalars (`unit`/`coordinateSystem`). New `testDocumentRotationConvention`: sets `rotationUnits="radians"`, `eulerOrder="ZYX"`, confirms both survive an MCB save+reload (previously silently dropped). Full 66/66 ctest green. |
| STAB-0659 | 📋 | P0 | MCB never writes/reads `Mc3Environment::skyboxTexture` | `mc3/include/MeshCraft/Mc3/Mc3Environment.hpp:21`, `mcb/src/McbWriter.cpp:404-410` (`writeEnvironment`), `mcb/src/McbReader.cpp:671-682` (`readEnvironment`) | `writeEnvironment()` writes `backgroundColor`/`backgroundTexture`/`fog` but never `env.skyboxTexture` (an equirectangular-panorama skybox, documented feature "I2"); the reader has no matching key either. `backgroundTexture` (its direct sibling, "I1") IS correctly covered on both sides — this is an asymmetric, clearly-unintentional single-field omission of a real, commonly-authored environment feature, silently dropped on every MCB round-trip. Fix: add `wIfStr`/read-key pair mirroring `backgroundTexture`'s handling. Verify: round-trip test with `skyboxTexture` set, confirm it survives MCB save+reload. |
| STAB-0660 | 📋 | P1 | MCB never writes/reads `Mc3Texture::mipMaps` (same bug class `STAB-0654` just fixed on the XML side, now found fresh on the MCB side) | `mc3/include/MeshCraft/Mc3/Mc3Texture.hpp:14`, `mcb/src/McbWriter.cpp:264-273` (`writeTexture`), `mcb/src/McbReader.cpp:441-455` (`readTexture`) | `mipMaps` was added to `Mc3Texture` this session (`STAB-0654`, 2026-07-10) specifically to fix silent data loss on XML load/save — that fix did not touch `mcb/`, so the identical bug now exists fresh on the MCB path: `mip_maps="false"` survives an XML round-trip but is silently lost the moment a scene goes through `mc3tomcb`. Fix: add `wIfBool`/read-key pair to `writeTexture()`/`readTexture()`, matching the pattern of `wrapU`/`colorSpace`. Verify: round-trip test — texture with `mipMaps=false`, confirm it survives MCB save+reload. |
| STAB-0661 | 📋 | P1 | MCB never writes/reads per-object `Mc3Object::metadata` (the document-level equivalent IS covered — this is the per-object one) | `mc3/include/MeshCraft/Mc3/Mc3Object.hpp:93-94`, `mcb/src/McbWriter.cpp:216-262` (`writeObject`), `mcb/src/McbReader.cpp:377-439` (`readObject`) | `obj.metadata` (opaque per-object key/value pass-through, mirrors `<metadata>` in the XSD, distinct from document-level `doc.metadata`/`doc.meta` which ARE both covered and tested) is never touched anywhere in `writeObject()`/`readObject()` — confirmed by reading the full body of both functions. Any object carrying import-sourced pass-through metadata with no first-class MC3 representation silently loses it on MCB round-trip. Fix: add a `wKeyMap`/read-key pair mirroring the document-level `metadata` handling (`McbWriter.cpp:465`, `McbReader.cpp:764`), scoped to `obj.metadata` instead of `doc.metadata`. Verify: round-trip test — object with per-object `<metadata>` properties, confirm they survive MCB save+reload. |

---

## Summary: Task Count by Section

_As of 2026-07-10, the 620 ✅ rows counted below live in `plan_20260710.md`; only the 29 🟡 + 1 📋 = 30 open rows remain inline in this file's S0-S20 sections above. Counts below are preserved for historical continuity._

| Section | Total | ✅ | 🟡 | 🧪 | 📋 | 🔴 |
|---------|-------|---|---|---|---|---|
| S0 Build | 25 | 24 | 1 | 0 | 0 | 0 |
| S1 Test infra | 40 | 40 | 0 | 0 | 0 | 0 |
| S2 MC3 XML | 55 | 54 | 1 | 0 | 0 | 0 |
| S3 MCB binary | 30 | 30 | 0 | 0 | 0 | 0 |
| S4 glTF export | 50 | 50 | 0 | 0 | 0 | 0 |
| S5 CSG | 35 | 35 | 0 | 0 | 0 | 0 |
| S6 Geometry | 25 | 25 | 0 | 0 | 0 | 0 |
| S7 Save/load | 35 | 34 | 1 | 0 | 0 | 0 |
| S8 UI robustness | 40 | 39 | 1 | 0 | 0 | 0 |
| S9 Registry | 35 | 34 | 1 | 0 | 0 | 0 |
| S10 AI | 40 | 39 | 1 | 0 | 0 | 0 |
| S11 Materials | 30 | 29 | 1 | 0 | 0 | 0 |
| S12 Animation | 30 | 28 | 2 | 0 | 0 | 0 |
| S13 Commands | 25 | 25 | 0 | 0 | 0 | 0 |
| S14 Rendering | 30 | 18 | 12 | 0 | 0 | 0 |
| S15 Import/export | 25 | 23 | 2 | 0 | 0 | 0 |
| S16 Cross-platform | 25 | 20 | 5 | 0 | 0 | 0 |
| S17 Documentation | 20 | 20 | 0 | 0 | 0 | 0 |
| S18 Code quality | 25 | 24 | 1 | 0 | 0 | 0 |
| S19 Security | 15 | 15 | 0 | 0 | 0 | 0 |
| S20 Release | 15 | 14 | 0 | 0 | 1 | 0 |
| S21 Format audit (new 2026-07-10) | 7 | 7 | 0 | 0 | 0 | 0 |
| S22 MCB coverage audit (new 2026-07-10) | 4 | 0 | 0 | 0 | 4 | 0 |
| **TOTAL** | **661** | **627** | **29** | **0** | **5** | **0** |

_Recomputed directly from per-row status markers after S0/S1/S2/S3/S4/S5/S6/
S7/S8/S9/S10/S11/S12/S13/S14/S16 fully closed (S1/S3/S4/S5/S6/S13/S16 100%,
S0/S2/S7/S8/S9/S10/S11 100% except one genuinely-flagged 🟡 row each —
STAB-0012 blocked on a CNA-side MinGW gap, STAB-0092 a documented accepted
limitation, STAB-0289/STAB-0327/STAB-0360/STAB-0389/STAB-0423 each a
documented real gap or corrected-premise row pending a product decision;
S12/S14 have a small handful of genuinely-flagged 🟡 rows each, mostly
requiring a live interactive display session this headless environment
can't provide). Total row count is 650 (this session found and removed an
accidental duplicate STAB-0521 row — the prior "651, S14 has 31 not 30"
note was describing that very duplicate, not a legitimate extra task; 650
matches the plan's original design count). Zero 🧪 rows remain anywhere in
the plan, and only **one** 📋 row remains total (STAB-0650, blocked on the
repo owner rotating a PAT before CI can even be run to check its output —
genuinely outside what this session can do). Derived, not hand-maintained
— recompute the same way after any batch of status changes rather than
incrementing by hand. Last recomputed 2026-07-06 (after closing
S11/S12/S13/S14/S16 — the GL-state-leak check, a from-scratch Emscripten
web build verified in real headless Chrome closing STAB-0571/0572/0573/0643
with concrete evidence instead of blank "manual" placeholders, and a real
Blender import test for release-representative content, STAB-0642).

---

## Priority Execution Order

### Gate 0 — Build (immediate)
1. **STAB-0001** — Verify debug build
2. **STAB-0004** — Confirm all 15 tests registered
3. **STAB-0019** — All 15 tests pass
4. **STAB-0014** — FetchContent offline mode verified

### Gate 1 — Format (next sprint)
5. **STAB-0121** — Create `mcb_roundtrip_test` binary (unblocks STAB-0122..0130)
6. **STAB-0122..0130** — MCB roundtrip for N1-N7 features
7. **STAB-0041..0045** — XSD validation fixtures for N3-N7
8. **STAB-0046** — Update `features.mc3.xml` with N3-N7 elements
9. **STAB-0068..0074** — Missing primitive roundtrip tests

### Gate 2 — Export (second sprint)
10. **STAB-0163** — All primitives have non-zero triangle count
11. **STAB-0178** — Animation export verified
12. **STAB-0199** — No silent geometry drops
13. **STAB-0205** — Nested CSG test

### Gate 3 — Editor safety (third sprint)
14. **STAB-0279..0283** — Commands are undoable
15. **STAB-0299..0303** — Dialog lifecycle safety
16. **STAB-0265..0268** — Autosave/backup verified

### Gate 4 — Registry/AI (fourth sprint)
17. **STAB-0371..0376** — AI mock tests
18. **STAB-0391** — AI result XSD validation
19. **STAB-0340..0342** — Registry edge case tests

### Gate 5 — Large scene (fifth sprint)
20. **STAB-0243** — Large scene unique mesh count check
21. **STAB-0244** — 500-object test

### Gate 6 — Documentation
22. **STAB-0579..0581** — README gaps
23. **STAB-0583..0584** — MC3_FORMAT.md updates
24. **STAB-0586..0587** — STABILIZATION.md and NEXT.md
25. **STAB-0592** — TESTING.md created

---

## Post-650 Follow-Up Findings (2026-07-07)

With the original 650-row list essentially closed (620 ✅ / 29 🟡 / 1 📋),
a fresh, independent bug-sweep investigation (not scoped to any specific
STAB row) looked at areas that get less scrutiny in a checklist-driven
process: resource lifetime, integer overflow, malformed/adversarial input
handling, CSG edge cases, the new `AiAssistant` threading code, and
silent-failure patterns. Two real, high-confidence bugs were found and
fixed (both empirically reproduced, not just inferred by inspection):

1. **MCB unbounded-recursion stack overflow** (`mcb/src/McbReader.cpp`,
   `readObject`/`skipValue`/`skipObject`). Unlike `CsgEvaluator.cpp` (which
   has an explicit `CSG_MAX_DEPTH = 12` guard), the MCB reader had no
   recursion-depth limit at all. A crafted `.mcb` file with ~20,000 levels
   of nested `<children>` (a few hundred KB) reliably **segfaulted the
   process** (stack overflow — not a catchable `std::exception`) when
   opened via `mc3tomcb` or the editor's Open dialog, both of which only
   guard against `catch (const std::exception&)`. Fixed with a
   `RecursionGuard<256>` template (mirrors the `CSG_MAX_DEPTH` pattern) in
   both `skipValue()` (covers the skip-unknown-field path, including
   nested `TAG_ARR`/`TAG_MAP`) and `readObject()` (covers the real
   `Mc3Object` tree's `children` self-recursion) — now throws a clean,
   named `"MCB: nesting depth exceeds 256"` error instead of crashing.
   Regression tests added: `testDeeplyNestedChildrenDoesNotCrash`
   (`mcb/test/mcb_roundtrip_test.cpp`, 2000-level nesting, confirms a
   clean throw naming the guard).
2. **MCB unbounded-allocation resource exhaustion** (`mcb/src/
   McbReader.cpp`'s `rRawStr`). A claimed string length was used to
   construct `std::string s(len, '\0')` — which actually commits/zero-
   writes `len` bytes — **before** validating it against the actual
   stream contents. A 23-byte crafted file (valid header + one `TAG_STR`
   field claiming a length of `0xFFFFFFF0` ≈ 4 GB, followed by only 2
   real bytes) forced **~4.1 GB of committed memory and 1.66s of CPU
   time** before the pre-existing truncation check finally threw. Fixed
   with a `kMcbMaxStringLen = 64 MB` sanity ceiling checked immediately
   after reading the length, before any allocation — no legitimate mc3
   scene has a single string field anywhere near this size. Regression
   test: `testHugeStringLengthRejectedCleanly` (confirms rejection in
   well under 1 second, not after a multi-second memory commit).
   (`vector::reserve(n)`-based amplification for the format's various
   count-prefixed arrays/maps was also investigated and found to already
   fail safely via a clean `std::bad_alloc` — `reserve()` only requests
   capacity, it doesn't commit/zero memory the way `std::string`'s
   fill-constructor does, so this secondary vector was not fixed.)
3. **`ModelRegistry.cpp` temp-file leak on exception** in both
   `insertIntoScene()` and `entryFromDefinition()` — same leak class as
   this session's STAB-0392 fix (`AiResponseAlgorithms.hpp::parseXmlAlg`,
   `MeshCraftApplication_UiAi.cpp::serializeScene`), just never
   propagated to these two call sites despite plan.md's own STAB-0611/
   STAB-0635 explicitly naming them as sharing the same tmp-file helper
   pattern. A malformed/corrupted registry entry (e.g. a hand-edited or
   corrupted SQLite row) made the risky call (`Mc3Document::
   loadFromFile`/`readFile`) throw before the temp-file removal line ran,
   leaking a file on every occurrence — reproduced empirically for
   `insertIntoScene`; `entryFromDefinition` has the structurally identical
   shape, fixed the same way. Regression test:
   `testInsertMalformedEntryDoesNotLeakTempFiles`
   (`mc3/test/mc3_registry_test.cpp`).

All three fixes are covered by new regression tests; full suite is 66/66
green (`mcb_roundtrip_test` +2, `mc3_registry_test` +1). No other bugs
were found — CSG edge cases (single-child, all-cutters, degenerate
geometry), the new `AiAssistant` detached-thread code, and
`Mc3XmlParser`/`Mc3XmlWriter`'s exception handling were all independently
re-examined and confirmed safe/already correctly handled.

**Follow-up check on the same bug class**: given how severe the MCB
recursion finding was, also empirically tested whether the *XML* load
path has the same unbounded-recursion vulnerability (`Mc3XmlParser.cpp`'s
`parseObject()` recurses on nested `<group>` children with no depth guard
of its own) — generated a real `.mc3.xml` fixture with 20,000 levels of
nested `<group>` and loaded it via `mc3togltf`. **No crash**: tinyxml2
itself already enforces its own element-nesting depth limit and returns a
clean `XML_ELEMENT_DEPTH_EXCEEDED` error (exit code 1, no exception needed
from `Mc3XmlParser.cpp` at all) — confirmed safe, no fix needed. This is
exactly why MCB (a from-scratch hand-rolled binary parser with no
third-party hardening) was the higher-value target, not the XML path
(which already benefits from tinyxml2's own defenses).

## Post-650 Follow-Up Findings (2026-07-07, conservative-maintainer audit)

**4. Clean-build reproducibility was broken — masked by incremental build
caching, not previously detectable by re-running `ctest` alone.** A
mandate to re-verify the project "from scratch" (rather than trust an
incrementally-updated `cmake-build-debug/`) led to fully deleting the
build directory and reconfiguring+rebuilding from absolute zero. Result:
**the literal `ninja` (default/`all`) build failed, exit code 1**, on
`../cna`'s `GamerProfile.cpp` calling a `RegionInfo::CurrentRegion()`
method that `../sharp-runtime` had renamed to `getCurrentRegionProperty()`
two days earlier (sharp-runtime commit `4af2e31`, 2026-07-05) — a
cross-repo API drift between the two sibling repos, neither of which this
project may modify without permission. Root-caused precisely: this broken
component (`CNA_GamerServices`, and `CNA_Net` which depends on it) is
**never linked by anything in this project** (confirmed via
`target_link_libraries` grep across every project `CMakeLists.txt`) — it
was only in the default build graph because `../cna/CMakeLists.txt`'s
`CNA_ENABLE_NET` option defaults to `ON`, and MeshCraft's own
`CMakeLists.txt` never opted out (unlike `CNA_BUILD_TESTS`/
`CNA_BUILD_EXAMPLES`/etc., which it already does set). **Fixed entirely on
the MeshCraft side, zero CNA/SHARP_RUNTIME source changes**: added
`set(CNA_ENABLE_NET OFF CACHE BOOL "" FORCE)` next to the existing
CNA cache-variable overrides in `CMakeLists.txt`, before
`add_subdirectory(../cna CNA_dep)`. Verified: `ninja` now exits 0 (120/120
steps, down from 566 — a build-time win too), `ctest` is 66/66, XSD
validation is 69/69. **Important nuance, not a retraction**: every prior
session's "N/N tests pass" claim was still genuinely true when run — the
now-broken `GamerProfile.cpp.o` had simply already been compiled
successfully *before* the 2026-07-05 rename and Ninja's incremental
dependency tracking had no reason to ever recompile it since nothing in
*this* repo touches that file. The tests themselves were never wrong; "the
project builds from a truly clean state" was quietly untrue for at least
two days across several sessions until this fix, and no amount of
re-running `ctest` in an already-populated build dir could have caught it
— only a genuinely empty build directory could. Full detail (root-cause
trace, all commands run) in `STABILIZATION_WORKLOG.md`'s Phase 1 section.

---

## Architecture Reference (preserved from original plan.md)

| Module | Location | Role |
|--------|----------|------|
| `MeshCraftApplication` | `src/MeshCraft/MeshCraftApplication.cpp` | Game loop, input, scene state, UI draw |
| `SceneRenderer` | `src/MeshCraft/Renderer/SceneRenderer*.cpp` | Renders MC3 scene + gizmos (4 files) |
| `EditorCamera` | `include/MeshCraft/Editor/EditorCamera.hpp` | Orbit/pan/zoom/focus |
| `Mc3Document` | `mc3/` sublibrary | Pure C++ scene data; load/save XML |
| `Mcb` | `mcb/` sublibrary | Binary serialization of Mc3Document |
| `mc3togltf_lib` | `mc3togltf/src/` | glTF/GLB export (GltfExporter + MeshBuilder + CsgEvaluator) |
| `ModelRegistry` | `src/MeshCraft/ModelRegistry.cpp` | SQLite asset store |
| `AiAssistant` | `src/MeshCraft/AiAssistant.cpp` | Claude API integration |
| CNA | `../cna` | SDL3 window, GL context, input, audio — DO NOT MODIFY |

### Hard Constraints
- **No CNA source changes** without owner permission.
- **No `${meta-gl_SOURCE_DIR}/include`** in CMakeLists.txt.
- **No `Mc3Document` public API changes** without checking `mc3togltf` and all test XMLs.
- **`file(GLOB_RECURSE)`** — new `.cpp` files require cmake reconfigure.
- **No new features** until gates S0–S6 are green.
