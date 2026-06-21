# MeshCraft Stabilization — Sub-tasks

**Goal:** Stabilize the MC3 format, export pipeline, and build without rewriting the editor.

**Audit findings (2026-06-20):**
- README says "YAML-based" — outdated; MC3 is XML
- `mc3togltf` has its own duplicate `Mc3XmlParser` (uses `source`; core uses `src`)
- Top-level CMake does not include `add_subdirectory(mc3togltf)` — tests reference hardcoded `mc3togltf/build/`
- XSD: `<plane>` has `size` as `vec2`, but the core parser reads it as vec3
- `mc3togltf` is not built from the root CMake

---

## Phase 1: Format — `src` vs `source` attribute

**S1** ✅ — Canonicalize mesh attribute to `src` in mc3togltf parser
- `mc3togltf/src/Mc3XmlParser.cpp:264`: `attr(el, "source")` → `attr(el, "src")`
- Core `mc3/src/Mc3XmlParser.cpp:281` already uses `src` ✓
- Core `mc3/src/Mc3XmlWriter.cpp:252` emits `src` ✓
- `mc3/mc3.xsd:320` defines `src` ✓
- `test/obj-test.mc3.xml`: `source=` → `src=` ✓

**S2** ✅ — Verify and unify plane size (vec2 vs vec3)
- XSD had vec2 for plane, parser read it as vec3 → `"8 8"` parsed to `{8,8,0}` → renderer used `size[2]=0` → zero-depth planes (silent bug)
- **Parser fix** (`Mc3XmlParser.cpp:parsePrimitive`): special-case `ObjectType::Plane`; canonical "W D" maps to `{W, 1, D}`; legacy "W 0 D" (3-value) maps to `{W, 0, D}` via existing path
- **Writer fix** (`Mc3XmlWriter.cpp`): plane case now writes `"size[0] size[2]"` (vec2) instead of vec3
- **`test/animation_test.mc3.xml`**: `size="2 0 2"` → `size="2 2"` (canonical)
- XSD: already correct (vec2Type for planeType.size), no change needed
- Roundtrip test: all 110 tests pass; `"50 50"` roundtrips correctly

---

## Phase 2: mc3togltf refactoring

**S3** ✅ — Remove duplicate parser from mc3togltf; use core mc3
- Deleted `mc3togltf/src/Mc3XmlParser.cpp` and `Mc3XmlParser.hpp` (duplicate 560-line parser)
- `main.cpp`: replaced `Mc3XmlParser::parse()` with `Mc3Document::loadFromFile()`
- `mc3togltf/CMakeLists.txt`: removed standalone tinyxml2 fetch + link (now used from `Mc3` lib)
- `GltfExporter.cpp` already consumed `Mc3Document` directly — no change needed
- Build clean; `house.mc3.xml` and `obj-test.mc3.xml` export correctly

**S4** ✅ — Add `mc3togltf` to top-level CMake
- `CMakeLists.txt`: added `add_subdirectory(mc3togltf)` after tinyobjloader fetch
- Removed hardcoded `mc3togltf/build/mc3togltf` test; mc3togltf/CMakeLists.txt registers its own test via `$<TARGET_FILE:mc3togltf>`
- `ninja mc3togltf` now works from root build dir; `./mc3togltf/mc3togltf` in cmake-build-debug

---

## Phase 3: Build and tests

**S5** ✅ — Verify top-level build: mc3 + mcb + mc3tomcb + mc3togltf + MeshCraft
- `ninja` from cmake-build-debug: all targets build clean, no errors
- Binaries confirmed: MeshCraft (36M), mc3tomcb (3.5M), mc3togltf (11M), mc3_roundtrip_test (3.1M)
- Smoke test: PASS; MCB roundtrip: OK

**S6** ✅ — Validate test XML files against XSD
- `xmllint` not available in build env; validator written in Python (lxml)
- `test/validate_xsd.py`: validates any number of mc3.xml files against mc3.xsd
- `CMakeLists.txt`: `xsd_validation` CTest target (requires python3 + lxml)
- All 11 test XML files pass validation
- Status: ✅

**S7** ✅ — Roundtrip test: load → save → reload → compare
- `mc3/test/roundtrip_test.cpp`: C++ CTest, 110 tests, covers all field types
- Already existed and passes; no new work needed beyond confirming it still passes after S1–S6

**S8** ✅ — mc3togltf export tests
- `mc3togltf/test/gltf_test.py`: tests basic conversion, animation export (3 actions), GLB magic bytes
- S4 already wired test to `$<TARGET_FILE:mc3togltf>` (top-level build binary)
- All 39 gltf_test assertions pass with top-level-built `cmake-build-debug/mc3togltf/mc3togltf`

---

## Phase 4: Primitives and CSG

**S9** ✅ — Audit primitive support in mc3togltf MeshBuilder
- **Originally**: Torus, Capsule, Disk, Grid, IcoSphere returned empty MeshData silently
- **Round 2**: each unimplemented type emitted a warning and was skipped
- **Round 3**: all five are now fully implemented in MeshBuilder and exported correctly
  - Disk supports solid (inner_radius=0) and ring (inner_radius>0) topology
  - Verified by `mc3togltf_all_primitives` and `mc3togltf_export_verification` CTests

**S10** ✅ — CSG export audit
- **Finding**: CSG nodes were silently exported as empty group nodes
- **Round 2 fix**: warning emitted when any CSG node encountered
- **Round 3 fix** (`GltfExporter.cpp`): all CSG nodes (union/difference/intersection) now **fail hard** by default with a descriptive error
  - Use `--allow-approximate-csg` (CLI) or the "Allow approximate CSG export" checkbox (editor) to export children separately
  - Verified by `mc3togltf_csg_strict` CTest

---

## Phase 5: Documentation

**S11** ✅ — Fix README
- Removed "YAML-based" → "XML-based (.mc3.xml)"
- Removed "Early prototype / No rendering" → accurate feature list
- Documented architecture table: mc3, mcb, mc3tomcb, mc3togltf, MeshCraft, CNA
- Fixed build instructions (ninja, cmake-build-debug paths)
- Added "Current Limitations" section
- Status: ✅

**S12** ✅ — Review and update MC3_FORMAT.md
- Created `MC3_FORMAT.md` (file did not exist)
- Documents all sections: root, environment, lights, cameras, textures, materials, objects, animations
- Canonical `src` attribute for `<mesh>` (not `source`)
- Plane `size` documented as vec2 (W × D)
- Export support matrix: which primitives/features mc3togltf handles vs. warns/skips
- Validation section pointing to `test/validate_xsd.py`

---

## Priority order

1. **S1** — src/source fix (small, safe change)
2. **S3** — remove duplicate parser (critical for consistency)
3. **S4** — mc3togltf into top-level CMake
4. **S5** — verify top-level build
5. **S2** — plane size
6. **S6** — XSD validation
7. **S11** — README
8. **S9** — primitives audit
9. **S7** + **S8** — roundtrip + export tests
10. **S10** — CSG
11. **S12** — MC3_FORMAT.md

---

## Audit notes

- `mc3/src/Mc3XmlParser.cpp:281` — core uses `src` ✓
- `mc3togltf/src/Mc3XmlParser.cpp:264` — togltf uses `source` ✗
- `mc3.xsd` plane: `vec2` — but core parser reads vec3 (watch backward compatibility)
- Top-level `CMakeLists.txt` does not include `add_subdirectory(mc3togltf)`
- `mc3togltf` builds standalone into `mc3togltf/build/`
