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

    finally:
        if os.path.exists(out):
            os.unlink(out)

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll light export checks passed.")

if __name__ == "__main__":
    main()
