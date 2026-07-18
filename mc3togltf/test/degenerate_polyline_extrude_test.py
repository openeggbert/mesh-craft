#!/usr/bin/env python3
"""
AUD-067: MeshBuilder.cpp's frameAxes() computed a binormal by dividing by
its own length with NO zero-length guard (unlike norm3() a few lines above
it in the same file, which does guard). A Polyline extrude path with two
consecutive identical <point> elements (a plausible authoring/AI mistake)
produces a zero-length segment tangent -- unlike the Bezier path, which
already falls back to {0,1,0} for exactly this case, Polyline had no
fallback and fed {0,0,0} straight into frameAxes, dividing 0/0 into NaN.
That NaN used to reach GltfExporter's finiteness gate and abort the whole
export with a generic "non-finite vertex position" error, instead of
producing valid (if locally pinched) geometry the way the Bezier path
already handles the same class of degenerate input.

Usage: degenerate_polyline_extrude_test.py <mc3togltf-binary>
"""
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

    # points[1] and points[2] are identical -> zero-length segment.
    fixture = """<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="DegeneratePolylineTest">
  <objects>
    <extrude name="Pipe" segments="8">
      <cross_section type="circle" radius="0.15" segments="12"/>
      <path type="polyline">
        <point x="0" y="0" z="0"/>
        <point x="0" y="1" z="0"/>
        <point x="0" y="1" z="0"/>
        <point x="1" y="2" z="0"/>
      </path>
    </extrude>
  </objects>
</mc3>
"""
    with tempfile.TemporaryDirectory() as tmpdir:
        xml_path = os.path.join(tmpdir, "polyline.mc3.xml")
        with open(xml_path, "w") as f:
            f.write(fixture)
        out_glb = os.path.join(tmpdir, "out.glb")

        r = subprocess.run([mc3togltf, xml_path, out_glb], capture_output=True, text=True)
        if r.returncode != 0:
            print(f"FAIL: export of a polyline path with a duplicate consecutive "
                  f"point failed instead of producing valid (if locally pinched) "
                  f"geometry: {r.stderr}", file=sys.stderr)
            sys.exit(1)
        print("PASS: polyline extrude path with a duplicate consecutive point "
              "exports without aborting")

        with open(out_glb, "rb") as f:
            data = f.read()
        off = 12
        json_len, _ = struct.unpack_from("<II", data, off)
        gltf = json.loads(data[off + 8: off + 8 + json_len])
        off += 8 + json_len
        bin_len, _ = struct.unpack_from("<II", data, off)
        bin_data = data[off + 8: off + 8 + bin_len]

        mesh = gltf["meshes"][0]
        attrs = mesh["primitives"][0]["attributes"]

        for attr_name in ("POSITION", "NORMAL"):
            if attr_name not in attrs:
                continue
            acc = gltf["accessors"][attrs[attr_name]]
            bv = gltf["bufferViews"][acc["bufferView"]]
            offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
            n_floats = acc["count"] * 3
            values = struct.unpack_from("<" + "f" * n_floats, bin_data, offset)

            bad = [v for v in values if math.isnan(v) or math.isinf(v)]
            if bad:
                print(f"FAIL: {attr_name} accessor contains {len(bad)} non-finite "
                      f"values (NaN/Inf) from the degenerate polyline segment",
                      file=sys.stderr)
                sys.exit(1)
            print(f"PASS: all {len(values)} {attr_name} floats are finite (no NaN/Inf)")

            declared_min = acc.get("min")
            declared_max = acc.get("max")
            if declared_min is not None:
                bad_bounds = [v for v in declared_min + declared_max
                              if math.isnan(v) or math.isinf(v)]
                if bad_bounds:
                    print(f"FAIL: {attr_name} accessor min/max contains non-finite "
                          f"values", file=sys.stderr)
                    sys.exit(1)
                print(f"PASS: {attr_name} accessor min/max are finite")


if __name__ == "__main__":
    main()
