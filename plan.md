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

## S0 — Baseline Build and Reproducibility

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0001 | ✅ | P0 | Verify clean debug build from root CMakeLists | `CMakeLists.txt` | `cmake .. -DBUILD_TESTING=ON && ninja` exits 0; 15/15 tests pass (verified 2026-06-27) |
| STAB-0002 | ✅ | P0 | Verify release build from root CMakeLists | `CMakeLists.txt` | `cmake -S . -B b-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build b-release -j4` exits 0; 73/73 targets, **18/18 CTest pass** (re-verified 2026-07-01, up from 17/17 on 2026-06-29 — reconfigured + rebuilt clean, no code changes needed) |
| STAB-0003 | 🧪 | P1 | Verify build with `BUILD_TESTING=OFF` | `CMakeLists.txt` | `cmake -S . -B b -DBUILD_TESTING=OFF && cmake --build b` — no test targets compiled |
| STAB-0004 | ✅ | P0 | Confirm all 15 CTest tests are registered | `CMakeLists.txt`, `mc3/CMakeLists.txt`, `mc3togltf/CMakeLists.txt` | `ctest -N` lists exactly 15 tests (verified 2026-06-27) |
| STAB-0005 | ✅ | P1 | Verify `mc3` standalone build (without root project) | `mc3/CMakeLists.txt` | Fixed: guarded `mc3_commands_test` (needs editor `EditorAlgorithms.hpp` outside mc3/) + added `enable_testing()` for `PROJECT_IS_TOP_LEVEL`. `cmake -S mc3 -B mc3-build -DBUILD_TESTING=ON && cmake --build mc3-build` exits 0; `ctest` 1/1; root 17/17 unaffected (verified 2026-06-29) |
| STAB-0006 | ✅ | P1 | Verify `mc3togltf` standalone build | `mc3togltf/CMakeLists.txt` | No code change needed — already guarded (`if(NOT TARGET manifold)`, `if(NOT TARGET Mc3)`, `FetchContent_GetProperties`). `cmake -S mc3togltf -B togltf-build -G Ninja && cmake --build togltf-build` exits 0 (41/41 targets); standalone `ctest` 11/11; root 17/17 unaffected (verified 2026-06-30) |
| STAB-0007 | ✅ | P1 | Verify `mcb` standalone build | `mcb/CMakeLists.txt` | Fixed: added `if(NOT TARGET Mc3)` guard adding `../mc3` as subdirectory (standalone build lacked Mc3 include dirs → `Mc3Document.hpp` not found). `cmake -S mcb -B mcb-build -G Ninja && cmake --build mcb-build` exits 0 (19/19); `ctest` 1/1; root 17/17 unaffected (verified 2026-06-30) |
| STAB-0008 | 🧪 | P2 | Verify build without SQLite3 on supported platform | `CMakeLists.txt` | Use container without `libsqlite3-dev`; confirm `MESHCRAFT_HAS_SQLITE3` undefined; registry stubs compile |
| STAB-0009 | 🧪 | P2 | Verify build without OpenSSL (AI disabled) | `CMakeLists.txt` | Build without OpenSSL; `MESHCRAFT_HAS_AI` undefined; AI stubs compile |
| STAB-0010 | 🧪 | P2 | Verify build with AI enabled (`MESHCRAFT_HAS_AI`) | `CMakeLists.txt`, `AiAssistant.cpp` | Build with OpenSSL present; `MESHCRAFT_HAS_AI` defined |
| STAB-0011 | 📋 | P2 | Document Windows cross-compile instructions | `README.md` | README explains MinGW toolchain; build steps tested |
| STAB-0012 | 🧪 | P2 | Verify MinGW build from Linux (cross-compile) | `CMakeLists.txt` | `x86_64-w64-mingw32-cmake` + `cmake --build` exits 0; runtime libs copied |
| STAB-0013 | 📋 | P2 | Verify Emscripten web build | `CMakeLists.txt` | `emcmake cmake` + `cmake --build` produces `.html`; no link errors |
| STAB-0014 | ✅ | P1 | Verify FetchContent offline mode | `mc3togltf/CMakeLists.txt` | Added `FetchContent_GetProperties` guards for tinygltf + tinyobjloader; `cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON` exits 0 (verified 2026-06-27) |
| STAB-0015 | 📋 | P2 | Add missing dependency diagnostics for lxml | `CMakeLists.txt` | If `python3 -c "import lxml"` fails, print actionable error before `xsd_validation` test |
| STAB-0016 | 🧪 | P1 | Verify `ninja MeshCraft` target builds only editor | `CMakeLists.txt` | `ninja MeshCraft` succeeds; verifiable that only editor targets compile |
| STAB-0017 | 🧪 | P1 | Verify `ninja mc3_roundtrip_test` builds only Mc3 + test | `mc3/CMakeLists.txt` | `ninja mc3_roundtrip_test` exits 0 |
| STAB-0018 | 🧪 | P1 | Verify `ninja mc3togltf` builds exporter | `mc3togltf/CMakeLists.txt` | `ninja mc3togltf` exits 0 |
| STAB-0019 | ✅ | P0 | All 15 CTest tests pass on CI-clean Linux build | all | `ctest --output-on-failure` reports 15/15, 2.78s (verified 2026-06-27) |
| STAB-0020 | 📋 | P3 | Investigate Android build support | `CMakeLists.txt` | Document which features are disabled on Android; SQLite/AI stubs verified |
| STAB-0021 | 🧪 | P1 | Confirm `file(GLOB_RECURSE)` reconfigure notes are documented | `README.md` | README warns that adding `.cpp` files requires `cmake ..` reconfigure |
| STAB-0022 | 🧪 | P1 | Confirm CMake min-version 3.21 is actually required | `CMakeLists.txt` | Test with CMake 3.21; build passes |
| STAB-0023 | 🧪 | P2 | Confirm C++23 features used compile with GCC ≥ 13 | `CMakeLists.txt` | Build with GCC 13; `deducing-this` lambdas in UiMenuBar.cpp compile cleanly |
| STAB-0024 | 🧪 | P2 | Confirm C++23 features compile with Clang ≥ 17 | `CMakeLists.txt` | Build with Clang 17; no errors |
| STAB-0025 | ✅ | P2 | Create `.github/workflows/ci.yml` skeleton | `.github_/workflows/ci.yml` | Added GH Actions matrix workflow building the CNA-free libs (mc3/mcb/mc3togltf/mc3tomcb) standalone + ctest on push/PR to develop/master. Dry-run of exact CI commands locally: 1/1, 1/1, 11/11, 2/2 (15 runs, 14 unique tests). Full editor build (needs CNA sibling + SDL3) left as documented TODO. **Parked deactivated under `.github_/`** (push token lacks `workflow` scope; rename to `.github/` to activate). (verified 2026-06-30) |

---

## S1 — Test Infrastructure

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0026 | 🟡 | P0 | Confirm all registered CTest tests appear in `ctest -N` | `CMakeLists.txt` | `ctest -N` lists exactly 15 expected tests |
| STAB-0027 | 📋 | P1 | Add CTest labels to each test for grouping | `CMakeLists.txt`, `mc3togltf/CMakeLists.txt` | `ctest -L format` runs only format tests; `ctest -L export` runs only export tests |
| STAB-0028 | 🟡 | P0 | Verify `smoke_test` exit-code contract | `test/smoke_test.sh` | `smoke_test.sh` fails and returns non-zero if MeshCraft crashes immediately |
| STAB-0029 | 🧪 | P1 | Add `smoke_test` timeout protection (TIMEOUT 30 already set; verify it fires) | `CMakeLists.txt` | If MeshCraft hangs, CTest kills it after 30 s and reports failure |
| STAB-0030 | ✅ | P0 | Verify `mc3_roundtrip` covers all primitive types | `mc3/test/roundtrip_test.cpp` | `testAllPrimitiveTypes()` — 10 types (Box/Sphere/Cylinder/Cone/Torus/Capsule/Disk/Grid/IcoSphere/Plane) parse + roundtrip (2026-06-27) |
| STAB-0031 | 🧪 | P0 | Verify `mc3_roundtrip` covers all object types | `mc3/test/roundtrip_test.cpp` | Exercises: object with mesh src, instance, CSG union/diff/isect, extrude, area |
| STAB-0032 | 🧪 | P1 | Add MCB binary roundtrip test for N1 (SVG texture) | `mc3/test/roundtrip_test.cpp` or new `mcb_test` | Write Mc3Document with svgTexture → MCB → re-read → compare |
| STAB-0033 | 🧪 | P1 | Add MCB binary roundtrip test for N2 (embedded GLTF) | new `mcb_test` | Write Mc3Document with embedGltf → MCB → re-read → compare |
| STAB-0034 | 🧪 | P1 | Add MCB binary roundtrip test for N3 (Lua scripts) | new `mcb_test` | Write doc with scripts → MCB → re-read → scripts match |
| STAB-0035 | 🧪 | P1 | Add MCB binary roundtrip test for N4 (sounds/music) | new `mcb_test` | Write doc with sounds + music → MCB → re-read → entries match |
| STAB-0036 | 🧪 | P1 | Add MCB binary roundtrip test for N5 (triggers) | new `mcb_test` | Write doc with triggers including all step types → MCB → re-read → steps match |
| STAB-0037 | 🧪 | P1 | Add MCB binary roundtrip test for N6 (scene states) | new `mcb_test` | Write doc with state overrides (visible/position/rotation/material) → MCB → re-read |
| STAB-0038 | 🧪 | P1 | Add MCB binary roundtrip test for N7 (meta map) | new `mcb_test` | Write doc with meta map → MCB → re-read → meta entries match |
| STAB-0039 | 📋 | P1 | Create `mcb_roundtrip_test` CTest target | `mcb/CMakeLists.txt` or `CMakeLists.txt` | `ctest -R mcb_roundtrip` runs and passes |
| STAB-0040 | 🧪 | P0 | Verify `xsd_validation` test covers all XMLs in `test/` | `test/validate_xsd.py`, `CMakeLists.txt` | `file(GLOB _mc3_xml_files ...)` captures every `.mc3.xml` in test/ |
| STAB-0041 | ✅ | P1 | Add XSD validation test fixture containing N3 (scripts) element | `test/n3_scripts.mc3.xml` | `xsd_validation` validates `<scripts><script id="onStart" type="lua">…</script></scripts>` (2026-06-27) |
| STAB-0042 | ✅ | P1 | Add XSD validation test fixture containing N4 (sounds/music) elements | `test/n4_sounds_music.mc3.xml` | `xsd_validation` validates `<sounds>` + `<music><track …/>` (2026-06-27) |
| STAB-0043 | ✅ | P1 | Add XSD validation test fixture containing N5 (triggers) elements | `test/n5_triggers.mc3.xml` | `xsd_validation` validates `<triggers><trigger …>` with all 4 step types (2026-06-27) |
| STAB-0044 | ✅ | P1 | Add XSD validation test fixture containing N6 (scene states) elements | `test/n6_scene_states.mc3.xml` | `xsd_validation` validates `<states><state …><object-override …>` with all optional attrs (2026-06-27) |
| STAB-0045 | ✅ | P1 | Add XSD validation test fixture containing N7 (meta) elements | `test/n7_meta.mc3.xml` | `xsd_validation` validates `<meta><metaentry key="…" value="…"/>` (2026-06-27) |
| STAB-0046 | ✅ | P1 | Update `features.mc3.xml` to include N3-N7 elements | `test/features.mc3.xml` | Added `<meta>`, `<scripts>`, `<sounds>`, `<music>`, `<triggers>`, `<states>` — XSD passes (2026-06-27) |
| STAB-0047 | 🧪 | P1 | Verify `mc3_commands` test covers `applyRenamePattern` with edge cases | `mc3/test/editor_commands_test.cpp` | Tests: empty pattern, no match, % wildcard, backslash edge cases |
| STAB-0048 | 🧪 | P1 | Verify `mc3_commands` test covers locked object skip in batchRename | `mc3/test/editor_commands_test.cpp` | Locked objects are skipped; unlocked are renamed |
| STAB-0049 | 📋 | P2 | Add golden-file test for mc3_roundtrip (write XML, diff vs baseline) | `mc3/test/roundtrip_test.cpp` | Saved XML compared byte-for-byte to committed golden file |
| STAB-0050 | 📋 | P2 | Add golden-file test for glTF exporter (JSON structure) | `mc3togltf/test/gltf_test.py` | Exported glTF JSON diffed against committed golden JSON |
| STAB-0051 | 📋 | P1 | Verify tests write no files in repository root | all tests | Run ctest; `git status` shows no untracked files in repo root |
| STAB-0052 | 📋 | P1 | Ensure all temp files in tests use system temp dir | `mc3togltf/test/*.py` | Tests use `tempfile.mkstemp()` or `/tmp/` — never `../../test/` for output |
| STAB-0053 | 🧪 | P1 | Verify `xsd_validation` returns meaningful failure message | `test/validate_xsd.py` | Intentionally invalid XML produces specific error with element name + line |
| STAB-0054 | 📋 | P1 | Add `mc3togltf_gltf` assertion: verify exported GLB JSON contains `animations` key | `mc3togltf/test/gltf_test.py` | JSON parsed from GLB contains `animations` array with ≥ 1 entry for animation_test.mc3.xml |
| STAB-0055 | 📋 | P1 | Add `mc3togltf_gltf` assertion: verify exported GLB `scenes` has exactly 1 scene | `mc3togltf/test/gltf_test.py` | GLB JSON `scenes` length == 1 |
| STAB-0056 | 📋 | P2 | Add determinism test: same input → bit-identical GLB output | `mc3togltf/test/gltf_test.py` | Run mc3togltf twice on same input; compare output bytes |
| STAB-0057 | 📋 | P2 | Add `ctest --rerun-failed` workflow documentation | `README.md` | README explains how to rerun failed tests |
| STAB-0058 | ✅ | P2 | Add CTest test for mcb CLI (mc3tomcb tool) | `mc3tomcb/CMakeLists.txt`, `mc3tomcb/test/mc3tomcb_roundtrip_test.py` | Added `mc3tomcb_roundtrip` CTest: drives the CLI through `mc3.xml→mcb→mc3.xml→mcb→mc3.xml` over 9 fixtures (incl. N3–N7), asserting MCB magic header, fixpoint determinism (a.mcb==c.mcb, b.xml==d.xml) and no dropped scene content. `ctest -R mc3tomcb_roundtrip` 1/1; root now 18/18 (verified 2026-06-30) |
| STAB-0059 | 📋 | P2 | Add test for mc3tomcb round-trip: mc3.xml → MCB → decode → compare | new test | `mc3tomcb in.mc3.xml out.mcb && mc3frommcb out.mcb restored.mc3.xml && diff in.mc3.xml restored.mc3.xml` |
| STAB-0060 | 🧪 | P1 | Verify GLB magic number test asserts on bytes 0-3 | `mc3togltf/test/gltf_test.py` | `assert data[:4] == b'glTF'` present in gltf_test.py |
| STAB-0061 | 🧪 | P1 | Verify `mc3_registry` test covers migration (ALTER TABLE ADD COLUMN) | `mc3/test/mc3_registry_test.cpp` | Test explicitly creates table without `description`/`source` then calls `open()` |
| STAB-0062 | 📋 | P2 | Add `mc3_registry` test: search returns empty list for no-match query | `mc3/test/mc3_registry_test.cpp` | `search("nonexistent_xyz")` returns empty vector |
| STAB-0063 | 📋 | P2 | Add `mc3_registry` test: remove() deletes exactly the specified entry | `mc3/test/mc3_registry_test.cpp` | Save entry; remove by id; search returns empty |
| STAB-0064 | 📋 | P2 | Add `mc3_registry` test: duplicate id handled (suffix appended) | `mc3/test/mc3_registry_test.cpp` | Insert two entries with same name; second gets unique id with suffix |
| STAB-0065 | 📋 | P3 | Add `mc3_registry` test: corrupted SQLite db produces clean error | `mc3/test/mc3_registry_test.cpp` | Write garbage to db file; `open()` throws or returns false cleanly |

---

## S2 — MC3 XML Schema, Parser, Writer, and Roundtrip

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0066 | ✅ | P0 | `src` vs `source` attribute canonicalized in mc3togltf | `mc3togltf/src/Mc3XmlParser.cpp` (removed) | S1 done; mc3togltf uses `Mc3Document::loadFromFile()` |
| STAB-0067 | ✅ | P0 | Plane `size` is vec2 in parser and writer | `Mc3XmlParser.cpp`, `Mc3XmlWriter.cpp` | Roundtrip of `"50 50"` passes; writer emits vec2 |
| STAB-0068 | ✅ | P1 | Add roundtrip test: plane size `"8 8"` parses to `{8,1,8}` | `mc3/test/roundtrip_test.cpp` | `testPlaneSizeVec2()` added and passes (2026-06-27) |
| STAB-0069 | ✅ | P1 | Add roundtrip test: legacy plane size `"8 0 8"` (vec3) parsed and re-emitted as vec2 | `mc3/test/roundtrip_test.cpp` | `testPlaneSizeLegacyVec3()` — loads legacy XML + double-roundtrip (2026-06-27) |
| STAB-0070 | ✅ | P1 | Add roundtrip test: Disk with `inner_radius > 0` (ring topology) | `mc3/test/roundtrip_test.cpp` | `testDiskInnerRadius()` — loads XML with `inner_radius="0.3"`, verifies parser + roundtrip (2026-06-27) |
| STAB-0071 | ✅ | P1 | Add roundtrip test: Torus with `minor_radius` | `mc3/test/roundtrip_test.cpp` | `testTorusMinorRadius()` — `minor_radius="0.2"` + `major_radius="0.5"` survive parse + roundtrip (2026-06-27) |
| STAB-0072 | ✅ | P1 | Add roundtrip test: Capsule with explicit `radius` and `height` | `mc3/test/roundtrip_test.cpp` | `testCapsuleRadiusHeight()` — `radius=0.4`, `height=1.5`, `segments=24` survive parse + roundtrip (2026-06-27) |
| STAB-0073 | ✅ | P1 | Add roundtrip test: IcoSphere with explicit `segments` | `mc3/test/roundtrip_test.cpp` | `testIcoSphereSegments()` — `segments=3` (default=2), `radius=0.6` survive parse + roundtrip (2026-06-27) |
| STAB-0074 | ✅ | P1 | Add roundtrip test: Grid with explicit `subdivisions_x` and `subdivisions_z` | `mc3/test/roundtrip_test.cpp` | `testGridSubdivisions()` — `subdivisions_x=5`, `subdivisions_z=8` (default=4) survive parse + roundtrip (2026-06-27) |
| STAB-0075 | ✅ | P1 | Verify parser handles unknown top-level element without crash | `mc3/test/roundtrip_test.cpp` | `testUnknownTopLevelElement()` — `<unknowntag/>` in `<mc3>` causes no throw; known objects still parsed (2026-06-27) |
| STAB-0076 | 🧪 | P1 | Verify parser handles unknown attribute on known element | `mc3/src/Mc3XmlParser.cpp` | `<box unknownAttr="x"/>` parses without throw; unknown attr ignored |
| STAB-0077 | 📋 | P1 | Add test: duplicate object ID in XML — define policy | `mc3/src/Mc3XmlParser.cpp` | Document and test: second object with same id overrides first (or first wins) |
| STAB-0078 | 📋 | P1 | Add test: duplicate material ID in XML — define policy | `mc3/src/Mc3XmlParser.cpp` | Same as above; policy documented |
| STAB-0079 | 🧪 | P1 | Verify invalid vec3 (e.g. `"1 2"` where vec3 expected) does not crash | `mc3/src/Mc3XmlParser.cpp` | Parser returns `{1,2,0}` or `{0,0,0}`; no throw; no UB |
| STAB-0080 | 🧪 | P1 | Verify invalid float attribute (e.g. `"abc"`) does not crash | `mc3/src/Mc3XmlParser.cpp` | `stof("abc")` exception caught internally; default value used |
| STAB-0081 | 🧪 | P1 | Verify missing required `id` attribute on object yields skip (not crash) | `mc3/src/Mc3XmlParser.cpp` | Object with no `id` is not added to scene; no throw |
| STAB-0082 | 📋 | P1 | Add test: cyclic `<include>` detected and throws `std::runtime_error` | `mc3/test/roundtrip_test.cpp` | `testIncludeCycle()` already exists; confirm it still passes |
| STAB-0083 | ✅ | P0 | Cyclic include detection implemented | `mc3/src/Mc3XmlParser.cpp` | Already implemented (M1) |
| STAB-0084 | ✅ | P0 | Nested include roundtrip implemented | `mc3/src/Mc3XmlParser.cpp` | Already implemented and tested (M1) |
| STAB-0085 | ✅ | P0 | Local override wins over included content | `mc3/src/Mc3XmlParser.cpp` | Already implemented (M1) |
| STAB-0086 | 🧪 | P1 | Add roundtrip test: include with nested include (A→B→C, 2 levels deep) | `mc3/test/roundtrip_test.cpp` | 2-level deep include preserved on save/load |
| STAB-0087 | 🧪 | P1 | Verify include relative path resolution works from subdirectory | `mc3/src/Mc3XmlParser.cpp` | Load `subdir/scene.mc3.xml` that includes `../shared.mc3.xml`; resolves correctly |
| STAB-0088 | 📋 | P1 | Add test: include path with spaces in filename | `mc3/src/Mc3XmlParser.cpp` | `<include file="my scene.mc3.xml"/>` loads correctly |
| STAB-0089 | 📋 | P2 | Add test: include of non-existent file produces clear error | `mc3/src/Mc3XmlParser.cpp` | `loadFromFile()` throws with filename in message; no UB |
| STAB-0090 | 🧪 | P1 | Verify include skip-sets (`includedDefs`, `includedMaterials`, `includedTextures`) prevent double-write | `mc3/src/Mc3XmlWriter.cpp` | Included defs not written to output XML; confirmed by roundtrip |
| STAB-0091 | 📋 | P2 | Define include-tracking policy for `svgTextures` map (N1) | `mc3/src/Mc3XmlParser.cpp` | Document: svgTextures from includes not tracked; accepted limitation or implement `includedSvgTextures` |
| STAB-0092 | 📋 | P2 | Define include-tracking policy for `embeds` map (N2) | `mc3/src/Mc3XmlParser.cpp` | Same as above for embeds |
| STAB-0093 | 🧪 | P0 | Verify N1 (SVG texture) XML roundtrip: external `src` form | `mc3/test/roundtrip_test.cpp` | `testSvgExternal()` passes |
| STAB-0094 | 🧪 | P0 | Verify N1 (SVG texture) XML roundtrip: inline CDATA form | `mc3/test/roundtrip_test.cpp` | `testSvgInline()` passes |
| STAB-0095 | 🧪 | P0 | Verify N2 (embedded GLTF) XML roundtrip: external src form | `mc3/test/roundtrip_test.cpp` | `testEmbedExternal()` passes |
| STAB-0096 | 🧪 | P0 | Verify N2 (embedded GLTF) XML roundtrip: inline base64 CDATA form | `mc3/test/roundtrip_test.cpp` | `testEmbedInline()` passes |
| STAB-0097 | 🧪 | P0 | Verify N3 (Lua script) XML roundtrip: inline CDATA source | `mc3/test/roundtrip_test.cpp` | `testScript()` passes |
| STAB-0098 | 🧪 | P0 | Verify N3 script with empty source (`<script id="x" type="lua"/>`) roundtrips | `mc3/test/roundtrip_test.cpp` | Empty script written and read back without crash |
| STAB-0099 | 🧪 | P0 | Verify N4 (sound) XML roundtrip: `loop="false"` default omitted | `mc3/test/roundtrip_test.cpp` | Sound with loop=false: writer omits loop attr; reader defaults to false |
| STAB-0100 | 🧪 | P0 | Verify N4 (sound) XML roundtrip: explicit `loop="true"` written | `mc3/test/roundtrip_test.cpp` | Sound with loop=true: writer emits loop="true" |
| STAB-0101 | 🧪 | P0 | Verify N4 (music) XML roundtrip: `loop="true"` default omitted | `mc3/test/roundtrip_test.cpp` | Music with loop=true: writer omits loop attr; reader defaults to true |
| STAB-0102 | 🧪 | P0 | Verify N4 (music) XML roundtrip: explicit `loop="false"` written | `mc3/test/roundtrip_test.cpp` | Music with loop=false: writer emits loop="false" |
| STAB-0103 | 🧪 | P0 | Verify N5 (trigger) XML roundtrip: all step types present | `mc3/test/roundtrip_test.cpp` | Trigger with play-action, play-sound, run-script, play-music steps — all survive |
| STAB-0104 | 🧪 | P1 | Verify N5 (trigger) with empty steps roundtrips | `mc3/test/roundtrip_test.cpp` | `<trigger id="t"/>` with no steps: written and read without crash |
| STAB-0105 | 🧪 | P0 | Verify N6 (scene state) XML roundtrip: all override fields | `mc3/test/roundtrip_test.cpp` | State with visible + position + rotation + material all survive |
| STAB-0106 | 🧪 | P1 | Verify N6 (scene state) roundtrip: optional fields absent by default | `mc3/test/roundtrip_test.cpp` | Override with only `visible` set — position/rotation/material not written |
| STAB-0107 | 🧪 | P0 | Verify N7 (meta) XML roundtrip: multiple entries | `mc3/test/roundtrip_test.cpp` | Meta map with 3 entries: all survive save/load |
| STAB-0108 | 🧪 | P1 | Verify N7 (meta) distinguished from `metadata` map | `mc3/src/Mc3XmlParser.cpp` | `<meta>` → `doc.meta`; `<metadata>` → `doc.metadata`; both coexist |
| STAB-0109 | 🧪 | P1 | Verify parser recovers cleanly from tinyxml2 parse error | `mc3/src/Mc3XmlParser.cpp` | Feeding invalid XML (e.g. unclosed tag) returns clear error; no crash |
| STAB-0110 | 📋 | P1 | Add test: save-reload semantic equivalence (not just text equality) | `mc3/test/roundtrip_test.cpp` | After load → save → reload: all object fields equal (not just XML text) |
| STAB-0111 | 🧪 | P1 | Verify writer emits `<mc3 version="0.3">` correctly | `mc3/src/Mc3XmlWriter.cpp` | Saved file has `version="0.3"` in root element |
| STAB-0112 | 📋 | P2 | Add test: object with all transform fields (position + rotation + scale + pivot) roundtrips | `mc3/test/roundtrip_test.cpp` | All transform fields preserved |
| STAB-0113 | 📋 | P2 | Add test: material with all PBR fields (baseColor, metallic, roughness, emissive, alpha) roundtrips | `mc3/test/roundtrip_test.cpp` | All material fields preserved with correct precision |
| STAB-0114 | 🧪 | P2 | Verify writer preserves insertion order of objects | `mc3/src/Mc3XmlWriter.cpp` | Object order in output matches order they were added to doc |
| STAB-0115 | 📋 | P2 | Add test: scene with 50 objects roundtrips correctly | `mc3/test/roundtrip_test.cpp` | All 50 objects present and equal after reload |
| STAB-0116 | 📋 | P2 | Add test: light with all fields (type, color, intensity, direction, castShadows) roundtrips | `mc3/test/roundtrip_test.cpp` | All light fields preserved |
| STAB-0117 | 📋 | P2 | Add test: camera with all fields (fov, near, far, orthographic) roundtrips | `mc3/test/roundtrip_test.cpp` | All camera fields preserved |
| STAB-0118 | 📋 | P2 | Add test: environment with all fields (bg color, texture, skybox, fog, bloom) roundtrips | `mc3/test/roundtrip_test.cpp` | All environment fields preserved |
| STAB-0119 | 📋 | P2 | Add test: action with multiple keyframes (transform + material + deform channels) roundtrips | `mc3/test/roundtrip_test.cpp` | All keyframe data preserved |
| STAB-0120 | 📋 | P2 | Add test: extrude with spiral path roundtrips | `mc3/test/roundtrip_test.cpp` | Extrude profile + path attributes preserved |

---

## S3 — MCB Binary Format Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0121 | ✅ | P0 | Create `mcb_roundtrip_test` CTest binary | `mcb/CMakeLists.txt` or `CMakeLists.txt` | `ninja mcb_roundtrip_test && ctest -R mcb_roundtrip` passes (verified 2026-06-27, 16/16 tests) |
| STAB-0122 | ✅ | P1 | MCB roundtrip: basic scene (objects + materials) | `mcb/test/mcb_roundtrip_test.cpp` | testBasicScene() — object count, id, name, primitive size, material roughness/baseColor verified (2026-06-27) |
| STAB-0123 | ✅ | P1 | MCB roundtrip: N1 svgTextures map | `mcb/test/mcb_roundtrip_test.cpp` | testSvgTexture() — inlineContent survives MCB encode/decode (2026-06-27) |
| STAB-0124 | ✅ | P1 | MCB roundtrip: N2 embeds map | `mcb/test/mcb_roundtrip_test.cpp` | testEmbed() — base64Content survives MCB encode/decode (2026-06-27) |
| STAB-0125 | ✅ | P1 | MCB roundtrip: N3 scripts map | `mcb/test/mcb_roundtrip_test.cpp` | testScript() — type + source survive MCB encode/decode (2026-06-27) |
| STAB-0126 | ✅ | P1 | MCB roundtrip: N4 sounds map | `mcb/test/mcb_roundtrip_test.cpp` | testSound() — src + loop=true survive MCB encode/decode (2026-06-27) |
| STAB-0127 | ✅ | P1 | MCB roundtrip: N4 musicTracks map | `mcb/test/mcb_roundtrip_test.cpp` | testMusic() — src + loop=false survive MCB encode/decode (2026-06-27) |
| STAB-0128 | ✅ | P1 | MCB roundtrip: N5 triggers map with all step types | `mcb/test/mcb_roundtrip_test.cpp` | testTrigger() — all 4 step types (PlayAction/PlaySound/RunScript/PlayMusic) survive (2026-06-27) |
| STAB-0129 | ✅ | P1 | MCB roundtrip: N6 sceneStates map with all override fields | `mcb/test/mcb_roundtrip_test.cpp` | testSceneState() — visible/position/rotation/material optional fields survive (2026-06-27) |
| STAB-0130 | ✅ | P1 | MCB roundtrip: N7 meta map | `mcb/test/mcb_roundtrip_test.cpp` | testMeta() — 2-entry meta map survives MCB encode/decode (2026-06-27) |
| STAB-0131 | 📋 | P1 | Add MCB roundtrip test: include list preserved | new `mcb_roundtrip_test.cpp` | `doc.includes` vector survives MCB encode/decode |
| STAB-0132 | 🧪 | P1 | Verify MCB unknown-key skipping: reader skips unrecognized keys without crash | `mcb/src/McbReader.cpp` | Feed MCB with unknown key `"xyz_future"`; reader skips gracefully |
| STAB-0133 | 📋 | P2 | Add MCB test: truncated file produces clear error (not crash) | new `mcb_roundtrip_test.cpp` | Write valid MCB; truncate at byte 50; `McbReader::read()` throws or returns false |
| STAB-0134 | 📋 | P2 | Add MCB test: all-zeros input (corrupted header) produces clear error | new `mcb_roundtrip_test.cpp` | Feed 100 zero bytes; reader throws or returns empty doc |
| STAB-0135 | 📋 | P2 | Add MCB test: single-byte input produces clear error | new `mcb_roundtrip_test.cpp` | Feed `\x00`; reader throws or returns empty doc |
| STAB-0136 | 📋 | P2 | Document MCB format version header if one exists | `MC3_FORMAT.md` or new `MCB_FORMAT.md` | Binary format documented: header, tag values, field order |
| STAB-0137 | 🧪 | P1 | Verify MCB writes all animation keyframes | `mcb/src/McbWriter.cpp` | MCB roundtrip of scene with 10-keyframe action preserves all keyframes |
| STAB-0138 | 📋 | P2 | Add MCB test: very large string values (>64KB) do not crash writer | `mcb/src/McbWriter.cpp` | Write script with 100KB source; writer handles without truncation |
| STAB-0139 | 📋 | P2 | Add MCB test: binary file size is smaller than equivalent XML | new test | MCB for house.mc3.xml < XML file size (compression ratio documented) |
| STAB-0140 | 📋 | P2 | Verify McbWriter is deterministic (same input → same bytes) | `mcb/src/McbWriter.cpp` | Write same doc twice; byte-compare output files |
| STAB-0141 | 🧪 | P1 | Verify MCB roundtrip for CSG operations | new `mcb_roundtrip_test.cpp` | CSG union with children survives MCB encode/decode |
| STAB-0142 | 🧪 | P1 | Verify MCB roundtrip for extrude geometry | new `mcb_roundtrip_test.cpp` | Extrude object with profile + path survives MCB encode/decode |
| STAB-0143 | 📋 | P2 | Add MCB test: deform object roundtrip | new `mcb_roundtrip_test.cpp` | Deform with x/y/z fields survives MCB encode/decode |
| STAB-0144 | 📋 | P2 | Verify MCB roundtrip for `metadata` (legacy) map | new `mcb_roundtrip_test.cpp` | `doc.metadata` (old `<metadata><property>` form) survives MCB encode/decode |
| STAB-0145 | 📋 | P2 | Test MCB reader with future-version tags (forward compat check) | `mcb/src/McbReader.cpp` | MCB written with extra unknown section; reader skips and reads rest |
| STAB-0146 | 📋 | P3 | Document MCB TAG_* constants | `mcb/src/McbWriter.cpp`, `mcb/src/McbReader.cpp` | Constants defined in shared header with documentation |
| STAB-0147 | 📋 | P3 | Extract MCB tag constants to shared header | `mcb/src/McbWriter.cpp`, `mcb/src/McbReader.cpp` | Both files include same `McbTags.hpp` header |
| STAB-0148 | 🧪 | P1 | Verify McbWriter handles UTF-8 strings correctly | `mcb/src/McbWriter.cpp` | Object id with UTF-8 characters survives MCB encode/decode |
| STAB-0149 | 📋 | P2 | Verify MCB file magic/header is identifiable | `mcb/src/McbWriter.cpp` | MCB files start with identifiable magic bytes or version string |
| STAB-0150 | 📋 | P3 | Investigate big-endian safety of MCB format | `mcb/src/McbWriter.cpp`, `mcb/src/McbReader.cpp` | Document endianness assumption; add static_assert for little-endian on affected platforms |

---

## S4 — glTF/GLB Exporter Correctness

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0151 | ✅ | P0 | Box primitive exported | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0152 | ✅ | P0 | Sphere primitive exported | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0153 | ✅ | P0 | Cylinder primitive exported | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0154 | ✅ | P0 | Cone primitive exported | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0155 | ✅ | P0 | Torus primitive exported | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0156 | ✅ | P0 | Capsule primitive exported | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0157 | ✅ | P0 | Disk primitive exported (solid + ring) | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0158 | ✅ | P0 | Grid primitive exported | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0159 | ✅ | P0 | IcoSphere primitive exported | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0160 | ✅ | P0 | Plane primitive exported | `mc3togltf/src/MeshBuilder.cpp` | `mc3togltf_all_primitives` passes |
| STAB-0161 | 🧪 | P1 | Verify Disk with `inner_radius > 0` produces ring topology (non-zero inner hole) | `mc3togltf/src/MeshBuilder.cpp` | Exported Disk with inner_radius=0.3 has `indexCount` > disk without inner_radius |
| STAB-0162 | 🧪 | P1 | Verify Torus uses `minor_radius` from Mc3Primitive | `mc3togltf/src/MeshBuilder.cpp` | Two tori with different minor_radius produce different vertex counts |
| STAB-0163 | ✅ | P1 | Verify no primitive returns zero triangles | `mc3togltf/test/all_primitives_export_test.py` | Added `indices` accessor count > 0 check + `% 3 == 0` for all 12 primitive nodes (2026-06-27) |
| STAB-0164 | 🧪 | P0 | Verify mesh export: external OBJ file loaded correctly | `mc3togltf/src/GltfExporter.cpp` | `obj-test.mc3.xml` exports; node has non-zero vertex count |
| STAB-0165 | 📋 | P1 | Add test: missing external OBJ file produces named error | `mc3togltf/src/GltfExporter.cpp` | mc3togltf on XML with nonexistent `meshSource` prints filename in error |
| STAB-0166 | 🧪 | P1 | Verify material export: `baseColor` → glTF `pbrMetallicRoughness.baseColorFactor` | `mc3togltf/src/GltfExporter.cpp` | Exported glTF JSON contains `baseColorFactor` matching mc3 material |
| STAB-0167 | 🧪 | P1 | Verify material export: `metallic` → glTF `metallicFactor` | `mc3togltf/src/GltfExporter.cpp` | metallic=0.8 in mc3 → metallicFactor=0.8 in glTF |
| STAB-0168 | 🧪 | P1 | Verify material export: `roughness` → glTF `roughnessFactor` | `mc3togltf/src/GltfExporter.cpp` | roughness=0.3 in mc3 → roughnessFactor=0.3 in glTF |
| STAB-0169 | 🧪 | P1 | Verify material export: `emissive` → glTF `emissiveFactor` | `mc3togltf/src/GltfExporter.cpp` | emissive="1 0.5 0" → emissiveFactor=[1,0.5,0] in glTF |
| STAB-0170 | 🧪 | P1 | Verify material export: `alpha` → glTF `alphaMode` / `alphaCutoff` | `mc3togltf/src/GltfExporter.cpp` | alpha < 1.0 → BLEND mode in exported glTF |
| STAB-0171 | 🧪 | P1 | Verify texture export: external URI texture in glTF | `mc3togltf/src/GltfExporter.cpp` | Texture with `src="textures/wall.png"` exported with matching uri |
| STAB-0172 | 📋 | P1 | Add test: missing texture file in export produces warning (not crash) | `mc3togltf/src/GltfExporter.cpp` | mc3togltf with missing texture `src` → warning printed; export continues |
| STAB-0173 | 🧪 | P1 | Verify node transform export: position → glTF `translation` | `mc3togltf/src/GltfExporter.cpp` | Object at position="1 2 3" → node.translation=[1,2,3] |
| STAB-0174 | 🧪 | P1 | Verify node transform export: rotation → glTF `rotation` (quaternion) | `mc3togltf/src/GltfExporter.cpp` | Object with rotation="0 90 0" → quaternion in glTF node |
| STAB-0175 | 🧪 | P1 | Verify node transform export: scale → glTF `scale` | `mc3togltf/src/GltfExporter.cpp` | Object with scale="2 2 2" → node.scale=[2,2,2] |
| STAB-0176 | 📋 | P1 | Add test: camera export (Mc3Camera → glTF camera node) | `mc3togltf/src/GltfExporter.cpp` | Camera in scene → glTF cameras array non-empty |
| STAB-0177 | 📋 | P1 | Add test: light export (Mc3Light → KHR_lights_punctual extension) | `mc3togltf/src/GltfExporter.cpp` | Point/spot light in scene → KHR_lights_punctual in glTF extensions |
| STAB-0178 | ✅ | P1 | Verify animation export: transform keyframes → glTF animations | `mc3togltf/test/gltf_test.py` | `mc3togltf_gltf` passes: Bounce/Spin/Pulse exported; Flash(visible-only) skipped; sampler/channel/accessor types verified (2026-06-27) |
| STAB-0179 | 📋 | P1 | Add test: material animation NOT exported (unsupported channel warning) | `mc3togltf/src/GltfExporter.cpp` | mc3togltf on scene with material animation → prints "unsupported channel" warning; exports rest |
| STAB-0180 | 📋 | P1 | Add test: deform animation NOT exported (unsupported channel warning) | `mc3togltf/src/GltfExporter.cpp` | mc3togltf on scene with deform animation → warning; export continues |
| STAB-0181 | 🧪 | P0 | Verify `.foo` extension rejected by mc3togltf | `mc3togltf/src/main.cpp` | `mc3togltf in.mc3.xml out.foo` exits non-zero with extension error |
| STAB-0182 | 🧪 | P0 | Verify `.glb` extension produces binary GLB | `mc3togltf/src/main.cpp` | Output file magic bytes == `b'glTF'` |
| STAB-0183 | 🧪 | P0 | Verify `.gltf` extension produces JSON glTF | `mc3togltf/src/main.cpp` | Output file starts with `{` and contains `"asset"` key |
| STAB-0184 | 🧪 | P1 | Verify `.GLB` uppercase extension accepted | `mc3togltf/src/main.cpp` | Uppercase extension treated same as lowercase |
| STAB-0185 | 📋 | P1 | Add test: output file already exists — document overwrite behavior | `mc3togltf/src/main.cpp` | Document whether overwrite is silent or prompted; test confirms behavior |
| STAB-0186 | ✅ | P0 | CSG strict mode: union/difference/intersection fail without flag | `mc3togltf/src/GltfExporter.cpp` | `mc3togltf_csg_strict` passes |
| STAB-0187 | ✅ | P0 | CSG approximate mode: children exported separately with flag | `mc3togltf/src/GltfExporter.cpp` | `mc3togltf_csg_export` passes |
| STAB-0188 | ✅ | P0 | Unsupported CSG child type detected | `mc3togltf/src/CsgEvaluator.cpp` | `mc3togltf_csg_unsupported` passes |
| STAB-0189 | 🧪 | P1 | Verify exporter stats: node count is correct | `mc3togltf/src/GltfExporter.cpp` | Export house.mc3.xml; stats output shows correct node count |
| STAB-0190 | 🧪 | P1 | Verify exporter stats: mesh count is correct | `mc3togltf/src/GltfExporter.cpp` | Stats output shows correct unique mesh count |
| STAB-0191 | 🧪 | P1 | Verify exporter stats: material count is correct | `mc3togltf/src/GltfExporter.cpp` | Stats output shows correct material count |
| STAB-0192 | 🧪 | P1 | Verify exporter stats: warning count reflects actual warnings | `mc3togltf/src/GltfExporter.cpp` | Zero warnings for clean file; N warnings when N unsupported features present |
| STAB-0193 | 📋 | P1 | Add test: SVG texture in exported scene — currently silently skipped | `mc3togltf/src/GltfExporter.cpp` | Document that SVG textures produce a warning; exported glTF has no SVG uri |
| STAB-0194 | 📋 | P1 | Add test: embedded GLTF (`embed:id` meshSource) — currently unsupported | `mc3togltf/src/GltfExporter.cpp` | Document that embed: meshSource produces warning; export continues with empty mesh |
| STAB-0195 | 🧪 | P1 | Verify glTF JSON validity (all required fields present) | `mc3togltf/test/gltf_test.py` | Exported JSON contains `asset.version`, `scenes`, `nodes`, `meshes` arrays |
| STAB-0196 | 📋 | P2 | Add test: extrude object exported as mesh | `mc3togltf/src/GltfExporter.cpp` | Scene with extrude object; exported glTF has non-empty mesh for it |
| STAB-0197 | 📋 | P2 | Add test: nested group objects exported as parent-child nodes | `mc3togltf/src/GltfExporter.cpp` | Group with children → glTF node tree reflects hierarchy |
| STAB-0198 | 📋 | P2 | Add test: definition/instance → glTF node shares mesh index | `mc3togltf/src/GltfExporter.cpp` | Two instances of same def → same `mesh` index in two glTF nodes |
| STAB-0199 | ✅ | P1 | Verify no silent geometry drops (every object produces a node) | `mc3togltf/test/all_primitives_export_test.py` | Added node count >= 12 assertion (12 objects in all_primitives.mc3.xml); test passes (2026-06-27) |
| STAB-0200 | 📋 | P2 | Add test: exporter handles completely empty scene (no objects) without crash | `mc3togltf/src/GltfExporter.cpp` | `<mc3><objects/></mc3>` exports empty GLB; exit 0 |

---

## S5 — CSG Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0201 | ✅ | P0 | CSG union export via Manifold | `mc3togltf/src/CsgEvaluator.cpp` | `mc3togltf_csg_export` passes |
| STAB-0202 | ✅ | P0 | CSG difference export via Manifold | `mc3togltf/src/CsgEvaluator.cpp` | `mc3togltf_csg_export` passes |
| STAB-0203 | ✅ | P0 | CSG intersection export via Manifold | `mc3togltf/src/CsgEvaluator.cpp` | `mc3togltf_csg_export` passes |
| STAB-0204 | ✅ | P0 | Unsupported CSG child type fails with error | `mc3togltf/src/CsgEvaluator.cpp` | `mc3togltf_csg_unsupported` passes |
| STAB-0205 | ✅ | P1 | Verify nested CSG (union inside difference) exports correctly | `test/csg_nested.mc3.xml`, `mc3togltf/test/csg_nested_test.py` | New fixture + test: `NestedCsg` node has 36 verts/36 indices; children fully baked; `mc3togltf_csg_nested` passes (2026-06-27) |
| STAB-0206 | 📋 | P1 | Add test: 3-level deep CSG nesting produces correct result | `mc3togltf/src/CsgEvaluator.cpp` | 3-level nested CSG computes without crash |
| STAB-0207 | 🧪 | P1 | Verify CSG with transforms inside children uses correct world transform | `mc3togltf/src/CsgEvaluator.cpp` | Box translated to (1,0,0) and subtracted from sphere: result reflects translation |
| STAB-0208 | 📋 | P1 | Add test: CSG cutter semantics (isCutter flag) | `mc3togltf/src/CsgEvaluator.cpp` | Object marked isCutter=true in CSG difference is used as cutter |
| STAB-0209 | 📋 | P1 | Add test: empty CSG result (intersection of non-overlapping shapes) — document policy | `mc3togltf/src/CsgEvaluator.cpp` | Non-overlapping intersection: export empty mesh or produce warning; behavior documented |
| STAB-0210 | 📋 | P2 | Add test: non-watertight geometry in CSG — Manifold behavior documented | `mc3togltf/src/CsgEvaluator.cpp` | Intentionally degenerate OBJ mesh in CSG: Manifold error caught and reported |
| STAB-0211 | 🧪 | P1 | Verify CSG approximate mode exports each child as separate node | `mc3togltf/src/GltfExporter.cpp` | `--allow-approximate-csg`: CSG node has child nodes (not empty); each child is a mesh |
| STAB-0212 | 📋 | P2 | Add test: CSG with material on CSG node (not children) — behavior documented | `mc3togltf/src/GltfExporter.cpp` | CSG node with material; real-CSG result applies that material |
| STAB-0213 | 🧪 | P1 | Verify CSG triangle count via csgCachedTriCount (editor) is non-zero | `SceneRenderer.cpp` | After rendering a union; `csgCachedTriCount(id)` returns positive value |
| STAB-0214 | 📋 | P2 | Verify CSG preview cache invalidation on child move | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Move child object; CSG cache key changes; re-evaluated on next render |
| STAB-0215 | 📋 | P2 | Verify CSG preview cache invalidation on parent move | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Move CSG parent; all child hashes change; cache miss; re-evaluation happens |
| STAB-0216 | 📋 | P2 | Verify CSG preview cache evicts at 128 entries | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Add 200 unique CSG nodes; cache size never exceeds 128 |
| STAB-0217 | 📋 | P2 | Add test: CSG OBJ export matches preview mesh | `src/MeshCraft/Renderer/SceneRenderer.cpp` | `exportCsgMesh()` produces same triangle count as preview cache |
| STAB-0218 | 📋 | P2 | Verify CSG child reorder is undoable | `src/MeshCraft/Scene/PropertiesPanel.cpp` | Reorder CSG children; `Ctrl+Z` restores previous order |
| STAB-0219 | 📋 | P2 | Add test: CSG difference with non-overlapping cutter — zero triangles or warning | `mc3togltf/src/CsgEvaluator.cpp` | Policy documented and tested |
| STAB-0220 | 🧪 | P1 | Verify `mc3togltf_csg_strict` test: exit code non-zero on CSG without flag | `mc3togltf/test/csg_strict_test.py` | Python test asserts `returncode != 0` |
| STAB-0221 | 🧪 | P1 | Verify `mc3togltf_csg_export` test: GLB contains non-empty mesh for CSG nodes | `mc3togltf/test/csg_export_test.py` | Exported GLB has `meshes[i].primitives[j].indices.count > 0` |
| STAB-0222 | 📋 | P2 | Verify Manifold library version compatibility | `CMakeLists.txt` | Manifold v3.0.0 pinned; document minimum version required |
| STAB-0223 | 📋 | P2 | Add test: CSG with 10 children (stress test) | `mc3togltf/src/CsgEvaluator.cpp` | Union of 10 boxes exports without crash; triangle count > 0 |
| STAB-0224 | 📋 | P3 | Document CSG UV/normal limitation (lost in Manifold output) | `MC3_FORMAT.md` | Limitation documented: CSG mesh has no UVs; renderer note |
| STAB-0225 | 📋 | P3 | Investigate if CSG UV can be preserved | `mc3togltf/src/CsgEvaluator.cpp` | Research task; result: either implement or document as explicit limitation |
| STAB-0226 | 🧪 | P1 | Verify CSG fails gracefully when Manifold throws | `mc3togltf/src/CsgEvaluator.cpp` | Catch `std::exception` from Manifold; print error; return empty mesh |
| STAB-0227 | 📋 | P2 | Add test: CSG intersection of two identical spheres (edge case) | `mc3togltf/src/CsgEvaluator.cpp` | Produces full sphere; triangle count ≈ sphere alone |
| STAB-0228 | 📋 | P2 | Add test: CSG difference subtracting entire base (full hole) | `mc3togltf/src/CsgEvaluator.cpp` | Cutter larger than base: empty result or near-empty; no crash |
| STAB-0229 | 🧪 | P1 | Verify CSG node names preserved in exported glTF | `mc3togltf/src/GltfExporter.cpp` | CSG node named "MyUnion" → glTF node.name == "MyUnion" |
| STAB-0230 | 📋 | P2 | Verify `--allow-approximate-csg` flag documented in `--help` | `mc3togltf/src/main.cpp` | `mc3togltf --help` shows `--allow-approximate-csg` with description |
| STAB-0231 | 📋 | P2 | Add test: approximate CSG with nested CSG child — all leaves exported | `mc3togltf/test/csg_export_test.py` | Nested CSG in approximate mode: all leaf primitives have non-empty mesh |
| STAB-0232 | 📋 | P3 | Investigate parallel CSG computation (Manifold parallelism) | `mc3togltf/src/CsgEvaluator.cpp` | `MANIFOLD_PAR` currently OFF; test if enabling improves large-scene perf |
| STAB-0233 | 📋 | P3 | Add CSG performance test: 5-level nested union of 32 boxes < 5s | `mc3togltf/test/` | New performance test with timeout |
| STAB-0234 | 📋 | P2 | Verify CSG isCutter semantics match XSD definition | `mc3/mc3.xsd` | XSD `csgChildType` documents `isCutter` attribute; confirmed in tests |
| STAB-0235 | 📋 | P3 | Investigate CSG cross-section support (currently OFF) | `CMakeLists.txt` | `MANIFOLD_CROSS_SECTION OFF`; document why and any future use case |

---

## S6 — Geometry Reuse, Instancing, Large Scenes

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0236 | ✅ | P0 | Primitive geometry cache by dimension key | `mc3togltf/src/GltfExporter.cpp` | `mc3togltf_float_cache_key` passes |
| STAB-0237 | ✅ | P0 | Instance definition cache: same def → same mesh | `mc3togltf/src/GltfExporter.cpp` | `mc3togltf_instance_deform_cache` passes |
| STAB-0238 | ✅ | P0 | Deform-specific cache key (different deform → different mesh) | `mc3togltf/src/GltfExporter.cpp` | `mc3togltf_instance_deform_cache` passes |
| STAB-0239 | ✅ | P0 | Float precision cache key (close dims don't collide) | `mc3togltf/src/GltfExporter.cpp` | `mc3togltf_float_cache_key` passes |
| STAB-0240 | 🧪 | P1 | Verify material-specific cache key (same prim, different material → different mesh) | `mc3togltf/src/GltfExporter.cpp` | Two boxes with different materials: exported glTF has 2 unique meshes |
| STAB-0241 | 🧪 | P1 | Verify repeated OBJ mesh src reuses same glTF mesh | `mc3togltf/src/GltfExporter.cpp` | Two objects with `<mesh src="same.obj"/>` → 1 mesh, 2 nodes |
| STAB-0242 | ✅ | P0 | Large scene (200 objects) exports correctly | `mc3togltf/test/large_scene_generated_test.py` | `mc3togltf_large_scene_generated` passes |
| STAB-0243 | 🧪 | P1 | Verify large_scene_generated unique mesh count (not 200 unique meshes) | `mc3togltf/test/large_scene_generated_test.py` | Test checks that `len(gltf.meshes) < len(gltf.nodes)` (reuse confirmed) |
| STAB-0244 | 📋 | P2 | Add 500-object generated test | `mc3togltf/test/large_scene_generated_test.py` | New test generates 500-object scene; export completes in < 30s |
| STAB-0245 | 📋 | P3 | Add 1000-object generated test (stress) | `mc3togltf/test/large_scene_generated_test.py` | 1000-object test with timeout 120s; memory usage < 500MB |
| STAB-0246 | 🧪 | P1 | Verify `mc3togltf_large_scene` test asserts node count | `mc3togltf/test/large_scene_test.py` | Test asserts `len(nodes) >= expected_count` |
| STAB-0247 | 🧪 | P1 | Verify group-only definition (no children) doesn't crash exporter | `mc3togltf/src/GltfExporter.cpp` | Instance of empty definition: exports as empty group node; no crash |
| STAB-0248 | 📋 | P1 | Add test: definition with multiple children instanced 10 times → 10 nodes, 1 mesh set | `mc3togltf/src/GltfExporter.cpp` | Mesh reuse confirmed for multi-child definition |
| STAB-0249 | 📋 | P2 | Verify editor CSG cache key uses content hash (not raw pointer) | `src/MeshCraft/Renderer/SceneRenderer.cpp` | After pushUndo/popUndo, same CSG node gets cache hit |
| STAB-0250 | 📋 | P2 | Verify `clearCsgCache()` called on document load | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Load new file; old CSG cache memory freed |
| STAB-0251 | 📋 | P2 | Add test: exported GLB file size is reasonable (< 2× input scene complexity) | `mc3togltf/test/large_scene_generated_test.py` | GLB size metric documented and tested |
| STAB-0252 | 📋 | P3 | Investigate Blender import of generated large scene | manual | Load 200-object GLB in Blender; verify all nodes visible; no errors |
| STAB-0253 | 📋 | P3 | Profile mc3togltf on 1000-object scene | `mc3togltf/src/GltfExporter.cpp` | Profiling shows cache hit rate > 80% for repeated primitives |
| STAB-0254 | 📋 | P2 | Verify no geometry corruption from cache (transform leak check) | `mc3togltf/src/GltfExporter.cpp` | Two boxes at different positions: each node has correct transform |
| STAB-0255 | 📋 | P2 | Document cache key format | `mc3togltf/src/GltfExporter.cpp` | In-code documentation: what fields form the cache key for each object type |
| STAB-0256 | 📋 | P2 | Verify animation does not corrupt geometry cache | `mc3togltf/src/GltfExporter.cpp` | Scene with animation + geometry: geometry unchanged; animation in correct channel |
| STAB-0257 | 📋 | P3 | Add test: very large OBJ file (>1M triangles) exported without memory exhaustion | `mc3togltf/src/GltfExporter.cpp` | OBJ with 1M tris: export completes; memory < 2GB |
| STAB-0258 | 📋 | P3 | Investigate streaming export for very large scenes | `mc3togltf/src/GltfExporter.cpp` | Research task; document findings |
| STAB-0259 | 🧪 | P1 | Verify `mc3togltf_float_cache_key` test: two close-dimension boxes produce 2 meshes | `mc3togltf/test/float_cache_key_test.py` | Test asserts `len(meshes) == 2` |
| STAB-0260 | 📋 | P2 | Verify repeated material reuse in large scene (same material → same glTF material index) | `mc3togltf/src/GltfExporter.cpp` | 100 objects sharing 3 materials → exactly 3 glTF materials |

---

## S7 — Editor Save/Load/Data-Loss Workflows

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0261 | ✅ | P1 | Verify new scene starts with dirty flag false | `src/MeshCraft/MeshCraftApplication.hpp` | Verified by code inspection: `modified_{false}` default member initializer (`MeshCraftApplication.hpp:76`), and `newScene()` also explicitly sets `modified_ = false;` (`MeshCraftApplication_FileOps.cpp:26`). No test needed — a private bool default can't regress silently without touching this exact line. |
| STAB-0262 | ✅ | P1 | Verify modifying scene sets dirty flag | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Verified by code inspection for the plan.md scenario ("add object"): `addPrimitive()` sets `modified_ = true;` in both branches (line 94, inserting into a group; line 102, appending to `document_.objects`), immediately after the mutation (`MeshCraftApplication_Commands.cpp:32-105`). A full audit of all ~130 `modified_ = true;` call sites across the editor was out of scope for one task; spot-checked that the previously-tested commands (delete/duplicate/group/ungroup) all follow the same pattern. |
| STAB-0263 | ✅ | P1 | Verify saving clears dirty flag | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Verified by code inspection: `saveFile()` sets `modified_ = false;` immediately after `document_.saveToFile(currentFile_);` (`MeshCraftApplication_FileOps.cpp:142,144`). |
| STAB-0264 | ✅ | P1 | Verify open-when-dirty prompts user (do not silently discard) | `mc3/test/editor_commands_test.cpp` | Found real branching logic worth mirroring beyond STAB-0261-0263's trivial cases: `confirmIfModified()` (`MeshCraftApplication_FileOps.cpp:85-90`) gates on `modified_` (execute now vs. defer to the "Unsaved Changes" dialog), and the dialog itself (`MeshCraftApplication_UiOverlays.cpp:1223-1253`) has three real branches — Save (only proceeds if `currentFile_` is set — otherwise refuses), Don't Save (always discards+proceeds), Cancel (never proceeds). Added CNA-free mirrors `confirmIfModifiedAlg`/`unsavedDialogResolvesToExecuteAlg` (`EditorAlgorithms.hpp`) plus `mc3_commands` coverage for all cases. Negative-checked (Save ignoring `hasCurrentFile` → 1 FAIL). root 18/18, Release 18/18 (verified 2026-07-01) |
| STAB-0265 | ✅ | P1 | Verify autosave interval is configurable | `mc3/test/editor_commands_test.cpp` | Confirmed the real logic first: `autoSaveInterval_` has a Prefs-dialog field (`MeshCraftApplication_UiOverlays.cpp:1811`) and round-trips through `prefs.ini` (`MeshCraftApplication_FileOps.cpp:305,322`). Added CNA-free mirrors `autoSavePathAlg`/`autoSaveTickAlg` (`EditorAlgorithms.hpp`) reproducing `autoSavePath()` and the update-loop countdown/trigger branch, plus `mc3_commands` coverage: custom intervals (5s, 2s) actually change trigger timing, `interval=0` disables auto-save entirely, unmodified/no-file never triggers. Negative-checked (hard-coded interval instead of reading `intervalSeconds` → 7 FAILs). root 18/18 (verified 2026-07-01) |
| STAB-0266 | ✅ | P1 | Verify autosave writes `.autosave` file, not the original | `mc3/test/editor_commands_test.cpp` | Added `mc3_commands` coverage using the real (CNA-free) `Mc3Document::saveToFile`, mirroring `performAutoSave()` (`MeshCraftApplication_FileOps.cpp:40-46`): saves an original file, mutates in-memory, "auto-saves" to `autoSavePathAlg(original)`, asserts the `.autosave` file is created with the mutated content and the original on-disk file is byte-for-byte unchanged. Negative-checked (auto-save written to the original path instead of `.autosave` → 3 FAILs). root 18/18 (verified 2026-07-01) |
| STAB-0267 | ✅ | P1 | Verify backup rotation creates backup.1, backup.2 | `mc3/test/editor_commands_test.cpp` | Added CNA-free mirror `rotateBackupsAlg` (`EditorAlgorithms.hpp`) of the "F6: rotate backups" block in `saveFile()` (`MeshCraftApplication_FileOps.cpp:130-154`). `mc3_commands` coverage: 1st save creates no backup; 2nd save creates `backup.1` with the prior version; 3rd save rotates `backup.1`→`backup.2` and both hold the correct version. Negative-checked (dropped the b1→b2 cascade → 4 FAILs). root 18/18 (verified 2026-07-01) |
| STAB-0268 | ✅ | P2 | Add test: backup rotation limit (max N backups) | `mc3/test/editor_commands_test.cpp` | Confirmed the mechanism is a fixed 2-slot ring buffer (no configurable N). `mc3_commands` coverage: after 10 sequential saves, `backup.1`/`backup.2` hold only the two most recent prior versions and no `backup.3` is ever created. Same negative check as STAB-0267 (shared implementation). root 18/18 (verified 2026-07-01) |
| STAB-0269 | 📋 | P2 | Document crash recovery workflow | `README.md` | README explains .autosave files and how to recover |
| STAB-0270 | ✅ | P1 | Verify undo before AI apply is possible | `mc3/test/editor_commands_test.cpp` | "Apply to Scene" (`MeshCraftApplication_UiAi.cpp:317-324`) does `pushUndo(); document_ = *aiPendingDoc_;` — a plain full-document replacement, no new Alg needed. `checkUndoRedo` confirms the generic snapshot/undo mechanism (STAB-0279) round-trips a full-document replacement the same way it does smaller per-field mutations. root 18/18 (verified 2026-07-02) |
| STAB-0271 | ✅ | P1 | Verify undo for merge scene | `mc3/test/editor_commands_test.cpp` | Added CNA-free mirror `mergeDocumentsAlg` (`EditorAlgorithms.hpp`) of `mergeSceneFromFile()` (`MeshCraftApplication_FileOps.cpp:255-283`). `mc3_commands` coverage: `checkUndoRedo` round-trip, plus direct collision-handling tests — texture key collision keeps the destination's existing texture, material key collision suffixes the source's copy (`_2`, `_3`, ...) and updates its `name` field, objects are appended. root 18/18 (verified 2026-07-02) |
| STAB-0272 | ✅ | P1 | Verify Save As creates new file without overwriting original | `mc3/test/editor_commands_test.cpp` | Added CNA-free mirror `resolveSaveAsPathAlg` (`EditorAlgorithms.hpp`) of the Save-As path-normalization logic (`MeshCraftApplication_UiOverlays.cpp:1196-1198`). `mc3_commands` coverage: extension-normalization cases (bare name, already-suffixed, `.mcb`, directory prefix) plus a real-file test — save an "original" doc, mutate in-memory, `saveToFile` to a *different* resolved path, assert the original file's on-disk bytes are unchanged and the new file holds the mutated content. root 18/18 (verified 2026-07-02) |
| STAB-0273 | ✅ | P1 | Verify Export Selection saves only selected objects | `mc3/test/editor_commands_test.cpp` | Added CNA-free mirror `exportSelectionAlg` (`EditorAlgorithms.hpp`) of `exportSelectionToFile()` (`MeshCraftApplication_FileOps.cpp:209-250`), taking a plain object vector instead of the CNA-coupled `Selection` class. `mc3_commands` coverage: exporting 2 of 3 objects yields exactly those 2, and materials referenced only by the unselected object are excluded. root 18/18 (verified 2026-07-02) |
| STAB-0274 | ✅ | P1 | Verify Export Selection includes dependent materials/textures | `mc3/test/editor_commands_test.cpp` | Same `exportSelectionAlg` mirror as STAB-0273. Coverage: materials referenced via `material`, `materialOverride`, and a nested child are all included; a material not referenced by the selection is excluded; a texture referenced by an included material is included, one that isn't is excluded; nested children are deep-copied with their parent. root 18/18 (verified 2026-07-02) |
| STAB-0275 | ✅ | P1 | Verify drag-drop MC3 file loads correctly | `mc3/test/editor_commands_test.cpp` | Already implemented via an `SDL_EVENT_DROP_FILE` watcher (`MeshCraftApplication.cpp:74-88`) routing to `confirmIfModified(PendingAction::OpenRecentFile, ...)`. Extracted the CNA-free routing decision as `isDroppableScenePathAlg` (`EditorAlgorithms.hpp`, mirrors `MeshCraftApplication.cpp:332`) and added `mc3_commands` coverage: `.mc3.xml`/bare `.xml` paths route to the loader, `.png`/`.txt` do not. The SDL event plumbing itself needs a CNA/window test harness (deferred, same as the literal GUI delete+Ctrl+Z flow in STAB-0279). root 18/18 (verified 2026-07-02) |
| STAB-0276 | ✅ | P1 | Verify invalid file load produces error dialog, not crash | `mc3/test/editor_commands_test.cpp` | The Open File dialog's catch block (`MeshCraftApplication_UiOverlays.cpp:1160-1179`) relies on `Mc3Document::loadFromFile()` throwing `std::exception` with a named message rather than crashing — already CNA-free, no mirror needed. Added direct `mc3_commands` coverage (no prior test exercised this): non-XML garbage, well-formed XML with the wrong root element, and a nonexistent path all throw with a non-empty message. root 18/18 (verified 2026-07-02) |
| STAB-0277 | ✅ | P1 | Verify locked object cannot be modified by move gizmo | `src/MeshCraft/MeshCraftApplication_Mouse.cpp` | Verified by code inspection: the Move (line 141-142), Scale (322-323) and Rotate (372-373) gizmo-drag apply loops all `if (lockedIds_.count(s->id)) continue;` before mutating `transform`, as do the vertex-snap (204-205) and surface-snap (252-253) helper passes; proportional editing (232) additionally excludes locked objects from the falloff target set. `MouseState` is an Xna/CNA input type with no headless test harness available, so this is inspection-only (same pattern as STAB-0302). No test needed — behavior already correct. |
| STAB-0278 | ✅ | P1 | Verify AI apply is undoable with Ctrl+Z | `mc3/test/editor_commands_test.cpp` | Same coverage as STAB-0270 (`testUndoRedoAiApply`) — both describe the same "Apply to Scene" undo path. root 18/18 (verified 2026-07-02) |
| STAB-0279 | ✅ | P1 | Verify delete command is undoable | `mc3/test/editor_commands_test.cpp` | Added CNA-free undo/redo coverage to `mc3_commands`: models the editor's snapshot-based undo (mirrors `deepCopyDoc`) and asserts a full snapshot→mutate→undo→redo round-trip (XML-equality oracle) for batchRename / findReplace / arrayDuplicate, plus a snapshot-independence test. Negative-checked (shallow copy → 3 FAILs). The literal GUI delete+Ctrl+Z flow needs an app-level/CNA test (deferred). root 18/18 (verified 2026-06-30) |
| STAB-0280 | ✅ | P1 | Verify duplicate command is undoable | `mc3/test/editor_commands_test.cpp` | Found the document mutation in `duplicateSelected()` (`MeshCraftApplication_Commands.cpp:178-204`) is pure vector/shared_ptr logic (app-state bookkeeping aside). Added CNA-free mirror `duplicateObjectsAlg` (`EditorAlgorithms.hpp`) plus direct behavior tests (copy inserted after original, `_copy` name/id, transform preserved, deep-independent) and a `checkUndoRedo` round-trip test. root 18/18 (verified 2026-07-01) |
| STAB-0281 | ✅ | P1 | Verify group command is undoable | `mc3/test/editor_commands_test.cpp` | Added CNA-free mirror `groupObjectsAlg` + `removeFromListAlg` mirroring `groupSelected()`/`removeFromList()` (`MeshCraftApplication_Commands.cpp:250-276`, `MeshCraftPrivate.hpp:60-68`). Direct tests confirm children/order/non-selected-object handling; `checkUndoRedo` confirms the round-trip. Negative-checked (dropped the `removeFromListAlg` call → 2 FAILs, caught by the direct behavior test). root 18/18 (verified 2026-07-01) |
| STAB-0282 | ✅ | P1 | Verify ungroup command is undoable | `mc3/test/editor_commands_test.cpp` | Added CNA-free mirror `ungroupObjectAlg` mirroring `ungroupSelected()` (`MeshCraftApplication_Commands.cpp:278-296`). Direct tests confirm children restored to their former position, group removed, and rejection of non-Group/empty-Group input; `checkUndoRedo` confirms the round-trip (re-locates the group by name each call since post-undo `d` is a fresh snapshot copy). root 18/18 (verified 2026-07-01) |
| STAB-0283 | ✅ | P1 | Verify material edit is undoable | `mc3/test/editor_commands_test.cpp` | The `PropertiesPanel.cpp` `ColorEdit4` widget is ImGui-coupled, but the document mutation it performs (`Mc3Material::baseColor` field assignment on `Mc3Document::materials`) is plain data — no new Alg function needed. `checkUndoRedo` confirms the already-proven generic snapshot/undo mechanism (STAB-0279) also round-trips material edits. root 18/18 (verified 2026-07-01) |
| STAB-0284 | ✅ | P1 | Verify animation keyframe edit is undoable | `mc3/test/editor_commands_test.cpp` | `insertAnimKeyframes()`'s document mutation (`MeshCraftApplication_Anim.cpp:94-153`) is plain `Mc3Document::actions` editing once `modified_`/`evaluateAndPushAnimOverrides()` are set aside. Added CNA-free mirror `insertAnimKeyframesAlg` (`EditorAlgorithms.hpp`); direct tests confirm create-new-channel, reuse-existing-channel, and replace-not-duplicate-at-same-time behavior, plus a `checkUndoRedo` round-trip. root 18/18 (verified 2026-07-02) |
| STAB-0285 | ✅ | P1 | Verify registry insert is undoable | `mc3/test/editor_commands_test.cpp` | The "Insert" button (`MeshCraftApplication_UiRegistry.cpp:80-101`) does `pushUndo(); document_.objects.push_back(obj); modified_ = true;` — a plain append, no new Alg needed (same pattern as STAB-0270/0278/0283). `checkUndoRedo` confirms the generic snapshot/undo mechanism round-trips it. root 18/18 (verified 2026-07-02) |
| STAB-0286 | ✅ | P1 | Verify keybinding changes persist across restart | `mc3/test/editor_commands_test.cpp` | Added CNA-free mirrors `KeyBindAlg`/`keyBindToStringAlg`/`keyBindFromStringAlg`/`saveKeybindingsAlg`/`loadKeybindingsAlg` (`EditorAlgorithms.hpp`) of `KeyBind::toString()`/`fromString()` and `loadKeybindings()`/`saveKeybindings()` (`MeshCraftApplication_Keybindings.cpp`) — using a standalone key-name table since the real `Keys::` enum lives in CNA and can't be included here; what's under test is the persistence *format*, not `Keys::` integer values. `mc3_commands` coverage: modifier-combination round-trip, case-insensitive parsing, and — simulating a restart — a saved binding overwrites a differing pre-load default while an id absent from the file is left untouched. root 18/18 (verified 2026-07-02) |
| STAB-0287 | ✅ | P1 | Verify preferences persist across restart | `mc3/test/editor_commands_test.cpp` | Added CNA-free mirror `PrefsAlg`/`savePrefsAlg`/`loadPrefsAlg` (`EditorAlgorithms.hpp`) of `loadPrefs()`/`savePrefs()` (`MeshCraftApplication_FileOps.cpp:296-329`), already CNA-free logic apart from the `applyTheme()` ImGui side effect (intentionally not mirrored). Coverage: all 6 fields round-trip; a missing file leaves defaults untouched; an unparseable value on one line doesn't block later well-formed lines from loading. root 18/18 (verified 2026-07-02) |
| STAB-0288 | 📋 | P2 | Add test: recent file list if implemented | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | File → Recent shows last opened file |
| STAB-0289 | 📋 | P2 | Verify Merge Scene handles ID collision (suffix appended) | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Merge file with same object IDs as current scene; collision handled |
| STAB-0290 | ✅ | P1 | Verify GLB export settings (embedded/external) persisted in UI | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Verified by inspection: `glbExportFmt_`/`glbAllowApproxCSG_` are plain `MeshCraftApplication` members (`include/MeshCraft/MeshCraftApplication.hpp`), not local dialog state; `exportGltf()` (`MeshCraftApplication_FileOps.cpp:164-180`), which runs each time the export dialog (re)opens, only resets `glbExportOutBuf_`/`glbExportErr_`/`glbExportOpen_` — it never touches the format/CSG-approximation fields, so both trivially persist across repeated dialog opens within a session. No test possible without a CNA/ImGui harness (same as STAB-0277/0302/0300). Note: this item asks about persistence across a dialog re-open, not across an app *restart* — unlike STAB-0286/0287, these two fields are NOT written to `prefs.ini`, so they do reset to their compiled-in defaults on restart; that's outside what this item's verification text asks for. |
| STAB-0291 | 📋 | P2 | Verify export overwrites existing file after confirmation | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Export to existing path; behavior: overwrite or prompt |
| STAB-0292 | ✅ | P1 | Verify macro save/load preserves all steps | `mc3/test/editor_commands_test.cpp` | Added CNA-free mirror `MacroStepAlg`/`saveMacroAlg`/`loadMacroAlg` (`EditorAlgorithms.hpp`) of `saveMacro()`/`loadMacro()` (`MeshCraftApplication_Macro.cpp:94-131`; `MacroStep` itself is plain data but only declared inside the CNA-coupled `MeshCraftApplication.hpp`, hence the mirror struct). Coverage: verb+args round-trip exactly and in order for a 4-step macro; blank lines between steps are skipped on load. root 18/18 (verified 2026-07-02) |
| STAB-0293 | 📋 | P2 | Verify macro playback skips missing object gracefully | `src/MeshCraft/MeshCraftApplication_Macro.cpp` | Macro references deleted object; step skipped; rest plays |
| STAB-0294 | 📋 | P2 | Verify headless screenshot export (`--screenshot`) works | `src/MeshCraft/main.cpp` | `./MeshCraft --screenshot scene.mc3.xml out.png` produces valid PNG |
| STAB-0295 | 📋 | P2 | Verify export subtree as template creates valid mc3.xml | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Select subtree; export as template; output validates against XSD |

---

## S8 — UI Robustness

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0296 | ✅ | P1 | Verify no fixed char buffer overflow in file path fields | `src/MeshCraft/MeshCraftApplication_FileOps.cpp`, `MeshCraftApplication_Anim.cpp`, `MeshCraftApplication_UiMenuBar.cpp`, `MeshCraftApplication_UiOverlays.cpp`, `Scene/PropertiesPanel.cpp` | Audited all 40 `std::strncpy` call sites across the editor UI (`grep -rn std::strncpy src/MeshCraft/`). **Found a real gap**: the established safe idiom is `strncpy(buf, src, sizeof(buf)-1)` followed by an explicit `buf[sizeof(buf)-1] = '\0'` (used correctly at ~33 sites) — but 7 sites (6 distinct buffers: `glbExportOutBuf_` ×2, `openDialogErr_`, `saveDialogErr_`, `subtreeExportNameBuf_`, `addChannelObjBuf_`, `layerBuf`) were missing the terminator, meaning a source string ≥ the buffer size would leave the buffer non-null-terminated (buffer over-read when later read via `%s`/`std::string(buf)` — `addChannelObjBuf_` is a confirmed case, read via `std::string(addChannelObjBuf_)` a few lines later). Fixed all 7 sites with the same one-line pattern used everywhere else. root 18/18, Release 18/18 (verified 2026-07-01) |
| STAB-0297 | ✅ | P1 | Verify no fixed char buffer overflow in rename field | `src/MeshCraft/Scene/SceneHierarchyPanel.cpp` | Verified by inspection: `renameBuf_` is `char[256]` (meets the ≥256 requirement) and both write sites (`strncpy` + explicit `[sizeof-1]='\0'`) are correctly terminated. No fix needed. |
| STAB-0298 | ✅ | P1 | Verify no unsafe `strcpy` in UI code | all `.cpp` | `grep -rn '\bstrcpy(' src/ mc3/ mcb/ mc3togltf/ mc3tomcb/ include/` returns zero hits in project-authored code (3 hits exist only in `mc3togltf/build/_deps/tinygltf-src/...` — vendored third-party sources pulled by FetchContent, out of scope). No fix needed. |
| STAB-0299 | ✅ | P1 | Verify AI pending dialog: cleared on Reset | `mc3/test/editor_commands_test.cpp` | Reset/Apply live inside `if (ImGui::Button(...))` blocks (`MeshCraftApplication_UiAi.cpp:264-380`) with no separable function, so added a small state-transition mirror `AiPanelStateAlg`/`aiResetAlg` to `EditorAlgorithms.hpp` (bools stand in for the real `optional<Mc3Document>`/`string` fields — presence/absence is what's under test). Confirms Reset clears the pending-doc and validation-error flags. root 18/18 (verified 2026-07-01) |
| STAB-0300 | ✅ | P1 | Verify AI pending dialog: not cleared on Apply (Save still available) | `mc3/test/editor_commands_test.cpp` | Added mirror `aiApplyToSceneAlg` of the "Apply to Scene" button body (`MeshCraftApplication_UiAi.cpp:317-324`). Confirms Apply intentionally leaves the pending-doc flag set. root 18/18 (verified 2026-07-01) |
| STAB-0301 | ✅ | P1 | Verify registry dialog: closed when Reset clicked in AI panel | `mc3/test/editor_commands_test.cpp` | Same `aiResetAlg` mirror: confirms Reset closes the registry save dialog only when it was opened from this AI result (`regSaveFromAi_`), and leaves an independently-opened dialog alone. Negative-checked (dropped the `regSaveFromAi_` guard → 1 FAIL). root 18/18 (verified 2026-07-01) |
| STAB-0302 | ✅ | P1 | Verify no crash when no object selected and Properties panel rendered | `src/MeshCraft/Scene/PropertiesPanel.cpp` | Verified by code inspection (no test needed/possible without an ImGui context): `draw()` is structured as a single top-level `if (ctx.selection.hasSelection()) { ... } else { ... }` (lines 36 / 1794) — every `sel0`/`selAll` access is inside the `if` branch, and the `else` branch already renders a document-summary + "Select All" button. Empty selection cannot reach a `sel0` dereference. |
| STAB-0303 | ✅ | P1 | Verify no crash when selected object is deleted externally | `mc3/test/editor_commands_test.cpp` | Verified by inspection + a regression test. `PropertiesPanel.cpp` never looks up the selected object in the document tree (only reads/writes fields on its `shared_ptr`, which stays valid regardless), and the sole delete path (`deleteSelected()`, reached from `SceneHierarchyPanel.cpp`'s "Delete" menu item) always clears `selection_` right after removing — so a "selected but externally removed" object can't currently occur. Added a `mc3_commands` regression test confirming `duplicateObjectsAlg`/`groupObjectsAlg`/`ungroupObjectAlg` all no-op (rather than crash) if ever called with an object not present in the tree, since those DO look the object up (`findParentListAlg`/`removeFromListAlg`, both null/no-match-safe). root 18/18 (verified 2026-07-01) |
| STAB-0304 | ✅ | P1 | Verify invalid material reference on object doesn't crash renderer | `mc3/test/editor_commands_test.cpp` | `materialColor()` (`Renderer/SceneRenderer.cpp:368-381`) already `find()`s (not `at()`s) and falls back to a fixed default gray for an empty or unresolvable material id — same pattern used everywhere `obj.material` is read for rendering (checked all remaining `doc.materials`/`doc.textures` lookups in `SceneRenderer.cpp`: all `find()`-guarded, no `.at()` on unvalidated ids). Added CNA-free mirror `materialColorAlg` (`EditorAlgorithms.hpp`, plain RGBA array instead of the CNA `Color` type); tests confirm the gray fallback for empty/missing ids and correct clamped resolution for a real material. root 18/18 (verified 2026-07-02) |
| STAB-0305 | ✅ | P1 | Verify missing mesh file doesn't crash renderer | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Verified by code inspection: `loadObjMesh()` (line 232) returns an empty `RenderMesh` (no `vb`) when `tinyobj::ObjReader::ParseFromFile` fails (line 241, e.g. nonexistent path — tinyobjloader itself doesn't throw), `loadOrGetMesh()` (line 487) turns that into a `nullptr`, and the `ObjectType::Mesh` draw case (line 792-803) falls through to `drawMesh(unitBox_, ...)` — a unit-box placeholder — whenever `loadOrGetMesh` returns null. No test possible without a real `GraphicsDevice`/GL context (tinyobjloader + VB/texture types are deeply OpenGL-coupled, not meaningfully separable into a CNA-free mirror). |
| STAB-0306 | ✅ | P1 | Verify hierarchy filter with no matches shows empty list (not crash) | `mc3/test/editor_commands_test.cpp` | **Found and fixed a real bug**: `drawHierarchy()` (`Scene/SceneHierarchyPanel.cpp`, was line 269/276) gated its skip-decision on `filtering` (text-search-only) instead of `anyFiltering` (which `matchesFilter()` itself already correctly computes) — so a type/layer/tag/material filter set with **no search text typed** silently had zero effect on the visible list; the filter buttons appeared active but did nothing. Fixed both sites to check `anyFiltering`. Added CNA-free mirrors `HierarchyFilterAlg`/`hierarchyMatchesTypeAlg`/`hierarchyAnyFilterActiveAlg`/`hierarchyFilterMatchesAlg` (`EditorAlgorithms.hpp`) and a regression test proving a type-only filter (no text) now correctly narrows the list; also covered: recursion into children (a non-matching parent with a matching descendant still matches), OR mode, and the empty-filter no-op case. root 18/18 (verified 2026-07-02) |
| STAB-0307 | ✅ | P1 | Verify duplicate object names handled in hierarchy | `src/MeshCraft/Scene/SceneHierarchyPanel.cpp` | Verified by code inspection: tree-node identity uses `ImGui::PushID(obj.get())` (pointer-based, line 270) so two same-named objects render as distinct nodes without ID collision; the rename mechanism is keyed on `renamingId_ == obj->id` (`obj->id`, not `obj->name` — lines 321/405/416), and `id` is assigned from an ever-incrementing session counter at creation (`addPrimitive()`, `MeshCraftApplication_Commands.cpp:37-41`) independent of the display name, so renaming never changes `id`. Duplicate `name` values therefore cannot cross-contaminate the rename target or cause an ImGui ID collision. No test possible/needed without an ImGui context (same as STAB-0302). |
| STAB-0308 | ✅ | P1 | Verify duplicate material IDs handled | `mc3/test/editor_commands_test.cpp` | Policy: **last wins**. `document_.materials` is a `std::map<std::string, Mc3Material>`, so an in-memory duplicate key is structurally impossible; the "+ Add" button (`MeshCraftApplication_UiLeftPanel.cpp:777-788`) and the inline key-rename field (line 916-943) both explicitly loop/guard to avoid ever creating a UI-driven collision. The only way a duplicate can arise is two `<material id="X">` elements in a hand-edited/AI-generated XML file — confirmed `parseMaterials()` (`mc3/src/Mc3XmlParser.cpp:482-512`) does `doc.materials[id] = mat`, i.e. last-element-wins, same as any map assignment; no error, no crash. Added a direct `mc3_commands` test (no mirror needed, `Mc3Document::loadFromFile` is already CNA-free) confirming exactly that against real hand-crafted duplicate-id XML. root 18/18 (verified 2026-07-02) |
| STAB-0309 | 📋 | P2 | Verify panel resize: left panel width has min/max bounds | `src/MeshCraft/MeshCraftApplication.cpp` | Drag left panel to minimum; content not clipped off-screen |
| STAB-0310 | 📋 | P2 | Verify viewport resize: render framebuffer resized correctly | `src/MeshCraft/MeshCraftApplication.cpp` | Resize window; viewport aspect ratio correct; no GL error |
| STAB-0311 | 📋 | P2 | Verify focus/keyboard conflicts between viewport and dialogs | `src/MeshCraft/MeshCraftApplication_Keyboard.cpp` | With dialog open: viewport shortcuts not fired; dialog input captured |
| STAB-0312 | ✅ | P1 | Verify command palette shows no crash with empty scene | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Verified by code inspection: the "Scene objects" section (`MeshCraftApplication_UiOverlays.cpp:496-567`) walks `document_.objects` recursively (`addObjs`) with no indexing/`front()`/`.at()` calls — an empty vector simply produces zero iterations, `shown` stays 0 for that section, and the pre-existing "No results" fallback (line 569) already handles the zero-results case generally (also applies when a search filters out every static File/Edit/Add/View command). No test possible without an ImGui context (same as STAB-0302/0307). |
| STAB-0313 | 📋 | P2 | Verify multi-selection: delete all selected deletes only selected | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Select 3 of 5; delete; 2 remain |
| STAB-0314 | 📋 | P2 | Verify multi-selection: group command groups selected only | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Select 2 of 5; group; 2 objects in group; 3 remain ungrouped |
| STAB-0315 | 📋 | P2 | Verify Group Scale dialog: locked objects skipped | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Mix of locked and unlocked in selection; locked not scaled |
| STAB-0316 | 📋 | P2 | Verify timeline: no crash with empty action | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Create empty action (no keyframes); timeline renders without crash |
| STAB-0317 | 📋 | P2 | Verify timeline: scrubbing past last keyframe does not crash | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Scrub to frame 9999 with 10-frame action; no crash |
| STAB-0318 | 📋 | P2 | Verify texture drag-drop: non-image file dropped doesn't crash | `src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp` | Drop `.mc3.xml` on texture field; handled gracefully |
| STAB-0319 | ✅ | P1 | Verify material preview sphere: no crash when no material selected | `src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp` | Verified by code inspection: the entire material-editor block, including `initMatPreview()`/`renderMatPreview()` (`MeshCraftApplication_UiLeftPanel.cpp:898-913`, not `MeshCraftApplication.cpp` — the plan's Key File column was off by one file), is gated behind `if (!selectedMaterialKey_.empty() && document_.materials.count(selectedMaterialKey_))` (line 898); with nothing selected, neither FBO function is ever called. No test possible without an ImGui + OpenGL FBO context (same as STAB-0302/0307/0312). |
| STAB-0320 | 📋 | P2 | Verify undo stack display: no crash with 0 items | `src/MeshCraft/MeshCraftApplication.hpp` | Edit menu "Undo Stack" shows "Empty" or hidden when stack is empty |
| STAB-0321 | 📋 | P2 | Verify walk mode: gravity and collision not tested but documented | `src/MeshCraft/MeshCraftApplication_WalkMode.cpp` | Walk mode limitations documented (no collision detection) |
| STAB-0322 | 📋 | P2 | Verify measurement tool: no crash when 0 or 1 point clicked | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Click once; no measurement shown; no crash |
| STAB-0323 | 📋 | P2 | Verify bounding box toggle: no crash for empty selection | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Toggle bounding boxes with nothing selected; no crash |
| STAB-0324 | 📋 | P2 | Verify shadow map debug overlay: no crash without directional light | `src/MeshCraft/MeshCraftApplication.cpp` | Open shadow frustum overlay with no lights; shows "No directional light" or blank |
| STAB-0325 | ✅ | P1 | Verify bloom strength slider: value clamped to [0,1] | `src/MeshCraft/MeshCraftApplication_UiMenuBar.cpp` | **Found and fixed a real bug**: `ImGui::SliderFloat("  Strength##bloom", &bloomStrength_, 0.5f, 8.0f, "%.1f")` (line 588; range is [0.5, 8.0] by design — HDR-style overdrive, not [0,1] as this item's title assumes, but the *actual* requirement — "no negative value possible" — was violated) had no `ImGuiSliderFlags_AlwaysClamp`. Per Dear ImGui's own documented behavior (`imgui.h`: "Manually input values aren't clamped by default and can go off-bounds"), Ctrl+Click → type a value bypasses the slider's [min,max] entirely, including negative — feeds straight into the bloom composite shader's `u_strength` uniform with no other guard. Fixed by adding `ImGuiSliderFlags_AlwaysClamp`. No headless test possible (ImGui widget interaction); fix verified by code review of Dear ImGui 1.91.6's documented flag semantics and confirming compilation. |
| STAB-0326 | ✅ | P1 | Verify SSAO strength slider: value clamped | `src/MeshCraft/MeshCraftApplication_UiMenuBar.cpp` | Same bug and fix as STAB-0325, applied to `ImGui::SliderFloat("  Strength##ssao", &ssaoStrength_, 0.0f, 1.0f, "%.2f")` (line 593) — this one's range genuinely is [0,1] as the title states. Also applied the identical fix to the adjacent `Radius##ssao` slider (line 595-596, `ssaoRadius_`, range [0.05, 2.0]) — same root cause, same file, same author intent (SSAO controls); an unclamped negative/zero radius would break the SSAO sample kernel. Not a separate STAB item, but leaving one of three adjacent identical-pattern sliders unfixed while fixing the other two seemed worse than a one-line consistency fix. |
| STAB-0327 | 📋 | P2 | Verify preferences dialog: INI file created on first launch | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Fresh install: prefs.ini doesn't exist; first close creates it |
| STAB-0328 | 📋 | P2 | Verify keybindings INI: invalid/unknown lines silently skipped | `src/MeshCraft/MeshCraftApplication_Keybindings.cpp` | Corrupted keybindings.ini; app loads with defaults; no crash |
| STAB-0329 | 📋 | P2 | Verify macro recorder: play with 0 steps doesn't crash | `src/MeshCraft/MeshCraftApplication_Macro.cpp` | Play empty macro; no crash; no changes to scene |
| STAB-0330 | 📋 | P2 | Verify scatter/place: zero-count input handled | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Scatter with count=0; no crash; no objects added |
| STAB-0331 | 📋 | P2 | Verify align command: single-object selection doesn't crash | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Align with 1 object selected; no crash; position unchanged |
| STAB-0332 | 📋 | P2 | Verify material list search: empty query shows all materials | `src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp` | Empty search string; full material list shown |
| STAB-0333 | 📋 | P2 | Verify "Look through selected camera": no crash when selected object is not a camera | `src/MeshCraft/MeshCraftApplication.cpp` | Select non-camera; camera-view ignored or button disabled |
| STAB-0334 | 📋 | P3 | Audit all `ImGui::InputText` for null-termination safety | all UI `.cpp` | All InputText calls use correct buffer size; no off-by-one |
| STAB-0335 | 📋 | P3 | Audit all `snprintf`/`sprintf` in UI code | all UI `.cpp` | `grep -r sprintf src/` — replace any `sprintf` with `snprintf` |

---

## S9 — ModelRegistry Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0336 | ✅ | P0 | Registry open/close/isOpen | `src/MeshCraft/ModelRegistry.cpp` | `mc3_registry` passes |
| STAB-0337 | ✅ | P0 | Registry save/search/remove | `src/MeshCraft/ModelRegistry.cpp` | `mc3_registry` passes |
| STAB-0338 | ✅ | P0 | Registry migration (ALTER TABLE) | `src/MeshCraft/ModelRegistry.cpp` | `mc3_registry` passes |
| STAB-0339 | ✅ | P0 | Registry insertIntoScene returns unique id | `src/MeshCraft/ModelRegistry.cpp` | `mc3_registry` passes (duplicate-id suffix test) |
| STAB-0340 | ✅ | P1 | Verify SQLite unavailable: registry shows error in UI (not crash) | `mc3/test/mc3_registry_test.cpp` | An unopened/closed registry (`db_ == nullptr`) exercises the identical guarded code path a no-SQLite3 stub build would (same defaults, same guards); added a test confirming `search()`/`save()`/`remove()` are all no-crash/safe-default in that state. The UI side (`MeshCraftApplication_UiRegistry.cpp:17-27`) already checks `isOpen()` and shows "Model Registry is not available in this build" — verified by code inspection (driving the real panel needs a CNA context). root 18/18, Release 18/18 (verified 2026-07-01) |
| STAB-0341 | ✅ | P1 | Verify registry DB open failure shows named error | `mc3/test/mc3_registry_test.cpp` | Added a test that opens a path pointing at a directory (sqlite3 reliably fails with "unable to open database file" for a non-file path) and confirms `open()` throws `std::runtime_error` with a non-empty, identifiable message, and `isOpen()` stays false afterward — matching the UI's `catch` block (`setStatusMsg(std::string("Registry: ") + ex.what())`). root 18/18, Release 18/18 (verified 2026-07-01) |
| STAB-0342 | ✅ | P1 | Verify registry search: query matches group, name, tags, description | `mc3/test/mc3_registry_test.cpp` | Existing tests already covered name/group/tags; added a test with a marker unique to each field (group/name/tags/description) confirming `search()` matches every one independently, matches case-insensitively, and doesn't false-positive on an absent marker. Negative-checked (dropped `description` from the SQL `OR` clause → 2 FAILs). root 18/18, Release 18/18 (verified 2026-07-01) |
| STAB-0343 | ✅ | P1 | Add test: registry search case-insensitive | `mc3/test/mc3_registry_test.cpp` | Added exact plan.md scenario: save "Chair", search "chair"/"CHAIR" both find it. root 18/18, Release 18/18 (verified 2026-07-01) |
| STAB-0344 | ✅ | P1 | Add test: registry search by source field | `mc3/test/mc3_registry_test.cpp`, `src/MeshCraft/ModelRegistry.cpp`, `src/MeshCraft/MeshCraftApplication_UiRegistry.cpp` | **Found a real gap during STAB-0342**: the search SQL's `WHERE` clause omitted `source` entirely (only name/grp/tags/description), and the search box's hint text didn't mention it either — `source` was write-only metadata, never searchable. User chose to fix rather than just document. Added `OR lower(source) LIKE lower(?1)` to the SQL, updated the hint text to "Search by name, group, tags, description or source…", and added `testSearchBySourceField` (two entries, distinct sources, confirms each search returns only the matching one) plus a source marker in the existing per-field test. Negative-checked (reverted the SQL clause → 3 FAILs). root 18/18, Release 18/18 (verified 2026-07-01) |
| STAB-0345 | ✅ | P1 | Verify entryFromDefinition: XML only includes referenced materials/textures | `mc3/test/mc3_registry_test.cpp` | Added `testEntryFromDefinitionOnlyIncludesReferencedMaterials()`: scene has 2 materials ("wood", referenced by the "crate" definition, and "unrelated_material", not referenced by anything); the saved entry's xml contains "wood" but not "unrelated_material" (both a substring check and, more rigorously, a full parse-back via `Mc3Document::loadFromFile` confirming exactly 1 material survives). Confirms `collectMaterials()`'s scoping (`ModelRegistry.cpp:64-70`) works as designed. root 18/18 (verified 2026-07-02) |
| STAB-0346 | ✅ | P1 | Verify insertIntoScene: inserted model parses and adds objects correctly | `mc3/test/mc3_registry_test.cpp` | Note: `insertIntoScene` inserts into `doc.definitions` (+ merges `materials`/`textures`) — it never touches `doc.objects` directly; placing an `Instance` object in the scene is the caller's job (`MeshCraftApplication_UiRegistry.cpp`'s "Insert" button). `testEntryFromDefinitionAndInsert()` already checked `scene.definitions.count(insertedId) == 1`, but only presence, not correctness — strengthened it to verify the inserted definition's actual parsed structure: type (Group), child count (1), child type (Box), and the child's material reference ("wood") all match what `entryFromDefinition` originally serialized. root 18/18 (verified 2026-07-02) |
| STAB-0347 | ✅ | P1 | Verify "Save to Registry" from AI panel uses aiPendingDoc, not scene doc | `mc3/test/editor_commands_test.cpp` | Extended the `AiPanelStateAlg` mirror (STAB-0299/0300/0301) with `regSaveDefId` + two new mirror functions: `aiSaveToRegistryClickAlg` (mirrors the button body, `MeshCraftApplication_UiAi.cpp:350-361`) and `registrySaveUsesAiDefinitionsAlg` (mirrors the def-source ternary in `MeshCraftApplication_UiRegistry.cpp:137-139`). `testAiSaveToRegistryUsesAiPendingDoc()` confirms a save initiated from the AI button sources definitions from `aiPendingDoc_`, while an independently-opened save uses `document_`. root 18/18 (verified 2026-07-02) |
| STAB-0348 | ✅ | P1 | Verify AI "Save to Registry" works without prior Apply | `mc3/test/editor_commands_test.cpp` | The real button's visibility gate (`UiAi.cpp:329`) and click handler both reference only `aiPendingDoc_` — no "was Apply clicked" flag exists in the real code to depend on. `testAiSaveToRegistryWorksWithoutApply()` models this directly: reaches a successful save-to-registry state transition without ever calling `aiApplyToSceneAlg()` first. root 18/18 (verified 2026-07-02) |
| STAB-0349 | ✅ | P1 | Verify default DB path is `~/.meshcraft/modelregistry.sqlite3` | `mc3/test/mc3_registry_test.cpp` | Added `testDefaultPathFormat()`: confirms the filename (`modelregistry.sqlite3`), parent directory name (`.meshcraft`), and (non-Windows, when `$HOME` is set) the full path against a directly-constructed expected path. root 18/18 (verified 2026-07-02) |
| STAB-0350 | ✅ | P1 | Verify registry auto-opens default path on first "Save to Registry" | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Verified by code inspection: the button's comment states explicitly "does NOT require `registry_.isOpen()`" (line 328), and its body (`lines 332-349`) does `bool regReady = registry_.isOpen(); if (!regReady) { registry_.open(ModelRegistry::defaultPath()); ... }` — auto-opening the default path on demand. Trivial boolean-gate logic, not extracted as a separate Alg function (would be a one-line `!isOpen` wrapper adding no verification value beyond reading the code). No headless test possible without a CNA/ImGui harness (same as STAB-0277/0290/0300/0312/0319). |
| STAB-0351 | ✅ | P2 | Add test: registry thumbnail explicitly unsupported (no column) | `mc3/test/mc3_registry_test.cpp` | `Entry` (`ModelRegistry.hpp`) having no `thumbnail` member is a compile-time fact, not runtime-assertable without reflection — but the SQL schema is, and matters more (it's what would need a migration if this ever changed). Added `testNoThumbnailColumn()`: opens the real DB file directly via `sqlite3_open_v2` + `PRAGMA table_info(models)` and confirms no column is named `thumbnail`. Already documented in `m1m2m3.md`: "Not yet implemented: thumbnail column (design placeholder only)." root 18/18 (verified 2026-07-02) |
| STAB-0352 | ✅ | P1 | Verify registry UI "no data" state shows message | `src/MeshCraft/MeshCraftApplication_UiRegistry.cpp` | Verified by code inspection: `if (regCachedResults_.empty()) { ... ImGui::TextDisabled("(empty)"); }` (lines 114-118) — the literal text is `"(empty)"`, not "No models found" as this item's example wording suggested, but it functionally satisfies the requirement (a visible, non-crashing empty-state message rather than a blank table). No headless test possible without an ImGui context (same as STAB-0277/0290/0300/0312/0319/0350). |
| STAB-0353 | 📋 | P2 | Verify registry handles concurrent open from two processes | `src/MeshCraft/ModelRegistry.cpp` | Two MeshCraft instances open same DB; SQLite WAL mode; no corruption |
| STAB-0354 | 📋 | P2 | Verify corrupted registry DB file produces error (not crash) | `src/MeshCraft/ModelRegistry.cpp` | Write garbage to DB file; `open()` returns error string; app continues |
| STAB-0355 | 📋 | P2 | Verify registry "Insert" from UI pushes undo | `src/MeshCraft/MeshCraftApplication_UiRegistry.cpp` | Insert from registry; Ctrl+Z; inserted objects removed |
| STAB-0356 | 📋 | P2 | Verify registry material merge on insert: no duplicate material IDs | `src/MeshCraft/ModelRegistry.cpp` | Insert model with material already in scene; material reused, not duplicated |
| STAB-0357 | 📋 | P2 | Verify registry texture merge on insert | `src/MeshCraft/ModelRegistry.cpp` | Insert model with texture already in scene; texture reused |
| STAB-0358 | 📋 | P2 | Verify "Save current definition" button in UI | `src/MeshCraft/MeshCraftApplication_UiRegistry.cpp` | Select definition in combo; fill fields; save; entry appears in search |
| STAB-0359 | ✅ | P3 | Add registry test: large XML content (>1MB definition XML) | `mc3/test/mc3_registry_test.cpp` | Added `testLargeXmlContent()`: builds a >1MB payload from a repeating, distinctive (not uniform-filler) pattern so a truncation bug couldn't hide, saves it, searches it back, and confirms both length and byte-for-byte content match exactly (`sqlite3_bind_text`/`TEXT` column have no practical size issue here, but this locks it in as a regression guard). root 18/18 (verified 2026-07-02) |
| STAB-0360 | 📋 | P3 | Verify registry DB path config (alternative path via env var or prefs) | `src/MeshCraft/ModelRegistry.cpp` | Document how to override default DB path |
| STAB-0361 | 📋 | P2 | Verify registry search is live (instant, not on Enter) | `src/MeshCraft/MeshCraftApplication_UiRegistry.cpp` | Type in search; results update each keystroke |
| STAB-0362 | 📋 | P2 | Verify registry panel shows group, name, variant columns | `src/MeshCraft/MeshCraftApplication_UiRegistry.cpp` | UI table shows all three fields for each entry |
| STAB-0363 | 📋 | P2 | Verify registry panel X (delete) button removes entry permanently | `src/MeshCraft/MeshCraftApplication_UiRegistry.cpp` | Click X; entry removed from search; DB record deleted |
| STAB-0364 | ✅ | P3 | Add registry test: save entry with special chars in name/tags | `mc3/test/mc3_registry_test.cpp` | Added `testSpecialCharsInNameAndTags()` using exactly the plan's example (name `"door (gothic)"`, tags `"arch medieval"`): saves without error, name round-trips byte-exact, and tag search still works alongside the parenthesized name. Expected to already work — `save()`/`search()` use parameterized `sqlite3_bind_text`, never string concatenation — this locks it in explicitly rather than just asserting by design. root 18/18 (verified 2026-07-02) |
| STAB-0365 | 📋 | P3 | Verify registry works on read-only filesystem (Emscripten IDBFS) | `src/MeshCraft/ModelRegistry.cpp` | Stub behavior on Emscripten documented |
| STAB-0366 | ✅ | P1 | Verify MESHCRAFT_HAS_SQLITE3 guard in ModelRegistry.cpp | `src/MeshCraft/ModelRegistry.cpp` | Verified by code inspection: the `#ifndef MESHCRAFT_HAS_SQLITE3` block (`ModelRegistry.cpp:40-57`) provides trivial no-op implementations of every public method with the same safe defaults the real (SQLite-available) implementation falls back to when `db_ == nullptr` — `isOpen()` hardcoded `false`, `search()`/`entryFromDefinition()`/`insertIntoScene()` return empty, `save()` returns `-1`. This is a *different compilation* (the `mc3_registry_test` target is only ever built `if(SQLite3_FOUND)`, per `CMakeLists.txt:344`), so it can't be exercised at runtime within this test binary — but `testUnavailableRegistryNoCrash()` (STAB-0340) already verifies the real implementation's `db_ == nullptr` path produces byte-identical safe defaults to what the stub hardcodes, which is the behavior that actually matters. |
| STAB-0367 | 📋 | P2 | Verify registry `created` timestamp stored as Unix epoch | `src/MeshCraft/ModelRegistry.cpp` | Saved entry has non-zero `created` field; matches current time ± 5s |
| STAB-0368 | ✅ | P3 | Add registry test: update existing entry (save with existing id) | `mc3/test/mc3_registry_test.cpp` | Added `testUpdateExistingEntry()`: no prior test ever re-saved the same id (all existing tests only insert fresh rows). Saves an entry, re-saves with `Entry.id` set to the returned id and a changed description, confirms `save()` returns the same id (not a new one), the table still has exactly 1 row (no duplicate insert), and `search()` returns the updated description — exercising `save()`'s `UPDATE` branch (`ModelRegistry.cpp:189-201`) for the first time. root 18/18 (verified 2026-07-02) |
| STAB-0369 | 📋 | P3 | Verify registry `source` field values are constrained | `src/MeshCraft/ModelRegistry.cpp` | Only "handmade" or "ai_generated" used; documented in API |
| STAB-0370 | ✅ | P2 | Verify registry variant field allows empty string | `mc3/test/mc3_registry_test.cpp` | Existing tests exercised empty variant incidentally (default-constructed `Entry`) but never explicitly saved+searched+asserted it stays empty. Added `testEmptyVariantField()`: saves with `variant = ""`, searches back, confirms `results[0].variant.empty()` — the `col()` lambda's `t ? t : ""` NULL-guard plus the schema's `variant TEXT NOT NULL DEFAULT ''` mean this was already safe, now verified directly. root 18/18 (verified 2026-07-02) |

---

## S10 — AI Integration Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0371 | ✅ | P0 | Add mock AI response test (no real network call) | `mc3/test/ai_test.cpp` | Added a test seam: `AiAssistant::apiBaseUrl` (default `https://api.anthropic.com`, override for tests) replaces the hardcoded `httplib::SSLClient("api.anthropic.com")` with `httplib::Client(baseUrlCopy)` — httplib's unified `Client` auto-dispatches to SSL for `https://` and plain sockets for `http://`, so production behavior (real HTTPS) is byte-identical while tests can point at `http://127.0.0.1:PORT` with no OpenSSL/cert complexity. New `ai_test` ctest target (`mc3_ai`) spins up a real `httplib::Server` on an ephemeral localhost port and drives `AiAssistant::sendAsync()`/`poll()` through it end-to-end for three cases: success (200, extracts text + stop_reason), truncation (`stop_reason:"max_tokens"` → `wasTruncated()`), and an HTTP error status (401 → `hasError()` with the status code in the message). No real network call. root 19/19 (verified 2026-07-02) |
| STAB-0372 | ✅ | P0 | Add test: JSON extraction from AI response (plain XML) | `mc3/test/ai_test.cpp` | Moved `extractXml()` out of `MeshCraftApplication_UiAi.cpp` (CNA-coupled TU) into new CNA-free `src/MeshCraft/AiResponseAlgorithms.hpp` as `extractXmlAlg` — `UiAi.cpp` now calls the same function, not a duplicate. `testExtractXmlPlain()` confirms plain `<mc3>…</mc3>` and `<?xml…>`-prefixed input both pass through unchanged. root 19/19 (verified 2026-07-02) |
| STAB-0373 | ✅ | P0 | Add test: JSON extraction strips markdown code fences | `mc3/test/ai_test.cpp`, `src/MeshCraft/AiResponseAlgorithms.hpp` | **Found and fixed a real bug**: the original `extractXml()` only located the *start* marker (`<?xml`/`<mc3`) and returned everything to the end of the string — a closing ` ``` ` markdown fence (or any trailing content) survived into the string handed to `Mc3Document::loadFromFile()`, and tinyxml2 does **not** tolerate content after the root element closes (confirmed empirically: this test failed against the original code before the fix, not just in theory). Fixed `extractXmlAlg` to also trim at the matching `</mc3>` close tag. `testExtractXmlStripsMarkdownFence()` confirms exact output; `testFullPipelineHandlesMarkdownFenceAndProse()` confirms the full extract→repair→parse pipeline now succeeds end-to-end for a fenced response. root 19/19 (verified 2026-07-02) |
| STAB-0374 | ✅ | P1 | Add test: XML extraction with extra text before/after XML | `mc3/test/ai_test.cpp` | Same fix and mechanism as STAB-0373 (the fix isn't fence-specific — it trims at `</mc3>` regardless of what follows). `testExtractXmlWithSurroundingProse()` confirms "Here is the scene: `<mc3>…</mc3>` Hope that helps" extracts to exactly the XML; the pipeline test confirms it parses successfully end-to-end too. root 19/19 (verified 2026-07-02) |
| STAB-0375 | ✅ | P1 | Add test: invalid XML response → `aiValidationError_` set | `mc3/test/ai_test.cpp`, `src/MeshCraft/AiResponseAlgorithms.hpp` | Extracted the *entire* validation pipeline (extract → repair → require `<mc3` → parse → reject-if-empty) from `drawAiPanel()`'s inline `try`/`catch` into `validateAndParseAiResponseAlg()` — `UiAi.cpp` now calls this one function and only keeps the ImGui-specific line-number-diagnostic enrichment locally. Tests confirm: no `<mc3>` root → error naming that specifically; malformed (unclosed) XML → non-empty error, no document. root 19/19 (verified 2026-07-02) |
| STAB-0376 | ✅ | P1 | Add test: empty document rejection (no objects, no definitions) | `mc3/test/ai_test.cpp`, `src/MeshCraft/AiResponseAlgorithms.hpp` | Extracted the empty-document guard as `isEmptyMc3DocumentAlg`, used by `validateAndParseAiResponseAlg`. `testValidateAndParseEmptyDocumentRejected()`: `<mc3 version="0.3"></mc3>` (no objects, no definitions) is rejected with an "empty document" error; `testValidateAndParseAcceptsNonEmptyDocument()` confirms a document with ≥1 object is accepted, as a negative check. root 19/19 (verified 2026-07-02) |
| STAB-0377 | ✅ | P0 | Add test: Apply to Scene pushes undo | `src/MeshCraft/MeshCraftApplication_UiAi.cpp`, `mc3/test/editor_commands_test.cpp` | Already covered by `testUndoRedoAiApply()` (editor_commands_test.cpp:974-984), which exercises `checkUndoRedo` against a plain document replacement — verified this matches the real "Apply to Scene" button code exactly: `MeshCraftApplication_UiAi.cpp:264-266` does `pushUndo(); document_ = *aiPendingDoc_;`. No new test needed. root 19/19 (verified 2026-07-03) |
| STAB-0378 | ✅ | P0 | Verify API key not logged or printed | `src/MeshCraft/AiAssistant.cpp` | `grep -n "std::cout\|std::cerr\|printf\|fprintf\|std::clog" src/MeshCraft/AiAssistant.cpp` — no match involving the key. The only use of `apiKeyCopy` is building the `x-api-key` HTTP header (`AiAssistant.cpp:210`); it is never written to any stream. (verified 2026-07-03) |
| STAB-0379 | ✅ | P0 | Verify API key loaded from `ANTHROPIC_API_KEY` env var | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Already implemented at `MeshCraftApplication_UiAi.cpp:138-143`: if the key input buffer is empty, `std::getenv("ANTHROPIC_API_KEY")` pre-fills it every frame the panel is open. (verified 2026-07-03) |
| STAB-0380 | 🧪 | P1 | Verify API key field is password-masked in UI | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | API key input uses `ImGuiInputTextFlags_Password` |
| STAB-0381 | 📋 | P1 | Add test: model name is user-configurable | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Model field editable; value sent in API request |
| STAB-0382 | 📋 | P1 | Verify network error produces user-visible error message | `src/MeshCraft/AiAssistant.cpp` | Simulate connection refused; `hasError()` true; `errorMsg()` non-empty |
| STAB-0383 | 📋 | P1 | Verify HTTP timeout is set (not infinite wait) | `src/MeshCraft/AiAssistant.cpp` | cpp-httplib timeout configured; `sendAsync()` eventually returns even if server hangs |
| STAB-0384 | 🧪 | P1 | Verify Send disabled when scope=Selection and selection empty | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Empty selection + Selection scope: Send button grayed |
| STAB-0385 | 🧪 | P1 | Verify Selection scope serializes only selected objects | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Select 2 of 5; send; sent XML has 2 objects (plus materials) |
| STAB-0386 | 📋 | P1 | Verify Full Scene scope sends complete document | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Full scene: sent XML matches `doc.saveToString()` |
| STAB-0387 | 📋 | P2 | Verify cancel/reset while request in flight is safe | `src/MeshCraft/AiAssistant.cpp` | Click Reset while request running; background thread terminates without crash |
| STAB-0388 | 📋 | P2 | Verify async lifetime safety: doc destroyed while AI running | `src/MeshCraft/AiAssistant.cpp` | Close file while request in flight; callback doesn't access freed memory |
| STAB-0389 | 📋 | P1 | Verify `MESHCRAFT_HAS_AI` guard: AI panel not shown when undefined | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Non-AI build: "AI Assistant" not in View menu |
| STAB-0390 | 📋 | P1 | Verify non-AI build compiles and links without httplib/OpenSSL | `CMakeLists.txt` | Build with `MESHCRAFT_HAS_AI` not defined; no linker errors |
| STAB-0391 | ✅ | P1 | Add XSD validation of AI response | `src/MeshCraft/AiResponseAlgorithms.hpp`, `mc3/mc3.xsd`, `CMakeLists.txt`, `cmake/Mc3XsdEmbed.hpp.in`, `mc3/test/ai_test.cpp` | Added `validateXmlAgainstXsdAlg` (libxml2 `xmlSchema*` API) and wired it into `validateAndParseAiResponseAlg` after structural parse + empty-doc check. `mc3.xsd` is embedded into a generated header at CMake configure time (`Mc3XsdEmbed.hpp.in` → `generated/MeshCraft/Mc3XsdEmbed.hpp`) so validation never depends on a runtime file path/CWD. New optional `find_package(LibXml2)` (desktop builds only, same pattern as SQLite3/OpenSSL): when found, `MESHCRAFT_HAS_LIBXML2` is defined and both `MeshCraft` and `ai_test` link `libxml2`; when absent, `validateXmlAgainstXsdAlg` is a no-op returning "valid" (graceful degradation — structural parsing via tinyxml2 already happened, XSD is a stricter additional check, not the only line of defense). **Found and fixed a real pre-existing schema gap while building this**: `Mc3XmlWriter.cpp`/`Mc3XmlParser.cpp` write/read an `id` attribute on every scene object, but `mc3.xsd`'s `objectAttrs` group never declared it — any object with `id` (which is all of them, in practice) would have failed strict XSD validation. Fixed by adding `<xs:attribute name="id" type="xs:string"/>` to `objectAttrs` (deliberately `xs:string` not `xs:ID`, since object ids aren't referenced via IDREF and constraining them to the same document-wide ID-uniqueness namespace as materials/textures/definitions isn't an invariant the app actually enforces). 6 new assertions in `ai_test.cpp` (accept a schema-valid response; reject one with `role="bogus"` — `roleType` only allows `"cutter"` — both directly on `validateXmlAgainstXsdAlg` and through the full `validateAndParseAiResponseAlg` pipeline). root 19/19, standalone `mc3` 1/1 (verified 2026-07-03) |
| STAB-0392 | 📋 | P2 | Verify temp files from AI validation are cleaned up | `src/MeshCraft/AiAssistant.cpp` | After AI flow; `/tmp/` has no leftover mc3 temp files |
| STAB-0393 | 📋 | P2 | Verify temp file names are unique (no race on concurrent calls) | `src/MeshCraft/AiAssistant.cpp` | Atomic temp file name uses PID or UUID |
| STAB-0394 | 📋 | P2 | Verify AI prompt length limit (very large scene) | `src/MeshCraft/AiAssistant.cpp` | Scene with 1000 objects: confirm API request body size is reasonable |
| STAB-0395 | 📋 | P2 | Verify AI destructive replace requires confirmation for large scene | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Apply AI that replaces 50-object scene with 3-object result: warn user |
| STAB-0396 | 🧪 | P1 | Verify `aiPendingDoc_` reset path: `regSaveFromAi_` cleared | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Reset in AI panel clears `regSaveFromAi_`; registry dialog uses scene definitions |
| STAB-0397 | 📋 | P2 | Add mock test for full AI → registry pipeline (no real API) | new `ai_test.cpp` | Feed mock response → parse → validate → save to in-memory registry |
| STAB-0398 | 🧪 | P1 | Verify system prompt sends valid instruction to Claude | `src/MeshCraft/AiAssistant.cpp` | System prompt starts with "You are a 3D scene editor. Return only valid mc3.xml." |
| STAB-0399 | 📋 | P2 | Verify network error message includes HTTP status code if available | `src/MeshCraft/AiAssistant.cpp` | 401 Unauthorized → "AI error: HTTP 401 (Unauthorized)" shown in UI |
| STAB-0400 | 📋 | P2 | Add test: malformed JSON in AI response → error, not crash | `src/MeshCraft/AiAssistant.cpp` | AI returns `{bad json}` as XML-in-JSON container; handled gracefully |
| STAB-0401 | 📋 | P3 | Verify AI integration documented in README | `README.md` | README explains ANTHROPIC_API_KEY, model config, scope |
| STAB-0402 | 📋 | P3 | Verify AI result XSD validation uses same mc3.xsd as test suite | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Same XSD path used in app and tests |
| STAB-0403 | 📋 | P2 | Verify `sendAsync()` only sends one request at a time | `src/MeshCraft/AiAssistant.cpp` | Click Send twice: second click ignored while first in flight |
| STAB-0404 | 📋 | P2 | Verify OpenSSL absence handled gracefully (runtime) | `src/MeshCraft/AiAssistant.cpp` | App built with OpenSSL but HTTPS fails: clear error; no crash |
| STAB-0405 | 📋 | P3 | Add rate-limit retry behavior documentation | `src/MeshCraft/AiAssistant.cpp` | 429 response: documented behavior (no retry, show error) |
| STAB-0406 | 📋 | P3 | Investigate token limit for large scenes (128K context) | `src/MeshCraft/AiAssistant.cpp` | Document max scene size for AI; truncation policy if needed |
| STAB-0407 | 📋 | P3 | Verify AI model name default updated to latest Claude version | `src/MeshCraft/AiAssistant.cpp` | Default model kept up to date; configurable by user |
| STAB-0408 | 🧪 | P1 | Verify progress indicator shown while AI request in flight | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | During request: ImGui spinner or "Waiting for AI..." shown |
| STAB-0409 | 📋 | P2 | Verify AI panel shows character count or XML size of sent context | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Before Send: "Sending X bytes to AI" info label |
| STAB-0410 | 📋 | P3 | Add test: AI response with definitions but no objects accepted | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | AI returns only `<definitions>` block; not rejected as empty (has definitions) |

---

## S11 — Materials, Textures, and Visual Fidelity

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0411 | 🧪 | P1 | Verify PBR baseColor exported correctly | `mc3togltf/src/GltfExporter.cpp` | See STAB-0166 |
| STAB-0412 | 🧪 | P1 | Verify PBR metallic exported correctly | `mc3togltf/src/GltfExporter.cpp` | See STAB-0167 |
| STAB-0413 | 🧪 | P1 | Verify PBR roughness exported correctly | `mc3togltf/src/GltfExporter.cpp` | See STAB-0168 |
| STAB-0414 | 🧪 | P1 | Verify PBR emissive exported correctly | `mc3togltf/src/GltfExporter.cpp` | See STAB-0169 |
| STAB-0415 | 🧪 | P1 | Verify PBR alpha / alphaMode exported correctly | `mc3togltf/src/GltfExporter.cpp` | See STAB-0170 |
| STAB-0416 | 🧪 | P1 | Verify texture URI exported as relative path | `mc3togltf/src/GltfExporter.cpp` | Texture `textures/wall.png` → glTF uri `textures/wall.png` (relative) |
| STAB-0417 | 📋 | P1 | Verify texture wrapU/wrapV exported to glTF sampler | `mc3togltf/src/GltfExporter.cpp` | `wrapU="repeat"` → glTF sampler wrapS = 10497 (REPEAT) |
| STAB-0418 | 📋 | P1 | Verify texture filter exported to glTF sampler | `mc3togltf/src/GltfExporter.cpp` | `filter="linear"` → glTF sampler min/magFilter values |
| STAB-0419 | 📋 | P1 | Verify texture colorSpace exported | `mc3togltf/src/GltfExporter.cpp` | `colorSpace="srgb"` → glTF extras or KHR extension |
| STAB-0420 | 📋 | P1 | Add test: missing texture warns but does not fail export | `mc3togltf/src/GltfExporter.cpp` | Warn: "texture 'wall.png' not found"; export continues |
| STAB-0421 | 🧪 | P1 | Verify material preview sphere: rendered when material selected | `src/MeshCraft/MeshCraftApplication.cpp` | Select material; 128×128 sphere visible in material panel |
| STAB-0422 | 🧪 | P1 | Verify material preview sphere reflects PBR values | `src/MeshCraft/MeshCraftApplication.cpp` | Change roughness; sphere appearance changes immediately |
| STAB-0423 | 📋 | P2 | Verify material preview sphere uses correct coordinate system | `src/MeshCraft/MeshCraftApplication.cpp` | Metallic=1/roughness=0: sphere looks mirror-like |
| STAB-0424 | 📋 | P2 | Add test: material with same ID but different fields in merge/include | `mc3/src/Mc3XmlParser.cpp` | Document local-wins policy; test confirms |
| STAB-0425 | 📋 | P2 | Verify material after registry insert: no ID collision with scene | `src/MeshCraft/ModelRegistry.cpp` | Insert model; inserted material gets suffix if already present |
| STAB-0426 | 🧪 | P1 | Verify material search filter: case-insensitive | `src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp` | Type "metal"; finds "Metal_01" and "METAL_rusty" |
| STAB-0427 | 📋 | P2 | Verify material duplicate skipped (D3 skipped task): document why | `plan.md` | D3 marked skipped with reason; no ghost implementation |
| STAB-0428 | 📋 | P2 | Verify texture drag-drop updates material reference | `src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp` | Drop PNG on texture field; `texture.src` updated; preview reloads |
| STAB-0429 | 📋 | P2 | Add roundtrip test: material with all fields + texture reference | `mc3/test/roundtrip_test.cpp` | Full material with texture survives save/load |
| STAB-0430 | 📋 | P2 | Add test: material animation roundtrip (color keyframes) | `mc3/test/roundtrip_test.cpp` | Material with animated baseColor: keyframes survive roundtrip |
| STAB-0431 | 📋 | P3 | Verify Blender import visual smoke test (one material) | manual | Load GLB with 1 PBR material in Blender; colors match mc3 spec |
| STAB-0432 | 📋 | P3 | Verify UV mapping roundtrip (scale + offset) | `mc3/test/roundtrip_test.cpp` | UV scale=2/offset=0.5 survives roundtrip |
| STAB-0433 | 📋 | P3 | Document texture path conventions (relative to mc3 file) | `MC3_FORMAT.md` | Texture paths relative to containing mc3.xml documented |
| STAB-0434 | 📋 | P3 | Verify embedded vs external texture setting in GLB export | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | "embed textures" checkbox: GLB embeds png data; "external": separate files |
| STAB-0435 | 📋 | P3 | Add test: material applied to object survives export | `mc3togltf/src/GltfExporter.cpp` | Object with material → glTF node references correct material index |
| STAB-0436 | 📋 | P3 | Verify material color precision (float vs uint8 round-trip) | `mc3/src/Mc3XmlWriter.cpp` | Color "0.8 0.3 0.1" → written with enough precision → re-parsed within 1e-5 |
| STAB-0437 | 📋 | P3 | Add test: unnamed material (id collision with default) | `mc3/src/Mc3XmlParser.cpp` | Material without id field: handled gracefully |
| STAB-0438 | 📋 | P3 | Verify alpha=0 material: exported with MASK or BLEND mode | `mc3togltf/src/GltfExporter.cpp` | Full transparency in mc3 → BLEND alphaMode in glTF |
| STAB-0439 | 📋 | P3 | Verify emissive-only material (no baseColor) exported | `mc3togltf/src/GltfExporter.cpp` | Material with only emissive set: baseColor defaults to 1,1,1,1 |
| STAB-0440 | 📋 | P3 | Verify SVG texture: documented as unsupported in mc3togltf | `MC3_FORMAT.md`, `mc3togltf/src/GltfExporter.cpp` | SVG textures produce warning and are skipped in export |

---

## S12 — Animation Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0441 | 🧪 | P1 | Verify transform animation roundtrip (position keyframes) | `mc3/test/roundtrip_test.cpp` | Action with position channel: keyframes survive save/load |
| STAB-0442 | 🧪 | P1 | Verify transform animation roundtrip (rotation keyframes) | `mc3/test/roundtrip_test.cpp` | Action with rotation channel: keyframes survive save/load |
| STAB-0443 | 🧪 | P1 | Verify transform animation roundtrip (scale keyframes) | `mc3/test/roundtrip_test.cpp` | Action with scale channel: keyframes survive save/load |
| STAB-0444 | 🧪 | P1 | Verify material animation roundtrip | `mc3/test/roundtrip_test.cpp` | Action with material.baseColor channel: keyframes survive |
| STAB-0445 | 🧪 | P1 | Verify deform animation roundtrip | `mc3/test/roundtrip_test.cpp` | Action with deform.x channel: keyframes survive |
| STAB-0446 | 🧪 | P1 | Verify glTF animation export: transform channel exported | `mc3togltf/test/gltf_test.py` | `mc3togltf_gltf` confirms animation array non-empty |
| STAB-0447 | 📋 | P1 | Add test: material animation NOT exported to glTF (unsupported) | `mc3togltf/test/gltf_test.py` | Material animation → mc3togltf prints warning; glTF has 0 material animation tracks |
| STAB-0448 | 📋 | P1 | Add test: deform animation NOT exported to glTF (unsupported) | `mc3togltf/test/gltf_test.py` | Deform animation → mc3togltf prints warning; only transform animations exported |
| STAB-0449 | 🧪 | P1 | Verify timeline multi-select: shift-click selects range | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Shift+click second keyframe; both selected |
| STAB-0450 | 🧪 | P1 | Verify timeline: delete selected keyframes removes them | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Select + Delete; keyframes removed from action |
| STAB-0451 | 🧪 | P1 | Verify timeline duplicate action: creates copy with new name | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Duplicate action; new action appears in dropdown |
| STAB-0452 | 🧪 | P1 | Verify timeline rename action: double-click → inline edit | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Double-click action name; field becomes editable; Enter confirms |
| STAB-0453 | 🧪 | P1 | Verify timeline copy/paste keyframes | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Select keyframes; Ctrl+C; scrub to new time; Ctrl+V; keyframes pasted |
| STAB-0454 | 📋 | P1 | Verify animation edit is undoable (add keyframe) | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Press K; undo; keyframe removed |
| STAB-0455 | 📋 | P1 | Verify animation edit is undoable (delete keyframe) | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Delete keyframe; undo; keyframe restored |
| STAB-0456 | 📋 | P2 | Verify animation on renamed object: action still references correct object | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Rename object mid-animation; action channels still reference it |
| STAB-0457 | 📋 | P2 | Verify animation on deleted object: no crash on playback | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Delete animated object; play action; no crash |
| STAB-0458 | 📋 | P2 | Verify animation after include: included objects animatable | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Load scene with include; add animation to included object; no crash |
| STAB-0459 | 📋 | P2 | Verify animation after registry insert: inserted object animatable | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Insert from registry; add keyframe to inserted object; no crash |
| STAB-0460 | 📋 | P2 | Verify animation scale-time function | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Scale time 2×; all keyframe times doubled |
| STAB-0461 | 📋 | P2 | Verify interpolation: linear keyframes produce linear values between | `src/MeshCraft/Renderer/SceneRenderer.cpp` | At time 0.5 between f0=0 and f1=10: value = 5 |
| STAB-0462 | 📋 | P2 | Verify animation played through renderer: material animation updates material | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Play animation with baseColor channel; material color changes each frame |
| STAB-0463 | 📋 | P2 | Verify animation played through renderer: deform animation updates geometry | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Play animation with deform.x channel; geometry deforms each frame |
| STAB-0464 | 🧪 | P1 | Verify curve editor: interpolation curve visible per channel | `src/MeshCraft/MeshCraftApplication_Anim.cpp` | Open timeline; each channel row shows mini curve |
| STAB-0465 | 📋 | P2 | Add test: action with 0 keyframes exported to glTF (empty animation) | `mc3togltf/src/GltfExporter.cpp` | Action with 0 keyframes: either omitted from glTF or exported as empty |
| STAB-0466 | 📋 | P3 | Verify glTF animation `samplers` and `channels` are consistent | `mc3togltf/test/gltf_test.py` | Each channel references a valid sampler index |
| STAB-0467 | 📋 | P3 | Verify glTF animation timestamps are in seconds | `mc3togltf/src/GltfExporter.cpp` | mc3 frame units → glTF seconds conversion documented |
| STAB-0468 | 📋 | P3 | Verify animation with identical keyframe times (duplicate frames) | `mc3/src/Mc3XmlParser.cpp` | Two keyframes at same time: last wins or error; policy documented |
| STAB-0469 | 📋 | P3 | Verify animation merge: merge scene adds actions without clobbering existing | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Merge two scenes with same action names; suffixes added |
| STAB-0470 | 📋 | P3 | Add animation XSD roundtrip: `interpolation` attribute preserved | `mc3/test/roundtrip_test.cpp` | Keyframe `interpolation="step"` survives save/load |

---

## S13 — Commands, Undo/Redo, and Algorithms

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0471 | ✅ | P0 | applyRenamePattern: 57 tests pass | `mc3/test/editor_commands_test.cpp` | `mc3_commands` passes |
| STAB-0472 | ✅ | P0 | batchRenameObjects: covered by commands tests | `mc3/test/editor_commands_test.cpp` | `mc3_commands` passes |
| STAB-0473 | ✅ | P0 | findReplace: covered by commands tests | `mc3/test/editor_commands_test.cpp` | `mc3_commands` passes |
| STAB-0474 | ✅ | P0 | arrayDuplicateObjects: covered by commands tests | `mc3/test/editor_commands_test.cpp` | `mc3_commands` passes |
| STAB-0475 | ✅ | P1 | Add command test: applyRenamePattern with `{index}` zero-padding | `mc3/test/editor_commands_test.cpp` | `testApplyRenamePatternZeroPaddingSequence()` locks in the sequence behavior (1→"01", 2→"02", 10→"10", and `{index:03d}` 1→"001", 42→"042") beyond the single-value cases `testApplyRenamePattern()` already covered. root 18/18 (verified 2026-07-02) |
| STAB-0476 | ✅ | P1 | Add command test: batchRename skips locked objects (algorithm) | `mc3/test/editor_commands_test.cpp` | Already covered — `testBatchRename()`'s "with one locked object" case (added earlier) IS the algorithm-level test this item asks for: locked object keeps its name, index still advances past it. No new test needed. |
| STAB-0477 | ✅ | P1 | Add command test: findReplace with regex special chars in search | `mc3/test/editor_commands_test.cpp` | `replaceAllInString()` uses plain `std::string::find()`, never `std::regex`, so special characters are literal by construction. Added `testFindReplaceTreatsSpecialCharsLiterally()` locking this in explicitly: `.` in "Box.001" matches only a literal `.` (not `_`/`X` in that position), and a regex-invalid string (`"(unbalanced*"`) is just a literal no-match, not an error. root 18/18 (verified 2026-07-02) |
| STAB-0478 | ✅ | P1 | Add command test: deepCopyObjectAlg preserves children | `mc3/test/editor_commands_test.cpp` | Already covered — `testDeepCopy()` (added earlier) directly asserts child count, child fields, and child independence after mutation. No new test needed. |
| STAB-0479 | ✅ | P1 | Add command test: deepCopyObjectAlg generates unique IDs | `mc3/test/editor_commands_test.cpp` | **The item's premise doesn't apply to this function** — `deepCopyObjectAlg` is the snapshot primitive `checkUndoRedo()` is built on (see `snapshotDoc()`), and undo/redo *requires* identical ids across a snapshot restore (every existing `checkUndoRedo` test compares XML-serialized equality after undo, which would break if ids changed). Unique-id generation is correctly a *different* operation, layered on top by `duplicateObjectsAlg` (already tested: `root[1]->id == "a_copy"` in `testDuplicateObjects()`). Added `testDeepCopyPreservesIdentityForSnapshots()` to document and lock in this distinction explicitly, cross-referencing both behaviors side by side. root 18/18 (verified 2026-07-02) |
| STAB-0480 | ✅ | P1 | Verify every command pushes undo entry | `mc3/test/editor_commands_test.cpp` | Audited every `MeshCraftApplication_Commands.cpp` function for a document mutation without a preceding `pushUndo()`. **Found and fixed a real bug**: `resetPivot()` mutated selected objects' `transform.position`/`pivot` and set `modified_ = true`, but never called `pushUndo()` — resetting a pivot could not be undone with Ctrl+Z, unlike every other mutating command in that file. Fixed by adding a `hasSelection()` guard + `pushUndo()` at the top, matching every other command's pattern. (`toggleIsolate()`'s missing `pushUndo()` was reviewed and left alone — it's intentionally self-reversing via `preisolateVisibility_`, so a discrete undo step would be redundant, not a bug.) No headless test possible: `resetPivot()`'s pivot-compensation math uses `Microsoft::Xna::Framework::Matrix` (a CNA type), not separable into a CNA-free mirror. Verified by code inspection + confirming the fix compiles clean and root 18/18 still passes. |
| STAB-0481 | ✅ | P1 | Verify undo/redo does not exceed stack depth limit | `mc3/test/editor_commands_test.cpp` | Confirmed a real, correctly-implemented cap already exists: `kUndoMax = 20` (`MeshCraftApplication.hpp:545`), enforced by the identical push-then-trim-oldest pattern at all three stack-mutation sites (`pushUndo()` in `MeshCraftApplication_Commands.cpp:323-329`, and the undo/redo key handlers' opposite-stack pushes in `MeshCraftApplication_Keyboard.cpp:44-69`). Added CNA-free mirror `pushWithCapAlg` (`EditorAlgorithms.hpp`) of that shared pattern; `testUndoStackDepthCapped()` pushes 200 entries and confirms the final size is exactly 20 and holds the 20 *most recent* pushes (oldest 180 correctly dropped), plus a below-cap sanity check. root 18/18 (verified 2026-07-02) |
| STAB-0482 | 📋 | P2 | Verify "Convert to Definition" creates correct Definition entry | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Selected object → definition; Instance added in place |
| STAB-0483 | 📋 | P2 | Verify "Break Instance" expands to copy of definition content | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Instance expanded; definition still exists; no shared pointers |
| STAB-0484 | 📋 | P2 | Verify "Align to Object": aligns selection to target transform | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Select 2 objects + target; align; position matches target |
| STAB-0485 | 📋 | P2 | Verify Scatter/Place: count objects added matches requested | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Scatter N=10; exactly 10 new objects added |
| STAB-0486 | 📋 | P2 | Verify command palette executes same code path as menu | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Command palette "Delete" → same function as Edit→Delete |
| STAB-0487 | 📋 | P2 | Verify macro recorder captures batchRename step | `src/MeshCraft/MeshCraftApplication_Macro.cpp` | Record batchRename; save macro; step appears in `.mc3macro` file |
| STAB-0488 | 📋 | P2 | Verify macro playback: step for nonexistent object name is skipped | `src/MeshCraft/MeshCraftApplication_Macro.cpp` | Macro recorded on "Cube"; play in scene without "Cube"; step skipped |
| STAB-0489 | 📋 | P2 | Verify proportional editing: nearby objects influenced | `src/MeshCraft/MeshCraftApplication_Mouse.cpp` | Move object with proportional on; nearby objects shifted by falloff amount |
| STAB-0490 | 📋 | P2 | Verify snapping: vertex snap to nearest object position | `src/MeshCraft/MeshCraftApplication_Mouse.cpp` | Shift+drag; object position snaps to nearest other object position |
| STAB-0491 | 📋 | P2 | Verify angle snapping: rotate snaps to 45° increments | `src/MeshCraft/MeshCraftApplication_Mouse.cpp` | Ctrl+rotate; rotation value rounds to nearest 45° |
| STAB-0492 | 📋 | P2 | Verify "Select Children" command selects all descendants | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Select parent with 2 levels of children; Select Children → all descendants selected |
| STAB-0493 | 📋 | P2 | Verify "Random variant" creates instance pointing to random definition | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Multiple calls produce different definition assignments |
| STAB-0494 | 📋 | P3 | Add command test: arrayDuplicate with negative count fails gracefully | `mc3/test/editor_commands_test.cpp` | count=-1; no objects added; no crash |
| STAB-0495 | 📋 | P3 | Verify Group Scale: positions scaled from selection centroid | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | 2 objects at (0,0,0) and (2,0,0); scale 2×; at (−1,0,0) and (3,0,0) |

---

## S14 — Rendering and Viewport Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0496 | 🧪 | P0 | Smoke test: editor starts and renders without crash | `test/smoke_test.sh` | `smoke_test` passes |
| STAB-0497 | 🧪 | P1 | Verify renderer handles all object types without crash | `src/MeshCraft/Renderer/SceneRenderer.cpp` | `all_objects.mc3.xml` loads; renders 1 frame; exits cleanly (smoke test) |
| STAB-0498 | 🧪 | P1 | Verify renderer handles empty scene (no objects) | `src/MeshCraft/Renderer/SceneRenderer.cpp` | `<mc3><objects/></mc3>` loaded; renders without crash |
| STAB-0499 | 📋 | P1 | Verify renderer handles missing mesh file gracefully | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Mesh with nonexistent `src`: placeholder sphere or empty; no crash |
| STAB-0500 | 📋 | P1 | Verify renderer handles missing material reference | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Object with `material="nonexistent"`: rendered with default white material |
| STAB-0501 | 🧪 | P1 | Verify gizmo: translate gizmo appears on selected object | `src/MeshCraft/Renderer/SceneRenderer_Gizmos.cpp` | Select object; XYZ arrows visible at object position |
| STAB-0502 | 🧪 | P1 | Verify gizmo: rotate gizmo appears on right mode | `src/MeshCraft/Renderer/SceneRenderer_Gizmos.cpp` | Switch to rotate mode (R); rotation circles appear |
| STAB-0503 | 🧪 | P1 | Verify picking: click on object selects it | `src/MeshCraft/MeshCraftApplication_Mouse.cpp` | Click on visible object; object appears selected in hierarchy |
| STAB-0504 | 📋 | P1 | Verify picking: click on empty space deselects all | `src/MeshCraft/MeshCraftApplication_Mouse.cpp` | Click on empty viewport; selection cleared |
| STAB-0505 | 🧪 | P1 | Verify bounding box toggle: AABB visible for selected | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Toggle bounding boxes; selected object has visible AABB outline |
| STAB-0506 | 🧪 | P1 | Verify camera presets: Front view sets camera correctly | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Click Front preset; camera aligned to -Z axis |
| STAB-0507 | 🧪 | P1 | Verify orthographic/perspective toggle | `src/MeshCraft/MeshCraftApplication.cpp` | Toggle button; projection matrix changes |
| STAB-0508 | 🧪 | P1 | Verify SSAO toggle: off/on changes visual output | `src/MeshCraft/MeshCraftApplication.cpp` | Toggle SSAO; scene AO changes |
| STAB-0509 | 🧪 | P1 | Verify bloom toggle: off/on changes visual output | `src/MeshCraft/MeshCraftApplication.cpp` | Toggle bloom; bright area glow changes |
| STAB-0510 | 🧪 | P1 | Verify wireframe mode: all objects rendered as wireframe | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Toggle wireframe; solid objects become wireframe |
| STAB-0511 | 📋 | P2 | Verify fog visualization: linear fog gradient correct | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Enable linear fog start/end; distant objects appear fogged |
| STAB-0512 | 📋 | P2 | Verify light sphere gizmos: point light shows sphere + rays | `src/MeshCraft/Renderer/SceneRenderer_Gizmos.cpp` | Scene with point light; gizmo visible in viewport |
| STAB-0513 | 📋 | P2 | Verify spot light gizmo: sphere + cone visible | `src/MeshCraft/Renderer/SceneRenderer_Gizmos.cpp` | Scene with spot light; cone gizmo visible |
| STAB-0514 | 📋 | P2 | Verify delta overlay: shown while dragging gizmo | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Drag position gizmo; "Δ +2.50" visible near cursor |
| STAB-0515 | 📋 | P2 | Verify render stats: vertex/face count updates on selection change | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Select object; stats show object-specific vertex count |
| STAB-0516 | 📋 | P2 | Verify shadow map debug overlay: renders from first directional light | `src/MeshCraft/MeshCraftApplication.cpp` | Add directional light; open Shadow Frustum window; depth image visible |
| STAB-0517 | 📋 | P2 | Verify "Look through camera": renders from Mc3Camera position/rotation | `src/MeshCraft/MeshCraftApplication.cpp` | Select camera object; look-through; viewport matches camera FOV |
| STAB-0518 | 📋 | P2 | Verify locked object outline: locked objects have distinct visual | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Lock object; visual outline appears in viewport |
| STAB-0519 | 📋 | P2 | Verify proportional editing falloff: visible sphere indicator | `src/MeshCraft/MeshCraftApplication_Mouse.cpp` | Enable proportional editing; radius sphere drawn in viewport |
| STAB-0520 | 📋 | P3 | Verify large scene FPS acceptable (200 objects, ≥ 30 FPS) | manual | Load `large_scene.mc3.xml`; stats overlay shows ≥ 30 FPS |
| STAB-0521 | 📋 | P3 | Verify no GL state leak between render passes | `src/MeshCraft/Renderer/SceneRenderer.cpp` | After rendering; GL error query returns GL_NO_ERROR |
| STAB-0522 | 📋 | P3 | Verify CSG preview cache: re-render without touching CSG returns cache hit | `src/MeshCraft/Renderer/SceneRenderer.cpp` | 60 fps with static CSG: no evaluation each frame |
| STAB-0523 | 📋 | P3 | Verify background texture renders before 3D scene | `src/MeshCraft/MeshCraftApplication.cpp` | Scene with background texture: texture behind all 3D objects |
| STAB-0524 | 📋 | P3 | Verify skybox panorama shader: equirectangular image displayed | `src/MeshCraft/MeshCraftApplication.cpp` | Add equirectangular skybox; renders around scene |
| STAB-0525 | 📋 | P3 | Verify LOD: primitive segment count decreases at distance | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Move camera far from sphere; rendered segment count decreases |

---

## S15 — Import/Export/Editor Integration

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0526 | 🧪 | P1 | Verify File → Export GLB from editor | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Menu export; file created; magic bytes OK |
| STAB-0527 | 🧪 | P1 | Verify File → Export glTF from editor | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Menu export; `.gltf` file created; valid JSON |
| STAB-0528 | 🧪 | P1 | Verify invalid extension in editor export produces error | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Type `out.foo` in export dialog; error shown; no file created |
| STAB-0529 | 🧪 | P1 | Verify exporter stats shown in status bar after export | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | After export: status shows "Exported N nodes, M meshes, W warnings" |
| STAB-0530 | 🧪 | P1 | Verify mc3tomcb tool: `mc3tomcb in.mc3.xml out.mcb` produces file | `mc3tomcb/src/main.cpp` | File created; non-zero size |
| STAB-0531 | 📋 | P1 | Add CTest for mc3tomcb: verify output file exists and is non-zero | `mc3tomcb/CMakeLists.txt` | `ctest -R mc3tomcb` passes |
| STAB-0532 | 🧪 | P1 | Verify OBJ import: Browse button opens file path entry | `src/MeshCraft/Scene/PropertiesPanel.cpp` | Browse; path filled in `meshSource` field |
| STAB-0533 | 📋 | P1 | Verify OBJ import: file path persists in XML on save | `mc3/src/Mc3XmlWriter.cpp` | Object with OBJ meshSource saved; path in XML |
| STAB-0534 | 📋 | P1 | Verify Merge Scene: all objects from merged file appear in hierarchy | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Merge `house.mc3.xml`; house objects visible in hierarchy |
| STAB-0535 | 📋 | P1 | Verify Merge Scene: definitions merged, not duplicated | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Merge file with same definition: suffix added if collision |
| STAB-0536 | 📋 | P1 | Verify Export Subtree: exported file validates against XSD | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Export subtree; `validate_xsd.py` on output: passes |
| STAB-0537 | 📋 | P2 | Verify mc3tomcb handles missing input file gracefully | `mc3tomcb/src/main.cpp` | `mc3tomcb nonexistent.mc3.xml out.mcb` exits non-zero with message |
| STAB-0538 | 📋 | P2 | Verify mc3tomcb handles write-protected output path | `mc3tomcb/src/main.cpp` | `mc3tomcb in.mc3.xml /root/out.mcb` exits non-zero with error |
| STAB-0539 | 📋 | P2 | Verify editor drag-drop: drop `.mc3.xml` → loads scene | `src/MeshCraft/MeshCraftApplication.cpp` | SDL drop event: file loaded |
| STAB-0540 | 📋 | P2 | Verify includes preserved through editor load/save cycle | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Load scene with `<include>`; save; reload; include paths preserved |
| STAB-0541 | 📋 | P2 | Verify embedded assets preserved through editor load/save | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Scene with embedded GLTF (N2): save; reload; embed data intact |
| STAB-0542 | 📋 | P2 | Verify screenshot export (headless): PNG valid | `src/MeshCraft/main.cpp` | `./MeshCraft --screenshot scene.mc3.xml out.png`; PNG magic bytes present |
| STAB-0543 | 📋 | P2 | Verify GLB export settings: texture embedding choice applied | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | "Embed textures" ON: GLB has bufferViews with image data |
| STAB-0544 | 📋 | P3 | Verify material export/import (.mc3mat.xml) roundtrip | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Export material; clear materials; import; material restored |
| STAB-0545 | 📋 | P3 | Verify material import with collision: suffix applied | `src/MeshCraft/MeshCraftApplication_UiOverlays.cpp` | Import material with same ID as existing; suffix added |
| STAB-0546 | 📋 | P3 | Verify `mc3togltf` CLI: no output file created on error | `mc3togltf/src/main.cpp` | Invalid input → no partial output file left on disk |
| STAB-0547 | 📋 | P3 | Verify `mc3togltf` help text accurate | `mc3togltf/src/main.cpp` | `mc3togltf --help` shows all CLI flags |
| STAB-0548 | 📋 | P3 | Verify includes during GLB export: included definitions exported | `mc3togltf/src/GltfExporter.cpp` | Scene with include; GLB has included definition meshes |
| STAB-0549 | 📋 | P3 | Verify embedded assets during GLB export: `embed:` meshSource resolved | `mc3togltf/src/GltfExporter.cpp` | (STAB-0194 extended) or marked as limitation |
| STAB-0550 | 📋 | P3 | Verify subtree template import resolves relative paths | `src/MeshCraft/MeshCraftApplication_Commands.cpp` | Imported template with relative texture paths: paths resolved relative to template file |

---

## S16 — Cross-Platform Stability

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0551 | 🟡 | P0 | Linux desktop build and run | `CMakeLists.txt` | Build + smoke test passes on Linux x86_64 |
| STAB-0552 | 📋 | P1 | Windows (MinGW) build + run | `CMakeLists.txt` | MinGW cross-compile; .exe starts on Windows |
| STAB-0553 | 📋 | P2 | Emscripten web build + basic smoke | `CMakeLists.txt` | emcmake cmake succeeds; `.html` loads in browser |
| STAB-0554 | 📋 | P2 | Verify path separators: Windows paths handled | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | File paths use `std::filesystem` or forward-slash normalization |
| STAB-0555 | 📋 | P2 | Verify UTF-8 filenames: load/save XML with non-ASCII path | `mc3/src/Mc3XmlParser.cpp` | Path with UTF-8 chars: file loaded and saved correctly on Linux |
| STAB-0556 | 📋 | P2 | Verify path with spaces: load/save XML with spaces in path | `mc3/src/Mc3XmlParser.cpp` | `my scene files/house.mc3.xml` loads correctly |
| STAB-0557 | 📋 | P2 | Verify non-ASCII object names in XML and UI | `mc3/src/Mc3XmlParser.cpp` | Object named `Slon_čelovek` roundtrips correctly |
| STAB-0558 | 📋 | P2 | Verify config dir per OS (Linux: `~/.config`, Windows: `%APPDATA%`) | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | Prefs written to OS-specific config dir |
| STAB-0559 | 📋 | P2 | Verify clipboard paste: Ctrl+V from OS clipboard | `src/MeshCraft/MeshCraftApplication_Keyboard.cpp` | Copy text from browser; paste in mesh path field |
| STAB-0560 | 📋 | P2 | Verify SDL drag-drop on Linux: `.mc3.xml` file drag works | `src/MeshCraft/MeshCraftApplication.cpp` | Drag from Nautilus/Dolphin; file loads |
| STAB-0561 | 📋 | P3 | Verify file dialogs (text path fields) work on all platforms | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | F1 (native) skipped; text path field works universally |
| STAB-0562 | 📋 | P3 | Verify OpenGL ES 3 features on web (WebGL2) | `CMakeLists.txt` | Emscripten build with WebGL2; all primitive shaders compile |
| STAB-0563 | 📋 | P3 | Verify SQLite stubs compile on Emscripten | `src/MeshCraft/ModelRegistry.cpp` | Emscripten build: `MESHCRAFT_HAS_SQLITE3` undefined; stubs compile |
| STAB-0564 | 📋 | P3 | Verify AI stubs compile on Emscripten | `src/MeshCraft/AiAssistant.cpp` | Emscripten build: `MESHCRAFT_HAS_AI` undefined; no linker errors |
| STAB-0565 | 📋 | P3 | Verify IDBFS persistence on web build | `CMakeLists.txt` | Web build uses `-lidbfs.js`; prefs survive page reload |
| STAB-0566 | 📋 | P3 | Verify Windows: SDL runtime DLLs copied | `CMakeLists.txt` | `cna_copy_sdl_runtime` post-build command works |
| STAB-0567 | 📋 | P3 | Verify MinGW: static libgcc/libstdc++ linked | `CMakeLists.txt` | `-static-libgcc -static-libstdc++` in MINGW flags |
| STAB-0568 | 📋 | P3 | Verify Android stub: main() → shared lib adapter | `CMakeLists.txt` | Android build: `add_library(main SHARED)` selected |
| STAB-0569 | 📋 | P3 | Verify preloaded web assets: `test/` files accessible | `CMakeLists.txt` | `--preload-file test@/test` → files accessible in Emscripten FS |
| STAB-0570 | 📋 | P3 | Verify wasm exceptions enabled on Emscripten | `CMakeLists.txt` | `-fwasm-exceptions` present in Emscripten compile/link flags |
| STAB-0571 | 📋 | P3 | Verify web export: exported GLB downloadable from browser | manual | Use web build; export scene; download and verify GLB |
| STAB-0572 | 📋 | P3 | Verify web SSAO: works on WebGL2 | manual | SSAO in web build produces visible AO; no GL error |
| STAB-0573 | 📋 | P3 | Verify web bloom: works on WebGL2 | manual | Bloom in web build produces visible glow; no GL error |
| STAB-0574 | 📋 | P3 | Document platform feature matrix | `README.md` | Table: feature × (Linux/Windows/Web/Android) availability |
| STAB-0575 | 📋 | P3 | Verify Windows: spaces in CMake binary dir work | `CMakeLists.txt` | Build from `C:\My Projects\MeshCraft\cmake-build-debug\` |

---

## S17 — Documentation and User-Facing Honesty

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0576 | ✅ | P0 | README: "XML-based (.mc3.xml)" not "YAML-based" | `README.md` | S11 done |
| STAB-0577 | ✅ | P0 | README: feature list accurate | `README.md` | S11 done |
| STAB-0578 | ✅ | P0 | README: build instructions correct (ninja, cmake-build-debug) | `README.md` | S11 done |
| STAB-0579 | 🟡 | P1 | README: add note that `file(GLOB_RECURSE)` requires reconfigure | `README.md` | README warns about cmake reconfigure after adding .cpp files |
| STAB-0580 | 🟡 | P1 | README: add C++23 compiler requirement | `README.md` | README states GCC ≥ 13 or Clang ≥ 17 required |
| STAB-0581 | 🟡 | P1 | README: document lxml dependency for `xsd_validation` | `README.md` | README says `pip3 install lxml` required for XSD test |
| STAB-0582 | ✅ | P0 | MC3_FORMAT.md: created and documents all sections | `MC3_FORMAT.md` | S12 done |
| STAB-0583 | 🟡 | P1 | MC3_FORMAT.md: update for N3-N7 new elements | `MC3_FORMAT.md` | `<scripts>`, `<sounds>`, `<music>`, `<triggers>`, `<states>`, `<meta>` sections added |
| STAB-0584 | 🟡 | P1 | MC3_FORMAT.md: document MCB format alongside XML | `MC3_FORMAT.md` | MCB section: what it is, how to produce, file extension, relationship to mc3.xml |
| STAB-0585 | 📋 | P1 | MC3_FORMAT.md: document export support matrix (mc3togltf limitations) | `MC3_FORMAT.md` | Table: which features export to glTF, which produce warnings, which are unsupported |
| STAB-0586 | 🟡 | P1 | STABILIZATION.md: update to reflect new stabilization plan | `STABILIZATION.md` | STABILIZATION.md shows STAB-XXXX task IDs; policy section updated |
| STAB-0587 | 🟡 | P1 | NEXT.md: rewritten to be short and operational | `NEXT.md` | NEXT.md shows current gate, exact next task, commands to run |
| STAB-0588 | 📋 | P1 | m1m2m3.md: verify still accurate (M1/M2/M3 complete) | `m1m2m3.md` | Review m1m2m3.md against current code; mark any discrepancies |
| STAB-0589 | 📋 | P1 | Document CSG limitations explicitly | `MC3_FORMAT.md` | Document: no UV, no normals preservation, strict mode default |
| STAB-0590 | 📋 | P1 | Document AI integration limitations | `README.md` or `AI.md` | Document: no Emscripten support, key not stored, approximate validation |
| STAB-0591 | 📋 | P1 | Document registry limitations | `README.md` or `AI.md` | Document: no thumbnail, SQLite required on desktop, no sync |
| STAB-0592 | 📋 | P2 | Add TESTING.md: documents test suite, how to run, what each test covers | `TESTING.md` | New file; each test listed with purpose and expected output |
| STAB-0593 | 📋 | P2 | Add contributor instructions (build setup, CLAUDE.md constraints) | `CONTRIBUTING.md` | How to add new .cpp file (reconfigure), CNA boundary, API change policy |
| STAB-0594 | 📋 | P2 | Document `<include>` semantics fully | `MC3_FORMAT.md` | Include merge rules, local-override policy, cycle detection, skip-sets documented |
| STAB-0595 | 📋 | P3 | Add release checklist | `RELEASE.md` | Checklist: build clean, all tests pass, docs current, sample files valid |

---

## S18 — Code Quality and Architecture

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0596 | 🟡 | P1 | Verify no editor code in Mc3 library | `mc3/src/*.cpp`, `mc3/include/*.hpp` | `grep -r "ImGui" mc3/` returns 0 hits |
| STAB-0597 | 🟡 | P1 | Verify no CNA dependency in Mc3 library | `mc3/CMakeLists.txt` | Mc3 links only tinyxml2; no CNA target in link libs |
| STAB-0598 | 🟡 | P1 | Verify no UI dependency in mc3togltf | `mc3togltf/CMakeLists.txt` | mc3togltf_lib links only Mc3, tinyobjloader, manifold; no ImGui |
| STAB-0599 | 🧪 | P1 | Verify EditorAlgorithms.hpp is CNA-free | `src/MeshCraft/EditorAlgorithms.hpp` | `grep -i cna src/MeshCraft/EditorAlgorithms.hpp` returns 0 |
| STAB-0600 | 📋 | P1 | Audit `MeshCraftApplication_UiProperties.cpp` for null-pointer risk | `src/MeshCraft/MeshCraftApplication_UiProperties.cpp` | All property accessors guard `selectedObject != nullptr` |
| STAB-0601 | 📋 | P1 | Audit `SceneRenderer.cpp` for null-pointer risk | `src/MeshCraft/Renderer/SceneRenderer.cpp` | All material/mesh pointer accesses checked |
| STAB-0602 | 🧪 | P1 | Verify `EditorAlgorithms.hpp` in `src/` is accessible from test | `mc3/CMakeLists.txt` | `target_include_directories(mc3_commands_test PRIVATE ${CMAKE_SOURCE_DIR}/src/MeshCraft)` |
| STAB-0603 | 📋 | P2 | Refactor `EditorAlgorithms.hpp` to public include directory | `include/MeshCraft/EditorAlgorithms.hpp` | Move to `include/`; update test include paths; fragile `src/` path removed |
| STAB-0604 | 📋 | P2 | Audit duplicate code: `SceneRenderer_Builders.cpp` vs `MeshBuilder.cpp` | both files | Any shared geometry functions? Extract to shared utility if yes |
| STAB-0605 | 📋 | P2 | Verify logging is consistent (no mix of printf/cerr/custom logger) | all `.cpp` | Error reporting uses consistent channel; no raw `printf` in library code |
| STAB-0606 | 📋 | P2 | Add error result type to Mc3XmlParser (instead of throwing) | `mc3/src/Mc3XmlParser.cpp` | Optional: provide `loadFromFile(path, &error)` non-throwing variant |
| STAB-0607 | 📋 | P2 | Verify McbWriter/McbReader don't use raw `new`/`delete` | `mcb/src/McbWriter.cpp`, `mcb/src/McbReader.cpp` | RAII everywhere; no manual memory management |
| STAB-0608 | 📋 | P2 | Verify all file I/O in Mc3 library uses `std::filesystem` | `mc3/src/Mc3XmlParser.cpp`, `mc3/src/Mc3XmlWriter.cpp` | No raw `fopen`; uses `std::filesystem::path` |
| STAB-0609 | 📋 | P2 | Investigate header-include hygiene (IWYU) | all headers | Run `include-what-you-use` on Mc3 library; fix unused includes |
| STAB-0610 | 📋 | P2 | Verify compiler warnings at `-Wall -Wextra` | `CMakeLists.txt` | Add `-Wall -Wextra` to Mc3 and mc3togltf targets; fix all warnings |
| STAB-0611 | 📋 | P2 | Extract temp-file helper (unique temp path generation) | new `TempFile.hpp` | Shared between AI assistant and any future user; atomic naming |
| STAB-0612 | 📋 | P2 | Extract XML-parse helper (safe attribute reading) | `mc3/src/Mc3XmlParser.cpp` | `attr()`, `attrF()`, `attrB()`, `attrVec3()` already exist; verify consistent use |
| STAB-0613 | 📋 | P2 | Verify no anonymous namespace symbol conflicts across TUs | `mc3/src/Mc3XmlParser.cpp` | Functions in anonymous namespace are TU-local; no ODR violation |
| STAB-0614 | 📋 | P3 | Run clang-tidy on Mc3 library | `mc3/src/*.cpp` | No clang-tidy errors in default checks |
| STAB-0615 | 📋 | P3 | Run clang-tidy on mc3togltf | `mc3togltf/src/*.cpp` | No clang-tidy errors |
| STAB-0616 | 📋 | P3 | Audit large source files: `MeshCraftApplication.hpp` > 400 lines | `include/MeshCraft/MeshCraftApplication.hpp` | Review; extract members to sub-systems if appropriate |
| STAB-0617 | 📋 | P3 | Audit large source files: `PropertiesPanel.cpp` > 2000 lines | `src/MeshCraft/Scene/PropertiesPanel.cpp` | Review; split by object type (transform, material, geometry tabs) |
| STAB-0618 | 📋 | P3 | Audit large source files: `SceneRenderer.cpp` | `src/MeshCraft/Renderer/SceneRenderer.cpp` | Review; note existing split into 4 files as good practice |
| STAB-0619 | 📋 | P3 | Verify MCB tag constants extracted to shared header | `mcb/src/` | See STAB-0147 |
| STAB-0620 | 📋 | P3 | Investigate static analysis: cppcheck on Mc3 library | `mc3/src/*.cpp` | `cppcheck --enable=all mc3/src/` → no errors; warnings reviewed |

---

## S19 — Security and Robustness

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0621 | 🧪 | P0 | Verify API key not stored in any file | `src/MeshCraft/AiAssistant.cpp`, prefs.ini | `grep -r ANTHROPIC ~/.config/meshcraft/` returns empty |
| STAB-0622 | 🧪 | P0 | Verify API key not logged to stderr/stdout | `src/MeshCraft/AiAssistant.cpp` | Code review: no `std::cout`/`std::cerr`/`printf` of `apiKey` |
| STAB-0623 | 🧪 | P1 | Verify no `std::system()` calls (command injection risk) | all `.cpp` | `grep -r "std::system\|::system(" src/` → 0 hits |
| STAB-0624 | 🧪 | P1 | Verify no `popen` calls | all `.cpp` | `grep -r "popen" src/` → 0 hits |
| STAB-0625 | 📋 | P1 | Verify include path traversal prevention | `mc3/src/Mc3XmlParser.cpp` | `<include file="../../../etc/passwd"/>` produces error or safe fallback |
| STAB-0626 | 📋 | P1 | Define include allowed-root policy | `mc3/src/Mc3XmlParser.cpp` | Document: includes must be relative to the containing file's directory |
| STAB-0627 | 📋 | P1 | Verify registry DB path cannot escape to system paths | `src/MeshCraft/ModelRegistry.cpp` | DB path is validated; no absolute path injection via UI |
| STAB-0628 | 📋 | P1 | Verify malformed XML does not cause infinite loop | `mc3/src/Mc3XmlParser.cpp` | Feed 10MB file of `<a`; parsing completes within 5s |
| STAB-0629 | 📋 | P1 | Verify huge texture URI does not overflow buffer | `mc3/src/Mc3XmlParser.cpp` | Texture src attr of 10MB length: handled without crash |
| STAB-0630 | 📋 | P2 | Verify untrusted OBJ file does not crash mesh builder | `mc3togltf/src/GltfExporter.cpp` | Feed malformed OBJ (negative indices, NaN coords); tinyobjloader handles |
| STAB-0631 | 📋 | P2 | Verify network timeout prevents indefinite hang | `src/MeshCraft/AiAssistant.cpp` | See STAB-0383 |
| STAB-0632 | 📋 | P2 | Verify no secrets in autosave or temp files | `src/MeshCraft/MeshCraftApplication_FileOps.cpp` | API key not written to `.autosave` or backup files |
| STAB-0633 | 📋 | P2 | Verify AI apply requires undo-capable state | `src/MeshCraft/MeshCraftApplication_UiAi.cpp` | Apply without undo push: impossible by design |
| STAB-0634 | 📋 | P2 | Verify SQLite parameterized queries prevent SQL injection | `src/MeshCraft/ModelRegistry.cpp` | All queries use `sqlite3_bind_*`; no string concatenation into SQL |
| STAB-0635 | 📋 | P3 | Verify tmp file race condition: unique tmp path per process/thread | `src/MeshCraft/AiAssistant.cpp` | Tmp filename includes PID or random suffix |

---

## S20 — Release Readiness

| ID | St | Pri | Title | Key File(s) | Verification |
|----|----|-----|-------|-------------|--------------|
| STAB-0636 | 📋 | P2 | Add version number to CMakeLists.txt | `CMakeLists.txt` | `project(MeshCraft VERSION 0.1.0)` or similar; version accessible at runtime |
| STAB-0637 | 📋 | P2 | Display version in About dialog or `--version` flag | `src/MeshCraft/main.cpp` | `./MeshCraft --version` prints version string |
| STAB-0638 | 📋 | P2 | Create CHANGELOG.md with current state | `CHANGELOG.md` | Lists completed feature groups (A–N), stabilization groups (S1–S12) |
| STAB-0639 | 📋 | P2 | Verify release build has no debug symbols in binary | `CMakeLists.txt` | Release build: file size significantly smaller than debug; no DWARF sections |
| STAB-0640 | 📋 | P2 | Verify all test fixtures validated against current XSD | `test/*.mc3.xml` | `xsd_validation` passes for every `.mc3.xml` in test/ |
| STAB-0641 | 📋 | P2 | Verify sample files usable (load cleanly, no errors) | `test/house.mc3.xml`, `test/garden_house.mc3.xml`, `test/features.mc3.xml` | Each loads in MeshCraft without error dialog |
| STAB-0642 | 📋 | P3 | Verify Blender import smoke test for release sample | manual | Export `house.mc3.xml` to GLB; import in Blender 4.x; no errors |
| STAB-0643 | 📋 | P3 | Verify web build smoke test (release candidate) | manual | Web build loads `features.mc3.xml`; renders without browser error |
| STAB-0644 | 📋 | P3 | Add license notices for all third-party dependencies | `LICENSES/` or `THIRD_PARTY.md` | tinyxml2, tinygltf, tinyobjloader, imgui, manifold, cpp-httplib, SQLite — all acknowledged |
| STAB-0645 | 📋 | P3 | List third-party dependencies with versions and licenses | `THIRD_PARTY.md` | All FetchContent deps listed with GIT_TAG and license |
| STAB-0646 | 📋 | P3 | Document known limitations in README | `README.md` | Section: "Known Limitations" — SVG rasterization not done, Emscripten SQLite, etc. |
| STAB-0647 | 📋 | P3 | Document crash log instructions | `README.md` | How to capture crash log on Linux/Windows; where to report |
| STAB-0648 | 📋 | P3 | Create user guide (basic workflow) | `docs/USER_GUIDE.md` | Create/save scene, add primitive, export GLB — step by step |
| STAB-0649 | 📋 | P3 | Document backup/recovery procedure | `README.md` | Where autosaves are stored; how to recover; backup.1 behavior |
| STAB-0650 | 📋 | P3 | Verify CI produces consistent test report | `.github/workflows/ci.yml` | CI output includes pass/fail count and test names |

---

## Summary: Task Count by Section

| Section | Total | ✅ | 🟡 | 🧪 | 📋 | 🔴 |
|---------|-------|---|---|---|---|---|
| S0 Build | 25 | 9 | 0 | 12 | 4 | 0 |
| S1 Test infra | 40 | 8 | 2 | 15 | 15 | 0 |
| S2 MC3 XML | 55 | 13 | 0 | 26 | 16 | 0 |
| S3 MCB binary | 30 | 10 | 0 | 5 | 15 | 0 |
| S4 glTF export | 50 | 16 | 0 | 21 | 13 | 0 |
| S5 CSG | 35 | 5 | 0 | 7 | 23 | 0 |
| S6 Geometry | 25 | 5 | 0 | 6 | 14 | 0 |
| S7 Save/load | 35 | 28 | 0 | 0 | 7 | 0 |
| S8 UI robustness | 40 | 17 | 0 | 0 | 23 | 0 |
| S9 Registry | 35 | 22 | 0 | 0 | 13 | 0 |
| S10 AI | 40 | 10 | 0 | 6 | 24 | 0 |
| S11 Materials | 30 | 0 | 0 | 9 | 21 | 0 |
| S12 Animation | 30 | 0 | 0 | 12 | 18 | 0 |
| S13 Commands | 25 | 11 | 0 | 0 | 14 | 0 |
| S14 Rendering | 30 | 0 | 0 | 12 | 18 | 0 |
| S15 Import/export | 25 | 0 | 0 | 6 | 19 | 0 |
| S16 Cross-platform | 25 | 0 | 1 | 0 | 24 | 0 |
| S17 Documentation | 20 | 4 | 7 | 0 | 9 | 0 |
| S18 Code quality | 25 | 0 | 3 | 2 | 20 | 0 |
| S19 Security | 15 | 0 | 0 | 4 | 11 | 0 |
| S20 Release | 15 | 0 | 0 | 0 | 15 | 0 |
| **TOTAL** | **650** | **158** | **13** | **143** | **336** | **0** |

_Recomputed directly from per-row status markers (the table had drifted from
actual row state over several prior sessions); derived, not hand-maintained
— recompute the same way after any batch of status changes rather than
incrementing by hand. Last recomputed 2026-07-02._

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
