#!/usr/bin/env python3
"""
Regression/verification test for STAB-0517 ("Look through camera" renders
from the Mc3Camera's own position/rotation, not the default editor camera).

look_through_camera.mc3.xml places a bright yellow box at z=+50, behind
where the default EditorCamera (yaw=0, pitch=0.4, distance=15,
target=(0,0,0)) looks — that default view is centered on the origin
looking toward -z, so the box is entirely outside it. The scene's
default_camera ("Behind") sits right next to the box, aimed straight at
it, so the box fills much of the frame *only if* MeshCraftApplication's
lookThroughCamera_ override (which replaces view/proj with the Mc3Camera's
own position/target/fov in --screenshot mode) is actually wired up. If
that override were broken, the render would silently fall back to the
default editor camera and show no yellow at all.

Usage: look_through_camera_test.py <MeshCraft-binary> <look_through_camera.mc3.xml>
"""
import subprocess
import sys
import tempfile
import os


def parse_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:2] == b"P6", "not a raw PPM (P6) file"
    i = 2
    vals = []
    while len(vals) < 3:
        while data[i] in b" \t\n\r":
            i += 1
        start = i
        while data[i] not in b" \t\n\r":
            i += 1
        vals.append(int(data[start:i]))
    w, h, _maxval = vals
    i += 1
    pixels = data[i:]
    return w, h, pixels


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <binary> <scene>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_lookthrough_")
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

        w, h, pixels = parse_ppm(ppm_path)

        yellowish = 0
        for y in range(0, h, 2):
            for x in range(0, w, 2):
                off = (y * w + x) * 3
                r_, g_, b_ = pixels[off], pixels[off + 1], pixels[off + 2]
                if r_ > 120 and g_ > 120 and b_ < 80:
                    yellowish += 1

        assert yellowish > 200, (
            f"expected a substantial yellow pixel cluster (the Target box "
            f"filling much of the frame as seen from the scene's own "
            f"'Behind' camera), found only {yellowish} matching samples — "
            f"the render may have fallen back to the default editor camera "
            f"instead of the Mc3Camera's own position/target/fov"
        )

        print(f"PASS: look_through_camera_test — {yellowish} yellow samples found")
    finally:
        if os.path.exists(ppm_path):
            os.remove(ppm_path)


if __name__ == "__main__":
    main()
