# Actions

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

Actions are declared inside the top-level `<actions>` element.

```xml
<actions>
  <action id="open_front_door"
          target="FrontDoor"
          type="rotate"
          axis="y"
          angle="90"
          duration="0.6"
          easing="ease_out"/>
</actions>
```

## Common Action Attributes

| Attribute | Type | Description |
|---|---|---|
| `id` | string | Unique action identifier. |
| `target` | string | Object `name` or `id`. For composite actions, may be omitted or set on individual steps. |
| `type` | string | Action type. |
| `duration` | number | Duration in seconds. |
| `easing` | string | Interpolation mode, e.g. `ease_in`, `ease_out`, `ease_in_out`, `linear`. |
| `relative` | bool | Whether transform is relative to current state. Default depends on action type. |

## Action Child Elements

| Element | Description |
|---|---|
| `<trigger>` | Optional trigger definition (see [Triggers](triggers.md)). |
| `<step>` | Used inside `sequence` and `parallel` actions. |
| `<state>` | Used inside `toggle` actions. |

See also:

- [Action Types](action-types.md)
- [Triggers](triggers.md)
