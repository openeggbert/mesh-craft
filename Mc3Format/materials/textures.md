# Textures

Textures are declared inside the `<textures>` element and referenced by materials using their `id`.

```xml
<textures>
  <texture id="brick_albedo"
           uri="textures/brick_albedo.png"
           wrap_u="repeat"
           wrap_v="repeat"
           filter="linear"/>

  <texture id="brick_normal"
           uri="textures/brick_normal.png"
           wrap_u="repeat"
           wrap_v="repeat"
           filter="linear"/>
</textures>
```

Then used by materials:

```xml
<material id="brick" roughness="0.9">
  <base_color_texture>brick_albedo</base_color_texture>
  <normal_texture>brick_normal</normal_texture>
</material>
```

## Texture Attributes

| Attribute | Type | Description |
|---|---|---|
| `id` | string | Unique texture identifier. |
| `uri` | string | Path to texture file. |
| `wrap_u` | string | `repeat`, `clamp`, `mirror`. Default: `repeat`. |
| `wrap_v` | string | `repeat`, `clamp`, `mirror`. Default: `repeat`. |
| `filter` | string | `nearest`, `linear`. Default: `linear`. |
| `color_space` | string | `srgb` or `linear`. Default: `srgb`. |
