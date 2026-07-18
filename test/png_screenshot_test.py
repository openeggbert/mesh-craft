#!/usr/bin/env python3
"""
SYS-W14-03: --screenshot's own --help text has always promised "Render
scene to PNG and exit", but saveScreenshot() unconditionally wrote raw PPM
(P6) bytes regardless of the requested path's extension -- a ".png" path
got PPM data with a misleading extension, not a decodable PNG. Now an
explicit ".png" path gets a real PNG (stbi_write_png(), reusing
mc3togltf_lib's already-compiled stb_image_write.h implementation); every
other extension (in particular every ".ppm" path this test suite's other
screenshot tests use) is unchanged.

Verifies both sides of that fix with one process each: a ".png" path
decodes as a real PNG (magic bytes + IHDR width/height/bit-depth/color-
type match the viewport), and a ".ppm" path is still the exact same raw
PPM (P6) format as before (regression guard for every other test that
depends on that).

Usage: png_screenshot_test.py <MeshCraft-binary> <scene.mc3.xml>
"""
import os
import struct
import subprocess
import sys
import tempfile

PNG_MAGIC = b"\x89PNG\r\n\x1a\n"


def run_screenshot(binary, scene, path):
    cmd = [binary, scene, "--screenshot", path]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        cmd = ["xvfb-run", "--auto-servernum"] + cmd
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, (
        f"MeshCraft exited {r.returncode}:\nstdout={r.stdout}\nstderr={r.stderr}"
    )
    return r


def parse_png_ihdr(data):
    assert data[:8] == PNG_MAGIC, "not a PNG file (bad signature)"
    # IHDR is always the first chunk: 4-byte length, "IHDR", then 13 bytes
    # of fields, then a 4-byte CRC.
    length = struct.unpack(">I", data[8:12])[0]
    assert length == 13, f"IHDR chunk has unexpected length {length}"
    assert data[12:16] == b"IHDR", "first chunk is not IHDR"
    width, height, bit_depth, color_type = struct.unpack(">IIBB", data[16:26])
    return width, height, bit_depth, color_type


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <binary> <scene>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    # ".png" path: must decode as a real PNG.
    fd, png_path = tempfile.mkstemp(suffix=".png", prefix="meshcraft_pngshot_")
    os.close(fd)
    try:
        run_screenshot(binary, scene, png_path)
        assert os.path.getsize(png_path) > 0, "PNG screenshot is empty"
        with open(png_path, "rb") as f:
            data = f.read()
        assert data[:8] == PNG_MAGIC, (
            f"--screenshot *.png did not write a real PNG (bad magic bytes: {data[:8]!r})"
        )
        width, height, bit_depth, color_type = parse_png_ihdr(data)
        assert width > 0 and height > 0, f"PNG IHDR reports a zero dimension ({width}x{height})"
        assert bit_depth == 8, f"expected 8-bit depth, got {bit_depth}"
        assert color_type == 6, f"expected color type 6 (RGBA), got {color_type}"
        print(f"PASS: --screenshot *.png writes a real, decodable PNG "
              f"({width}x{height}, {bit_depth}-bit RGBA)")
    finally:
        if os.path.exists(png_path):
            os.unlink(png_path)

    # ".ppm" path: must be completely unaffected by the PNG change (every
    # other test in this suite depends on this staying raw PPM).
    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_pngshot_ppm_")
    os.close(fd)
    try:
        run_screenshot(binary, scene, ppm_path)
        with open(ppm_path, "rb") as f:
            data = f.read()
        assert data[:2] == b"P6", (
            f"--screenshot *.ppm regressed away from raw PPM (P6) -- got {data[:2]!r}"
        )
        print("PASS: --screenshot *.ppm is unchanged (still raw PPM/P6)")
    finally:
        if os.path.exists(ppm_path):
            os.unlink(ppm_path)


if __name__ == "__main__":
    main()
