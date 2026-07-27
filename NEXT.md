# MeshCraft — Current Handoff

_Last updated: 2026-07-27._ Read this before resuming work. The authoritative
active queue is [`plan.md`](plan.md); historical detail is in
[`docs/history/`](docs/history/).

## Product snapshot

MeshCraft is a pre-production C++23 3D scene editor for the MC3 format. Its
canonical `Mc3Document` model is shared by XML/JSON parsing and writing, MCB,
the MC3→MCB and MC3→glTF/GLB tools, and the editor. The editor is built on CNA;
the sibling `cna` and `sharp-runtime` repositories are consumed read-only and
must not be modified here.

The feature set already includes:

- MC3 XML/JSON, MCB, glTF/GLB export, CLI conversion tools, validation, and
  bounded parser/resource limits;
- CSG, dynamic/generated geometry, authored LOD, materials, UV mapping,
  textures, lights, cameras, animation clips, and scene history;
- editable OBJ and self-contained GLB/glTF import, local model registries,
  asset packs, Lua scripts, triggers, states, and explicit event bindings;
- undo/redo, autosave recovery, and automated unit, differential, smoke,
  screenshot, and format round-trip coverage.

The original stabilization work and `SYS-W14-28` through `SYS-W14-39` are
complete. MeshCraft is now roughly 75–80% of the way to a solid 1.0: the work
remaining is concentrated in correctness, performance, documentation, CI, and
distribution rather than missing editor breadth.

## Current priorities

1. Obtain native standalone Windows qualification (`SYS-W11-06`). The
   release-readiness changes are published on `develop`; await the next
   remote `windows-2022` CTest/artifact evidence run — the first real run
   found 6 concrete regressions (see "Session log" below), all 6 now fixed
   and pushed, but none re-verified on an actual Windows runner yet (no
   Wine in this sandbox).
2. The freshly-added `[PROPOSED]` backlog (`SYS-W9-06/07`, `SYS-W1-08`,
   `SYS-W2-06`, `SYS-W11-08/09/10`, `SYS-W13-03`, `SYS-W3-05`) is
   **all 9 DONE**. `SYS-W11-09` found a real heap-use-after-free bug in
   `EventPreviewRunner::execute()`'s RunScript step (`ff62004`) — a stale
   `working.scripts` iterator read again after `LuaScriptRunner::run()`
   already replaced `working` via move-assignment. Fixed. See `plan.md` for
   full evidence on everything done.
3. All 6 of the deferred CI-red regressions from that first Windows run are
   now fixed and pushed (see "Session log" below for each one's root cause
   and evidence) — still awaiting real Windows re-verification for the 3
   Windows-specific fixes (`mc3_roundtrip`, `mcb_load_policy`, `mc3togltf`
   DLL staging).
4. `SYS-W11-11` (the first-release version-number decision) is deliberately
   left `[BLOCKED]` per the user's explicit choice not to decide yet.

`SYS-W14-40` is complete: the explicit bounded Event Preview/Play mode is
covered by `event_preview_runner`. See `plan.md` for the remaining Windows
qualification evidence and the full release-gate order.

## Session log (2026-07-26, autonomous continuation)

User explicitly authorized an unattended multi-hour session over the
freshly-merged `[PROPOSED]` backlog, with one carved-out exception to
"leave general CI-red alone for now": fixing the Editor (EASYGL/VULKAN) CI
job's missing `cna` submodule checkout (mesh-craft's own workflow file, not
CNA source — `submodules: true` added to `.github/workflows/ci.yml`'s
Checkout CNA step).

**The first real `develop` CI run (before tonight's push) surfaced 6 concrete
regressions, deliberately left alone per the user's instruction, tracked here
for the next pass that picks them up:**
1. Windows: `mc3togltf`'s test suite almost entirely fails with
   `STATUS_DLL_NOT_FOUND (0xC0000135)` — a required runtime DLL isn't next to
   the exe on the CI runner itself, not just in a downloaded artifact.
2. Windows: `mc3_roundtrip` — `std::filesystem::remove()` on a temp file
   throws "cannot remove: ... being used by another process".
3. Windows: `mcb_load_policy` — one assertion's expected rejection-message
   text differs because of POSIX- vs Windows-style absolute-path handling.
4. Windows: `mc3togltf_large_obj_stress` — the Python test harness does
   `import resource` unconditionally; that module doesn't exist on Windows.
5. Linux ASan+UBSan: `mc3_roundtrip`'s cyclic-children-graph test leaks by
   construction (LeakSanitizer correctly flags the test fixture itself).
6. Linux ASan+UBSan: `mc3_json_document_budget` times out at the 30s CTest
   ceiling — confirmed (via `git stash` isolating the change) this is
   pre-existing on unmodified code, caused by `-DCMAKE_BUILD_TYPE=Debug`
   (~50-55s) vs. a `Release`-flagged build (~8-10s), not anything from
   tonight's `SYS-W1-08` work.

Completed and pushed, one commit each, in this order:
- **CI fix**: `cna` submodule checkout in the Editor matrix job (`084af6f`).
- **`SYS-W9-06`** (`edcc677`): one portable atomic-file-replace primitive
  (`Mc3::writeFileAtomically`/`uniqueSiblingTempPath`,
  `mc3/include/MeshCraft/Mc3/Mc3AtomicFileWriter.hpp`) replacing the 4
  duplicated fixed-`.tmp`-name write-then-rename call sites in
  `Mc3XmlWriter`/`Mc3JsonWriter`/`McbWriter`/`GltfExporter`'s GLB path.
- **`SYS-W2-06`** (`a7ecc90`): fixed `budgetedLuaAllocator()`'s new-allocation
  accounting bug (a new object's `oldSize` is Lua's type tag, not a real
  prior size — confirmed against the vendored Lua 5.4 source). Moved to
  header-only `include/MeshCraft/Editor/LuaMemoryBudget.hpp`.
- **`SYS-W1-08`** (`a0b4530`): `Mc3JsonParser` gained the same
  `Mc3Validation`-capturing diagnostics `Mc3XmlParser` already had; wired
  `loadSceneFileDispatched()` and both `.mc3lib` loaders through it.

Each commit built and tested clean (standalone `mc3`/`mcb`/`mc3togltf`, the
full root `MeshCraft` editor target, and the affected root-project test
subset) before committing. The 3 pre-existing `mc3togltf_blender_import`
family failures (missing `numpy` in this sandbox's Blender) are unrelated
environment gaps, not regressions, and were left alone.

Continuing (each its own commit, in this order after the above): **`SYS-W9-07`**
(`bdd3d03`, untitled-scene crash recovery), **`SYS-W13-03`** (`2c2ce4f`, 4
verified doc-drift fixes), **`SYS-W11-08`** (`fbaf683`, clean-room CLI
release artifacts). `SYS-W11-08` found a real functional gap along the
way, not just a testing gap: **neither `mc3tomcb` nor `mc3togltf` supported
`--version` at all** (usage text / "input file not found" respectively) —
fixed for both. Also fixed a self-referential bug in the new SHA-256
manifest generator (a stale manifest from a previous run got its own old
hash included in the new one). Windows got the same manifest/notices staged
additively in CI, without touching the already-deferred DLL-not-found gap.

Remaining: `SYS-W11-09`, `SYS-W11-10`, `SYS-W3-05` in that order (see
`plan.md`'s recommended ordering note on `SYS-W11-06`). If this file wasn't
updated further after this paragraph and the session ended, check `git log`
on `develop` and `plan.md`'s own `[DONE]`/`[IN_PROGRESS]` markers for the
true current state before assuming
anything below this point is stale.

`SYS-W11-09` and `SYS-W11-10` landed (see `plan.md`, both `[DONE]`). Then
**`SYS-W3-05`** (real refactor, not cosmetic): split the 2,514-line
`EditorAlgorithms.hpp` into 7 cohesive, CNA-free headers
(`EditorCommandAlgorithms.hpp`/`EditorSelectionAlgorithms.hpp`/
`EditorTransformAlgorithms.hpp`/`EditorPersistenceAlgorithms.hpp`/
`EditorEventAlgorithms.hpp`/`EditorPreferencesAlgorithms.hpp`/
`EditorUtilityAlgorithms.hpp`), moved `findParentListAlg`/`removeFromListAlg`
into Commands (not Selection as first proposed) once grep proved every call
site is a Commands/Transform mutator, and migrated all 30 flagged consumer
files individually — 14 now need exactly 1 new header, 8 need 2, 1 needs 3,
1 (`Commands.cpp`) needs 4; one (`Macro.cpp`) had a dead include removed
outright; 5 were comment-only false positives needing no change. No facade
left behind; the original header is deleted. `MeshCraft` plus all 13
affected test targets built clean with zero missing-include errors;
`ctest -LE render` is 181 tests / 163 passed, with only the two
already-known pre-existing exceptions (`mc3_json_document_budget` timeout,
3 Blender-`numpy` import tests) plus 14 unrelated `mcb_*`/`mc3togltf_*`
tests that were simply never built in this reused `cmake-build-debug`
(spot-built 3 to confirm — pre-existing gap, not a regression). See
`plan.md`'s `SYS-W3-05` entry for full line-count/fan-out evidence.

All 9 `[PROPOSED]` backlog tasks are now `[DONE]`. User asked what's next;
chose to work through the 6 deferred CI-red regressions from this session's
earlier survey (see the numbered list above), leaving `SYS-W11-11`'s
version-number decision deliberately undecided and keeping `build-asan/` in
place. Progress, each its own commit:
- **#4 + #6** (`fc0fbd4`): `mc3togltf_large_obj_stress`'s Python harness now
  guards `import resource` behind `os.name != "nt"`; `mc3_json_document_budget`'s
  CTest `TIMEOUT` raised 30s→200s (measured ~50-55s Debug / ~96-100s
  ASan+UBSan, pre-existing, not a regression from tonight's work).
- **#5** was already fixed as a side effect of `SYS-W11-09`'s cyclic-leak
  cleanup (same file, same `.clear()` pattern).
- **#2** (`mc3_roundtrip`'s Windows "used by another process" on `remove()`):
  root cause is NOT a transient lock — 6 of the file's helper/test blocks
  opened an `std::ifstream` on the temp file to read its contents back, then
  called `std::filesystem::remove()` on the same path while that `ifstream`
  was still in scope (its destructor runs later, at the end of the
  enclosing block). POSIX allows unlinking an open file; Windows'
  `DeleteFile` does not without `FILE_SHARE_DELETE`, so this fails
  deterministically there, not flakily. Fixed by adding an explicit
  `.close()` immediately before each such `remove()`. Also added a shared
  `removeTestFile()` helper (bounded retry via the non-throwing
  `std::error_code` overload, matching `Mc3::writeFileAtomically`'s
  finalize-step precedent from `SYS-W9-06`) and swapped every bare
  `std::filesystem::remove(...)` call in the file to use it, as a backstop
  against any remaining external transient lock (AV/indexing) this
  diagnosis doesn't explain. Verified: `mc3_roundtrip` and the full
  non-render `mc3_*` suite (29 tests) pass clean under ASan+UBSan
  (`build-asan/`), no new leak/UB findings from the added `.close()` calls.
  Unverified on real Windows (no Wine in this sandbox).
- **#3** (`mcb_load_policy`'s POSIX- vs Windows-style absolute-path message
  text): root cause is `std::filesystem::path::is_absolute()` itself, not
  the test. On Windows it requires BOTH a root-name (drive letter/UNC) AND
  a root-directory; a POSIX-style rooted path like `/etc/passwd` has a
  root-directory but no root-name, so `is_absolute()` is false there even
  though it's exactly the filesystem-root-escape attempt the check exists
  to catch. The untrusted policy still rejected it on Windows (the
  `includePathWithinRoot()` fallback catches it as "escapes the document
  root" instead), just with a different message than the `mcb_load_policy`
  test's stricter substring assertion expected — `mc3_load_policy`/
  `mc3_json_load_policy`'s sibling tests only assert `threw`, not the
  message text, so they stayed green despite having the identical
  semantic gap. Fixed all 3 mirrored `is_absolute()` checks
  (`mcb/src/McbReader.cpp`, `mc3/src/Mc3XmlParser.cpp`,
  `mc3/src/Mc3JsonParser.cpp` — same names, same logic by design) to also
  treat "has a root-directory but no root-name" as absolute, so the
  rejection message is the same on every platform. `has_root_name()` is
  always false on POSIX, so `has_root_directory() && !has_root_name()` is
  already implied by the existing `is_absolute()` there — zero behavior
  change on Linux/macOS, confirmed by the full `mc3_*`/`mcb_*`/
  `mc3togltf_*` non-render suite (115/118 pass, the 3 failures are the
  already-known Blender/`numpy` gap, unrelated) still passing unchanged.
  Windows-side reasoning follows documented `std::filesystem` semantics
  (cppreference), not executable in this sandbox (no Wine).
- **#1** (`mc3togltf.exe` `STATUS_DLL_NOT_FOUND`): root cause is that
  Manifold's own `CMakeLists.txt` defaults `BUILD_SHARED_LIBS` to `ON`, and
  `tinyobjloader`'s `add_library()` call has no explicit `STATIC`/`SHARED`,
  so it inherits that same global cache value — both build as DLLs on
  Windows. Linux/macOS never hit this: CMake automatically adds build-tree
  RPATH entries to every executable pointing at its shared-library
  dependencies' own build output, and Windows PE executables have no
  equivalent (the loader only searches the exe's own directory, CWD,
  `System32`, and `PATH` — never `_deps/manifold-build/...`). The root
  `CMakeLists.txt` already knew about this for the *installed* tree
  (`install(TARGETS manifold tinyobjloader ...)`, staging both next to the
  installed binaries for the CLI release archive/`clean_room_cli_release`),
  which is why that path already worked — but the `standalone-windows` CI
  job runs `ctest` directly against the raw, un-installed build tree, which
  had no equivalent staging.
  Added `mc3togltf_stage_runtime_dlls(target)` to `mc3togltf/CMakeLists.txt`
  — a `WIN32`-guarded `POST_BUILD` copy of `$<TARGET_RUNTIME_DLLS:target>`
  (CMake 3.21+ generator expression, the project's existing minimum) next to
  each target — and called it after all 9 affected executables (`mc3togltf`
  itself, `mc3togltf_glb_libfuzzer`, and the 7 C++ test binaries that link
  `mc3togltf_lib`). A no-op on Linux/macOS (`$<TARGET_RUNTIME_DLLS:...>`
  resolves to nothing there), confirmed by rebuilding both the standalone
  `mc3togltf/build` (75/78 pass, 3 already-known Blender/`numpy` failures)
  and the full root `MeshCraft` editor + `cmake-build-debug`'s
  `mc3togltf_*`/`package_consumer_smoke`/`clean_room_cli_release_smoke`
  (all pass, packaging path unaffected) with zero build or test changes.
  Unverified on real Windows in this sandbox (no Wine) — the DLL-copy
  mechanism itself is standard CMake, not something this project invented.
  **All 6 of tonight's deferred CI-red regressions are now addressed.**

**Real CI evidence landed after pushing the above.** Confirmed working on an
actual `windows-2022` runner: `mc3togltf.exe` now starts (no more
`STATUS_DLL_NOT_FOUND`) and `mc3_load_policy`/`mc3_json_load_policy`/
`mcb_load_policy` all pass. But clearing those earlier blockers let both the
Windows and Linux sanitizer jobs run further than ever before, surfacing 2
new, previously-unreachable bugs (same "fixing one bug unmasks the next"
pattern as this whole session):

- **Linux Clang ASan+UBSan: ~67 of 75 `mc3togltf_*` tests crashed at process
  startup** with an AddressSanitizer odr-violation on
  `typeinfo name for tinyobj::MaterialFileReader` between
  `mc3togltf/src/MeshBuilder.cpp` and `libtinyobjloader.so.2`. Root cause:
  `MeshBuilder.cpp` defines `TINYOBJLOADER_IMPLEMENTATION` and `#include`s
  the header directly (compiling its own copy into `mc3togltf_lib`), while
  `mc3togltf_lib` ALSO linked the separately-compiled `tinyobjloader` CMake
  target — which builds as a shared library for the same
  Manifold-`BUILD_SHARED_LIBS`-default reason as the Windows DLL bug above.
  Two live definitions of the same class/vtable in one process is genuine
  undefined behavior, not just an ASan nitpick. This was always latent but
  never reached in CI before tonight: the standalone-component loop always
  died earlier at `mc3`'s `mc3_json_document_budget` timeout (CI-red #6,
  now fixed), before ever building/running `mc3togltf`'s own tests.
  Fixed by dropping the `tinyobjloader` *link* dependency everywhere
  (`mc3togltf/CMakeLists.txt`'s `mc3togltf_lib`, and both branches of the
  root `CMakeLists.txt`'s main editor target) — only the include directory
  is needed since `MeshBuilder.cpp` already provides the one-and-only
  implementation. Also dropped `tinyobjloader` from the root project's
  release-artifact DLL/shared-library install loop, since nothing links it
  at runtime anymore. Verified: rebuilt the standalone `mc3togltf` component
  under Clang ASan+UBSan matching CI's exact flags (`build-sanitize/mc3togltf`)
  — 75/78 pass (up from 8/78), the 3 failures are the already-known
  Blender/`numpy` gap; the full root `MeshCraft` editor (`cmake-build-debug`)
  still links and runs (`--version` works), and OBJ import specifically
  (`mc3togltf_obj_material_import`/`_obj_robustness`/`_large_obj_stress`)
  still passes, confirming tinyobjloader itself still works correctly
  through its single remaining (header-only) code path.
- **Windows: `mc3_roundtrip` still crashed, for a different, new reason**
  than tonight's earlier fix — root-caused and fixed with real evidence, not
  guesswork. Discovered that **Wine actually runs trivial MinGW-cross-compiled
  console programs fine in this sandbox** (only the full GUI editor hits the
  previously-documented SIGSYS block) — found an existing, unused
  `cmake-build-verification-windows-standalone/` + Wine-prefix setup from an
  earlier session and reused it (`CMAKE_SYSTEM_NAME=Windows`,
  `CMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++`,
  `CMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/wine`) to get a real, faithful
  local repro of the exact CI failure. First repro attempt (a minimal
  `writeFileAtomically()`-only program) surprisingly PASSED — meaning the bug
  isn't in the atomic-write primitive itself. Running the *actual*
  `mc3_roundtrip_test.exe` under Wine reproduced it with a clearer message:
  "No such file or directory" (not "Input/output error" — Wine's fopen()
  behavior differs slightly from real Windows here, same underlying bug).
  Root cause: `Mc3XmlWriter.cpp`/`Mc3XmlParser.cpp` called tinyxml2's
  `SaveFile(const char*)`/`LoadFile(const char*)` with `path.string()` — a
  UTF-8-encoded narrow string. tinyxml2 opens that with plain `fopen()`,
  which on Windows converts narrow strings via the process's ANSI code page,
  **not UTF-8** — for a Czech+Japanese filename (STAB-0555's test case) this
  mismatch means `fopen()` opens/creates a different, mangled path than the
  one `std::filesystem::rename()` expects afterward. `Mc3JsonWriter`/
  `Mc3JsonParser` never had this bug (they already used
  `std::ifstream`/`std::ofstream` directly with the `path` object, which
  libstdc++ opens via the native wide string on Windows, correctly).
  Fixed by adding `saveXmlFileUnicodeSafe()`/`loadXmlFileUnicodeSafe()`
  helpers (one in each file, `#ifdef _WIN32`) that open the file themselves
  via `_wfopen(path.c_str(), ...)` (the path's native wide representation,
  no narrow conversion at all) and hand tinyxml2 the `FILE*` overload
  instead; POSIX keeps the original narrow-string call unchanged. Also
  widened `Mc3::writeFileAtomically()`'s finalize retry budget from 5×20ms
  to 20×50ms as defense-in-depth (real Windows CI's original "Input/output
  error" symptom, before this deeper root cause was found, is consistent
  with a slower antivirus-scan lock on top of the encoding bug).
  Verified: `mc3_roundtrip`/`mc3_atomic_write`/`mc3_load_policy`/
  `mc3_json_load_policy` all pass under the real MinGW+Wine repro (including
  the exact `utf8 filename`/`path with spaces`/`non-ascii name` cases), and
  the full 28-test standalone `mc3` suite passes there too. Also re-verified
  clean under Linux Clang ASan+UBSan (29/29 `mc3_*` tests, zero regressions).
  This is the strongest verification any Windows-only fix has had this
  session — a real cross-compiled binary actually executing the real code
  path, not just source-level reasoning.

**Confirmed on the next real `windows-2022` CI run after pushing:**
`mc3_roundtrip` now passes 100% (28/28, up from 27/28) — the Unicode-fopen
fix works on real Windows, not just the Wine repro. `mcb` standalone is also
100% (11/11). Also confirmed on the same push's Linux job: `mc3togltf`
standalone now passes 100% (75/75) under Clang ASan+UBSan — the
tinyobjloader ODR-violation fix is fully verified on real CI. Remaining
known (not new) failures, not yet fixed:
- `mc3togltf` standalone: 5/75 fail — `mc3togltf_no_partial_output`,
  4x Python `UnicodeEncodeError` printing `≥`/`≠` to a Windows cp1252
  console (`mc3togltf_instance_deform_cache`,
  `mc3togltf_large_scene_generated/_500/_1000`).
- `mc3tomcb` standalone: 1/13 fails — `mc3tomcb_error_handling`, same
  write-protection-assumption class as `mc3togltf_no_partial_output`
  (Windows doesn't enforce a chmod-based write-protection simulation the
  way the test expects).
- Newly reached (Linux Clang ASan+UBSan job, later "Build bounded libFuzzer
  targets"/fuzz-smoke step, only reachable now that the ODR violation no
  longer aborts the job earlier): `mcb_libfuzzer` hit libFuzzer's OOM guard
  (550MB against a 512MB `rss_limit_mb`) during the 20s corpus-seeded fuzz
  run — not yet triaged as a real unbounded-allocation bug vs. just a
  too-tight memory ceiling for legitimate corpus growth.
- Editor (EASYGL/VULKAN, both plain and ASan+UBSan) jobs still fail at
  CMake configure: "SDL could not find X11 or Wayland development
  libraries" — confirmed pre-existing (reproduces on a CI run from before
  any of tonight's work), a CI-runner apt-package gap unrelated to anything
  fixed this session.

User asked to keep going on the remaining 4 items. Fixed 2 more:
- **Python `UnicodeEncodeError`**: 2 files (`instance_deform_cache_test.py`,
  `large_scene_generated_test.py` — the latter backs all 3 `_generated`/
  `_500`/`_1000` CTest names) printed `≠`/`≥` in PASS messages; Windows'
  default console codepage (cp1252) can't encode those. Replaced with
  `!=`/`>=`. Verified unchanged behavior on Linux (still 100% pass);
  correctness of the fix itself doesn't depend on Windows-specific
  verification since removing non-ASCII output is unconditionally safe on
  any codepage.
- **Windows write-protection test assumption**: `mc3togltf_no_partial_output`
  (STAB-0546b) and `mc3tomcb_error_handling` (STAB-0538) simulated an
  unwritable output directory via `os.chmod(dir, ...)` with POSIX mode
  bits — on Windows, `os.chmod()` can only toggle `FILE_ATTRIBUTE_READONLY`,
  which Windows ignores for directories, so it never actually blocked
  writes there. Replaced with a directory-permission-independent
  mechanism: point the output path at a parent directory that's simply
  never created. This fails identically on every platform (the exporter's
  file-open-for-write call gets "no such file or directory") and tests the
  exact same downstream code path ("does a write failure leave partial
  output or a non-zero exit"), without relying on OS permission-model
  differences at all. Verified via the same MinGW+Wine repro technique as
  the `mc3_roundtrip` fix above — built `mc3togltf.exe`/`mc3tomcb.exe` for
  Windows and ran both Python test scripts against them through Wine (via
  a small wrapper script, since this sandbox has no binfmt_misc handler
  for `.exe`): both STAB-0546a/b and STAB-0537/0538 cases pass. Also
  reconfirmed the full `mc3togltf_*`/`mc3tomcb_*` suite on Linux (77/80,
  same 3 already-known Blender/`numpy` failures, zero regressions).

## Known release blockers and decisions

| Area | Live state |
| --- | --- |
| Alternate graphics backends | `SYS-W8-05` is blocked on CNA owner coordination and actual backend evidence. |
| Android | `AUD-042` is blocked on NDK/tooling, sibling-runtime cross-build repair, packaging, and device/emulator testing. |
| Undo retention | `AUD-038` deliberately limits undo to 20 deep-copy entries; `SYS-W9-05` targets duplicate ownership, not unlimited history. |
| Unknown MC3 XML | Current policy is lossy handling of unknown elements/attributes; do not add a preservation bag without an approved compatibility contract. |
| New features | Do not add formats, importers, skinning, Draco/meshopt, or broad modelling tools before the active quality queue. |

## Immediate technical risks

- Existing CI now has Clang ASan+UBSan and bounded XML/JSON/MCB/GLB fuzz
  smoke coverage for standalone code. `Mc3`/`Mcb` CMake packages and a
  relocatable Linux CLI archive are locally qualified; Windows standalone
  qualification has a dedicated `windows-2022` job and successful local
  MinGW 14 builds, but awaits its first runner CTest/artifact evidence.

## Documentation truth

`SYS-W13-02` is complete. [`docs/CAPABILITY_MATRIX.md`](docs/CAPABILITY_MATRIX.md)
is the authoritative short matrix for format, editor, exporter, and platform
scope. `capability_documentation` statically checks the source-backed Walk
Mode proxy, Web IDBFS, autosave-recovery, and ordinary-UV claims. It excludes
volatile test totals and unverified platform-runtime assertions.

`SYS-W2-01` is complete. Lua executes against an isolated document copy with a
16 MiB VM allocator cap and the existing 50-million-instruction ceiling. Only
a fully validated result commits; the editor creates undo/history at that
commit boundary, so a failed script cannot leave a partial edit or false dirty
state.

`SYS-W12-03` is complete. Ordinary tinting uses existing permanent geometry
and effect state, authored UV buffers are persistent/bounded by mesh and
mapping inputs, and the 100-instance benchmark fixture proves a warm draw has
zero ordinary tint/authored-UV creations while reusing the UV cache.

`SYS-W12-04` is complete. The benchmark now splits CPU primitive mesh
generation, CSG cold evaluation/warm lookup, and cache-miss texture
decode/upload from frame time, with allocation/cache counters and no fragile
wall-clock gate. The UV fixture and a CSG fixture both exercised those lines.

`SYS-W3-03` is complete. `RegistryWorkspace` owns the registry database and
all Registry UI workflow state; `MeshCraftApplication` now has a single
workspace member and provides only explicit scene-insert/status callbacks.
The controller lifecycle is CNA-free tested and existing registry-data tests
continue to cover SQLite and no-SQLite behavior.

`SYS-W3-04` is complete. `AutomationWorkspace` owns the shared non-serialized
Scripts/Triggers/States/Events selection, Lua, and Preview/Play state, while
all four renderers use narrow callbacks rather than reaching into
`MeshCraftApplication`. Trigger execution snapshots its steps before a Lua
transaction can swap the document, so later steps remain valid.

`SYS-W9-05` is complete. Matching undo and automatic-history entries share a
single immutable document snapshot, while each restore still produces an
independent writable document. The 1,000-object test proves one retained graph
instead of two and covers selection/undo/history equivalence.

`SYS-W11-04` is complete. CI runs all four CNA-free standalone components
under Clang ASan+UBSan, then runs fixed-seed, bounded libFuzzer smoke tests for
XML, JSON, MCB and self-contained GLB import. The source corpora stay
immutable; generated MCB/GLB seeds and fuzzer working copies live in CI build
directories. The local Clang 19 smoke verification passed for all four input
classes; the existing GLB import CTest also passed.

`SYS-W11-05` is complete. The `release` install component exports
`MeshCraft::Mc3` and `MeshCraft::Mcb`, checks them through an external
`find_package` consumer CTest, and `meshcraft_cli_release` produces a
relocatable CLI `.tar.gz` from the same install rules. The archive includes
the two CLI tools, MC3/MCB headers and libraries, package configs, and the
non-system runtime libraries used by the glTF exporter.

`SYS-W11-06` is in progress. Its Windows CI job remains CNA-independent,
builds and CTests all four standalone components, verifies a fixed
MC3→MCB/GLB fixture before artifact upload, and publishes only the qualified
Windows CLI executables. The release-readiness changes are pushed to
`origin/develop`. This session reconfigured the local MinGW 14 tree with
`BUILD_TESTING=ON`,
confirmed its CTest registration uses the configured cross-test emulator, and
compiled the affected standalone targets. The sandbox blocks Wine itself with
`SIGSYS`, so it cannot provide runtime evidence. The native Linux
`cli_cross_platform_fixture`, package-consumer, capability-documentation, and
plan-consistency CTests passed. Freshly relinked XML, JSON, and MCB
load-policy tests plus the embedded-GLB and SVG safety export tests also
passed. The four path-confinement comparisons use `generic_string()` rather
than Windows-wide `path::native()` strings. Await the first GitHub
`windows-2022` CTest/artifact run before marking this task done.

## Practical commands

```bash
python3 test/validate_plan_consistency.py .
cmake -S . -B b-release -DBUILD_TESTING=ON
cmake --build b-release -j4 --target MeshCraft
ctest --test-dir b-release -LE render --output-on-failure -j4
```

Rendering tests depend on a usable display preflight; their registration or
availability is environment-dependent. Do not write a volatile “all N/N tests
pass” count into handoff documentation unless it was just verified in the same
build tree.

Standalone format/tooling builds are useful when CNA siblings are unavailable:

```bash
cmake -S mc3 -B mc3/build -DBUILD_TESTING=ON
cmake --build mc3/build -j4
ctest --test-dir mc3/build --output-on-failure -j4
cmake -S mcb -B mcb/build -DBUILD_TESTING=ON
cmake --build mcb/build -j4
ctest --test-dir mcb/build --output-on-failure -j4
```

Package and CLI-release verification from the established root build is:

```bash
ctest --test-dir cmake-build-debug -R '^package_consumer_smoke$' --output-on-failure
cmake --build cmake-build-debug --target meshcraft_cli_release -j2
```

## Guardrails

- Preserve `Mc3Document` as the CNA-free canonical AST. Check every consumer
  before changing its public API.
- Keep independent editor/exporter geometry paths independent except for
  deliberate, documented sharing.
- Use four jobs or fewer for local builds/tests.
- Do not modify sibling repositories or push changes without explicit user
  authorization.
- Keep `plan.md` bounded: archive completed narratives instead of letting the
  active queue become a worklog.

## Verification caveat

This session configured the existing root Debug tree and freshly built the
focused standalone/path-confinement targets with `CCACHE_DISABLE=1`; its
configured sibling CNA and sharp-runtime revisions differ from the historical
verified SHAs and must not be treated as a full-editor qualification. The
normal compiler-cache location is read-only in this environment, so a fresh
full suite wasn't completed here. That is not evidence of a source failure.
**Correction (2026-07-27): Wine is NOT blocked in this sandbox for
MinGW-cross-compiled console/test binaries** — only the full GUI editor hits
the previously-documented `SIGSYS` block. See
`cmake-build-verification-windows-standalone/` (an existing, reusable
MinGW+Wine build/verification setup) and the "MinGW+Wine repro" writeups
below for the technique; use it before assuming any Windows-only bug is
unverifiable here.
The next session should start by reviewing the first `standalone-windows`
GitHub Actions run for the current `develop` head; use its CTest/artifact
result to complete or diagnose `SYS-W11-06`.

## Session log (2026-07-27, CI-red deep dive — all env/config layers fixed, 2 real third-party bugs found and deferred)

Continuing from the 6-CI-red-regression work above: pushing those fixes
triggered real CI, which itself cascaded through **9 more previously-hidden
layers**, each one only reachable because the previous layer's fix let the
pipeline progress further than any earlier run ever had. Every fix here is
its own commit on `develop`:

1. Confirmed on real CI: DLL staging (`STATUS_DLL_NOT_FOUND`) and
   `is_absolute()` fixes both work on actual `windows-2022` hardware.
2. Found + fixed: `tinyobjloader` ODR violation (Linux Clang ASan+UBSan) —
   crashed ~67/75 `mc3togltf_*` tests at startup; `MeshBuilder.cpp` compiled
   its own copy via `TINYOBJLOADER_IMPLEMENTATION` while also linking the
   separately-compiled, shared `tinyobjloader` CMake target. Fixed by
   dropping the link dependency everywhere (`mc3togltf/CMakeLists.txt`, root
   `CMakeLists.txt`'s editor target, the release-artifact install loop).
3. Found + fixed: `Mc3::writeFileAtomically()`'s finalize `rename()` failed
   ("No such file or directory" under a real MinGW+Wine repro, "Input/output
   error" on real CI) for the STAB-0555 Czech+Japanese filename case. Real
   root cause, found via the repro, not guesswork: `Mc3XmlWriter.cpp`/
   `Mc3XmlParser.cpp` called tinyxml2's `SaveFile`/`LoadFile(const char*)`
   with a UTF-8 `path.string()`; tinyxml2's `fopen()` converts that via the
   Windows ANSI code page, not UTF-8, mangling non-ASCII paths. Fixed with
   `_wfopen`-based `#ifdef _WIN32` helpers handing tinyxml2 a `FILE*` instead.
4. Fixed 2 minor Windows test-portability gaps: Python `UnicodeEncodeError`
   (`≠`/`≥` in PASS messages, cp1252 console) and a write-protection test
   assumption (`os.chmod()` on a directory is a no-op for blocking writes on
   Windows) — replaced with a platform-independent "parent directory never
   created" mechanism.
5. Found + fixed: `mcb_libfuzzer` OOM (550MB against a 512MB `rss_limit_mb`).
   Root-caused via local repro (NOT a per-input security bug — replaying the
   exact CI "oom-" artifact 2000x in one process leaked nothing): ordinary
   coverage-guided-fuzzing corpus growth + ASan overhead. Raised the shared
   `--rss-mib` ceiling 512→2048 in `run_fuzz_smoke.py`/`ci.yml`.
6. Found + fixed: Editor (EASYGL/VULKAN) jobs failing at CMake configure —
   CNA's vendored SDL3 needs X11 (or Wayland) dev headers, never installed.
   Added `libx11-dev`/`libxext-dev`/`libxrandr-dev`/`libxcursor-dev`/
   `libxi-dev`/`libxfixes-dev`/`libxss-dev` to both editor jobs.
7. Found + fixed: SDL3 also needs `libxtst-dev` for its X11 XTEST extension
   (missed in step 6) — added.
8. Found + fixed: CNA's `CNA_BACKEND_EASY_GL` option (enabled by every
   matrix entry, since the shared "Configure editor" step always configures
   both backends) requires a sibling `../easy-gl` checkout — CI never
   checked it out. Added a "Checkout easy-gl" step (pinned to this sandbox's
   local easy-gl sibling's HEAD) to both editor jobs.
9. Found + fixed: CNA also needs FFmpeg dev packages
   (`libavcodec-dev`/`libavformat-dev`/`libavutil-dev`/`libswresample-dev`)
   for an unconditional `pkg_check_modules(... REQUIRED ...)` — added.
10. Found + fixed: `easy-gl` itself needs a sibling `../meta-gl` checkout —
    one layer deeper than step 8. Added a "Checkout meta-gl" step (pinned to
    this sandbox's local meta-gl sibling's HEAD) to both editor jobs.

**Result after all of the above:** `Clang ASan+UBSan and bounded fuzz` and
`Standalone Windows qualification` are now **fully green** on real CI — the
first time either has passed completely this session. All 4 `standalone`
matrix jobs (`mc3`/`mcb`/`mc3togltf`/`mc3tomcb`) are green. The 3 Editor jobs
now all get **all the way through CMake configure** (never happened before
tonight) and fail only at the *build* step, on two genuinely different,
real third-party-repository bugs — filed as `SYS-W8-07`/`SYS-W8-08` in
`plan.md` rather than guessed at or fixed here:

- **`SYS-W8-07`**: `sharp-runtime/src/System/Environment.cpp:307` ignores
  `chdir()`'s return value; `-Werror=unused-result` (Editor VULKAN + plain
  EASYGL jobs) turns that into a hard build failure. A one-line fix, but
  inside `sharp-runtime`'s own source — outside this session's authorized
  scope. **User explicitly chose to report and defer, not fix**, when asked.
- **`SYS-W8-08`**: the Editor EASYGL ASan+UBSan job fails differently —
  `src/MeshCraft/Renderer/SceneRenderer.cpp` calls `ShaderEffect` methods
  (`setWorldProperty`/etc.) that don't exist on the CI-pinned `sharp-runtime`
  revision. Not yet triaged (source drift vs. stale pin vs. cache
  staleness) — needs investigation before any fix.

Also corrected a stale belief recorded in this file's own "Verification
caveat" section above (see the correction inserted there): Wine actually
runs MinGW-cross-compiled console/test binaries fine in this sandbox; only
the full GUI editor hits the documented `SIGSYS` block. This was the key
technique that let 2 of tonight's Windows-only bugs (items 2-3 above) get
*real* verification instead of source-level reasoning alone.
