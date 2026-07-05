#!/usr/bin/env python3
"""
Regression/verification test for STAB-0523 (background texture renders
before the 3D scene, i.e. behind all 3D objects).

background_texture.mc3.xml sets a solid bright-green background_texture
(test/textures/solid_green.png) and places a bright red box in front of
the camera. MeshCraftApplication.cpp:436-457 draws the background
texture unconditionally, stretched to fill the viewport, before the 3D
scene is rendered. If it works, the screenshot should show a substantial
green cluster (background, uncovered by the box) AND a substantial red
cluster (the box, rendered on top of/after the background) in the same
frame. If the background texture were missing, broken, or drawn *after*
(covering) the 3D scene, one of those two clusters would be absent.

Usage: background_texture_test.py <MeshCraft-binary> <background_texture.mc3.xml>
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

    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_bgtex_")
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

        greenish = 0
        reddish = 0
        for y in range(0, h, 2):
            for x in range(0, w, 2):
                off = (y * w + x) * 3
                r_, g_, b_ = pixels[off], pixels[off + 1], pixels[off + 2]
                if g_ > 150 and r_ < 80 and b_ < 80:
                    greenish += 1
                elif r_ > 120 and g_ < 80 and b_ < 80:
                    reddish += 1

        assert greenish > 200, (
            f"expected a substantial green pixel cluster (the background "
            f"texture behind the box), found only {greenish} matching "
            f"samples — the background texture may not be rendering"
        )
        assert reddish > 30, (
            f"expected a substantial red pixel cluster (the foreground "
            f"box), found only {reddish} matching samples — either the box "
            f"isn't rendering or the background texture is covering it"
        )

        print(f"PASS: background_texture_test — {greenish} green + {reddish} red samples found")
    finally:
        if os.path.exists(ppm_path):
            os.remove(ppm_path)


if __name__ == "__main__":
    main()
