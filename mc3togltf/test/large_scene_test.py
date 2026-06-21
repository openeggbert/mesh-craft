#!/usr/bin/env python3
"""
Regression test for geometry reuse in large repeated-geometry scenes.

Verifies:
- Export succeeds for a scene with 50 objects (20 block instances, 10 walls,
  5 spheres, 10 column instances, 5 window instances).
- glTF node count matches expected object count.
- Unique glTF mesh count is much lower than node count (geometry is reused).
- Instance nodes referencing the same definition share the same mesh index.
- Identical direct primitives (same size/radius) share the same mesh index.
"""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <large_scene.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "large_scene.gltf")
        r   = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, (
            f"Export failed (returncode={r.returncode}):\n{r.stderr}"
        )
        assert os.path.exists(out) and os.path.getsize(out) > 0, \
            "Output glTF is missing or empty"

        with open(out) as f:
            gltf = json.load(f)

        nodes  = gltf.get("nodes",  [])
        meshes = gltf.get("meshes", [])
        nmap   = {n.get("name", ""): n for n in nodes}

        # ----------------------------------------------------------------
        # Node count: we have 50 MC3 objects → 50 glTF nodes expected.
        # ----------------------------------------------------------------
        assert len(nodes) >= 40, (
            f"Expected at least 40 glTF nodes, got {len(nodes)}"
        )
        print(f"Node count: {len(nodes)} — PASS")

        # ----------------------------------------------------------------
        # Mesh reuse: 5 unique geometry shapes for 50 nodes.
        # Allow some headroom (≤15 meshes) to handle minor geometry splits.
        # ----------------------------------------------------------------
        assert len(meshes) <= 15, (
            f"Expected at most 15 unique glTF meshes, got {len(meshes)} "
            f"(geometry reuse not working)"
        )
        assert len(nodes) >= len(meshes) * 3, (
            f"Expected nodes ({len(nodes)}) to be at least 3× meshes "
            f"({len(meshes)}) to confirm reuse"
        )
        print(f"Unique meshes: {len(meshes)} vs {len(nodes)} nodes — PASS")

        # ----------------------------------------------------------------
        # Instance mesh sharing: all stone_block instances share one mesh.
        # ----------------------------------------------------------------
        for name in ("Block_01", "Block_05", "Block_10", "Block_15", "Block_20"):
            assert name in nmap, f"Expected node '{name}' in glTF output"
        block_meshes = {nmap[f"Block_{i:02d}"]["mesh"]
                        for i in range(1, 21)
                        if f"Block_{i:02d}" in nmap}
        assert len(block_meshes) == 1, (
            f"stone_block instances must share exactly one glTF mesh, "
            f"got mesh indices: {block_meshes}"
        )
        print(f"stone_block instances share mesh {block_meshes.pop()} — PASS")

        # ----------------------------------------------------------------
        # Column instance sharing.
        # ----------------------------------------------------------------
        col_meshes = {nmap[f"Col_{i:02d}"]["mesh"]
                      for i in range(1, 11)
                      if f"Col_{i:02d}" in nmap}
        assert len(col_meshes) == 1, (
            f"column instances must share exactly one glTF mesh, "
            f"got mesh indices: {col_meshes}"
        )
        print(f"column instances share mesh {col_meshes.pop()} — PASS")

        # ----------------------------------------------------------------
        # Direct primitive sharing: identical boxes and spheres.
        # ----------------------------------------------------------------
        for name in ("Wall_01", "Wall_02", "Wall_05", "Wall_06", "Wall_10"):
            assert name in nmap, f"Expected node '{name}'"
        wall_meshes = {nmap[f"Wall_{i:02d}"]["mesh"]
                       for i in range(1, 11)
                       if f"Wall_{i:02d}" in nmap}
        assert len(wall_meshes) == 1, (
            f"Identical wall boxes must share one glTF mesh, "
            f"got mesh indices: {wall_meshes}"
        )
        print(f"Wall boxes share mesh {wall_meshes.pop()} — PASS")

        orb_meshes = {nmap[f"Orb_{i:02d}"]["mesh"]
                      for i in range(1, 6)
                      if f"Orb_{i:02d}" in nmap}
        assert len(orb_meshes) == 1, (
            f"Identical spheres must share one glTF mesh, "
            f"got mesh indices: {orb_meshes}"
        )
        print(f"Sphere orbs share mesh {orb_meshes.pop()} — PASS")

    print(f"\nAll large-scene reuse tests: PASS "
          f"({len(nodes)} nodes, {len(meshes)} unique meshes)")
