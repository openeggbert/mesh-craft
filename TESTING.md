# Testing

_Last updated: 2026-07-06. This document is derived from the actual `CMakeLists.txt` test registrations and test source files — if it drifts from `ctest -N`'s output, trust `ctest -N`._

MeshCraft's tests run through **CTest** — 47 tests today, mixing C++ assertion-based binaries and Python subprocess-driven checks against the `mc3togltf`/`mc3tomcb` CLIs and the `MeshCraft` editor binary itself (headless `--screenshot` real-pixel-sampling tests). **Known gap:** the "CLI-driving Python tests" table below only documents ~17 of the ~44 registered Python tests — the `render`-labeled smoke/pixel-sampling tests (`smoke_test*`, `fog_linear_test`, `point_light_gizmo_test`, etc.) and several newer `mc3togltf_*` tests aren't listed yet; `ctest -N` and `ctest --print-labels` are authoritative until this table is refreshed.

---

## Running the tests

```sh
# Configure + build (see README.md for prerequisites)
cmake -S . -B cmake-build-debug -DBUILD_TESTING=ON
ninja -C cmake-build-debug

# Run everything
cd cmake-build-debug && ctest --output-on-failure

# List all registered tests
ctest -N

# Run one test by name (regex match)
ctest -R mc3_commands --output-on-failure
ctest -R mc3togltf_csg --output-on-failure   # matches all 4 CSG tests

# Run one group by label (format/export/render/registry/ai/commands)
ctest -L export --output-on-failure
ctest --print-labels   # list all labels

# Re-run only what failed last time
ctest --rerun-failed --output-on-failure
```

Expected result: **47/47 Passed**. A failing test prints its assertion/subprocess output inline with `--output-on-failure`; without that flag, CTest only shows pass/fail per test name.

Each C++ test binary can also be run directly (bypassing CTest) for faster iteration:

```sh
./cmake-build-debug/mc3/mc3_commands_test
./cmake-build-debug/mc3_registry_test
./cmake-build-debug/ai_test
./cmake-build-debug/mc3/mc3_roundtrip_test
./cmake-build-debug/mcb/mcb_roundtrip_test
```

### Standalone (CNA-free) builds

`mc3/`, `mcb/`, `mc3togltf/`, and `mc3tomcb/` must each build and test independently, without the root project or CNA:

```sh
for c in mc3 mcb mc3togltf mc3tomcb; do
  cmake -S "$c" -B "$c-build" -G Ninja -DBUILD_TESTING=ON -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
  cmake --build "$c-build" -j4
  (cd "$c-build" && ctest --output-on-failure)
done
```

Expected: `mc3` 1/1, `mcb` 1/1, `mc3togltf` 24/24, `mc3tomcb` 3/3. (`mc3`'s standalone build registers only `mc3_roundtrip_test` — `mc3_commands_test` needs `EditorAlgorithms.hpp` from the editor tree and is skipped outside the root build; this is intended, not a bug.)

---

## Test reference

### C++ assertion-based binaries

| Test | Binary | Covers | Pass criteria |
|------|--------|--------|----------------|
| `mc3_registry` | `mc3_registry_test` | `ModelRegistry` (SQLite): open/save/search/remove/migration (including a real legacy-schema `ALTER TABLE ADD COLUMN` migration test, STAB-0061), unavailable-registry and DB-open-failure edge cases, schema introspection | 109 `PASS:` assertions, 0 `FAIL:` |
| `mc3_ai` | `ai_test` | `AiAssistant`'s JSON helpers; the AI-response validation pipeline (extract markdown/prose → repair → parse → reject-if-empty → validate against `mc3.xsd`); four end-to-end mock-HTTP-server round-trips (success, truncation, HTTP error, indefinite-hang timeout) — no real network call | 57 `PASS:` assertions, 0 `FAIL:`. The mock-server and libxml2-dependent cases are skipped (with a `SKIP:` line, not a failure) on builds without `MESHCRAFT_HAS_AI`/`MESHCRAFT_HAS_LIBXML2` |
| `mc3_roundtrip` | `mc3_roundtrip_test` | Full `.mc3.xml` parser/writer roundtrip for every element type, including all N1-N7 extensions, UTF-8/space-containing paths, and edge cases (legacy attribute forms, defaults) | 413 `PASS:` assertions, ends with `All tests passed.` |
| `mc3_commands` | `mc3_commands_test` | Editor command algorithms (rename incl. empty-pattern/backslash edge cases, find/replace, array-dup, duplicate, group/ungroup), undo/redo round-trips for every mutating command, auto-save/backup, Save-As/Export-Selection/drag-drop/invalid-file-load workflows, keybinding/preferences/macro persistence formats, hierarchy-panel filtering, material-color resolution, undo-stack depth capping, AI-panel + unsaved-changes-confirmation dialog lifecycles | 489 `PASS:` assertions |
| `mcb_roundtrip` | `mcb_roundtrip_test` | MCB binary encode/decode roundtrip for the base scene and all N1-N7 extension types | 50 `PASS:` assertions, ends with `All MCB roundtrip tests passed.` |

All five print one `PASS: <description>` or `FAIL: <description>` line per assertion and exit non-zero if any `FAIL:` occurred — grep for `^FAIL:` to find failures quickly in CI-style output.

### CLI-driving Python tests

These spawn the built `mc3togltf`/`mc3tomcb` binaries as subprocesses and assert on their output (exit code, generated glTF/GLB JSON structure, or file diffs). They need `PYTHON3_EXEC` (auto-detected by CMake) and are skipped entirely if no `python3` is found.

| Test | Script | Covers | Pass criteria |
|------|--------|--------|----------------|
| `smoke_test` | `test/smoke_test.py` | Editor binary starts, opens a scene headlessly, exits cleanly | Process exits 0, no crash |
| `xsd_validation` | `test/validate_xsd.py` | Every `test/*.mc3.xml` fixture validates against `mc3/mc3.xsd` (via `lxml`) | `PASS:` per file, `All N files valid.` |
| `mc3tomcb_roundtrip` | `mc3tomcb/test/*.py` | `mc3tomcb` CLI: `.mc3.xml → .mcb → .mc3.xml`, deterministic and lossless | File-diff / re-parse comparison passes |
| `mc3togltf_gltf` | `mc3togltf/test/*.py` | Animation + house scene GLB export; magic bytes correct | Exported GLB is valid, expected node/animation counts |
| `mc3togltf_all_primitives` | `mc3togltf/test/*.py` | All primitive types export without error | Every primitive present as a glTF mesh |
| `mc3togltf_export_verification` | `mc3togltf/test/*.py` | Node count, mesh presence, material names in exported glTF | Assertions on parsed glTF JSON |
| `mc3togltf_large_scene` | `test/large_scene_test.py` | Static 200-object `.mc3.xml` fixture exports correctly | Node count ≥ expected |
| `mc3togltf_large_scene_generated` | `test/large_scene_generated_test.py` | Python-generated 200-object scene (100 instances + 50 spheres + 50 boxes); confirms geometry **reuse**, not a 1-mesh-per-node blowup | `len(meshes) <= 6` (not ~200), `nodes >= meshes * 10`, per-shape-type mesh-sharing checks, export completes in < 30s |
| `mc3togltf_large_scene_500` | `test/large_scene_generated_test.py` (scale arg `2.5`) | Same generator scaled to 500 objects (250+125+125) | Same reuse checks at 500-object scale; export completes in < 30s (measured ~0.02s) |
| `mc3togltf_csg_strict` | `mc3togltf/test/*.py` | CSG export fails hard (clear error, non-zero exit) without `--allow-approximate-csg` when an unsupported child type is present | Non-zero exit, error message present |
| `mc3togltf_csg_export` | `mc3togltf/test/*.py` | Union/difference/intersection evaluated by Manifold and exported as real merged geometry | Exported mesh has expected triangle count / bounds |
| `mc3togltf_csg_unsupported` | `mc3togltf/test/*.py` | An unsupported CSG child type (Plane, Disk, Grid, Mesh, Extrude) is detected and reported | Error names the unsupported type |
| `mc3togltf_csg_nested` | `mc3togltf/test/*.py` | Nested CSG operations (CSG-of-CSG) export correctly | Exported mesh matches expected nested-boolean result |
| `mc3togltf_instance_deform_cache` | `mc3togltf/test/*.py` | Instances of the same definition with different `<deform>` produce separate cached meshes (not incorrectly shared) | Distinct mesh indices per distinct deform |
| `mc3togltf_float_cache_key` | `mc3togltf/test/*.py` | Two primitives with close-but-not-equal float dimensions produce 2 distinct meshes (cache key doesn't collide on float rounding) | `len(meshes) == 2` |
| `mc3togltf_obj_robustness` | `mc3togltf/test/obj_robustness_test.py` | Untrusted OBJ input: out-of-range negative vertex index and an infinite (`1e400`-overflow) coordinate must not crash the exporter | Exit 0, a `Warning:`/`non-finite` message per malformed file, valid mesh still exports geometry, no `null` (non-finite) values in any accessor `min`/`max` |

---

## Writing a new test

- **New assertion inside an existing C++ binary** (e.g. another `mc3_commands` case): add a `static void testX()` function following the file's existing `CHECK(cond, "description")` pattern, call it from `main()`. No `CMakeLists.txt` change needed — but you do need a **cmake reconfigure**, not just a rebuild, if you added a new `.cpp` source file (`file(GLOB_RECURSE)` doesn't notice new files automatically — see `README.md`).
- **New standalone C++ test binary**: register it explicitly in the root `CMakeLists.txt`'s `if(MESH_CRAFT_BUILD_TESTING AND UNIX AND NOT ANDROID AND NOT EMSCRIPTEN)` block — see the `ai_test`/`mc3_registry_test` entries there for the pattern (`add_executable` → `target_include_directories` → `target_link_libraries` → `add_test` → `set_tests_properties(... PROPERTIES TIMEOUT ...)`).
- **New CLI-driving Python test**: follow `mc3togltf/test/large_scene_generated_test.py`'s pattern — take the target binary path as `sys.argv[1]`, build a temp scene/file, run the subprocess, assert on its output or exit code, `print(...)` a `PASS:`-style line per check, and register the test with `add_test(NAME ... COMMAND "${PYTHON3_EXEC}" "${CMAKE_CURRENT_SOURCE_DIR}/test/your_test.py" "$<TARGET_FILE:your_target>")` plus a `TIMEOUT`.
- Either way: reconfigure with **CLion's bundled cmake (4.2.2)** for `cmake-build-debug/`, not the system cmake — see `README.md`/`NEXT.md` for why.

## `TESTING.md` vs `NEXT.md` vs `STABILIZATION.md`

- **This file** — how to run tests and what each one checks. Reference material, changes rarely.
- **`NEXT.md`** — current status, current blocker, exact next task. Changes every session.
- **`STABILIZATION.md`** — the stabilization policy and gate structure. Changes when a gate closes.
- **`plan.md`** — the authoritative per-task (`STAB-XXXX`) status for all 650 backlog items, including ones not yet covered by any test above.
