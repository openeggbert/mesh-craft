# MeshCraft Stabilization Policy and Summary

_Last updated: 2026-06-27_

---

## Current Stabilization Policy

1. **No new features until stabilization gates S0–S6 are green.**
2. Every task in `plan.md` requires:
   - Acceptance criteria
   - Verification command or test
   - Status changed to ✅ only after test exists, is registered, runs, and passes
3. Status symbols: ✅ verified · 🟡 partial · 🧪 needs test · 📋 planned · 🔴 bug
4. Claude Code must ask before implementing any `plan.md` task (per CLAUDE.md workflow).
5. No CNA changes without owner permission.
6. No `${meta-gl_SOURCE_DIR}/include` in CMakeLists.txt.
7. No `Mc3Document` public API changes without checking `mc3togltf` and all test XMLs.

---

## Stabilization Gates

| Gate | Name | Condition |
|------|------|-----------|
| **Gate 0** | Build | All 15 CTest tests registered and passing; debug + release build verified |
| **Gate 1** | Format | MC3 XML + MCB roundtrip for all features (N1-N7); XSD fixtures for all elements |
| **Gate 2** | Export | All primitives/objects export; CSG real export; geometry reuse verified |
| **Gate 3** | Editor safety | Undo for all destructive ops; save/load/autosave/backup verified; no dialog lifecycle bugs |
| **Gate 4** | Registry/AI | AI mock tests; registry edge cases; non-SQLite/non-AI builds clean |
| **Gate 5** | Large scene | 500-object test; export stats correct; memory acceptable |
| **Gate 6** | Documentation | README, MC3_FORMAT, NEXT, STABILIZATION, m1m2m3 current and honest |

---

## Completed Stabilization Phases (prior to 2026-06-27)

### Phase 1 — Format (S1) ✅
- Canonicalized `src` vs `source` in mc3togltf parser
- Fixed plane `size` vec2 vs vec3 discrepancy

### Phase 2 — mc3togltf refactoring (S3, S4) ✅
- Removed duplicate `Mc3XmlParser` from mc3togltf
- Added mc3togltf to root CMakeLists.txt (`add_subdirectory(mc3togltf)`)

### Phase 3 — Build and tests (S5, S6, S7, S8) ✅
- Verified full top-level build: all targets
- Added `xsd_validation` CTest (Python/lxml)
- Confirmed `mc3_roundtrip` test (110 tests)
- Confirmed `mc3togltf_gltf` test (39 assertions)

### Phase 4 — Primitives and CSG (S9, S10) ✅
- Implemented Disk, Grid, IcoSphere, Torus, Capsule in MeshBuilder
- CSG real export (Manifold); strict mode default; approximate fallback with flag

### Phase 5 — Documentation (S11, S12) ✅
- Fixed README (XML-based, accurate features, correct build steps)
- Created MC3_FORMAT.md (canonical format specification)

### N1-N7 Schema Extensions ✅
- N1: SVG texture (Mc3SvgTexture, parser, writer, MCB, XSD, 10 tests)
- N2: Embedded GLTF (Mc3EmbedGltf, parser, writer, MCB, XSD, 13 tests)
- N3: Lua scripts (Mc3Script, parser, writer, MCB, XSD, 9 tests)
- N4: Sound/music (Mc3Sound, Mc3Music, parser, writer, MCB, XSD, 13 tests)
- N5: Triggers (Mc3Trigger, Mc3TriggerStep, parser, writer, MCB, XSD, 16 tests)
- N6: Scene states (Mc3SceneState, Mc3ObjectOverride, parser, writer, MCB, XSD, 15 tests)
- N7: Meta map (doc.meta, parser, writer, MCB, XSD, 10 tests)

---

## Current Test Suite (2026-06-27)

15 CTest tests, all passing (~2.7s total):

| Test | What it covers |
|------|---------------|
| `smoke_test` | Editor binary starts, opens scene, exits cleanly |
| `xsd_validation` | All `test/*.mc3.xml` validate against `mc3/mc3.xsd` |
| `mc3_registry` | ModelRegistry SQLite CRUD + migration |
| `mc3_roundtrip` | Full XML parser/writer roundtrip (all features) |
| `mc3_commands` | 57 unit tests for EditorAlgorithms |
| `mc3togltf_gltf` | Animation + house GLB export + magic bytes |
| `mc3togltf_all_primitives` | All 10 primitive types export without error |
| `mc3togltf_export_verification` | Node count, mesh presence, material names |
| `mc3togltf_large_scene` | Static 200-object scene export |
| `mc3togltf_large_scene_generated` | Python-generated 200-object test |
| `mc3togltf_csg_strict` | CSG fails hard without flag |
| `mc3togltf_csg_export` | Union/difference/intersection real export |
| `mc3togltf_csg_unsupported` | Unsupported CSG child detected |
| `mc3togltf_instance_deform_cache` | Deform cache key produces separate meshes |
| `mc3togltf_float_cache_key` | Close float dimensions don't collide |

---

## Known Gaps (as of 2026-06-27)

1. **No MCB binary roundtrip test** — N1-N7 MCB code compiles but no test verifies encode/decode (STAB-0121–0130)
2. **No XSD fixtures for N3-N7** — `xsd_validation` doesn't exercise new elements (STAB-0041–0046)
3. **AI tests require real network** — no mock test exists for AI pipeline (STAB-0371–0410)
4. **EditorViewport not integrated** — `EditorViewport` class complete but unused in `MeshCraftApplication`
5. **SVG rasterization not implemented** — N1 data model done; rasterization for GLTF export deferred
6. **Embedded GLTF not resolved in exporter** — `embed:` meshSource not handled by GltfExporter
7. **Include tracking missing** for `svgTextures` and `embeds` maps (N1-N2)
8. **Many UI behaviors untested** — undo, save/load, dialog lifecycle, picking (S7-S8)

---

## New Stabilization Backlog

See `plan.md` for the full 650-task stabilization backlog (STAB-0001 through STAB-0650).

Next tasks: STAB-0001, STAB-0004, STAB-0019, STAB-0121–0130, STAB-0041–0046.

See `NEXT.md` for operational next steps.
