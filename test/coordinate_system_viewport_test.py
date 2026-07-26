#!/usr/bin/env python3
"""Real viewport regression for the MC3 right-handed Z-up conversion.

Usage: coordinate_system_viewport_test.py <MeshCraft-binary> <fixture.mc3.xml>
"""
import os
import subprocess
import sys
import tempfile


def parse_ppm(path):
    with open(path, "rb") as handle:
        data = handle.read()
    assert data[:2] == b"P6", "not a raw PPM (P6) file"
    pos, values = 2, []
    while len(values) < 3:
        while data[pos] in b" \t\n\r":
            pos += 1
        start = pos
        while data[pos] not in b" \t\n\r":
            pos += 1
        values.append(int(data[start:pos]))
    width, height, _max_value = values
    return width, height, data[pos + 1:]


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <MeshCraft-binary> <fixture.mc3.xml>", file=sys.stderr)
        return 2
    binary, fixture = sys.argv[1:]
    fd, output = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_zup_")
    os.close(fd)
    try:
        command = [binary, fixture, "--screenshot", output]
        if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
            command = ["xvfb-run", "--auto-servernum"] + command
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode == 0, (
            f"MeshCraft exited {result.returncode}:\nstdout={result.stdout}\nstderr={result.stderr}")
        width, height, pixels = parse_ppm(output)
        yellow = 0
        for y in range(height // 2 - 45, height // 2 + 45, 2):
            for x in range(width // 2 - 45, width // 2 + 45, 2):
                red, green, blue = pixels[(y * width + x) * 3:(y * width + x + 1) * 3]
                if red > 130 and green > 90 and blue < 110 and red > blue * 1.5:
                    yellow += 1
        assert yellow > 120, (
            f"expected the converted Z-up yellow box at screen center, found only {yellow} samples")
        print(f"PASS: coordinate_system_viewport_test — {yellow} centered yellow samples")
        return 0
    finally:
        if os.path.exists(output):
            os.remove(output)


if __name__ == "__main__":
    raise SystemExit(main())
