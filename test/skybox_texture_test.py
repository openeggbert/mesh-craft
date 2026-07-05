#!/usr/bin/env python3
"""
Regression/verification test for STAB-0524 (skybox panorama shader: an
equirectangular image is actually displayed).

skybox_texture.mc3.xml sets a solid-blue equirectangular skybox_texture,
no background_texture, and no foreground objects. drawSkybox()
(MeshCraftApplication.cpp:1207+) samples the texture along each pixel's
camera-space ray direction via u_right/u_up/u_forward/u_tanFov uniforms
— for a solid-color image, every ray direction maps to the same flat
color, so if the skybox renders at all, it should fill the 3D viewport
with that blue. The editor's left/right panels, menu bar, and status
bar occupy a sizeable share of the full 800x480 screenshot, so this
samples a safe rectangle known to sit inside the 3D viewport rather
than requiring a majority of the whole image to match.

This test previously caught a real bug (STAB-0524): drawSkybox() built
and bound a dedicated VAO/VBO for its full-screen quad, but that
VAO-based vertex-attribute path silently produced a degenerate,
invisible draw in this environment (confirmed via a hardcoded solid-
color fragment shader that still rendered nothing) — while switching
to the gl_VertexID-based procedural quad generation already used by the
working bloom passes fixed it immediately.

Usage: skybox_texture_test.py <MeshCraft-binary> <skybox_texture.mc3.xml>
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

    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_skybox_")
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

        # A rectangle safely inside the 3D viewport for the standard
        # 800x480 headless screenshot size (well clear of the left/right
        # panels, menu bar, and status bar) — see MeshCraftApplication.cpp's
        # kLeftPanelW/kRightPanelW/kStatusH/imguiTopH_ viewport math.
        x0, x1 = int(w * 0.36), int(w * 0.64)
        y0, y1 = int(h * 0.25), int(h * 0.65)

        blueish = 0
        total = 0
        for y in range(y0, y1, 2):
            for x in range(x0, x1, 2):
                off = (y * w + x) * 3
                r_, g_, b_ = pixels[off], pixels[off + 1], pixels[off + 2]
                total += 1
                # Loose blue-dominant check: accepts both the pure skybox
                # color and blue-tinted pixels where a thin foreground
                # overlay (camera gizmo line, ground grid line) blends with
                # the sky behind it — still clearly "blue", not gray/other.
                if b_ > r_ + 30 and b_ > g_ + 30:
                    blueish += 1

        ratio = blueish / total if total else 0.0
        assert ratio > 0.8, (
            f"expected the solid-blue skybox to fill this viewport-interior "
            f"sample rectangle almost entirely (no foreground objects, no "
            f"background_texture), found only {blueish}/{total} ({ratio:.0%}) "
            f"blue-ish samples — the skybox may not be rendering"
        )

        print(f"PASS: skybox_texture_test — {blueish}/{total} ({ratio:.0%}) blue samples found in viewport-interior sample")
    finally:
        if os.path.exists(ppm_path):
            os.remove(ppm_path)


if __name__ == "__main__":
    main()
