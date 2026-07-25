#!/usr/bin/env python3
"""
Regression test for AUD-092: SSAO has a headless test hook but needs a
pixel-level assertion that the CNA depth-pre-pass/AO/blur/composite pipeline
actually affects the viewport.

light_shading.mc3.xml is deliberately reused: its centered, camera-facing red
sphere has a stable depth discontinuity against the dark background. Rendering
it with MESHCRAFT_TEST_FORCE_SSAO=1 must darken a substantial set of sphere
pixels relative to the byte-identical no-SSAO baseline. This is stronger than a
no-crash smoke test: a broken depth pre-pass, shader, render target, or
multiplicative composite leaves the two screenshots unchanged.

Usage: ssao_test.py <MeshCraft-binary> <light_shading.mc3.xml>
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
    values = []
    while len(values) < 3:
        while data[i] in b" \t\n\r":
            i += 1
        start = i
        while data[i] not in b" \t\n\r":
            i += 1
        values.append(int(data[start:i]))
    width, height, _max_value = values
    return width, height, data[i + 1:]


def render(binary, scene, output, force_ssao):
    command = [binary, scene, "--screenshot", output]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        command = ["xvfb-run", "--auto-servernum"] + command
    environment = dict(os.environ)
    if force_ssao:
        environment["MESHCRAFT_TEST_FORCE_SSAO"] = "1"
    result = subprocess.run(command, capture_output=True, text=True, env=environment)
    assert result.returncode == 0, (
        f"MeshCraft exited {result.returncode}:\n"
        f"stdout={result.stdout}\nstderr={result.stderr}"
    )
    assert os.path.getsize(output) > 0, "screenshot PPM is empty"
    return parse_ppm(output)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <binary> <scene.mc3.xml>")
        return 1
    binary, scene = sys.argv[1], sys.argv[2]

    fd_off, path_off = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_ssao_off_")
    fd_on, path_on = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_ssao_on_")
    os.close(fd_off)
    os.close(fd_on)
    try:
        width_off, height_off, pixels_off = render(binary, scene, path_off, False)
        width_on, height_on, pixels_on = render(binary, scene, path_on, True)
        assert (width_off, height_off) == (width_on, height_on), (
            f"resolution mismatch: off={(width_off, height_off)} "
            f"on={(width_on, height_on)}"
        )

        darkened_pixels = 0
        red_darkening = 0
        max_red_darkening = 0
        for offset in range(0, len(pixels_off), 3):
            red_delta = pixels_off[offset] - pixels_on[offset]
            if red_delta > 0:
                darkened_pixels += 1
                red_darkening += red_delta
                max_red_darkening = max(max_red_darkening, red_delta)

        assert darkened_pixels > 1_000, (
            f"SSAO should darken the depth-discontinuity edge, found only "
            f"{darkened_pixels} red-channel pixels (total={red_darkening}, "
            f"max={max_red_darkening})"
        )
        assert red_darkening > 2_000 and max_red_darkening >= 2, (
            f"SSAO darkening is too weak: total={red_darkening}, "
            f"max per pixel={max_red_darkening}; the depth/AO/composite path "
            f"may not be active"
        )

        print(
            f"PASS: ssao_test — darkened_pixels={darkened_pixels}; "
            f"red_darkening={red_darkening}; max_red_darkening={max_red_darkening}"
        )
    finally:
        for path in (path_off, path_on):
            if os.path.exists(path):
                os.remove(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
