# Purpose and Goals

## Purpose

MC3 is intended for models built from primitives, constructive solid geometry, reusable parts, materials, textures, transforms, groups, and object actions.

Typical use cases:

- houses, rooms, furniture, doors, windows, platforms, machines,
- simple game levels,
- block-based or SketchUp-like modeling,
- procedural assets,
- editable source models that can later be compiled to meshes,
- runtime-interactive objects such as doors, drawers, elevators, buttons, chairs, crates, and moving platforms.

MC3 is not meant to replace glTF as a final runtime asset format. Instead, MC3 should be treated as a source format that can be compiled or exported to glTF/GLB, CNA/Nova-3D internal meshes, or another runtime representation.

## Design Goals

MC3 should be:

- readable by humans,
- easy to generate by tools and AI,
- stable enough for long-term project files,
- simple enough to parse in C++,
- useful for both static and interactive models,
- independent of any specific graphics backend,
- compatible with future export to glTF/GLB,
- suitable as an input format for Nova-3D.

MC3 should avoid:

- storing low-level rendering state,
- storing backend-specific details such as OpenGL, Vulkan, or DirectX handles,
- requiring a full CAD kernel for the first version,
- becoming as complex as USD or Blender files.

## Summary

MC3 is a source format for describing 3D models as editable constructive objects.

It supports:

- primitive shapes,
- groups and hierarchies,
- materials and textures,
- UV mapping,
- CSG operations such as union, difference, and intersection,
- reusable definitions,
- object pivots,
- actions and states,
- interactive runtime behavior.

The recommended architecture is:

```text
MC3 XML source -> Nova-3D importer/compiler -> generated meshes and scene nodes -> optional GLB export
```

This keeps modeling simple and human-readable while still allowing the engine to use efficient runtime assets.
