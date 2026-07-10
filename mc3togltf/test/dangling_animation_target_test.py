#!/usr/bin/env python3
"""
STAB-0682: an animation channel whose targetObject doesn't match any
exported node (a typo, or a target that was deleted/renamed after the
action was authored) silently did nothing, unlike the nearby
unsupported-property skip path which does warn.

Usage: dangling_animation_target_test.py <mc3togltf-binary>
"""
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
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf-binary>", file=sys.stderr)
        sys.exit(2)
    mc3togltf = sys.argv[1]

    fixture = """<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="DanglingAnimTargetTest">
  <objects>
    <box name="RealBox" size="1 1 1"/>
  </objects>
  <actions>
    <action name="Move" duration="1.0">
      <channel target="TypoedBoxName" property="position.y">
        <keyframe time="0.0" value="0.0" interp="linear"/>
        <keyframe time="1.0" value="2.0" interp="linear"/>
      </channel>
    </action>
  </actions>
</mc3>
"""
    with tempfile.TemporaryDirectory() as tmpdir:
        xml_path = os.path.join(tmpdir, "dangling.mc3.xml")
        with open(xml_path, "w") as f:
            f.write(fixture)
        out = os.path.join(tmpdir, "out.gltf")

        r = subprocess.run([mc3togltf, xml_path, out], capture_output=True, text=True)
        check(r.returncode == 0, f"exits 0 despite the dangling target (stderr: {r.stderr.strip()!r})")

        check("TypoedBoxName" in r.stderr and "does not match any exported node" in r.stderr,
              "a warning naming the dangling target ('TypoedBoxName') is printed to stderr")

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll dangling-animation-target checks passed.")

if __name__ == "__main__":
    main()
