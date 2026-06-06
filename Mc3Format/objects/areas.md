# Areas and Interaction Volumes

An invisible area can trigger actions when entered, exited, or occupied.

```xml
<area name="DoorInteractionArea"
      shape="box"
      size="1.5 2 1"
      position="0 1 -3"
      visible="false"
      trigger_on="enter">
  <trigger_actions>
    <action ref="open_front_door"/>
  </trigger_actions>
</area>
```

## Area Attributes

| Attribute | Type | Description |
|---|---|---|
| `shape` | string | Shape of the area volume: `box`, `sphere`, or `cylinder`. |
| `size` | vec3 / number / vec2 | Dimensions of the area. For `box`: `w h d`. For `sphere`: number (radius). For `cylinder`: `radius height`. |
| `trigger_on` | string | When to fire: `enter` (default), `exit`, or `stay`. |
| `collision_layer` | string | Collision layer this area belongs to. |

## Area Child Elements

| Element | Description |
|---|---|
| `<trigger_actions>` | List of `<action ref="..."/>` elements to invoke when the trigger fires. Actions are called in list order. |
| `<collision_mask>` | Space-separated layers whose objects can activate this area. If omitted, all layers activate the area. |

Example with collision mask:

```xml
<area name="PlayerOnlyZone" shape="box" size="3 2 3" position="0 1 0">
  <collision_mask>player</collision_mask>
  <trigger_actions>
    <action ref="open_gate"/>
  </trigger_actions>
</area>
```
