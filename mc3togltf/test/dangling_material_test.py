#!/usr/bin/env python3
"""
STAB-0679: an object referencing a nonexistent material was silently
exported without one, inconsistent with the analogous dangling-SVG-
texture case (which does warn).

Usage: dangling_material_test.py <mc3togltf-binary>
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
<mc3 version="0.3" model="DanglingMaterialTest">
  <objects>
    <box name="OrphanBox" size="1 1 1" material="does-not-exist"/>
  </objects>
</mc3>
"""
    with tempfile.TemporaryDirectory() as tmpdir:
        xml_path = os.path.join(tmpdir, "dangling_mat.mc3.xml")
        with open(xml_path, "w") as f:
            f.write(fixture)
        out = os.path.join(tmpdir, "out.gltf")

        r = subprocess.run([mc3togltf, xml_path, out], capture_output=True, text=True)
        check(r.returncode == 0, f"exits 0 despite the dangling material (stderr: {r.stderr.strip()!r})")
        check("does-not-exist" in r.stderr and "unknown material" in r.stderr,
              "a warning naming the unknown material is printed to stderr")

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll dangling-material checks passed.")

if __name__ == "__main__":
    main()
