# Importer / Compiler Behavior

An MC3 importer should perform these steps:

1. Parse XML.
2. Validate `<mc3>` root element and required attributes.
3. Load textures defined in `<textures>`.
4. Create materials from `<materials>`.
5. Resolve definitions from `<definitions>`.
6. Build object hierarchy from `<objects>`.
7. Generate primitive meshes.
8. Evaluate CSG where supported.
9. Generate UV coordinates.
10. Create collision shapes where supported.
11. Register actions and triggers from `<actions>`.
12. Export or create runtime scene objects.

## Error Handling

MC3 importers must define behavior for the following error conditions:

| Condition | Recommended response |
|---|---|
| Unknown attribute on known element | Warning, ignore attribute. |
| Unknown element type (unknown object type) | Warning, skip element. |
| Missing required attribute on object | Error, skip object. |
| `<instance>` references unknown `definition` | Error, skip instance. |
| Action `target` references unknown object name or ID | Warning, skip action. |
| Object references unknown material `id` | Warning, use default material. |
| Material references unknown texture `id` | Warning, use fallback texture. |

**Missing definition reference**: When an `<instance>` references a definition `id` that does not exist in `<definitions>`, the importer must report an error and skip that instance. The rest of the document must continue loading.

**Missing action target**: When an action's `target` references an object name or ID that does not exist in the scene, the importer must report a warning and skip that action. The rest of the document must continue loading.

**Missing material reference**: When an object references a material that is not defined in `<materials>`, the importer must report a warning and substitute a default material (for example, a flat grey diffuse material).

**Missing texture reference**: When a material references a texture that is not defined in `<textures>`, the importer must report a warning and use a fallback texture (for example, a 1×1 white pixel).
