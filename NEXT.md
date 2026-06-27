# NEXT.md — MeshCraft Handoff Document

_Last updated: 2026-06-27 (N3 Lua scripts complete; N4–N7 mc3.xml schema extensions pending)_

---

## 1. Project Summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` format — a lightweight
XML-based scene description used by the OpenEggbert project. The editor provides a
Dear ImGui UI with orbit camera, object hierarchy, properties panel, timeline/animation,
gizmos, CSG boolean operations, extrude/path geometry, material editing, AI-assisted
scene generation, and a local model registry.

**Main goal:** Full-featured authoring tool for `.mc3.xml` scenes, analogous to a
stripped-down Blender for the MC3 format.

**Current phase:** Late feature implementation (~111 of ~130 plan.md tasks done).
The current work area is the N group: mc3.xml schema extensions (new data types:
SVG textures, embedded GLTF, Lua scripts, sound/music, triggers, scene states, metadata).
N1 (SVG textures) and N2 (embedded GLTF) are complete. N3–N7 remain.

**Important architectural decisions:**
- **CNA** (C#-like framework for C++) owns the OpenGL/input layer. A separate Claude Code
  instance manages CNA. Do not touch CNA headers or CMakeLists.txt entries that reference
  `${meta-gl_SOURCE_DIR}/include` (triggers full CNA recompile).
- **Mc3 library** (`mc3/`) is a standalone static library with no CNA/ImGui/SDL dependency.
  The XML parser (`Mc3XmlParser`) and writer (`Mc3XmlWriter`) are internal (`Internal::`
  namespace). All new schema features follow the same pattern: new `Mc3*.hpp` struct +
  parser routing + writer emission + MCB binary serialization + XSD update + roundtrip test.
- **MCB** (`mcb/`) is a compact binary serialization of `Mc3Document`. Every new field added
  to `Mc3Document` must also be added to `McbWriter.cpp` and `McbReader.cpp`.
- **`Mc3Document` public API** must not change without checking `mc3togltf` and all test XMLs.
  Adding new fields/maps is safe (additive). Removing or renaming existing fields is not.

---

## 2. Current Status

### Build
- **Clean build** — `ninja MeshCraft` succeeds with 0 errors, 0 warnings.
- Build directory: `cmake-build-debug/` (in-source, not committed).

### Tests
- **15/15 CTest tests pass** (2.67 s total):
  - `smoke_test` — MeshCraft binary starts and exits cleanly
  - `xsd_validation` — mc3.xsd validates against all test XMLs
  - `mc3_registry` — ModelRegistry SQLite test
  - `mc3_roundtrip` — full parser/writer round-trip including SVG textures, embeds
  - `mc3_commands` — 57 unit tests for editor command algorithms (CNA-free)
  - `mc3togltf_*` (10 tests) — GLTF export: primitives, CSG, instances, deform, cache

### Libraries / Tools
- `MeshCraft` — main editor binary
- `Mc3` — static lib (XML parser/writer, all mc3 data types)
- `Mcb` — static lib (binary serialization of Mc3Document)
- `mc3togltf_lib` — static lib (GLTF/GLB exporter)
- `mc3_roundtrip_test`, `mc3_commands_test`, `mc3_registry_test` — test binaries

### Recently implemented (this session)
- **L2** — PropertiesPanel extracted to its own class (`PropertiesContext` DI struct)
- **L3** — EditorAlgorithms.hpp (CNA-free) + 57 unit tests for batchRename/findReplace/arrayDuplicate
- **L4** — UiMenuBar.cpp: 8 repeated `std::function` tree-walk blocks → `walkAll`/`selectBy` lambdas
- **L5** — EditorViewport.cpp stub implemented: added `EditorCamera` member + `PickRay pickRay()` method
- **N1** — SVG texture: `Mc3SvgTexture` struct, `<texture type="svg">`, MCB, XSD, 10 roundtrip tests
- **N2** — Embedded GLTF: `Mc3EmbedGltf` struct, `<embeds>` section, MCB, XSD, 13 roundtrip tests
- **N3** — Lua scripts: `Mc3Script` struct, `<scripts>` section, MCB, XSD, 9 roundtrip tests

### What does not work yet
- N4–N7: Sound/music, triggers, document-level scene states, `<meta>` — schema types
  not yet defined; no parser/writer/MCB/XSD/tests.
- SVG → bitmap rasterization for GLTF export (N1 data model done; rasterization deferred).
- `EditorViewport` not yet integrated into `MeshCraftApplication` — app still holds its own
  `camera_` and `gizmo_` directly; `EditorViewport` is a standalone class with no callers yet.

---

## 3. Recent Changes

### Files added
- `mc3/include/MeshCraft/Mc3/Mc3SvgTexture.hpp` — SVG texture struct
- `mc3/include/MeshCraft/Mc3/Mc3EmbedGltf.hpp` — embedded GLTF struct
- `src/MeshCraft/EditorAlgorithms.hpp` — CNA-free batch-rename / find-replace / array-dup algorithms
- `mc3/test/editor_commands_test.cpp` — 57 unit tests for the algorithms above
- `include/MeshCraft/Scene/PropertiesPanel.hpp` + `src/MeshCraft/Scene/PropertiesPanel.cpp`
- `include/MeshCraft/Scene/SceneHierarchyPanel.hpp` + `src/MeshCraft/Scene/SceneHierarchyPanel.cpp`

### Files modified (key changes)
- `mc3/include/MeshCraft/Mc3/Mc3Document.hpp` — added `svgTextures`, `embeds` maps; `addSvgTexture()`, `addEmbed()`
- `mc3/src/Mc3XmlParser.cpp` — routes `type="svg"` to svgTextures; `parseEmbeds()` for `<embeds>` section
- `mc3/src/Mc3XmlWriter.cpp` — emits SVG textures with CDATA; emits `<embeds>` section
- `mc3/mc3.xsd` — `textureElementType` now mixed + optional uri + type/src attrs; new `embedsType`
- `mcb/src/McbWriter.cpp` + `McbReader.cpp` — SVG textures and embeds serialization
- `mc3/test/roundtrip_test.cpp` — +23 tests (SVG: 10, embed: 13)
- `mc3/CMakeLists.txt` — added `mc3_commands_test` target
- `src/MeshCraft/MeshCraftApplication_UiMenuBar.cpp` — L4 refactor (`walkAll`/`selectBy`)
- `include/MeshCraft/Editor/EditorViewport.hpp` + `src/MeshCraft/Editor/EditorViewport.cpp` — L5 implemented
- `include/MeshCraft/MeshCraftApplication.hpp` — `insertAnimKeyframes` signature: `initializer_list` → `vector`

---

## 4. Current Blocker / Main Problem

**No current blocker.** Build is clean, all 15 tests pass.

The next natural work is **N4** (Sound and music): adding `Mc3Sound` + `Mc3Music` structs and
`<sounds>`/`<music>` sections. This follows the exact same 9-step pattern as N1–N3 and has no
dependencies on incomplete or broken code.

---

## 5. Known Bugs and Limitations

- **incomplete** — N4–N7 (sound/music, triggers, scene states, meta) not yet implemented.
- **incomplete** — `EditorViewport` not integrated into `MeshCraftApplication`; `camera_` and `gizmo_`
  are still direct members of the app class. `EditorViewport::pickRay()` is unused in production code.
- **incomplete** — SVG-to-bitmap rasterization for GLTF export (noted in N1). The GLTF exporter
  silently skips SVG textures without error.
- **incomplete** — Embedded GLTF (`embed:` meshSource) not resolved by the GLTF exporter; only
  file-path meshSource values are handled.
- **needs verification** — MCB round-trip for N1/N2 new fields (`svgTextures`, `embeds`) has no
  dedicated test binary. McbWriter/McbReader compile and link cleanly but there is no test that
  writes then reads an MCB file containing SVG or embed data.
- **unknown** — XSD validation passes but it is unclear whether the validator exercises the new
  `embedsType` and `textureElementType` (mixed-content) changes fully.
- **technical debt** — `EditorAlgorithms.hpp` is header-only (`inline`) and lives in `src/MeshCraft/`
  (a private path), but the test in `mc3/test/` includes it via a cmake `target_include_directories`
  pointing at `${CMAKE_SOURCE_DIR}/src/MeshCraft`. This is slightly fragile.

---

## 6. Architecture Notes

### Module boundaries
```
MeshCraft (binary)
  └── depends on: Mc3, Mcb, mc3togltf_lib, CNA, ImGui, SDL, OpenGL

Mc3 (static lib)             — pure C++23, no CNA/ImGui/SDL/OpenGL
  mc3/include/               — public headers (Mc3Document, all Mc3* types)
  mc3/src/Mc3XmlParser.cpp   — tinyxml2-based parser (Internal:: namespace)
  mc3/src/Mc3XmlWriter.cpp   — tinyxml2-based writer (Internal:: namespace)
  mc3/src/Mc3Document.cpp    — builder methods (addTexture, addEmbed, etc.)

Mcb (static lib)             — binary format, depends on Mc3
  mcb/src/McbWriter.cpp      — serialize Mc3Document to binary stream
  mcb/src/McbReader.cpp      — deserialize binary stream to Mc3Document

mc3togltf_lib                — depends on Mc3, tinygltf
  mc3togltf/src/GltfExporter.cpp — converts Mc3Document → GLTF model
```

### New type pattern (used for N1/N2; must be followed for N3–N7)
Every new mc3.xml schema extension follows this 9-step checklist:
1. `mc3/include/MeshCraft/Mc3/Mc3Foo.hpp` — new struct
2. `Mc3Document.hpp` — `#include`, new `map<string, Mc3Foo>`, `addFoo()` declaration
3. `Mc3Document.cpp` — implement `addFoo()`
4. `Mc3XmlParser.cpp` — `parseFoos()` static function; call from `Mc3XmlParser::parse()`
5. `Mc3XmlWriter.cpp` — emit `<foos>` section in `Mc3XmlWriter::write()`
6. `McbWriter.cpp` — `writeFoo()` + `if (!doc.foos.empty())` block
7. `McbReader.cpp` — `readFoo()` + `else if (k == "foos")` branch
8. `mc3/mc3.xsd` — type definition + reference in root `<xs:sequence>`
9. `mc3/test/roundtrip_test.cpp` — `testFoo()` covering external, inline, coexistence

### Invariants
- Parser and writer are `Internal::` — callers use `Mc3Document::loadFromFile()` / `saveToFile()`.
- Map keys for all asset maps (textures, svgTextures, embeds, …) are always the `id` field.
- `includedTextures` / `includedMaterials` / `includedDefs` skip-sets exist for `<include>` support.
  New maps (svgTextures, embeds) do not yet have include-tracking — acceptable for now.
- `Mc3Object::meshSource` stores either a file path or `"embed:<id>"` — no separate struct field.

### CLAUDE.md hard constraints
- **No CNA changes without owner permission.**
- **Do not add `${meta-gl_SOURCE_DIR}/include` to CMakeLists.txt.**
- **Do not change `Mc3Document` public API** without checking `mc3togltf` and all test XMLs.
- **plan.md workflow:** ask user before each task; confirm yes/no; implement in priority order.

---

## 7. Useful Commands

```bash
# Configure (only needed once, or after CMakeLists changes)
cd cmake-build-debug
cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON -DBUILD_TESTING=ON

# Build everything
ninja MeshCraft

# Build Mc3 library and its tests only
ninja mc3_roundtrip_test mc3_commands_test

# Run all 15 tests
ctest --output-on-failure

# Run roundtrip tests with full XML fixture
./mc3/mc3_roundtrip_test ../test/features.mc3.xml

# Run editor command algorithm tests
./mc3/mc3_commands_test

# Run the editor (requires display)
./MeshCraft

# Convert a .mc3.xml to .glb
./mc3togltf/mc3togltf ../test/features.mc3.xml /tmp/out.glb
```

---

## 8. Next Smallest Tasks

In priority order (from plan.md group N):

### Task 1 — N4: Sound and music elements in mc3.xml
**Goal:** `Mc3Sound` + `Mc3Music` structs; `<sounds>` and `<music>` sections (no audio playback).
- Two maps: `doc.sounds` (`Mc3Sound`: id, src, loop=false) and `doc.musicTracks` (`Mc3Music`: id, src, loop=true)
- XML: `<sound id="…" src="…" loop="false"/>` and `<music id="…" src="…" loop="true"/>`
- MCB, XSD, roundtrip tests
- **Verify:** `ctest --output-on-failure`

### Task 3 — N5: `<trigger>` element
**Goal:** `Mc3Trigger` struct with ordered list of steps (play-action, play-sound, run-script, play-music).
- `<trigger id="intro"><play-action ref="walk_anim"/><play-sound ref="click"/></trigger>`
- `Mc3TriggerStep`: `type` enum (PlayAction/PlaySound/RunScript/PlayMusic) + `ref` string
- `Mc3Trigger`: `id` + `vector<Mc3TriggerStep>`
- MCB, XSD, roundtrip tests
- **Verify:** `ctest --output-on-failure`

### Task 4 — N6: Document-level `<state>` element
**Goal:** Named scene-wide configuration snapshots (distinct from per-object `Mc3ObjectState`).
- `Mc3ObjectOverride`: `id`, optional position/rotation/visible/material
- `Mc3SceneState`: `name` + `vector<Mc3ObjectOverride>`
- `<state name="night"><object-override id="lamp" visible="false"/></state>`
- MCB, XSD, roundtrip tests
- **Verify:** `ctest --output-on-failure`

### Task 5 — N7: `<meta>` element
**Goal:** Top-level key-value metadata, key/value as XML attributes (not sub-tags).
- Note: `doc.metadata` already exists for the old `<metadata><property name="…"/>` form.
  N7 adds `doc.meta` (separate map) for `<meta><metaentry key="…" value="…"/></meta>`.
- XSD, roundtrip tests
- **Verify:** `ctest --output-on-failure`

---

## 9. Do Not Do Yet

- **No CNA changes** — a separate instance manages the CNA layer.
- **No `EditorViewport` integration into `MeshCraftApplication`** — correct class, no callers;
  the refactor to replace `camera_`/`gizmo_` touches too many files for the current focus (N3–N7).
- **No SVG rasterization** — add `stb_image_write`/`lodepng` only after N3–N7 are done.
- **No GLTF exporter changes for SVG/embed** — keep `GltfExporter.cpp` stable for now.
- **No new plan.md groups** — complete N4–N7 before adding more scope.
- **No mass refactor** of `MeshCraftApplication_*.cpp` files.
- **No `Mc3Document` field removals or renames** — breaks `mc3togltf` and test XMLs.
- **No MCB format version bump** — MCB uses forward-compatible unknown-key skipping; new fields
  are backward-compatible without a version change.
- **No MCB round-trip test binary** yet — addressing the missing MCB test is lower priority
  than completing N3–N7.

---

## 10. Resume Prompt

```
Read NEXT.md first. Then implement the next task listed in section 8
(currently N4 — Sound and music elements in mc3.xml). Follow the exact 9-step
checklist in that section. Do not refactor unrelated code. After each
file change, verify: ninja mc3_roundtrip_test && ctest --output-on-failure
Confirm all 15 tests still pass, then update NEXT.md to reflect N4 done
and promote N5 to the top of section 8.
```
