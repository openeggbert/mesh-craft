#!/usr/bin/env python3
"""
STAB-0662: buildCapsule()'s bottom-hemisphere phi formula skipped the
cylinder-body seam ring and overshot past the pole, producing folded/
non-manifold geometry -- ANY Capsule CSG child failed (both export via
CsgEvaluator.cpp and the editor's live preview, STAB-0670), independent
of the vertex-welding fix that was sufficient for IcoSphere.

Verifies the fix via SceneRenderer::csgCachedTriCount() (same mechanism
csg_cache_test.py/csg_icosphere_cutter_test.py use): a Box minus a
Capsule cutter must report a triangle count higher than a plain, un-cut
box's (12), proving the subtraction actually happened.

Usage: csg_capsule_cutter_test.py <MeshCraft-binary> <csg_capsule_cutter.mc3.xml>
"""
import re
import subprocess
import sys
import tempfile
import os

PLAIN_BOX_TRI_COUNT = 12  # unmodified box: 6 faces * 2 tris


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <binary> <scene>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_csg_cap_")
    os.close(fd)
    try:
        cmd = [binary, scene, "--screenshot", ppm_path]
        if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
            cmd = ["xvfb-run", "--auto-servernum"] + cmd
        r = subprocess.run(cmd, capture_output=True, text=True)
        assert r.returncode == 0, (
            f"MeshCraft exited {r.returncode}:\nstdout={r.stdout}\nstderr={r.stderr}"
        )
        assert os.path.getsize(ppm_path) > 0, "screenshot PPM is empty"

        m = re.search(r"\[CsgTriCount\] count=(\d+)", r.stdout)
        assert m, f"expected a '[CsgTriCount] count=N' line in stdout, found none.\nstdout={r.stdout}"
        tri_count = int(m.group(1))

        assert tri_count > PLAIN_BOX_TRI_COUNT, (
            f"expected Box-minus-Capsule to have MORE than a plain box's "
            f"{PLAIN_BOX_TRI_COUNT} triangles (proving the subtraction actually "
            f"happened -- the STAB-0662/STAB-0670 bugs); got {tri_count}"
        )

        print(f"PASS: CSG preview correctly subtracted Capsule cutter "
              f"({tri_count} triangles, > plain box's {PLAIN_BOX_TRI_COUNT})")
    finally:
        if os.path.exists(ppm_path):
            os.unlink(ppm_path)


if __name__ == "__main__":
    main()
