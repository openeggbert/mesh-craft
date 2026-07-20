#!/usr/bin/env python3
"""
Regression test for the 2026-07-20 audit finding #6 fix: a camera
authored with `rotation` instead of `target` used to be ignored by both
the live viewport's camera gizmo and its Look-Through-Camera mode -- both
always derived the view direction from `target` (left at its {0,0,0}
default when unused), silently pointing the camera at the origin instead
of the authored direction, while mc3togltf's export already handled
`rotation` correctly.

camera_rotation.mc3.xml places a box at camera_position + (0,0,-distance)
(this project's right_handed_y_up "looks down -Z at identity rotation"
convention) and gives the camera an identity `rotation` ("0 0 0") with NO
`target` authored (so it defaults to {0,0,0}, a point far outside this
camera's frame). `--screenshot` renders through the document's own camera
(the same code path Look-Through-Camera mode uses) -- the box is visible
dead-center if and only if `rotation` is actually honored.

Usage: camera_rotation_test.py <MeshCraft-binary> <camera_rotation.mc3.xml>
"""
import os
import subprocess
import sys
import tempfile


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
        print(f"Usage: {sys.argv[0]} <binary> <camera_rotation.mc3.xml>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_camrot_")
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

        # Sample a region around the center of the frame -- the box, if
        # visible at all, occupies a solid block there (it's dead-center
        # by construction). The background is a dark viewport color; the
        # box's white material is bright. Count "light" pixels in a
        # central sampling window.
        light_count = 0
        cx, cy = w // 2, h // 2
        window = 40
        for y in range(cy - window, cy + window, 2):
            for x in range(cx - window, cx + window, 2):
                off = (y * w + x) * 3
                r_, g_, b_ = pixels[off], pixels[off + 1], pixels[off + 2]
                if r_ > 150 and g_ > 150 and b_ > 150:
                    light_count += 1

        assert light_count > 200, (
            f"camera_rotation.mc3.xml: expected the box (a bright white material) "
            f"to be visible dead-center in frame -- a camera authored with "
            f"`rotation` (target left at its unused {{0,0,0}} default) that "
            f"correctly honors `rotation` should look straight at it -- but found "
            f"only {light_count} bright samples in the central region, consistent "
            f"with the camera instead looking at the origin (target's default), "
            f"far outside this frame"
        )

        print(f"PASS: camera_rotation_test — {light_count} bright (box) samples "
              f"found dead-center, confirming the camera's `rotation` (not the "
              f"unused default `target`) was actually honored")
    finally:
        if os.path.exists(ppm_path):
            os.remove(ppm_path)


if __name__ == "__main__":
    main()
