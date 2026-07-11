# NEXT.md — baseline & handoff

_Last updated: 2026-07-11 (session 2). Branch `develop`, working tree clean at
the start of each session below. See `git log --oneline -20` for the exact
current HEAD — it is not hard-coded here because this file is edited in the
same commits it describes, which would make a literal hash stale immediately.
See [`plan.md`](plan.md) for the full backlog and
`python3 test/validate_plan_consistency.py . <build-dir>` to mechanically
check this file and `plan.md` haven't drifted apart again._

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
- **Tests:** `(cd b-release && ctest -j"$(nproc)")` → all pass. Run
  `ctest -N` for the exact live count (do not trust a hard-coded number in
  prose — it changes every session; `test/validate_plan_consistency.py`
  cross-checks it against this file when a build dir is available).
- **Standalone libs** (`mc3`, `mcb`, `mc3tomcb`, `mc3togltf`): build and their
  tests pass as part of the above (`mc3_*`, `mcb_*`, `mc3togltf_*`).
- **Blocked configurations:** Emscripten/web (no `emcc` here; and a CNA-side
  crash blocks the web build even where `emcc` exists — see §4). MinGW/Windows
  (1 remaining failure, all in `../cna`). Android (no NDK).

Distinction of evidence in this file: "verified now" = run in this environment
today; "blocked" = could not run here; older narrative claims are in
[`docs/history/`](docs/history/).

## 2. Session log

### Session 1 (2026-07-11) — deep stabilization

Ran a 12-dimension audit, adversarially re-verified every finding (57
confirmed, 18 refuted), and fixed the 2 confirmed P0s plus most confirmed
P1s. Commits on `develop` (oldest first): `afb1159` (`ActiveTool::Measure`
OOB), `838eefc` (`ObjectType` mapping), `dae910f` (NaN/Inf rejection),
`fd606d2` (input budgets + instance-cycle + finiteness gate), `737af77`
(undo dead-pattern), `0dfcd4f` (docs consolidation), `40643a4`
(`Mc3LoadPolicy`), `22b7129` (glTF lights), `dcd0d33` (anim mirror Gate B),
`2d126bf` (deterministic shutdown), `e53af49` (backend-truth warning),
`ac75eb6` (concave extrude caps), `901965f` (resource-path confinement),
`d16c82c` (CI file readiness).

The session's own end-of-session summary claimed "every P0 and every
identified P1 addressed" and reported inconsistent test counts (93/93 in one
place, 95/95 in another) — **both claims were wrong on inspection**; see
Session 2.

### Session 2 (2026-07-11, continued) — adversarial re-open + reconciliation

A fresh adversarial pass re-opened the session-1 summary against actual
source and git history:

- Only 4 of 57 detailed `AUD-###` tasks in `plan.md` were marked DONE despite
  16+ P1s already being fixed in git history — `plan.md` was not tracking
  reality. Re-verified all 57 against current source: **28 DONE, 27 TODO, 2
  DEFERRED** among the originals.
- 3 findings reported "fixed" were only partially fixed: resource-path
  confinement covers mc3togltf export only, not the editor/AI application
  layer (`AUD-006b`); the undo fix closed one dead-code pattern but not a
  full transaction-safety audit (`AUD-036b`); Gate C backend truthfulness got
  a warning, not real enforcement (`AUD-039b`).
- 2 new defects found by direct code reading: `s_bloom.cleanup()` exists but
  has zero call sites, so bloom/SSAO/skybox/material-preview GL resources
  leak on every shutdown (`AUD-058`); the tessellation clamp is per-field
  only, not a total-document budget (`AUD-059`).
- Fixed this file's own 93-vs-95 test-count inconsistency and stale
  `HEAD 737af77` reference (both are exactly the class of drift this session
  exists to catch — see `test/validate_plan_consistency.py`, added this
  session, which now gates this from recurring silently a third time).
- Archived `plan_deep_audit.md` (all 57 of its own tasks already completed)
  and fixed `RELEASE.md`'s stale 66/66 test count.

See [`plan.md`](plan.md)'s session log for the fuller version of this entry.
**This narrative will itself go stale — the live source of truth is always
`git log` + `ctest -N` + the `AUD-###`/`SYS-###` task table, not this prose.**

## 3. Next tasks

See the **Priority execution queue** at the top of [`plan.md`](plan.md) — it
is kept free of DONE items by `test/validate_plan_consistency.py`. As of
session 2 the top items are: `AUD-058` (call the dead `BloomGL::cleanup()`),
`AUD-002` (bounds-check `loadObjMesh` tinyobj indices), `AUD-039b` (real
Gate C enforcement), `AUD-006b` (broaden resource confinement past mc3togltf
export), `AUD-059` (total-document allocation budget), `AUD-036b` (undo
transaction abstraction — large, incremental).

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
- **CI activation (owner-gated):** `.github_/workflows/ci.yml` needs a
  `workflow`-scoped push token to move to `.github/`. The workflow file itself
  was made correct/ready in commit `d16c82c` (`AUD-052`/`AUD-056`).

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
  (documented in-header) are intentionally-unwired duplicates — and at least
  two (`insertAnimKeyframesAlg`/`loadPrefsAlg`) have already been caught
  DRIFTING from production despite that intent (`AUD-030` fixed,
  `AUD-032`/`AUD-031`/`AUD-033` still open). Check whether an `Alg` function is
  actually called before assuming a fix there takes effect.
- **Undo/redo:** snapshot-based (whole-document deep copy), not command-diff.
  The dead-pattern bug fixed in `AUD-036`/commit `737af77` is not the full
  story — see `AUD-036b` for the remaining transaction-safety work.
- **`mc3.xsd` is compiled into the binary at configure time** — editing it needs
  a reconfigure, not just a rebuild.
- **XML comment gotcha:** a literal `--` inside an XML comment is rejected by
  `lxml`/`test/validate_xsd.py` though `tinyxml2` tolerates it. Run
  `test/validate_xsd.py` on new fixtures.
- **`AiAssistant` threading invariant:** background HTTP runs on a detached
  `std::thread` writing into a `shared_ptr<AiRequestResult>` (atomic done flag +
  mutex). Never `std::async`/`std::future` (destructor-blocking hang-on-close).
  The detached thread is still never joined at shutdown (`AUD-014`, open).
- **`CNA_ENABLE_NET` must stay `OFF`** — unused CNA subsystem that fails to
  compile; re-enabling breaks the default build.
- **API/compat boundaries:** `Mc3Document`'s public API is depended on by
  `mc3togltf` and every test fixture — additive changes only; check both. CNA /
  SHARP_RUNTIME must not be modified without owner permission.

## 7. Useful commands

```bash
# Configure + build (this session used system cmake on b-release successfully).
# For a separate Debug tree, if your system cmake has a documented reconfigure
# bug for this project, point CLION_CMAKE at your own CLion's bundled cmake
# binary instead of the system one (path is machine-specific — find yours
# under your CLion install's cmake/<platform>/bin/cmake, or just use system
# cmake if it works, as b-release does above).
cmake -S . -B cmake-build-debug -DBUILD_TESTING=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -G Ninja
cd cmake-build-debug && ninja -j"$(nproc)"

# Release tree used this session:
cmake -S . -B b-release && cmake --build b-release -j"$(nproc)"

# Test
(cd b-release && ctest --output-on-failure)        # full suite
(cd b-release && ctest -N)                          # list registered tests + live count
(cd b-release && ctest -R undo_snapshot_lint -V)    # session-1 lint guard

# Plan/doc self-consistency (added session 2 — run before trusting any count in plan.md/NEXT.md)
python3 test/validate_plan_consistency.py . b-release

# XSD validation of a new/changed fixture
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# Static audits
python3 test/xsd_docs_diff.py            # MC3_FORMAT.md vs XSD drift
python3 test/undo_coverage_audit.py .    # undo-coverage candidates (78 untriaged, AUD-036b)

# Run / demo
./b-release/MeshCraft test/house.mc3.xml
./b-release/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
./b-release/mc3tomcb/mc3tomcb test/house.mc3.xml /tmp/out.mcb
```
