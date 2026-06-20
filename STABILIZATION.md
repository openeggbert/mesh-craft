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

**S2** — Verify and unify plane size (vec2 vs vec3)
- XSD has `vec2`, core parser reads vec3 (lines 177–184 of Mc3XmlParser.cpp)
- Decision: plane is 2D, size = vec2 (width × depth = X × Z)
- Fix: parser, writer, XSD, test XML files
- Status: 📋

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

**S6** — Validate test XML files against XSD
- Add CMake/CTest target for `xmllint --schema mc3.xsd`
- Fix test files if they fail validation
- Status: 📋

**S7** — Roundtrip test: load → save → reload → compare
- Add a C++ test or CMake/ctest script
- Compare semantic fields (not byte-for-byte)
- Status: 📋

**S8** — mc3togltf export tests
- Export simple scenes (box, sphere, cylinder…) to .gltf and .glb
- Verify test suite uses top-level-built binary
- Status: 📋

---

## Phase 4: Primitives and CSG

**S9** — Audit primitive support in mc3togltf MeshBuilder
- Which primitives MeshBuilder supports: Box, Sphere, Cylinder, Cone, Plane, Torus, Capsule, Disk, Grid, IcoSphere
- Unimplemented ones: emit a clear error instead of silently ignoring
- Status: 📋

**S10** — CSG export audit
- Determine whether mc3togltf evaluates CSG (union/difference/intersection) or ignores it
- Either implement (Manifold?) or add a clear error message
- Status: 📋

---

## Phase 5: Documentation

**S11** — Fix README
- Remove "YAML-based" → "XML-based (.mc3.xml)"
- Document architecture: mc3, mcb, mc3tomcb, mc3togltf, MeshCraft editor
- Fix build instructions (working cmake commands)
- Add "Current limitations" section
- Status: 📋

**S12** — Review and update MC3_FORMAT.md
- Canonical `src` attribute for mesh
- Plane size = vec2
- Supported primitives list
- CSG export status
- Status: 📋

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
