# NEXT.md — baseline & handoff

_Last updated: 2026-07-11. Branch `develop`, HEAD `737af77`, working tree clean
at start of session. This session ran a deep evidence-based audit and fixed all
resulting P0 defects; see [`plan.md`](plan.md) for the full backlog._

---

## 1. Baseline (verified now, 2026-07-11)

- **Toolchain:** GCC 14.2, Clang 19.1.7, CMake 3.31.6, Ninja 1.12.1, Python
  3.11.9, Blender present. `emcc` absent (no local web build). Display `:0`
  available. Network up.
- **Configure + build (Release, EASYGL backend):**
  ```bash
  cmake -S . -B b-release            # already configured; reconfigure is a no-op
  cmake --build b-release -j"$(nproc)"
  ```
  Result: **clean build, exit 0.**
- **Tests:** `(cd b-release && ctest -j"$(nproc)")` → **93/93 pass.**
  By label: `render` 20, `export` 62, `format` 5, `unit` 2, `lint` 1,
  `ai` 1, `commands` 1, `registry` 1.
- **Standalone libs** (`mc3`, `mcb`, `mc3tomcb`, `mc3togltf`): build and their
  tests pass as part of the above (`mc3_*`, `mcb_*`, `mc3togltf_*`).
- **Blocked configurations:** Emscripten/web (no `emcc` here; and a CNA-side
  crash blocks the web build even where `emcc` exists — see §4). MinGW/Windows
  (1 remaining failure, all in `../cna`). Android (no NDK).

Distinction of evidence in this file: "verified now" = run in this environment
today; "blocked" = could not run here; older narrative claims are in
[`docs/history/`](docs/history/).

## 2. This session (2026-07-11) — deep stabilization

Ran a 12-dimension audit, adversarially re-verified every finding (57 confirmed,
18 refuted), and fixed all P0s. Commits on `develop`:

| Commit | P | What |
|--------|---|------|
| `afb1159` | P0 | `ActiveTool::Measure` window-title out-of-bounds read → exhaustive `activeToolName()` + test |
| `838eefc` | P1 | Unified `ObjectType`↔name (Torus/Capsule/Disk/Grid/IcoSphere stopped showing as "Object"; macro round-trip fixed) + test |
| `dae910f` | P0 | Reject NaN/Inf floats at MC3 parse (`finiteOr`) + hostile-input test |
| `fd606d2` | P1 | Tessellation-count clamp; instance-cycle depth cap; glTF non-finite export gate + tests |
| `737af77` | P0 | Editor undo never recorded drag/color edits (80 dead sites) → snapshot relocated + source-lint guard |
| `40643a4` | P1 | `Mc3LoadPolicy`: AI/untrusted parsing can't process `<include>` (LFI); in-memory parse |
| `22b7129` | P1 | glTF spot lights no longer at origin; light position/range unit-scaled |
| `dcd0d33` | P1 | `insertAnimKeyframes` mirror converged with production (Gate B) |
| `2d126bf` | P1 | Deterministic shutdown: dangling SDL event watch removed, ImGui/GL teardown |
| `e53af49` | P1 | Stop advertising non-functional editor graphics backends (Gate C) |
| `ac75eb6` | P1 | Ear-clip extrude caps → concave cross-sections triangulate correctly |
| `901965f` | P1 | Confine exporter texture/mesh paths to document root by default |

**Every P0 and every identified P1 from the audit is addressed (Gate A met).**
Full suite: **95/95**, green after each commit. Also: consolidated planning docs
— one active [`plan.md`](plan.md), archived 6 superseded status/plan docs to
[`docs/history/`](docs/history/), and rewrote this baseline.

Not done (owner-gated): CI activation — `.github_/workflows/ci.yml` needs a
`workflow`-scoped push token to move to `.github/`. Parked workflow was made
correct/ready this session.

## 3. Next tasks

See the **Priority execution queue** at the top of [`plan.md`](plan.md). Highest
value next: **AUD-005 / SYS-W2-01** — a load-policy so AI/untrusted MC3 parsing
disables `<include>` and confines resource paths (shared root cause with the
path-traversal findings), plus the W7 glTF export-fidelity fixes.

## 4. Current blockers (external, re-verified 2026-07-11)

- **Web (Emscripten) canvas crash — in CNA, not this repo.** The web build
  loads (window + WebGL2/EasyGL init + scene creation all succeed), then dies on
  the first `SDL_EVENT_WINDOW_RESIZED`: CNA's `GameWindow::queryClientBoundsFromSDL()`
  calls `SDL_GetWindowSize()`, which fails with *"Video subsystem has not been
  initialized"* seconds after the same subsystem worked, throwing an uncaught
  `std::runtime_error` that kills the wasm module. 100% inside CNA
  (`Game.cpp`/`GameWindow.cpp`); no MeshCraft-side hook runs early enough to
  catch it. Blocks web live-verification (including the `STAB-0571` GLB download
  bridge, which is implemented and present in the wasm import table but unrun).
- **Windows GUI (MinGW):** does not compile — 1 remaining failure, all in `../cna`.
- **Web GLB export:** writes to Emscripten MEMFS; the JS download bridge exists
  but is unverified pending the crash above.

Not modifiable from this repo: `../cna`, `../sharp-runtime` (owner permission
required). Record precise repro and continue with in-repo work.

## 5. Known limitations (by design or deferred)

SVG textures parsed/edited but not rasterized; `embed:` mesh refs parsed/edited
but not resolved on export; scripts/triggers are data-model + editing only (no
runtime execution); `rotation_units="radians"`/non-default `euler_order` honored
on export but not in live editor interaction (won't-fix `STAB-0701`, status-bar
warning on load); native file-browse dialog not wired (`CNA_DEVICES` all-or-
nothing flag). These are tracked in [`plan.md`](plan.md) W14 / DEFERRED.

## 6. Architecture notes (load-bearing invariants)

- **CSG dual-path invariant:** `mc3togltf/src/CsgEvaluator.cpp` (export) and
  `SceneRenderer`'s CSG preview cache (editor) both key off `isCutter`/
  `role="cutter"`. A missing cutter flag silently turns a subtraction into a
  union. Keep both in sync if CSG semantics change.
- **Primitive dual-path invariant:** `mc3togltf/src/MeshBuilder.cpp::buildPrimitive()`
  (export + CSG) and `SceneRenderer`'s primitive dispatch (editor unit-mesh +
  scale + LOD) are two independent geometry generators for all 11 `PrimitiveType`
  values. No automated cross-check yet (see plan W7 SYS-W7-02).
- **Triangle winding differs by convention between those two paths — do not
  "fix" one to match the other.** Export (`MeshBuilder.cpp`) targets
  **CCW-from-outside** (glTF/OpenGL). Editor preview
  (`SceneRenderer_Builders.cpp`) needs **CW-from-outside** (CNA's default
  `RasterizerState` is `CullCounterClockwiseFace`). Getting it backwards renders
  the mirrored interior — looks see-through, not obviously wrong.
- **`Alg` mirror pattern:** pure logic in CNA-free headers for headless testing.
  Most mirrors are the single source of truth their `.cpp` calls into, but a few
  (documented in-header) are intentionally-unwired duplicates. Check whether an
  `Alg` function is actually called before assuming a fix there takes effect.
  (Audit note: `EditorAlgorithms.hpp` IS production — included by 8 `.cpp` files.)
- **Undo/redo:** snapshot-based (whole-document deep copy), not command-diff.
- **`mc3.xsd` is compiled into the binary at configure time** — editing it needs
  a reconfigure, not just a rebuild.
- **XML comment gotcha:** a literal `--` inside an XML comment is rejected by
  `lxml`/`test/validate_xsd.py` though `tinyxml2` tolerates it. Run
  `test/validate_xsd.py` on new fixtures.
- **`AiAssistant` threading invariant:** background HTTP runs on a detached
  `std::thread` writing into a `shared_ptr<AiRequestResult>` (atomic done flag +
  mutex). Never `std::async`/`std::future` (destructor-blocking hang-on-close).
- **`CNA_ENABLE_NET` must stay `OFF`** — unused CNA subsystem that fails to
  compile; re-enabling breaks the default build.
- **API/compat boundaries:** `Mc3Document`'s public API is depended on by
  `mc3togltf` and every test fixture — additive changes only; check both. CNA /
  SHARP_RUNTIME must not be modified without owner permission.

## 7. Useful commands

```bash
# Configure + build (this session used system cmake on b-release successfully).
# For the Debug tree, the project historically requires CLion's bundled cmake:
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -G Ninja
cd cmake-build-debug && ninja -j"$(nproc)"

# Release tree used this session:
cmake -S . -B b-release && cmake --build b-release -j"$(nproc)"

# Test
(cd b-release && ctest --output-on-failure)        # full suite (93)
(cd b-release && ctest -N)                          # list registered tests
(cd b-release && ctest -R undo_snapshot_lint -V)    # this session's lint guard

# XSD validation of a new/changed fixture
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# Static audits
python3 test/xsd_docs_diff.py            # MC3_FORMAT.md vs XSD drift
python3 test/undo_coverage_audit.py .    # undo-coverage candidates

# Run / demo
./b-release/MeshCraft test/house.mc3.xml
./b-release/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
./b-release/mc3tomcb/mc3tomcb test/house.mc3.xml /tmp/out.mcb
```
