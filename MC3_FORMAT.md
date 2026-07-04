# MC3 Format Specification

MC3 (MeshCraft 3D) is an XML-based scene format (`.mc3.xml`). It describes a 3D scene as editable constructive objects rather than raw triangle meshes.

---

## Root element

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.1" model="MyScene" unit="meter" coordinate_system="right_handed_y_up">
  ...
</mc3>
```

| Attribute | Values | Default |
|-----------|--------|---------|
| `version` | `"0.1"` | `"0.1"` |
| `model` | string | `"unnamed"` |
| `unit` | `"meter"`, `"centimeter"`, `"inch"` | `"meter"` |
| `coordinate_system` | `"right_handed_y_up"` | `"right_handed_y_up"` |

---

## Top-level sections

```xml
<mc3 ...>
  <include file="..."/>
  <meta>...</meta>
  <environment>...</environment>
  <lights>...</lights>
  <cameras>...</cameras>
  <textures>...</textures>
  <materials>...</materials>
  <scripts>...</scripts>
  <sounds>...</sounds>
  <music>...</music>
  <triggers>...</triggers>
  <states>...</states>
  <definitions>...</definitions>
  <objects>...</objects>
  <actions>...</actions>
</mc3>
```

All sections are optional, but **the XSD schema (`mc3/mc3.xsd`) requires
whichever sections are present to appear in this exact order** —
`<objects>` before `<materials>`, for example, fails schema validation
even though `Mc3XmlParser` itself is lenient about order. This matters
in practice: the AI Assistant's "Apply to Scene" pipeline validates
AI-generated XML against this schema (see MESHCRAFT_HAS_LIBXML2 in
`AiResponseAlgorithms.hpp`) and rejects out-of-order responses.

---

## Include (`<include>`)

Lets one `.mc3.xml` file pull shared **definitions, materials, and
textures** from another file, so an asset library can be maintained once
and reused across multiple scenes.

```xml
<!-- scene.mc3.xml -->
<mc3 version="0.3" model="MyScene">
  <include file="furniture_library.mc3.xml"/>
  <include file="materials_pbr.mc3.xml"/>
  <objects>
    <instance name="Chair1" definition="chair" position="0 0 0"/>
  </objects>
</mc3>
```

| Attribute | Type | Required |
|-----------|------|----------|
| `file` | string (path) | yes |

**What gets merged** — only `<definitions>`, `<materials>`, and
`<textures>` from the included file. Everything else in an included file
(`<objects>`, `<environment>`, `<lights>`, `<cameras>`, `<actions>`,
`<scripts>`, `<sounds>`, `<music>`, `<triggers>`, `<states>`, `<meta>`) is
**silently ignored** — an included file is treated purely as an asset
library, never as a sub-scene.

**Merge order and local-override policy:** includes are merged **before**
the including file's own `<definitions>`/`<materials>`/`<textures>` are
parsed, and later parsing simply overwrites same-`id` map entries — so
**a local entry with the same `id` as an included one always wins**,
with no error or warning. This lets a scene "override" one asset from a
shared library without forking the whole library file.

**Path resolution:** `file` is resolved **relative to the file that
contains the `<include>` element**, not relative to the top-level scene
file. So if `a.mc3.xml` includes `libs/b.mc3.xml`, and `b.mc3.xml`
itself includes `c.mc3.xml`, `c.mc3.xml`'s path is resolved relative to
`libs/`, not to `a.mc3.xml`'s directory.

**Nested includes and merge order:** an included file's own `<include>`s
are processed **before** that file's own definitions/materials/textures
are merged (depth-first, pre-order) — so in the `a → b → c` chain above,
`c`'s assets are merged first, then `b`'s own assets, then (after `a`
processes any of its other top-level includes) `a`'s own local content
last. Only the top-level file's `<include>` elements are recorded in the
saved output (`doc.includes`) — nested includes are followed and merged
but not themselves re-emitted as separate `<include>` elements.

**Diamond includes** (e.g. `A→B`, `A→C`, `B→D`, `C→D`): each included
file is merged **exactly once**, tracked by a "processed" set keyed on
the file's canonical (resolved, absolute) path — the second and later
times `D` is reached, it's silently skipped rather than merged twice or
erroring.

**Cycle detection:** a "currently being processed" set tracks the
include chain's call stack (depth-first). If a file's canonical path is
already in that set when it's reached again (e.g. `A` includes `B`, `B`
includes `A`), parsing throws
`std::runtime_error("Cyclic <include> detected: ...")` rather than
recursing forever.

**Roundtrip / skip-set on save:** `Mc3Document` tracks which definition/
material/texture `id`s came from an include
(`includedDefs`/`includedMaterials`/`includedTextures`), and the writer
**does not** re-emit those entries into the saved file — they continue
to live only in the referenced library file, keeping the include
structure intact across save/load. If the main file locally overrides
one of those `id`s, the override is detected during parsing (the local
entry is removed from the "included" set the moment it's parsed) and
correctly gets written out as local content on save, not skipped.

---

## Meta (N7)

Free-form document-level key/value metadata — author, license, description, version tags, etc. Purely descriptive; not interpreted by the parser, exporter, or editor beyond round-tripping it.

```xml
<meta>
  <metaentry key="author"      value="Jane Doe"/>
  <metaentry key="license"     value="MIT"/>
  <metaentry key="description" value="A small demo scene"/>
</meta>
```

| Attribute | Type | Required |
|-----------|------|----------|
| `key` | string | yes |
| `value` | string | yes |

**Note:** this is distinct from the older `<metadata>` section (`<property name="..." value="..."/>` children), which exists for opaque pass-through data captured during import from other formats. `<meta>`/`<metaentry>` is the newer, general-purpose key/value store — prefer it for new content.

---

## Environment

```xml
<environment>
  <background color="0.1 0.1 0.15"/>
  <fog mode="linear" color="0.5 0.5 0.5" start="10" end="100" density="0.01"/>
</environment>
```

---

## Lights

```xml
<lights>
  <directional name="Sun" color="1 1 1" brightness="2.0" direction="-1 -2 -1" cast_shadows="false"/>
  <point       name="Lamp" color="1 0.9 0.8" brightness="3.0" position="0 3 0" range="10"/>
  <spot        name="Spot" color="1 1 1" brightness="5.0" position="0 5 0" direction="0 -1 0"
               angle="30" falloff="0.5" range="20"/>
  <ambient     color="0.2 0.2 0.2" brightness="1.0"/>
</lights>
```

---

## Cameras

```xml
<cameras default="MainCam">
  <camera name="MainCam" type="perspective" position="3 3 5" target="0 0 0"
          fov="60" near="0.1" far="1000"/>
  <camera name="OrthoTop" type="orthographic" position="0 10 0" target="0 0 0"
          size="10"/>
</cameras>
```

---

## Textures

```xml
<textures>
  <texture id="wall_tex" uri="textures/wall.png" wrap_u="repeat" wrap_v="repeat"
           filter="linear" color_space="srgb"/>
</textures>
```

**Path resolution:** `uri` is resolved **relative to the top-level
scene file's directory** — every consumer (`GltfExporter.cpp`,
`SceneRenderer.cpp`) resolves it as `doc.sourcePath / uri`, where
`sourcePath` is set once, from the file originally passed to
`Mc3Document::loadFromFile()`. This is **not** the same rule as
`<include>`'s own path resolution above: a `<texture>` declared inside
an `<include>`d file still resolves its `uri` relative to the
*top-level* scene file's directory, not relative to the included
file's own directory. In practice this means texture files referenced
by a shared/included material library should be placed relative to
wherever scenes that include that library actually live, not relative
to the library file itself.

---

## Materials

```xml
<materials>
  <material id="red_metal" roughness="0.2" metallic="0.8"
            alpha_mode="opaque" double_sided="false"
            normal_scale="1.0" occlusion_strength="1.0" alpha_cutoff="0.5">
    <base_color>0.8 0.1 0.1 1.0</base_color>    <!-- RGBA -->
    <emissive_color>0 0 0</emissive_color>       <!-- RGB, HDR values allowed (e.g. 2.4 0.9 0.05) -->
    <base_color_texture>wall_tex</base_color_texture>
    <normal_texture>wall_normal</normal_texture>
    <metallic_roughness_texture>wall_mr</metallic_roughness_texture>
    <occlusion_texture>wall_ao</occlusion_texture>
    <emissive_texture>wall_em</emissive_texture>
  </material>
</materials>
```

---

## Scripts (N3)

Inline Lua source, referenced by id from `<triggers>` (`<run-script ref="..."/>`) or by future runtime hooks. The script body is the element's text content (CDATA-safe via `mixed="true"` in the schema).

```xml
<scripts>
  <script id="onStart"   type="lua">print("scene started")</script>
  <script id="onCollide" type="lua">player:takeDamage(10)</script>
</scripts>
```

| Attribute | Type | Required | Notes |
|-----------|------|----------|-------|
| `id` | ID | yes | Referenced by `<run-script ref="...">` |
| `type` | string | yes | Only `"lua"` is currently defined |

**Status:** data model, parser, writer, MCB round-trip, and XSD validation are complete (STAB-0032). There is no Lua interpreter embedded in the editor or exporters yet — scripts are stored and round-tripped, not executed.

---

## Sounds and Music (N4)

One-shot/loopable sound effects and background music tracks, referenced by id from `<triggers>` (`<play-sound ref="...">`, `<play-music ref="...">`).

```xml
<sounds>
  <sound id="explosion" src="sounds/explosion.ogg" loop="false"/>
  <sound id="footstep"  src="sounds/footstep.ogg"  loop="true"/>
</sounds>

<music>
  <track id="theme"  src="music/theme.ogg"  loop="true"/>
  <track id="battle" src="music/battle.ogg" loop="true"/>
</music>
```

| Element | Attribute | Type | Required | Default |
|---------|-----------|------|----------|---------|
| `<sound>` | `id` | ID | yes | — |
| `<sound>` | `src` | URI | yes | — |
| `<sound>` | `loop` | bool | no | `false` |
| `<track>` (inside `<music>`) | `id` | ID | yes | — |
| `<track>` (inside `<music>`) | `src` | URI | yes | — |
| `<track>` (inside `<music>`) | `loop` | bool | no | `true` |

**Status:** data model, parser, writer, MCB round-trip, and XSD validation are complete (STAB-0032). No audio playback is implemented in the editor or exporters — these are data-only for now.

---

## Triggers (N5)

Named sequences of steps — references into `<actions>`, `<sounds>`, `<scripts>`, and `<music>` — intended to be fired by future gameplay/event logic.

```xml
<triggers>
  <trigger id="door_open">
    <play-action ref="anim_open"/>
    <play-sound  ref="creak"/>
    <run-script  ref="onOpen"/>
  </trigger>
  <trigger id="pickup">
    <play-action ref="pickup_anim"/>
    <play-music  ref="fanfare"/>
  </trigger>
</triggers>
```

| Step element | References |
|--------------|------------|
| `<play-action ref="..."/>` | an `<action name="...">` in `<actions>` |
| `<play-sound ref="..."/>` | a `<sound id="...">` in `<sounds>` |
| `<play-music ref="..."/>` | a `<track id="...">` in `<music>` |
| `<run-script ref="..."/>` | a `<script id="...">` in `<scripts>` |

A `<trigger>` can contain any number of steps in any order/combination. `ref` values are plain strings in the schema (not `IDREF`) — cross-references are not validated at parse time.

**Status:** data model, parser, writer, MCB round-trip, and XSD validation are complete (STAB-0043). Nothing in the editor or exporters currently fires triggers — there's no event system wired up to them yet.

---

## Scene States (N6)

Named snapshots of per-object property overrides (visibility, transform, material) — e.g. "day" vs. "night" variants of the same scene.

```xml
<states>
  <state name="day">
    <object-override id="lamp" visible="false"/>
    <object-override id="sun"  visible="true" material="day_mat"/>
  </state>
  <state name="night">
    <object-override id="lamp" visible="true" position="0 3 0" material="night_mat"/>
    <object-override id="sun"  visible="false"/>
  </state>
</states>
```

| Attribute (on `<object-override>`) | Type | Required |
|-------------------------------------|------|----------|
| `id` | string | yes — the target object's `id` attribute (see [Objects](#objects)) |
| `visible` | bool | no |
| `position` | vec3 | no |
| `rotation` | vec3 | no |
| `material` | string | no |

Only the attributes present on `<object-override>` are overridden; everything else keeps the target object's base value.

**Status:** data model, parser, writer, MCB round-trip, and XSD validation are complete (STAB-0044). There is no runtime "apply state" logic in the editor yet — states are stored and round-tripped, not switched between at runtime.

---

## Objects

All objects share common transform attributes:

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `name` | string | `""` | Display name |
| `id` | string | `""` | Unique identifier for references |
| `material` | string | `""` | Material id |
| `visible` | bool | `true` | Visibility |
| `position` | vec3 | `0 0 0` | Translation |
| `rotation` | vec3 | `0 0 0` | XYZ Euler angles in degrees |
| `scale` | vec3 or float | `1 1 1` | Non-uniform or uniform scale |
| `pivot` | vec3 | `0 0 0` | Rotation/scale pivot offset from position |
| `collision` | string | `"none"` | `"none"`, `"box"`, `"mesh"` |
| `tags` | space-separated | `""` | Arbitrary tags |
| `layer` | string | `""` | Named layer |

### Primitives

#### `<box>` / `<cube>`

```xml
<box name="Crate" size="1 1 1" material="wood"/>
<cube name="UnitCube"/>
```

| Attribute | Type | Default |
|-----------|------|---------|
| `size` | vec3 | `1 1 1` |

#### `<sphere>`

```xml
<sphere name="Ball" radius="0.5" segments="32"/>
```

#### `<cylinder>`

```xml
<cylinder name="Pillar" radius="0.3" height="2.0" segments="32" axis="y"/>
```

#### `<cone>`

```xml
<cone name="Hat" radius="0.5" height="1.0" segments="32"/>
```

#### `<plane>`

```xml
<plane name="Floor" size="10 10" axis="y"/>
```

**Note:** `size` is **vec2** — width (X) × depth (Z). The canonical form is `"W D"`.

| Attribute | Type | Default |
|-----------|------|---------|
| `size` | vec2 | `1 1` |
| `axis` | `"x"`, `"y"`, `"z"` | `"y"` |

#### `<torus>`, `<capsule>`, `<disk>`, `<grid>`, `<icosphere>`

Supported by the MeshCraft editor and exported by `mc3togltf`.

### `<mesh>` — external OBJ file

```xml
<mesh name="Tree" src="meshes/tree.obj" material="bark"/>
```

**Note:** The attribute is `src` (not `source`).

### `<extrude>` — path extrusion

```xml
<extrude name="Arch" segments="32" twist="0" smooth="true" caps="true">
  <cross_section type="circle" radius="0.1" segments="16"/>
  <path type="arc" radius="2.0" angle="180"/>
</extrude>
```

Cross-section types: `rect` (w/h), `circle` (radius, segments), `polygon` (radius, sides), `custom` (`<point x y/>` children).

Path types: `line` (length, axis), `arc` (radius, angle), `helix` (radius, height, turns), `polyline` (`<point x y z/>`), `bezier` (Catmull-Rom through `<point x y z cx cy cz/>`).

### `<group>`

```xml
<group name="Building">
  <box name="Wall" .../>
  <sphere name="Dome" .../>
</group>
```

### `<instance>`

```xml
<definitions>
  <definition id="tree_def">
    <sphere name="Canopy" radius="0.5"/>
  </definition>
</definitions>
<objects>
  <instance name="Tree1" definition="tree_def" position="3 0 0"/>
</objects>
```

### CSG operations

```xml
<difference name="DoorFrame">
  <box name="Frame" size="1 2 0.2"/>
  <box name="Opening" size="0.6 1.8 0.3" role="cutter"/>
</difference>
<union name="Combined">...</union>
<intersection name="Cut">...</intersection>
```

**Export note:** `mc3togltf` evaluates CSG booleans using the [Manifold](https://github.com/elalish/manifold) library and exports the result as a single merged mesh. The CSG root's transform (position/rotation/scale) is preserved as a glTF node TRS; children are fully baked into the boolean result and do not appear as separate glTF nodes.

Supported child primitives for CSG: Box, Cube, Sphere, Cylinder, Cone (analytic), Torus, Capsule, IcoSphere (triangulated), nested CSG/Group/Instance (if their children are also supported).

Unsupported child types cause the export to **fail with an error** in default mode: Plane, Disk, Grid (not watertight), Mesh (OBJ), Extrude (complex topology).

Pass `--allow-approximate-csg` (CLI) or enable the "Allow approximate CSG export" checkbox (editor) to bypass Manifold evaluation and export children as separate meshes instead. This mode is **geometrically incorrect** and intended only as a debug fallback.

**Strict mode is the default** (`allowApproximateCSG = false` in `GltfExporter`; the `mc3togltf_csg_strict` test asserts this): a CSG node containing an unsupported child type fails the whole export with an error rather than silently producing wrong geometry. There is no separate "strict" flag to set — it's simply what happens unless `--allow-approximate-csg` is explicitly passed.

**Limitations of CSG output** (the Manifold-evaluated result mesh, not the approximate-fallback path), per `CsgEvaluator.cpp`'s `manifoldToMeshData()`:
- **UV coordinates are not real** — a texcoord channel is present (same vertex count as positions/normals), but every value is a hardcoded `(0, 0)` placeholder, not an actual UV unwrap. Any material with texture slots (base color, normal, etc.) samples the same texel everywhere on a CSG result.
- **Normals are not preserved from the child geometry** — flat per-face normals are recomputed from each triangle's winding via cross product; there is no smooth-shading / vertex-normal-interpolation option for CSG output.
- Child material assignments are not preserved — the CSG root's material is used for the entire merged mesh, regardless of what materials the children had.

---

## Animations

```xml
<actions>
  <action name="Spin" duration="2.0" loop="true">
    <channel target="Wheel" property="rotation_y">
      <keyframe time="0" value="0"   interp="linear"/>
      <keyframe time="2" value="360" interp="linear"/>
    </channel>
  </action>
</actions>
```

**Animated properties:** `position_x/y/z`, `rotation_x/y/z`, `scale_x/y/z`, `visible`, `emissive_r/g/b`, `deform_x/y/z`

**Interpolation:** `linear`, `step`, `cubic` (cubic bezier with `<handle_left dt dv/>` and `<handle_right dt dv/>`)

**Units:** `duration` and keyframe `time` are already in **seconds** — there
is no frame-rate concept anywhere in the format, so `mc3togltf` passes
keyframe times straight through into glTF's animation sampler `input`
accessor (which the glTF spec also requires to be in seconds) with no
conversion needed or applied.

---

## mc3togltf export support matrix

| Feature | Exported |
|---------|----------|
| Box, Cube | ✅ |
| Sphere | ✅ |
| Cylinder, Cone | ✅ |
| Plane | ✅ |
| Extrude (all path types) | ✅ |
| OBJ mesh (`src`) | ✅ |
| Materials (PBR, textures) | ✅ |
| Lights | ✅ |
| Cameras | ✅ |
| Animations (position/rotation/scale) | ✅ |
| Animations (visible, emissive, deform) | ❌ (no glTF equivalent) |
| Torus, Capsule, Disk, Grid, IcoSphere | ✅ |
| CSG (union/difference/intersection) | ✅ (evaluated by Manifold; unsupported child types fail the export; `--allow-approximate-csg` exports children separately as debug fallback) |
| Instance (via definitions) | ✅ |
| SVG textures (N1, `<textures><texture>` with an SVG source) | ❌ — `svgTextures` is a separate map in `Mc3Document` that `GltfExporter` never rasterizes (rasterization isn't implemented anywhere yet, editor viewport included); since STAB-0440, a material referencing an SVG texture prints a warning naming the material/slot/texture id instead of dropping it silently, but the texture itself is still omitted from the export |
| Embedded glTF (N2, `<mesh src="embed:id"/>`) | ❌ — treated as a literal OBJ file path, which fails to parse; the export doesn't crash but continues with **no mesh on that node** (`Warning: OBJ load failed (...)` on stderr, `stats.warnings` incremented). See STAB-0194 for the tracked automated test of this exact behavior |
| Scripts, Sounds, Music, Triggers, Scene States, Meta (N3-N7) | ❌ (no glTF equivalent — these are MCB/XML-only data, round-tripped but not translated to any glTF concept; see [Scripts (N3)](#scripts-n3) etc. above) |

---

## MCB Binary Format

MCB (`.mcb`) is a compact binary encoding of the exact same `Mc3Document` model that `.mc3.xml` describes — same fields, same tree structure, just serialized as tagged binary values instead of XML text. It exists for faster load times at runtime; it is not a separate format with different capabilities, and every `.mc3.xml` scene round-trips through MCB losslessly (see `mc3_roundtrip`/`mcb_roundtrip`/`mc3tomcb_roundtrip` tests).

**File extension:** `.mcb`

**Producing an MCB file** — via the `mc3tomcb` CLI (direction is chosen by file extension):

```sh
./cmake-build-debug/mc3tomcb/mc3tomcb scene.mc3.xml scene.mcb   # XML -> MCB
./cmake-build-debug/mc3tomcb/mc3tomcb scene.mcb scene.mc3.xml   # MCB -> XML
```

Or from C++, via the `Mcb` library (`mcb/include/MeshCraft/Mcb/`):

```cpp
#include <MeshCraft/Mcb/McbWriter.hpp>
#include <MeshCraft/Mcb/McbReader.hpp>

MeshCraft::Mcb::saveToFile(doc, "scene.mcb");
Mc3::Mc3Document doc2 = MeshCraft::Mcb::loadFromFile("scene.mcb");
```

**Header layout** (`mcb/include/MeshCraft/Mcb/McbFormat.hpp`):

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 4 bytes | Magic | `"MCB\0"` |
| 4 | 1 byte | Version | Currently `1` (`MCB_VERSION`); readers reject any other value |
| 5 | 1 byte | Flags | Bit 0 = compressed payload — **defined but not implemented**; a reader throws if this bit is set |
| 6-7 | 2 bytes | Reserved | Always `0` |
| 8 | 1 byte | Root tag | Always `TAG_OBJ` (`0x07`) |
| 9+ | — | Payload | The document as a tagged key/value tree (see below) |

**Payload encoding:** every value is a 1-byte type tag followed by its data — `TAG_BOOL`/`TAG_I32`/`TAG_F32` (fixed-size), `TAG_STR` (uint32 length + UTF-8 bytes, no null terminator), `TAG_VEC3`/`TAG_VEC4` (3 or 4 float32), `TAG_OBJ` (key/value pairs terminated by a zero-length key), `TAG_ARR` (uint32 count + that many tagged values), `TAG_MAP` (uint32 count + that many `STR key` + tagged value pairs). Every `Mc3Document` field (objects, materials, textures, definitions, scripts, sounds, music, triggers, states, meta, etc. — including all N1-N7 extensions) is written under a string key matching its XML element/attribute name, so the two formats stay structurally parallel.

**Relationship to `.mc3.xml`:** MCB is a runtime-loading optimization, not an authoring format — there is no MCB-specific editor UI; you edit `.mc3.xml` and convert to `.mcb` as a build/export step (or open a `.mcb` directly, which the app transparently round-trips through the same `Mc3Document` model). Compression (flags bit 0) is reserved in the header for a future zlib payload but not implemented — an MCB file with that bit set cannot currently be read.

---

## Validation

The XSD schema is at `mc3/mc3.xsd`. Validate with:

```sh
python3 test/validate_xsd.py mc3/mc3.xsd scene.mc3.xml
```
