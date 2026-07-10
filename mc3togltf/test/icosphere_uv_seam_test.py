#!/usr/bin/env python3
"""
STAB-0663: buildIcoSphere()'s equirectangular UV mapping (atan2-based)
produced badly distorted UVs for any triangle straddling the seam
(longitude wraps from ~1.0 back to ~0.0) -- before the fix, 46 of 1280
triangles (subdivisions=3) had a U-span > 0.5 (i.e. stretched across
more than half the texture width), with the worst spanning almost the
entire [0,1] range. The fix nudges wrapped-around vertices +1.0 on a
per-triangle basis (each triangle already has its own unwelded vertex
copies, so this can't disturb any other triangle) -- reduces this to 2
residual near-pole numerical-instability triangles, a ~23x improvement.

Exports a plain IcoSphere primitive to GLB and inspects the real
TEXCOORD_0 accessor data (not just accessor metadata) to verify the
fraction of high-U-span triangles is small (the fix's actual effect),
not just "some data exists."

Usage: icosphere_uv_seam_test.py <mc3togltf-binary>
"""
import base64
import json
import os
import struct
import subprocess
import sys
import tempfile

# glTF componentType -> (struct format char, size in bytes)
COMPONENT_TYPES = {
    5121: ('B', 1), 5123: ('H', 2), 5125: ('I', 4),
    5126: ('f', 4), 5120: ('b', 1), 5122: ('h', 2),
}
TYPE_COMPONENT_COUNTS = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}


def read_accessor(gltf, bin_data, acc_idx):
    acc = gltf['accessors'][acc_idx]
    bv = gltf['bufferViews'][acc['bufferView']]
    fmt_char, comp_size = COMPONENT_TYPES[acc['componentType']]
    n_comp = TYPE_COMPONENT_COUNTS[acc['type']]
    offset = bv.get('byteOffset', 0) + acc.get('byteOffset', 0)
    stride = bv.get('byteStride', n_comp * comp_size)
    values = []
    for i in range(acc['count']):
        base = offset + i * stride
        vals = struct.unpack_from('<' + fmt_char * n_comp, bin_data, base)
        values.append(vals)
    return values


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf-binary>", file=sys.stderr)
        sys.exit(2)
    mc3togltf = sys.argv[1]

    # segments maps to subdivisions via clamp(segments/8, 1, 4) in
    # MeshBuilder.cpp -- segments=24 gives subdivisions=3 (1280 triangles),
    # matching the counts this test's docstring/threshold were derived from.
    fixture = f"""<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="IcoSphereUvSeamTest">
  <objects>
    <icosphere name="Sphere" radius="0.5" segments="24"/>
  </objects>
</mc3>
"""
    with tempfile.TemporaryDirectory() as tmpdir:
        xml_path = os.path.join(tmpdir, "ico.mc3.xml")
        with open(xml_path, "w") as f:
            f.write(fixture)
        out_glb = os.path.join(tmpdir, "out.glb")

        r = subprocess.run([mc3togltf, xml_path, out_glb], capture_output=True, text=True)
        assert r.returncode == 0, f"export failed: {r.stderr}"

        with open(out_glb, "rb") as f:
            data = f.read()
        magic, version, length = struct.unpack_from("<III", data, 0)
        assert magic == 0x46546C67, f"bad glTF magic: {magic:#x}"

        off = 12
        json_len, json_type = struct.unpack_from("<II", data, off)
        json_bytes = data[off + 8: off + 8 + json_len]
        gltf = json.loads(json_bytes)
        off += 8 + json_len

        bin_len, bin_type = struct.unpack_from("<II", data, off)
        bin_data = data[off + 8: off + 8 + bin_len]

        mesh = gltf['meshes'][0]
        prim = mesh['primitives'][0]
        uv_acc = prim['attributes']['TEXCOORD_0']
        idx_acc = prim['indices']

        uvs = read_accessor(gltf, bin_data, uv_acc)
        indices = [v[0] for v in read_accessor(gltf, bin_data, idx_acc)]

        n_tris = len(indices) // 3
        bad = 0
        max_span = 0.0
        for t in range(n_tris):
            i0, i1, i2 = indices[t*3], indices[t*3+1], indices[t*3+2]
            u0, u1, u2 = uvs[i0][0], uvs[i1][0], uvs[i2][0]
            span = max(u0, u1, u2) - min(u0, u1, u2)
            max_span = max(max_span, span)
            if span > 0.5:
                bad += 1

        bad_fraction = bad / n_tris
        print(f"triangles={n_tris} bad(span>0.5)={bad} max_span={max_span:.4f} "
              f"bad_fraction={bad_fraction:.4f}")

        # Before the fix: 46/1280 (~3.6%) with max_span up to ~0.99.
        # After: 2/1280 (~0.16%), max_span ~0.66 (residual near-pole cases).
        assert bad_fraction < 0.01, (
            f"expected < 1% of triangles to have a UV span > 0.5 (was ~3.6% "
            f"before the STAB-0663 fix); got {bad_fraction:.4f} ({bad}/{n_tris})"
        )
        print(f"PASS: IcoSphere UV seam distortion is rare ({bad}/{n_tris} triangles, "
              f"was 46/{n_tris} before the fix)")


if __name__ == "__main__":
    main()
