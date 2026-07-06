#!/usr/bin/env python3
"""
STAB-0223/0233: CSG stress tests.

  - STAB-0223: a union of 10 boxes exports without crashing and produces
    real (non-zero) triangle geometry.
  - STAB-0233: a union tree exactly 5 levels deep containing 32 boxes
    total evaluates in under 5 seconds.
"""
import json
import os
import subprocess
import sys
import tempfile
import time


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def node_map(gltf):
    return {n.get("name", ""): n for n in gltf.get("nodes", [])}


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <csg_stress.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")

        start = time.monotonic()
        r = run([mc3togltf, xml_path, out])
        elapsed = time.monotonic() - start

        assert r.returncode == 0, (
            f"CSG stress export failed (returncode={r.returncode}):\n{r.stderr}"
        )

        with open(out) as f:
            gltf = json.load(f)
        nmap = node_map(gltf)
        meshes = gltf.get("meshes", [])
        accs = gltf.get("accessors", [])

        def triangle_count(node_name):
            node = nmap[node_name]
            mesh_idx = node.get("mesh")
            assert mesh_idx is not None, f"'{node_name}' has no mesh"
            prim = meshes[mesh_idx]["primitives"][0]
            idx_acc = prim.get("indices")
            assert idx_acc is not None, f"'{node_name}' primitive has no indices accessor"
            ic = accs[idx_acc].get("count", 0)
            assert ic > 0 and ic % 3 == 0, f"'{node_name}' has invalid index count {ic}"
            return ic // 3

        # STAB-0223: 10-box union.
        assert "TenBoxUnion" in nmap, f"Expected 'TenBoxUnion' node, got: {sorted(nmap.keys())}"
        ten_box_tris = triangle_count("TenBoxUnion")
        print(f"STAB-0223: 10-box union exports without crash "
              f"({ten_box_tris} triangles) — PASS")

        # STAB-0233: 5-level nested union of 32 boxes, under 5 seconds.
        assert "DeepUnion" in nmap, f"Expected 'DeepUnion' node, got: {sorted(nmap.keys())}"
        deep_tris = triangle_count("DeepUnion")
        assert elapsed < 5.0, (
            f"Expected the whole export (including the 5-level/32-box "
            f"DeepUnion) to finish in under 5s, took {elapsed:.2f}s"
        )
        print(f"STAB-0233: 5-level nested union of 32 boxes evaluates in "
              f"{elapsed:.2f}s (< 5s), {deep_tris} triangles — PASS")

    print("\nCSG stress test: PASS")
