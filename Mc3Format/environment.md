# Global Environment

The `<environment>` element defines scene-wide visual settings such as background color and fog. It is a top-level optional element.

```xml
<environment>
  <background color="0.5 0.7 1.0"/>
  <fog color="0.5 0.7 1.0" density="0.02" start="20" end="80"/>
</environment>
```

## Background

Defines what is rendered behind all scene objects.

```xml
<background color="0.53 0.81 0.98"/>
```

| Attribute | Type | Description |
|---|---|---|
| `color` | vec3 | RGB sky/background color. Default: `0 0 0`. |
| `texture` | string | Optional texture reference (by `id`) used as a sky panorama. |

## Fog

Linear or exponential depth fog applied to all objects.

```xml
<fog color="0.6 0.6 0.6" mode="linear" start="10" end="60"/>
```

```xml
<fog color="0.8 0.8 0.8" mode="exponential" density="0.04"/>
```

| Attribute | Type | Description |
|---|---|---|
| `color` | vec3 | RGB fog color. |
| `mode` | string | `linear` or `exponential`. Default: `linear`. |
| `start` | number | Distance at which fog begins (linear mode). |
| `end` | number | Distance at which fog is fully opaque (linear mode). |
| `density` | number | Fog density coefficient (exponential mode). |

## Relationship with Lights

Ambient light belongs in `<lights>` (see [lights.md](lights.md)), not in `<environment>`. The `<environment>` element covers purely visual scene-background settings; `<lights>` covers all light sources including ambient.

## Full Example

```xml
<environment>
  <background color="0.53 0.81 0.98"/>
  <fog color="0.53 0.81 0.98" mode="linear" start="40" end="120"/>
</environment>

<lights>
  <ambient color="0.3 0.4 0.5" brightness="0.4"/>
  <directional name="Sun" direction="0.58 -0.58 0.58"
               color="1 0.95 0.8" brightness="2.0" cast_shadows="true"/>
</lights>
```
