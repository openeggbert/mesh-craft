# MeshCraft

A C++23 3D scene editor for the `.mc3.xml` format — a lightweight XML-based scene description used by the OpenEggbert project.

![MeshCraft screenshot](mesh_craft_screenshot.png)

## Description

MeshCraft lets you build and edit 3D scenes from primitive shapes, groups, materials, CSG boolean operations, and external OBJ meshes. Scenes are saved as `.mc3.xml` files and can be exported to glTF/GLB for use in games and real-time applications.

## The MC3 Format

MC3 (MeshCraft 3D) is an XML-based source format (`.mc3.xml`). It describes scenes as editable constructive objects rather than raw triangle meshes.

Key capabilities:

- primitive shapes: box, sphere, cylinder, cone, plane, torus, capsule, disk, grid, icosphere
- hierarchical groups and transforms (position, rotation, scale, pivot)
- materials: PBR (base color, roughness, metallic, emissive, normal/occlusion textures)
- CSG operations: union, difference, intersection (via Manifold)
- extrude along path (line, arc, helix, polyline, bezier)
- animation channels: position, rotation, scale, visibility, material color, deform
- reusable definitions (prefabs / instances)
- layers, tags, collision hints
- fog and environment settings
- export: glTF/GLB via `mc3togltf`; binary MCB via `mc3tomcb`

See [MC3_FORMAT.md](MC3_FORMAT.md) for the full specification.

## Architecture

| Component | Description |
|-----------|-------------|
| `mc3/` | Core library — `Mc3Document` data model, XML parser/writer, no graphics dependency |
| `mcb/` | MCB binary format read/write (fast runtime loading) |
| `mc3tomcb/` | CLI: convert `.mc3.xml` ↔ `.mcb` |
| `mc3togltf/` | CLI: export `.mc3.xml` → `.gltf` / `.glb` |
| `MeshCraft` | Editor application — Dear ImGui UI, orbit camera, gizmos, CSG, timeline |
| `../cna/` | XNA-like C++ runtime (SDL3 + OpenGL ES 3, sibling repo) |

## Building

### Prerequisites

- CMake 3.21 or newer
- C++23-capable compiler (GCC 13+, Clang 16+, MSVC 2022+)
- Python 3 + `lxml` (for XSD validation tests)
- Sibling repositories checked out next to `mesh-craft/`:
  - `cna/`
  - `sharp-runtime/`

### Build (Linux)

```sh
cmake -S . -B cmake-build-debug
ninja -C cmake-build-debug
```

### Build with SDL_RENDERER backend

```sh
cmake -S . -B cmake-build-debug -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER
ninja -C cmake-build-debug
```

### Run

```sh
./cmake-build-debug/MeshCraft path/to/scene.mc3.xml
```

### Export to glTF

```sh
./cmake-build-debug/mc3togltf/mc3togltf scene.mc3.xml scene.glb
```

### Convert to MCB

```sh
./cmake-build-debug/mc3tomcb/mc3tomcb scene.mc3.xml scene.mcb
```

### Tests

```sh
ctest -V --test-dir cmake-build-debug
```

## Current Features

- Full `.mc3.xml` scene load/save with XML roundtrip
- All primitive types rendered in 3D viewport
- Transform gizmos (move / rotate / scale), local/world space
- Object hierarchy panel with selection, multi-select, drag-and-drop
- Properties panel (geometry, material, transform), multi-edit
- Material editor (PBR: base color, roughness, metallic, emissive)
- CSG boolean operations (union, difference, intersection via Manifold)
- Extrude along path (arc, helix, polyline, bezier cross-sections)
- Animation timeline: keyframes, channels, cubic/step/linear interpolation
- Named layers, object tags, collision hints
- Fog, environment background, emissive bloom post-process
- First-person walk mode
- GLB export settings UI; headless screenshot mode
- Undo/redo (20-step deep-copy stack)
- Auto-save with rotating backups
- MCB binary format (`mc3tomcb`)
- glTF/GLB export (`mc3togltf`)

## Current Limitations

- Bloom post-process: pipeline runs (FBO + Gaussian blur + composite) but visual effect not always visible depending on scene emissive brightness
- Walk mode: floor collision at y=0 only; no collision with scene geometry
- Preferences dialog: only auto-save interval is persisted; snap/grid/theme not saved
- Headless screenshot: always writes PPM regardless of file extension
- MCB: compression flag reserved in header but not implemented
- CSG export to glTF: CSG boolean evaluation not implemented; exporting CSG scenes requires "Allow approximate CSG export" checkbox in the export dialog (children exported as separate meshes, geometrically incorrect)
- No automated UI tests; only XML roundtrip and smoke test

## License

MIT — see [LICENSE](LICENSE).
