#!/usr/bin/env python3
"""Test CSG strict-failure and --allow-approximate-csg behavior, and invalid extension rejection."""
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
        out_glb = os.path.join(tmpdir, "out.glb")

        # Without flag: must fail (CSG present)
        r = run([mc3togltf, csg_xml, out_glb])
        assert r.returncode != 0, (
            "Expected non-zero exit without --allow-approximate-csg, "
            f"but got returncode={r.returncode}"
        )
        combined = r.stdout + r.stderr
        assert "CSG" in combined or "csg" in combined.lower(), (
            f"Expected CSG error message in output, got:\n{combined}"
        )
        print("CSG strict-fail without flag: PASS")

        # With flag: must succeed
        r = run([mc3togltf, "--allow-approximate-csg", csg_xml, out_glb])
        assert r.returncode == 0, (
            f"Expected success with --allow-approximate-csg, got returncode={r.returncode}\n"
            f"stderr: {r.stderr}"
        )
        print("CSG approximate export with flag: PASS")

        # Invalid output extension: must fail with a useful message
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
