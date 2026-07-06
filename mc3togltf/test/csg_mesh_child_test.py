#!/usr/bin/env python3
"""
STAB-0210: a <mesh> (external OBJ) child inside real CSG is rejected with
a clear, named error -- even when the OBJ itself is perfectly watertight
(tetra.obj). This confirms CsgEvaluator.cpp's Mesh-type rejection is an
unconditional, type-based pre-check ("is not supported in real CSG
export"), not a Manifold-detected non-manifold-geometry failure -- that
separate code path (real Manifold::Status() error for a degenerate
triangulated primitive) is currently unreachable via any object type,
since buildPrimitive() always produces valid watertight meshes.

Approximate mode must still succeed, exporting the mesh child separately.
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
        print(f"Usage: {sys.argv[0]} <mc3togltf> <csg_mesh_child.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        # Default (real CSG) mode: must fail with a named, specific error.
        out_gltf = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, xml_path, out_gltf])
        assert r.returncode != 0, (
            "Expected non-zero exit for a Mesh-type CSG child in default mode, "
            f"got returncode=0\nstdout: {r.stdout}"
        )
        combined = r.stdout + r.stderr
        assert "TetraInsideCsg" in combined and "Mesh" in combined, (
            f"Expected the error to name the child 'TetraInsideCsg' and its "
            f"type 'Mesh', got:\n{combined}"
        )
        assert "not supported" in combined.lower(), (
            f"Expected 'not supported' in the error, got:\n{combined}"
        )
        print("STAB-0210: Mesh-type CSG child rejected with a named error "
              "(default/strict mode) — PASS")

        # Approximate mode: must still succeed, exporting the mesh separately.
        out_approx = os.path.join(tmpdir, "out_approx.gltf")
        r2 = run([mc3togltf, "--allow-approximate-csg", xml_path, out_approx])
        assert r2.returncode == 0, (
            f"Expected --allow-approximate-csg to succeed despite the Mesh "
            f"child, got returncode={r2.returncode}\nstderr: {r2.stderr}"
        )
        with open(out_approx) as f:
            gltf = json.load(f)
        nmap = {n.get("name", ""): n for n in gltf.get("nodes", [])}
        assert "TetraInsideCsg" in nmap, (
            f"Expected 'TetraInsideCsg' as a separate node in approximate mode, "
            f"got: {sorted(nmap.keys())}"
        )
        assert nmap["TetraInsideCsg"].get("mesh") is not None, (
            "Expected 'TetraInsideCsg' to have real mesh geometry in approximate mode"
        )
        print("STAB-0210: Mesh-type CSG child exported separately in "
              "--allow-approximate-csg mode — PASS")

    print("\nCSG Mesh-child rejection test: PASS")
