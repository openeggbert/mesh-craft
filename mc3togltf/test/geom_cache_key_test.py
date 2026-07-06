#!/usr/bin/env python3
"""
STAB-0240/0241: geometry cache key composition (buildGeomCacheKey() in
GltfExporter.cpp).

  - STAB-0240: two boxes with identical geometry but different materials
    must NOT share a glTF mesh (the cache key folds in matIdx).
  - STAB-0241: two <mesh> objects referencing the same OBJ src (and no
    material) must share a single glTF mesh.
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
        print(f"Usage: {sys.argv[0]} <mc3togltf> <geom_cache_key.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, f"Export failed (returncode={r.returncode}):\n{r.stderr}"

        with open(out) as f:
            gltf = json.load(f)
        nmap = {n.get("name", ""): n for n in gltf.get("nodes", [])}

        # STAB-0240: same geometry, different material -> different meshes.
        for name in ("BoxRed", "BoxBlue", "TetraA", "TetraB"):
            assert name in nmap, f"Expected node '{name}', got: {sorted(nmap.keys())}"
            assert nmap[name].get("mesh") is not None, f"'{name}' has no mesh"

        box_red_mesh  = nmap["BoxRed"].get("mesh")
        box_blue_mesh = nmap["BoxBlue"].get("mesh")
        assert box_red_mesh != box_blue_mesh, (
            f"BoxRed (mesh {box_red_mesh}) and BoxBlue (mesh {box_blue_mesh}) have "
            f"identical geometry but different materials — they must NOT share a mesh"
        )
        print(f"STAB-0240: different-material boxes use different meshes "
              f"({box_red_mesh} vs {box_blue_mesh}) — PASS")

        # STAB-0241: same OBJ src, no material -> shared mesh.
        tetra_a_mesh = nmap["TetraA"].get("mesh")
        tetra_b_mesh = nmap["TetraB"].get("mesh")
        assert tetra_a_mesh == tetra_b_mesh, (
            f"TetraA (mesh {tetra_a_mesh}) and TetraB (mesh {tetra_b_mesh}) reference "
            f"the same OBJ src and should share one glTF mesh"
        )
        print(f"STAB-0241: repeated OBJ mesh src reuses one glTF mesh "
              f"(mesh {tetra_a_mesh}) — PASS")

    print("\nGeometry cache key test: PASS")
