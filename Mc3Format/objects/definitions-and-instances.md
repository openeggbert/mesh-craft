# Definitions and Instances

## Definitions

Reusable templates are stored in the `<definitions>` element at the top level.

```xml
<definitions>
  <definition id="SimpleChair">
    <group>
      <box name="Seat" size="1 0.15 1" position="0 0.6 0" material="wood"/>
      <box name="Back" size="1 1 0.15" position="0 1.1 0.45" material="wood"/>
      <box name="Leg1" size="0.1 0.55 0.1" position="-0.4 0.275 -0.4" material="wood"/>
      <box name="Leg2" size="0.1 0.55 0.1" position="0.4 0.275 -0.4" material="wood"/>
      <box name="Leg3" size="0.1 0.55 0.1" position="-0.4 0.275 0.4" material="wood"/>
      <box name="Leg4" size="0.1 0.55 0.1" position="0.4 0.275 0.4" material="wood"/>
    </group>
  </definition>
</definitions>
```

## Instances

Instances reference a definition by its `id`:

```xml
<objects>
  <instance name="ChairA" definition="SimpleChair" position="1 0 2"/>
  <instance name="ChairB" definition="SimpleChair" position="3 0 2" rotation="0 90 0"/>
</objects>
```

## Instance Field Override Rules

Attributes set directly on an `<instance>` element override the corresponding fields from the definition. If a field is not set on the instance, the value from the definition is used.

Attributes that can be overridden on an instance: `position`, `rotation`, `scale`, `material`, `visible`, `collision`, `state`.

Tags from an instance are merged with tags from the definition.

When `collision` is set on an instance, it replaces any collision behavior defined inside the definition. The override applies to the instance as a whole and is intended to control the top-level physics body of the instanced object. Importers may propagate the collision override to the root-level children of the definition if the definition does not have a single top-level physics object.
