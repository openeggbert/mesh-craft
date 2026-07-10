#!/usr/bin/env python3
"""STAB-0664 -- TANGENT attribute generation for normal-mapped meshes.

GltfExporter.cpp never generated a TANGENT accessor for any mesh, even
when its material had a normal_texture assigned -- glTF viewers without
their own derivative-based tangent fallback would render such meshes with
undefined/wrong normal-map lighting. Not a full MikkTSpace port (angle/
area-weighted, feature-vertex-aware); uses the standard per-triangle-
tangent-then-per-vertex-average-then-Gram-Schmidt-orthogonalize algorithm
(the same approach three.js's own computeTangents() uses).

Exports normal_map_tangent.mc3.xml (NormalMappedBox + NormalMappedSphere,
both using a material with normal_texture set, and PlainBox, which has no
normal_texture) and verifies:
  - NormalMappedBox/NormalMappedSphere DO have a TANGENT accessor.
  - PlainBox does NOT (no normal map -> no reason to compute tangents).
  - Every TANGENT value is a unit-length XYZ vector (w = +-1 exactly),
    orthogonal to its own vertex NORMAL (the defining property of a valid
    tangent-space basis) -- checked directly against the real exported
    accessor bytes, including NormalMappedSphere specifically to exercise
    the UV-seam/pole-vertex case (must not produce NaN or zero-length
    tangents there).

Usage: tangent_generation_test.py <mc3togltf> <normal_map_tangent.mc3.xml>
"""
import json
import math
import os
import struct
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def read_vecn(gltf, bin_data, acc_idx, n):
    acc = gltf["accessors"][acc_idx]
    bv = gltf["bufferViews"][acc["bufferView"]]
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = bv.get("byteStride", 4 * n)
    fmt = "<" + "f" * n
    return [struct.unpack_from(fmt, bin_data, offset + i * stride) for i in range(acc["count"])]


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <fixture.mc3.xml>")
        sys.exit(1)

    mc3togltf, fixture = sys.argv[1], sys.argv[2]
    failures = 0

    def check(cond, msg):
        global failures
        if cond:
            print(f"PASS: {msg}")
        else:
            print(f"FAIL: {msg}", file=sys.stderr)
            failures += 1

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, fixture, out])
        check(r.returncode == 0, f"mc3togltf exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            sys.exit(1)

        with open(out) as f:
            gltf = json.load(f)
        with open(os.path.join(tmpdir, gltf["buffers"][0]["uri"]), "rb") as f:
            bin_data = f.read()

    node_by_name = {n["name"]: n for n in gltf["nodes"] if "name" in n}

    check("PlainBox" in node_by_name, "PlainBox node present")
    plain_prim = gltf["meshes"][node_by_name["PlainBox"]["mesh"]]["primitives"][0]
    check("TANGENT" not in plain_prim["attributes"],
          "PlainBox (no normal_texture) has NO TANGENT accessor")

    for name in ("NormalMappedBox", "NormalMappedSphere"):
        check(name in node_by_name, f"{name} node present")
        if name not in node_by_name:
            continue
        prim = gltf["meshes"][node_by_name[name]["mesh"]]["primitives"][0]
        check("TANGENT" in prim["attributes"], f"{name} has a TANGENT accessor")
        if "TANGENT" not in prim["attributes"]:
            continue

        tangents = read_vecn(gltf, bin_data, prim["attributes"]["TANGENT"], 4)
        normals = read_vecn(gltf, bin_data, prim["attributes"]["NORMAL"], 3)
        check(len(tangents) == len(normals),
              f"{name}: TANGENT count ({len(tangents)}) matches NORMAL count ({len(normals)})")

        bad = []
        for i, (t, n) in enumerate(zip(tangents, normals)):
            xyz = t[:3]
            w = t[3]
            length = math.sqrt(sum(c * c for c in xyz))
            dot_n = sum(xyz[k] * n[k] for k in range(3))
            if not all(math.isfinite(c) for c in t):
                bad.append((i, "non-finite"))
            elif abs(length - 1.0) > 1e-3:
                bad.append((i, f"length={length:.5f}"))
            elif abs(dot_n) > 1e-3:
                bad.append((i, f"dot(T,N)={dot_n:.5f}"))
            elif w not in (1.0, -1.0):
                bad.append((i, f"w={w}"))

        check(not bad,
              f"{name}: all {len(tangents)} TANGENT values are finite, unit-length, "
              f"orthogonal to their NORMAL, and have w=+-1 "
              f"({len(bad)} bad, first few: {bad[:3]})")

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll TANGENT generation checks passed.")
