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

**Editor status (`SYS-W14-21`, 2026-07-20):** `<imports>` are actually
resolved by the editor, not just round-tripped — `Mc3ImportResolver`
(R101, `mc3/src/Mc3ImportResolver.cpp`) is invoked automatically right
after every load (search directory = the loaded document's own
directory) and merges each imported library's definitions into
`document_.definitions` under its local alias, so
`<instance definition="namespace:id">` referencing an imported
definition renders. An explicit "Resolve Imports" button in the editor's
Imports tab re-runs resolution after editing the rows, without a full
reload. A resolution failure (missing library file, content-hash
mismatch, an import cycle, or exceeding the resolver's chain-depth cap)
does not fail the whole document load — it's reported to the user and
the affected imports simply stay unresolved.

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

**`<embeds>` are include-aware (STAB-0092):** embeds declared by an included
file are merged alongside its definitions. Their external `src` paths are
rebased from the included file's directory to the main document's directory,
and `includedEmbeds` ensures an unchanged included embed is not inlined when
the main document is saved. A local edit makes it local content, matching the
existing definitions/materials/textures ownership rule. Duplicate ids use the
same documented last-write-wins rule as other included resources and emit a
named warning.

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

**Live-viewport fog implementation (`AUD-078`, 2026-07-20):** `<fog>` is
applied by `SceneRenderer.cpp` entirely via a per-object CPU-side color
blend (mixes each object's draw color toward `fog.color` based on camera
distance, correctly honoring both `mode="linear"` (`start`/`end`) and
`mode="exponential"` (`density`)) — a complete, correct implementation on
its own. An earlier version of this code *also* enabled CNA `BasicEffect`'s
own built-in GPU fog on top, unconditionally whenever `<fog>` existed,
always using linear `start`/`end` regardless of the declared `mode` —
wrong for Exponential mode (which has no `start`/`end` concept at all) and
redundant even for Linear mode. That extra GPU-fog call has been removed.
Empirically confirmed (rendering the exact same scene with and without it)
that it had produced no measurable pixel difference in this renderer's
actual configuration — so this was a dead/incorrect code cleanup, not a
fix for a previously-*visible* rendering defect.

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

`brightness` is an arbitrary, unitless authored scalar (default `1.0`) —
not a physical lux/candela value itself. As of `AUD-077` (2026-07-20), the
live editor viewport DOES use it for actual illumination for
`directional`/`ambient` lights specifically — see "Live-viewport shading"
below — though it's still just a multiplier on the light's own `color`,
not a physically calibrated unit. glTF's
`KHR_lights_punctual` spec requires `intensity` to be lux for directional
lights and candela for point/spot — physically different units, since a
candela is already "per steradian" and a lux isn't. As of `SYS-W14-26`
(2026-07-20), `mc3togltf` applies a deliberate, documented per-light-type
conversion instead of writing `brightness` unconverted into every type
(which made the same authored number implicitly ~13× dimmer as a
point/spot candela value than as a directional lux value, in any
glTF-conformant PBR viewer): `directional.intensity = brightness`
(unconverted — matches Blender's own glTF exporter's Sun-lamp convention,
treating W/m² as directly usable as lux), `point.intensity =
spot.intensity = brightness / (4π) × 683` (683 lm/W is the CIE
photometric luminous-efficacy constant at the 555nm peak-sensitivity
wavelength; ÷4π converts from total emitted "power" to per-steradian
candela — the same formula Blender's exporter uses for Point/Spot lamps).
This is a deliberate scale factor chosen to match an established,
widely-recognized exporter convention, not a claim that `brightness` is
now a fully physically-calibrated real-world quantity — there is still no
editor-side lux/candela input mode.
`ambient` has no glTF equivalent at all — neither glTF 2.0 core nor
`KHR_lights_punctual` support ambient lighting, a real spec gap, not an
oversight — so it is never exported as an actual light. As of
`SYS-W14-27` (2026-07-20), it is instead **approximated by baking its
contribution into every material's own emissive channel**: every
`<ambient>` light's `color × brightness` in the document is summed
(multiple ambients combine the same way multiple real fill lights
would), then that flat RGB contribution is added to each material's
`emissiveFactor`, tinted by that material's own `base_color` (so the
approximation still reflects each material's own albedo rather than
washing every material out to the same flat color) and clamped to
`[0, 1]`. This is a lossy but useful approximation — not physically
accurate global illumination — that keeps a glTF-conformant viewer's
render from looking fully unlit wherever an ambient fill was authored,
instead of just silently going dark. A warning is still printed naming
each ambient light and explaining that it was baked rather than
exported as a light.

**Live-viewport shading (`AUD-077`, 2026-07-20):** the editor's own
BasicEffect-based renderer (`SceneRenderer.cpp`) now actually shades the
live preview using `doc.lights`, instead of always using a fixed 3-point
default rig regardless of what's authored — `applyDocumentLighting()`
maps up to the first 3 `directional` lights onto BasicEffect's real
`DirectionalLight0`/`1`/`2` slots (`direction`, `color × brightness`
clamped to `[0,1]`, applied as both diffuse and specular) and the first
`ambient` light onto `AmbientLightColor`. **`point`/`spot` remain
gizmo-only in the live preview** — BasicEffect (a faithful port of real
XNA's fixed-function lighting model) has no position/attenuation API at
all, only up to 3 directional slots plus one ambient color, so there is
no way to represent them without a custom shader (a materially larger
change, out of scope here). If the document has no `directional`/
`ambient` lights to represent (including documents with only `point`/
`spot` lights, or no lights at all), the viewport falls back to the
original fixed default rig, preserving the existing look for the common
unlit-by-design case. `mc3togltf`'s export already handles every light
type correctly (see above) — this section is specifically about what the
live viewport, a separate and less capable renderer, can and can't show.

---

## Cameras

```xml
<cameras default="MainCam">
  <camera name="MainCam" type="perspective" position="3 3 5" target="0 0 0"
          fov="60" near="0.1" far="1000"/>
  <camera name="OrthoTop" type="orthographic" position="0 10 0" target="0 0 0"
          size="10"/>
  <camera name="RotCam" type="perspective" position="0 2 5" rotation="-10 0 0"
          fov="60" near="0.1" far="1000"/>
</cameras>
```

The default camera can also be set via the root `<mc3 default_camera="MainCam">`
attribute instead of `<cameras default="...">`. If both are present,
`<cameras default="...">` wins. If neither is present, the first `<camera>`
in document order is used. The writer always outputs the `<cameras default>`
form on save, regardless of which spelling was used on load.

`rotation` (optional `[x, y, z]` degrees, same axis convention as every
other rotation field in this format) is an **alternative to `target`** for
aiming the camera — set one or the other, not both meaningfully at once
(if `rotation` is present, it takes priority over `target`). **Live-editor
status (`AUD-079`, 2026-07-20):** both the camera gizmo and Look-Through-Camera
mode now actually honor `rotation` when present — previously both always
derived the view direction from `target` (silently pointing at its unused
`{0,0,0}` default whenever a camera was authored with `rotation` alone),
while `mc3togltf`'s export already handled it correctly.

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

**`mip_maps` status (`SYS-W14-22`, 2026-07-20):** honored by `mc3togltf` —
`false` makes the exporter emit a plain (non-mipmap) `LINEAR`/`NEAREST`
glTF sampler `minFilter` instead of unconditionally requesting a
mipmapped one. **Not honored by the live editor viewport** — CNA's
`Texture2D` asset-loading constructor has no mipmap parameter (only its
raw-pixel constructor does, and CNA's own OpenGL backend explicitly does
not generate mipmaps by default for the `Linear` filter that path uses),
so no mip chain is ever generated for viewport textures regardless of
this flag. Closing that gap needs a CNA-side API addition, out of scope
per `CLAUDE.md`'s CNA boundary — documented here as a known, deliberate
gap rather than left unexamined.

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

**`color_space` is a pass-through hint, not enforced — but mismatches are
now warned about (`SYS-W14-23`, 2026-07-20).** `mc3togltf` parses and
stores it, and still never re-encodes pixels at export time (the
texture's raw file bytes are copied/referenced as-is — correctness of
the actual encoding is entirely the responsibility of the image file on
disk). glTF 2.0 requires `baseColorTexture`/`emissiveTexture` to be
sRGB-encoded and `normalTexture`/`metallicRoughnessTexture`/
`occlusionTexture` to be linear (non-color) data — a fixed, spec-mandated
convention per slot that glTF has no per-texture way to override, so the
exported file always follows it regardless of what's declared. What
changed: if a texture's declared `color_space` conflicts with its
slot's mandated encoding (e.g. `color_space="srgb"` on a texture used as
`normal_texture`), the exporter now emits an explicit warning naming the
material, the texture id, the slot, and both the declared and required
color space — surfacing the likely-mistaken authoring intent instead of
silently doing nothing with it.

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

**Status:** data model, parser, writer, MCB round-trip, and XSD validation are complete (STAB-0032). As of `SYS-W14-18` (2026-07-20), a real sandboxed Lua 5.4 interpreter (`LuaScriptRunner`, Lua + sol2) IS embedded in the editor — run explicitly via the Scripts tab's "Run Script" button, or a trigger's `<run-script>` step ([Triggers (N5)](#triggers-n5)). Two globals are bound while a script runs: `def` (compose-time socket placement — `place`/`place_at`/`has_socket`, mirroring `../mesh-world`'s own established R103/R104 API) and `scene` (broader: `scene:find(idOrName)` returns a handle to read/write any object's position/rotation/scale/visible/material). No automatic execution exists yet (e.g. running a definition's script the moment it's placed/composed) — only the two explicit entry points above. Exporters (`mc3togltf`/`mc3tomcb`) still never execute scripts.

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

**Status:** data model, parser, writer, MCB round-trip, and XSD validation are complete (STAB-0032). Real audio playback IS implemented in the editor (`Editor::AudioPreview`, STAB-0706) — the Audio tab's own ▶/■ buttons play a `<sound>`/`<track>` directly, and a trigger's `<play-sound>`/`<play-music>` step ([Triggers (N5)](#triggers-n5)) reuses the same mechanism when fired. Exporters still never touch audio (not a glTF/OBJ concept).

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

**Status:** data model, parser, writer, MCB round-trip, and XSD validation are complete (STAB-0043). As of `SYS-W14-19` (2026-07-20), the editor's Triggers tab has an explicit "Fire" action (a per-row button, and a "Fire Trigger" button in the detail view) that actually executes a trigger's steps in order: `<play-action>` drives the same Timeline playback state the Play button uses, `<play-sound>`/`<play-music>` call `Editor::AudioPreview::play()`, `<run-script>` runs via `LuaScriptRunner` ([Scripts (N3)](#scripts-n3)). `SYS-W14-31` adds authored event bindings below, but the editor currently previews them as dry-run dispatch records: it deliberately does **not** execute trigger steps. The manual Fire action remains the only editor operation that executes a trigger, and only one "current action"/one shared audio-preview slot exists, so multiple `<play-action>` (or multiple `<play-sound>`/`<play-music>`) steps in one trigger replace rather than layer.

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

**Status:** data model, parser, writer, MCB round-trip, and XSD validation are complete (STAB-0044). As of `SYS-W14-20` (2026-07-20), the editor's States tab has an "Apply State" button that writes a state's overrides onto the matching live objects (by id) right now, so a state can be previewed interactively. Only the SET fields on an override are applied — an unset field leaves the target object's existing value untouched, matching this section's own "only the attributes present... are overridden" contract exactly. `SYS-W14-31` can target a named state in an authored event binding, but editor simulation is dry-run and does **not** apply the overrides; Apply State remains the mutating preview action.

---

## Event Bindings (SYS-W14-31)

`<event-bindings>` stores explicit document-level links from an ordinary
object or Area to a named trigger or scene state. It avoids implicit
name-based gameplay rules and is preserved by XML, semantic `.mc3.json`, and
MCB.

```xml
<event-bindings>
  <binding id="door_enter" source="door_area" event="enter"
           target_type="trigger" target="open_door" cooldown="0.25"/>
  <binding id="night_tick" source="clock" event="timer"
           target_type="state" target="night" enabled="false"
           once="true" interval="2"/>
</event-bindings>
```

| Attribute | Type / values | Required | Default | Meaning |
|-----------|---------------|----------|---------|---------|
| `id` | XML ID | yes | — | persistent binding identity; must be unique in the XML document |
| `source` | string | yes | — | object or Area `id` that emits the event |
| `event` | `enter`, `exit`, `click`, `timer` | yes | — | event kind |
| `target_type` | `trigger`, `state` | yes | — | target namespace |
| `target` | string | yes | — | trigger `id` or state `name` |
| `enabled` | bool | no | `true` | disabled bindings are ignored |
| `cooldown` | float seconds | no | `0` | minimum time after a successful dispatch before the binding can dispatch again |
| `once` | bool | no | `false` | successful dispatch is allowed only once per runtime/simulation session |
| `interval` | float seconds | no | `1` | timer period; only used when `event="timer"` |

The editor's **Events** tab has a selected-binding simulation button and an
optional timer simulation mode. Both are intentionally **dry-run**: they
produce a “would dispatch trigger/state” report and dangling-source/target
diagnostics, while never firing trigger steps, applying state overrides,
changing the authored document, or creating undo entries. The dispatcher has
a recursion guard and a 32-dispatch budget per call. Timer catch-up is also
bounded to one attempt per binding per frame. `enter`, `exit`, and `timer`
are delivered by this authoring/simulation slice; `click` is serialised and
can be manually dry-run, but viewport picking does not yet generate live
click events.

Event bindings are MC3/MCB runtime semantics only. `mc3togltf` emits one
explicit warning and omits all bindings because glTF has no portable
equivalent for MC3 triggers or state application.

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

Optional child element on geometry-producing primitives (box/sphere/cylinder/
cone/plane/cube/torus/capsule/disk/grid/icosphere/mesh/extrude) and CSG roots,
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

**`projection` (`SYS-W14-24`, `SYS-W14-06`):** `"box"` and `"sphere"` are
implemented by `mc3togltf`. Ordinary primitive viewport projection remains
export-only, but CSG roots use the same generated projection in both the
exporter and live CSG preview.
`"box"`/triplanar picks, per vertex, the dominant axis of that vertex's
normal (or its direction from the mesh's local bounding-box center if
normals are absent) and projects onto the other two axes using **raw,
non-normalized local-space coordinates** — so a texture's apparent scale
stays tied to object size, and `scale_u`/`scale_v` (applied afterward)
are the tiling control, exactly as they already are against the default
planar unwrap. `"sphere"` is an equirectangular mapping around the
mesh's local bounding-box center, normalized to `[0, 1]` on both axes.
`scale_u`/`scale_v`/`offset_u`/`offset_v`/`rotation` are still applied
on top of either regenerated projection, same as for `"planar"`.

For a CSG root, `uv_mapping` is applied **after** the Manifold boolean to the
generated result: `"planar"` projects local X/Z, `"box"` uses the dominant
normal axis, and `"sphere"` is the documented equirectangular projection.
Without it, a CSG result defaults to box projection. This is generated mapping,
not a retained authored unwrap from the operands; see [CSG operations](#csg-operations)
for that remaining limitation.

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

**CSG output shading/material behavior** (`SYS-W14-06`):

- Manifold calculates vertex normals after the boolean with its 60-degree
  sharp-edge threshold. Curved result surfaces shade smoothly; hard edges such
  as box corners stay sharp. This is generated result geometry, so it does not
  retain arbitrary authored normal vectors from inputs.
- The output always has usable generated UVs: default box projection or the
  CSG root's explicit `uv_mapping`. It intentionally does **not** preserve the
  operands' original UV seams/unwrapping through new cut surfaces. Retaining
  that would require feeding UV-carrying `MeshGL` data for every analytic
  primitive and defining seam policy for boolean-created vertices.
- An explicit material on the CSG root remains a full-result override. Without
  one, `mc3togltf` restores the Manifold source relation as one glTF primitive
  per effective child material, including cut faces associated with that input.
  The live preview uses the CSG root material/texture only; its per-child
  material split is export-only for now.

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

**Runtime semantics (SYS-W14-29):** for an `<instance>`, the consumer first
chooses its deterministic variant (FNV-1a of the serialized instance id; not
an implementation-defined `std::hash` or per-frame random value), then reads
that base definition's metadata. The live viewport treats `near`, `mid`, and
`far` as authored definition tiers, using configurable 25 m / 75 m defaults
and 2 m hysteresis. A missing `mid` or `far` tier falls back to the next finer
authored tier; a missing target definition falls back safely to the base and
is reported in the selected Instance's **Asset Definition LOD** debug panel.
`max_visibility_distance` culls only the live viewport when positive. This is
separate from the renderer's older primitive-tessellation LOD, which never
changes an authored definition. glTF/GLB export has no camera-distance context
and therefore exports the explicit `near`/default tier (including CSG
instances), not viewport culling or a provisional LOD extension.

---

## Animations

```xml
<actions>
  <action name="Spin" duration="2.0" loop="true" autoplay="false">
    <channel target="Wheel" property="rotation.y">
      <keyframe time="0" value="0"   interp="linear"/>
      <keyframe time="2" value="360" interp="linear"/>
    </channel>
  </action>
</actions>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|--------------|
| `autoplay` | bool | `false` | Whether this action starts playing automatically when the document loads |

**Animated properties** (`property` attribute — dot notation, not underscores; see
`Mc3Animation.cpp`'s `animatedPropertyName()`/`animatedPropertyFromName()`, the
single source of truth both the parser and the editor's Timeline/Anim panel use):

- Transform: `position.x/y/z`, `rotation.x/y/z`, `scale.x/y/z`
- Visibility: `visible`
- Deform: `deform.x/y/z`
- Material (targets the object's assigned material — see `Mc3Object.material`):
  `material.baseColor.r/g/b/a`, `material.roughness`, `material.metallic`,
  `material.emissive.r/g/b`

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
| Animations (`visible`, `deform.x/y/z`, all `material.*` channels) | ❌ (no glTF core-spec equivalent — glTF animation channels can only target `translation`/`rotation`/`scale`/`weights`). The channel is skipped with a warning naming it; no fallback is attempted (e.g. `visible` is not approximated via a scale-to-zero animation) — this is a deliberate, accepted limitation, not a bug. |
| Torus, Capsule, Disk, Grid, IcoSphere | ✅ |
| CSG (union/difference/intersection) | ✅ (evaluated by Manifold; unsupported child types fail the export; `--allow-approximate-csg` exports children separately as debug fallback) |
| Instance (via definitions) | ✅ |
| Per-object UV mapping (`<uv_mapping scale_u/scale_v/offset_u/offset_v/rotation>`) | ✅ (`AUD-024`) — applied to generated `TEXCOORD_0` (scale, then rotate about the UV origin, then offset). `projection="box"`/`"sphere"` (`SYS-W14-24`, 2026-07-20) actually regenerate `TEXCOORD_0` in `mc3togltf` before scale/offset/rotation is applied — no longer a warn-only no-op. `<uv_mapping>` in general (any attribute, any projection mode) is `mc3togltf`-only — `SceneRenderer.cpp` never reads it, so the live editor viewport always shows each primitive's default planar unwrap regardless of what's authored. |
| Per-object `metadata` (`<metadata><property name="..." value="..."/></metadata>`) | ✅ (`AUD-029`) — serialized into `node.extras.metadata`, alongside the pre-existing `tags`/`collision` extras |
| `--stats` "Warnings" count | ✅ truthful (`AUD-026`) — every `"Warning:"` print site in `GltfExporter.cpp` increments the shared counter (verified by grep, not spot-checked); previously several paths (unknown material, SVG-slot warnings, ambient-light drop, duplicate node name, image-format detection, missing embed texture, action warnings) printed a warning without counting it |
| `TANGENT` accessor (for `normal_texture`-mapped meshes) | ✅ (STAB-0664) — computed per-vertex (standard per-triangle-then-averaged-then-Gram-Schmidt-orthogonalized algorithm, not a full MikkTSpace port), only when a mesh has both `NORMAL`/`TEXCOORD_0` and its material sets `normal_texture`; meshes without a normal map get no `TANGENT` (not needed) |
| SVG textures (N1, `<textures><texture>` with an SVG source) | ✅ — external and inline SVG are rasterized into bounded PNG pixels for both glTF export and the live viewport. See the Textures section for the 2048px safety cap and cache/sampler details. |
| Embedded GLB (N2, `<mesh src="embed:id"/>`) | ✅ (`SYS-W14-05`) — external self-contained `.glb` files and inline base64 GLB are decoded, their default-scene node transforms are flattened, and triangle geometry reaches both `mc3togltf` and the live viewport. The MC3 object's material remains authoritative: source GLB materials/textures, skins, morph targets, animations, non-triangle primitives, loose `.gltf` companion-file assets, singular transforms, and geometry beyond 64 MiB/300,000 triangles are deliberately rejected with a named warning rather than partially or unsafely imported. |
| Scripts, Sounds, Music, Triggers, Scene States, Event Bindings, Meta (N3-N7) | ❌ (no glTF equivalent — these are MC3/MCB-only data, round-tripped but not translated to any glTF concept; exporting event bindings emits one explicit omission warning; see [Scripts (N3)](#scripts-n3) etc. above) |

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
| 5 | 1 byte | Flags | Bit 0 = compressed payload (`SYS-W14-25`, 2026-07-20 — see below) |
| 6-7 | 2 bytes | Reserved | Always `0` |
| 8+ | — | Payload | See below — layout depends on the Flags byte |

**Uncompressed (flags bit 0 unset, the default):**

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 8 | 1 byte | Root tag | Always `TAG_OBJ` (`0x07`) |
| 9+ | — | Document | The document as a tagged key/value tree (see below) |

**Compressed (flags bit 0 set):**

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 8 | 4 bytes | Uncompressed size | uint32 LE, byte length of the decompressed document payload (root tag + document) |
| 12 | 4 bytes | Compressed size | uint32 LE, byte length of the zlib-deflated bytes that follow |
| 16+ | — | Compressed bytes | zlib (deflate) compression, at `Z_BEST_COMPRESSION`, of `[TAG_OBJ byte][document payload]` |

`MeshCraft::Mcb::saveToBinary(doc, out, /*compress=*/true)` opts into
the compressed layout (default `false` — writes the uncompressed
layout, byte-for-byte the same as before this option existed).
`loadFromBinary`/`loadFromFile` transparently detect and decompress
either layout via the flags byte — callers never need to know which
one a given `.mcb` file uses. Requires this build to have been compiled
with zlib available (`mcb/CMakeLists.txt`'s `find_package(ZLIB)`,
optional — see `THIRD_PARTY.md`); `compress=true` throws a clear error
if zlib is unavailable, and reading a compressed file throws a distinct
"requires zlib" error rather than misparsing it. The claimed
uncompressed/compressed sizes are each validated against a 512MB sanity
ceiling before being used to size any buffer — the same zip-bomb
defense already applied to every other length-prefixed field in this
reader (`kMcbMaxStringLen`/`kMcbMaxCollectionCount`).

**Payload encoding:** every value is a 1-byte type tag followed by its data — `TAG_BOOL`/`TAG_I32`/`TAG_F32` (fixed-size), `TAG_STR` (uint32 length + UTF-8 bytes, no null terminator), `TAG_VEC3`/`TAG_VEC4` (3 or 4 float32), `TAG_OBJ` (key/value pairs terminated by a zero-length key), `TAG_ARR` (uint32 count + that many tagged values), `TAG_MAP` (uint32 count + that many `STR key` + tagged value pairs). Every `Mc3Document` field (objects, materials, textures, definitions, scripts, sounds, music, triggers, states, event bindings, meta, etc. — including all N1-N7 extensions) is written under a string key matching its XML element/attribute name, so the two formats stay structurally parallel.

**Relationship to `.mc3.xml`:** MCB is a runtime-loading optimization, not an authoring format — there is no MCB-specific editor UI; you edit `.mc3.xml` and convert to `.mcb` as a build/export step (or open a `.mcb` directly, which the app transparently round-trips through the same `Mc3Document` model). Compression (flags bit 0) is opt-in via `saveToBinary`/`saveToFile`'s `compress` parameter — there is no editor UI toggle for it (out of scope for `SYS-W14-25`); a compressed `.mcb` produced by another caller still loads transparently through the normal `loadFromFile()` path either way.

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
