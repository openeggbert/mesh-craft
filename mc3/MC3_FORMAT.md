# MC3 Scene Format — Reference Documentation

**Version:** 0.1  
**File extension:** `.mc3.xml`  
**Encoding:** UTF-8  
**Schema:** `mc3.xsd`

---

## Overview

MC3 is an XML-based 3D scene description format used by the OpenEggbert project.
A scene is composed of geometric objects (primitives, extrusions, CSG booleans,
instances of reusable definitions), lights, cameras, materials, and textures.
The output pipeline exports to `.glb` via `mc3togltf`.

---

## Root Element

```xml
<mc3 version="0.1" model="MyScene" unit="meter">
  ...
</mc3>
```

| Attribute | Type | Default | Description |
|---|---|---|---|
| `version` | string | `0.1` | Format version |
| `model` | string | — | Scene/model name |
| `unit` | `meter` \| `centimeter` \| `inch` | `meter` | World unit |
| `coordinate_system` | `right_handed_y_up` \| `right_handed_z_up` | `right_handed_y_up` | Coordinate convention |
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

**Child elements** (all optional, content = texture id reference or color value):

| Element | Content | Description |
|---|---|---|
| `<base_color>` | RGBA vec4 | Base color tint (multiplied with texture) |
| `<emissive_color>` | RGB vec3 | Emissive light color |
| `<base_color_texture>` | texture id | Albedo / diffuse map |
| `<metallic_roughness_texture>` | texture id | ORM map: R=occlusion, G=roughness, B=metallic |
| `<normal_texture>` | texture id | Tangent-space normal map |
| `<occlusion_texture>` | texture id | Ambient occlusion map |
| `<emissive_texture>` | texture id | Emissive map |

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
| `rotation` | vec3 (°) | `0 0 0` | Euler rotation (XYZ, degrees) |
| `scale` | vec3 | `1 1 1` | Scale per axis |
| `pivot` | vec3 | `0 0 0` | Rotation/scale pivot offset (local space) |
| `material` | string | — | Reference to a material id |
| `visible` | boolean | `true` | Object visibility |
| `collision` | string | `none` | Collision shape hint (`none`, `box`, `mesh`, …) |
| `tags` | string | — | Space-separated tag list |
| `role` | `cutter` | — | Marks object as a CSG cutter inside `<difference>` |

All objects may contain an optional `<deform>` child:

```xml
<deform scale="2.0 1.0 0.5"/>
```

Deform applies a **geometry-level** non-uniform scale (applied before the transform),
independent of the `scale` attribute (which is part of the transform).

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
| `definition` | string | Reference to a definition id |
| `material` | string | Material override (replaces per-object materials in the definition) |

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
<mc3 version="0.1" model="Minimal">

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

## Proposed Improvements (v0.2+)

These features are not yet implemented but are planned or recommended for future versions.

### 1. Animation — Actions & States

Add an `<actions>` section to define keyframe animations and state machines.

```xml
<actions>
  <action name="door_open">
    <keyframe time="0"   target="Door" property="rotation" value="0 0 0"/>
    <keyframe time="1.5" target="Door" property="rotation" value="0 90 0"/>
  </action>
</actions>

<states>
  <state name="idle"  on_enter="door_close"/>
  <state name="open"  on_enter="door_open"/>
  <transition from="idle" to="open" on="interact"/>
</states>
```

### 2. LOD (Level of Detail)

Group multiple mesh representations of the same object by screen size.

```xml
<lod name="Tree">
  <level max_distance="20"  object="tree_high"/>
  <level max_distance="60"  object="tree_mid"/>
  <level max_distance="150" object="tree_low"/>
  <level max_distance="-1"  object="tree_billboard"/>
</lod>
```

### 3. Physics Properties

Per-object physics parameters.

```xml
<box name="Crate" size="1 1 1">
  <physics mass="10" friction="0.5" restitution="0.3" body="dynamic"/>
</box>
```

### 4. Procedural Noise / Scatter

Place objects on a surface following noise or distribution rules.

```xml
<scatter name="ForestFloor" count="200" seed="42" area="-20 20 -20 20">
  <item definition="tree_small" scale_min="0.8" scale_max="1.3"/>
  <item definition="rock"       scale_min="0.5" scale_max="2.0" weight="0.3"/>
</scatter>
```

### 5. Named Coordinate Spaces / Anchors

Named world-space points for attaching objects or logic.

```xml
<anchor name="door_handle" position="0.55 1.05 -4.05"/>
```

### 6. Material Variants

Allow swapping a set of materials at runtime without duplicating objects.

```xml
<material_variant id="night_mode">
  <override material="wall_brick"   with="wall_brick_dark"/>
  <override material="grass"        with="grass_night"/>
</material_variant>
```

### 7. Explicit UV Mapping on Primitives

Control UV scale/offset per object to avoid atlas bleeding and seam issues.

```xml
<box name="Wall" size="5 3 0.3">
  <uv_mapping scale="2.5 1.5" offset="0 0" rotation="0"/>
</box>
```

### 8. Terrain

Heightmap-based terrain with per-splat material blending.

```xml
<terrain name="Hills" heightmap="terrain/height.png"
         width="256" depth="256" max_height="30">
  <splat at="0.0" material="grass"/>
  <splat at="0.6" material="rock"/>
  <splat at="0.9" material="snow"/>
</terrain>
```

### 9. Instanced Arrays (GPU Instancing Hint)

Mark large groups for GPU instancing.

```xml
<instance_array definition="grass_blade" count="50000" instanced="true">
  <distribution type="uniform" area="-50 50 -50 50"/>
</instance_array>
```

### 10. Schema Version Namespace

Use a proper XML namespace to allow forward compatibility.

```xml
<mc3 xmlns="https://openeggbert.org/mc3/0.2" version="0.2">
```
