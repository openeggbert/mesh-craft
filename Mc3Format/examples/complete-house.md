# Complete House Example

An interactive house with a door, window, chair, and actions.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.1" model="InteractiveHouse" unit="meter" coordinate_system="right_handed_y_up">

  <textures>
    <texture id="brick_albedo" uri="textures/brick_albedo.png"
             wrap_u="repeat" wrap_v="repeat" filter="linear"/>
    <texture id="wood_albedo"  uri="textures/wood_albedo.png"
             wrap_u="repeat" wrap_v="repeat" filter="linear"/>
  </textures>

  <materials>
    <material id="brick" roughness="0.9">
      <base_color_texture>brick_albedo</base_color_texture>
    </material>
    <material id="painted_wood" roughness="0.6">
      <base_color_texture>wood_albedo</base_color_texture>
    </material>
    <material id="glass" roughness="0.05" alpha_mode="blend">
      <base_color>0.7 0.9 1.0 0.35</base_color>
    </material>
  </materials>

  <definitions>
    <definition id="SimpleChair">
      <group>
        <box name="Seat" size="1 0.15 1" position="0 0.55 0" material="painted_wood"/>
        <box name="Back" size="1 1 0.15"  position="0 1.1 0.45" material="painted_wood"/>
        <box name="Leg1" size="0.1 0.55 0.1" position="-0.4 0.275 -0.4" material="painted_wood"/>
        <box name="Leg2" size="0.1 0.55 0.1" position=" 0.4 0.275 -0.4" material="painted_wood"/>
        <box name="Leg3" size="0.1 0.55 0.1" position="-0.4 0.275  0.4" material="painted_wood"/>
        <box name="Leg4" size="0.1 0.55 0.1" position=" 0.4 0.275  0.4" material="painted_wood"/>
      </group>
    </definition>
  </definitions>

  <objects>

    <!-- Front wall with door hole cut out -->
    <difference name="FrontWallWithDoorHole">
      <box name="FrontWallBase" size="8 3 0.3" position="0 1.5 -3" material="brick">
        <uv mode="box" scale="2 2"/>
      </box>
      <box name="DoorHole" size="1.1 2.2 0.5" position="0 1.1 -3" role="cutter" visible="false"/>
    </difference>

    <!-- Rotating door with states -->
    <box name="FrontDoor" size="1 2.1 0.08" position="0 1.05 -3.08"
         pivot="-0.5 0 0" material="painted_wood" collision="kinematic" state="closed">
      <states>
        <state id="closed" rotation="0 0 0"/>
        <state id="open"   rotation="0 90 0"/>
      </states>
    </box>

    <!-- Window -->
    <box name="WindowGlass" size="1.2 1.0 0.05" position="2.2 1.7 -3.08" material="glass"/>

    <!-- Movable chair from definition -->
    <instance name="ChairA" definition="SimpleChair"
              position="1.5 0 1.5" rotation="0 20 0" collision="dynamic"/>

  </objects>

  <actions>

    <action id="open_front_door" target="FrontDoor" type="set_state"
            state="open" duration="0.6" easing="ease_out">
      <trigger type="interact" prompt="Open door"/>
    </action>

    <action id="close_front_door" target="FrontDoor" type="set_state"
            state="closed" duration="0.6" easing="ease_in">
      <trigger type="interact" prompt="Close door"/>
    </action>

    <action id="move_chair_aside" target="ChairA" type="translate"
            offset="1.0 0 0" duration="0.4" easing="ease_in_out">
      <trigger type="on_signal" signal="move_chair"/>
    </action>

  </actions>

</mc3>
```
