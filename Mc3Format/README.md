# MC3 Model Source Format

Version: 0.1 draft  
Format type: human-readable source format for procedural / constructive 3D models  
Recommended file extension: `.mc3.xml`  
Recommended compiled/runtime export: `.glb` / `.gltf`

## Overview

MC3 is a simple XML-based source format for describing 3D models as editable objects rather than only as final triangle meshes.

Recommended pipeline:

```text
.mc3.xml  ->  MC3 compiler/importer  ->  mesh / scene graph / GLB
```

## Contents

- [Purpose and Goals](purpose-and-goals.md)
- [Top-Level Structure](top-level-structure.md)
- [Coordinates and Units](coordinates-and-units.md)
- [Versioning](versioning.md)
- [Best Practices](best-practices.md)
- [Future Extensions](future-extensions.md)

### Objects

- [Object Basics](objects/README.md) — common fields, default values
- [Primitive Objects](objects/primitives.md) — box, cube, sphere, cylinder, cone, plane, mesh
- [Groups and Hierarchy](objects/groups-and-hierarchy.md)
- [CSG Operations](objects/csg.md) — union, difference, intersection
- [Definitions and Instances](objects/definitions-and-instances.md)
- [Pivots](objects/pivots.md)
- [States](objects/states.md)
- [Collision and Physics](objects/collision.md)
- [Areas and Interaction Volumes](objects/areas.md)

### Materials and Textures

- [Materials](materials/materials.md)
- [Textures](materials/textures.md)
- [UV Mapping](materials/uv-mapping.md)

### Actions

- [Action Basics](actions/README.md)
- [Action Types](actions/action-types.md)
- [Triggers](actions/triggers.md)

### Implementation

- [Importer Behavior](implementation/importer-behavior.md)
- [C++ Runtime Mapping](implementation/cpp-mapping.md)
- [Minimal Implementation Order](implementation/minimal-implementation-order.md)

### Examples

- [Minimal Example](examples/minimal-example.md)
- [Complete House Example](examples/complete-house.md)

## Quick Look: Minimal MC3 File

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.1" model="House">

  <materials>
    <material id="brick">
      <base_color>0.8 0.2 0.1 1.0</base_color>
    </material>
    <material id="stone">
      <base_color>0.5 0.5 0.5 1.0</base_color>
    </material>
  </materials>

  <objects>
    <box name="Wall" size="10 3 0.3" position="0 1.5 0" material="brick"/>
    <cylinder name="Column" radius="0.3" height="3" position="2 1.5 0" material="stone"/>
  </objects>

</mc3>
```
