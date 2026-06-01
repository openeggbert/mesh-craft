# MC3 Model Source Format

Version: 0.1 draft  
Format type: human-readable source format for procedural / constructive 3D models  
Recommended file extension: `.mc3.yaml` or `.mc3.yml`  
Recommended compiled/runtime export: `.glb` / `.gltf`

## 1. Purpose

MC3 is a simple YAML-based source format for describing 3D models as editable objects rather than only as final triangle meshes.

It is intended for models built from primitives, constructive solid geometry, reusable parts, materials, textures, transforms, groups, and object actions.

Typical use cases:

- houses, rooms, furniture, doors, windows, platforms, machines,
- simple game levels,
- block-based or SketchUp-like modeling,
- procedural assets,
- editable source models that can later be compiled to meshes,
- runtime-interactive objects such as doors, drawers, elevators, buttons, chairs, crates, and moving platforms.

MC3 is not meant to replace glTF as a final runtime asset format. Instead, MC3 should be treated as a source format that can be compiled or exported to glTF/GLB, CNA/Nova-3D internal meshes, or another runtime representation.

Recommended pipeline:

```text
.mc3.yaml  ->  MC3 compiler/importer  ->  mesh / scene graph / GLB
```

## 2. Design Goals

MC3 should be:

- readable by humans,
- easy to generate by tools and AI,
- stable enough for long-term project files,
- simple enough to parse in C++,
- useful for both static and interactive models,
- independent of any specific graphics backend,
- compatible with future export to glTF/GLB,
- suitable as an input format for Nova-3D.

MC3 should avoid:

- storing low-level rendering state,
- storing backend-specific details such as OpenGL, Vulkan, or DirectX handles,
- requiring a full CAD kernel for the first version,
- becoming as complex as USD or Blender files.

## 3. Minimal Example

```yaml
mc3: 0.1
model: House

materials:
  brick:
    base_color: [0.8, 0.2, 0.1, 1.0]
  stone:
    base_color: [0.5, 0.5, 0.5, 1.0]

objects:
  - type: box
    name: Wall
    size: [10, 3, 0.3]
    position: [0, 1.5, 0]
    material: brick

  - type: cylinder
    name: Column
    radius: 0.3
    height: 3
    position: [2, 1.5, 0]
    material: stone
```

## 4. Top-Level Structure

An MC3 file contains a single top-level YAML object.

```yaml
mc3: 0.1
model: ExampleModel
unit: meter
coordinate_system: right_handed_y_up

textures: {}
materials: {}
definitions: {}
objects: []
actions: {}
```

### 4.1 Required Fields

| Field | Type | Description |
|---|---|---|
| `mc3` | number/string | MC3 format version. |
| `model` | string | Human-readable model name. |
| `objects` | list | Main object list. |

### 4.2 Optional Fields

| Field | Type | Description |
|---|---|---|
| `unit` | string | Unit scale, for example `meter`, `centimeter`, `pixel`, `unit`. |
| `coordinate_system` | string | Recommended: `right_handed_y_up`. |
| `textures` | map | Texture definitions. |
| `materials` | map | Material definitions. |
| `definitions` | map | Reusable object templates/prefabs. |
| `actions` | map | Named actions affecting one or more objects. |
| `metadata` | map | Author, license, notes, etc. |

### 4.3 Metadata Keys

The `metadata` field accepts any key-value pairs. Recommended keys:

| Key | Type | Description |
|---|---|---|
| `author` | string | Name or identity of the model author. |
| `license` | string | License identifier, e.g. `CC0`, `MIT`, `CC-BY-4.0`. |
| `description` | string | Short human-readable description of the model. |
| `version` | string | Model version, e.g. `1.0.0`. |
| `created` | string | Creation date in ISO 8601 format, e.g. `2024-01-15`. |
| `tags` | list | Searchable tags for asset libraries. |
| `source_url` | string | URL of the original or upstream source. |
| `notes` | string | Free-form notes for editors or importers. |

Example:

```yaml
metadata:
  author: Jane Doe
  license: CC-BY-4.0
  description: A simple interactive house model
  version: 1.0.0
  created: 2024-06-01
```

## 5. Coordinates and Units

Recommended default:

```yaml
unit: meter
coordinate_system: right_handed_y_up
```

Coordinate meaning:

```text
X = right / left
Y = up / down
Z = forward / backward
```

Vector values use arrays:

```yaml
position: [x, y, z]
rotation: [pitch, yaw, roll]
scale: [x, y, z]
```

Angles are in degrees unless specified otherwise.

```yaml
angle_unit: degrees
```

### 5.1 Rotation Order

Rotation uses extrinsic XYZ Euler order: X (pitch) is applied first, then Y (yaw), then Z (roll), each around the fixed parent axes. This is equivalent to intrinsic ZYX order. Importers must use this convention to ensure consistent behavior across tools.

## 6. Object Basics

Every object has a `type`. Most objects may also have `name`, `position`, `rotation`, `scale`, `material`, `visible`, `collision`, and `tags`.

```yaml
- type: box
  name: Wall
  size: [10, 3, 0.3]
  position: [0, 1.5, 0]
  rotation: [0, 0, 0]
  scale: [1, 1, 1]
  material: brick
  visible: true
  collision: static
  tags: [wall, building]
```

### 6.1 Common Object Fields

| Field | Type | Description |
|---|---|---|
| `type` | string | Object type, e.g. `box`, `cylinder`, `group`, `union`. |
| `name` | string | Unique or human-readable name. Recommended for interactive objects. |
| `id` | string | Optional stable unique ID. Useful for tools and actions. |
| `position` | vec3 | Local position. |
| `rotation` | vec3 | Local Euler rotation in degrees (extrinsic XYZ order). |
| `scale` | vec3/number | Local scale. |
| `pivot` | vec3 | Pivot point for rotation/scale, in local coordinates. |
| `material` | string | Material reference. |
| `visible` | bool | Whether object is rendered. |
| `collision` | string/bool/map | Collision behavior. |
| `tags` | list | Tool/game/editor tags. |
| `children` | list | Child objects for groups and compound objects. |

### 6.2 Default Values

| Field | Default |
|---|---|
| `position` | `[0, 0, 0]` |
| `rotation` | `[0, 0, 0]` |
| `scale` | `[1, 1, 1]` |
| `pivot` | `[0, 0, 0]` |
| `visible` | `true` |
| `collision` | `none` |
| `segments` (sphere, cylinder, cone) | `32` |
| `axis` (cylinder, plane) | `y` |
| `angle_unit` | `degrees` |
| `coordinate_system` | `right_handed_y_up` |
| `alpha_mode` | `opaque` |
| `double_sided` | `false` |
| `wrap_u` / `wrap_v` | `repeat` |
| `filter` | `linear` |
| `color_space` | `srgb` |
| `uv.mode` | `default` |
| `uv.scale` | `[1, 1]` |
| `uv.offset` | `[0, 0]` |
| `uv.rotation` | `0` |
| `relative` (action) | depends on action type |
| `trigger` (action) | `manual` |
| `trigger_on` (area) | `enter` |
| `state` (object) | first listed state, or none |

## 7. Primitive Objects

### 7.1 Box

```yaml
- type: box
  name: Wall
  size: [10, 3, 0.3]
  position: [0, 1.5, 0]
  material: brick
```

Fields:

| Field | Type | Description |
|---|---|---|
| `size` | vec3 | Width, height, depth. |

### 7.2 Cube

Shortcut for a box with equal dimensions.

```yaml
- type: cube
  name: Crate
  size: 1
  material: wood
```

`cube` is a true alias for `box` with `size: [n, n, n]`. The compiler treats a `cube` as fully equivalent to a `box` with three equal side lengths. There is no behavioral or semantic difference between a `cube` with `size: 2` and a `box` with `size: [2, 2, 2]`.

### 7.3 Sphere

```yaml
- type: sphere
  name: Ball
  radius: 0.5
  segments: 32
  material: red_plastic
```

Fields:

| Field | Type | Description |
|---|---|---|
| `radius` | number | Sphere radius. |
| `segments` | integer | Optional mesh quality. |

### 7.4 Cylinder

```yaml
- type: cylinder
  name: Column
  radius: 0.3
  height: 3
  segments: 32
  position: [2, 1.5, 0]
  material: stone
```

Fields:

| Field | Type | Description |
|---|---|---|
| `radius` | number | Radius. |
| `height` | number | Height along the chosen axis. Default axis is Y. |
| `segments` | integer | Optional mesh quality. |
| `axis` | string | Optional: `x`, `y`, or `z`. Default: `y`. |

The `axis` field is a mesh-generation hint: it selects which local axis runs along the cylinder's length during geometry generation. It does **not** apply a rotation transform. The generated mesh is oriented so that the specified axis is the height axis.

`axis` and `rotation` may be combined freely. `axis` controls how the mesh is generated internally; `rotation` is then applied as a standard object transform on top of the generated geometry.

### 7.5 Cone

```yaml
- type: cone
  name: RoofCone
  radius: 2
  height: 1.5
  segments: 32
  material: roof_tile
```

### 7.6 Plane

```yaml
- type: plane
  name: Floor
  size: [10, 10]
  axis: y
  material: floor_tiles
```

A plane is usually generated as a flat mesh.

### 7.7 Mesh Reference

MC3 may reference external mesh assets for complex objects.

```yaml
- type: mesh
  name: ChairDetailed
  source: assets/chair.glb
  position: [2, 0, 3]
  material_override: varnished_wood
```

This allows simple MC3 objects and imported artist-made assets to coexist.

## 8. Groups and Hierarchy

Groups allow multiple objects to share a transform.

```yaml
- type: group
  name: Table
  position: [0, 0, 0]
  children:
    - type: box
      name: TableTop
      size: [2, 0.1, 1]
      position: [0, 1, 0]
      material: wood

    - type: box
      name: Leg1
      size: [0.1, 1, 0.1]
      position: [-0.9, 0.5, -0.4]
      material: wood
```

Transforms are hierarchical:

```text
world_transform = parent_transform * local_transform
```

## 9. Materials

Materials are defined at the top level and referenced by name.

```yaml
materials:
  brick:
    base_color: [0.8, 0.2, 0.1, 1.0]
    roughness: 0.8
    metallic: 0.0

  glass:
    base_color: [0.7, 0.9, 1.0, 0.35]
    roughness: 0.05
    metallic: 0.0
    alpha_mode: blend
```

### 9.1 Basic Material Fields

| Field | Type | Description |
|---|---|---|
| `base_color` | vec4 | RGBA color, values 0..1. |
| `base_color_texture` | string | Texture reference. |
| `normal_texture` | string | Normal map reference. |
| `roughness` | number | 0..1. |
| `metallic` | number | 0..1. |
| `alpha_mode` | string | `opaque`, `mask`, or `blend`. |
| `double_sided` | bool | Render both sides. |
| `emissive_color` | vec3 | Emission color. |
| `emissive_texture` | string | Emission texture. |

## 10. Textures

Textures are declared separately and referenced by materials.

```yaml
textures:
  brick_albedo:
    uri: textures/brick_albedo.png
    wrap_u: repeat
    wrap_v: repeat
    filter: linear

  brick_normal:
    uri: textures/brick_normal.png
    wrap_u: repeat
    wrap_v: repeat
    filter: linear
```

Then used by materials:

```yaml
materials:
  brick:
    base_color_texture: brick_albedo
    normal_texture: brick_normal
    roughness: 0.9
```

### 10.1 Texture Fields

| Field | Type | Description |
|---|---|---|
| `uri` | string | Path to texture file. |
| `wrap_u` | string | `repeat`, `clamp`, `mirror`. |
| `wrap_v` | string | `repeat`, `clamp`, `mirror`. |
| `filter` | string | `nearest`, `linear`. |
| `color_space` | string | `srgb` or `linear`. |

## 11. UV Mapping and Texture Application

Objects can define how textures are applied.

```yaml
- type: box
  name: Wall
  size: [10, 3, 0.3]
  material: brick
  uv:
    mode: box
    scale: [2, 2]
    offset: [0, 0]
    rotation: 0
```

### 11.1 UV Modes

| Mode | Description |
|---|---|
| `default` | Importer chooses reasonable defaults. |
| `box` | Box projection, good for walls/crates. |
| `planar` | Projection onto one plane. |
| `cylindrical` | Good for cylinders. |
| `spherical` | Good for spheres. |
| `custom` | External or manually provided UV data. |

Example for cylinder:

```yaml
- type: cylinder
  name: Column
  radius: 0.3
  height: 3
  material: stone
  uv:
    mode: cylindrical
    scale: [1, 3]
```

## 12. Constructive Solid Geometry: CSG

MC3 supports CSG operations for creating objects from primitives.

Required CSG operations:

- `union`
- `difference`
- `intersection`

### 12.1 Union

Combines objects into one generated solid/mesh.

```yaml
- type: union
  name: SimpleHouseBody
  children:
    - type: box
      name: MainBlock
      size: [8, 3, 6]
      position: [0, 1.5, 0]
      material: plaster

    - type: box
      name: RoofBlock
      size: [8.5, 1, 6.5]
      position: [0, 3.5, 0]
      rotation: [0, 0, 45]
      material: roof_tile
```

### 12.2 Difference

Subtracts one or more child shapes from the first child.

```yaml
- type: difference
  name: WallWithDoorHole
  children:
    - type: box
      name: WallBase
      size: [5, 3, 0.3]
      position: [0, 1.5, 0]
      material: brick

    - type: box
      name: DoorHole
      size: [1, 2.2, 0.5]
      position: [0, 1.1, 0]
      role: cutter
      visible: false
```

Meaning:

```text
result = WallBase - DoorHole
```

The `role: cutter` field explicitly marks an object as a subtraction volume. This is a reserved role; importers must treat any object with `role: cutter` as a CSG subtracter regardless of its material name. Setting `visible: false` on cutter objects is recommended. The material assigned to a cutter object has no effect on the result.

### 12.3 Intersection

Keeps only the overlapping volume.

```yaml
- type: intersection
  name: RoundedPartApproximation
  children:
    - type: box
      size: [2, 2, 2]
    - type: sphere
      radius: 1.4
```

### 12.4 CSG Implementation Notes

For version 0.1, CSG may be implemented in one of two ways:

1. Real mesh boolean operations.
2. Delayed representation converted by an external CSG library/tool.

A minimal importer may initially support only non-CSG primitives and groups, then add CSG later.

Recommended behavior:

- CSG children should be evaluated in local coordinates.
- Materials from the first child should be used by default.
- Cutter objects should be identified by `role: cutter` and should usually also set `visible: false`.
- CSG results should be cacheable.

## 13. Reusable Definitions / Prefabs

Reusable templates are stored in `definitions`.

```yaml
definitions:
  SimpleChair:
    type: group
    children:
      - type: box
        name: Seat
        size: [1, 0.15, 1]
        position: [0, 0.6, 0]
        material: wood

      - type: box
        name: Back
        size: [1, 1, 0.15]
        position: [0, 1.1, 0.45]
        material: wood
```

Instances use `type: instance`:

```yaml
objects:
  - type: instance
    name: ChairA
    definition: SimpleChair
    position: [1, 0, 2]

  - type: instance
    name: ChairB
    definition: SimpleChair
    position: [3, 0, 2]
    rotation: [0, 90, 0]
```

### 13.1 Instance Field Override Rules

Fields set directly on a `type: instance` node override the corresponding fields from the definition. If a field is not set on the instance node, the value from the definition is used.

Fields that can be overridden on an instance: `position`, `rotation`, `scale`, `material`, `visible`, `collision`, `tags`, `state`.

When `collision` is set on an instance node, it replaces any collision behavior defined inside the definition. The override applies to the instance as a whole and is intended to control the top-level physics body of the instanced object. Importers may propagate the collision override to the root-level children of the definition if the definition does not have a single top-level physics object.

## 14. Pivots

Pivots are important for doors, lids, levers, wheels, and rotating objects.

```yaml
- type: box
  name: FrontDoor
  size: [1, 2.1, 0.08]
  position: [0, 1.05, -3.05]
  pivot: [-0.5, 0, 0]
  material: painted_wood
```

Here the door rotates around its left edge.

Pivot coordinates are local to the object before transform.

## 15. Actions

Actions describe named transformations or state changes that may happen at runtime or in an editor.

Examples:

- open a door,
- close a door,
- move a chair,
- slide a drawer,
- rotate a lever,
- hide/show an object,
- change material,
- play an animation.

Actions are declared in the top-level `actions` map.

```yaml
actions:
  open_front_door:
    target: FrontDoor
    type: rotate
    axis: y
    angle: 90
    duration: 0.6
    easing: ease_out
```

### 15.1 Basic Action Fields

| Field | Type | Description |
|---|---|---|
| `target` | string/list | Object name or ID. |
| `type` | string | Action type. |
| `duration` | number | Duration in seconds. |
| `easing` | string | Interpolation mode. |
| `relative` | bool | Whether transform is relative to current state. Default depends on action. |
| `trigger` | string/map | Optional trigger definition. |

## 16. Action Types

### 16.1 Rotate

```yaml
actions:
  open_front_door:
    target: FrontDoor
    type: rotate
    axis: y
    angle: 90
    duration: 0.6
    easing: ease_out
```

Alternative vector form:

```yaml
actions:
  rotate_part:
    target: SomeObject
    type: rotate
    rotation: [0, 90, 0]
    duration: 1.0
```

### 16.2 Move / Translate

```yaml
actions:
  move_chair_aside:
    target: ChairA
    type: translate
    offset: [1.0, 0, 0]
    duration: 0.4
    easing: ease_in_out
```

### 16.3 Set Position

```yaml
actions:
  reset_chair_position:
    target: ChairA
    type: set_position
    position: [1, 0, 2]
    duration: 0.3
```

### 16.4 Scale

```yaml
actions:
  grow_platform:
    target: Platform
    type: scale
    scale: [2, 1, 2]
    duration: 1.0
```

### 16.5 Show / Hide

```yaml
actions:
  hide_secret_wall:
    target: SecretWall
    type: set_visible
    visible: false
```

### 16.6 Change Material

```yaml
actions:
  turn_lamp_on:
    target: LampBulb
    type: set_material
    material: glowing_yellow
```

### 16.7 Sequence

Runs actions one after another.

```yaml
actions:
  open_door_and_move_chair:
    type: sequence
    steps:
      - action: open_front_door
      - action: move_chair_aside
```

### 16.8 Parallel

Runs actions at the same time.

```yaml
actions:
  open_double_door:
    type: parallel
    steps:
      - target: LeftDoor
        type: rotate
        axis: y
        angle: -90
        duration: 0.6
      - target: RightDoor
        type: rotate
        axis: y
        angle: 90
        duration: 0.6
```

### 16.9 Toggle

Switches between two states.

```yaml
actions:
  toggle_front_door:
    type: toggle
    states:
      closed:
        target: FrontDoor
        type: rotate
        axis: y
        angle: 0
        duration: 0.5
      open:
        target: FrontDoor
        type: rotate
        axis: y
        angle: 90
        duration: 0.5
```

`toggle` is syntactic sugar over `set_state`. Each invocation of the toggle action advances the object to the next listed state in order, cycling back to the first state after the last. It is equivalent to calling `set_state` with the next state name.

When a `toggle` action and a `states` block are both present on the same object, the toggle cycles through the states declared in the object's `states` map. The two mechanisms are complementary: use `states` on the object to declare named configurations (see section 18), and use a `toggle` action to cycle through them with a single call. Direct `set_state` actions remain available for explicit state targeting. Importers must not treat `toggle` and `states` as conflicting; they are independent layers of the same state machine.

## 17. Triggers

Triggers are optional. They describe when an action should run.

```yaml
actions:
  open_front_door:
    target: FrontDoor
    type: rotate
    axis: y
    angle: 90
    duration: 0.6
    trigger:
      type: interact
      prompt: Open door
```

Possible trigger types:

| Trigger | Description |
|---|---|
| `interact` | Player/user interaction. |
| `on_load` | Runs when model/scene is loaded. |
| `on_collision` | Runs when collision occurs. |
| `on_enter_area` | Runs when something enters a volume. |
| `on_signal` | Runs when game code sends a named signal. |
| `manual` | Only invoked by code/editor. |

Example signal trigger:

```yaml
actions:
  open_gate:
    target: Gate
    type: translate
    offset: [0, 3, 0]
    duration: 1.5
    trigger:
      type: on_signal
      signal: gate_opened
```

## 18. States

Objects may define named states.

```yaml
- type: box
  name: FrontDoor
  size: [1, 2.1, 0.08]
  pivot: [-0.5, 0, 0]
  material: painted_wood
  state: closed
  states:
    closed:
      rotation: [0, 0, 0]
    open:
      rotation: [0, 90, 0]
```

Actions can change state:

```yaml
actions:
  set_door_open:
    target: FrontDoor
    type: set_state
    state: open
    duration: 0.6
```

`states` declares named configurations for an object. Each state is a partial set of object fields that override the base values when the object is in that state. A `toggle` action (section 16.9) is syntactic sugar that cycles through these states. For interactive objects with exactly two configurations (open/closed, on/off), either approach is acceptable; for objects with three or more configurations, use `states` with explicit `set_state` actions or a single `toggle`.

## 19. Collision and Physics

Simple collision examples:

```yaml
- type: box
  name: Wall
  size: [10, 3, 0.3]
  collision: static
```

```yaml
- type: box
  name: Crate
  size: [1, 1, 1]
  collision:
    type: dynamic
    mass: 10
```

Recommended collision types:

| Value | Description |
|---|---|
| `none` | No collision. |
| `static` | Static solid object. |
| `dynamic` | Dynamic physics body. |
| `kinematic` | Moved by code/actions. |
| `trigger` | Detects overlap but does not block. |

Detailed form:

```yaml
collision:
  type: kinematic
  shape: box
  layer: environment
  mask: [player, npc]
```

## 20. Areas and Interaction Volumes

An invisible area can trigger actions.

```yaml
- type: area
  name: DoorInteractionArea
  shape: box
  size: [1.5, 2, 1]
  position: [0, 1, -3]
  visible: false
  trigger_actions:
    - open_front_door
```

### 20.1 Area Fields

| Field | Type | Description |
|---|---|---|
| `shape` | string | Shape of the area volume: `box`, `sphere`, or `cylinder`. |
| `size` | vec3 / number / [number, number] | Dimensions of the area. For `box`: vec3 `[w, h, d]`. For `sphere`: number (radius). For `cylinder`: `[radius, height]`. |
| `trigger_actions` | list | List of action names to invoke when the trigger fires. Actions are called in list order. |
| `trigger_on` | string | When to fire: `enter` (default), `exit`, or `stay`. |
| `collision_layer` | string | Collision layer this area belongs to. |
| `collision_mask` | list | Layers whose objects can activate this area. If omitted, all layers activate the area. |

## 21. Complete House Example

```yaml
mc3: 0.1
model: InteractiveHouse
unit: meter
coordinate_system: right_handed_y_up

textures:
  brick_albedo:
    uri: textures/brick_albedo.png
    wrap_u: repeat
    wrap_v: repeat
    filter: linear

  wood_albedo:
    uri: textures/wood_albedo.png
    wrap_u: repeat
    wrap_v: repeat
    filter: linear

materials:
  brick:
    base_color_texture: brick_albedo
    roughness: 0.9

  painted_wood:
    base_color_texture: wood_albedo
    roughness: 0.6

  glass:
    base_color: [0.7, 0.9, 1.0, 0.35]
    alpha_mode: blend
    roughness: 0.05

objects:
  - type: difference
    name: FrontWallWithDoorHole
    children:
      - type: box
        name: FrontWallBase
        size: [8, 3, 0.3]
        position: [0, 1.5, -3]
        material: brick
        uv:
          mode: box
          scale: [2, 2]

      - type: box
        name: DoorHole
        size: [1.1, 2.2, 0.5]
        position: [0, 1.1, -3]
        role: cutter
        visible: false

  - type: box
    name: FrontDoor
    size: [1, 2.1, 0.08]
    position: [0, 1.05, -3.08]
    pivot: [-0.5, 0, 0]
    material: painted_wood
    collision: kinematic
    state: closed
    states:
      closed:
        rotation: [0, 0, 0]
      open:
        rotation: [0, 90, 0]

  - type: box
    name: WindowGlass
    size: [1.2, 1.0, 0.05]
    position: [2.2, 1.7, -3.08]
    material: glass

  - type: instance
    name: ChairA
    definition: SimpleChair
    position: [1.5, 0, 1.5]
    rotation: [0, 20, 0]
    collision: dynamic

definitions:
  SimpleChair:
    type: group
    children:
      - type: box
        name: Seat
        size: [1, 0.15, 1]
        position: [0, 0.55, 0]
        material: painted_wood
      - type: box
        name: Back
        size: [1, 1, 0.15]
        position: [0, 1.1, 0.45]
        material: painted_wood
      - type: box
        name: Leg1
        size: [0.1, 0.55, 0.1]
        position: [-0.4, 0.275, -0.4]
        material: painted_wood
      - type: box
        name: Leg2
        size: [0.1, 0.55, 0.1]
        position: [0.4, 0.275, -0.4]
        material: painted_wood
      - type: box
        name: Leg3
        size: [0.1, 0.55, 0.1]
        position: [-0.4, 0.275, 0.4]
        material: painted_wood
      - type: box
        name: Leg4
        size: [0.1, 0.55, 0.1]
        position: [0.4, 0.275, 0.4]
        material: painted_wood

actions:
  open_front_door:
    target: FrontDoor
    type: set_state
    state: open
    duration: 0.6
    easing: ease_out
    trigger:
      type: interact
      prompt: Open door

  close_front_door:
    target: FrontDoor
    type: set_state
    state: closed
    duration: 0.6
    easing: ease_in
    trigger:
      type: interact
      prompt: Close door

  move_chair_aside:
    target: ChairA
    type: translate
    offset: [1.0, 0, 0]
    duration: 0.4
    easing: ease_in_out
    trigger:
      type: on_signal
      signal: move_chair
```

## 22. Importer / Compiler Behavior

An MC3 importer should perform these steps:

1. Parse YAML.
2. Validate top-level fields.
3. Load textures.
4. Create materials.
5. Resolve definitions and instances.
6. Build object hierarchy.
7. Generate primitive meshes.
8. Evaluate CSG where supported.
9. Generate UV coordinates.
10. Create collision shapes where supported.
11. Register actions and triggers.
12. Export or create runtime scene objects.

### 22.1 Error Handling

MC3 importers must define behavior for the following error conditions:

| Condition | Recommended response |
|---|---|
| Unknown top-level field | Warning, ignore field. |
| Unknown object `type` | Warning, skip object. |
| Missing required field on object | Error, skip object. |
| `type: instance` references unknown `definition` | Error, skip instance. |
| Action `target` references unknown object name or ID | Warning, skip action. |
| Object references unknown material name | Warning, use default material. |
| Material references unknown texture name | Warning, use fallback texture. |

**Missing definition reference**: When a `type: instance` node references a definition name that does not exist in the `definitions` map, the importer must report an error and skip that instance. The rest of the document must continue loading.

**Missing action target**: When an action's `target` references an object name or ID that does not exist in the scene, the importer must report a warning and skip that action. The rest of the document must continue loading.

**Missing material reference**: When an object references a material name that is not defined in the `materials` map, the importer must report a warning and substitute a default material (for example, a flat grey diffuse material).

**Missing texture reference**: When a material references a texture name that is not defined in the `textures` map, the importer must report a warning and use a fallback texture (for example, a 1×1 white pixel).

## 23. Recommended C++ Runtime Mapping

Possible Nova-3D mapping:

```text
MC3 object       -> Nova3D::Node / Entity
primitive        -> Nova3D::Mesh
material         -> Nova3D::Material
texture          -> Nova3D::Texture2D
children         -> scene graph children
action           -> Nova3D::Action / Animation / Script command
collision        -> Nova3D::Collider / PhysicsBody
```

Suggested classes:

```text
MC3Document
MC3TextureDef
MC3MaterialDef
MC3ObjectDef
MC3ActionDef
MC3Importer
MC3MeshBuilder
MC3CSGBuilder
MC3ActionSystem
```

## 24. Versioning

The `mc3` field should be used for compatibility.

```yaml
mc3: 0.1
```

Breaking changes should increase the major or minor version.

Recommended policy:

```text
0.1 = experimental draft
0.2 = more primitives/materials/actions
1.0 = stable basic format
```

## 25. Recommended Minimal Implementation Order

For CNA/Nova-3D, implement in this order:

1. YAML parser and validation.
2. `box`, `sphere`, `cylinder` primitives.
3. basic transforms: position, rotation, scale.
4. materials with `base_color`.
5. textures and simple UV mapping.
6. groups and hierarchy.
7. instances/definitions.
8. actions: rotate, translate, set_state.
9. collision metadata.
10. CSG `union`.
11. CSG `difference`.
12. CSG `intersection`.
13. export to GLB/glTF.
14. editor integration.

## 26. Notes on CSG vs Runtime Objects

CSG is useful for generating static geometry such as walls with holes, windows, arches, tunnels, pipes, and cutouts.

Interactive objects should usually not be baked into the same CSG mesh if they need to move.

Good:

```text
WallWithDoorHole = wall minus door hole
Door = separate rotating object
```

Bad:

```text
WholeHouseWithDoor = one baked CSG mesh where the door is part of the wall
```

If an object needs actions, keep it as a separate named object.

## 27. Best Practices

- Give names to all objects that may be animated or referenced by actions.
- Use `id` for stable tool references if object names may change.
- Mark CSG subtracters with `role: cutter` and set `visible: false`.
- Keep doors, drawers, chairs, crates, and buttons separate from static CSG geometry.
- Use pivots for rotating objects.
- Prefer top-level material definitions over inline materials.
- Use definitions for repeated objects.
- Use simple primitives first; add mesh references only when needed.
- Keep the source `.mc3.yaml` as the editable truth.
- Treat `.glb` as compiled/exported output.

## 28. Future Extensions

Possible future features:

- curves and paths,
- stairs generator,
- roof generator,
- terrain patches,
- bevels and rounded boxes,
- LOD levels,
- light definitions,
- cameras,
- skeleton animation references,
- particle emitters,
- navmesh hints,
- editor gizmo metadata,
- constraints,
- procedural randomization,
- scripting hooks,
- import/export profiles,
- glTF extension mapping.

## 29. Summary

MC3 is a source format for describing 3D models as editable constructive objects.

It should support:

- primitive shapes,
- groups and hierarchies,
- materials and textures,
- UV mapping,
- CSG operations such as union, difference, and intersection,
- reusable definitions,
- object pivots,
- actions and states,
- interactive runtime behavior.

The recommended architecture is:

```text
MC3 YAML source -> Nova-3D importer/compiler -> generated meshes and scene nodes -> optional GLB export
```

This keeps modeling simple and human-readable while still allowing the engine to use efficient runtime assets.
