# Lights

Lights are defined in the top-level `<lights>` element. The scene has no lights by default — if `<lights>` is omitted, the importer may apply a default ambient light at its discretion.

```xml
<lights>
  <ambient color="0.3 0.3 0.3" brightness="0.5"/>

  <directional name="Sun"
               direction="0.58 -0.58 0.58"
               color="1 1 1"
               brightness="1.5"
               cast_shadows="true"/>

  <spot name="Lamp"
        position="2 4 0"
        direction="0 -1 0"
        angle="45"
        falloff="0.2"
        color="1 1 1"
        brightness="1.0"
        cast_shadows="true"/>

  <point name="Bulb"
         position="0 3 0"
         color="1 0.9 0.7"
         brightness="2.0"
         range="10"
         cast_shadows="false"/>
</lights>
```

## Ambient Light

| Attribute | Type | Description |
|---|---|---|
| `color` | vec3 | RGB color, values 0..1. Default: `1 1 1`. |
| `brightness` | number | Intensity multiplier. Default: `1.0`. |

There is at most one `<ambient>` element per `<lights>` block.

## Directional Light

Models a light infinitely far away (like the sun). All rays are parallel.

| Attribute | Type | Description |
|---|---|---|
| `name` | string | Optional name for editor and action references. |
| `direction` | vec3 | Direction the light travels (normalized). |
| `color` | vec3 | RGB color, values 0..1. Default: `1 1 1`. |
| `brightness` | number | Intensity multiplier. Default: `1.0`. |
| `cast_shadows` | bool | Whether this light produces shadows. Default: `false`. |

## Spot Light

A cone-shaped light with a defined origin.

| Attribute | Type | Description |
|---|---|---|
| `name` | string | Optional name. |
| `position` | vec3 | World position of the light source. |
| `direction` | vec3 | Direction the cone points (normalized). Default: `0 -1 0`. |
| `angle` | number | Half-angle of the cone in degrees. Default: `45`. |
| `falloff` | number | Softness of the cone edge, 0..1. Default: `0`. |
| `color` | vec3 | RGB color. Default: `1 1 1`. |
| `brightness` | number | Intensity. Default: `1.0`. |
| `range` | number | Maximum distance. Unlimited if omitted. |
| `cast_shadows` | bool | Default: `false`. |

## Point Light

An omnidirectional light that radiates in all directions from a point.

| Attribute | Type | Description |
|---|---|---|
| `name` | string | Optional name. |
| `position` | vec3 | World position of the light source. |
| `color` | vec3 | RGB color. Default: `1 1 1`. |
| `brightness` | number | Intensity. Default: `1.0`. |
| `range` | number | Maximum distance. Unlimited if omitted. |
| `cast_shadows` | bool | Default: `false`. |

## Example: Outdoor Scene Lighting

```xml
<lights>
  <ambient color="0.2 0.25 0.3" brightness="0.4"/>
  <directional name="Sun" direction="0.58 -0.58 0.58"
               color="1 0.95 0.8" brightness="2.0" cast_shadows="true"/>
</lights>
```

## Example: Indoor Lamp

```xml
<lights>
  <ambient color="0.1 0.1 0.1" brightness="0.3"/>
  <point name="CeilingLight" position="0 3 0"
         color="1 0.9 0.7" brightness="3.0" range="8"/>
</lights>
```
