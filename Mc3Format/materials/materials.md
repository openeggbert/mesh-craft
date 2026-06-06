# Materials

Materials are defined at the top level inside `<materials>` and referenced by their `id`.

```xml
<materials>
  <material id="brick" roughness="0.8" metallic="0.0">
    <base_color>0.8 0.2 0.1 1.0</base_color>
  </material>

  <material id="glass" roughness="0.05" metallic="0.0" alpha_mode="blend">
    <base_color>0.7 0.9 1.0 0.35</base_color>
  </material>
</materials>
```

## Material Attributes

| Attribute | Type | Description |
|---|---|---|
| `id` | string | Unique material identifier. |
| `roughness` | number | 0..1. |
| `metallic` | number | 0..1. |
| `alpha_mode` | string | `opaque`, `mask`, or `blend`. |
| `double_sided` | bool | Render both sides. |

## Material Child Elements

| Element | Type | Description |
|---|---|---|
| `<base_color>` | vec4 text | RGBA color as space-separated `r g b a`, values 0..1. |
| `<base_color_texture>` | string text | Texture reference by `id`. |
| `<normal_texture>` | string text | Normal map texture reference by `id`. |
| `<emissive_color>` | vec3 text | Emission color as space-separated `r g b`. |
| `<emissive_texture>` | string text | Emission texture reference by `id`. |

## Example with Texture References

```xml
<material id="brick" roughness="0.9">
  <base_color_texture>brick_albedo</base_color_texture>
  <normal_texture>brick_normal</normal_texture>
</material>
```

Objects reference materials by `id`:

```xml
<box name="Wall" size="10 3 0.3" material="brick"/>
```
