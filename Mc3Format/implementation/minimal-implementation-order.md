# Recommended Minimal Implementation Order

For CNA/Nova-3D, implement in this order:

1. XML parser and validation.
2. `<box>`, `<sphere>`, `<cylinder>` primitives.
3. Basic transforms: `position`, `rotation`, `scale`.
4. Materials with `<base_color>`.
5. Textures and simple UV mapping.
6. Groups and hierarchy.
7. Instances and definitions.
8. `<deform>` geometry-level non-uniform scale.
9. Actions: `rotate`, `translate`, `set_state`.
10. `<environment>`: background color and fog.
11. `<lights>`: ambient, directional, point, spot.
12. `<cameras>`: perspective and orthographic.
13. Collision metadata.
14. `<extrude>` with `line` and `arc` paths.
15. `<extrude>` with `helix` path.
16. `<extrude>` with bezier and polyline paths.
17. CSG `<union>`.
18. CSG `<difference>`.
19. CSG `<intersection>`.
20. Export to GLB/glTF.
21. Editor integration.
