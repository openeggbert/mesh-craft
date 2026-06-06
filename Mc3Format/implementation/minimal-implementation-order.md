# Recommended Minimal Implementation Order

For CNA/Nova-3D, implement in this order:

1. XML parser and validation.
2. `<box>`, `<sphere>`, `<cylinder>` primitives.
3. Basic transforms: `position`, `rotation`, `scale`.
4. Materials with `<base_color>`.
5. Textures and simple UV mapping.
6. Groups and hierarchy.
7. Instances and definitions.
8. Actions: `rotate`, `translate`, `set_state`.
9. Collision metadata.
10. CSG `<union>`.
11. CSG `<difference>`.
12. CSG `<intersection>`.
13. Export to GLB/glTF.
14. Editor integration.
