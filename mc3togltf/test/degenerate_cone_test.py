#!/usr/bin/env python3
"""
STAB-0668: buildCone() computed slopeLen = sqrt(radius^2 + height^2),
then ny = radius/slopeLen -- a degenerate cone (radius==0 AND
height==0) makes slopeLen==0, producing a 0/0 NaN normal that would
have propagated into the exported NORMAL accessor undetected (no
finite-value guard exists for primitive normals, only for the
OBJ-import path's positions).

Usage: degenerate_cone_test.py <mc3togltf-binary>
"""
import base64
import json
import math
import os
import struct
import subprocess
import sys
import tempfile

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf-binary>", file=sys.stderr)
        sys.exit(2)
    mc3togltf = sys.argv[1]

    fixture = """<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="DegenerateConeTest">
  <objects>
    <cone name="Degenerate" radius="0" height="0" segments="8"/>
  </objects>
</mc3>
"""
    with tempfile.TemporaryDirectory() as tmpdir:
        xml_path = os.path.join(tmpdir, "cone.mc3.xml")
        with open(xml_path, "w") as f:
            f.write(fixture)
        out_glb = os.path.join(tmpdir, "out.glb")

        r = subprocess.run([mc3togltf, xml_path, out_glb], capture_output=True, text=True)
        if r.returncode != 0:
            print(f"FAIL: export failed: {r.stderr}", file=sys.stderr)
            sys.exit(1)
        print("PASS: degenerate cone (radius=0, height=0) exports without crashing")

        with open(out_glb, "rb") as f:
            data = f.read()
        off = 12
        json_len, _ = struct.unpack_from("<II", data, off)
        gltf = json.loads(data[off + 8: off + 8 + json_len])
        off += 8 + json_len
        bin_len, _ = struct.unpack_from("<II", data, off)
        bin_data = data[off + 8: off + 8 + bin_len]

        mesh = gltf["meshes"][0]
        norm_acc_idx = mesh["primitives"][0]["attributes"]["NORMAL"]
        acc = gltf["accessors"][norm_acc_idx]
        bv = gltf["bufferViews"][acc["bufferView"]]
        offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
        n_floats = acc["count"] * 3
        values = struct.unpack_from("<" + "f" * n_floats, bin_data, offset)

        bad = [v for v in values if math.isnan(v) or math.isinf(v)]
        if bad:
            print(f"FAIL: NORMAL accessor contains {len(bad)} non-finite values "
                  f"(NaN/Inf) for a degenerate cone", file=sys.stderr)
            sys.exit(1)
        print(f"PASS: all {len(values)} NORMAL floats are finite (no NaN/Inf)")


if __name__ == "__main__":
    main()
