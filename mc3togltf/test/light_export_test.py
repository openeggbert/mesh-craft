#!/usr/bin/env python3
"""
STAB-0692/0696/0697: mc3togltf light export -- KHR_lights_punctual
correctness (spot cone angle, ambient-light handling), previously
completely untested.
Usage: light_export_test.py <mc3togltf-binary> <light_export_all_types.mc3.xml>
"""
import json
import math
import os
import subprocess
import sys
import tempfile

failures = 0

def check(cond, msg):
    global failures
    if cond:
        print(f"PASS: {msg}")
    else:
        print(f"FAIL: {msg}", file=sys.stderr)
        failures += 1

def main():
    if len(sys.argv) != 3:
        print("usage: light_export_test.py <mc3togltf-binary> <fixture.mc3.xml>", file=sys.stderr)
        sys.exit(2)
    binary, fixture = sys.argv[1], sys.argv[2]

    with tempfile.NamedTemporaryFile(suffix=".gltf", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, fixture, out], capture_output=True, text=True)
        check(r.returncode == 0, f"exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            return

        # STAB-0696: ambient light should warn, not silently vanish.
        check("ambient" in r.stderr.lower() and "Fill" in r.stderr,
              "ambient light 'Fill' produces a warning naming it")

        with open(out) as f:
            g = json.load(f)

        check("KHR_lights_punctual" in g.get("extensionsUsed", []),
              "extensionsUsed contains KHR_lights_punctual")

        lights = g.get("extensions", {}).get("KHR_lights_punctual", {}).get("lights", [])
        check(len(lights) == 3,
              f"exactly 3 lights exported (directional+point+spot, ambient omitted); got {len(lights)}")

        by_type = {l["type"]: l for l in lights}
        check("directional" in by_type, "directional light present")
        check("point" in by_type, "point light present")
        check("spot" in by_type, "spot light present")

        # SYS-W14-26 (2026-07-20): KHR_lights_punctual mandates directional
        # intensity in lux and point/spot intensity in candela -- physically
        # different units -- so the exporter now converts per light type
        # instead of writing the same raw brightness number into all three.
        # Matches Blender's own glTF exporter convention: directional
        # (Sun brightness=2.0 in the fixture) passes through unconverted
        # (W/m^2 == lux by that convention); point/spot (brightness=5.0/3.0)
        # convert via brightness / (4*pi) * 683 (CIE luminous efficacy).
        PBR_WATTS_TO_LUMENS = 683.0
        if "directional" in by_type:
            expected = 2.0
            actual = by_type["directional"].get("intensity")
            check(actual is not None and abs(actual - expected) < 1e-4,
                  f"directional 'Sun' intensity == brightness unconverted ({expected}); got {actual}")
        if "point" in by_type:
            expected = 5.0 / (4.0 * math.pi) * PBR_WATTS_TO_LUMENS
            actual = by_type["point"].get("intensity")
            check(actual is not None and abs(actual - expected) < 1e-2,
                  f"point 'Bulb' intensity == brightness/(4*pi)*683 ({expected:.4f}); got {actual}")
        if "spot" in by_type:
            expected = 3.0 / (4.0 * math.pi) * PBR_WATTS_TO_LUMENS
            actual = by_type["spot"].get("intensity")
            check(actual is not None and abs(actual - expected) < 1e-2,
                  f"spot 'Torch' intensity == brightness/(4*pi)*683 ({expected:.4f}); got {actual}")

        if "spot" in by_type:
            spot = by_type["spot"]
            # STAB-0692: angle="25" in the fixture is a half-angle in degrees.
            # Correct outerConeAngle (radians) = 25 * pi/180 =~ 0.4363.
            # The old (buggy) /360 conversion would have produced =~ 0.2182.
            expected = 25.0 * math.pi / 180.0
            actual = spot.get("spot", {}).get("outerConeAngle")
            check(actual is not None and abs(actual - expected) < 1e-4,
                  f"spot outerConeAngle == 25deg-as-radians ({expected:.4f}), not halved; got {actual}")
            check(actual is None or abs(actual - expected / 2.0) > 1e-4,
                  "spot outerConeAngle is NOT the old halved (buggy) value")

        # --- Light NODE placement (regression: spot lights used to export at
        # the world origin because they took the rotation-only branch). The
        # light node carries the light's name, so match on that. ---
        nodes = g.get("nodes", [])
        by_name = {n.get("name"): n for n in nodes}

        pt = by_name.get("Bulb")
        check(pt is not None and pt.get("translation") == [1.0, 2.0, 3.0],
              f"point light 'Bulb' node translation == [1,2,3]; got "
              f"{pt.get('translation') if pt else None}")

        sp = by_name.get("Torch")
        check(sp is not None and sp.get("translation") == [4.0, 5.0, 6.0],
              f"spot light 'Torch' node translation == [4,5,6] (not dropped to "
              f"origin); got {sp.get('translation') if sp else None}")
        check(sp is not None and "rotation" in sp,
              "spot light node is still aimed (has a rotation)")

        dr = by_name.get("Sun")
        check(dr is not None and not dr.get("translation"),
              "directional light 'Sun' node has no translation (rotation only)")

    finally:
        if os.path.exists(out):
            os.unlink(out)

    # --- Unit scale: point/spot position and range scale like all geometry. ---
    with tempfile.TemporaryDirectory() as td:
        cm = os.path.join(td, "cm.mc3.xml")
        with open(cm, "w") as f:
            f.write(
                '<mc3 version="0.3" model="cm" unit="centimeter">\n'
                '  <lights>\n'
                '    <point name="P" color="1 1 1" brightness="1"'
                ' position="100 200 300" range="1000"/>\n'
                '  </lights>\n'
                '  <objects><box name="B" size="1 1 1"/></objects>\n'
                '</mc3>\n')
        out2 = os.path.join(td, "cm.gltf")
        r2 = subprocess.run([binary, cm, out2], capture_output=True, text=True)
        check(r2.returncode == 0, f"centimeter fixture exits 0 (stderr {r2.stderr.strip()!r})")
        if r2.returncode == 0:
            g2 = json.load(open(out2))
            l2 = g2.get("extensions", {}).get("KHR_lights_punctual", {}).get("lights", [])
            pnode = next((n for n in g2.get("nodes", []) if n.get("name") == "P"), None)
            tr = pnode.get("translation") if pnode else None
            check(tr is not None and all(abs(a - b) < 1e-5 for a, b in zip(tr, [1.0, 2.0, 3.0])),
                  f"centimeter point translation scaled by 0.01 -> [1,2,3]; got {tr}")
            check(l2 and abs(l2[0].get("range", 0) - 10.0) < 1e-6,
                  f"centimeter point range 1000cm scaled -> 10m; got "
                  f"{l2[0].get('range') if l2 else None}")

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll light export checks passed.")

if __name__ == "__main__":
    main()
