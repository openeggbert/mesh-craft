# Mesh Craft

A 3D modeling and editing application for creating game-ready models, inspired by Ray Dream Studio and Google SketchUp.

![MeshCraft screenshot](mesh_craft_screenshot.png)

## Description

Mesh Craft is a C++23 application for building and editing 3D models from primitive shapes, groups, materials, and constructive solid geometry (CSG). Models are stored in the `.mc3` format — a human-readable YAML-based source format that can be compiled to glTF/GLB for use in games and real-time applications.

## The MC3 Format

MC3 (Mesh Craft 3D) is the native source format used by Mesh Craft. It describes models as editable constructive objects rather than raw triangle meshes.

Key capabilities:

- primitive shapes: box, sphere, cylinder, cone, plane
- hierarchical groups and transforms
- materials and textures (PBR-style)
- CSG operations: union, difference, intersection
- reusable definitions (prefabs)
- object actions and states for interactive/animated models
- export target: glTF/GLB

See [mc3-format.md](mc3-format.md) for the full specification.

## Dependencies

Mesh Craft is built on top of:

- **CNA** — XNA-like C++ runtime (windowing, input, audio, graphics abstraction)
- **sharp-runtime** — C++ sharp/System layer used by CNA
- **Nova3D** — 3D engine built on top of CNA (used for 3D viewport rendering)

CNA and sharp-runtime APIs are publicly exposed through the Mesh Craft public API, unlike other applications in this family.

## Building

### Prerequisites

- CMake 3.21 or newer
- C++23-capable compiler (GCC 13+, Clang 16+, MSVC 2022+)
- The following sibling repositories checked out next to `mesh-craft`:
  - `cna`
  - `sharp-runtime`
  - `nova-3d`

### Build (Linux / macOS)

```sh
cmake -S . -B build
cmake --build build
```

### Build with SDL_RENDERER backend

```sh
cmake -S . -B build -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER
cmake --build build
```

### Run

```sh
./build/MeshCraft
```

## Development Status

Early prototype. The project structure and core class skeletons are in place. No rendering or full MC3 parsing is implemented yet.

## Planned Features

- Primitive creation tools: box, sphere, cylinder, plane, cone
- Transforms: position, rotation (XYZ extrinsic Euler), scale, pivot
- Materials and textures (PBR)
- CSG operations: union, difference, intersection
- Object hierarchy and grouping
- Object actions and animations (opening doors, moving objects, etc.)
- Export to glTF/GLB
- 3D editor UI:
  - main 3D viewport
  - object hierarchy panel
  - properties and material panel
  - tool palette
  - transform gizmo (move / rotate / scale)
  - primitive creation tools
  - CSG operation tools
  - texture and material assignment
  - action and animation editor

## License

MIT — see [LICENSE](LICENSE).
