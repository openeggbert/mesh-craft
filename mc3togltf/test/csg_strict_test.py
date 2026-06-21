#!/usr/bin/env python3
"""
Test CSG export behavior:
- Real CSG (default): csg_test.mc3.xml must export successfully.
- Approximate mode (--allow-approximate-csg): must also succeed but prints a warning.
- Invalid extension: must fail with a useful message.
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
        print(f"Usage: {sys.argv[0]} <mc3togltf> <csg_test.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    csg_xml   = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out_gltf = os.path.join(tmpdir, "out.gltf")

        # Default mode: real CSG evaluation must succeed.
        r = run([mc3togltf, csg_xml, out_gltf])
        assert r.returncode == 0, (
            "Expected success with real CSG (default mode), "
            f"got returncode={r.returncode}\nstderr: {r.stderr}"
        )
        assert os.path.exists(out_gltf) and os.path.getsize(out_gltf) > 0, \
            "Output GLTF is empty in default (real CSG) mode"

        with open(out_gltf) as f:
            gltf = json.load(f)

        # All three CSG root nodes must have a mesh (not just children).
        nmap = {n.get("name", ""): n for n in gltf.get("nodes", [])}
        for csg_name in ("ArchHole", "Merged", "RoundedBlock"):
            assert csg_name in nmap, (
                f"CSG node '{csg_name}' missing from output.\n"
                f"Present nodes: {sorted(nmap.keys())}"
            )
            node = nmap[csg_name]
            assert node.get("mesh") is not None, (
                f"CSG node '{csg_name}' has no mesh in real CSG mode "
                f"(children may have been exported instead of the boolean result)"
            )

        # Child nodes (BoxA, s1, s2, etc.) must NOT appear as top-level nodes
        # or as children of the CSG node — they are merged into the CSG mesh.
        child_names = {"block", "cavity", "s1", "s2", "b", "s"}
        for child in child_names:
            if child in nmap:
                # The child is a glTF node — make sure it's NOT a child of any
                # of the CSG nodes (it should be gone entirely).
                for csg_name in ("ArchHole", "Merged", "RoundedBlock"):
                    assert csg_name in nmap, "already checked above"
                    csg_node = nmap[csg_name]
                    for ci in csg_node.get("children", []):
                        cn = gltf["nodes"][ci].get("name", "")
                        assert cn != child, (
                            f"CSG child '{child}' appears as a glTF child of '{csg_name}'"
                            " — should be baked into the CSG mesh"
                        )

        print("Real CSG export (default mode): PASS")

        # Approximate mode: must also succeed, children exported separately.
        out_approx = os.path.join(tmpdir, "out_approx.gltf")
        r = run([mc3togltf, "--allow-approximate-csg", csg_xml, out_approx])
        assert r.returncode == 0, (
            f"Expected success with --allow-approximate-csg, got returncode={r.returncode}\n"
            f"stderr: {r.stderr}"
        )
        combined = r.stdout + r.stderr
        assert "approximate" in combined.lower() or "separate" in combined.lower(), (
            f"Expected approximate-mode warning in output, got:\n{combined}"
        )
        print("Approximate CSG export (--allow-approximate-csg): PASS")

        # Invalid output extension: must fail with a useful message.
        bad_out = os.path.join(tmpdir, "out.foo")
        r = run([mc3togltf, csg_xml, bad_out])
        assert r.returncode != 0, (
            "Expected non-zero exit for unknown extension '.foo', "
            f"but got returncode={r.returncode}"
        )
        combined = r.stdout + r.stderr
        assert ("Unknown output extension" in combined or
                "Only .gltf and .glb" in combined or
                ".foo" in combined), (
            f"Expected useful error message for unknown extension, got:\n{combined}"
        )
        print("Invalid extension rejection: PASS")

    print("All CSG/extension tests: PASS")
