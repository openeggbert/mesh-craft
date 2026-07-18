# MC3 Format Specification

MC3 (MeshCraft 3D) is an XML-based scene format (`.mc3.xml`). It describes a 3D scene as editable constructive objects rather than raw triangle meshes.

---

## Root element

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="MyScene" unit="meter" coordinate_system="right_handed_y_up">
  ...
</mc3>
```

| Attribute | Values | Default |
|-----------|--------|---------|
| `version` | string | `"0.3"` |
| `model` | string | `"unnamed"` |
| `unit` | `"meter"`, `"centimeter"`, `"inch"` | `"meter"` |
| `coordinate_system` | `"right_handed_y_up"`, `"right_handed_z_up"` | `"right_handed_y_up"` |
| `rotation_units` | `"degrees"`, `"radians"` | `"degrees"` |
| `euler_order` | `"XYZ"`, `"XZY"`, `"YXZ"`, `"YZX"`, `"ZXY"`, `"ZYX"` | `"XYZ"` |
| `default_camera` | string (references a `<camera>`'s `name`) | — (see [Cameras](#cameras)) |

**`rotation_units`/`euler_order` are export-only** — honored by `mc3togltf`,
but the live editor's transform gizmo, keyboard nudging, and mouse-drag
rotation always assume degrees in a fixed XYZ order regardless of what a
loaded document declares (won't-fix, tracked as `STAB-0701`).

**`coordinate_system` is declarative metadata only — not honored anywhere,
not even by `mc3togltf`** (won't-fix, tracked as `SYS-W14-14`; a weaker
guarantee than `rotation_units`/`euler_order` above, which are at least
export-honored). Both the live editor and the exporter always treat scene
geometry, cameras, and lights as right-handed Y-up (matching glTF's own
fixed convention) regardless of what a document declares. Setting
`right_handed_z_up` parses, round-trips, and is settable through the
Scene Properties panel, but has zero effect on rendering or export.
Actually honoring it would mean applying a consistent axis-conversion
transform across every renderer/exporter/picking/gizmo entry point that
walks the scene graph — `SceneRenderer.cpp` alone has several independent
root-transform call sites (`draw()`, `drawEmissivePass()`,
`drawCsgGizmos()`, `computeObjectWorldMatrix()`) that would all need the
exact same conversion applied consistently, matching this project's own
documented "two independent geometry generators" risk (see the
architecture notes in `NEXT.md`) — getting even one of them wrong would
make gizmos/picking silently disagree with rendered geometry, a worse bug
than today's inert-field gap. Loading a document with a non-default
`coordinate_system` (or `rotation_units`/`euler_order`) now shows a
combined status-bar notice naming every declared-but-unhonored
convention (`checkRotationConventionNotice()`,
`MeshCraftApplication_FileOps.cpp`).

---

## Top-level sections

```xml
<mc3 ...>
  <library namespace="..." version="..."/>
  <imports>...</imports>
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

## Library and Imports (`<library>`, `<imports>`)

R110/R101: reusable-library identity and cross-library references, mostly
relevant to `.mc3lib.xml`/`.mc3lib.json` reusable-definition-library files
(`Mc3Document::saveToLibraryFile()`/`loadFromLibraryFile()`) rather than
ordinary scene documents.

```xml
<mc3 version="0.3" model="CityAssets">
  <library namespace="city-core" version="3.2.1" hash="sha256:..."/>
  <imports>
    <import namespace="furniture" source="mc3lib://furniture-pack@1.0.0" hash="sha256:..."/>
  </imports>
  ...
</mc3>
```

`<library>` (at most one, root-level) declares that **this document itself**
is a reusable library, referenced elsewhere as `mc3lib://<namespace>@<version>`.
Absent on an ordinary scene/model document.

| Attribute | Type | Required | Description |
|-----------|------|----------|--------------|
| `namespace` | string | yes | e.g. `"city-core"` |
| `version` | string | yes | semver `major.minor.patch` |
| `hash` | string | no | `"sha256:<64 lowercase hex chars>"` of the library's own content (`Mc3Document::computeLibraryContentHash()`) |

`<imports>` (at most one, root-level) lists libraries this document pulls
in under a local alias, so `<instance>`s can reference their definitions as
`"<namespace>:<definitionId>"`.

| `<import>` attribute | Type | Required | Description |
|-----------------------|------|----------|--------------|
| `namespace` | string | yes | local alias, e.g. `"furniture"` |
| `source` | string | yes | `"mc3lib://<library-name>@<version>"` — the library's own declared name/version, which may differ from the local alias |
| `hash` | string | no | if present, verified against the resolved library's own content hash on import |

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
correctly gets written out as local content on save, not skipped. SVG
textures (`<texture type="svg">`) share `textures`' own id-tracking and
are covered by this same skip-set (fixed in STAB-0091 — the writer's
SVG-texture loop was previously missing this check and silently
re-inlined included SVG textures on every save).

**Known limitation — `<embeds>` is not include-aware (STAB-0092):**
unlike definitions/materials/textures, an included file's own `<embeds>`
section is **never merged** — only the main document's own top-level
`<embeds>` is parsed. If a `<definition>` merged from an included file
references `<mesh src="embed:xyz"/>` where `xyz` is declared in *that
same included file's* `<embeds>` section (rather than the main
document's), the reference silently fails to resolve. This is a narrow,
accepted limitation (embedding glTF data specifically inside a
shared/included asset library), not implemented — would need an
`includedEmbeds` tracking set plus an `<embeds>` merge block in
`mergeInclude()`, mirroring the existing definitions-merge pattern.

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
  <background_texture>textures/sky.png</background_texture>
  <skybox_texture>textures/skybox.png</skybox_texture>
  <fog mode="linear" color="0.5 0.5 0.5" start="10" end="100" density="0.01"/>
</environment>
```

`<background_texture>`/`<skybox_texture>` are element **text content**
(not attributes on `<background>`), each holding a path resolved the same
way `<texture uri="...">` is (relative to the top-level scene file's
directory).

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

`brightness` is an arbitrary, unitless multiplier — not physical lux/candela units.
The editor renders it as a direct scale on the light's contribution, and
`mc3togltf` passes it straight through as `KHR_lights_punctual`'s `intensity`
field with no per-light-type conversion. glTF's `KHR_lights_punctual` spec
defines `intensity` as lux for directional lights and candela for point/spot
— physically-based glTF viewers/renderers may therefore render exported
lights at different *relative* brightness than the MeshCraft editor's own
preview, since the same raw number is being reinterpreted under different
physical units per light type. Not planned to change without a product
decision on whether physically-correct cross-renderer brightness is worth
requiring real unit conversion (and, likely, an editor-side lux/candela
input mode) — this is a deliberate, documented limitation, not an oversight.
`ambient` has no glTF equivalent at all and is dropped on export (with a
warning) since `KHR_lights_punctual` doesn't support ambient lighting.

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

The default camera can also be set via the root `<mc3 default_camera="MainCam">`
attribute instead of `<cameras default="...">`. If both are present,
`<cameras default="...">` wins. If neither is present, the first `<camera>`
in document order is used. The writer always outputs the `<cameras default>`
form on save, regardless of which spelling was used on load.

---

## Textures

```xml
<textures>
  <texture id="wall_tex" uri="textures/wall.png" wrap_u="repeat" wrap_v="repeat"
           filter="linear" color_space="srgb" mip_maps="true"/>
</textures>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|--------------|
| `mip_maps` | bool | `true` | Whether mipmaps should be generated for this texture |

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

**`color_space` is a pass-through hint only, not enforced.** `mc3togltf`
parses and stores it but never reads it back — no pixel-level
re-encoding happens at export time, the texture's raw file bytes are
copied/referenced as-is. glTF 2.0 requires `baseColorTexture` and
`emissiveTexture` to be sRGB-encoded and `normalTexture`/
`metallicRoughnessTexture`/`occlusionTexture` to be linear (non-color)
data; correctness of that encoding is entirely the responsibility of
the actual image file on disk, not something MeshCraft validates or
converts. Setting `color_space="srgb"` on a texture used as a normal
map, for example, does not trigger any warning or conversion — it is
simply unused metadata for that texture's actual role.

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
| `role` | string | `""` | `"cutter"` marks this object as a CSG cutter (see [CSG operations](#csg-operations)) |
| `script` | string | `""` | R103: references a `<script id="...">` (see [Scripts](#scripts-n3)) to run when this object triggers |

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

#### `<torus>`

```xml
<torus name="Ring" major_radius="0.35" minor_radius="0.15" segments="32"/>
```

| Attribute | Type | Default |
|-----------|------|---------|
| `major_radius` | float | `0.35` |
| `minor_radius` | float | `0.15` |
| `segments` | positive integer | `32` |

#### `<capsule>`

```xml
<capsule name="Pill" radius="0.5" height="1.0" segments="32" axis="y"/>
```

| Attribute | Type | Default |
|-----------|------|---------|
| `radius` | float | `0.5` |
| `height` | float | `1.0` |
| `segments` | positive integer | `32` |
| `axis` | `"x"`, `"y"`, `"z"` | `"y"` |

#### `<disk>`

```xml
<disk name="Coin" radius="0.5" inner_radius="0" segments="32" axis="y"/>
```

| Attribute | Type | Default |
|-----------|------|---------|
| `radius` | float | `0.5` |
| `inner_radius` | float | `0` (solid disk; > 0 makes an annulus/ring) |
| `segments` | positive integer | `32` |
| `axis` | `"x"`, `"y"`, `"z"` | `"y"` |

**Note:** the legacy attribute name `minor_radius` is still accepted as an
alias for `inner_radius` for backward compatibility, but `inner_radius` is
the canonical name and the only one in the schema.

#### `<grid>`

```xml
<grid name="Terrain" size="10 1 10" subdivisions_x="4" subdivisions_z="4"/>
```

| Attribute | Type | Default |
|-----------|------|---------|
| `size` | vec3 | `1 1 1` |
| `subdivisions_x` | positive integer | `4` |
| `subdivisions_z` | positive integer | `4` |

#### `<icosphere>`

```xml
<icosphere name="Rock" radius="0.5" segments="2"/>
```

| Attribute | Type | Default |
|-----------|------|---------|
| `radius` | float | `0.5` |
| `segments` | positive integer | `2` (subdivision level; note this primitive's default differs from the `32` shared by the other primitives above) |

All five are supported by the MeshCraft editor and exported by `mc3togltf`.

### `<uv_mapping>` — per-object UV override

Optional child element on most primitives (box/sphere/cylinder/cone/plane/
cube/torus/capsule/disk/grid/icosphere/mesh/extrude/group/CSG roots),
overriding that object's default UV generation.

```xml
<box name="Wall" size="4 3 0.2">
  <uv_mapping projection="box" scale_u="2.0" scale_v="1.0" offset_u="0" offset_v="0" rotation="0"/>
</box>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|--------------|
| `projection` | `"planar"`, `"box"`, `"sphere"` | unset | Projection mode |
| `scale_u` / `scale_v` | float | `1.0` | Per-axis UV scale (tiling) |
| `offset_u` / `offset_v` | float | `0.0` | Per-axis UV offset |
| `rotation` | float (degrees) | `0.0` | UV rotation |

Per-object UV mapping is ignored on CSG boolean output (see
[CSG operations](#csg-operations)'s "UV coordinates are not real"
limitation) — it only affects primitives whose geometry is generated
directly, not the Manifold-evaluated result of a `<union>`/`<difference>`/
`<intersection>`.

### `<mesh>` — external OBJ file

```xml
<mesh name="Tree" src="meshes/tree.obj" material="bark"/>
```

**Note:** The attribute is `src` (not `source`).

**Single material only:** a `<mesh>` object always uses its own `material`
attribute (or none) for the *entire* imported OBJ — this is intentional, not
a bug. If the source OBJ file assigns multiple materials per face/group
(`usemtl` groups, `.mtl`-referenced materials), those per-face assignments
are read by the OBJ parser but discarded: `mc3togltf`'s `loadObjMesh()`
flattens all faces into one triangle list with no material information, and
the single mc3-declared material (if any) is applied uniformly to the whole
mesh on export. Splitting such an OBJ into multiple glTF primitives/
materials on import would require deciding how OBJ `.mtl` material
properties map onto mc3's own material model — a real modeling decision, not
a mechanical fix — so for now, multi-material OBJ meshes should be
pre-split into separate single-material `<mesh>` objects (or `<group>`
children) if per-face materials are needed in the exported glTF.

### `<extrude>` — path extrusion

```xml
<extrude name="Arch" segments="32" twist="0" smooth="true" caps="true">
  <cross_section type="circle" radius="0.1" segments="16"/>
  <path type="arc" radius="2.0" angle="180"/>
</extrude>
```

Cross-section types: `rect` (w/h), `circle` (radius, segments), `polygon` (radius, sides), `star` (radius, inner_radius, sides), `custom` (`<point x y/>` children).

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
  <instance name="Tree2" definition="tree_def" position="5 0 0" material_override="autumn_mat"/>
</objects>
```

| Attribute | Type | Required | Description |
|-----------|------|----------|--------------|
| `definition` | string | yes | References a `<definition id="...">` |
| `material_override` | string | no | Material id applied to this instance in place of the definition's own material |
| `variants` | space-separated definition ids | no | Alternate definitions this instance may be randomly assigned among (variant picking) |

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

**Investigated: could UV be preserved (STAB-0225)?** Not without a real feature addition. Every `Manifold` fed into a boolean op in `CsgEvaluator.cpp` is built either from Manifold's own built-in primitive generators (`Manifold::Cube`/`Sphere`/`Cylinder`, which carry no UV data at all) or from a `MeshGL` with `numProp = 3` (position-only) for the Torus/Capsule/IcoSphere path — no UV channel is ever fed in for Manifold to carry through the boolean op in the first place. Manifold v3's `MeshGL` *does* support extra per-vertex properties beyond position (and interpolates them across new cut edges during boolean ops), so preserving UVs is technically possible — but it would require rebuilding every CSG-eligible primitive with a UV-carrying `MeshGL` (including writing new UV-aware constructors for the cases currently using Manifold's built-in generators) and handling the interpolated-but-unwrapped seams that boolean cuts create. That's a real, non-trivial feature, out of scope for this stabilization effort — documented as an explicit limitation above rather than attempted.

---

## Asset Metadata (`<assetMetadata>`)

R111: optional child element on any object (mesh_world_revival.md §6),
in practice authored on `<definition>` contents describing catalog/
authoring metadata about a reusable asset — deliberately covers only
AUTHORABLE fields (things a human/generator would author), not derived
data like triangle counts or a validation-status snapshot (see
`Mc3AssetMetadata.hpp`'s own doc comment for the full rationale of what
was deliberately left out).

```xml
<definition id="chair_def">
  <box name="Chair" size="0.5 0.9 0.5">
    <assetMetadata category="furniture" subcategory="chair" facing="-Z"
                    collision_proxy="box" shadow_policy="cast_receive"
                    license="CC-BY-4.0" provenance="hand-authored"
                    source="lua.object.chair.simple" version="1.2.3"
                    instancing_eligible="true" max_visibility_distance="250"
                    selection_weight="1.0" nominal_size="0.5 0.9 0.5"
                    bounds_min="-0.25 0 -0.25" bounds_max="0.25 0.9 0.25"
                    clearance_volume="0.8 1.2 0.8">
      <semanticTags><tag value="seating"/><tag value="wood"/></semanticTags>
      <styleTags><tag value="rustic"/></styleTags>
      <regionTags/>
      <periodTags/>
      <materialSlots><tag value="seat"/><tag value="legs"/></materialSlots>
      <sockets><socket name="seat_top" position="0 0.45 0"/></sockets>
      <lods><lod tier="near" definition="chair_def"/></lods>
    </assetMetadata>
  </box>
</definition>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|--------------|
| `category` / `subcategory` | string | `""` | Catalog classification |
| `facing` | string | `""` | Front-facing axis convention, e.g. `"-Z"`, `"+X"` |
| `collision_proxy` | string | `""` | Free-form collision shape descriptor, e.g. `"box"`, `"convex_hull"`, `"none"` |
| `shadow_policy` | string | `""` | Free-form shadow behavior descriptor, e.g. `"cast_receive"`, `"cast_only"`, `"none"` |
| `license` | string | `""` | SPDX id or free text |
| `provenance` | string | `""` | Author/source description |
| `source` | string | `""` | Generator id (e.g. `"lua.object.window.simple"`) or an AI request/recipe hash |
| `version` | string | `""` | Per-definition semantic version (distinct from `<library version="...">`, which versions the whole library file) |
| `instancing_eligible` | bool | `true` | Whether this definition is safe to instance many times |
| `max_visibility_distance` | float | `0` | LOD/culling hint; `0` = unspecified/no limit |
| `selection_weight` | float | `1.0` | Relative weight for random-variant picking; higher = more common |
| `nominal_size` | vec3 | `0 0 0` | Approximate authored size |
| `bounds_min` / `bounds_max` | vec3 | `0 0 0` | Bounding box in the definition's own local space |
| `clearance_volume` | vec3 | `0 0 0` | Required clearance as a width/height/depth size (not a positioned box) |

Child elements (all optional):

| Element | Contains | Description |
|---------|----------|--------------|
| `<semanticTags>` / `<styleTags>` / `<regionTags>` / `<periodTags>` / `<materialSlots>` | zero or more `<tag value="..."/>` | Free-form tag lists |
| `<sockets>` | zero or more `<socket name="..." position="x y z"/>` | Named anchor points/attachment sockets in local space |
| `<lods>` | zero or more `<lod tier="..." definition="..."/>` | LOD tier name → definition id (e.g. `tier="near"` → a higher-detail definition); `definition` may reference an id in an imported library, not just this document |

---

## Animations

```xml
<actions>
  <action name="Spin" duration="2.0" loop="true" autoplay="false">
    <channel target="Wheel" property="rotation_y">
      <keyframe time="0" value="0"   interp="linear"/>
      <keyframe time="2" value="360" interp="linear"/>
    </channel>
  </action>
</actions>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|--------------|
| `autoplay` | bool | `false` | Whether this action starts playing automatically when the document loads |

**Animated properties:** `position_x/y/z`, `rotation_x/y/z`, `scale_x/y/z`, `visible`, `emissive_r/g/b`, `deform_x/y/z`

**Interpolation:** `linear`, `step`, `cubic` (cubic bezier with `<handle_left dt dv/>` and `<handle_right dt dv/>`)

**Units:** `duration` and keyframe `time` are already in **seconds** — there
is no frame-rate concept anywhere in the format, so `mc3togltf` passes
keyframe times straight through into glTF's animation sampler `input`
accessor (which the glTF spec also requires to be in seconds) with no
conversion needed or applied.

**`cubic` (bezier) export:** glTF's own `CUBICSPLINE` sampler mode requires
real in/out tangent data in a strict triple-per-keyframe layout, which mc3's
tangent-handle data isn't converted into — instead, `mc3togltf` bakes a
bezier-interpolated channel down to dense `LINEAR` samples at a fixed 30
samples/sec, using the exact same curve evaluation the live editor uses (so
the export visually matches the editor preview). The rate is fixed, not
adaptive to curve complexity — a very long bezier action produces a
correspondingly large sampler purely from its duration, and a very fast/sharp
curve in a short time window could in principle be under-sampled.

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
| Animations (`visible`, `emissive_r/g/b`, `deform_x/y/z`) | ❌ (no glTF core-spec equivalent — glTF animation channels can only target `translation`/`rotation`/`scale`/`weights`). The channel is skipped with a warning naming it; no fallback is attempted (e.g. `visible` is not approximated via a scale-to-zero animation) — this is a deliberate, accepted limitation, not a bug. |
| Torus, Capsule, Disk, Grid, IcoSphere | ✅ |
| CSG (union/difference/intersection) | ✅ (evaluated by Manifold; unsupported child types fail the export; `--allow-approximate-csg` exports children separately as debug fallback) |
| Instance (via definitions) | ✅ |
| Per-object UV mapping (`<uv_mapping scale_u/scale_v/offset_u/offset_v/rotation>`) | ✅ (`AUD-024`) — applied to generated `TEXCOORD_0` (scale, then rotate about the UV origin, then offset); a non-`"planar"` `projection` value is not implemented anywhere in the codebase (neither export nor the live editor viewport) and now warns rather than silently doing nothing |
| Per-object `metadata` (`<metadata><property name="..." value="..."/></metadata>`) | ✅ (`AUD-029`) — serialized into `node.extras.metadata`, alongside the pre-existing `tags`/`collision` extras |
| `--stats` "Warnings" count | ✅ truthful (`AUD-026`) — every `"Warning:"` print site in `GltfExporter.cpp` increments the shared counter (verified by grep, not spot-checked); previously several paths (unknown material, SVG-slot warnings, ambient-light drop, duplicate node name, image-format detection, missing embed texture, action warnings) printed a warning without counting it |
| `TANGENT` accessor (for `normal_texture`-mapped meshes) | ✅ (STAB-0664) — computed per-vertex (standard per-triangle-then-averaged-then-Gram-Schmidt-orthogonalized algorithm, not a full MikkTSpace port), only when a mesh has both `NORMAL`/`TEXCOORD_0` and its material sets `normal_texture`; meshes without a normal map get no `TANGENT` (not needed) |
| SVG textures (N1, `<textures><texture>` with an SVG source) | ❌ — `svgTextures` is a separate map in `Mc3Document` that `GltfExporter` never rasterizes (rasterization isn't implemented anywhere yet, editor viewport included); since STAB-0440, a material referencing an SVG texture prints a warning naming the material/slot/texture id instead of dropping it silently, but the texture itself is still omitted from the export |
| Embedded glTF (N2, `<mesh src="embed:id"/>`) | ❌ — treated as a literal OBJ file path, which fails to parse; the export doesn't crash but continues with **no mesh on that node** (`Warning: OBJ load failed (...)` on stderr, `stats.warnings` incremented). See STAB-0194 for the tracked automated test of this exact behavior |
| Scripts, Sounds, Music, Triggers, Scene States, Meta (N3-N7) | ❌ (no glTF equivalent — these are MCB/XML-only data, round-tripped but not translated to any glTF concept; see [Scripts (N3)](#scripts-n3) etc. above) |

### Export scalability (STAB-0699)

Measured directly (not assumed): exporting scenes of 200 through 50,000
top-level primitive objects (box/sphere/cylinder/cone, round-robin, no
`<instance>`/definition sharing — each object independently triggers its own
`buildMesh()` call, the actual worst case since instanced geometry is
deduplicated via `buildDefCacheKey()`), export time and peak resident memory
both scale **linearly** with object count, with no sign of quadratic-or-worse
growth:

| Objects | Time | Peak RSS | time/object | RSS/object |
|--------:|-----:|---------:|------------:|-----------:|
| 1,000 | 0.02s | 8.2 MB | 0.020 ms | 8.4 KB |
| 5,000 | 0.09s | 16.0 MB | 0.019 ms | 3.3 KB |
| 10,000 | 0.18s | 26.1 MB | 0.018 ms | 2.7 KB |
| 25,000 | 0.47s | 55.2 MB | 0.019 ms | 2.3 KB |
| 50,000 | 0.93s | 104.1 MB | 0.019 ms | 2.1 KB |

Per-object time and memory cost stay flat (even improving slightly as fixed
process/parsing overhead amortizes over more objects) all the way to 50,000
objects — real-world scenes, which are very unlikely to approach that count,
export in well under a second. This was previously unaudited (flagged, not
confirmed, by the STAB-0662-0701 export-quality audit); no code change was
needed as a result of this measurement.

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

## Numeric ranges and document-complexity budgets (SYS-W1-02 / SYS-W1-03)

Two related but distinct kinds of input limits are enforced at load time in
`mc3/src/Mc3XmlParser.cpp`. Both are additive safety nets over the base
type/finiteness checks every numeric attribute already gets (`finiteOr`
rejects NaN/Inf; malformed non-numeric text like `"abc"` is defaulted, not
thrown) — see `finite_input_test`/`mc3_numeric_range_test` for the former and
`mc3_input_budget_test`/`mc3_document_budget_test` for the latter.

**Per-field numeric ranges (SYS-W1-02)** — a value that's a perfectly finite
float can still be outside its field's documented valid domain (a FOV of
600 degrees, a roughness of -3). Out-of-range values are **clamped**, not
rejected — the whole document isn't refused over what's almost always an
authoring typo, not an attack (the same judgment call this doc's
"tessellation count" note below makes). Named constants live next to each
check in `Mc3XmlParser.cpp`.

| Domain | Field(s) | Valid range | Notes |
|--------|----------|-------------|-------|
| Camera | `fov` | `[1, 179]` degrees | 0 or ≥180 isn't representable by a symmetric perspective frustum |
| Camera | `near` | `> 0` | perspective projection divides by `near` |
| Camera | `far` | `> near` (by ≥ 0.001) | near≥far collapses the z-buffer's usable depth range |
| Camera | `aspect` | `> 0` | zero/negative mirrors or collapses the view volume |
| Material | `roughness`, `metallic` | `[0, 1]` | glTF PBR convention; every consumer (live shading, glTF export) assumes it |
| Material | `occlusion_strength`, `alpha_cutoff` | `[0, 1]` | same glTF PBR convention |
| Material | `base_color`'s alpha channel | `[0, 1]` | opacity; RGB channels and `emissive_color` are deliberately left unclamped (emissive explicitly allows HDR values above 1.0) |
| Geometry | primitive `radius`/`height`/`size`/`major_radius`/`minor_radius` | `>= 0` | negative has no physical meaning; only negative is clamped (to 0) — zero itself is already meaningful for some fields, e.g. `<disk inner_radius="0"/>` |
| Geometry | extrude `<cross_section>` `width`/`height`/`radius`/`inner_radius` | `>= 0` | same reasoning |
| Geometry | extrude `<path>` `length`/`radius` (arc or helix)/`height` (helix) | `>= 0` | same reasoning; `angle`/`turns` are left unclamped (legitimately signed, for direction) |
| Environment | `<fog density="...">` | `>= 0` | negative inverts the exponential falloff's intended direction |
| Environment | `<fog start="..." end="...">` | `start < end` | **not clamped** — `SceneRenderer.cpp` already guards `end > start` before dividing, so this is diagnostic-only (a warning), not a value repair |
| Animation | `<action time_scale="...">` | `> 0` | 0 permanently stalls playback; negative isn't a supported "reverse" feature |
| Transform | `scale` (base transform, `<deform scale="...">`, and named `<state scale="...">`) | magnitude `>= 1e-4` per axis | near-zero degenerates the transform matrix (non-invertible); **sign is preserved** — negative scale is a legitimate glTF-supported mirroring feature, only near-zero *magnitude* is clamped |

**Not applicable — no field exists to range-check:** Audio (`<sound>`/
`<music><track>`) has no `volume`/`pitch` field at all in the current format
(`id`/`src`/`loop` only — see the Sounds and Music section above; audio
playback itself isn't implemented yet). Post-processing/bloom is a
runtime/editor UI toggle, not scene-file data at all (see the
`testEnvironmentAllFieldsRoundtrip` note in `mc3/test/roundtrip_test.cpp`) —
there is no `Mc3Environment` field for it to range-check. Both would need a
field added to the format first, which is out of this task's scope.

**Document-complexity budgets (SYS-W1-03)** — independent of any single
field's own range, a document-wide *running total* across many
individually-legal values can still be pathological (100,000 spheres at
`segments="4096"` are each legal on their own but request ~8.6e11 vertices
combined). These are hard **rejections** (the document fails to load with a
clear error naming the budget), not clamps — unlike a per-field range, there
is no sane "clamp" for "too many objects", only "refuse before the
corresponding allocation is attempted downstream." A `DocumentBudget`
(thread-local, reset once per top-level `parse()`/`parseString()` call,
shared across all `<include>`s merged into it) tracks each dimension:

| Dimension | Ceiling | Notes |
|-----------|---------|-------|
| Total objects | 100,000 | AUD-059; every parsed `<box>`/`<group>`/... counts, including inside `<definitions>` |
| Total tessellation weight | 500,000 | AUD-059; sum of every `segments`/`sides`/`subdivisions_*` value across the whole document |
| Total `<include>` fan-out | 1,000 | SYS-W1-04; distinct non-cyclic, non-diamond-duplicate included files |
| Total materials | 20,000 | |
| Total textures | 20,000 | `<texture>` and `<texture type="svg">` combined |
| Total embeds (count) | 1,000 | independent of the existing 64MB **per-embed** base64 size ceiling |
| Total embed bytes (sum) | 256MB | sum of every embed's base64 content length combined — bounds N embeds each individually under the per-embed cap from summing to unbounded memory |
| Total actions | 10,000 | |
| Total channels | 200,000 | across all actions |
| Total keyframes | 2,000,000 | across all channels |
| Total definitions | 20,000 | `doc.definitions` map-entry count, independent of each definition's own object-tree cost (already counted under "total objects") |
| Children per node | 20,000 | **not** a document-wide running total — a LOCAL per-node breadth cap (one `<group>`/`<union>`/`<difference>`/`<intersection>`/`<area>`'s direct children), distinct from total object count (a wide-but-shallow tree can stay under the total-object budget while still choking non-virtualized UI tree widgets) |
| Max document bytes | 512MB | the raw input file/string size itself, checked via `std::filesystem::file_size()` **before** tinyxml2 buffers or parses anything |
| Recursion (nesting) depth | 500 (tinyxml2's own `TINYXML2_MAX_ELEMENT_DEPTH`) | **not a novel MC3-level guard** — confirmed empirically that tinyxml2's own built-in element-depth cap already rejects (`XML_ELEMENT_DEPTH_EXCEEDED`) any XML nested deeper than this before `Mc3XmlParser`'s own `parseObject`/`parseChildren` recursion is ever reached, so the C++ call stack can never recurse past ~500 levels regardless of input |

**Explicitly not implemented:** a "total generated output bytes" estimate
(i.e. guessing at the eventual exported GLB or in-memory geometry size
across the whole document) was considered and deliberately skipped — it
would require estimating downstream allocation with no precise formula
(tessellation weight already approximates geometry complexity; texture/
embed byte totals already bound the largest binary blobs), so a fuzzy
byte-estimate budget wouldn't usefully bound anything beyond what the
dimensions above already do. The **input** byte ceiling above ("Max document
bytes") is the precise, actionable version of a "max bytes" budget.

---

## Validation

The XSD schema is at `mc3/mc3.xsd`. Validate with:

```sh
python3 test/validate_xsd.py mc3/mc3.xsd scene.mc3.xml
```

---

## Forward/backward compatibility (`SYS-W5-03`)

**Unrecognized XML attributes and elements are silently dropped on
round-trip** (load, then save). `Mc3XmlParser.cpp` reads every field via
named, explicit lookups with no "collect anything I didn't recognize"
fallback, and `Mc3XmlWriter.cpp` rebuilds the XML element from scratch on
save, emitting only the fields it knows about. There is no attribute-bag,
raw-node-preservation, or version-gate mechanism anywhere in either layer.

This is a **deliberate, accepted limitation**, not a bug: a document
authored with a newer MeshCraft version (or hand-edited with an
experimental attribute) that's opened and saved by an older version, or
vice versa, will lose whatever the loading version doesn't recognize —
silently, with no warning. (`doc.metadata`/`<meta>` are a separate,
narrow, opt-in passthrough store for exactly those two elements — not a
general mechanism for arbitrary unrecognized data.) Revisit only if a
concrete forward/backward-compatibility need arises; see `plan.md`'s
`SYS-W5-03` entry for the tradeoffs considered (generic attribute bag /
raw-node preservation / hard version gate) and why none was adopted.
