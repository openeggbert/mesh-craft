# Action Types

## Rotate

```xml
<action id="open_front_door" target="FrontDoor" type="rotate"
        axis="y" angle="90" duration="0.6" easing="ease_out"/>
```

Alternative vector form:

```xml
<action id="rotate_part" target="SomeObject" type="rotate"
        rotation="0 90 0" duration="1.0"/>
```

## Move / Translate

```xml
<action id="move_chair_aside" target="ChairA" type="translate"
        offset="1.0 0 0" duration="0.4" easing="ease_in_out"/>
```

## Set Position

```xml
<action id="reset_chair_position" target="ChairA" type="set_position"
        position="1 0 2" duration="0.3"/>
```

## Scale

```xml
<action id="grow_platform" target="Platform" type="scale"
        scale="2 1 2" duration="1.0"/>
```

## Show / Hide

```xml
<action id="hide_secret_wall" target="SecretWall" type="set_visible" visible="false"/>
```

## Change Material

```xml
<action id="turn_lamp_on" target="LampBulb" type="set_material" material="glowing_yellow"/>
```

## Set State

```xml
<action id="set_door_open" target="FrontDoor" type="set_state" state="open" duration="0.6"/>
```

## Sequence

Runs actions one after another.

```xml
<action id="open_door_and_move_chair" type="sequence">
  <step action="open_front_door"/>
  <step action="move_chair_aside"/>
</action>
```

## Parallel

Runs actions at the same time.

```xml
<action id="open_double_door" type="parallel">
  <step target="LeftDoor" type="rotate" axis="y" angle="-90" duration="0.6"/>
  <step target="RightDoor" type="rotate" axis="y" angle="90" duration="0.6"/>
</action>
```

## Toggle

Switches between two or more named states.

```xml
<action id="toggle_front_door" type="toggle">
  <state id="closed" target="FrontDoor" type="rotate" axis="y" angle="0" duration="0.5"/>
  <state id="open"   target="FrontDoor" type="rotate" axis="y" angle="90" duration="0.5"/>
</action>
```

`toggle` is syntactic sugar over `set_state`. Each invocation of the toggle action advances the object to the next listed state in order, cycling back to the first state after the last.

When a `toggle` action and a `<states>` block are both present on the same object, the toggle cycles through the states declared in the object's `<states>` element. The two mechanisms are complementary: use `<states>` on the object to declare named configurations (see [States](../objects/states.md)), and use a `toggle` action to cycle through them with a single call. Direct `set_state` actions remain available for explicit state targeting. Importers must not treat `toggle` and `<states>` as conflicting.
