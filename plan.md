# Mesh Craft — Active Backlog

**This is the single active backlog.** For a compact baseline and handoff see
[`NEXT.md`](NEXT.md); for quality gates and process see
[`STABILIZATION.md`](STABILIZATION.md); for the registered tests see
[`TESTING.md`](TESTING.md). Superseded plans live in
[`docs/history/`](docs/history/).

## Status legend

- `TODO` — not started.
- `IN_PROGRESS` — being worked now.
- `DONE` — implemented, tested, and verified (verification command recorded).
- `BLOCKED` — cannot proceed; blocker recorded.
- `DEFERRED` — intentionally postponed with a reason.

Priority: `P0` (UB / crash / data-loss / unsafe-input / invalid-output-as-success),
`P1` (wrong behavior, missing validation, untruthful capability, critical test gap),
`P2` (maintainability / duplication / perf), `P3` (polish).

Workstreams: `W0` immediate defects · `W1` validation & untrusted input ·
`W2` AI/import sandbox · `W3` architecture decomposition · `W4` de-duplication ·
`W5` MC3 governance · `W6` MCB hardening · `W7` glTF fidelity · `W8` backend truth ·
`W9` undo & data-loss · `W10` UI testability · `W11` build/CI/DX ·
`W12` performance · `W13` documentation · `W14` new features.

## How this backlog was built

The `AUD-###` tasks below come from a 12-dimension evidence-based audit of the
current checkout (2026-07-11). Every finding was **adversarially re-verified**
against the real source by a second independent pass; 18 candidate findings were
refuted and dropped, leaving **57 confirmed**. Each task cites exact
`file:line` evidence. The `SYS-###` tasks are the mandated systematic
workstream items (matrices, sanitizers, benchmarks, features) that are not tied
to a single defect. This backlog is grounded in evidence — it is deliberately
**not** padded with filler to hit a round number.

---

## Done this session (2026-07-11)

All P0s surfaced by the audit are fixed. Verification for each:
`cmake --build b-release -j && (cd b-release && ctest)` → **93/93 pass**.

| ID | P | Summary | Commit | Verify |
|----|---|---------|--------|--------|
| AUD-000a | P0 | `ActiveTool::Measure` window-title out-of-bounds read → exhaustive `activeToolName()` | `afb1159` | `ctest -R active_tool` |
| AUD-000b | P1 | `ObjectType`↔name unified; Torus/Capsule/Disk/Grid/IcoSphere no longer mislabel; macro round-trip fixed | `838eefc` | `ctest -R object_type_name` |
| AUD-000c | P0 | NaN/Inf floats rejected at MC3 parse (`finiteOr`) | `dae910f` | `ctest -R mc3_finite_input` |
| AUD-000d | P1 | Tessellation counts clamped; instance-cycle depth cap; glTF non-finite export gate | `fd606d2` | `ctest -R "mc3_input_budget|mc3togltf_hostile_geometry"` |
| AUD-000e | P0 | Editor undo never recorded drag/color edits (80 dead sites) → snapshot relocated + lint guard | `737af77` | `ctest -R undo_snapshot_lint` |
| AUD-005  | P1 | AI/untrusted MC3 parsing sandboxed via `Mc3LoadPolicy` (no `<include>` LFI; in-memory parse) | `40643a4` | `ctest -R mc3_load_policy` |
| W7 lights | P1 | glTF spot lights no longer exported at origin; light position/range unit-scaled | `22b7129` | `ctest -R mc3togltf_light` |
| W4 anim | P1 | `insertAnimKeyframes` mirror converged with production (Gate B); Deform/Material seed live values | `dcd0d33` | `ctest -R mc3_commands` |
| W0 life | P1 | Deterministic shutdown; dangling SDL event watch removed; ImGui/GL teardown | `2d126bf` | `ctest -L render` |
| W8 truth | P1 | Stop advertising non-functional editor backends (Gate C): configure-time warning + honest CMake/README | `e53af49` | configure with non-EASYGL → warning |
| W7 caps | P1 | Ear-clip extrude caps so concave (Star/Custom) cross-sections triangulate correctly | `ac75eb6` | `ctest -R mc3togltf_earclip` |
| W1 paths | P1 | Confine exporter texture/mesh paths to the document root by default (`--allow-external-resources` opt-out) | `901965f` | `ctest -R mc3togltf_hostile_geometry` |

Also: archived 6 superseded status/plan docs to `docs/history/`; added `unit`/
`lint` CTest labels; test count 87 → 95. **Every P0 from the audit is closed
(Gate A), and every P1 identified by the audit is addressed.** Full suite green
after each commit.

**Not done (owner-gated):** activating CI. `.github_/workflows/ci.yml` is parked
because the repo push token lacks the GitHub `workflow` scope (owner-controlled)
— documented in the file. The parked workflow was made correct and ready this
session (de-staled comment counts, fixed the FetchContent cache key to include
`mc3/CMakeLists.txt`). To activate: rename `.github_` → `.github` and push with a
`workflow`-scoped token. See SYS-W11-01/03.

---

## Priority execution queue (next up)

1. **AUD-005 (P1/W2)** — sandbox AI/untrusted MC3 parsing: a load-policy that
   disables `<include>` and confines resource paths; parse AI XML in-memory.
   (Root cause shared with the path-traversal and include findings.)
   *Foundational — unblocks W1/W2.*
2. **W1 path-traversal containment** for mesh/texture/include resource paths.
3. **W7 glTF export fidelity** (spot-light position, unit-scale on lights,
   concave extrude caps).
4. **W4** `insertAnimKeyframesAlg` mirror diverged from production.
5. **W0** SDL event-watch registered with raw `this`, never removed.
6. **SYS-W11-01/02** — un-park CI + `-Wall -Wextra` on all first-party targets.
7. **W13 docs-drift** — reconcile remaining STABILIZATION/CHANGELOG/README counts
   and Web-status claims (several handled by this session's consolidation).

---

## Systematic workstream tasks (SYS-###)

Mandated workstream items not tied to a single audit finding.

### W1 — Validation subsystem
- **SYS-W1-01** `[TODO]` `P1` — First-class `Mc3Validation` result type (errors +
  warnings, each with source path, object/field identity, suggested safe repair).
  Wire into load / MCB-load / include-merge / AI-apply / pre-render / pre-export /
  save. Replaces scattered clamps.
- **SYS-W1-02** `[TODO]` `P1` — Documented numeric ranges per domain (geometry,
  material, camera near/far/FOV/aspect, environment, animation, audio, transforms,
  post-processing) with tests.
- **SYS-W1-03** `[TODO]` `P1` — Document-complexity budgets (max bytes, objects,
  definitions, materials, textures, embeds, actions, channels, keyframes,
  children-per-node, total nodes, recursion depth) enforced at load.
- **SYS-W1-04** `[IN_PROGRESS]` `P1` — Pathological-input fixture corpus. Seeded:
  `finite_input_test`, `input_budget_test`, `hostile_geometry_test`. Remaining:
  duplicate IDs, oversized base64, invalid UTF-8, include bombs, MCB corruption.
- **SYS-W1-05** `[TODO]` `P2` — Graph-cycle / shared-node policy for API-built trees.

### W2 — AI / import sandbox
- **SYS-W2-01** `[TODO]` `P1` — `Mc3LoadPolicy` threaded through parsing
  (`allowIncludes`, `confineToRoot`, budgets); `untrusted()` factory for AI.
- **SYS-W2-02** `[TODO]` `P1` — In-memory AI parse path (no temp file).
- **SYS-W2-03** `[TODO]` `P2` — Real JSON parse scoped to `content[].type=="text"`;
  correct UTF-16 surrogate pairs.
- **SYS-W2-04** `[TODO]` `P2` — Redact API keys / auth headers from errors/logs;
  bound HTTP + extracted-XML size.

### W3 — Architecture decomposition
- **SYS-W3-01** `[TODO]` `P2` — Extract from `MeshCraftApplication` (a god object
  split across .cpp files, not by responsibility): document/session, command/undo,
  selection, transform/gizmo, camera, animation, import/export, file/autosave,
  preferences, render pipeline, AI, registry, audio, dialog/status, platform
  bridge — with narrow interfaces, not another god object.

### W5 — MC3 governance
- **SYS-W5-01** `[TODO]` `P2` — Machine-readable field matrix (XSD / model /
  reader / writer / MCB / UI / renderer / exporter / examples / tests); CI-fail on
  a field present in one layer but missing another.
- **SYS-W5-02** `[TODO]` `P2/W13` — Document the 5 elements + 12 attributes
  `xsd_docs_diff.py` still reports missing from `MC3_FORMAT.md`: `area`,
  `background_texture`, `deform`, `skybox_texture`, `uv_mapping`; `aspect`,
  `autoplay`, `euler_order`, `material_override`, `mip_maps`, `offset_u`,
  `offset_v`, `projection`, `rotation_units`, `scale_u`, `scale_v`, `time_scale`.
- **SYS-W5-03** `[TODO]` `P2` — MC3 versioning + unknown element/attribute policy;
  round-trip must not silently drop unknown data unless policy says so.
- **SYS-W5-04** `[TODO]` `P2` — Central document index / reference resolver.
- **SYS-W5-05** `[TODO]` `P2` — Property-based round-trip tests + parser fuzz target.

### W6 — MCB hardening
- **SYS-W6-01** `[TODO]` `P1` — Budget every count read from MCB before allocation;
  check `count × size` overflow; detect truncation.
- **SYS-W6-02** `[TODO]` `P2` — Malformed/truncated/corrupt/random/fuzz MCB tests;
  byte-for-byte determinism; XML→MCB→XML equivalence.

### W7 — glTF fidelity
- **SYS-W7-01** `[TODO]` `P2` — Truthful export matrix per model feature, machine-checked.
- **SYS-W7-02** `[TODO]` `P2` — Differential geometry tests (viewport vs exporter).

### W8 — Backend truth
- **SYS-W8-01** `[TODO]` `P1` — Decide whether the *editor* supports non-GL
  backends (evidence: no — hard-wired GL ES3 + ImGui GL3). Either reject
  BGFX/VULKAN/SDL_RENDERER at configure time for the editor or implement a real
  backend interface; make README + CMake claims match.

### W9 — Undo & data-loss
- **SYS-W9-01** `[TODO]` `P1` — Classify every `undo_coverage_audit.py` candidate;
  one gesture = one transaction; restore selection on undo/redo.
- **SYS-W9-02** `[TODO]` `P1` — Atomic save (temp + rename); autosave; crash/
  corrupt recovery; never overwrite a user file after a failed save.

### W11 — Build / CI / DX
- **SYS-W11-01** `[TODO]` `P1` — Un-park CI (`.github_` → `.github`); build + test
  the 4 standalone libs on GCC and Clang.
- **SYS-W11-02** `[TODO]` `P1` — `-Wall -Wextra` on **all** first-party targets via
  a shared interface target; fix warnings; `-Werror` in CI.
- **SYS-W11-03** `[TODO]` `P2` — Editor build+test CI job (siblings + SDL3/GL/xvfb).
- **SYS-W11-04** `[TODO]` `P2` — Opt-in ASan + UBSan configs.
- **SYS-W11-05** `[TODO]` `P2` — Fuzz/differential harnesses over `McbReader` and
  `Mc3XmlParser`, wired into CI.
- **SYS-W11-06** `[TODO]` `P2` — `clang-format` + scoped `clang-tidy`.
- **SYS-W11-07** `[TODO]` `P2` — Package-first discovery + offline mode; pin
  sibling repos to recorded SHAs.

### W12 — Performance baselines
- **SYS-W12-01** `[TODO]` `P2` — Benchmark scenes + baselines (XML open/save, MCB
  convert/load, mesh-gen, CSG + cache, traversal, picking, undo snapshot, export,
  texture processing, animation eval, registry, startup, first frame). Optimize
  only measured bottlenecks.

### W14 — New features (after P0/P1 gates)
- **SYS-W14-01** `[TODO]` `P2` — Autosave + crash recovery.
- **SYS-W14-02** `[TODO]` `P2` — Scene validation & diagnostics UI (builds on SYS-W1-01).
- **SYS-W14-03** `[TODO]` `P2` — PNG screenshot / image export.
- **SYS-W14-04** `[DEFERRED]` `P3` — SVG texture rasterization pipeline.
- **SYS-W14-05** `[DEFERRED]` `P3` — Safe `embed:` mesh/resource support end-to-end.
- **SYS-W14-06** `[DEFERRED]` `P3` — Improved CSG output (smooth normals/UVs/materials).
- **SYS-W14-07** `[DEFERRED]` `P3` — Improved walk/navigation collision.
- **SYS-W14-08** `[TODO]` `P2` — AI change preview/diff before destructive replace.
- **SYS-W14-09** `[BLOCKED]` `P3` — Web persistence & export verification (blocked on
  CNA/browser — see NEXT.md).

---

## Audit-derived tasks (AUD-###)

Confirmed, adversarially-verified findings from the 2026-07-11 audit, ordered by
severity. The four already resolved this session are marked `DONE`.

### AUD-001 `[DONE]` `P0` `W9` · Undo never records Drag/Color edits: `if(IsItemActivated()) pushUndo()` inside the `if(DragFloat/ColorEdit)` block is dead code (transforms, all primitive dims, lights, camera, fog, material, keyframes — none undoable; also breaks redo-invalidation → silent data loss)
- **Component:** src/MeshCraft/Scene/PropertiesPanel.cpp, src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp, src/MeshCraft/MeshCraftApplication_Anim.cpp
- **Evidence:** The pervasive editor idiom nests the undo snapshot INSIDE the widget-changed block, e.g. /rv/data/development/github.com/openeggbert/mesh-craft/src/MeshCraft/Scene/PropertiesPanel.cpp:135-136 `if (ImGui::DragFloat3("##pos", pos, 0.1f)) {` / ` if (ImGui::IsItemActivated()) ctx.pushUndo();`. This is provably dead for Drag*/ColorEdit*: (1) IsItemActivated() is true ONLY on the activation frame — imgui.cpp `bool ImGui::IsItemActivated()` returns true iff `g.ActiveId==g.LastItemData.ID && g.ActiveIdP
- **Outcome:** Move the undo snapshot OUT of the widget-changed block so it is evaluated every frame, i.e. `bool ch = ImGui::DragFloat(...); if (ImGui::IsItemActivated()) pushUndo(); if (ch) { ...apply...; markModified(); }` (or push undo on IsItemActivated regardless of change) at all ~71 Drag*/ColorEdit* sites l
- **Tests:** Add a headless ImGui frame-driven regression (like the probe in scratchpad/undo_probe2.cpp) asserting a click-drag on a DragFloat/DragFloat3/ColorEdit3 fires pushUndo exactly once; add an editor test: undo, then a Drag edit, then redo must NOT discar

### AUD-002 `[DONE]` `P0` `W1` · MC3 parser accepts NaN/Inf floats unchecked; they flow into geometry and produce a spec-invalid glTF reported as success
- **Component:** mc3/src/Mc3XmlParser.cpp, mc3/src/MathUtils.hpp, mc3togltf/src/GltfExporter.cpp
- **Evidence:** There is NO isfinite/isnan check anywhere in mc3/src (grep confirms only mc3/test/roundtrip_test.cpp uses std::isfinite). attrF at Mc3XmlParser.cpp:38-42 only guards against exceptions: `try { return std::stof(v); } catch (...) { return def; }` — but std::stof("nan")/std::stof("inf") do NOT throw, they return NaN/Inf. Same at line 193: `try { float f = std::stof(s); p.size = {f, f, f}; }` so `<box size="nan"/>` yields size {NaN,NaN,NaN}. The vec paths are worse: MathUtils.hpp:11 `std::sscanf(s.c
- **Outcome:** Validate every parsed float (attrF, parseVec3/parseVec4, the stof size/scale paths) with std::isfinite and reject or clamp/default non-finite values at parse time; alternatively add an isfinite gate on generated vertex data in mc3togltf before writing accessors, mirroring the existing OBJ check.
- **Tests:** Add a fixture <box size="nan"/> and <sphere position="1e400 0 0"/>; assert Mc3Document load rejects/sanitizes, and that mc3togltf either fails loudly or emits only finite accessor min/max (no NaN/Inf in the GLB buffer).

### AUD-003 `[TODO]` `P1` `W2` · AI-generated XML is parsed through the full Mc3Document::loadFromFile pipeline, which honors <include file="...">, giving untrusted AI output arbitrary local-file access / path traversal
- **Component:** AI response parsing (src/MeshCraft/AiResponseAlgorithms.hpp -> mc3/src/Mc3XmlParser.cpp)
- **Evidence:** The AI's raw text is written to a temp file and fed straight to the production XML loader: AiResponseAlgorithms.hpp:98-112 parseXmlAlg() does `{ std::ofstream f(tmp); f << xml; } ... auto doc = Mc3::Mc3Document::loadFromFile(tmp);`. validateAndParseAiResponseAlg (line 216) calls parseXmlAlg BEFORE any XSD check (line 223), so the parse side effects fire regardless. Mc3Document::loadFromFile -> Mc3XmlParser::parse (Mc3Document.cpp:7-10, Mc3XmlParser.cpp:915) runs processIncludes() at :960. proces
- **Outcome:** Before writing an AI response to the temp file / parsing it, either strip/reject any <include> element, or parse AI output in a hardened mode that disables include processing and any filesystem-relative resolution (texture uri / meshSource / embed src). At minimum, confine include/resource paths to 
- **Tests:** Add an ai_test.cpp case feeding validateAndParseAiResponseAlg a response whose XML contains `<include file="/etc/hostname">` and `<include file="../../../secret.mc3.xml">`, asserting the include is NOT loaded (no filesystem access outside an allowed 

### AUD-004 `[TODO]` `P1` `W8` · Gate C: editor UI + post-FX hard-wired to OpenGL ES3, yet CMake advertises SDL_RENDERER/BGFX/VULKAN as supported backends
- **Component:** src/MeshCraft/MeshCraftApplication.cpp + CMakeLists.txt (MESH_CRAFT_GRAPHICS_BACKEND option)
- **Evidence:** CMakeLists.txt:27 `# Supported: SDL_RENDERER, EASYGL, BGFX, VULKAN`; CMakeLists.txt:31-32 `set_property(CACHE MESH_CRAFT_GRAPHICS_BACKEND PROPERTY STRINGS SDL_RENDERER EASYGL BGFX VULKAN)`; CMakeLists.txt:47-50 FATAL_ERROR accepts all four as valid. But the editor renders its UI ONLY through OpenGL, unconditionally, with no backend branch: MeshCraftApplication.cpp:7 `#include <imgui_impl_opengl3.h>`; :212-213 `ImGui_ImplSDL3_InitForOpenGL(sdlWindow, glCtx); ImGui_ImplOpenGL3_Init("#version 300 e
- **Outcome:** Either (a) truthfully restrict the advertised option to backends the editor can actually drive — i.e. make CMake hard-error (not just accept) on BGFX/VULKAN/SDL_RENDERER, or explicitly mark them experimental/unsupported for the GUI editor — or (b) actually implement per-backend ImGui renderers (imgu
- **Tests:** Configure+build with -DMESH_CRAFT_GRAPHICS_BACKEND=VULKAN (and BGFX, SDL_RENDERER) and either observe a clear configure-time rejection, or launch the editor and confirm the ImGui UI actually renders. Currently there is no test asserting the editor wo
- **Verify note:** Severity P1 is correct. One trivial line-number imprecision: the SDL_GL screenshot readback in MeshCraftApplication_Commands.cpp is at lines 381-383 (glFinish 381, glBindBuffer 382, glReadPixels 383), not the claimed 379-381. This does not affect the finding.
- **Blocked:** Full multi-backend ImGui support would require CNA-side GL/Vulkan context coordination (CNA is out of scope per CLAUDE.md). The truthfulness fix (restrict/label the option) is fully in-repo and NOT blocked.

### AUD-005 `[TODO]` `P1` `W8` · ImGui is compiled OpenGL-ES3-only and linked into the editor for every backend; no alternate ImGui backend is ever built
- **Component:** CMakeLists.txt (imgui target + editor link)
- **Evidence:** CMakeLists.txt:129-135 the imgui static lib compiles ONLY `imgui_impl_sdl3.cpp` and `imgui_impl_opengl3.cpp` — no imgui_impl_sdlrenderer3.cpp, imgui_impl_vulkan.cpp, or bgfx backend. CMakeLists.txt:141 `target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)` and :152 `target_link_libraries(imgui PUBLIC GLESv2)` bake in GLES3 unconditionally. The editor links this GL-only imgui for ALL four backends with no guard: CMakeLists.txt:338 (Emscripten), :357 (native GNU/Clang), :369 (fallback) a
- **Outcome:** When a non-GL backend is selected, either compile+link the matching ImGui backend (imgui_impl_vulkan/imgui_impl_sdlrenderer3) instead of imgui_impl_opengl3, or fail configuration. Do not silently link the GL3 UI backend into a Vulkan/BGFX/SDL_Renderer build.
- **Tests:** Assert at configure time that the ImGui backend source list matches the selected MESH_CRAFT_GRAPHICS_BACKEND; add a CI matrix entry per backend.
- **Verify note:** Two minor evidence corrections: (1) the editor's imgui link lines are 338 (Emscripten), 358 (native GNU/Clang), 365 (fallback) — not the claimed :357/:369. (2) GLESv2 at line 152 is guarded by `if(NOT EMSCRIPTEN)` (151), so it is unconditional only for native builds; Emscripten instead uses WebGL2 (
- **Blocked:** Not blocked for the honest-restriction fix; a real per-backend ImGui build needs the corresponding CNA context type.

### AUD-006 `[TODO]` `P1` `W8` · README documents 'Build with SDL_RENDERER backend' as a first-class supported build with no caveat that the editor UI cannot render
- **Component:** README.md
- **Evidence:** README.md:59-64 `### Build with SDL_RENDERER backend` followed by copy-paste `cmake -S . -B cmake-build-debug -G Ninja -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER` / `ninja ...`, presented identically to the default EASYGL build immediately above it. The only following `> Note:` (:66) is about GLOB_RECURSE, not about the backend. Nowhere in README does it state that selecting SDL_RENDERER/BGFX/VULKAN yields an editor whose ImGui UI (the whole GUI) will not draw because MeshCraft only ships the Im
- **Outcome:** Remove or explicitly caveat the SDL_RENDERER build section: state that EASYGL (desktop GL) / WebGL2 is the only backend the GUI editor currently renders on, and that SDL_RENDERER/BGFX/VULKAN are engine-level (CNA) options not yet wired to the editor UI.
- **Tests:** Doc review; ensure the Platform Support Matrix and build sections agree with the actual working backend set.
- **Blocked:** None — pure documentation fix.

### AUD-007 `[TODO]` `P1` `W11` · CI is permanently parked under `.github_/` — GitHub Actions never runs; the repo has zero CI
- **Component:** .github_/workflows/ci.yml
- **Evidence:** .github_/workflows/ci.yml:1-7 self-documents the deactivation: "DEACTIVATED: this file lives under `.github_/` (note the trailing underscore), not `.github/`, so GitHub does NOT run it." Confirmed: `ls -d .github` -> "No such file or directory"; the only workflow file tracked in git is `.github_/workflows/ci.yml`; `git log --all --diff-filter=D -- .github/workflows/ci.yml` is empty, so an active workflow dir never existed. Net effect: no automated build/test has ever gated any push or PR on mast
- **Outcome:** Rename `.github_` -> `.github` (over the SSH remote) so the workflow actually runs, or explicitly document why CI must remain disabled; the current state means every correctness/build regression ships unverified.
- **Tests:** After renaming, confirm a run appears under Actions for a test push to develop and that all matrix jobs go green; verify branch protection requires the checks.
- **Verify note:** Core finding fully verified; evidence is accurate. Two refinements: (1) Severity is arguably P2 rather than P1 — the deactivation is intentional, honestly self-documented in the file itself, and tracked as a known owner-blocked task (STAB-0650, referenced in NEXT.md:263-264 and MEMORY as "blocked on

### AUD-008 `[TODO]` `P1` `W13` · STABILIZATION.md is doubly stale: 650 tasks / 620 done AND 66 tests, both wrong
- **Component:** STABILIZATION.md
- **Evidence:** STABILIZATION.md:23 "The full backlog is **650** `STAB-XXXX` tasks ... sectioned S0-S20"; :26 "Overall: **620 ✅ / 29 🟡 / 0 🧪 / 1 📋 / 0 🔴** across all 650 rows."; :44 "**66 CTest tests, all passing**". Line 3 asserts "every count below was recomputed directly from `plan.md`'s per-row status markers and `ctest`." But plan.md's own current summary table (plan.md:399) is 723 total / 683 ✅ (S21–S25 added 73 tasks), and ctest is 87. The 650/620/66 figures predate the S21–S25 expansion and were never u
- **Outcome:** Either refresh every count (723 total, 683 ✅ per plan.md table, 87 tests) or archive STABILIZATION.md to docs/history/ and keep only the policy section, which is the sole non-stale content.
- **Tests:** Compare STABILIZATION.md:23/26/44 against plan.md:399 and `ctest -N`

### AUD-009 `[TODO]` `P1` `W13` · STABILIZATION_VERIFICATION.md marks Web/Emscripten ✅ fully working while README and NEXT.md say it is broken
- **Component:** STABILIZATION_VERIFICATION.md vs README.md vs NEXT.md
- **Evidence:** STABILIZATION_VERIFICATION.md:56 "Web (Emscripten) | ✅ | Full build succeeds; real headless-Chrome session confirms a working WebGL2 context, no console/GPU errors". Directly contradicted by README.md:213 "3D viewport rendering | ... | ❌ blank canvas" and NEXT.md:67-72 which root-causes it as "an uncaught crash ... kills the whole wasm module ... The app dies on the first resize event, before it ever finishes sizing anything". Three docs assert three mutually exclusive states (fully-working / bl
- **Outcome:** Reconcile to the latest verified state (NEXT.md's crash root-cause, 2026-07-11). Flip STABILIZATION_VERIFICATION.md:56 from ✅ to 🟡/❌ or archive the file.
- **Tests:** Diff the Web row across STABILIZATION_VERIFICATION.md:56, README.md:212-213, NEXT.md:67-72
- **Verify note:** Evidence is accurate as written. One minor nuance to add for precision: STABILIZATION_VERIFICATION.md carries a file-level disclaimer at line 5 ("_Last verified: 2026-07-07, ... commit 4b7fcd2_"), so it is a dated snapshot older than README's 2026-07-09 Web-column update and NEXT.md's 2026-07-11 roo

### AUD-010 `[TODO]` `P1` `W13` · CHANGELOG.md presents a wildly stale progress snapshot (194 done / 317 not-started of 650) as current
- **Component:** CHANGELOG.md
- **Evidence:** CHANGELOG.md:12-13 "the full 650-task backlog (`STAB-0001`–`STAB-0650`, sectioned S0–S20)"; :25-28 "As of this writing: **194 ✅ done, 3 🟡 partial, 136 🧪 has a plan but not executed, 317 📋 not started** out of 650 `STAB-XXXX` tasks". Current reality (plan.md:399) is 723 total / 683 ✅ / 0 🧪 / 2 📋, and NEXT.md:12 says 688 ✅. The changelog's 194-done/317-not-started/136-needs-test figures are off by hundreds of rows and describe a project phase that ended weeks ago.
- **Outcome:** Replace the [Unreleased] progress paragraph with the current summary (or drop the per-status counts entirely and link plan.md's table, which the changelog already calls authoritative at :105). Add S21–S25 to the '650-task' description.
- **Tests:** Compare CHANGELOG.md:25-28 against plan.md:399 and NEXT.md:12
- **Verify note:** Severity should be P2, not P1: this is documentation staleness in an [Unreleased] changelog progress count, not an "untruthful capability claim" — it misrepresents no shipped software capability, causes no UB/crash/invalid output. Two mitigating facts the finding omits: (a) CHANGELOG.md:3-6 frames t

### AUD-011 `[TODO]` `P1` `W13` · plan.md self-contradicts: summary table says 723 rows, the note directly under it says 650
- **Component:** plan.md
- **Evidence:** plan.md:399 "| **TOTAL** | **723** | **683** | **38** | **0** | **2** | **0** |" (arithmetically self-consistent: 683+38+2=723). But the annotation immediately below, plan.md:409-411, states "Total row count is 650 (this session found and removed an accidental duplicate STAB-0521 row ... 650 matches the plan's original design count)", and plan.md:369 (12 lines above the table) says "the 620 ✅ rows counted below ... only the 29 🟡 + 1 📋 = 30 open rows remain inline" (620+30=650). The same section 
- **Outcome:** Delete the obsolete '650' note (plan.md:409-411) and the '620 ✅ / 30 open' framing (plan.md:369); they predate the S21–S25 rows the table itself now counts. Keep one internally consistent total.
- **Tests:** Re-run the per-row recompute the note itself prescribes and confirm it equals the table
- **Verify note:** Evidence is accurate and needs no correction. Severity should be P2, not P1: this is a documentation/bookkeeping inconsistency in a planning markdown file (plan.md) with zero runtime, data, or output impact and no untruthful claim about software capability, so it falls under P2 (maintainability/docu

### AUD-012 `[TODO]` `P1` `W4` · insertAnimKeyframesAlg mirror has diverged from production insertAnimKeyframes (STAB-0715) — Material/Deform keyframes captured as 0.0 instead of the live value
- **Component:** include/MeshCraft/EditorAlgorithms.hpp:1456 vs src/MeshCraft/MeshCraftApplication_Anim.cpp:195
- **Evidence:** The mirror is explicitly documented as "Mirrors MeshCraftApplication::insertAnimKeyframes()" (EditorAlgorithms.hpp:1424-1434) and is TEST-ONLY (0 production call sites; only mc3/test/editor_commands_test.cpp:2166/2178/2187/2193 call it). The mirror still uses the pre-STAB-0715 10-way switch: EditorAlgorithms.hpp:1457-1470 handles Position/Rotation/Scale/Visible from the live object but every other property (all Material* and Deform*) hits `default: value = Mc3::evaluateChannel(action.channels[ci
- **Outcome:** Re-sync insertAnimKeyframesAlg to call the same value-resolution logic as production (a resolveObjectPropertyValueAlg mirror of Anim.cpp:24-79), OR delete the mirror and test insertAnimKeyframes through production. Then add a test that inserts a Deform/Material keyframe and asserts the captured valu
- **Tests:** New editor_commands_test case: object with no deform, insert DeformX keyframe, assert keyframe.value==1.0 (currently the mirror would yield 0.0). Same for MaterialRoughness → 0.5.
- **Verify note:** Evidence line-citation is slightly off: the "Mirrors MeshCraftApplication::insertAnimKeyframes()" statement is at EditorAlgorithms.hpp:1411 (the doc block spans 1409-1419), not "1424-1434" (line 1424 is a function parameter). Two doc-comment staleness details strengthen the finding: (a) lines 1416-1

### AUD-013 `[TODO]` `P1` `W7` · Spot light position is dropped — every spotlight is exported at the world origin
- **Component:** mc3togltf/src/GltfExporter.cpp addLights()
- **Evidence:** GltfExporter.cpp:979-990: `if (light.type == LightType::Directional || light.type == LightType::Spot) { ... lnode.rotation = {q...}; } else { lnode.translation = {light.position...}; }`. Spot lights take the rotation-only branch, so `lnode.translation` is never set. But Mc3Light.hpp:19-20 documents `position` as meaningful for `// Spot / Point`. A spotlight authored at any position is emitted with only its direction-derived rotation and no translation → placed at (0,0,0). Directional lights legi
- **Outcome:** Emit BOTH translation (from light.position) and rotation (from direction) for Spot lights; only Directional should be rotation-only.
- **Tests:** Export a scene with a spot light at position (5,3,0); assert the light node's translation == (5,3,0)*unitScale, not origin.

### AUD-014 `[TODO]` `P1` `W7` · Point/spot light position and range are not multiplied by unitScale — lights misplaced in non-meter documents
- **Component:** mc3togltf/src/GltfExporter.cpp addLights()
- **Evidence:** addLights (GltfExporter.cpp:922-924) takes no unitScale parameter and is called at GltfExporter.cpp:1453 `addLights(model, doc.lights, lightNodes);` (no scale). Point-light translation uses the raw value: GltfExporter.cpp:985-989 `lnode.translation = { static_cast<double>(light.position[0]), ... }`, and range at 958-959 `lo["range"] = ...(light.range)`. Meanwhile all geometry AND cameras are scaled: camera code at 1062-1066 multiplies by unitScale (STAB-0693 comment: 'cameras previously used raw
- **Outcome:** Pass ctx.unitScale into addLights and multiply light.position and light.range by it, matching addCameraNodes (STAB-0693).
- **Tests:** Export a unit='centimeter' scene with a point light at (100,0,0); assert node translation == (1,0,0).
- **Verify note:** Evidence is correct with one small imprecision: spot lights (979-983) receive only a rotation and no translation at all, so the "spot light position not scaled" wording is off — spot lights export no position. Accurate statement: point-light position (985-989) is exported unscaled, and light.range (

### AUD-015 `[TODO]` `P1` `W7` · Extrude end-caps use naive fan triangulation — concave cross-sections (built-in Star, arbitrary Custom) produce overlapping/incorrect cap geometry, silently
- **Component:** mc3togltf/src/MeshBuilder.cpp buildExtrude() addCap()
- **Evidence:** MeshBuilder.cpp:1052-1055: `for (uint32_t i = 1; i + 1 < ring.size(); ++i) { if (flip) ...{base, base+i+1, base+i}; else ...{base, base+i, base+i+1}; }` — a triangle fan anchored at ring vertex 0. This is only correct for CONVEX polygons. sampleCrossSection produces a genuinely concave built-in shape: CT::Star (MeshBuilder.cpp:686-699) alternates outer/inner radius vertices. Fan-triangulating a star from vertex 0 emits triangles that cover the concave notches and overlap outside the outline — a 
- **Outcome:** Use a proper polygon triangulation (ear-clipping / monotone) for caps, or at minimum warn when the cross-section is non-convex and caps are requested.
- **Tests:** Extrude a Star cross-section with caps=true; verify cap triangles all lie inside the star outline and do not overlap (e.g. signed-area / point-in-polygon check).

### AUD-016 `[TODO]` `P1` `W7` · Generated primitive/extrude geometry is never validated for finiteness — degenerate params emit NaN positions and NaN accessor min/max while export reports success
- **Component:** mc3togltf/src/MeshBuilder.cpp / GltfExporter.cpp
- **Evidence:** Helix path tangent MeshBuilder.cpp:748 `float tx = -std::sin(t), ty = h / (r * totalAngle), tz = std::cos(t);` divides by `r * totalAngle`; helixTurns==0 (totalAngle=0) or helixRadius==0 makes ty = h/0 = inf → normalization ty/tl = inf/inf = NaN → NaN propagates into positions/normals. buildSphere (97-101,104-106) computes `phi = pi * r / rings` with `rings = segments/2`; segments==1 → rings==0 → 0/0 = NaN. There is NO finiteness guard on generated geometry before writing, and addAccessorVec3's 
- **Outcome:** Add the same finiteness check the OBJ loader has (or clamp/validate degenerate primitive params) to buildMesh/addMeshDataToGltf so a NaN/Inf mesh fails loudly instead of producing a spec-invalid glTF reported as success.
- **Tests:** Export an Extrude with a Helix path, helixTurns=0; assert the exporter throws (or the output contains no NaN) rather than exiting 0 with NaN accessor bounds.

### AUD-017 `[TODO]` `P1` `W0` · SDL event watch registered with raw `this` is never removed (dangling callback / UAF at shutdown)
- **Component:** src/MeshCraft/MeshCraftApplication.cpp (LoadContent / sdlEventWatch)
- **Evidence:** LoadContent registers a global SDL event filter capturing the application pointer: MeshCraftApplication.cpp:215 `SDL_AddEventWatch(reinterpret_cast<SDL_EventFilter>(sdlEventWatch), this);`. The callback dereferences that pointer, e.g. MeshCraftApplication.cpp:91 `auto* self = static_cast<MeshCraftApplication*>(userdata);` and :101 `self->pendingDropTexture_ = path;`. There is NO matching `SDL_RemoveEventWatch` anywhere (grep of src/ + include/ returns none), and there is no destructor/UnloadCont
- **Outcome:** Pair the SDL_AddEventWatch with SDL_RemoveEventWatch(sdlEventWatch, this) in a new UnloadContent()/Dispose override (or destructor) so the callback cannot fire against a destroyed object; overriding CNA's UnloadContent/Dispose is the intended hook.
- **Tests:** Add a shutdown test (or run under ASAN) that opens the app, triggers Exit(), and confirms no SDL event dispatch reaches sdlEventWatch after the object is destroyed; verify a drop event during teardown does not use freed memory.
- **Verify note:** Severity is borderline P1/P2, and the "UAF at shutdown" framing slightly overstates the immediate impact: in the normal single-run flow nothing pumps SDL events after Run() returns, and SDL is not quit during the object's lifetime, so the stale watch is never actually invoked with a freed `this`. Th

### AUD-018 `[DONE]` `P1` `W1` · Segment/subdivision/sides counts are parsed unbounded, enabling memory-exhaustion DoS from a hostile primitive
- **Component:** mc3/src/Mc3XmlParser.cpp, mc3togltf/src/MeshBuilder.cpp
- **Evidence:** attrI (Mc3XmlParser.cpp:44-48) returns std::stoi with no range clamp. parsePrimitive sets `p.segments = attrI(el, "segments", type == ObjectType::IcoSphere ? 2 : 32);` (line 212) and subdivisionsX/Z (lines 223-224) with no upper bound; parseCrossSection sets `cs.sides = attrI(el, "sides", 6); cs.segments = attrI(el, "segments", 32);` (lines 116-117); parseExtrude sets `ext.segments = attrI(el, "segments", 32);` (line 162). buildPrimitive (MeshBuilder.cpp:1106) forwards this straight to `buildSph
- **Outcome:** Clamp segments/sides/subdivisions to a sane maximum (e.g. <=4096) at parse time in parsePrimitive/parseCrossSection/parseExtrude, or reject values above a budget, so no untrusted count can drive unbounded allocation.
- **Tests:** Add a fixture <sphere segments="100000000"/> and assert the loader clamps p.segments (or mc3togltf fails fast) instead of attempting a multi-terabyte allocation.
- **Verify note:** Severity P1 is appropriate (resource-exhaustion/availability DoS on an untrusted input path, no UB/corruption). One clarification that strengthens rather than weakens the finding: the CSG path is not fully bounded either — while Manifold's native Sphere/Cylinder/Cone use CSG_SEGMENTS=32 (CsgEvaluato

### AUD-019 `[TODO]` `P1` `W1` · Mesh src / texture uri / include file paths are unvalidated -> path traversal reads arbitrary files (texture bytes are exfiltrated into the output GLB)
- **Component:** mc3/src/Mc3XmlParser.cpp, mc3togltf/src/GltfExporter.cpp, mc3togltf/src/MeshBuilder.cpp
- **Evidence:** The parser stores filesystem references verbatim with no containment check: parseTextures line 491 `tex.uri = attr(c, "uri");`, mesh source lines 317-319 `obj->meshSource = attr(el, "src");`, and processIncludes line 901 `std::filesystem::path includePath = selfPath.parent_path() / fileAttr;`. In the consumer, GltfExporter.cpp:431 does `std::filesystem::path imgPath = basePath / tex.uri;` then reads raw bytes and base64-embeds them into the GLB (lines 432-438: `img.image = std::vector<unsigned c
- **Outcome:** After resolving meshSource/tex.uri/embed src/include file against basePath, canonicalize and reject any path that escapes the document root (contains .. traversal above root or is absolute), before opening the file.
- **Tests:** Fixture with <texture uri="/etc/hostname"> and uri="../../secret"; assert mc3togltf refuses to read/embed files outside the source directory.
- **Verify note:** Minor evidence imprecision: `tex.uri = attr(c, "uri");` is at Mc3XmlParser.cpp line 490, not 491 (491 is `tex.wrapU`). Severity P1 is reasonable and arguably conservative — since the rubric lists "unsafe-input" under P0 and this yields arbitrary-file-read/exfiltration from untrusted converter input 

### AUD-020 `[DONE]` `P1` `W1` · <instance> definition cycle causes unbounded recursion / stack overflow in GltfExporter::buildNode (no depth or visited-set guard)
- **Component:** mc3togltf/src/GltfExporter.cpp, mc3/src/Mc3XmlParser.cpp
- **Evidence:** parseDefinitions (Mc3XmlParser.cpp:636-646) and parseObject build <instance definition="..."> nodes with no cycle validation. buildNode is declared `static int buildNode(ExportCtx& ctx, const Mc3Object& obj)` (GltfExporter.cpp:716) with NO depth parameter and no visited set. On an instance it resolves the definition and then unconditionally recurses into the definition's children: lines 800-805 `for (const auto& child : defObj.children) { ... int ci = buildNode(ctx, *child); ... }`, and the gene
- **Outcome:** Add a recursion depth cap and/or an in-progress definition-id set to buildNode (mirroring CSG_MAX_DEPTH), throwing on cyclic/over-deep instance expansion instead of recursing unbounded.
- **Tests:** Fixture: <definition id="A"><group><instance definition="A"/></group></definition> plus a top-level <instance definition="A"/>; assert mc3togltf reports a cycle error rather than crashing.
- **Verify note:** Finding is accurate as stated. Severity P1 is defensible but arguably understated: because this is a stack-overflow crash triggered by untrusted/malformed input to a file-conversion tool, it plausibly qualifies as P0 (crash / unsafe-input) under the rubric. No downgrade is warranted.

### AUD-021 `[TODO]` `P2` `W2` · Provider JSON parsed by hand-rolled string scanning: extractFirstTextValue trusts the first literal "text":" and mis-decodes UTF-16 surrogate pairs into invalid UTF-8
- **Component:** JSON extraction (src/MeshCraft/AiAssistant.cpp)
- **Evidence:** AiAssistant.cpp:66-69: `const std::string key = "\"text\":\""; auto pos = json.find(key);` — it takes the FIRST occurrence of the literal `"text":"` anywhere in the body as the model's answer, with no awareness of JSON structure (not scoped to content[].type=="text"); any provider/feature that emits a `"text":"..."` field earlier (citations, tool_use input echoes, server-tool blocks, future response shapes) would be extracted instead. The \u handler at :85-108 only synthesizes BMP code points (`
- **Outcome:** Parse the Messages response with a real JSON parser (or at minimum scope extraction to the content array and the first block with type=="text"), and either correctly combine UTF-16 surrogate pairs or reject/replace lone surrogates so output is always valid UTF-8.
- **Tests:** Add ai_test.cpp cases: (a) a response where an earlier field literally contains `"text":"decoy"` before the real content block, asserting the real text is returned; (b) an astral `😀` escape asserting valid UTF-8 output.

### AUD-022 `[TODO]` `P2` `W2` · No test covers a malicious <include> in an AI response — the arbitrary-file-read path is entirely unverified
- **Component:** Test coverage (mc3/test/ai_test.cpp)
- **Evidence:** ai_test.cpp exercises only JSON helpers and extract/repair/parse of benign XML (grep shows cases for extractStopReason/extractFirstTextValue/extractXmlAlg/repairXmlAlg/parseXmlAlg with hello/café/fenced XML), and never feeds an AI response containing an `<include>` directive. Combined with finding #1 (parseXmlAlg -> loadFromFile honors <include> against attacker paths), the security-critical file-access vector is completely untested, so a regression that (re)enables arbitrary include resolution 
- **Outcome:** Add a regression test asserting that AI responses containing <include> do not cause any filesystem read outside an allowed root once finding #1 is fixed.
- **Tests:** New ai_test.cpp case as described in finding #1's tests field.
- **Verify note:** Impact framing should be tightened: loadFromFile performs local-file INCLUSION/parse (LFI/XXE-style — it merges the target's definitions or throws a parse error), not direct content exfiltration, so 'arbitrary-file-read' overstates it. 'Completely untested' is imprecise: the include-resolution machi
- **Blocked:** Depends on the sanitization decision in finding #1 (test asserts whatever confinement policy is chosen).

### AUD-023 `[TODO]` `P2` `W8` · Android build path force-selects SDL_RENDERER, guaranteeing the editor UI would not render if built for Android
- **Component:** CMakeLists.txt (Android backend auto-select)
- **Evidence:** CMakeLists.txt:36-37 `if(ANDROID) set(MESH_CRAFT_GRAPHICS_BACKEND_UPPER "SDL_RENDERER")`. Combined with the fact (Finding 1) that the editor's ImGui UI and post-FX only work through ImGui_ImplOpenGL3 + SDL_GL_GetProcAddress (which need an SDL GL context, not an SDL_Renderer), an Android build would compile against a backend the editor cannot render on. README.md:211 correctly marks Android as `never attempted` so it is not falsely claimed working, but the CMake default choice bakes in a non-func
- **Outcome:** If Android support is intended, wire the editor to a backend it can actually render on (e.g. GLES via EASYGL) or gate the GUI editor off on Android with a clear message, rather than auto-selecting SDL_RENDERER which the UI layer cannot drive.
- **Tests:** An Android NDK configure that either selects a GL-capable backend or errors clearly; not currently testable here (no NDK installed).
- **Verify note:** Refinement (does not change the verdict): the editor↔SDL_RENDERER incompatibility is not Android-specific — the identical breakage occurs for ANY build configured with -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER on desktop, since the editor's ImGui path (MeshCraftApplication.cpp:212-213) is hardwired
- **Blocked:** No Android NDK in this environment; also intersects CNA backend behavior (out of scope).

### AUD-024 `[TODO]` `P2` `W11` · Even if un-parked, CI never builds or tests the actual editor (18.5k LOC) — only 4 standalone CLI/format libs
- **Component:** .github_/workflows/ci.yml
- **Evidence:** ci.yml:39-46 restricts the matrix to `component: [ mc3, mcb, mc3togltf, mc3tomcb ]` and configures each standalone (`cmake -S ${{ matrix.component }}`). ci.yml:19-24 admits the gap: "TODO (not yet wired up): a full editor build job. The root project pulls in the CNA sibling repo ... That covers the remaining root-level tests (smoke_test, xsd_validation, mc3_registry, mc3_ai, mc3_commands, and the render-labeled tests — 20 tests...)". The uncovered `src/MeshCraft/**` editor is 32 .cpp files / 18,
- **Outcome:** Add an editor build+test job (checkout ../cna + ../sharp-runtime, install SDL3/GL/xvfb) so the smoke/render/xsd/registry/ai CTest suite is exercised, or accept and document that the editor is validated only by hand.
- **Tests:** New CI job runs `ctest` at the repo root under xvfb-run and passes the render-labeled tests.
- **Verify note:** Minor line-range imprecision only: the root-level test registrations actually span lines 408–807 (the final add_test, mc3_ai at line 807, and set_tests_properties through ~808), not 408–798 — CMakeLists.txt is 810 lines total. All other numbers (matrix components, 32 files / 18,529 LOC, the TODO quo

### AUD-025 `[TODO]` `P2` `W11` · Compiler warnings enabled on only 2 of the first-party targets; the whole editor + Mcb + mc3tomcb build warning-free, and -Werror is used nowhere
- **Component:** CMakeLists.txt
- **Evidence:** `-Wall -Wextra` appear on exactly two targets: `target_compile_options(Mc3 PRIVATE -Wall -Wextra)` (mc3/CMakeLists.txt:54) and `target_compile_options(mc3togltf_lib PRIVATE -Wall -Wextra)` (mc3togltf/CMakeLists.txt:92). The main editor target is created at CMakeLists.txt:275 (`add_executable(${_target} ${SOURCES})`) with NO warning options anywhere for it — so all 32 files / 18,529 LOC under src/MeshCraft compile with warnings off. Mcb (mcb/CMakeLists.txt), mc3tomcb (mc3tomcb/CMakeLists.txt), an
- **Outcome:** Enable `-Wall -Wextra` on the MeshCraft/Mcb/mc3tomcb targets (ideally via a shared interface target) and turn on `-Werror` in CI so regressions surface, rather than only decorating two library targets.
- **Tests:** Build the editor with `-Wall -Wextra -Werror` and triage/fix the resulting diagnostics; add the flags to the CI configure step.

### AUD-026 `[TODO]` `P2` `W11` · No ASan/UBSan/clang-tidy/clang-format/coverage/fuzz config anywhere, despite parsing untrusted MCB binary + MC3 XML input
- **Component:** CMakeLists.txt (build hardening)
- **Evidence:** Tree-wide grep across all first-party CMakeLists/*.cmake/*.sh/*.yml for `sanitize|clang-tidy|clang-format|gcov|--coverage|lcov|libfuzzer|afl|cppcheck|iwyu` returns zero hits; no `.clang-format`/`.clang-tidy`/`.editorconfig` files exist. The one fuzzing knob present is a disable: `set(MANIFOLD_FUZZ OFF ... FORCE)` (CMakeLists.txt:180). Yet the project parses untrusted, attacker-controllable inputs — the MCB binary reader (mcb/src/McbReader.cpp) and the MC3 XML parser (mc3/src/Mc3XmlParser.cpp) — 
- **Outcome:** Add an opt-in ASan/UBSan build configuration and at least one fuzz/differential harness over McbReader and Mc3XmlParser, wired into CI; add clang-format/clang-tidy config for consistency.
- **Tests:** Run the existing round-trip/parse tests under `-fsanitize=address,undefined` and a short libFuzzer/AFL run over the two parsers to confirm no UB on malformed input.
- **Verify note:** Severity P2 is fair (could also be argued P3 polish). Two evidence refinements for accuracy: (a) clang-tidy (19.1.7) and cppcheck (2.17.1) WERE actually run as one-off manual passes over mc3/src/ per plan_20260710.md STAB-0614/STAB-0620 — so the precise defect is 'no checked-in .clang-tidy/.clang-fo

### AUD-027 `[TODO]` `P2` `W11` · Editor build pulls sibling repos via add_subdirectory(../cna) / SHARP_RUNTIME with no version pin — non-reproducible and unguarded by CI
- **Component:** CMakeLists.txt
- **Evidence:** CMakeLists.txt:107 `add_subdirectory(../cna CNA_dep)` and the link lines at CMakeLists.txt:338/354/365 (`CNA ... SHARP_RUNTIME ...`) consume two sibling repos purely by relative path, with no GIT_TAG, commit, or version check — whatever happens to be checked out at ../cna and ../sharp-runtime is used. README.md:48-50 documents the checkout requirement but not any pinned revision, and README.md:211 records that this is actively fragile: "a *fresh* rebuild now fails — `../sharp-runtime` gained a n
- **Outcome:** Pin the sibling repos to explicit commits/tags (submodule or a recorded SHA + a configure-time check), and gate the editor build in CI against those pinned revisions so cross-repo regressions are caught.
- **Tests:** Add a configure-time assertion that ../cna and ../sharp-runtime are at expected revisions; add a CI editor-build job that fails when they drift/break.
- **Blocked:** Fixing the sibling-repo build regressions themselves is out of scope (../cna and ../sharp-runtime are owned elsewhere); only mesh-craft's pinning/CI wiring is in scope here.

### AUD-028 `[TODO]` `P2` `W13` · NEXT.md and plan.md disagree on completion counts by 5 rows (688/33 vs 683/38)
- **Component:** NEXT.md vs plan.md
- **Evidence:** NEXT.md:12 "**688/723 rows ✅, 33 🟡 (mostly implemented-but-pending-live-visual-verification ...), 0 needs_human, 2 📋**". plan.md:399 summary table "TOTAL 723 | 683 ✅ | 38 🟡 | 0 🧪 | 2 📋". Both agree on 723 total and 2 📋, but differ by exactly 5 on ✅ (688 vs 683) and 🟡 (33 vs 38). These are the two docs each other calls authoritative (plan.md:105/STABILIZATION.md:73 point to plan.md; NEXT.md tracks live status), so a reader cannot tell which is right.
- **Outcome:** Recompute from plan.md + plan_20260710.md row markers with a script and make both files cite the same numbers.
- **Tests:** python3 parse of plan.md/plan_20260710.md status markers per section vs both quoted totals
- **Verify note:** The "688/723 rows ✅, 33 🟡" quote is at NEXT.md:13, not NEXT.md:12. Everything else in the evidence is accurate. Note the finding is understated: NEXT.md:13 literally opens with "`plan.md`: **688/723 ...**", presenting these as plan.md's own counts, while plan.md:399 TOTAL says 683/38 — so it's not m

### AUD-029 `[TODO]` `P2` `W13` · RELEASE.md and plan_deep_audit.md still hard-code 66/66 as the test total
- **Component:** RELEASE.md, plan_deep_audit.md
- **Evidence:** RELEASE.md:35 "**all** tests pass (66/66 as of this writing — check `ctest -N` for the current count, since it grows over time)" and :39 "`mc3togltf` 41/41". plan_deep_audit.md:11 "The native Linux build is clean and 66/66 tests pass"; :13 "all 66 CTest tests are properly registered, no orphaned test binaries" and "650 `STAB-XXXX` tasks in `plan.md`, 620 ✅". Actual is 87 registered / 723 tasks. RELEASE.md at least hedges ('check ctest -N'); plan_deep_audit.md states 66 flatly.
- **Outcome:** Bump RELEASE.md to 87 (or make it purely 'check ctest -N'); update plan_deep_audit.md's summary to 87/723 or mark the file historical (all its AUDIT tasks are now 'completed' per plan_deep_audit.md:23).
- **Tests:** grep -n '66/66\|650\|620' RELEASE.md plan_deep_audit.md vs ctest -N and plan.md:399
- **Verify note:** Severity is better classified as P3 (doc polish/staleness), not P2. The RELEASE.md half is substantially self-mitigated: both cited lines say "as of this writing" and the main one explicitly instructs the reader to "check ctest -N for the current count, since it grows over time" — intentional snapsh

### AUD-030 `[TODO]` `P2` `W13` · plan.md's 'Known Test Suite (as of 2026-06-27)' section still lists 15 tests
- **Component:** plan.md
- **Evidence:** plan.md:46 heading "## Known Test Suite (as of 2026-06-27)" then :51 "15 CTest tests registered, all passing" followed by a per-test table. This is 6x below the current 87. It is dated, but it sits inline in the active plan (not in an archive) with no 'superseded' banner, so a reader scanning plan.md hits '15 tests' before reaching any current figure.
- **Outcome:** Either delete this section (its content is fully superseded by TESTING.md/ctest -N) or move it under a clearly-labeled Historical heading.
- **Tests:** n/a (doc)
- **Verify note:** The "15 CTest tests registered, all passing" statement is at plan.md:48 (not :51); the per-test table spans lines 50-66. The section heading is explicitly dated "(as of 2026-06-27)" but no "superseded" banner marks the section, and the same file's current figure of "87/87 ctest green" appears far be

### AUD-031 `[TODO]` `P2` `W13` · web_issues.md is fully superseded — both of its 'unresolved' issues were later overturned by NEXT.md
- **Component:** web_issues.md vs NEXT.md / README.md
- **Evidence:** web_issues.md:58 "## 2. Emscripten build no longer completes from scratch (new regression)" — overturned by NEXT.md:81 "the Emscripten web build regression (§4) is resolved ... builds with 0 errors." web_issues.md:22-42 roots the blank canvas in "`<canvas ...>` ... `width=\"0\" height=\"0\"` ... inside SDL3's own Emscripten video backend" — overturned by NEXT.md:67-71 "it's actually an uncaught crash, not a sizing-config issue ... The old theory (`<canvas width=0 height=0>`, a config/sizing prob
- **Outcome:** Archive web_issues.md to docs/history/ (or rewrite around NEXT.md's crash root-cause) and update README.md:212-213's Web rows to the crash story so README stops citing the retracted sizing theory.
- **Tests:** Diff web_issues.md §1/§2 against NEXT.md:67-72 and :81; grep README.md:213 for 'width="0"'
- **Verify note:** All claims verified as-is; severity P2 stands. One strengthening addition: README.md:212 not only repeats the stale root cause but states the web app has "no crash," which now directly contradicts NEXT.md:65/70 ("the web build currently crashes before rendering a single ImGui frame" / "The app dies 

### AUD-032 `[TODO]` `P2` `W13` · README Web build description claims it 'loads and initializes correctly' — NEXT.md says it crashes before the first frame
- **Component:** README.md vs NEXT.md
- **Evidence:** README.md:119-124 "Builds and links cleanly (449/449 objects) and the produced page loads and initializes correctly in a real browser (WebGL 2.0 context, `SDL_CreateWindow` succeeds, no console errors) — **but the 3D viewport currently renders a blank canvas**, an open, undiagnosed limitation". README.md:212 Web 'App launches / runs' 🟡 "loads, initializes ... no crash". NEXT.md:69-71 contradicts: on the first resize event CNA "Throws `std::runtime_error`, uncaught anywhere in the chain, kills th
- **Outcome:** Update README's Web section and Platform Matrix rows to NEXT.md's crash finding; drop 'undiagnosed' (it is now diagnosed) and 'no crash'.
- **Tests:** Diff README.md:119-124 & :212-213 against NEXT.md:65-72
- **Verify note:** Evidence is precise and accurate as stated. Severity P2 is defensible as an internal documentation inconsistency; however, per the rubric ("untruthful capability claim" = P1) it arguably warrants P1, since README asserts a false positive status ("no console errors", "no crash") about a build that th

### AUD-033 `[TODO]` `P2` `W4` · 26 EditorAlgorithms.hpp *Alg functions are test-only parallel copies with zero production call sites — production runs a separate hand-copied implementation (Gate B)
- **Component:** include/MeshCraft/EditorAlgorithms.hpp (whole file)
- **Evidence:** Grepping each *Alg name for real call sites (`name(` in src/ and mc3togltf/src/, excluding comments) returns production_call_sites=0 for: mergeDocumentsAlg, collectObjectIdsAlg, resolveObjectIdCollisionsAlg, duplicateObjectsAlg, groupObjectsAlg, ungroupObjectAlg, autoSavePathAlg, autoSaveTickAlg, rotateBackupsAlg, exportSelectionAlg, insertAnimKeyframesAlg, loadPrefsAlg, savePrefsAlg, saveMacroAlg, loadMacroAlg, loadRecentFilesAlg, saveRecentFilesAlg, addRecentFileAlg, keyBindToStringAlg, keyBin
- **Outcome:** Where the *Alg body is CNA-free (duplicateObjectsAlg, groupObjectsAlg, ungroupObjectAlg, autoSaveTickAlg, rotateBackupsAlg, mergeDocumentsAlg, collect/resolveObjectIds, exportSelectionAlg, insertAnimKeyframesAlg…), delete the production duplicate and have the real .cpp call the *Alg function (as alr
- **Tests:** After rewiring, the existing editor_commands tests now cover the real code path; add a build assertion / grep gate that no *Alg has a byte-identical sibling in src/.
- **Verify note:** Line numbers for several Alg definitions are cited a few lines into the function body rather than at the signature: duplicateObjectsAlg is at :303 (not :318), groupObjectsAlg :326 (not :341), autoSaveTickAlg :974 (not :989), rotateBackupsAlg :1001 (not :1016), mergeDocumentsAlg :1163 (not :1178). Pr

### AUD-034 `[TODO]` `P2` `W4` · loadPrefsAlg mirror omits the input clamping that production loadPrefs performs — a hand-edited prefs.ini with out-of-range values is silently accepted
- **Component:** include/MeshCraft/EditorAlgorithms.hpp:1631 vs src/MeshCraft/MeshCraftApplication_FileOps.cpp:683
- **Evidence:** The mirror header (EditorAlgorithms.hpp:1590-1598) claims to mirror loadPrefs()/savePrefs() and says "An unknown key or an unparseable value is silently skipped". But loadPrefsAlg (EditorAlgorithms.hpp:1631-1638) assigns raw values: `if (key=="snapScale") p.snapScale = std::stof(val);` with no bounds. Production loadPrefs (FileOps.cpp:683-688) clamps every field — `snapScale_ = std::clamp(std::stof(val),0.01f,100.0f)` etc — precisely so "a hand-edited prefs.ini can't set a value neither slider c
- **Outcome:** Add the same std::clamp bounds to loadPrefsAlg so it faithfully mirrors production, or route the test through production loadPrefs.
- **Tests:** Extend testPrefsPersistenceRoundTrip: write snapScale=-5 / autoSaveInterval=99999 to the ini, load, assert clamped to 0.01 / 300.0.
- **Verify note:** Line citations in the finding are off. Corrected: (1) Mirror header comment is at EditorAlgorithms.hpp:1575-1583 (finding said 1590-1598, which is actually inside the PrefsAlg struct/savePrefsAlg). (2) loadPrefsAlg is at EditorAlgorithms.hpp:1606-1625, with the uncllamped snapScale assignment at lin

### AUD-035 `[TODO]` `P2` `W4` · MeshCraftPrivate.hpp keeps byte-identical duplicates of five EditorAlgorithms.hpp helpers, and both copies are compiled into the same translation units
- **Component:** src/MeshCraft/MeshCraftPrivate.hpp vs include/MeshCraft/EditorAlgorithms.hpp
- **Evidence:** objectTypeName (Private.hpp:139-158) is identical to objectTypeNameAlg (EditorAlgorithms.hpp:30-49); deepCopyObject (Private.hpp:78-85) == deepCopyObjectAlg (:53-60); findParentList (Private.hpp:87-99) == findParentListAlg (:64-76); removeFromList (Private.hpp:68-76) == removeFromListAlg (:80-88); applyRenamePattern (Private.hpp:160-181) == applyRenamePatternAlg (:94-116). MeshCraftApplication_Commands.cpp includes BOTH headers (lines 2-3), so both objectTypeName and objectTypeNameAlg, both appl
- **Outcome:** Delete the Private.hpp duplicates and have production include/use the EditorAlgorithms.hpp *Alg versions (they are already CNA-free), leaving one tested implementation of each helper.
- **Tests:** No behavior change expected; existing rename/duplicate tests should still pass against the single surviving copy.
- **Verify note:** Retitle to FOUR byte-identical duplicates, not five. Correct pairs and lines: removeFromList (src/MeshCraft/MeshCraftPrivate.hpp:51-59) vs removeFromListAlg (include/MeshCraft/EditorAlgorithms.hpp:65-73); deepCopyObject (Private.hpp:61-68) vs deepCopyObjectAlg (EditorAlgorithms.hpp:38-45); findParen

### AUD-036 `[TODO]` `P2` `W4` · convertToDefinition drops the object's layer on the replacement Instance, while the sibling exportSubtreeAsTemplate preserves it
- **Component:** include/MeshCraft/EditorAlgorithms.hpp:412 vs src/MeshCraft/MeshCraftApplication_Commands.cpp:553
- **Evidence:** convertToDefinition (Commands.cpp:507-518) delegates to convertToDefinitionAlg, whose new Instance copies id/name/type/definition/transform/visible/tags but NOT layer: EditorAlgorithms.hpp:412-419 has no `inst->layer = src->layer;`. The near-identical exportSubtreeAsTemplate, which also replaces the source with an Instance, DOES preserve it: `inst->layer = src->layer;` (Commands.cpp:553). layer is a real, filter-affecting field (Mc3Object.hpp:73 `std::string layer; // named layer`; SceneHierarch
- **Outcome:** Add `inst->tags = src->tags;`-adjacent `inst->layer = src->layer;` in convertToDefinitionAlg so it matches exportSubtreeAsTemplate and preserves the object's layer.
- **Tests:** Unit test: object with layer="bg", convertToDefinitionAlg, assert returned Instance->layer=="bg".
- **Verify note:** Line-number corrections: (1) The exportSubtreeAsTemplate layer-preserving line is Commands.cpp:555 (`inst->layer = src->layer;`), not 553 — line 553 is `inst->transform = src->transform;`. (2) The convertToDefinitionAlg field-copy block that omits layer is EditorAlgorithms.hpp:398-404 (not 412-419; 

### AUD-037 `[TODO]` `P2` `W7` · Per-object UV mapping (projection/scale/offset/rotation) is silently ignored by the exporter
- **Component:** mc3togltf/src/GltfExporter.cpp / MeshBuilder.cpp
- **Evidence:** Mc3Object carries `std::optional<Mc3UvMapping> uvMapping;` (Mc3Object.hpp:91; Mc3UvMapping has projection + scaleU/V, offsetU/V, rotation, Mc3Object.hpp:21-28). grep across mc3togltf/src for uvMapping/scaleU/offsetU returns NONE — no exporter code reads it. buildMesh/buildPrimitive emit only the primitive's built-in TEXCOORD_0. A user who set UV scale=4 or a Box/Sphere projection in the editor gets 1x planar UVs in the export, with no warning. MC3_FORMAT.md lists 'Materials (PBR, textures) ✅', i
- **Outcome:** Apply Mc3UvMapping (scale/offset/rotation and projection) to generated texcoords, or warn that authored UV mapping is not exported.
- **Tests:** Export an object with uvMapping scaleU=4; assert exported TEXCOORD_0 U range spans ~4x, or a warning is emitted.
- **Verify note:** Evidence is accurate except one nuance: the finding implies the editor viewport applies uvMapping while only the export drops it. In fact the editor renderer (src/MeshCraft/Renderer/*) also never reads uvMapping — a grep shows only PropertiesPanel.cpp (the property editor) touches it. So uvMapping i

### AUD-038 `[TODO]` `P2` `W7` · embed: mesh source is treated as a literal OBJ path — node exports with no mesh while export exits 0 'Written'
- **Component:** mc3togltf/src/GltfExporter.cpp buildMesh()
- **Evidence:** GltfExporter.cpp:586-593: `if (obj.type == ObjectType::Mesh && !obj.meshSource.empty()) { try { md = loadObjMesh(ctx.basePath, obj.meshSource); } catch (...) { std::cerr << Warning ...; ctx.stats.warnings++; return -1; } }`. The exporter never checks for the `embed:<id>` form the mc3 parser/writer round-trip (Mc3XmlParser.cpp:743-749). `loadObjMesh` tries to open a file literally named 'embed:tree', fails, warning printed, node gets no mesh. main.cpp:93 then prints 'Written:' and returns 0. Docu
- **Outcome:** Resolve embed:<id> against doc.embeds (parse the referenced/inline GLB and merge its meshes), or make the missing-geometry case a non-zero exit / clearer failure rather than 'Written' success.
- **Tests:** The existing embed_mesh_source_test.py locks in the degraded behavior; add resolution or assert a distinct exit/status when geometry is dropped.
- **Verify note:** Evidence is accurate; no correction needed. Severity P2 is appropriate: the geometry loss is signalled by a stderr Warning and the stats.warnings counter (only shown with --show-stats), and the behavior is documented in MC3_FORMAT.md and locked in by a passing test (mc3togltf/test/embed_mesh_source_

### AUD-039 `[TODO]` `P2` `W7` · --stats 'Warnings' count is untruthful — several warning paths never increment stats.warnings
- **Component:** mc3togltf/src/GltfExporter.cpp
- **Evidence:** ctx.stats.warnings++ is called only at GltfExporter.cpp:591 (OBJ load fail), 809 (unknown definition), and 849 (approximate CSG). These warning paths print to stderr but do NOT increment it: unknown material at 765 `std::cerr << "Warning: object '" ... unknown material ...` (no ++); SVG-slot warnings in buildMaterial (496-501, function has no ctx); ambient-light drop at 935 `std::cerr << ... ambient light ... omitted`; duplicate node name at 1476. So `--stats` 'Warnings: N' (main.cpp:107) underc
- **Outcome:** Route all warning emissions through a single counter (or thread ctx/stats into buildMaterial and addLights) so stats.warnings matches the warnings actually printed.
- **Tests:** Export a scene that triggers an unknown-material and an ambient-light warning; assert stats.warnings equals the number of warnings printed.
- **Verify note:** Evidence understates the issue: even more warning paths omit the increment — line 393 (could not detect image format), 441 (texture not found for embedding), and 1177/1199 (action warnings) also print 'Warning:' without incrementing stats.warnings. Severity P2 is appropriate (misleading reported sta

### AUD-040 `[TODO]` `P2` `W7` · Pivoted object + translation animation loses the pivot offset — object jumps when its translation is animated
- **Component:** mc3togltf/src/GltfExporter.cpp buildNode()/exportAnimations()
- **Evidence:** For a pivoted object the outer node's static translation includes the pivot: GltfExporter.cpp:738-743 `node.translation = { t.position[i] + t.pivot[i] }`. Animation channels target that same outer node by name (nodeNameMap), but the translation sampler builds values from the pivot-free base transform: GltfExporter.cpp:1259-1260 `base[0] = baseT.position[0]; ...` (baseTransforms is obj.transform, no pivot), then valueData uses base for non-animated axes and evaluateChannel (also pivot-free) for a
- **Outcome:** Add the pivot offset into the animated translation values (or target the inner _origin node), so animated and static poses agree for pivoted objects.
- **Tests:** Export an object with a non-zero pivot and a translation channel; assert the sampler's value at t=keyframe0 equals the static node translation.
- **Verify note:** Jump magnitude is precisely -pivot*unitScale (unitScale is applied to both the static translation at 908-912 and the animated values at 1287). Severity is arguably understated: this silently emits incorrect exported animation output (visible discontinuity), which matches P1 (wrong behavior / silent 

### AUD-041 `[TODO]` `P2` `W7` · Rotation animation exported only as LINEAR/STEP quaternion samples (never CUBICSPLINE); euler-space keyframe interpolation is replaced by quaternion shortest-path lerp
- **Component:** mc3togltf/src/GltfExporter.cpp exportAnimations()
- **Evidence:** Rotation output is baked to per-keyframe quaternions (GltfExporter.cpp:1269-1280 `auto q = eulerToQuat(euler...); valueData.push_back(q[0..3])`) and the sampler uses `interp` = STEP or LINEAR (1255, 1343). glTF LINEAR rotation interpolation is normalized quaternion lerp along the SHORTEST arc between consecutive samples. For plain (non-cubic) LINEAR keyframes only the original keyframe times are emitted, so a euler channel that sweeps a component >180° between two keyframes (e.g. 0°→270° about o
- **Outcome:** Either densely sample large-angle rotation keyframes (as done for cubic) so quaternion lerp tracks the euler path, or document that rotation channels with >180° per-segment component deltas are re-pathed.
- **Tests:** Export a rotation channel 0°→270° over two keyframes; assert intermediate sampled orientation matches the editor's euler interpolation (not the -90° short path).
- **Verify note:** Two refinements. (1) Terminology: glTF LINEAR interpolation for rotation channels is spherical linear interpolation (slerp), not plain "normalized quaternion lerp"; both take the shortest arc, so the finding's conclusion is unchanged. (2) Severity: P2 understates it. This silently produces animation

### AUD-042 `[TODO]` `P2` `W0` · ImGui context and SDL3/OpenGL3 backends never shut down (no destructor/UnloadContent/Dispose)
- **Component:** src/MeshCraft/MeshCraftApplication.cpp (LoadContent)
- **Evidence:** LoadContent creates ImGui state that is never released: MeshCraftApplication.cpp:130 `ImGui::CreateContext();`, :212 `ImGui_ImplSDL3_InitForOpenGL(sdlWindow, glCtx);`, :213 `ImGui_ImplOpenGL3_Init("#version 300 es");`. grep for `ImGui::DestroyContext`, `ImGui_ImplOpenGL3_Shutdown`, `ImGui_ImplSDL3_Shutdown` over src/+include/ returns nothing. MeshCraftApplication.hpp declares no `~MeshCraftApplication`, and does not override the CNA hooks `virtual void UnloadContent()` (Game.hpp:253) or `virtual
- **Outcome:** Override UnloadContent() (or add a destructor) that calls ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext(); in reverse init order.
- **Tests:** Run the app to exit under a GL/leak checker (e.g. valgrind/apitrace) and confirm the ImGui GL objects and context are released; assert the shutdown path executes.

### AUD-043 `[TODO]` `P2` `W0` · shadowDebug FBO and its two textures (member handles) are never deleted by any code path
- **Component:** src/MeshCraft/MeshCraftApplication.cpp (initShadowDebug)
- **Evidence:** initShadowDebug generates GL objects into MeshCraftApplication member fields: MeshCraftApplication.cpp:1568 `gl.GenTextures(1, &shadowDebugColorTex_);`, :1577 `gl.GenTextures(1, &shadowDebugDepthTex_);`, :1586 `gl.GenFramebuffers(1, &shadowDebugFbo_);`. grep for shadowDebug shows these are only ever created, bound, and read (UiOverlays.cpp:2199 ImGui::Image); no glDeleteTextures/glDeleteFramebuffers ever references them. Unlike the s_bloom pool, these live in the MeshCraftApplication object, so 
- **Outcome:** Delete shadowDebugFbo_/shadowDebugColorTex_/shadowDebugDepthTex_ (guarded by non-zero) in the shutdown hook, and reset the handles to 0.
- **Tests:** Enable shadow-map debug, exit, and confirm the FBO + 2 textures are deleted (GL object counter or apitrace); add a regression test that the handles are freed.

### AUD-044 `[TODO]` `P2` `W0` · Detached AI worker thread is never joined; may run httplib/OpenSSL during process-exit static destruction
- **Component:** src/MeshCraft/AiAssistant.cpp (sendAsync)
- **Evidence:** sendAsync spawns and immediately detaches a network worker: AiAssistant.cpp:180 `std::thread([...]() {...}).detach();` (`.detach()` at :264). reset() (:148-163) only drops the shared_ptr result box and explicitly does NOT wait for the worker ('no blocking anywhere', comment :149-156). isInFlight() (:126) is polled but nothing joins or cancels the thread at exit. main.cpp returns after app.Run() with no shutdown barrier. If an AI request is still in flight when the process exits, the detached thr
- **Outcome:** Track the in-flight worker and, on shutdown, either join it (with a bounded timeout) or ensure the process does not begin static/OpenSSL teardown while a request thread is live; at minimum document/guard the exit ordering.
- **Tests:** Start a request against a slow/blocking mock endpoint, trigger app exit mid-request, and run under TSan/ASan to confirm no thread is executing library code during static destruction.

### AUD-045 `[TODO]` `P2` `W6` · Known-key value decoding ignores the declared tag byte (no type validation on the hot read path)
- **Component:** mcb/src/McbReader.cpp — readObject/readDocument and all read* deserializers
- **Evidence:** For every recognized key the reader reads the tag byte but never validates it against the expected type; it decodes the value purely by key name. readObject reads `uint8_t tag = rU8(in);` (McbReader.cpp:383) then for "type" does `obj->type = static_cast<Mc3::ObjectType>(rI32(in));` (line 384) and for "visible" does `obj->visible = rU8(in) != 0;` (line 388) — `tag` is used ONLY in the final `else skipValue(in, tag);` (line 446), never checked for known keys. Same pattern in every read* helper (e.
- **Outcome:** For recognized keys, verify the read tag equals the expected tag before decoding (throw "MCB: type mismatch for key ..." otherwise), so a corrupt/hostile tag/value mismatch is detected at the field instead of silently desyncing into a wrong-but-parsed document. Bounds already hold (rRawStr/rU32Bound
- **Tests:** Add a corruption test in mcb_roundtrip_test.cpp: hand-write a stream with a known key (e.g. "visible") carrying a wrong tag and assert loadFromBinary throws a type-mismatch error rather than succeeding or throwing an unrelated 'unknown tag' later.
- **Verify note:** Evidence path correction: the header is at mcb/include/MeshCraft/Mcb/McbFormat.hpp (not bare "McbFormat.hpp:6-20"); the no-checksum claim is still accurate (lines 6-32 define only MAGIC/VERSION/MIN_SUPPORTED_VERSION/FLAG_COMPRESSED and TAG_* constants). The readTransform "206-209" and readMaterial "

### AUD-046 `[TODO]` `P2` `W0` · mc3togltf helix extrude divides by zero -> NaN/inf geometry written to glTF as success
- **Component:** mc3togltf/src/MeshBuilder.cpp (samplePath, PT::Helix)
- **Evidence:** MeshBuilder.cpp:748 computes the path tangent as `float tx = -std::sin(t), ty = h / (r * totalAngle), tz = std::cos(t);` where `r = path.helixRadius` (MeshBuilder.cpp:741) and `totalAngle = 2*pi*turns`. The parser applies NO lower bound: `path.helixRadius = attrF(el, "radius", 0.5f);` (Mc3XmlParser.cpp:139), `path.helixTurns = attrF(el, "turns", 4.0f);` (Mc3XmlParser.cpp:141). With `helix_radius="0"` (or `turns="0"`), `r*totalAngle==0` so `ty = h/0` = +/-inf, then `tl=sqrt(...)`=inf and `ty/tl`=
- **Outcome:** Guard the helix tangent against a zero denominator (e.g. compute the raw derivative vector then normalize, matching SceneRenderer_Extrude.cpp), and/or add a non-finite geometry check in GltfExporter before writing accessors (mirroring the OBJ path's isfinite rejection) so degenerate parametric input
- **Tests:** Add an mc3togltf test: an <extrude> whose <path type="helix" radius="0"> (and a second with turns="0") must either export finite POSITION/min/max or fail with a clear error, never emit NaN. Assert all exported POSITION floats and accessor min/max are
- **Verify note:** Evidence is accurate; two refinements. (1) The parser file is mc3/src/Mc3XmlParser.cpp (invoked via Mc3Document::loadFromFile in main.cpp), not a mc3togltf-local Mc3XmlParser.cpp — the line numbers 139/141 are exact. (2) Severity is understated, not inflated: producing a spec-invalid glTF (NaN posit

### AUD-047 `[TODO]` `P2` `W0` · loadObjMesh indexes attrib arrays with unvalidated tinyobj face indices (potential OOB read)
- **Component:** mc3togltf/src/MeshBuilder.cpp (loadObjMesh)
- **Evidence:** MeshBuilder.cpp:1188-1191 reads `auto vi = static_cast<size_t>(idx.vertex_index); m.positions.push_back(attrib.vertices[3*vi+0]); ...[3*vi+1]; ...[3*vi+2];` with no check that `3*vi+2 < attrib.vertices.size()`. Same for normals (MeshBuilder.cpp:1194-1197: `attrib.normals[3*ni+2]`) and texcoords (MeshBuilder.cpp:1204-1207: `attrib.texcoords[2*ti+1]`), and in the face-normal fallback (MeshBuilder.cpp:1176-1177). The code already validates vertex *values* are finite (MeshBuilder.cpp:1145-1150) but 
- **Outcome:** Bounds-check each index against the corresponding attrib array size before subscripting (e.g. skip or reject the face/vertex when `3*vi+2 >= attrib.vertices.size()`, `ni<0 || 3*ni+2 >= attrib.normals.size()`, `ti<0 || 2*ti+1 >= attrib.texcoords.size()`), throwing a clear error like the finite-vertex
- **Tests:** Add an mc3togltf test that imports a hand-crafted malformed .obj whose face references a vertex index beyond the vertex count; it must fail with a clear error, not crash or read OOB (verify under ASan).
- **Verify note:** Mechanism precision: the OOB is reachable specifically because a triangle face (npolys==3) with triangulate=true bypasses the (3*vi+2)>=v.size() guards that protect the quad/polygon triangulation paths, landing in the unchecked else branch at tiny_obj_loader.h:1965-1979; tinyobjloader emits only a n
- **Blocked:** Confidence is PLAUSIBLE: proving an actual OOB requires confirming tinyobjloader passes through out-of-range indices, and tinyobjloader is vendored under _deps (out of audit scope). The first-party missing bounds check is certain; the trigger depends on loader behavior.

### AUD-048 `[TODO]` `P2` `W9` · GLB/GLTF export reports unqualified 'Exported' success in the UI even when the exporter counted warnings or skipped geometry (warnings only go to stdout)
- **Component:** src/MeshCraft/MeshCraftApplication_FileOps.cpp, mc3togltf/src/GltfExporter.cpp
- **Evidence:** /rv/data/development/github.com/openeggbert/mesh-craft/src/MeshCraft/MeshCraftApplication_FileOps.cpp:282-297 builds `statusMsg = "Exported " + ... + " (" + uniqueMeshes + " meshes"...)` and unconditionally `setStatusMsg(statusMsg);`. The warning count is emitted only to std::cout at line 300 (`... << s.warnings << " warnings"`), never surfaced in the on-screen status. The exporter drops geometry while merely incrementing counters: GltfExporter.cpp:846-849 approximate-CSG path (`ctx.stats.warnin
- **Outcome:** Surface s.warnings (and approximate-CSG usage) in the user-facing status message and mark it as a warning-colored status when warnings>0, so a partial/approximate export is not presented as a clean success.
- **Tests:** Export a scene containing an unsupported/approximate CSG node with allowApproximateCSG and assert the returned status string is flagged as a warning and includes the warning count.

### AUD-049 `[TODO]` `P3` `W11` · CI FetchContent cache key omits the file where 3 of 4 components' real dependency is pinned — documented invalidation guarantee is inaccurate
- **Component:** .github_/workflows/ci.yml
- **Evidence:** ci.yml:60-71 keys the `build/_deps` cache on `hashFiles(format('{0}/CMakeLists.txt', matrix.component))` and comments (ci.yml:61-64) that this is "keyed on the component's own CMakeLists.txt content so a dependency version bump (a GIT_TAG/URL change in that file) invalidates the cache." But mcb, mc3togltf, and mc3tomcb fetch their only external dep (tinyxml2) transitively via `add_subdirectory(../mc3 ...)` (mcb/CMakeLists.txt:14-17, mc3togltf/CMakeLists.txt:32-35, mc3tomcb/CMakeLists.txt:11-14),
- **Outcome:** Either include mc3/CMakeLists.txt in the hash for the mc3-dependent components, or reword the comment so it doesn't overstate the invalidation guarantee.
- **Tests:** Bump the tinyxml2 GIT_TAG in mc3/CMakeLists.txt and confirm the mcb/mc3togltf/mc3tomcb cache keys change (currently they do not).
- **Verify note:** Additional context that the evidence omits but that reinforces (does not undermine) the low P3 severity: the entire workflow is deactivated — its own banner at ci.yml:3-7 states the file lives under `.github_/` (trailing underscore) specifically so GitHub does NOT run it, meaning the cache step neve

### AUD-050 `[TODO]` `P3` `W4` · Batch-rename live preview uses a parallel rename copy that ignores lockedIds, so it shows locked objects being renamed when Apply skips them
- **Component:** src/MeshCraft/MeshCraftApplication_UiOverlays.cpp:564 vs include/MeshCraft/EditorAlgorithms.hpp:152
- **Evidence:** The preview loop (UiOverlays.cpp:564-569) calls applyRenamePattern for the first 3 selected objects unconditionally and displays "old → new" for each. The actual apply, batchRenameObjects (EditorAlgorithms.hpp:152-164), skips locked objects: `if (lockedIds.count(s->id)) { ++idx; continue; }` — a locked object keeps its name. So a locked object in the top 3 of the selection shows a rename in the preview that will not occur on Apply. (Index numbering itself matches: both advance the counter for ev
- **Outcome:** Have the preview consult lockedIds_ and render locked objects as unchanged (or grey "(locked)"), or drive the preview through the same batchRenameObjects code path used by Apply.
- **Tests:** Manual/UI: select a locked + unlocked object, open Batch Rename, confirm the locked row shows no rename.
- **Verify note:** The EditorAlgorithms.hpp line citation is imprecise. The batchRenameObjects function spans lines 129-150 (not 152-164); the locked-skip `if (lockedIds.count(s->id)) { ++idx; continue; }` is at line 138. Line 152 is actually the "// ── Find-replace helpers ──" comment. Corrected component: src/MeshCr

### AUD-051 `[TODO]` `P3` `W7` · Per-object metadata is silently dropped from glTF, unlike tags/collision which are exported to node extras
- **Component:** mc3togltf/src/GltfExporter.cpp buildNode()
- **Evidence:** Mc3Object has `std::map<std::string,std::string> metadata;` (Mc3Object.hpp:94, an opaque pass-through mirroring <metadata>). The node-extras block (GltfExporter.cpp:882-905) emits mc3_type, tags, and collision but never obj.metadata (grep for .metadata/->metadata in mc3togltf/src returns NONE). So metadata survives mc3/mcb round-trips but is dropped on glTF export with no warning, even though sibling opaque data (tags) is preserved via extras.
- **Outcome:** Serialize obj.metadata into node.extras (e.g. extras['metadata']) alongside tags/collision, or document the drop.
- **Tests:** Export an object with metadata['foo']='bar'; assert node.extras carries it.
- **Verify note:** Full header path is mc3/include/MeshCraft/Mc3/Mc3Object.hpp (line 94 is exactly correct). Note the one grep hit at GltfExporter.cpp:1414 is a comment for DOCUMENT-level metadata (doc.model/version/unit -> asset.extras), not per-object obj.metadata, so it does not contradict the finding.

### AUD-052 `[TODO]` `P3` `W6` · RecursionGuard comment claims two independent depth counters, but both recursion trees share one
- **Component:** mcb/src/McbReader.cpp — RecursionGuard
- **Evidence:** The comment states: "Two independent counters (one per recursion tree: skipValue's skip-path, readObject's real-object-tree path) are used rather than one shared counter" (McbReader.cpp:130-132). But the counter is a `static thread_local int depth_` on the class template (lines 147, 149-150), and BOTH recursion sites instantiate the SAME specialization `RecursionGuard<256>`: skipValue at line 165 and readObject at line 379. One instantiation ⇒ one shared `thread_local depth_`. So the object-tree
- **Outcome:** Either make the counters actually independent (give skipValue and readObject distinct template tags, e.g. RecursionGuard<256, struct SkipTag> vs RecursionGuard<256, struct ObjTag>) to match the comment, or fix the comment to state that a single shared 256 budget is used across both trees. The code a
- **Tests:** No behavior change needed if the comment is corrected; if made independent, add a test that a legal-but-deep combined object+skip nesting (~300 combined) still loads.

### AUD-053 `[TODO]` `P3` `W6` · Enum fields are cast from unvalidated file ints with no range check
- **Component:** mcb/src/McbReader.cpp — enum-typed field reads
- **Evidence:** Every enum field is a raw static_cast of an attacker-controlled int with no validation: `obj->type = static_cast<Mc3::ObjectType>(rI32(in));` (McbReader.cpp:384), `p.primitiveType = static_cast<Mc3::PrimitiveType>(rI32(in))` (line 220), `csg.csgType = static_cast<Mc3::CsgType>(rI32(in))` (251), `cs.type = static_cast<Mc3::CrossSectionType>(rI32(in))` (262), plus LightType (633), CameraType (654), FogMode (674), Interpolation (704), AnimatedProperty (720). A malformed MCB can inject out-of-range 
- **Outcome:** Optionally clamp/validate each enum against its known range on read (fall back to the default enumerator on out-of-range, matching the unknown-key forward-compat philosophy) so a corrupt file cannot inject an enum value no downstream switch handles.
- **Tests:** Add a corruption test feeding an out-of-range ObjectType/PrimitiveType int and asserting the reader either normalizes it to the default or rejects it, rather than storing an unhandled value.

### AUD-054 `[TODO]` `P3` `W6` · MCB_FORMAT.md top-level field-order list is stale (omits rotationUnits, eulerOrder, includedEmbeds)
- **Component:** MCB_FORMAT.md — 'Field key names and document layout'
- **Evidence:** MCB_FORMAT.md:100-106 presents the authoritative writeDocument() order as: "version, model, unit, coordinateSystem, defaultCamera, meta, metadata, includes, includedDefs, includedMaterials, includedTextures, environment, ...". But McbWriter.cpp actually writes `rotationUnits` and `eulerOrder` between coordinateSystem and defaultCamera (McbWriter.cpp:461-462) and writes `includedEmbeds` after includedTextures (McbWriter.cpp:494-497). Both are also read back (McbReader.cpp:768-769, 822-829) and co
- **Outcome:** Update the field-order block in MCB_FORMAT.md to include rotationUnits, eulerOrder, and includedEmbeds in their actual written positions.
- **Tests:** Doc-only; no runtime test.
- **Verify note:** Line numbers are exact; only the file paths in the claim were abbreviated. Full paths: /rv/data/development/github.com/openeggbert/mesh-craft/mcb/src/McbWriter.cpp:461-462 and :494-497; /rv/data/development/github.com/openeggbert/mesh-craft/mcb/src/McbReader.cpp:768-769 and :822-829. Severity P3 sta

### AUD-055 `[TODO]` `P3` `W6` · Compression-flag rejection path has no test despite being a documented guarantee
- **Component:** mcb/test/mcb_roundtrip_test.cpp
- **Evidence:** McbReader.cpp:1004-1005 rejects a file with MCB_FLAG_COMPRESSED set (`throw std::runtime_error("MCB: compressed format not yet supported")`), and MCB_FORMAT.md:19 documents this as a guaranteed behavior ("a file with this bit set is rejected with a clear error, not silently misread"). The test suite exercises version bounds, huge string/count, truncation, all-zeros, single-byte, and deep nesting, but grep shows the only 'compress' reference in tests is testSmoke asserting the writer emits flags=
- **Outcome:** Add a test that hand-writes a valid header with flags=MCB_FLAG_COMPRESSED and asserts loadFromBinary throws with 'compressed format not yet supported'.
- **Tests:** The new test itself is the verification.

### AUD-056 `[TODO]` `P3` `W0` · glTF re-read type-puns via reinterpret_cast from a byte vector (strict-aliasing/alignment UB)
- **Component:** src/MeshCraft/MeshCraftApplication_FileOps.cpp (ReadVec3/ReadVec2/ReadIndex, OBJ export)
- **Evidence:** FileOps.cpp:370 `const float* f = reinterpret_cast<const float*>(&buf.data[offset]);` and FileOps.cpp:379 (ReadVec2) read `float` objects out of `buf.data`, which is tinygltf's `std::vector<unsigned char>` where no `float` object was ever created (reading a value through a type that doesn't match the object's dynamic type is UB), and `offset = bv.byteOffset + acc.byteOffset + i*stride` is not guaranteed 4-byte aligned. ReadIndex is worse: FileOps.cpp:389 `reinterpret_cast<const uint16_t*>(&buf.d
- **Outcome:** Read the bytes into a properly-typed local via std::memcpy (e.g. `float f; std::memcpy(&f, &buf.data[offset], sizeof f);`) instead of reinterpret_cast+deref, which is well-defined and handles misalignment.
- **Tests:** N/A behavioral (self-generated trusted data); a UBSan/ASan run over an OBJ export would flag any alignment issue if a platform enforces it.
- **Blocked:** Low severity: operates only on MeshCraft's own freshly-written GLB, not on untrusted input, so no observed misbehavior on supported platforms.

### AUD-057 `[TODO]` `P3` `W9` · Undo history is a bounded 20-entry whole-document deep-copy stack; oldest entries are silently dropped (informational — answers the audit question, by-design)
- **Component:** include/MeshCraft/MeshCraftApplication.hpp, src/MeshCraft/MeshCraftApplication_Commands.cpp
- **Evidence:** /rv/data/development/github.com/openeggbert/mesh-craft/include/MeshCraft/MeshCraftApplication.hpp:629 `static constexpr int kUndoMax = 20;`. pushUndo() at Commands.cpp:331-336 does `undoStack_.push_back(deepCopyDoc(document_)); if (undoStack_.size() > kUndoMax) undoStack_.erase(undoStack_.begin()); redoStack_.clear();` — so beyond 20 operations the oldest snapshot is silently discarded (no user notice), and each snapshot is a full deep copy of the entire document (all objects/materials/textures/
- **Outcome:** No code change required for correctness. Optionally document the 20-op limit in the UI and/or consider a command-delta model if memory becomes a concern.
- **Tests:** n/a (behavioral note).
- **Verify note:** The `static constexpr int kUndoMax = 20;` is at include/MeshCraft/MeshCraftApplication.hpp:632, NOT line 629 (line 629 is the unrelated `void checkRotationConventionNotice();` declaration). The pushUndo() body spans Commands.cpp:331-337 and uses `static_cast<int>(undoStack_.size()) > kUndoMax` (the 
