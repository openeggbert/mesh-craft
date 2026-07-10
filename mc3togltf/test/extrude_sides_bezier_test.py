#!/usr/bin/env python3
"""
STAB-0665: test/extrude_sides_bezier.mc3.xml (added for the mc3.xsd fix,
STAB-0651/0652) had never been exercised by any mc3togltf test -- the
actual export-time geometry math for non-default-sided polygon/star
cross-sections and Catmull-Rom bezier paths (MeshBuilder.cpp's
sampleCrossSection()/samplePath()) had zero export-side coverage.

Exports the fixture and, for each of the 3 objects (Hexagon,
FivePointStar, BezierRibbon), asserts: a mesh exists, has a non-zero
triangle count, all POSITION values are finite (no NaN/Inf), and the
accessor's declared min/max bounds actually match the real data.

Usage: extrude_sides_bezier_test.py <mc3togltf-binary> <extrude_sides_bezier.mc3.xml>
"""
import json
import math
import os
import struct
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

def read_vec3_accessor(gltf, bin_data, acc_idx):
    acc = gltf["accessors"][acc_idx]
    bv = gltf["bufferViews"][acc["bufferView"]]
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = bv.get("byteStride", 12)
    out = []
    for i in range(acc["count"]):
        base = offset + i * stride
        out.append(struct.unpack_from("<fff", bin_data, base))
    return out

def main():
    if len(sys.argv) != 3:
        print("usage: extrude_sides_bezier_test.py <mc3togltf-binary> <fixture.mc3.xml>", file=sys.stderr)
        sys.exit(2)
    binary, fixture = sys.argv[1], sys.argv[2]

    with tempfile.NamedTemporaryFile(suffix=".glb", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, fixture, out], capture_output=True, text=True)
        check(r.returncode == 0, f"exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            return

        with open(out, "rb") as f:
            data = f.read()
        off = 12
        json_len, _ = struct.unpack_from("<II", data, off)
        gltf = json.loads(data[off + 8: off + 8 + json_len])
        off += 8 + json_len
        bin_len, _ = struct.unpack_from("<II", data, off)
        bin_data = data[off + 8: off + 8 + bin_len]

        node_by_name = {n["name"]: n for n in gltf["nodes"] if "name" in n}
        for name in ("Hexagon", "FivePointStar", "BezierRibbon"):
            check(name in node_by_name, f"node '{name}' present")
            if name not in node_by_name:
                continue
            node = node_by_name[name]
            mesh = gltf["meshes"][node["mesh"]]
            prim = mesh["primitives"][0]

            idx_acc = gltf["accessors"][prim["indices"]]
            tri_count = idx_acc["count"] // 3
            check(tri_count > 0, f"{name}: non-zero triangle count ({tri_count})")

            pos_acc_idx = prim["attributes"]["POSITION"]
            pos_acc = gltf["accessors"][pos_acc_idx]
            positions = read_vec3_accessor(gltf, bin_data, pos_acc_idx)

            all_finite = all(math.isfinite(c) for p in positions for c in p)
            check(all_finite, f"{name}: all POSITION values are finite (no NaN/Inf)")

            real_min = [min(p[a] for p in positions) for a in range(3)]
            real_max = [max(p[a] for p in positions) for a in range(3)]
            declared_min = pos_acc["min"]
            declared_max = pos_acc["max"]
            bounds_ok = all(abs(real_min[a] - declared_min[a]) < 1e-4 for a in range(3)) and \
                        all(abs(real_max[a] - declared_max[a]) < 1e-4 for a in range(3))
            check(bounds_ok,
                  f"{name}: accessor min/max bounds match the real data "
                  f"(declared min={declared_min} max={declared_max}, "
                  f"actual min={real_min} max={real_max})")

    finally:
        if os.path.exists(out):
            os.unlink(out)

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll extrude sides/bezier export checks passed.")

if __name__ == "__main__":
    main()
