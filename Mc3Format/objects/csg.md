# Constructive Solid Geometry (CSG)

MC3 supports CSG operations for creating objects from primitives.

Required CSG operations:

- `union`
- `difference`
- `intersection`

## Union

Combines objects into one generated solid/mesh. Children are placed directly inside `<union>`.

```xml
<union name="SimpleHouseBody">
  <box name="MainBlock" size="8 3 6" position="0 1.5 0" material="plaster"/>
  <box name="RoofBlock" size="8.5 1 6.5" position="0 3.5 0" rotation="0 0 45" material="roof_tile"/>
</union>
```

## Difference

Subtracts one or more child shapes from the first child.

```xml
<difference name="WallWithDoorHole">
  <box name="WallBase" size="5 3 0.3" position="0 1.5 0" material="brick"/>
  <box name="DoorHole" size="1 2.2 0.5" position="0 1.1 0" role="cutter" visible="false"/>
</difference>
```

Meaning:

```text
result = WallBase - DoorHole
```

The `role="cutter"` attribute explicitly marks an object as a subtraction volume. This is a reserved role; importers must treat any object with `role="cutter"` as a CSG subtracter regardless of its material. Setting `visible="false"` on cutter objects is recommended. The material assigned to a cutter object has no effect on the result.

## Intersection

Keeps only the overlapping volume.

```xml
<intersection name="RoundedPartApproximation">
  <box size="2 2 2"/>
  <sphere radius="1.4"/>
</intersection>
```

## Implementation Notes

For version 0.1, CSG may be implemented in one of two ways:

1. Real mesh boolean operations.
2. Delayed representation converted by an external CSG library/tool.

A minimal importer may initially support only non-CSG primitives and groups, then add CSG later.

Recommended behavior:

- CSG children should be evaluated in local coordinates.
- Materials from the first child should be used by default.
- Cutter objects should be identified by `role="cutter"` and should usually also set `visible="false"`.
- CSG results should be cacheable.

## CSG vs Runtime Objects

CSG is useful for generating static geometry such as walls with holes, windows, arches, tunnels, pipes, and cutouts.

Interactive objects should usually not be baked into the same CSG mesh if they need to move.

Good:

```text
WallWithDoorHole = wall minus door hole
Door = separate rotating object
```

Bad:

```text
WholeHouseWithDoor = one baked CSG mesh where the door is part of the wall
```

If an object needs actions, keep it as a separate named object.
