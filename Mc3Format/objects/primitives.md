# Primitive Objects

## Box

```xml
<box name="Wall" size="10 3 0.3" position="0 1.5 0" material="brick"/>
```

| Attribute | Type | Description |
|---|---|---|
| `size` | vec3 | Width, height, depth as space-separated `w h d`. |

## Cube

Shortcut for a box with equal dimensions.

```xml
<cube name="Crate" size="1" material="wood"/>
```

`<cube>` is a true alias for `<box>` with `size="n n n"`. The compiler treats a `<cube>` as fully equivalent to a `<box>` with three equal side lengths. There is no behavioral or semantic difference between a `<cube size="2">` and a `<box size="2 2 2">`.

## Sphere

```xml
<sphere name="Ball" radius="0.5" segments="32" material="red_plastic"/>
```

| Attribute | Type | Description |
|---|---|---|
| `radius` | number | Sphere radius. |
| `segments` | integer | Optional mesh quality. Default: `32`. |

## Cylinder

```xml
<cylinder name="Column" radius="0.3" height="3" segments="32" position="2 1.5 0" material="stone"/>
```

| Attribute | Type | Description |
|---|---|---|
| `radius` | number | Radius. |
| `height` | number | Height along the chosen axis. Default axis is Y. |
| `segments` | integer | Optional mesh quality. Default: `32`. |
| `axis` | string | Optional: `x`, `y`, or `z`. Default: `y`. |

The `axis` attribute is a mesh-generation hint: it selects which local axis runs along the cylinder's length during geometry generation. It does **not** apply a rotation transform. The generated mesh is oriented so that the specified axis is the height axis.

`axis` and `rotation` may be combined freely. `axis` controls how the mesh is generated internally; `rotation` is then applied as a standard object transform on top of the generated geometry.

## Cone

```xml
<cone name="RoofCone" radius="2" height="1.5" segments="32" material="roof_tile"/>
```

## Plane

```xml
<plane name="Floor" size="10 10" axis="y" material="floor_tiles"/>
```

A plane is usually generated as a flat mesh.

## Mesh Reference

MC3 may reference external mesh assets for complex objects.

```xml
<mesh name="ChairDetailed" src="assets/chair.glb" position="2 0 3" material="varnished_wood"/>
```

This allows simple MC3 objects and imported artist-made assets to coexist.
