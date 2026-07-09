# NEXT.md

_Last updated: 2026-07-09, prior commit `c4e352a` (branch `develop`, in sync with `origin/develop` as of that commit). Native Linux build/tests last verified from-scratch 2026-07-07 (unaffected by this session's findings, which are Emscripten/Windows-only) — see `STABILIZATION_WORKLOG.md` for the full command trace._

---

## 1. Project summary

**MeshCraft** is a C++23 3D scene editor for **`.mc3.xml`**, a custom XML scene-description format used by the OpenEggbert project. The editor UI is built on **Dear ImGui**, running on **CNA** (an XNA-style SDL3+OpenGL runtime — sibling repo at `../cna`, never modified from this repo) and **SHARP_RUNTIME** (`../sharp-runtime`, the .NET-BCL-style math/collections library CNA depends on). Scenes export to **glTF/GLB** via `mc3togltf` and to a compact **binary format (MCB)** via `mc3tomcb`.

**Main goal**: reach a fully stabilized, test-covered codebase before adding new product features. All stabilization work is tracked in `plan.md` as 650 `STAB-XXXX` tasks across sections S0–S20 (`STABILIZATION.md` holds the gate policy).

**Current phase**: stabilization is functionally complete but not formally "all green." `plan.md`: **620/650 rows ✅, 29 🟡 (permanently flagged — real gaps or product decisions, not meant to auto-resolve), 1 📋 (blocked on an external action), 0 🧪, 0 🔴**. Only **Gate 2 (Export)** is 100% green; every other gate has at least one flagged row remaining. Per this project's own policy, **new feature work is not yet authorized** until the project owner decides the current state is "green enough."

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
**66/66 CTest tests pass.** Labels: `ai` 1, `commands` 1, `export` 44, `format` 3, `registry` 1, `render` 16. XSD validation: **69/69** `test/*.mc3.xml` fixtures validate against `mc3/mc3.xsd`. Standalone (CNA-free) subproject builds also verified independently: `mc3` 1/1, `mcb` 1/1, `mc3togltf` 41/41, `mc3tomcb` 3/3.

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
- Web (Emscripten) build: the **2026-07-06 build artifacts** (`cmake-build-web/MeshCraft.{html,js,wasm}`, still present) run in a real headless-Chrome session with a working WebGL2 context (no console/GPU errors). **A fresh rebuild currently fails** — see §4, new blocker.
- Real headless-Blender-based tests confirm exported GLBs (including a real authored scene, `medieval_castle.mc3.xml`) import correctly with matching PBR material values.

### What does NOT work yet
- **Windows GUI build (MinGW)**: does not compile — see §4.
- **Emscripten web build**: initializes (WebGL2 context confirmed working), but the 3D viewport renders a **blank canvas**. **Root cause now identified** (2026-07-09): the live `<canvas>` DOM element ends up `width="0" height="0"` (confirmed via headless-Chrome `--dump-dom` against the 2026-07-06 build artifacts) even though CNA's `GraphicsDevice::createOrAttachWindow()` requests 1024×768 from `SDL_CreateWindow` — this alone fully explains the blank viewport, independent of GL correctness. The size-loss happens somewhere inside SDL3's own Emscripten video backend (third-party, vendored under CNA_dep) or a later resize path, not in this repo's code. **Cannot currently attempt a fix**: a fresh Emscripten rebuild fails before reaching MeshCraft's own sources at all — see §4, new blocker. Detail: `plan.md` STAB-0553's 2026-07-09 update.
- **Web GLB export download**: exporting a GLB in the web build writes to Emscripten's in-browser virtual filesystem only — there is no JS bridge to actually download the file to the user's real filesystem.
- SVG texture rasterization: parsed/serialized but never rasterized (stub only).
- Embedded glTF references (`<mesh src="embed:id"/>`): parsed/serialized but not resolved by the exporter.
- N3–N7 scene data (scripts, sounds, music, triggers, scene states): fully round-tripped in the data model but not executed at runtime (no Lua interpreter, no audio playback, etc. — data-model-first by design, not a bug).
- Android build: never attempted (no Android NDK in this environment).

---

## 3. Recent changes

- **New**: committed a MinGW cross-compile toolchain file, `cmake/toolchains/mingw-w64.cmake` (standard `CMAKE_SYSTEM_NAME Windows` / `x86_64-w64-mingw32-{gcc,g++,windres}` / `CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32` setup) — previously reconstructed ad hoc each session per §4/§8. Verified: `cmake -S . -B /tmp/b-mingw -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake -DMESH_CRAFT_BUILD_TESTING=OFF -G Ninja` configures cleanly (exit 0), reporting the same expected SQLite3/OpenSSL/LibXml2 not-found-and-gracefully-disabled result as prior sessions. Does not change build behavior — same 4 cross-repo blockers in §4 remain (not attempted, out of scope).
- **New (investigation only, no code changed)**: root-caused the Emscripten blank-canvas bug down to `<canvas width="0" height="0">` (see §2/§4/§5, `plan.md` STAB-0553). While investigating, discovered a **separate, newly-introduced regression**: `../sharp-runtime` now fails to build under Emscripten (16 `-Werror` failures + a hard `std::chrono::clock_cast` error), which didn't exist as of the last verified web build (2026-07-06) — sharp-runtime's HEAD has since moved to `e5e38db` (2026-07-07). This blocks a fresh web rebuild entirely, so the canvas-sizing root cause could not be turned into a verified fix this session. No files in this repo were changed by this investigation.

Most recent commits (`develop`, newest first):
- `dc46af4`, `da8f1b8`, `9e5f252`, `4b7fcd2` — conservative-maintainer audit: re-verified the whole project from a genuinely empty build directory, fixed docs that had drifted from reality, re-verified MinGW/Emscripten cross-compile status.
- `9d673a0` — **fix**: `CMakeLists.txt` now sets `CNA_ENABLE_NET OFF` (was implicitly `ON` via CNA's own default) — this was the actual clean-build fix, see §4/§6.
- `865d015` — **fix**: MCB reader had no recursion-depth limit (a crafted file could stack-overflow-crash the process) and no sanity check on claimed string lengths (a tiny crafted file could force multi-GB memory allocation). Both fixed with a bounded recursion guard and a size ceiling; regression tests added.
- `f0dd333`/`f663585` and earlier — closed out most of the S10–S20 stabilization sections (AI integration hang-on-close fix, GL-error-check dead code fix, materials/visual-fidelity verification, cross-platform re-verification with real Blender/Chrome evidence).

Tests added this session: 2 MCB regression tests (recursion-depth, string-length), 1 ModelRegistry temp-file-leak regression test. No tests were removed. Behavior changes: `AiAssistant`'s async mechanism switched from `std::async`/`std::future` to a detached `std::thread` + `shared_ptr` result box (non-blocking shutdown); `CNA_ENABLE_NET` now defaults OFF for this project specifically.

Full history: `git log --oneline`. Per-task detail: `plan.md` (one row per `STAB-XXXX` ID) and `STABILIZATION_WORKLOG.md` (narrative + exact commands run).

---

## 4. Current blocker / main problem

**Nothing blocks Linux development, building, or testing** — the build is clean and 66/66 tests pass. If forced to name the single most significant *known, unresolved* problem in the project right now, it is:

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
- **Confirmed, by design (not a bug)**: SVG texture rasterization stub-only; embedded-glTF references not resolved; N3–N7 scene data not executed at runtime; two independent material-editing UIs exist (`Scene/PropertiesPanel.cpp` and `MeshCraftApplication_UiLeftPanel.cpp`) — a fix in one doesn't apply to the other.
- **Needs verification (blocked on tooling, not known-bad)**: ~15+ `plan.md` rows need a live interactive display/mouse session this headless environment can't provide (curve-editor visibility, proportional-edit radius indicator, FPS counter, live drag-and-drop gesture, etc.) — genuinely untested either way, not confirmed broken.
- **Incomplete**: ~100+ non-material ImGui slider/drag call sites still lack `ImGuiSliderFlags_AlwaysClamp` (6 highest-impact material-PBR ones were fixed and confirmed to matter; the rest need a per-site downstream-safety review before a blanket fix).

---

## 6. Architecture notes

- **CSG dual-path invariant**: `mc3togltf/src/CsgEvaluator.cpp` (export) and `SceneRenderer`'s CSG preview cache (editor) both key off `isCutter`/`role="cutter"` on child objects. A missing cutter flag silently turns a subtraction into a union — this has bitten real test fixtures before. Both paths must be kept in sync if CSG semantics change.
- **Primitive dual-path invariant** (same risk class as CSG, found in `plan_deep_audit.md` AUDIT-0011): `mc3togltf/src/MeshBuilder.cpp::buildPrimitive()` (exact per-type triangulation, used for export and CSG evaluation) and `SceneRenderer`'s primitive dispatch (`SceneRenderer.cpp:635-720`, unit-mesh + scale + LOD) are two independent geometry-generation implementations of all 11 `PrimitiveType` values (Box/Cube/Sphere/Cylinder/Cone/Plane/Torus/Capsule/Disk/Grid/IcoSphere). Both currently agree on which types exist and their basic shape, but there is no automated cross-check that they stay visually/dimensionally consistent as either is changed independently — see `plan_deep_audit.md` AUDIT-0012 for a proposed bounding-box invariant test.
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
ctest --output-on-failure          # full suite (66)
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

4. **Pick one of the 6 flagged product-decision rows (`STAB-0092`/`0289`/`0327`/`0360`/`0460`/`0571`) and get an explicit scope decision from the project owner**, then implement only that one. (Note: `STAB-0427`, previously listed here, is actually already ✅ — resolved as "confirmed deliberately unimplemented, feature moratorium"; `STAB-0092`, "define include-tracking policy for `embeds` map," is the correct 6th row.)
   Files: varies per row — see `plan.md` for the specific row's "Key File(s)" column.
   Verify: whatever test the chosen row's own `plan.md` entry specifies.

---

## 9. Do not do yet

- **No new product features** until the project owner explicitly says the current stabilization state (620/650, Gate 2 fully green, rest flagged) is sufficient to resume feature work.
- **No CNA or SHARP_RUNTIME source changes** without explicit owner permission — this includes the GLES3 header gap and the 3 sharp-runtime `-Werror` issues in §4, even though the fixes are individually easy to guess at.
- **No `Mc3Document` public API changes** without checking `mc3togltf`, `mc3tomcb`, and every test fixture that touches it.
- **No mass refactor or blanket fix** for the ~100+ un-clamped ImGui slider sites (§5) — each needs its own downstream-safety check first.
- **No speculative implementation** of any of the 6 flagged product-decision rows (§8 item 5) without first getting the scope decision — guessing at scope risks building the wrong thing.
- **No SVG rasterization or `embed:`/`<embeds>` resolution work** without an explicit decision on which library/approach to use.
- **No re-running the full "delete cmake-build-debug and rebuild from scratch" verification** unless a meaningful amount of new work has landed since the last one (2026-07-07) — it would just re-confirm the same 66/66 with no new information.

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

Current branch: develop, in sync with origin/develop at commit dc46af4.
No new feature work without explicit owner authorization (see section 9).
```
