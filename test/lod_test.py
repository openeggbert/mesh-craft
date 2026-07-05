#!/usr/bin/env python3
"""
Regression/verification test for STAB-0525 (LOD: primitive segment count
decreases at distance).

SceneRenderer::draw() picks a LOD tier per object (0=full/32-segment,
1=mid/16-segment, 2=low/6-segment for a sphere) based on squared camera
distance to the object's world pivot (SceneRenderer.cpp:633-639, G8):
<10 units -> 0, 10-40 units -> 1, >40 units -> 2. This is camera/document
-driven, not a UI toggle, so it's reachable headlessly via --screenshot.

lod_near.mc3.xml places a sphere 3 units from the camera (expect
lodLevel 0); lod_far.mc3.xml is the identical sphere 60 units away
(expect lodLevel 2). MeshCraftApplication::EndDraw() prints
"[LOD] level=N" (SceneRenderer::lastLodLevel(), incremented on every
draw of the front object) right before exiting --screenshot mode. This
test runs both scenes and asserts the reported level actually differs
as expected — not just that some level is printed.

Usage: lod_test.py <MeshCraft-binary> <lod_near.mc3.xml> <lod_far.mc3.xml>
"""
import re
import subprocess
import sys
import tempfile
import os


def run_and_get_lod(binary, scene):
    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_lod_")
    os.close(fd)
    try:
        cmd = [binary, scene, "--screenshot", ppm_path]
        if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
            cmd = ["xvfb-run", "--auto-servernum"] + cmd
        r = subprocess.run(cmd, capture_output=True, text=True)
        assert r.returncode == 0, (
            f"MeshCraft exited {r.returncode} on {scene}:\n"
            f"stdout={r.stdout}\nstderr={r.stderr}"
        )
        assert os.path.getsize(ppm_path) > 0, f"screenshot PPM is empty for {scene}"

        m = re.search(r"\[LOD\] level=(-?\d+)", r.stdout)
        assert m, f"expected a '[LOD] level=N' line in stdout for {scene}, found none.\nstdout={r.stdout}"
        return int(m.group(1))
    finally:
        if os.path.exists(ppm_path):
            os.remove(ppm_path)


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <binary> <lod_near.mc3.xml> <lod_far.mc3.xml>")
        sys.exit(1)
    binary, near_scene, far_scene = sys.argv[1], sys.argv[2], sys.argv[3]

    near_level = run_and_get_lod(binary, near_scene)
    far_level = run_and_get_lod(binary, far_scene)

    assert near_level == 0, (
        f"expected the near camera (3 units away) to render the sphere at "
        f"full LOD (level 0), got level {near_level}"
    )
    assert far_level == 2, (
        f"expected the far camera (60 units away) to render the sphere at "
        f"low LOD (level 2), got level {far_level}"
    )
    assert far_level > near_level, (
        f"expected LOD tier to actually increase (lower detail) with "
        f"camera distance, got near={near_level} far={far_level} — the "
        f"segment count may not be decreasing at distance"
    )

    print(f"PASS: lod_test — near=level {near_level}, far=level {far_level}")


if __name__ == "__main__":
    main()
