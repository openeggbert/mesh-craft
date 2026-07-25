#!/usr/bin/env python3
"""External SVG material texture must be visibly rasterized in the viewport."""
import os
import subprocess
import sys
import tempfile


def parse_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:2] == b"P6", "not a raw PPM (P6) file"
    i, values = 2, []
    while len(values) < 3:
        while data[i] in b" \t\r\n":
            i += 1
        end = i
        while data[end] not in b" \t\r\n":
            end += 1
        values.append(int(data[i:end]))
        i = end
    while data[i] in b" \t\r\n":
        i += 1
    return values[0], values[1], data[i:]


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <MeshCraft> <svg-material-scene>")
        sys.exit(1)
    binary, scene = sys.argv[1:]
    fd, screenshot = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_svg_")
    os.close(fd)
    try:
        command = [binary, scene, "--screenshot", screenshot]
        if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
            command = ["xvfb-run", "--auto-servernum"] + command
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
        width, height, pixels = parse_ppm(screenshot)
        red = sum(
            1 for y in range(0, height, 2) for x in range(0, width, 2)
            if (lambda c: c[0] > 120 and c[1] < 80 and c[2] < 80)
            (pixels[(y * width + x) * 3:(y * width + x) * 3 + 3])
        )
        assert red > 100, f"expected rasterized red SVG material pixels, found {red}"
        print(f"SVG viewport texture: PASS ({red} red samples)")
    finally:
        if os.path.exists(screenshot):
            os.remove(screenshot)


if __name__ == "__main__":
    main()
