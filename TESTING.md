# Testing

_Last updated: 2026-07-26. This document is derived from the actual
`CMakeLists.txt` registrations and test source files. Test registration and
pass totals are intentionally not recorded here: trust `ctest -N` and a fresh
CTest run in the build tree being reported._

MeshCraft's tests run through **CTest**, mixing C++ assertion binaries and
Python/bash subprocess-driven checks against the `mc3togltf`/`mc3tomcb` CLIs
and the `MeshCraft` editor (including headless real-pixel checks and optional
headless Blender verification). `render_display_preflight` verifies an actual
`xvfb-run` + `xdpyinfo` X client before render tests; if unavailable, it
reports a CTest skip and CMake disables only the other render-labelled tests.
The tables below are representative rather than a one-to-one test registry;
`ctest -N` and `ctest --print-labels` are authoritative.

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
ctest -R mc3togltf_csg --output-on-failure   # matches all CSG export tests

# Run one group by label (format/export/render/registry/ai/commands/lint/perf/unit)
ctest -L export --output-on-failure
ctest --print-labels   # list all labels

# Re-run only what failed last time
ctest --rerun-failed --output-on-failure
```

Expected result: every test selected in the current build passes. A host without
a working display can intentionally skip/disable the render-labelled subset;
record that environment result with the CTest output instead of treating it as
product coverage. `asset_lod` covers tier selection, hysteresis, culling, safe
fallbacks, and portable deterministic variants; `mc3togltf_asset_lod_export`
proves the explicit near/default export tier. `event_binding_dispatch` locks
pure eligibility/diagnostic behavior, `event_preview_runner` locks atomic
Preview/Play execution and rollback, and `mc3togltf_event_bindings_omitted`
locks explicit glTF omission. Run focused
suites with at most `-j4`. A failing test prints its assertion/subprocess
output inline with `--output-on-failure`.

Each C++ test binary can also be run directly (bypassing CTest) for faster iteration:

```sh
./cmake-build-debug/mc3/mc3_commands_test
./cmake-build-debug/mc3_registry_test
./cmake-build-debug/registry_workspace_test
./cmake-build-debug/ai_test
./cmake-build-debug/mc3/mc3_roundtrip_test
./cmake-build-debug/mcb/mcb_roundtrip_test
./cmake-build-debug/library_workflow_test
./cmake-build-debug/asset_lod_test
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

Expected: every test registered by each standalone build passes. (`mc3`'s
standalone build omits `mc3_commands_test` because it needs
`EditorAlgorithms.hpp` from the editor tree; this is intended, not a bug.)

### Installed CMake packages and CLI archive

`Mc3` and `Mcb` install under the `release` component with
`MeshCraft::Mc3` and `MeshCraft::Mcb` imported targets. The root CTest
`package_consumer_smoke` installs that component into the active build tree,
then configures and runs a separate consumer project using `find_package`.

```sh
ctest --test-dir cmake-build-debug -R '^package_consumer_smoke$' --output-on-failure
cmake --build cmake-build-debug --target meshcraft_cli_release -j2
```

The second command writes
`cmake-build-debug/MeshCraft-<version>-<system>-cli.tar.gz`. Its prefix
contains `bin/mc3tomcb`, `bin/mc3togltf`, the MC3/MCB headers and static
libraries, package configs, and non-system glTF-exporter runtime libraries.

`cli_cross_platform_fixture` writes `house.mcb` and `house.glb` under the
active build directory and compares their SHA-256 values with fixed source
contracts. The `windows-2022` CI job runs the same fixture only after all four
CNA-free standalone builds and CTests succeed; its `mc3tomcb.exe` and
`mc3togltf.exe` artifacts are therefore published only after that comparison.

### Sanitizer and fuzz qualification

CI runs each CNA-free standalone component under Clang AddressSanitizer and
UndefinedBehaviorSanitizer. It then runs four deterministic libFuzzer smoke
targets for XML, JSON, MCB, and self-contained GLB import. Each target has a
seed corpus, a 20-second total-time limit, a 5-second per-input timeout, a
512 MiB RSS limit, and a 1 MiB input limit. The MCB and GLB seed files are
generated from checked-in scenes immediately before the smoke run, so no
opaque binary blob is committed.

To reproduce the parser portion locally, use a stable build directory (not a
temporary directory) and Clang:

```sh
cmake -S mc3 -B cmake-build-verification-clang-sanitize/mc3 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DCMAKE_CXX_COMPILER=clang++ -DMESHCRAFT_FUZZ=ON
cmake --build cmake-build-verification-clang-sanitize/mc3 \
  --target mc3_xml_libfuzzer mc3_json_libfuzzer -j4
python3 test/run_fuzz_smoke.py --seconds 20 --rss-mib 512 \
  --work-dir cmake-build-verification-clang-sanitize/fuzz-work \
  --target cmake-build-verification-clang-sanitize/mc3/mc3_xml_libfuzzer mc3/test/fuzz/corpus/xml \
  --target cmake-build-verification-clang-sanitize/mc3/mc3_json_libfuzzer mc3/test/fuzz/corpus/json
```

These fuzzers are deliberately CI/manual targets rather than ordinary CTest
tests: their strict resource limits make failures actionable without adding a
fragile wall-clock gate to every local edit.

---

## Test reference

### C++ assertion-based binaries

| Test | Binary | Covers | Pass criteria |
|------|--------|--------|----------------|
| `mc3_registry` | `mc3_registry_test` | `ModelRegistry` (SQLite): open/save/search/remove, legacy-schema migration, deterministic preview cache invalidation, metadata/tag filters, duplicate/unused-material inspection, dependency-aware local asset-pack manifest and pinned-hash check, import-conflict rejection, unavailable-registry/DB-open edges, a mock AI-response → registry pipeline, and malformed-entry temp-file cleanup | 181 `PASS:` assertions, 0 `FAIL:` (re-counted 2026-07-26) |
| `model_registry_no_sqlite` | `model_registry_no_sqlite_test` | The real `#ifndef MESHCRAFT_HAS_SQLITE3` branch: harmless database API fallbacks plus local asset-pack/preview output without SQLite | 9 `PASS:` assertions, 0 `FAIL:` (2026-07-26) |
| `registry_workspace` | `registry_workspace_test` | CNA-free RegistryWorkspace lifecycle: panel ownership, deterministic AI-definition prefill, and closing only an AI-owned save dialog | All lifecycle assertions pass; no database or ImGui dependency |
| `scene_history` | `scene_history_test` | Separate memory-budgeted review timeline: automatic eviction, checkpoint retention/rejection, restore-with-selection and snapshot independence, large-document budget handling, and object/resource diff review. Existing `undo_manager` coverage continues to assert redo invalidation for the untouched exact undo stack. | 13 `PASS:` assertions, 0 `FAIL:` (2026-07-26) |
| `mc3_ai` | `ai_test` | `AiAssistant`'s JSON helpers; the AI-response validation pipeline (extract markdown/prose → repair → parse → reject-if-empty → validate against `mc3.xsd`); mock-HTTP-server round-trips (success, truncation, HTTP error, indefinite-hang timeout, model-name-on-the-wire, connection-refused, malformed-JSON-body, back-to-back-calls) — no real network call | 122 `PASS:` assertions, 0 `FAIL:` (re-counted 2026-07-20; was 73 as of 2026-07-07). The mock-server and libxml2-dependent cases are skipped (with a `SKIP:` line, not a failure) on builds without `MESHCRAFT_HAS_AI`/`MESHCRAFT_HAS_LIBXML2` |
| `mc3_roundtrip` | `mc3_roundtrip_test` | Full `.mc3.xml` parser/writer roundtrip for every element type, including all N1-N7 extensions, UTF-8/space-containing paths, and edge cases (legacy attribute forms, defaults) | 577 `PASS:` assertions, ends with `All tests passed.` (re-counted 2026-07-20; was 542 as of 2026-07-07) |
| `mc3_commands` | `mc3_commands_test` | Editor command algorithms (rename incl. empty-pattern/backslash edge cases, find/replace, array-dup, duplicate, group/ungroup), undo/redo round-trips for every mutating command, auto-save/backup, Save-As/Export-Selection/drag-drop/invalid-file-load workflows, keybinding/preferences/macro persistence formats, hierarchy-panel filtering, material-color resolution, undo-stack depth capping, AI-panel + unsaved-changes-confirmation dialog lifecycles | 558 `PASS:` assertions (re-counted 2026-07-20; was 510 as of 2026-07-07) |
| `mcb_roundtrip` | `mcb_roundtrip_test` | MCB binary encode/decode roundtrip for the base scene and all N1-N7 extension types, plus regression tests for a recursion-depth guard and a string-length sanity check | 236 `PASS:` assertions, ends with `All MCB roundtrip tests passed.` (re-counted 2026-07-20, later same day; was 234 earlier the same day, before the `SYS-W14-25` compression roundtrip cases landed, and 155 as of 2026-07-07) |
| `event_binding_dispatch` | `event_binding_dispatch_test` | CNA-free dry-run event binding dispatch: enter/exit/timer, cooldown, one-shot, dangling targets, recursion guard, execution budget, and authored-document immutability | Valid targets are reported as “would dispatch”; no trigger/state effect or document mutation occurs |
| `event_preview_runner` | `event_preview_runner_test` | CNA-free Preview/Play execution: nested Area enter/exit, timers, click fan-out, effects staged until commit, atomic state/script mutation, and failed-script rollback | A successful batch commits at most once; failed scripts preserve the live document and runtime eligibility while suppressing pending effects |
| `camera_bookmarks` | `camera_bookmarks_test` | Editor camera-bookmark capture/restore state | Empty/invalid slots are rejected; all orbit-camera fields round-trip |
| `transform_clipboard` | `transform_clipboard_test` | Editor transform clipboard | Copies only position/rotation/scale; empty paste is harmless and target pivot stays unchanged |
| `status_notification` | `status_notification_test` | Timed status-bar notification state | Message/severity replacement and expiry behavior |
| `object_lock_state` | `object_lock_state_test` | Editor-session object lock membership | Idempotent lock/unlock and toggle semantics |
| `benchmark_progress` | `benchmark_progress_test` | Headless `--benchmark` frame-progress lifecycle | Frame count/sample order, one-shot completion, startup timing, and invalid-count clamping |

Each assertion-based binary listed here prints one `PASS: <description>` or `FAIL: <description>` line per assertion and exits non-zero if any `FAIL:` occurred — grep for `^FAIL:` to find failures quickly in CI-style output.

### CLI-driving Python tests

These spawn the built `mc3togltf`/`mc3tomcb` binaries as subprocesses and assert on their output (exit code, generated glTF/GLB JSON structure, or file diffs). They need `PYTHON3_EXEC` (auto-detected by CMake) and are skipped entirely if no `python3` is found.

`capability_documentation` is a fast `lint` test that compares a small set of
mechanically-verifiable capability claims with their source anchors. It guards
Walk Mode proxy support, the web IDBFS bootstrap, autosave recovery, and
ordinary-object viewport UV mapping, plus the shared rotation-convention path;
it deliberately contains no volatile test or benchmark totals.

`lua_script_runner` and `trigger_fire` are CNA-free unit tests of the real Lua
runner and its trigger call path. They cover atomic rollback after a script
error, the instruction and 16 MiB allocation limits, finite values and material
references, successful placement, and the rule that a failed trigger script
does not create undo/dirty state.

`benchmark_editor` uses `uv_buffer_cache_large.mc3.xml`, a 100-instance
textured scene with one authored UV projection. Its warm-frame assertion
requires zero ordinary tint/authored-UV buffer creations and a mapped-UV cache
hit; it also requires the direct CPU mesh-generation, cold/warm CSG, and
live texture decode/upload category lines. It deliberately does not impose a
wall-clock threshold.

`scene_history` also covers the shared snapshot contract used by automatic
history and UndoManager: a 1,000-object fixture must retain one immutable
graph, restore a writable independent document and preserve paired selection.
It prints an informational attach-time/logical-retained-byte comparison rather
than enforcing a host-dependent RSS or wall-clock limit.

| Test | Script | Covers | Pass criteria |
|------|--------|--------|----------------|
| `smoke_test` | `test/smoke_test.sh` (bash, not Python — corrected 2026-07-07) | Editor binary starts, opens a scene headlessly (`--screenshot`), exits cleanly | Screenshot PPM written, non-empty, process exits 0 |
| `smoke_test_all_objects` / `smoke_test_empty_scene` / `smoke_test_missing_material` / `smoke_test_orthographic_camera` | `test/smoke_test.sh` against different fixtures | Same smoke mechanism against edge-case scenes: every `ObjectType`, a genuinely empty scene, a missing-material reference, an orthographic camera | Same as `smoke_test` |
| `missing_mesh_test` | `test/missing_mesh_test.sh` | A `<mesh>` with a nonexistent `src` renders a placeholder (not a crash) and prints a warning | Exit 0, warning printed |
| `xsd_validation` | `test/validate_xsd.py` | Every `test/*.mc3.xml` fixture validates against `mc3/mc3.xsd` (via `lxml`) | `PASS:` per file, `All N files valid.` |
| `mc3_roundtrip` / `mcb_roundtrip` | (C++ binaries, also `format`-labeled) | See the C++ assertion-based binaries table above | See above |
| `fog_linear_test` / `point_light_gizmo_test` / `spot_light_gizmo_test` / `look_through_camera_test` / `background_texture_test` / `skybox_texture_test` / `lod_test` | `test/*_test.py` | Real-pixel-sampling checks against a headless `--screenshot` PPM for each visual feature named | Substantial pixel-cluster match for the expected color/effect |
| `ssao_test` | `test/ssao_test.py` | CNA SSAO depth pre-pass, AO, blur, and multiplicative composition; compares `light_shading.mc3.xml` with the test hook off and on | More than 1,000 red pixels darken, total red darkening exceeds 2,000, and at least one pixel darkens by 2+ |
| `csg_cache_test` / `csg_cache_eviction_test` / `csg_material_viewport_test` | `test/csg_*_test.py` | Editor's content-hash CSG preview cache and material composition: static re-render is a cache hit; cache evicts/rebuilds when CSG inputs change; child material ranges render unless a root material overrides them | Evaluation-count diagnostics; red/blue child pixels and green root-override pixels |
| `editor_export_test` | `mc3/test/*` (export-labeled) | Editor's own Export Selection / subtree-template export path (not the standalone `mc3togltf` CLI) | Exported subtree round-trips and validates against `mc3.xsd` |
| `mc3tomcb_roundtrip` / `mc3tomcb_error_handling` | `mc3tomcb/test/*.py` | `mc3tomcb` CLI: `.mc3.xml → .mcb → .mc3.xml`, deterministic and lossless; malformed input produces a clean error, not a crash | File-diff / re-parse comparison passes; non-zero exit + message on bad input |
| `mc3togltf_gltf` | `mc3togltf/test/*.py` | Animation + house scene GLB export; magic bytes correct | Exported GLB is valid, expected node/animation counts |
| `mc3togltf_all_primitives` | `mc3togltf/test/*.py` | All primitive types export without error | Every primitive present as a glTF mesh |
| `mc3togltf_export_verification` | `mc3togltf/test/*.py` | Node count, mesh presence, material names in exported glTF | Assertions on parsed glTF JSON |
| `mc3togltf_event_bindings_omitted` | `mc3togltf/test/event_bindings_export_test.py` | MC3 event bindings have no glTF runtime equivalent | Export succeeds, emits one omission warning, and contains no event-binding payload |
| `mc3togltf_large_scene` | `test/large_scene_test.py` | Static 200-object `.mc3.xml` fixture exports correctly | Node count ≥ expected |
| `mc3togltf_large_scene_generated` | `test/large_scene_generated_test.py` | Python-generated 200-object scene (100 instances + 50 spheres + 50 boxes); confirms geometry **reuse**, not a 1-mesh-per-node blowup | `len(meshes) <= 6` (not ~200), `nodes >= meshes * 10`, per-shape-type mesh-sharing checks, export completes in < 30s |
| `mc3togltf_large_scene_500` / `mc3togltf_large_scene_1000` | `test/large_scene_generated_test.py` (scale arg `2.5` / `5.0`) | Same generator scaled to 500 / 1000 objects | Same reuse checks at scale; export completes well within timeout |
| `mc3togltf_large_obj_stress` | `mc3togltf/test/large_obj_stress_test.py` | >1M-triangle OBJ import/export stress test | Export completes, output is well-formed |
| `mc3togltf_csg_strict` | `mc3togltf/test/*.py` | CSG export fails hard (clear error, non-zero exit) without `--allow-approximate-csg` when an unsupported child type is present | Non-zero exit, error message present |
| `mc3togltf_csg_export` | `mc3togltf/test/*.py` | Union/difference/intersection evaluated by Manifold and exported as real merged geometry | Exported mesh has expected triangle count / bounds |
| `mc3togltf_csg_unsupported` | `mc3togltf/test/*.py` | An unsupported CSG child type (Plane, Disk, Grid, Mesh, Extrude) is detected and reported | Error names the unsupported type |
| `mc3togltf_csg_nested` | `mc3togltf/test/*.py` | Nested CSG operations (CSG-of-CSG) export correctly | Exported mesh matches expected nested-boolean result |
| `mc3togltf_csg_semantics` / `mc3togltf_csg_mesh_child` / `mc3togltf_csg_stress` / `mc3togltf_csg_shading_materials` | `mc3togltf/test/*.py` | `isCutter`/world-transform/empty-result/material-on-node CSG semantics; a `<mesh>` child inside a CSG node; a deep/many-child CSG stress case; generated CSG UVs, smooth normals, and child-material glTF primitives | Calibrated geometry / GLB-buffer / structural assertions |
| `mc3togltf_instance_deform_cache` | `mc3togltf/test/*.py` | Instances of the same definition with different `<deform>` produce separate cached meshes (not incorrectly shared) | Distinct mesh indices per distinct deform |
| `asset_lod` / `asset_lod_viewport_test` / `mc3togltf_asset_lod_export` | `test/asset_lod_test.cpp`, `test/asset_lod_viewport_test.py`, `mc3togltf/test/asset_lod_export_test.py` | Authored near/mid/far definition resolution, hysteresis, max-distance culling and debug output in the live viewport; explicit near/default tier in glTF | Safe fallback on missing references; viewport emits the expected tier/cull record; glTF mesh is the near definition |
| `mc3togltf_float_cache_key` / `mc3togltf_geom_cache_key` | `mc3togltf/test/*.py` | Two primitives with close-but-not-equal float dimensions produce 2 distinct meshes (cache key doesn't collide on float rounding); the geometry cache key's full construction is exercised directly | `len(meshes) == 2`; cache-key assertions |
| `mc3togltf_obj_robustness` | `mc3togltf/test/obj_robustness_test.py` | Untrusted OBJ input: out-of-range negative vertex index and an infinite (`1e400`-overflow) coordinate must not crash the exporter | Exit 0, a `Warning:`/`non-finite` message per malformed file, valid mesh still exports geometry, no `null` (non-finite) values in any accessor `min`/`max` |
| `mc3togltf_blender_import` | `mc3togltf/test/blender_import_test.py` | A synthetic 200-object generated scene imports cleanly into real headless Blender | Blender's glTF import operator reports `FINISHED`, expected mesh-object count |
| `mc3togltf_material_pbr_blender_import` | `mc3togltf/test/material_pbr_blender_import_test.py` | A single box + one PBR material survives export → Blender import with matching Base Color/Roughness/Metallic (added 2026-07-06, STAB-0431) | Values match the mc3 source within tolerance |
| `mc3togltf_release_sample_blender_import` | `mc3togltf/test/release_sample_blender_import_test.py` | A real, richly-authored scene (`medieval_castle.mc3.xml`) survives export → Blender import (added 2026-07-06, STAB-0642) | `FINISHED`, substantial mesh-object count |
| `mc3togltf_texture_sampler` / `mc3togltf_material_pbr` / `mc3togltf_node_transform` | `mc3togltf/test/*.py` | Texture wrap/filter sampler settings; PBR material factor export; node TRS transform export | Exact enum/value assertions on parsed glTF JSON |
| `mc3togltf_determinism` / `mc3togltf_golden` / `mc3togltf_no_partial_output` | `mc3togltf/test/*.py` | Two exports of the same input are byte-identical; output matches a golden file; a failed export never leaves a partial/corrupt file behind | Byte-identical diff; golden match; no output file on failure |
| `mc3togltf_svg_texture_export` | `mc3togltf/test/*.py` | External and inline SVG material textures rasterize into generated glTF PNG images | Each material keeps a valid texture/image reference; the external texture's wrap/filter sampler fields are exact |
| `mc3togltf_svg_texture_safety` | `mc3togltf/test/svg_texture_safety_test.py` | Malformed and hostile-dimension SVG inputs | Named non-fatal warning; no texture for malformed input; generated PNG is capped at 2048px |
| `mc3togltf_svg_rasterizer` | `mc3togltf/test/svg_rasterizer_test.cpp` | Compact SVG cache key plus external source timestamp invalidation | No inline markup in the key; changing the file changes its timestamp and rasterized pixels |
| `svg_texture_viewport_test` / `svg_inline_texture_viewport_test` | `test/svg_texture_viewport_test.py` | External and inline SVG material textures reach the live CNA viewport | Headless screenshots contain the fixtures' red and blue SVG pixels |
| `mc3togltf_embed_mesh_source` / `embed_mesh_viewport_test` | `mc3togltf/test/embed_mesh_source_test.py` | External self-contained and inline-base64 GLB `embed:` mesh sources; default-scene transform flattening; shared exporter/viewport loader | Both exported GLBs contain the transformed triangle without warnings; a real editor screenshot has visible embedded geometry and never falls back to the placeholder |

Blender-based tests (`mc3togltf_blender_import`, `mc3togltf_material_pbr_blender_import`, `mc3togltf_release_sample_blender_import`) are only registered when `find_program(BLENDER_EXEC blender)` finds a real `blender` binary at configure time — optional external tooling, not a hard build requirement.

---

## Writing a new test

- **New assertion inside an existing C++ binary** (e.g. another `mc3_commands` case): add a `static void testX()` function following the file's existing `CHECK(cond, "description")` pattern, call it from `main()`. No `CMakeLists.txt` change needed — but you do need a **cmake reconfigure**, not just a rebuild, if you added a new `.cpp` source file (`file(GLOB_RECURSE)` doesn't notice new files automatically — see `README.md`).
- **New standalone C++ test binary**: register it explicitly in the root `CMakeLists.txt`'s `if(MESH_CRAFT_BUILD_TESTING AND UNIX AND NOT ANDROID AND NOT EMSCRIPTEN)` block — see the `ai_test`/`mc3_registry_test` entries there for the pattern (`add_executable` → `target_include_directories` → `target_link_libraries` → `add_test` → `set_tests_properties(... PROPERTIES TIMEOUT ...)`).
- **New CLI-driving Python test**: follow `mc3togltf/test/large_scene_generated_test.py`'s pattern — take the target binary path as `sys.argv[1]`, build a temp scene/file, run the subprocess, assert on its output or exit code, `print(...)` a `PASS:`-style line per check, and register the test with `add_test(NAME ... COMMAND "${PYTHON3_EXEC}" "${CMAKE_CURRENT_SOURCE_DIR}/test/your_test.py" "$<TARGET_FILE:your_target>")` plus a `TIMEOUT`.
- Either way: reconfigure with **CLion's bundled cmake (4.2.2)** for `cmake-build-debug/`, not the system cmake — see `README.md`/`NEXT.md` for why.

## `TESTING.md` vs `NEXT.md` vs `plan.md`

- **This file** — how to run tests and what each one checks. Reference material, changes rarely.
- **`NEXT.md`** — current status, current blocker, exact next task. Changes every session.
- **`plan.md`** — the authoritative per-task (`AUD-###`/`SYS-###`) status for the active backlog, including tasks not yet covered by any test above. (The stabilization policy and Gates A-F structure that used to live in `STABILIZATION.md` is archived at `docs/history/STABILIZATION.md` — the phase it governed is now substantively complete.)
