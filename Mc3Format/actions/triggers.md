# Triggers

Triggers are optional. They describe when an action should run automatically. Triggers are defined as a `<trigger>` child element of an action.

```xml
<action id="open_front_door" target="FrontDoor" type="rotate"
        axis="y" angle="90" duration="0.6">
  <trigger type="interact" prompt="Open door"/>
</action>
```

## Trigger Attributes

| Attribute | Type | Description |
|---|---|---|
| `type` | string | Trigger type. |
| `prompt` | string | UI prompt text, used with `interact`. |
| `signal` | string | Signal name, used with `on_signal`. |

## Trigger Types

| Type | Description |
|---|---|
| `interact` | Player/user interaction. |
| `on_load` | Runs when model/scene is loaded. |
| `on_collision` | Runs when collision occurs. |
| `on_enter_area` | Runs when something enters a volume. |
| `on_signal` | Runs when game code sends a named signal. |
| `manual` | Only invoked by code/editor. Default. |

## Signal Trigger Example

```xml
<action id="open_gate" target="Gate" type="translate"
        offset="0 3 0" duration="1.5">
  <trigger type="on_signal" signal="gate_opened"/>
</action>
```

## On Load Example

```xml
<action id="start_animation" target="Windmill" type="rotate"
        axis="y" angle="360" duration="4.0">
  <trigger type="on_load"/>
</action>
```
