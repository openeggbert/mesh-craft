# Extruded Shape

An `<extrude>` creates a 3D object by sweeping a 2D cross-section along a path. It is useful for beams, pipes, arches, springs, custom profiles, and any shape that has a consistent cross-section.

```xml
<extrude name="Beam" material="wood" position="0 0 0">
  <cross_section type="rect" width="0.2" height="0.3"/>
  <path type="line" length="4" axis="y"/>
</extrude>
```

## Cross-Section Types

The `<cross_section>` element defines the 2D shape that is swept.

### Rectangle

```xml
<cross_section type="rect" width="0.3" height="0.2"/>
```

| Attribute | Type | Description |
|---|---|---|
| `width` | number | Width of the rectangle. |
| `height` | number | Height of the rectangle. |

### Circle

```xml
<cross_section type="circle" radius="0.1"/>
```

For hollow cross-sections (pipe):

```xml
<cross_section type="circle" radius="0.15" inner_radius="0.12"/>
```

| Attribute | Type | Description |
|---|---|---|
| `radius` | number | Outer radius. |
| `inner_radius` | number | Optional inner radius for hollow shapes. |
| `segments` | integer | Mesh quality. Default: `32`. |

### Regular Polygon

```xml
<cross_section type="polygon" sides="6" radius="0.2"/>
```

| Attribute | Type | Description |
|---|---|---|
| `sides` | integer | Number of sides. |
| `radius` | number | Circumscribed radius. |

### Custom (Bezier)

```xml
<cross_section type="custom">
  <point x="-0.5" y="0"/>
  <point x="0"    y="0.5"/>
  <point x="0.5"  y="0"/>
  <point x="0"    y="-0.2"/>
</cross_section>
```

Points define a closed polygon. Curved segments can be added with `cx`/`cy` control-point attributes on the next point (cubic bezier handle).

## Path Types

The `<path>` element defines the trajectory along which the cross-section is swept.

### Line (straight extrusion)

```xml
<path type="line" length="4" axis="y"/>
```

| Attribute | Type | Description |
|---|---|---|
| `length` | number | Length of the extrusion. |
| `axis` | string | Sweep axis: `x`, `y`, or `z`. Default: `y`. |

### Arc (curved extrusion)

```xml
<path type="arc" radius="3" angle="180" axis="y"/>
```

Creates a curved sweep — useful for arches, curved pipes, and rails.

| Attribute | Type | Description |
|---|---|---|
| `radius` | number | Radius of the arc. |
| `angle` | number | Sweep angle in degrees. |
| `axis` | string | Axis around which the arc curves. Default: `y`. |

### Helix (spring / corkscrew)

```xml
<path type="helix" radius="0.5" height="3" turns="8"/>
```

Creates a helical sweep — useful for springs, screws, and spiral staircases.

| Attribute | Type | Description |
|---|---|---|
| `radius` | number | Radius of the helix. |
| `height` | number | Total height. |
| `turns` | number | Number of full rotations. |

### Polyline (segmented path)

```xml
<path type="polyline">
  <point x="0" y="0" z="0"/>
  <point x="2" y="0" z="0"/>
  <point x="2" y="3" z="0"/>
</path>
```

### Bezier path (free-form)

```xml
<path type="bezier">
  <point x="0"  y="0"  z="0"  cx="0"   cy="1"  cz="0"/>
  <point x="3"  y="3"  z="0"  cx="-1"  cy="3"  cz="0"/>
</path>
```

`cx/cy/cz` define the incoming bezier tangent control point for each point.

## Additional Extrude Attributes

| Attribute | Type | Description |
|---|---|---|
| `twist` | number | Rotation in degrees applied uniformly along the full path. Default: `0`. |
| `segments` | integer | Number of path subdivision steps. Default: `32`. |
| `smooth` | bool | Whether normals are smoothed along the path. Default: `true`. |
| `caps` | bool | Whether end caps are generated. Default: `true`. |

## Examples

### Wooden beam

```xml
<extrude name="Rafter" material="wood" position="-3 4 0" rotation="0 0 45">
  <cross_section type="rect" width="0.15" height="0.1"/>
  <path type="line" length="6" axis="x"/>
</extrude>
```

### Stone arch

```xml
<extrude name="Arch" material="stone">
  <cross_section type="rect" width="0.4" height="0.4"/>
  <path type="arc" radius="2" angle="180" axis="z"/>
</extrude>
```

### Metal pipe

```xml
<extrude name="Pipe" material="metal">
  <cross_section type="circle" radius="0.12" inner_radius="0.10"/>
  <path type="line" length="3" axis="y"/>
</extrude>
```

### Spring

```xml
<extrude name="Spring" material="metal">
  <cross_section type="circle" radius="0.04" segments="16"/>
  <path type="helix" radius="0.4" height="2" turns="10"/>
</extrude>
```

### Spiral staircase handrail

```xml
<extrude name="Handrail" material="varnished_wood" position="0 0 0">
  <cross_section type="circle" radius="0.03" segments="12"/>
  <path type="helix" radius="2.5" height="6" turns="2"/>
</extrude>
```
