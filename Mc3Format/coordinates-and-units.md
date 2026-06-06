# Coordinates and Units

## Default

```xml
<mc3 version="0.1" model="Example" unit="meter" coordinate_system="right_handed_y_up">
```

## Coordinate Meaning

```text
X = right / left
Y = up / down
Z = forward / backward
```

## Vector Attributes

Vector values are written as space-separated numbers in XML attributes:

```xml
position="x y z"
rotation="pitch yaw roll"
scale="x y z"
```

Angles are in degrees unless specified otherwise.

```xml
<mc3 version="0.1" model="Example" angle_unit="degrees">
```

## Rotation Order

Rotation uses extrinsic XYZ Euler order: X (pitch) is applied first, then Y (yaw), then Z (roll), each around the fixed parent axes. This is equivalent to intrinsic ZYX order. Importers must use this convention to ensure consistent behavior across tools.
