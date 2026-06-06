# Collision and Physics

## Simple Form

Use the `collision` attribute for the common cases:

```xml
<box name="Wall" size="10 3 0.3" collision="static"/>
```

## Complex Form

Use the `<collision>` child element for physics properties:

```xml
<box name="Crate" size="1 1 1">
  <collision type="dynamic" mass="10"/>
</box>
```

## Recommended Collision Types

| Value | Description |
|---|---|
| `none` | No collision. |
| `static` | Static solid object. |
| `dynamic` | Dynamic physics body. |
| `kinematic` | Moved by code/actions. |
| `trigger` | Detects overlap but does not block. |

## Detailed Form

```xml
<box name="Gate">
  <collision type="kinematic" shape="box" layer="environment">
    <mask>player npc</mask>
  </collision>
</box>
```

`<mask>` contains a space-separated list of collision layers whose objects interact with this object.
