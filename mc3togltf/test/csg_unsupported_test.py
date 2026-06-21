#!/usr/bin/env python3
"""
Test that real CSG export fails hard when an unsupported child type is present.

- Default mode: must fail (non-zero exit) with a message mentioning the
  unsupported child type.
- Approximate mode (--allow-approximate-csg): must succeed, children
  exported separately, and a warning printed.
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
        print(f"Usage: {sys.argv[0]} <mc3togltf> <csg_unsupported_child.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out_gltf = os.path.join(tmpdir, "out.gltf")

        # Default (real CSG) mode: must fail with a useful error.
        r = run([mc3togltf, xml_path, out_gltf])
        assert r.returncode != 0, (
            "Expected non-zero exit for unsupported CSG child in default mode, "
            f"got returncode=0\nstdout: {r.stdout}\nstderr: {r.stderr}"
        )
        combined = r.stdout + r.stderr
        assert (
            "watertight" in combined.lower() or
            "not supported" in combined.lower() or
            "unsupported" in combined.lower() or
            "Disk" in combined or
            "CSG evaluation failed" in combined
        ), (
            f"Expected error mentioning unsupported CSG child type, got:\n{combined}"
        )
        print(f"Unsupported CSG child (default mode): PASS — got expected error")

        # Approximate mode: must succeed, children exported separately.
        out_approx = os.path.join(tmpdir, "out_approx.gltf")
        r = run([mc3togltf, "--allow-approximate-csg", xml_path, out_approx])
        assert r.returncode == 0, (
            "Expected success with --allow-approximate-csg for unsupported CSG child, "
            f"got returncode={r.returncode}\nstderr: {r.stderr}"
        )
        assert os.path.exists(out_approx) and os.path.getsize(out_approx) > 0, \
            "Output GLTF is empty in --allow-approximate-csg mode"

        with open(out_approx) as f:
            gltf = json.load(f)

        nmap    = {n.get("name", ""): n for n in gltf.get("nodes", [])}
        meshes  = gltf.get("meshes", [])
        accs    = gltf.get("accessors", [])

        # All three nodes must be present — children exported separately.
        for expected in ("BadCsg", "BaseBox", "DiskInsideCsg"):
            assert expected in nmap, (
                f"Expected node '{expected}' in approx output, "
                f"got: {sorted(nmap.keys())}"
            )

        # BadCsg must be the parent; children must be glTF child indices of it.
        badcsg_children = {
            gltf["nodes"][ci].get("name", "")
            for ci in nmap["BadCsg"].get("children", [])
        }
        assert "BaseBox" in badcsg_children, (
            f"Expected 'BaseBox' as a child of 'BadCsg', got: {badcsg_children}"
        )
        assert "DiskInsideCsg" in badcsg_children, (
            f"Expected 'DiskInsideCsg' as a child of 'BadCsg', got: {badcsg_children}"
        )

        # Both children must have mesh geometry (vertex count > 0).
        def vertex_count(node_name):
            mesh_idx = nmap[node_name].get("mesh")
            if mesh_idx is None:
                return 0
            prims = meshes[mesh_idx].get("primitives", []) if 0 <= mesh_idx < len(meshes) else []
            if not prims:
                return 0
            pos_idx = prims[0].get("attributes", {}).get("POSITION")
            if pos_idx is None or not (0 <= pos_idx < len(accs)):
                return 0
            return accs[pos_idx].get("count", 0)

        basebox_vc = vertex_count("BaseBox")
        assert basebox_vc > 0, (
            f"Expected 'BaseBox' to have mesh geometry in approx mode, got {basebox_vc} vertices"
        )

        disk_vc = vertex_count("DiskInsideCsg")
        assert disk_vc > 0, (
            f"Expected 'DiskInsideCsg' to have mesh geometry in approx mode, got {disk_vc} vertices"
        )

        combined = r.stdout + r.stderr
        assert "approximate" in combined.lower() or "separate" in combined.lower(), (
            f"Expected approximate-mode warning in output, got:\n{combined}"
        )
        print(f"Unsupported CSG child (--allow-approximate-csg): PASS "
              f"(BaseBox={basebox_vc}v, DiskInsideCsg={disk_vc}v)")

    print("All unsupported CSG child tests: PASS")
