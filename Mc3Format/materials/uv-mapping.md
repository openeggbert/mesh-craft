# UV Mapping and Texture Application

Objects can define how textures are applied using the `<uv>` child element.

```xml
<box name="Wall" size="10 3 0.3" material="brick">
  <uv mode="box" scale="2 2" offset="0 0" rotation="0"/>
</box>
```

## UV Attributes

| Attribute | Type | Description |
|---|---|---|
| `mode` | string | UV projection mode. Default: `default`. |
| `scale` | vec2 | Texture scale as `u v`. Default: `1 1`. |
| `offset` | vec2 | Texture offset as `u v`. Default: `0 0`. |
| `rotation` | number | Rotation in degrees. Default: `0`. |

## UV Modes

| Mode | Description |
|---|---|
| `default` | Importer chooses reasonable defaults. |
| `box` | Box projection, good for walls and crates. |
| `planar` | Projection onto one plane. |
| `cylindrical` | Good for cylinders. |
| `spherical` | Good for spheres. |
| `custom` | External or manually provided UV data. |

## Example for Cylinder

```xml
<cylinder name="Column" radius="0.3" height="3" material="stone">
  <uv mode="cylindrical" scale="1 3"/>
</cylinder>
```
