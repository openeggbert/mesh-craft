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

## 6. Import a GLB or glTF scene

Choose **File → Import GLB / glTF...**. A normal import accepts a
self-contained `.glb` and creates a selected editable MC3 group: its node
hierarchy, triangle primitives, transforms, PBR materials, embedded images,
cameras, and punctual lights are retained. The source is stored inside the
MC3 document, so later saves and exports do not depend on the original GLB.

For a textual `.gltf`, first enable **Trusted external .gltf import** in the
dialog. This is deliberately off by default. It reads only companion files in
the selected glTF's directory, applies size limits, then embeds the resolved
result into the MC3 document. Use it only for assets you trust. Skins, morph
targets, and animations are reported as lossy; non-triangle primitives are
rejected before anything is added to the scene.

## 7. Export to glTF/GLB

**File → Export GLB** (`Ctrl+E`). This requires the scene to already
be saved (it derives the default output path from the current
filename) — if you haven't saved yet, save first. A dialog opens
pre-filled with the derived output path (same name, `.glb` or `.gltf`
extension depending on the format toggle in that dialog); confirm to
export.

The exported file is a standard glTF/GLB you can open in Blender, a
glTF viewer, or any engine that imports glTF.

The export dialog offers only controls that the exporter can perform. Choose
**Quantize mesh attributes (16-bit)** to opt into
`KHR_mesh_quantization`: normals and tangents use signed normalized 16-bit
components (at most 1/32767 component error), UVs in the `[0, 1]` range use
unsigned normalized 16-bit components (at most 1/65535 error), and index
buffers with at most 65,536 vertices use lossless 16-bit indices. Positions
remain 32-bit floats. UVs outside `[0, 1]` (for example tiled UVs), and any
other attribute outside its supported range, stay 32-bit floats; the Export
Report identifies the affected MC3 object ID.

Use **Refresh pre-export estimate** to build the same scene representation the
exporter will write and show an approximate JSON overhead plus exact generated
geometry-buffer size. It is intentionally an estimate, not a byte-for-byte
promise, because the JSON serializer controls final formatting. A `.glb`
embeds readable texture bytes in its one file. A textual `.gltf` writes JSON
and a `.bin`: regular image paths remain external/rebased, while existing
inline `data:` images remain inline. Texture codecs and general-purpose
compression are not offered until their distribution and viewer-compatibility
policy has been decided.

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
