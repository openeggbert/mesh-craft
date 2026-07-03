# User Guide — Basic Workflow

This walks through the smallest complete loop: create a scene, add an
object, move it, save, and export to glTF. Every menu path and shortcut
below is taken directly from the editor's own menu code
(`MeshCraftApplication_UiMenuBar.cpp`), not guessed.

See `README.md` for build/launch instructions and `MC3_FORMAT.md` for
the full `.mc3.xml` format reference. This guide only covers the
editor UI, not the CLI tools (`mc3togltf`, `mc3tomcb`).

## 1. Launch the editor

```sh
./cmake-build-debug/MeshCraft
```

Starting with no arguments opens an empty scene. You can also launch
directly into an existing file: `./MeshCraft path/to/scene.mc3.xml`.

## 2. Start a new scene (if one is already open)

**File → New** (`Ctrl+N`). If the current scene has unsaved changes,
you'll be asked to confirm before it's discarded.

## 3. Add a primitive object

**Add → Box** (`F1`), or any other primitive in the same menu
(Sphere is `F2`, and so on). The new object appears in the 3D viewport
and is selected automatically, and in the **hierarchy panel** on the
left.

## 4. Move it

With the object selected, switch to the Move tool — click the "Move"
button in the toolbar, or press `G` — then drag one of the gizmo's
axis arrows in the viewport. The **Properties panel** on the right
also lets you type exact position/rotation/scale values directly.

Other transform tools use the same one-letter pattern: `Q` Select,
`G` Move, `R` Rotate, `S` Scale.

## 5. Save the scene

**File → Save** (`Ctrl+S`). The first save on a new scene opens the
**Save As** dialog (**File → Save As...**, `Ctrl+Shift+S`) since there's
no filename yet — type a path ending in `.mc3.xml`.

Every subsequent save rotates a 2-slot backup
(`yourscene.mc3.xml.backup.1`/`.backup.2`) next to the file — see
`README.md`'s "Backup and Recovery" section if you ever need to
recover from one.

## 6. Export to glTF/GLB

**File → Export GLB** (`Ctrl+E`). This requires the scene to already
be saved (it derives the default output path from the current
filename) — if you haven't saved yet, save first. A dialog opens
pre-filled with the derived output path (same name, `.glb` or `.gltf`
extension depending on the format toggle in that dialog); confirm to
export.

The exported file is a standard glTF/GLB you can open in Blender, a
glTF viewer, or any engine that imports glTF.

## Next steps

- **Undo/redo**: every mutating command (add, delete, transform, ...)
  pushes an undo entry — `Ctrl+Z`/`Ctrl+Y` (Edit menu also has
  "Undo History..." to see the stack).
- **Materials**: assign one in the Properties panel once you've
  created a material (Materials panel, not covered here).
- **Groups, instances, CSG booleans, animation**: see `MC3_FORMAT.md`
  for what the format supports — the editor menus (Edit → Group,
  Add → Union/Difference/Intersection, the Timeline panel) expose all
  of it, but a full tour is out of scope for this quick-start guide.
- **AI Assistant** and **Model Registry**: both have their own panels
  (View menu) — see `m1m2m3.md` for how they work and their current
  limitations.
