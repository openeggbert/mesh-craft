# Pivots

Pivots are important for doors, lids, levers, wheels, and rotating objects.

```xml
<box name="FrontDoor"
     size="1 2.1 0.08"
     position="0 1.05 -3.05"
     pivot="-0.5 0 0"
     material="painted_wood"/>
```

Here the door rotates around its left edge.

The `pivot` attribute is specified in local object coordinates before any transform is applied. The object's rotation and scale are applied around this point rather than around the object's local origin.
