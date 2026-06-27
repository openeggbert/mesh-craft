# NEXT.md — MeshCraft handoff document

_Last updated: 2026-06-27_

---

## 1. Project summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` scene format. It
uses Dear ImGui for its UI, SDL3 + OpenGL via the **CNA** runtime (a separate
repo — do NOT modify), and exports scenes to glTF/GLB via **mc3togltf**.

**Main goal:** reach a fully stabilized, test-covered codebase before adding
new features. All work is tracked in `plan.md` as STAB-XXXX tasks across
sections S0–S20, guarded by Gates 0–6.

**Current phase:** Stabilization — Gate 0 (Build) and Gate 1 (Format) are
complete. Gate 2 (Export) priority tasks are complete. Gate 3 (Editor safety)
is next.

**Key architectural decision:** `mc3/` and `mcb/` are pure C++ static libs
with no CNA/ImGui dependency. `mc3togltf/` is a standalone CLI + lib. CNA is
fetched as a sibling CMake subdirectory and must not be modified here.

---

## 2. Current status

### Build
- **Debug build:** clean (`cmake .. -DBUILD_TESTING=ON && ninja`)
- **Offline mode:** working (`cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON`)
- **Release build:** not yet verified (STAB-0002 pending)

### Tests
- **17/17 CTest tests pass** (~3.4 s total)
  1. `smoke_test`
  2. `xsd_validation`
  3. `mc3_registry`
  4. `mc3_roundtrip`
  5. `mc3_commands`
  6. `mcb_roundtrip`
  7. `mc3togltf_gltf`
  8. `mc3togltf_all_primitives`
  9. `mc3togltf_export_verification`
  10. `mc3togltf_large_scene`
  11. `mc3togltf_csg_strict`
  12. `mc3togltf_csg_export`
  13. `mc3togltf_csg_unsupported`
  14. `mc3togltf_csg_nested`
  15. `mc3togltf_instance_deform_cache`
  16. `mc3togltf_float_cache_key`
  17. `mc3togltf_large_scene_generated`

### Tools / libraries available
- `MeshCraft` — editor executable (builds; viewport not fully integrated)
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb`
- `mc3tomcb` / `mc3frommcb` — MCB binary serialization CLIs (no CTest yet)
- `Mc3` static lib — scene data, XML load/save
- `Mcb` static lib — binary serialization (McbWriter + McbReader)
- `mc3togltf_lib` static lib — GltfExporter, MeshBuilder, CsgEvaluator

### What works
- Full XML roundtrip for all 10 primitive types including edge cases (vec2 plane
  size, inner_radius disk, legacy attrs)
- MCB binary roundtrip for base scene + all N1-N7 extension types (svg, embed,
  script, sound, music, trigger, sceneState, meta)
- XSD validation for all test fixtures including N3-N7
- glTF/GLB export: all 12 primitives (zero-triangle check), animations
  (Bounce/Spin/Pulse), CSG including nested (union inside difference),
  materials, lights, cameras, instances, groups, OBJ mesh import
- SQLite asset registry

### What does NOT work yet
- EditorViewport not integrated into MeshCraftApplication render loop
- SVG texture rasterization not implemented (stub only)
- Embedded GLTF not resolved in GltfExporter
- `mc3tomcb`/`mc3frommcb` have no CTest coverage
- Release build not verified
- No CI/CD workflow

---

## 3. Recent changes (2026-06-27)

**New files:**
- `mcb/test/mcb_roundtrip_test.cpp` — MCB roundtrip CTest binary covering
  smoke + N1-N7 types (10 test functions)
- `test/n3_scripts.mc3.xml` — XSD fixture for N3 Lua scripts
- `test/n4_sounds_music.mc3.xml` — XSD fixture for N4 sounds + music
- `test/n5_triggers.mc3.xml` — XSD fixture for N5 triggers (all 4 step types)
- `test/n6_scene_states.mc3.xml` — XSD fixture for N6 scene states
- `test/n7_meta.mc3.xml` — XSD fixture for N7 meta map
- `test/csg_nested.mc3.xml` — nested CSG fixture (union inside difference)
- `mc3togltf/test/csg_nested_test.py` — nested CSG export test script

**Modified files:**
- `mc3/test/roundtrip_test.cpp` — added 9 new test functions:
  testAllPrimitiveTypes, testPlaneSizeVec2, testPlaneSizeLegacyVec3,
  testDiskInnerRadius, testTorusMinorRadius, testCapsuleRadiusHeight,
  testIcoSphereSegments, testGridSubdivisions, testUnknownTopLevelElement
- `mc3togltf/CMakeLists.txt` — FetchContent guards for tinygltf/tinyobjloader;
  `mc3togltf_csg_nested` CTest target added
- `mc3togltf/test/all_primitives_export_test.py` — added indices count > 0
  and `% 3 == 0` check; node count >= object count assertion
- `mcb/CMakeLists.txt` — `mcb_roundtrip_test` executable + CTest target
- `test/features.mc3.xml` — extended with meta, scripts, sounds, music,
  triggers, states sections
- `plan.md`, `STABILIZATION.md` — gate status updated

---

## 4. Current blocker / main problem

**No hard blocker.** All 17 tests pass and build is clean.

The next gate (Gate 3 — Editor safety, STAB-0261–0335) requires writing tests
for editor command undo/redo, dialog lifecycle, and autosave behavior. These
involve `MeshCraftApplication` which depends on CNA (SDL3/OpenGL). Running
the editor in a headless test environment may need a stub or offscreen context.

---

## 5. Known bugs and limitations

- **SVG rasterization** (incomplete): parsed and serialized but not rasterized
  to PNG for glTF export
- **Embedded GLTF** (incomplete): parsed/serialized but not resolved/inlined
  by GltfExporter
- **EditorViewport not integrated** (incomplete): renderer exists but not wired
  into MeshCraftApplication
- **mc3tomcb CLI** (needs verification): binary tools exist, no CTest coverage
- **Release build** (needs verification): STAB-0002 never explicitly run
- **No CI** (incomplete): `.github/workflows/ci.yml` not created
- **AI tests need network** (incomplete): mock layer needed for STAB-0371–0376
- **Include tracking** (suspected): svgTextures/embeds maps may not track
  which entries came from `<include>` files

---

## 6. Architecture notes

```
MeshCraft (editor exe)
├── CNA (SDL3/OpenGL runtime — sibling repo, DO NOT MODIFY)
├── Mc3 (static lib — mc3/): Mc3Document, XML parser/writer, all types
├── Mcb (static lib — mcb/): McbWriter, McbReader, MCB binary format v1
├── mc3togltf_lib (static lib): GltfExporter, MeshBuilder, CsgEvaluator
└── mc3togltf (CLI exe): thin wrapper around mc3togltf_lib
```

**Hard constraints:**
- `Mc3Document` public API: do NOT change without checking `mc3togltf` and all
  test XMLs
- `${meta-gl_SOURCE_DIR}/include` must NOT be added to CMakeLists.txt (triggers
  full CNA recompile)
- CNA source files must NOT be modified from this repo
- `file(GLOB_RECURSE)` is used for sources — adding `.cpp` files requires
  `cmake ..` reconfigure
- MCB format `MCB_VERSION = 1` in `McbFormat.hpp` — bump on breaking wire change
- XSD element order at root: `include → metadata → meta → environment → lights
  → cameras → textures → materials → embeds → scripts → sounds → music →
  triggers → states → definitions → objects → actions`

**Stabilization gates:**
- Gate 0 (Build): ✅
- Gate 1 (Format): ✅ priority items
- Gate 2 (Export): ✅ priority items
- Gate 3 (Editor safety): next
- Gate 4 (Registry/AI): pending
- Gate 5 (Large scene): pending
- Gate 6 (Documentation): pending

---

## 7. Useful commands

```bash
# Configure (from repo root, build dir: cmake-build-debug/)
cd cmake-build-debug
cmake .. -DBUILD_TESTING=ON
cmake .. -DBUILD_TESTING=ON -DFETCHCONTENT_UPDATES_DISCONNECTED=ON  # offline

# Build
ninja
ninja mc3togltf
ninja mc3_roundtrip_test
ninja mcb_roundtrip_test

# Run tests
ctest --output-on-failure
ctest -N                                    # list all 17 tests
ctest -R mcb_roundtrip --output-on-failure
ctest -R mc3togltf_csg --output-on-failure

# Export a scene
./mc3togltf/mc3togltf ../test/features.mc3.xml /tmp/out.glb

# Validate XML
cd .. && python3 test/validate_xsd.py mc3/mc3.xsd test/features.mc3.xml

# Verify release build (STAB-0002)
cmake -S .. -B b-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build b-release -j4
```

---

## 8. Next smallest tasks

1. **STAB-0002** — Verify release build  
   Command: `cmake -S .. -B b-release -DCMAKE_BUILD_TYPE=Release && cmake --build b-release -j4`

2. **STAB-0005** — Verify `mc3` standalone build  
   Command: `cmake -S mc3 -B mc3-build -DBUILD_TESTING=ON && cmake --build mc3-build`

3. **STAB-0006** — Verify `mc3togltf` standalone build  
   Command: `cmake -S mc3togltf -B togltf-build && cmake --build togltf-build`

4. **STAB-0007** — Verify `mcb` standalone build  
   Command: `cmake -S mcb -B mcb-build && cmake --build mcb-build`

5. **STAB-0058** — Add CTest for `mc3tomcb` CLI (binary round-trip)  
   Files: new Python test, `CMakeLists.txt`  
   Verify: `ctest -R mc3tomcb`

6. **STAB-0025** — Create `.github/workflows/ci.yml` skeleton  
   Files: `.github/workflows/ci.yml`

7. **STAB-0279** — Verify editor commands are undoable  
   Files: `mc3/test/editor_commands_test.cpp`  
   Verify: `ctest -R mc3_commands`

8. **STAB-0265** — Verify autosave writes `.autosave` file on change  
   Files: `src/MeshCraft/MeshCraftApplication.cpp`, new test or manual check

---

## 9. Do not do yet

- **No new scene format features** — N1-N7 complete; further schema additions
  need design discussion
- **No CNA source changes** — separate Claude Code instance owns CNA
- **No `Mc3Document` public API changes** without checking `mc3togltf` + all XMLs
- **No `${meta-gl_SOURCE_DIR}/include`** in CMakeLists.txt
- **No SVG rasterization** — blocked on library choice (librsvg vs NanoSVG)
- **No AI/network tests** until mock layer exists
- **No mass refactoring** of passing code
- **No new features** until Gate 3 editor safety tasks are done

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect only the files needed for the first task in
section 8. Do not refactor unrelated code. Make one small, verified
improvement. Run `ctest --output-on-failure` from cmake-build-debug/ to
confirm all 17 tests still pass. Update NEXT.md after finishing.

Current branch: develop
Build dir: cmake-build-debug/
Active plan: plan.md (STAB-XXXX tasks, Gate 3 next)
Last commit: 05496d7 — stab: Gate 0-2 stabilization
```
