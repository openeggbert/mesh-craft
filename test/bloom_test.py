#!/usr/bin/env python3
"""
Regression/verification test for AUD-084: Bloom post-processing (I6) migrated
from a legacy native FBO+shader pipeline
glGenFramebuffers/glCreateShader/etc.) onto CNA's own RenderTarget2D +
ShaderEffect + SpriteBatch. This did not exist as a dedicated test before --
the only prior coverage (AUD-058) proved the GL resource pool is fully
released on shutdown, not that bloom is visually correct.

bloom.mc3.xml: a single bright-white emissive box, centered in frame, on a
near-black background with open space around it. --screenshot rendered
with and without MESHCRAFT_TEST_FORCE_POSTFX=1 (the existing AUD-058
test-only hook that force-enables bloom for one headless frame, since
bloom/SSAO are otherwise UI-menu-only toggles with no CLI/scene-file
equivalent).

Real pixel sampling proves: (1) the box's own center stays bright with or
without bloom (sanity check), (2) a "halo" point a few pixels outside the
box's own silhouette -- pure background, no geometry there at all -- is
measurably brighter with bloom on than off (only additive glow spillover
from the blur passes can explain that), (3) a "far" point well beyond the
blur kernel's small (~5px) reach stays background-dark even with bloom on,
so the halo isn't just "bloom washes out the whole image".

Usage: bloom_test.py <MeshCraft-binary> <bloom.mc3.xml>
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


def render(binary, scene, ppm_path, force_postfx):
    cmd = [binary, scene, "--screenshot", ppm_path]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        cmd = ["xvfb-run", "--auto-servernum"] + cmd
    env = dict(os.environ)
    if force_postfx:
        env["MESHCRAFT_TEST_FORCE_POSTFX"] = "1"
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
        print(f"Usage: {sys.argv[0]} <binary> <bloom.mc3.xml>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    # Empirically measured against this fixture's own 800x480 default
    # screenshot resolution and camera framing (see AUD-084's plan.md row):
    # the box's own right edge sits at x=460 on row y=300 (clear of the
    # Properties-panel overlay further up); the blur kernel's actual reach
    # is small (~5px), so the halo point must be close to the edge.
    center = (408, 300)
    halo = (463, 300)
    far = (560, 300)

    fd1, off_ppm = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_bloom_off_")
    os.close(fd1)
    fd2, on_ppm = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_bloom_on_")
    os.close(fd2)
    try:
        w1, h1, off_px = render(binary, scene, off_ppm, force_postfx=False)
        w2, h2, on_px = render(binary, scene, on_ppm, force_postfx=True)
        assert (w1, h1) == (w2, h2), f"resolution mismatch: {(w1,h1)} vs {(w2,h2)}"

        center_off = sample(w1, h1, off_px, *center)
        center_on = sample(w2, h2, on_px, *center)
        halo_off = sample(w1, h1, off_px, *halo)
        halo_on = sample(w2, h2, on_px, *halo)
        far_off = sample(w1, h1, off_px, *far)
        far_on = sample(w2, h2, on_px, *far)

        assert center_off[0] > 200 and center_on[0] > 200, (
            f"box center should be bright with or without bloom, "
            f"got off={center_off} on={center_on}"
        )
        assert halo_off[0] < 20, (
            f"halo point {halo} should be background-dark with bloom OFF "
            f"(it's pure background, no geometry there), got {halo_off}"
        )
        assert halo_on[0] - halo_off[0] > 30, (
            f"halo point {halo} should be measurably brighter with bloom ON "
            f"than OFF -- only additive glow spillover from the blur passes "
            f"can brighten a point with no geometry -- got off={halo_off} "
            f"on={halo_on} (delta={halo_on[0] - halo_off[0]})"
        )
        assert far_on[0] < 20, (
            f"far point {far}, well beyond the blur kernel's reach, should "
            f"stay background-dark even with bloom ON, got off={far_off} "
            f"on={far_on} -- a bright reading here would mean bloom is "
            f"washing out the whole image rather than a real localized glow"
        )

        print(
            f"PASS: bloom_test — center off={center_off} on={center_on}; "
            f"halo off={halo_off} on={halo_on} (delta={halo_on[0]-halo_off[0]}); "
            f"far off={far_off} on={far_on}"
        )
    finally:
        if os.path.exists(off_ppm):
            os.remove(off_ppm)
        if os.path.exists(on_ppm):
            os.remove(on_ppm)


if __name__ == "__main__":
    main()
