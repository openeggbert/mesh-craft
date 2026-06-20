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
  <environment>...</environment>
  <lights>...</lights>
  <cameras>...</cameras>
  <textures>...</textures>
  <materials>...</materials>
  <definitions>...</definitions>
  <objects>...</objects>
  <actions>...</actions>
</mc3>
```

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

Supported by the MeshCraft editor. **Not yet exported by `mc3togltf`** (objects skipped with a warning).

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

**Export note:** `mc3togltf` does **not** evaluate CSG booleans. Children are exported as individual meshes with a warning.

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
| Torus, Capsule, Disk, Grid, IcoSphere | ❌ (warning emitted) |
| CSG (union/difference/intersection) | ❌ (warning, children exported separately) |
| Instance (via definitions) | ✅ |

---

## Validation

The XSD schema is at `mc3/mc3.xsd`. Validate with:

```sh
python3 test/validate_xsd.py mc3/mc3.xsd scene.mc3.xml
```
