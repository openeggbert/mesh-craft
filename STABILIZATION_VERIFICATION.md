# STABILIZATION_VERIFICATION.md

Point-in-time verification snapshot. Unlike `STABILIZATION_WORKLOG.md` (a chronological narrative of what was investigated and why), this file is a dashboard: the current, actually-checked state of each verification area, each line traceable to a command that was run. Refresh this file's numbers whenever a full clean verification pass is repeated — don't hand-edit a number without re-running its command.

_Last verified: 2026-07-07, from a genuinely clean build (`rm -rf cmake-build-debug` + full reconfigure/rebuild), commit `4b7fcd2`._

---

## Build

| Check | Command | Result |
|---|---|---|
| Clean configure | `<clion-cmake> -S . -B cmake-build-debug -DBUILD_TESTING=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -G Ninja` (after `rm -rf cmake-build-debug`) | ✅ succeeds, 0 errors (79.5s) |
| Clean build | `ninja -j$(nproc)` | ✅ exit 0, 120/120 steps (was exit 1 before the `CNA_ENABLE_NET` fix — see below) |
| Compiler warnings | grep of full build log for `warning` (excluding filename false-positives) | ✅ 0 real warnings |
| Compiler / CMake versions | `g++ --version`, `cmake --version` | `g++ (Debian 14.2.0-19) 14.2.0`; CLion-bundled `cmake 4.2.2` (required — system `cmake 3.31.6` has a documented reconfigure bug for this project) |

**Fixed this session**: `ninja` (default `all` target) failed from a truly clean build directory — `CNA_GamerServices`/`CNA_Net` (never linked by any MeshCraft target) defaulted to being built via `../cna`'s `CNA_ENABLE_NET` option (default `ON`), and one of their source files called a `RegionInfo` API that `../sharp-runtime` had renamed 2 days earlier. Fixed entirely on the MeshCraft side (`set(CNA_ENABLE_NET OFF CACHE BOOL "" FORCE)` in `CMakeLists.txt`) — zero CNA/sharp-runtime changes. Full trace: `STABILIZATION_WORKLOG.md` Phase 1, `plan.md` "Post-650 Follow-Up Findings" item 4.

## Tests

| Check | Command | Result |
|---|---|---|
| Full suite | `ctest --test-dir cmake-build-debug --output-on-failure` | ✅ **66/66 passed**, exit 0 |
| Test registration integrity | Regex-extracted `add_test(NAME ...)` from all 5 project `CMakeLists.txt` files vs. `ctest -N` output | ✅ exact 1:1 match — 66 declared, 66 registered, 0 orphans, 0 phantoms |
| Label breakdown | `ctest --print-labels` + per-label counts | `ai` 1, `commands` 1, `export` 44, `format` 3, `registry` 1, `render` 16 = 66 |
| `mc3_registry_test` | `./mc3_registry_test \| grep -c '^PASS:'` | 152 `PASS:` |
| `ai_test` | `./ai_test \| grep -c '^PASS:'` | 73 `PASS:` |
| `mc3_roundtrip_test` | `./mc3/mc3_roundtrip_test \| grep -c '^PASS:'` | 542 `PASS:` |
| `mc3_commands_test` | `./mc3/mc3_commands_test \| grep -c '^PASS:'` | 510 `PASS:` |
| `mcb_roundtrip_test` | `./mcb/mcb_roundtrip_test \| grep -c '^PASS:'` | 155 `PASS:` |

## XSD Validation

| Check | Command | Result |
|---|---|---|
| All test fixtures | `python3 test/validate_xsd.py mc3/mc3.xsd test/*.mc3.xml` | ✅ **69/69 files valid**, exit 0 |

## Standalone (CNA-free) Subproject Builds

Each must build and test independently, without the root project or CNA (per `CLAUDE.md`).

| Subproject | Configure | Build | Tests |
|---|---|---|---|
| `mc3` | ✅ | ✅ | ✅ 1/1 |
| `mcb` | ✅ | ✅ | ✅ 1/1 |
| `mc3togltf` | ✅ | ✅ | ✅ 41/41 (was 24/24 as of an earlier, stale doc revision — re-counted from an actual run) |
| `mc3tomcb` | ✅ | ✅ | ✅ 3/3 |

## Cross-Platform (re-verified this session)

| Platform | Status | Detail |
|---|---|---|
| Linux (native) | ✅ | Full build + 66/66 tests, see above |
| Windows (MinGW cross-compile) | 🟡 | GUI editor (`MeshCraft.exe`) blocked on 1 CNA-side (`GLES3/gl3.h`) + 3 sharp-runtime-side (`-Werror`) issues, all out of this project's scope. **New**: `mc3togltf.exe`/`mc3tomcb.exe` build and link as real Windows PE32+ executables (confirmed via `file`) |
| Web (Emscripten) | ✅ | Full build succeeds; real headless-Chrome session confirms a working WebGL2 context, no console/GPU errors (`--headless=new --enable-unsafe-swiftshader --use-gl=angle --use-angle=swiftshader`) |
| Android | ❓ | Not attempted — no Android NDK installed in this environment |

## Stabilization Plan (`plan.md`) Status

| Metric | Value |
|---|---|
| Total rows | 650 (STAB-0001–STAB-0650, contiguous, no duplicates — verified via ID-range check) |
| ✅ Done (verified) | 620 |
| 🟡 Flagged (real gap / blocked / product decision) | 29 |
| 🧪 Needs test / partial | 0 |
| 📋 Planned / not started | 1 (STAB-0650 — CI activation, blocked on repo owner PAT rotation) |
| 🔴 Known bug | 0 |
| Structural integrity | ✅ every row has a valid status/priority/title/files/verification field — 0 malformed rows |

**No row is marked ✅ without a verification method described in its own writeup** — this was true before this session's audit and was not found to be violated by spot-checking (see `STABILIZATION_WORKLOG.md` Phase 5 for the specific rows re-checked).

---

## How to refresh this file

```sh
rm -rf cmake-build-debug
<clion-cmake> -S . -B cmake-build-debug -DBUILD_TESTING=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -G Ninja
cd cmake-build-debug && ninja -j$(nproc) && ctest --output-on-failure
cd .. && python3 test/validate_xsd.py mc3/mc3.xsd test/*.mc3.xml
```

Then re-run the standalone subproject builds (see `TESTING.md`) if verifying those too. Update every number in this file only from an actual command's output — never hand-increment.
