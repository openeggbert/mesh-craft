# Cameras

Cameras are defined in the top-level `<cameras>` element. If `<cameras>` is omitted, the importer may use a default perspective camera.

The `default` attribute on `<cameras>` names the active camera for rendering and export.

```xml
<cameras default="GameCamera">

  <camera name="GameCamera"
          type="perspective"
          fov="50"
          near="0.1"
          far="1000"
          position="30 30 30"
          target="0 0 0"/>

  <camera name="TopDown"
          type="orthographic"
          size="20"
          near="0.1"
          far="200"
          position="0 50 0"
          target="0 0 0"/>

</cameras>
```

## Common Camera Attributes

| Attribute | Type | Description |
|---|---|---|
| `name` | string | Camera identifier. |
| `type` | string | `perspective` or `orthographic`. Default: `perspective`. |
| `position` | vec3 | Camera world position. |
| `target` | vec3 | Point the camera looks at. |
| `rotation` | vec3 | Alternative to `target`: explicit camera rotation in Euler degrees. |
| `near` | number | Near clipping plane distance. Default: `0.1`. |
| `far` | number | Far clipping plane distance. Default: `1000`. |

When both `target` and `rotation` are specified, `rotation` takes precedence.

## Perspective Camera

```xml
<camera name="PlayerCam" type="perspective"
        fov="60" near="0.1" far="500"
        position="0 2 10" target="0 1 0"/>
```

| Attribute | Type | Description |
|---|---|---|
| `fov` | number | Vertical field of view in degrees. Default: `60`. |

## Orthographic Camera

```xml
<camera name="IsoCam" type="orthographic"
        size="15" near="0.1" far="200"
        position="30 30 30" target="0 0 0"/>
```

| Attribute | Type | Description |
|---|---|---|
| `size` | number | Half-height of the view volume in world units. Default: `10`. |

## Multiple Cameras

Multiple cameras may be defined. Tools and game engines can select which one to use. The `default` attribute on `<cameras>` specifies which camera is used when no explicit selection is made.

```xml
<cameras default="GameCamera">
  <camera name="GameCamera" type="perspective" fov="50"
          position="20 15 20" target="0 0 0"/>
  <camera name="DebugTop"   type="orthographic" size="30"
          position="0 50 0"  target="0 0 0"/>
  <camera name="FrontView"  type="orthographic" size="15"
          position="0 5 30"  target="0 5 0"/>
</cameras>
```
