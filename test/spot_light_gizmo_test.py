#!/usr/bin/env python3
"""
Regression/verification test for STAB-0513 (spot light gizmo: sphere + cone).

drawLightGizmos() (SceneRenderer.cpp) is called unconditionally in the main
draw loop (MeshCraftApplication.cpp), alongside drawCameraGizmos()/
drawCsgGizmos() — not gated by selection state or a UI toggle the way the
translate/rotate/bbox gizmos are (STAB-0501/0502/0505). The Spot case draws
a small sphere at the light's apex plus an 8-segment cone (spokes + ring)
along its direction — same unconditional draw path as STAB-0512's point
light gizmo. spot_light_gizmo.mc3.xml places a single spot light with a
distinctive bright cyan color at the origin, aimed straight at the camera,
and no other scene content that could plausibly produce that color, so the
gizmo's sphere+cone marker should show up as a real, substantial cyan-ish
pixel cluster.

Usage: spot_light_gizmo_test.py <MeshCraft-binary> <spot_light_gizmo.mc3.xml>
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

    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_spotgizmo_")
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

        cyanish = 0
        for y in range(0, h, 2):
            for x in range(0, w, 2):
                off = (y * w + x) * 3
                r_, g_, b_ = pixels[off], pixels[off + 1], pixels[off + 2]
                if g_ > 150 and b_ > 150 and r_ < 80:
                    cyanish += 1

        assert cyanish > 30, (
            f"expected a substantial cyan-ish pixel cluster for the spot "
            f"light gizmo (sphere + cone), found only {cyanish} matching "
            f"samples — the gizmo may not be rendering"
        )

        print(f"PASS: spot_light_gizmo_test — {cyanish} cyan gizmo samples found")
    finally:
        if os.path.exists(ppm_path):
            os.remove(ppm_path)


if __name__ == "__main__":
    main()
