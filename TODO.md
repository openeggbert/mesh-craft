# Mesh Craft — Next Steps

## 1. YAML parser for MC3

Integrate a YAML library (e.g. yaml-cpp) and implement
`Mc3Document::loadFromFile` and `saveToFile`.
Follow the field mapping defined in `mc3-format.md`.

## 2. Viewport rendering

Fill in `EditorViewport` to render the scene via the Nova3D `Renderer` API
(boxes, spheres, cylinders, ground plane, grid).

## 3. Mouse picking

Implement ray-cast from the viewport camera through the mouse cursor position
to select objects in the scene.

## 4. SelectTool — first concrete EditorTool

Create `src/MeshCraft/Editor/Tools/SelectTool` as the first concrete
`EditorTool` subclass. It should handle click-to-select and
drag-box multi-select.

## 5. Primitive creation tools

Add tools for placing box, sphere, cylinder, cone, and plane primitives
directly in the viewport (click-to-place or drag-to-size).

## 6. Transform gizmo rendering and dragging

Render the `TransformGizmo` axes in the viewport and implement
mouse-drag interaction for translate, rotate, and scale modes.

## 7. Scene hierarchy panel UI

Render the `SceneHierarchyPanel` as an actual UI tree
(object names, type icons, expand/collapse for groups).

## 8. Properties panel UI

Render the `PropertiesPanel` with editable fields for the selected object:
transform, material assignment, collision, tags, visible flag.

## 9. Material and texture editor

Add UI for creating and editing `Mc3Material` and `Mc3Texture` entries
and assigning them to objects.

## 10. CSG operations

Implement CSG union, difference, and intersection mesh evaluation.
Wire up `Mc3CsgOperation` to an actual CSG library or mesh boolean solver.

## 11. Actions and states

Implement `Mc3Action` and `Mc3State` data classes and an action editor panel
for defining animations such as opening doors and moving objects.

## 12. glTF/GLB export

Add export from `Mc3Document` to glTF/GLB for use in game engines.

## 13. Add `build/` to .gitignore

The build directory is currently untracked. Add it to `.gitignore`.
