# MC3 Scene Format — Reference Documentation

**Version:** 0.3  
**File extension:** `.mc3.xml`  
**Encoding:** UTF-8  
**Schema:** `mc3.xsd`

---

## Overview

MC3 is an XML-based 3D scene description format used by the OpenEggbert project.
A scene is composed of geometric objects (primitives, extrusions, CSG booleans,
instances of reusable definitions), lights, cameras, materials, textures, and
keyframe animations. The output pipeline exports to `.glb` via `mc3togltf`
(animations are exported as glTF `animations[]`).

---

## Root Element

```xml
<mc3 version="0.3" model="MyScene" unit="meter" rotation_units="degrees" euler_order="XYZ">
  ...
</mc3>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `version` | string | `0.3` | Format version |
| `model` | string | — | Scene/model name |
| `unit` | `meter` \| `centimeter` \| `inch` | `meter` | World unit |
| `coordinate_system` | `right_handed_y_up` \| `right_handed_z_up` | `right_handed_y_up` | Coordinate convention |
| `rotation_units` | `degrees` \| `radians` | `degrees` | Units for all `rotation` attributes in the document |
| `euler_order` | `XYZ` \| `XZY` \| `YXZ` \| `YZX` \| `ZXY` \| `ZYX` | `XYZ` | Euler rotation composition order for all objects |
| `default_camera` | string | — | Name of the default camera |

---

## Top-Level Sections

```xml
<mc3>
  <environment>…</environment>
  <lights>…</lights>
  <cameras>…</cameras>
  <textures>…</textures>
  <materials>…</materials>
  <definitions>…</definitions>
  <objects>…</objects>
  <actions>…</actions>
</mc3>
```

All sections are optional and may appear in any order.

---

## `<environment>`

```xml
<environment>
  <background color="0.5 0.7 1.0" texture="sky"/>
  <fog mode="linear" color="0.5 0.7 1.0" start="50" end="200" density="0.01"/>
</environment>
```

### `<background>`

| Attribute | Type | Default | Description |
|---|---|---|---|
| `color` | vec3 (RGB 0–1) | — | Background clear color |
| `texture` | string | — | Reference to a texture id (skybox / background image) |

### `<fog>`

| Attribute | Type | Default | Description |
|---|---|---|---|
| `mode` | `linear` \| `exponential` | `linear` | Fog falloff mode |
| `color` | vec3 (RGB 0–1) | — | Fog color |
| `start` | float | — | Linear fog start distance |
| `end` | float | — | Linear fog end distance |
| `density` | float | — | Exponential fog density coefficient |

---

## `<lights>`

```xml
<lights>
  <ambient    color="0.3 0.3 0.4" brightness="0.5"/>
  <directional name="Sun" color="1 0.95 0.8" brightness="2" direction="-0.5 -1 -0.5" cast_shadows="true"/>
  <point       name="Lamp" color="1 0.6 0.2" brightness="5" position="0 3 0" range="10"/>
  <spot        name="Torch" color="1 0.7 0.2" brightness="8"
               position="0 5 0" direction="0 -1 0" angle="30" falloff="45" range="20"/>
</lights>
```

**Common attributes** (all light types):

| Attribute | Type | Default | Description |
|---|---|---|---|
| `name` | string | — | Light identifier |
| `color` | vec3 (RGB 0–1) | `1 1 1` | Light color |
| `brightness` | float | `1.0` | Intensity multiplier |

**`<ambient>`** — uniform fill light, no position or direction.

**`<directional>`** — parallel rays from infinite distance.

| Attribute | Type | Description |
|---|---|---|
| `direction` | vec3 | Light direction vector (towards target) |
| `cast_shadows` | boolean | Enable shadow casting |

**`<point>`** — omnidirectional point source.

| Attribute | Type | Description |
|---|---|---|
| `position` | vec3 | World position |
| `range` | float | Attenuation radius |
| `cast_shadows` | boolean | Enable shadow casting |

**`<spot>`** — cone-shaped directional source.

| Attribute | Type | Description |
|---|---|---|
| `position` | vec3 | World position |
| `direction` | vec3 | Cone axis direction |
| `angle` | float (°) | Inner cone half-angle |
| `falloff` | float (°) | Outer cone half-angle (penumbra) |
| `range` | float | Attenuation radius |
| `cast_shadows` | boolean | Enable shadow casting |

---

## `<cameras>`

```xml
<cameras default="Main">
  <camera name="Main"   type="perspective"   position="8 5 12" target="0 1 0" fov="60" near="0.1" far="500"/>
  <camera name="Ortho"  type="orthographic"  position="0 10 0" target="0 0 0" size="20" near="0.1" far="200"/>
</cameras>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `name` | string | — | Camera identifier |
| `type` | `perspective` \| `orthographic` | `perspective` | Projection type |
| `position` | vec3 | `0 0 10` | Eye position |
| `target` | vec3 | `0 0 0` | Look-at point |
| `rotation` | vec3 (°) | — | Euler rotation override (overrides target) |
| `near` | float | `0.1` | Near clip distance |
| `far` | float | `1000` | Far clip distance |
| `fov` | float (°) | `60` | Vertical field of view (perspective only) |
| `size` | float | `10` | World-space height (orthographic only) |

---

## `<textures>`

```xml
<textures>
  <texture id="brick_wall" uri="textures/brick.png" wrap_u="repeat" wrap_v="repeat" filter="linear" color_space="srgb"/>
</textures>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `id` | ID | — | Unique texture identifier (referenced by materials) |
| `uri` | anyURI | — | Path to image file (relative to the `.mc3.xml` file) |
| `wrap_u` | `repeat` \| `clamp` \| `mirror` | `repeat` | U-axis wrap mode |
| `wrap_v` | `repeat` \| `clamp` \| `mirror` | `repeat` | V-axis wrap mode |
| `filter` | `linear` \| `nearest` | `linear` | Texture filter |
| `color_space` | `srgb` \| `linear` | `srgb` | Color space (affects shader gamma) |
| `mip_maps` | boolean | `true` | Generate mipmaps |

---

## `<materials>`

```xml
<materials>
  <material id="brick" roughness="0.8" metallic="0.0" alpha_mode="opaque">
    <base_color>0.8 0.2 0.1 1.0</base_color>
    <base_color_texture>brick_wall</base_color_texture>
    <emissive_color>0 0 0</emissive_color>
  </material>
</materials>
```

**Attributes:**

| Attribute | Type | Default | Description |
|---|---|---|---|
| `id` | ID | — | Unique material identifier |
| `roughness` | float 0–1 | `0.5` | PBR roughness |
| `metallic` | float 0–1 | `0.0` | PBR metallic factor |
| `normal_scale` | float | `1.0` | Normal map strength multiplier |
| `occlusion_strength` | float 0–1 | `1.0` | Ambient occlusion strength |
| `alpha_mode` | `opaque` \| `mask` \| `blend` | `opaque` | Transparency mode |
| `alpha_cutoff` | float 0–1 | `0.5` | Alpha threshold for `mask` mode |
| `double_sided` | boolean | `false` | Disable back-face culling |

**Child elements** (all optional):

| Element | Content type | Description |
|---|---|---|
| `<base_color>` | `colorType` (RGB vec3 or RGBA vec4) | Base color tint; alpha defaults to `1.0` when 3 components given |
| `<emissive_color>` | `vec3Type` (RGB) | Emissive light color |
| `<base_color_texture>` | IDREF → texture `id` | Albedo / diffuse map |
| `<metallic_roughness_texture>` | IDREF → texture `id` | ORM map: R=occlusion, G=roughness, B=metallic |
| `<normal_texture>` | IDREF → texture `id` | Tangent-space normal map |
| `<occlusion_texture>` | IDREF → texture `id` | Ambient occlusion map |
| `<emissive_texture>` | IDREF → texture `id` | Emissive map |

Texture slot elements contain the texture `id` as text, validated by XSD as an `xs:IDREF` (the referenced `<texture>` must exist in `<textures>`).

---

## `<definitions>`

Reusable object templates that can be instanced multiple times via `<instance>`.

```xml
<definitions>
  <definition id="tree">
    <group name="Tree">
      <cylinder name="trunk" radius="0.2" height="2" material="bark" position="0 1 0"/>
      <sphere   name="canopy" radius="1.2" material="leaves" position="0 2.8 0"/>
    </group>
  </definition>
</definitions>
```

| Attribute | Type | Description |
|---|---|---|
| `id` | ID | Unique definition identifier |

The definition body contains exactly one root object (any type including `group`).

---

## Objects

All objects support these **common attributes** (plus type-specific ones):

| Attribute | Type | Default | Description |
|---|---|---|---|
| `name` | string | — | Object name / identifier |
| `position` | vec3 | `0 0 0` | World position (XYZ) |
| `rotation` | vec3 | `0 0 0` | Euler rotation — units and order set by root `rotation_units` / `euler_order` |
| `scale` | vec3 | `1 1 1` | Scale per axis |
| `pivot` | vec3 | `0 0 0` | Rotation/scale pivot offset (local space) |
| `material` | IDREF | — | Reference to a material `id` (validated by XSD) |
| `visible` | boolean | `true` | Object visibility |
| `collision` | string | `none` | Collision shape hint (`none`, `box`, `mesh`, …) |
| `tags` | string | — | Space-separated tag list |
| `role` | `cutter` | — | Marks object as a CSG cutter inside `<difference>` |

**Transform order:** `T(position + pivot) × R(rotation) × S(scale) × T(-pivot)`

All objects may contain optional child elements in this order:

1. `<deform>` — geometry-level scale, applied before the object transform
2. `<uv_mapping>` — texture coordinate scale/offset/rotation (leaf objects only)
3. `<metadata>` — opaque key/value store for import-time source data

```xml
<deform scale="2.0 1.0 0.5"/>
```

```xml
<uv_mapping scale="2.5 1.5" offset="0 0" rotation="0"/>
```

```xml
<metadata>
  <property name="rd4_original_type" value="SweepObject"/>
  <property name="rd4_source_line"   value="142"/>
</metadata>
```

---

### `<box>`

Rectangular box centered at the origin.

```xml
<box name="Wall" size="10 4 0.3" position="0 2 -5" material="brick"/>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `size` | vec3 | `1 1 1` | Width × Height × Depth |
| `segments` | int | `1` | Subdivisions per axis |

---

### `<sphere>`

```xml
<sphere name="Ball" radius="0.5" segments="24" position="0 1 0"/>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `radius` | float | `0.5` | Sphere radius |
| `segments` | int | `16` | Latitude and longitude subdivisions |

---

### `<cylinder>`

```xml
<cylinder name="Pillar" radius="0.3" height="4" segments="16" position="0 2 0"/>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `radius` | float | `0.5` | Base radius |
| `height` | float | `1.0` | Total height |
| `segments` | int | `16` | Circumference subdivisions |
| `axis` | `x` \| `y` \| `z` | `y` | Cylinder axis |

---

### `<cone>`

```xml
<cone name="Cap" radius="0.5" height="1.5" segments="16" position="0 3 0"/>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `radius` | float | `0.5` | Base radius |
| `height` | float | `1.0` | Total height |
| `segments` | int | `16` | Circumference subdivisions |

---

### `<plane>`

Flat surface with no thickness.

```xml
<plane name="Ground" size="50 50" axis="y" position="0 0 0" material="grass"/>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `size` | vec2 (W H) | `1 1` | Width and height |
| `axis` | `x` \| `y` \| `z` | `y` | Normal axis |
| `segments` | int | `1` | Subdivisions |

---

### `<mesh>`

External polygon mesh loaded from file.

```xml
<mesh name="Chair" src="assets/chair.obj" position="2 0 1" material="wood"/>
```

| Attribute | Type | Description |
|---|---|---|
| `src` | anyURI | Path to mesh file (OBJ, GLB, etc.) relative to scene file |

---

### `<extrude>`

Sweeps a 2D cross-section along a 3D path.

```xml
<extrude name="Arch" position="0 0 -4" material="stone"
         segments="24" twist="0" smooth="true" caps="true">
  <cross_section type="rect" width="0.3" height="0.2"/>
  <path type="arc" radius="2.0" angle="180"/>
</extrude>
```

**Extrude attributes:**

| Attribute | Type | Default | Description |
|---|---|---|---|
| `segments` | int | `8` | Path subdivisions |
| `twist` | float (°) | `0` | Cross-section rotation along path |
| `smooth` | boolean | `true` | Smooth normals between segments |
| `caps` | boolean | `true` | Close the ends with cap faces |

#### `<cross_section>`

| `type` | Extra attributes | Description |
|---|---|---|
| `rect` | `width`, `height` | Rectangle |
| `circle` | `radius`, `segments` | Circle (approximated polygon) |
| `polygon` | `radius`, `segments` | Regular polygon |
| `custom` | child `<point x y>` elements | Arbitrary 2D polygon |

All cross-section types accept `inner_radius` for a hollow tube
(generates outer wall + inner wall + annular caps).

#### `<path>`

| `type` | Extra attributes | Description |
|---|---|---|
| `line` | `axis`, `length` | Straight line along axis |
| `arc` | `radius`, `angle` | Circular arc in the XZ plane |
| `helix` | `radius`, `height`, `turns` | Helical coil |
| `polyline` | child `<point x y z>` | Piecewise linear |
| `bezier` | child `<point x y z>` | Cubic Bézier (control points) |

---

### `<group>`

Container that applies a shared transform to its children.

```xml
<group name="House" position="10 0 0" rotation="0 45 0">
  <box name="Walls" …/>
  <box name="Roof"  …/>
</group>
```

Children can be any object type (including nested groups).

---

### `<instance>`

Places a copy of a `<definition>` in the scene. May override the material.

```xml
<instance name="Tree1" definition="tree" position="-5 0 3"/>
<instance name="Tree2" definition="tree" position=" 5 0 1" material="autumn_leaves"/>
```

| Attribute | Type | Description |
|---|---|---|
| `definition` | IDREF | Reference to a definition `id` (validated by XSD) |
| `material` | IDREF | Material override — replaces per-object materials inside the definition |

---

### `<union>` / `<difference>` / `<intersection>`

CSG boolean operations. Children define the input geometry.

```xml
<difference name="ArchHole" position="0 1 0" material="stone">
  <box    name="block"  size="2 2 2"/>
  <sphere name="cavity" radius="0.8" role="cutter"/>
</difference>
```

For `<difference>`, children with `role="cutter"` are subtracted from the rest.
For `<union>` and `<intersection>`, all children participate equally.

Children can be any primitive or nested CSG operation (recursive).

**glTF export:** `mc3togltf` does not evaluate CSG booleans. By default any CSG node causes an export error. Pass `--allow-approximate-csg` (CLI) or enable the editor checkbox to export children as separate meshes instead (geometrically incorrect but useful for preview).

---

### `<area>`

Invisible logical zone (collision, trigger, pathfinding, etc.).

```xml
<area name="TriggerZone" size="5 3 5" position="0 1.5 0" tags="trigger checkpoint"/>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `size` | vec3 | `1 1 1` | Box extent of the area |

---

## Value Formats

| Type | Format | Example |
|---|---|---|
| vec2 | `"W H"` | `"50 50"` |
| vec3 | `"X Y Z"` | `"0 2 -5"` or `"-0.5 -1.0 -0.5"` |
| vec4 | `"R G B A"` | `"0.8 0.2 0.1 1.0"` |
| boolean | `"true"` or `"false"` | |
| float | decimal | `"0.5"` |
| int | positive integer | `"24"` |
| ID | XML ID (unique) | `"brick"` |

---

## Complete Minimal Example

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="Minimal">

  <environment>
    <background color="0.4 0.6 0.9"/>
  </environment>

  <lights>
    <ambient     color="0.3 0.3 0.4" brightness="0.4"/>
    <directional name="Sun" color="1 0.95 0.8" brightness="2.5" direction="-0.5 -1 -0.3"/>
  </lights>

  <cameras>
    <camera name="Main" position="5 4 8" target="0 1 0" fov="60"/>
  </cameras>

  <materials>
    <material id="grey">
      <base_color>0.7 0.7 0.7 1.0</base_color>
    </material>
  </materials>

  <objects>
    <plane name="Ground" size="20 20" axis="y" material="grey"/>
    <box   name="Cube"   size="1 1 1" position="0 0.5 0" material="grey"/>
  </objects>

</mc3>
```

---

## `<actions>` — Keyframe Animation

Defines named animation clips. Each action contains channels; each channel animates one
scalar property of one named scene object over time.

```xml
<actions>
  <action name="Bounce" duration="2.0" loop="true">
    <channel target="BouncingBox" property="position.y">
      <keyframe time="0.0" value="0.0" interp="cubic">
        <handle_left  dt="-0.2" dv="0.0"/>
        <handle_right dt=" 0.2" dv="2.0"/>
      </keyframe>
      <keyframe time="1.0" value="3.0" interp="cubic">
        <handle_left  dt="-0.2" dv="2.0"/>
        <handle_right dt=" 0.2" dv="-2.0"/>
      </keyframe>
      <keyframe time="2.0" value="0.0" interp="cubic">
        <handle_left  dt="-0.2" dv="-2.0"/>
        <handle_right dt=" 0.2" dv="0.0"/>
      </keyframe>
    </channel>
  </action>

  <action name="Spin" duration="3.0" loop="true">
    <channel target="SpinningSphere" property="rotation.y">
      <keyframe time="0.0"  value="0.0"   interp="linear"/>
      <keyframe time="3.0"  value="360.0" interp="linear"/>
    </channel>
  </action>

  <action name="Flash" duration="2.0" loop="true">
    <channel target="FadingPlane" property="visible">
      <keyframe time="0.0" value="1.0" interp="step"/>
      <keyframe time="0.5" value="0.0" interp="step"/>
      <keyframe time="1.0" value="1.0" interp="step"/>
    </channel>
  </action>
</actions>
```

### `<action>`

| Attribute | Type | Default | Description |
|---|---|---|---|
| `name` | string | — | Unique clip name |
| `duration` | float | `1.0` | Clip length in seconds |
| `loop` | boolean | `false` | Loop when end is reached |

### `<channel>`

| Attribute | Type | Description |
|---|---|---|
| `target` | string | Name of the scene object to animate |
| `property` | string | Animatable property (see table below) |

Keyframes in a channel are automatically sorted by `time` on load.

### `<keyframe>`

| Attribute | Type | Default | Description |
|---|---|---|---|
| `time` | float | — | Time in seconds |
| `value` | float | — | Property value at this time |
| `interp` | `linear` \| `step` \| `cubic` | `linear` | Interpolation to the **next** keyframe |

For `interp="cubic"`, two optional child elements define the cubic bezier tangent handles
(expressed as offsets relative to the owning keyframe's `(time, value)` point):

```xml
<handle_left  dt="-0.2" dv="0.0"/>   <!-- in-tangent -->
<handle_right dt=" 0.2" dv="1.5"/>   <!-- out-tangent -->
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `dt` | float | `±0.1` | Time offset of the handle (negative for left, positive for right) |
| `dv` | float | `0.0` | Value offset of the handle |

### Animatable Properties

All properties are float-valued. `visible` uses `0.0` = false, `≥ 0.5` = true.

| Property | Unit / Range | glTF export |
|---|---|---|
| `position.x` | world units | `translation.x` |
| `position.y` | world units | `translation.y` |
| `position.z` | world units | `translation.z` |
| `rotation.x` | degrees | combined into quaternion `rotation` |
| `rotation.y` | degrees | combined into quaternion `rotation` |
| `rotation.z` | degrees | combined into quaternion `rotation` |
| `scale.x` | factor | `scale.x` |
| `scale.y` | factor | `scale.y` |
| `scale.z` | factor | `scale.z` |
| `visible` | 0 or 1 | *(not exported)* |
| `deform.x` | factor | *(not exported)* |
| `deform.y` | factor | *(not exported)* |
| `deform.z` | factor | *(not exported)* |
| `material.baseColor.r` | 0–1 | *(not exported)* |
| `material.baseColor.g` | 0–1 | *(not exported)* |
| `material.baseColor.b` | 0–1 | *(not exported)* |
| `material.baseColor.a` | 0–1 | *(not exported)* |
| `material.roughness` | 0–1 | *(not exported)* |
| `material.metallic` | 0–1 | *(not exported)* |
| `material.emissive.r` | 0–1 | *(not exported)* |
| `material.emissive.g` | 0–1 | *(not exported)* |
| `material.emissive.b` | 0–1 | *(not exported)* |

**glTF export notes:**
- Position/rotation/scale channels are exported as glTF `animations[]`.
- Per-component mc3 channels (e.g., `position.y` only) are merged into VEC3/VEC4 at export time;
  non-animated components fall back to the object's base transform value.
- Cubic bezier channels are sampled at 30 fps and exported as `LINEAR` glTF interpolation.
- `visible`, `deform.*`, and `material.*` channels have no glTF node-transform equivalent
  and are skipped during export with a warning printed to stderr.

---

## Proposed Improvements (v0.4+)

These features are not yet implemented but are planned or recommended for future versions.

### 1. LOD (Level of Detail)

Group multiple mesh representations of the same object by screen size.

```xml
<lod name="Tree">
  <level max_distance="20"  object="tree_high"/>
  <level max_distance="60"  object="tree_mid"/>
  <level max_distance="150" object="tree_low"/>
  <level max_distance="-1"  object="tree_billboard"/>
</lod>
```

### 2. Physics Properties

Per-object physics parameters.

```xml
<box name="Crate" size="1 1 1">
  <physics mass="10" friction="0.5" restitution="0.3" body="dynamic"/>
</box>
```

### 3. Procedural Noise / Scatter

Place objects on a surface following noise or distribution rules.

```xml
<scatter name="ForestFloor" count="200" seed="42" area="-20 20 -20 20">
  <item definition="tree_small" scale_min="0.8" scale_max="1.3"/>
  <item definition="rock"       scale_min="0.5" scale_max="2.0" weight="0.3"/>
</scatter>
```

### 4. Named Coordinate Spaces / Anchors

Named world-space points for attaching objects or logic.

```xml
<anchor name="door_handle" position="0.55 1.05 -4.05"/>
```

### 5. Material Variants

Allow swapping a set of materials at runtime without duplicating objects.

```xml
<material_variant id="night_mode">
  <override material="wall_brick"   with="wall_brick_dark"/>
  <override material="grass"        with="grass_night"/>
</material_variant>
```

### 6. Terrain

Heightmap-based terrain with per-splat material blending.

```xml
<terrain name="Hills" heightmap="terrain/height.png"
         width="256" depth="256" max_height="30">
  <splat at="0.0" material="grass"/>
  <splat at="0.6" material="rock"/>
  <splat at="0.9" material="snow"/>
</terrain>
```

### 7. Instanced Arrays (GPU Instancing Hint)

Mark large groups for GPU instancing.

```xml
<instance_array definition="grass_blade" count="50000" instanced="true">
  <distribution type="uniform" area="-50 50 -50 50"/>
</instance_array>
```

### 8. Schema Version Namespace

Use a proper XML namespace to allow forward compatibility.

```xml
<mc3 xmlns="https://openeggbert.org/mc3/0.3" version="0.3">
```
