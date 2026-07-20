#!/usr/bin/env python3
"""
AUD-024: per-object <uv_mapping> (scale/offset/rotation) must actually
transform the exported TEXCOORD_0 -- it used to round-trip through XML/MCB
and then be silently ignored by the exporter.

SYS-W14-24 (2026-07-20): box/sphere projection now actually regenerates
TEXCOORD_0 instead of only warning that it isn't implemented. Box
projection uses raw (non-normalized) local-space coordinates on the two
non-dominant axes, so a unit box's projected UVs span [-0.5, 0.5] instead
of the default planar unwrap's [0, 1]. Sphere projection is an
equirectangular mapping normalized to [0, 1] on both axes.

Reads the real GLB buffer bytes (not just accessor min/max -- TEXCOORD_0
accessors don't carry bounds) to verify actual sample values.

Usage: uv_mapping_export_test.py <mc3togltf-binary> <uv_mapping_export.mc3.xml>
"""
import json
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

def parse_glb(path):
    with open(path, "rb") as f:
        data = f.read()
    off = 12
    json_len, _ = struct.unpack_from("<II", data, off)
    gltf = json.loads(data[off + 8: off + 8 + json_len])
    off += 8 + json_len
    bin_len, _ = struct.unpack_from("<II", data, off)
    bin_data = data[off + 8: off + 8 + bin_len]
    return gltf, bin_data

def read_vec2_accessor(gltf, bin_data, acc_idx):
    acc = gltf["accessors"][acc_idx]
    bv = gltf["bufferViews"][acc["bufferView"]]
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    out = []
    for i in range(acc["count"]):
        out.append(struct.unpack_from("<ff", bin_data, offset + i * 8))
    return out

def texcoords_for(gltf, bin_data, node_name):
    node = next(n for n in gltf["nodes"] if n.get("name") == node_name)
    mesh = gltf["meshes"][node["mesh"]]
    acc_idx = mesh["primitives"][0]["attributes"]["TEXCOORD_0"]
    return read_vec2_accessor(gltf, bin_data, acc_idx)

def main():
    if len(sys.argv) < 3:
        print("Usage: uv_mapping_export_test.py <binary> <uv_mapping_export.mc3.xml>",
              file=sys.stderr)
        sys.exit(1)
    binary, src = sys.argv[1], sys.argv[2]

    with tempfile.NamedTemporaryFile(suffix=".glb", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, src, out], capture_output=True, text=True)
        check(r.returncode == 0, f"mc3togltf exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            sys.exit(1)

        gltf, bin_data = parse_glb(out)

        # Control: no uv_mapping -> unmodified default unwrap, U/V both in [0,1].
        plain_uv = texcoords_for(gltf, bin_data, "PlainBox")
        us = [uv[0] for uv in plain_uv]
        check(min(us) >= -1e-5 and max(us) <= 1.0 + 1e-5,
              f"PlainBox (no uv_mapping): U stays in [0,1], got [{min(us)}, {max(us)}]")

        # AUD-024: scale_u=4, offset_u=0.5 -> U in [0.5, 4.5] (span 4.0).
        scaled_uv = texcoords_for(gltf, bin_data, "ScaledUvBox")
        us = [uv[0] for uv in scaled_uv]
        vs = [uv[1] for uv in scaled_uv]
        span = max(us) - min(us)
        check(abs(span - 4.0) < 1e-3,
              f"ScaledUvBox: U range spans ~4x (scale_u=4), got span={span} "
              f"(min={min(us)}, max={max(us)})")
        check(abs(min(us) - 0.5) < 1e-3,
              f"ScaledUvBox: U min reflects offset_u=0.5, got {min(us)}")
        check(abs(min(vs) - 0.0) < 1e-3 and abs(max(vs) - 1.0) < 1e-3,
              f"ScaledUvBox: V unaffected by scale_u/offset_u (scale_v=1, offset_v=0), "
              f"got [{min(vs)}, {max(vs)}]")

        # SYS-W14-24: box projection actually regenerates TEXCOORD_0 now --
        # raw local-space coordinates on the two non-dominant axes, so a
        # unit (size 1x1x1) box's projected UVs span [-0.5, 0.5], not the
        # default planar unwrap's [0, 1]. No more "not implemented" warning.
        box_uv = texcoords_for(gltf, bin_data, "BoxProjectedBox")
        us = [uv[0] for uv in box_uv]
        vs = [uv[1] for uv in box_uv]
        check(abs(min(us) - (-0.5)) < 1e-3 and abs(max(us) - 0.5) < 1e-3,
              f"BoxProjectedBox: box-projected U spans [-0.5, 0.5] (unit box, raw "
              f"local coords), got [{min(us)}, {max(us)}]")
        check(abs(min(vs) - (-0.5)) < 1e-3 and abs(max(vs) - 0.5) < 1e-3,
              f"BoxProjectedBox: box-projected V spans [-0.5, 0.5] (unit box, raw "
              f"local coords), got [{min(vs)}, {max(vs)}]")
        check("not implemented" not in r.stderr,
              f"BoxProjectedBox: no 'not implemented' warning now that box projection "
              f"is actually applied (stderr: {r.stderr.strip()!r})")

        # SYS-W14-24: sphere projection is an equirectangular mapping
        # normalized to [0, 1] on both axes, distinct per-vertex (a box's 8
        # distinct corner directions must not collapse to one UV).
        sphere_uv = texcoords_for(gltf, bin_data, "SphereProjectedBox")
        us = [uv[0] for uv in sphere_uv]
        vs = [uv[1] for uv in sphere_uv]
        check(min(us) >= -1e-5 and max(us) <= 1.0 + 1e-5,
              f"SphereProjectedBox: U stays within [0,1], got [{min(us)}, {max(us)}]")
        check(min(vs) >= -1e-5 and max(vs) <= 1.0 + 1e-5,
              f"SphereProjectedBox: V stays within [0,1], got [{min(vs)}, {max(vs)}]")
        check(len(set(round(u, 4) for u in us)) > 1,
              f"SphereProjectedBox: U varies across the box's distinct corner "
              f"directions (not degenerate/constant), got values {sorted(set(us))}")
        check("not implemented" not in r.stderr,
              f"SphereProjectedBox: no 'not implemented' warning now that sphere "
              f"projection is actually applied (stderr: {r.stderr.strip()!r})")
    finally:
        if os.path.exists(out):
            os.unlink(out)

    print(f"\n{'All tests passed.' if failures == 0 else f'FAILURES: {failures}'}")
    sys.exit(1 if failures > 0 else 0)

if __name__ == "__main__":
    main()
