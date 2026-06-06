# Groups and Hierarchy

Groups allow multiple objects to share a transform. Child objects are placed directly inside the `<group>` element.

```xml
<group name="Table" position="0 0 0">
  <box name="TableTop" size="2 0.1 1" position="0 1 0" material="wood"/>
  <box name="Leg1" size="0.1 1 0.1" position="-0.9 0.5 -0.4" material="wood"/>
  <box name="Leg2" size="0.1 1 0.1" position="0.9 0.5 -0.4" material="wood"/>
  <box name="Leg3" size="0.1 1 0.1" position="-0.9 0.5 0.4" material="wood"/>
  <box name="Leg4" size="0.1 1 0.1" position="0.9 0.5 0.4" material="wood"/>
</group>
```

Transforms are hierarchical:

```text
world_transform = parent_transform * local_transform
```

All primitive objects may also have child objects, making them implicit groups. Use `<group>` when no geometry of its own is needed at the parent level.
