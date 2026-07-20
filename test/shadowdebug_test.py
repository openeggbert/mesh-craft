#!/usr/bin/env python3
"""
Regression/verification test for AUD-088: Shadow Map Debug (I7) migrated
from a hand-rolled raw-GL FBO + depth-texture pair onto CNA's
RenderTarget2D. Unlike AUD-084/086/087, this pass needs no custom
ShaderEffect at all -- it reuses the normal, already-CNA-based
sceneRenderer_->draw() path unchanged, just redirected into an off-screen
target instead of the backbuffer.

Shadow Map Debug is a UI-menu-only toggle (no CLI/scene-file equivalent),
and even when enabled it only renders when the document has a directional
light with cast_shadows="true". shadow_debug.mc3.xml provides exactly
that (one light, one box). MESHCRAFT_TEST_FORCE_SHADOWDEBUG=1 (a new
test-only hook, same shape as AUD-058/AUD-087's own) force-enables the
toggle for the one headless frame the screenshot path renders, and blits
the resulting render target into a fixed, known top-right screen corner
-- deliberately not relying on the real "Shadow Frustum" ImGui overlay's
own ImGuiCond_FirstUseEver window layout (title bar height, borders) for
a test's pixel coordinates, which would be more fragile.

Real pixel sampling proves: (1) the corner region is the light-view clear
color everywhere except a small cluster near its center, where the box
(centered at the world origin, viewed by an ortho camera looking straight
down at it) actually rendered, and (2) without the hook, that same corner
region shows neither the clear color nor the box -- the pass never runs.

Usage: shadowdebug_test.py <MeshCraft-binary> <shadow_debug.mc3.xml>
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


def render(binary, scene, ppm_path, force_shadowdebug):
    cmd = [binary, scene, "--screenshot", ppm_path]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        cmd = ["xvfb-run", "--auto-servernum"] + cmd
    env = dict(os.environ)
    if force_shadowdebug:
        env["MESHCRAFT_TEST_FORCE_SHADOWDEBUG"] = "1"
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
        print(f"Usage: {sys.argv[0]} <binary> <shadow_debug.mc3.xml>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    RES = 256
    CLEAR = (102, 102, 128)

    fd1, off_ppm = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_shadowdebug_off_")
    os.close(fd1)
    fd2, on_ppm = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_shadowdebug_on_")
    os.close(fd2)
    try:
        w1, h1, off_px = render(binary, scene, off_ppm, force_shadowdebug=False)
        w2, h2, on_px = render(binary, scene, on_ppm, force_shadowdebug=True)
        assert (w1, h1) == (w2, h2), f"resolution mismatch: {(w1,h1)} vs {(w2,h2)}"

        x0 = w2 - RES  # top-right corner, matching the test-only blit's own placement
        center = (x0 + 127, 127)
        edge = (x0 + 10, 10)

        edge_on = sample(w2, h2, on_px, *edge)
        edge_off = sample(w1, h1, off_px, *edge)
        center_on = sample(w2, h2, on_px, *center)

        def close(a, b, tol=10):
            return all(abs(x - y) <= tol for x, y in zip(a, b))

        assert close(edge_on, CLEAR), (
            f"corner edge should show the light-view clear color "
            f"({CLEAR}) with the hook on, got {edge_on}"
        )
        assert not close(edge_off, CLEAR), (
            f"corner edge should NOT show the light-view clear color "
            f"without the hook -- the pass should never run, got {edge_off} "
            f"(matches on-state, suggesting the hook has no effect)"
        )
        assert not close(center_on, CLEAR, tol=15), (
            f"corner center should show the rendered box (brighter than "
            f"the clear color, lit by a brightness=2.0 directional light), "
            f"got {center_on} -- the scene may not be rendering into the "
            f"migrated RenderTarget2D"
        )

        print(
            f"PASS: shadowdebug_test — edge off={edge_off} on={edge_on}; "
            f"center on={center_on}"
        )
    finally:
        if os.path.exists(off_ppm):
            os.remove(off_ppm)
        if os.path.exists(on_ppm):
            os.remove(on_ppm)


if __name__ == "__main__":
    main()
