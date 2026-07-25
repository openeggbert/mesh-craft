#!/usr/bin/env python3
"""
Regression/verification test for AUD-087: the material-preview swatch
(PropertiesPanel's "D7" sphere, shown when a material is selected in the
left panel) migrated from a hand-rolled raw-GL FBO+shader pipeline onto
CNA's RenderTarget2D + ShaderEffect + SpriteBatch. This feature never had
any test coverage before -- it is only ever drawn inside an ImGui panel
gated on UI selection state, with no CLI/scene-file equivalent, so a plain
headless --screenshot run never exercised it at all.

MESHCRAFT_TEST_FORCE_MATPREVIEW=1 (a new test-only hook, same shape as
AUD-058's MESHCRAFT_TEST_FORCE_POSTFX) renders a swatch with a known
color (pure red, roughness=0.5, metallic=0.0) and blits it into the
screen's top-left 128x128 corner in EndDraw(), after ImGui's own real
rendering (drawImGuiUi() during Draw() only queues ImGui's draw list; the
actual rasterization -- which would otherwise cover this exact corner
with the left panel's toolbar/hierarchy -- happens later in EndDraw()'s
CNA ImGui renderer call).

Real pixel sampling proves: (1) the swatch center is red-dominant (the SDF
sphere with Blinn-Phong shading applied), (2) the swatch corners (outside
the SDF sphere's radius, per kMatPreviewFragSrc's `discard`) show the
dark-gray clear color, not red, confirming the discard/sphere-shape logic
survived the migration, and (3) without the env var, that same corner
region shows neither color (the swatch never renders at all).

Usage: matpreview_test.py <MeshCraft-binary> <any .mc3.xml scene>
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


def render(binary, scene, ppm_path, force_matpreview):
    cmd = [binary, scene, "--screenshot", ppm_path]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        cmd = ["xvfb-run", "--auto-servernum"] + cmd
    env = dict(os.environ)
    if force_matpreview:
        env["MESHCRAFT_TEST_FORCE_MATPREVIEW"] = "1"
    r = subprocess.run(cmd, capture_output=True, text=True, env=env)
    assert r.returncode == 0, (
        f"MeshCraft exited {r.returncode}:\nstdout={r.stdout}\nstderr={r.stderr}"
    )
    assert os.path.getsize(ppm_path) > 0, "screenshot PPM is empty"
    return parse_ppm(ppm_path)


def sample(w, h, pixels, x, y):
    off = (y * w + x) * 3
    return pixels[off], pixels[off + 1], pixels[off + 2]


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <binary> <scene.mc3.xml>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    center = (64, 64)
    corner = (5, 5)

    fd1, off_ppm = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_matpreview_off_")
    os.close(fd1)
    fd2, on_ppm = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_matpreview_on_")
    os.close(fd2)
    try:
        w1, h1, off_px = render(binary, scene, off_ppm, force_matpreview=False)
        w2, h2, on_px = render(binary, scene, on_ppm, force_matpreview=True)
        assert (w1, h1) == (w2, h2), f"resolution mismatch: {(w1,h1)} vs {(w2,h2)}"

        center_off = sample(w1, h1, off_px, *center)
        center_on = sample(w2, h2, on_px, *center)
        corner_on = sample(w2, h2, on_px, *corner)

        assert center_on[0] > 100 and center_on[1] < 40 and center_on[2] < 40, (
            f"swatch center should be red-dominant with the hook enabled "
            f"(pure red material), got {center_on}"
        )
        assert not (center_off[0] > 100 and center_off[1] < 40 and center_off[2] < 40), (
            f"swatch center should NOT be red-dominant without the hook -- "
            f"the swatch should never render at all, got {center_off} "
            f"(matches on-state, suggesting the hook has no effect)"
        )
        assert abs(corner_on[0] - 46) <= 8 and abs(corner_on[1] - 46) <= 8 and abs(corner_on[2] - 46) <= 8, (
            f"swatch corner (outside the SDF sphere's radius) should show "
            f"the dark-gray clear color (~46,46,46), got {corner_on} -- "
            f"the discard-based sphere shape may not have survived the migration"
        )

        print(
            f"PASS: matpreview_test — center off={center_off} on={center_on}; "
            f"corner on={corner_on}"
        )
    finally:
        if os.path.exists(off_ppm):
            os.remove(off_ppm)
        if os.path.exists(on_ppm):
            os.remove(on_ppm)


if __name__ == "__main__":
    main()
