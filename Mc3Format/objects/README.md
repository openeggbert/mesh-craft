# Object Basics

Every object element has an element name that represents its `type` (e.g. `<box>`, `<cylinder>`, `<group>`). Most objects may also have `name`, `position`, `rotation`, `scale`, `material`, `visible`, `collision`, and `tags` attributes.

```xml
<box name="Wall"
     size="10 3 0.3"
     position="0 1.5 0"
     rotation="0 0 0"
     scale="1 1 1"
     material="brick"
     visible="true"
     collision="static">
  <tags>wall building</tags>
</box>
```

## Common Object Attributes

| Attribute | Type | Description |
|---|---|---|
| element name | string | Object type, e.g. `box`, `cylinder`, `group`, `union`. |
| `name` | string | Unique or human-readable name. Recommended for interactive objects. |
| `id` | string | Optional stable unique ID. Useful for tools and actions. |
| `position` | vec3 | Local position as space-separated `x y z`. |
| `rotation` | vec3 | Local Euler rotation in degrees (extrinsic XYZ order), space-separated. |
| `scale` | vec3/number | Local scale as space-separated `x y z`, or a single number for uniform scale. |
| `pivot` | vec3 | Pivot point for rotation/scale, in local coordinates. |
| `material` | string | Material reference (by `id`). |
| `visible` | bool | Whether object is rendered. |
| `collision` | string | Simple collision type shorthand. |
| `role` | string | Reserved. Use `cutter` to mark CSG subtracters. |

## Common Object Child Elements

| Element | Description |
|---|---|
| `<tags>` | Space-separated tool/game/editor tags. |
| `<collision>` | Complex collision definition. |
| `<uv>` | UV mapping settings. |
| `<states>` | Named object state configurations. |

## Default Values

| Field | Default |
|---|---|
| `position` | `0 0 0` |
| `rotation` | `0 0 0` |
| `scale` | `1 1 1` |
| `pivot` | `0 0 0` |
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
| `uv mode` | `default` |
| `uv scale` | `1 1` |
| `uv offset` | `0 0` |
| `uv rotation` | `0` |
| `relative` (action) | depends on action type |
| `trigger` (action) | `manual` |
| `trigger_on` (area) | `enter` |
| `state` (object) | first listed state, or none |
