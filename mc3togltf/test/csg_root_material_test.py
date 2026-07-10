#!/usr/bin/env python3
"""
STAB-0674: MC3_FORMAT.md documents "the CSG root's material is used
for the entire merged mesh", but no prior test asserted the merged
mesh's actual material index -- only that a material happened to render
without erroring.

Usage: csg_root_material_test.py <mc3togltf-binary> <csg_root_material.mc3.xml>
"""
import json
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
        print("usage: csg_root_material_test.py <mc3togltf-binary> <fixture.mc3.xml>", file=sys.stderr)
        sys.exit(2)
    binary, fixture = sys.argv[1], sys.argv[2]

    with tempfile.NamedTemporaryFile(suffix=".gltf", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, fixture, out], capture_output=True, text=True)
        check(r.returncode == 0, f"exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            return

        with open(out) as f:
            g = json.load(f)

        node = next((n for n in g["nodes"] if n.get("name") == "HollowBlock"), None)
        check(node is not None, "CSG root node 'HollowBlock' present")
        if not node:
            return
        mesh = g["meshes"][node["mesh"]]
        mat_idx = mesh["primitives"][0].get("material")
        check(mat_idx is not None, "CSG merged mesh has a material assigned")
        if mat_idx is None:
            return

        mat_name = g["materials"][mat_idx].get("name", "")
        base_color = g["materials"][mat_idx]["pbrMetallicRoughness"]["baseColorFactor"]
        check(base_color[0] > 0.9 and base_color[1] < 0.1,
              f"CSG merged mesh uses the ROOT's material (red, root_mat), not a "
              f"child's (green, child_mat) -- got baseColor={base_color}")

    finally:
        if os.path.exists(out):
            os.unlink(out)

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll CSG root material checks passed.")

if __name__ == "__main__":
    main()
