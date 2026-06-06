# Best Practices

- Give names to all objects that may be animated or referenced by actions.
- Use `id` attributes for stable tool references if object names may change.
- Mark CSG subtracters with `role="cutter"` and set `visible="false"`.
- Keep doors, drawers, chairs, crates, and buttons separate from static CSG geometry.
- Use pivots for rotating objects.
- Prefer top-level material definitions over inline materials.
- Use definitions for repeated objects.
- Use simple primitives first; add mesh references only when needed.
- Keep the source `.mc3.xml` as the editable truth.
- Treat `.glb` as compiled/exported output.
