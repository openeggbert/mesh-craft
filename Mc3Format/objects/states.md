# States

Objects may define named states using the `<states>` child element.

```xml
<box name="FrontDoor"
     size="1 2.1 0.08"
     pivot="-0.5 0 0"
     material="painted_wood"
     state="closed">
  <states>
    <state id="closed" rotation="0 0 0"/>
    <state id="open" rotation="0 90 0"/>
  </states>
</box>
```

The `state` attribute on the object sets the initial active state.

Actions can change state using `type="set_state"`:

```xml
<action id="set_door_open" target="FrontDoor" type="set_state" state="open" duration="0.6"/>
```

`<states>` declares named configurations for an object. Each `<state>` element is a partial set of object attributes that override the base values when the object is in that state.

A `toggle` action (see [action types](../actions/action-types.md)) is syntactic sugar that cycles through these states. For interactive objects with exactly two configurations (open/closed, on/off), either approach is acceptable; for objects with three or more configurations, use `<states>` with explicit `set_state` actions or a single `toggle`.

Direct `set_state` actions remain available for explicit state targeting. Importers must not treat `toggle` and `<states>` as conflicting; they are independent layers of the same state machine.
