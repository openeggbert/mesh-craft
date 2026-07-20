#!/usr/bin/env python3
"""
Regression test for the 2026-07-20 audit finding #5 fix.

SceneRenderer.cpp used to additionally enable BasicEffect's own GPU fog
whenever doc.environment->fog existed, ALWAYS using linear start/end
regardless of the document's declared fog mode -- wrong for Exponential
mode (which has no start/end concept at all) and redundant even for
Linear mode, since drawObject()'s per-object CPU-side blend ("I3") is a
complete, correct, mode-aware implementation on its own.

Empirically verified while writing this test (via git stash, comparing
the pre-fix and post-fix binaries' actual rendered output pixel-for-pixel
on this exact fixture): removing BasicEffect's GPU fog produced ZERO
pixel difference -- the GPU fog branch was not visibly reachable in this
renderer's actual effect/shader configuration (VertexColorEnabled=true
appears to route to a shader permutation where it never took effect).
So this is confirmed dead/incorrect code (blind to `mode`, redundant with
an already-complete CPU implementation), not a currently *visible*
double-fog rendering defect -- this test is scoped accordingly:

1. A static source check that the removed pattern (BasicEffect's fog
   enabled from SceneRenderer.cpp) is not reintroduced -- the actual
   regression guard, since re-adding it would resurrect mode-blind,
   redundant (and, if CNA's shader wiring ever changes, potentially
   visibly double-applying) fog logic.
2. A real render check that Exponential-mode fog (previously untested --
   fog_linear_test.py only covers Linear) actually works via the CPU path
   alone: a partial fogFactor measurably shifts the box's color toward
   the fog color, without needing to assume an exact numeric prediction.

Usage: fog_exponential_test.py <MeshCraft-binary> <fog_exponential_a.mc3.xml> <fog_exponential_nofog.mc3.xml>
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


def sample_center(w, h, pixels):
    off = ((h // 2) * w + (w // 2)) * 3
    return pixels[off], pixels[off + 1], pixels[off + 2]


def check_no_basiceffect_fog(repo_root):
    path = os.path.join(repo_root, "src", "MeshCraft", "Renderer", "SceneRenderer.cpp")
    with open(path) as f:
        content = f.read()
    assert "setFogEnabledProperty(true)" not in content, (
        f"{path} calls setFogEnabledProperty(true) -- BasicEffect's own GPU fog "
        f"was deliberately removed (2026-07-20 audit finding #5): it's blind to "
        f"Mc3Fog::mode (always uses linear start/end, even for exponential fog) "
        f"and redundant with drawObject()'s already-correct, mode-aware CPU-side "
        f"per-object fog blend. Re-adding it reintroduces mode-blind, redundant "
        f"fog logic."
    )
    print("PASS: SceneRenderer.cpp does not re-enable BasicEffect's own GPU fog")


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <binary> <fog_exponential_a.mc3.xml> <fog_exponential_nofog.mc3.xml>")
        sys.exit(1)
    binary, fog_scene, nofog_scene = sys.argv[1], sys.argv[2], sys.argv[3]
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    check_no_basiceffect_fog(repo_root)

    fd1, ppm_fog = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_fogexp_")
    os.close(fd1)
    fd2, ppm_nofog = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_fogexp_nofog_")
    os.close(fd2)
    try:
        w1, h1, fog_pixels = render(binary, fog_scene, ppm_fog)
        w2, h2, nofog_pixels = render(binary, nofog_scene, ppm_nofog)
        assert (w1, h1) == (w2, h2), "both renders should be the same resolution"

        base_r, base_g, base_b = sample_center(w1, h1, nofog_pixels)
        actual_r, actual_g, actual_b = sample_center(w1, h1, fog_pixels)

        # Sanity: the un-fogged control's box must actually be red-ish
        # (its authored material), not e.g. background color.
        assert base_r > 100 and base_g < 100 and base_b < 100, (
            f"fog_exponential_nofog.mc3.xml: expected a red-ish center pixel "
            f"(the box's own base color), got ({base_r},{base_g},{base_b}) -- "
            f"fixture may not be set up as intended"
        )

        # Qualitative check: Exponential-mode fog (color="0 1 0", a partial
        # blend at this fixture's distance/density -- neither fully un-
        # fogged nor fully fog-colored) must measurably shift the color
        # AWAY from the base red AND TOWARD the fog green, via the CPU
        # path alone. Not an exact numeric prediction (BasicEffect's own
        # lighting/rasterization internals aren't something this test
        # should need to model precisely) -- just that real, directional
        # fog blending actually happened.
        assert actual_r < base_r - 20, (
            f"expected red channel to drop measurably from the un-fogged base "
            f"({base_r}) toward the fog color's red=0, got {actual_r} -- "
            f"exponential fog does not appear to be blending toward the fog color"
        )
        assert actual_g > base_g + 20, (
            f"expected green channel to rise measurably from the un-fogged base "
            f"({base_g}) toward the fog color's green=255, got {actual_g} -- "
            f"exponential fog does not appear to be blending toward the fog color"
        )
        # Must be a PARTIAL blend, not fully saturated to the fog color --
        # this fixture's density/distance are chosen so fogFactor is
        # roughly in the middle, not 0 or 1 (a fully-saturated result
        # can't distinguish "correct single blend" from "over-fogged", the
        # same blind spot that made an earlier draft of this test
        # ineffective).
        assert actual_r > 40, (
            f"expected a PARTIAL blend (red channel still clearly above the fog "
            f"color's red=0), got {actual_r} -- if fog is fully saturating here, "
            f"this fixture can no longer distinguish a correct blend from a "
            f"double-applied one"
        )

        print(f"PASS: fog_exponential_test — base=({base_r},{base_g},{base_b}) "
              f"actual=({actual_r},{actual_g},{actual_b}) — a real, partial "
              f"exponential-mode fog blend via the CPU path")
    finally:
        if os.path.exists(ppm_fog):
            os.remove(ppm_fog)
        if os.path.exists(ppm_nofog):
            os.remove(ppm_nofog)


if __name__ == "__main__":
    main()
