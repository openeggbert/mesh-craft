# Minimal Example

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.1" model="House">

  <materials>
    <material id="brick">
      <base_color>0.8 0.2 0.1 1.0</base_color>
    </material>
    <material id="stone">
      <base_color>0.5 0.5 0.5 1.0</base_color>
    </material>
  </materials>

  <objects>
    <box name="Wall" size="10 3 0.3" position="0 1.5 0" material="brick"/>
    <cylinder name="Column" radius="0.3" height="3" position="2 1.5 0" material="stone"/>
  </objects>

</mc3>
```
