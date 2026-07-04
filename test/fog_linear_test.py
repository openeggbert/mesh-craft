#!/usr/bin/env python3
"""
Regression/verification test for STAB-0511 (linear fog visualization).

Unlike the bloom/SSAO/wireframe toggles (STAB-0505/0508/0509/0510), fog is a
document-level <environment><fog> setting (Mc3Environment::fog), applied
unconditionally whenever it's present — no runtime UI toggle gates it. That
makes it verifiable headlessly: fog_linear.mc3.xml places a bright red box
20 units from the camera, with linear fog (start=1, end=8, color pure green)
fully covering that distance (fogFactor clamps to 1.0), so the rendered box
should come out fog-colored, not its authored red base color.

Usage: fog_linear_test.py <MeshCraft-binary> <fog_linear.mc3.xml>
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

    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_fog_")
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
        for y in range(0, h, 2):
            for x in range(0, w, 2):
                off = (y * w + x) * 3
                r_, g_, b_ = pixels[off], pixels[off + 1], pixels[off + 2]
                if g_ > 150 and r_ < 80 and b_ < 80:
                    greenish += 1
                assert not (r_ > 150 and g_ < 80 and b_ < 80), (
                    f"found an un-fogged red pixel at ({x},{y}): "
                    f"({r_},{g_},{b_}) — the box should be fully fog-colored "
                    f"(fog end=8 << camera distance=20), not its authored red "
                    f"base color"
                )

        assert greenish > 50, (
            f"expected a substantial fog-colored (green) pixel cluster for "
            f"the fully-fogged box, found only {greenish} matching samples"
        )

        print(f"PASS: fog_linear_test — {greenish} fog-colored samples, no un-fogged red found")
    finally:
        if os.path.exists(ppm_path):
            os.remove(ppm_path)


if __name__ == "__main__":
    main()
