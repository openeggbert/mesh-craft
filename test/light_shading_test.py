#!/usr/bin/env python3
"""
Regression/verification test for the 2026-07-20 audit finding #4 fix:
doc.lights now actually affects live-viewport shading (mapped onto
BasicEffect's DirectionalLight0-2 / AmbientLightColor), instead of the
fixed 3-point default rig being used unconditionally regardless of what
was authored.

light_shading.mc3.xml places a white-material sphere lit by a single
strong, pure-red directional light aimed at the camera-facing hemisphere.
light_shading_control.mc3.xml is the identical scene with NO authored
lights (falls back to the default warm-white rig). Real pixel sampling
proves: (1) the lit scene's sphere is visibly red-dominant, and (2) the
control scene's sphere is NOT red-dominant (so the result is caused by the
authored light, not a default-rig coincidence).

Usage: light_shading_test.py <MeshCraft-binary> <light_shading.mc3.xml> <light_shading_control.mc3.xml>
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


def render(binary, scene, ppm_path):
    cmd = [binary, scene, "--screenshot", ppm_path]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        cmd = ["xvfb-run", "--auto-servernum"] + cmd
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, (
        f"MeshCraft exited {r.returncode}:\nstdout={r.stdout}\nstderr={r.stderr}"
    )
    assert os.path.getsize(ppm_path) > 0, "screenshot PPM is empty"
    return parse_ppm(ppm_path)


def count_red_dominant(w, h, pixels):
    count = 0
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            off = (y * w + x) * 3
            r_, g_, b_ = pixels[off], pixels[off + 1], pixels[off + 2]
            if r_ > 120 and g_ < 60 and b_ < 60:
                count += 1
    return count


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <binary> <light_shading.mc3.xml> <light_shading_control.mc3.xml>")
        sys.exit(1)
    binary, lit_scene, control_scene = sys.argv[1], sys.argv[2], sys.argv[3]

    fd1, lit_ppm = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_lightshading_lit_")
    os.close(fd1)
    fd2, control_ppm = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_lightshading_ctrl_")
    os.close(fd2)
    try:
        w1, h1, lit_pixels = render(binary, lit_scene, lit_ppm)
        w2, h2, control_pixels = render(binary, control_scene, control_ppm)

        lit_red = count_red_dominant(w1, h1, lit_pixels)
        control_red = count_red_dominant(w2, h2, control_pixels)

        assert lit_red > 200, (
            f"light_shading.mc3.xml (red directional light): expected a substantial "
            f"red-dominant pixel cluster on the sphere's lit hemisphere, found only "
            f"{lit_red} matching samples -- doc.lights does not appear to be reaching "
            f"live shading"
        )
        assert control_red < 10, (
            f"light_shading_control.mc3.xml (no lights, default rig): expected "
            f"essentially no red-dominant pixels, found {control_red} -- the default "
            f"rig itself should not look red, so a high count here would mean the "
            f"'lit' scene's redness isn't actually caused by its authored light"
        )

        print(f"PASS: light_shading_test — lit scene: {lit_red} red-dominant samples, "
              f"control scene: {control_red} red-dominant samples (default rig unaffected)")
    finally:
        if os.path.exists(lit_ppm):
            os.remove(lit_ppm)
        if os.path.exists(control_ppm):
            os.remove(control_ppm)


if __name__ == "__main__":
    main()
