#!/usr/bin/env python3
"""SYS-W14-32: default, box, and sphere UV projection must differ visibly."""

import os
import subprocess
import sys
import tempfile


def parse_ppm(path):
    with open(path, "rb") as handle:
        data = handle.read()
    assert data[:2] == b"P6", "not a raw PPM (P6) file"
    position, values = 2, []
    while len(values) < 3:
        while data[position] in b" \t\r\n":
            position += 1
        end = position
        while data[end] not in b" \t\r\n":
            end += 1
        values.append(int(data[position:end]))
        position = end
    while data[position] in b" \t\r\n":
        position += 1
    return values[0], values[1], data[position:]


def screenshot(binary, scene, output):
    command = [binary, scene, "--screenshot", output]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        command = ["xvfb-run", "--auto-servernum"] + command
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return parse_ppm(output)


def changed_samples(first, second):
    width, height, first_pixels = first
    other_width, other_height, second_pixels = second
    assert (width, height) == (other_width, other_height), "screenshot dimensions changed"
    changed = 0
    # Restrict to the central object region so an unrelated UI/edge pixel
    # cannot satisfy the assertion. The logo's high-contrast regions make a
    # missing UV remap produce exactly zero meaningful differences here.
    for y in range(height // 2 - 140, height // 2 + 140, 2):
        for x in range(width // 2 - 180, width // 2 + 180, 2):
            index = (y * width + x) * 3
            distance = sum(abs(first_pixels[index + c] - second_pixels[index + c])
                           for c in range(3))
            if distance > 60:
                changed += 1
    return changed


if len(sys.argv) != 5:
    print(f"Usage: {sys.argv[0]} <MeshCraft> <plain> <box> <sphere>", file=sys.stderr)
    raise SystemExit(2)


binary, plain_scene, box_scene, sphere_scene = sys.argv[1:]
paths = []
try:
    for suffix in ("plain", "box", "sphere"):
        fd, path = tempfile.mkstemp(suffix=".ppm", prefix=f"meshcraft_uv_{suffix}_")
        os.close(fd)
        paths.append(path)
    plain = screenshot(binary, plain_scene, paths[0])
    box = screenshot(binary, box_scene, paths[1])
    sphere = screenshot(binary, sphere_scene, paths[2])
    box_changed = changed_samples(plain, box)
    sphere_changed = changed_samples(plain, sphere)
    assert box_changed > 300, f"box projection changed only {box_changed} object samples"
    assert sphere_changed > 300, f"sphere projection changed only {sphere_changed} object samples"
    print(f"PASS: UV viewport projection differs from default (box={box_changed}, sphere={sphere_changed})")
finally:
    for path in paths:
        if os.path.exists(path):
            os.remove(path)
