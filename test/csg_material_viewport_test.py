#!/usr/bin/env python3
"""SYS-W14-34: live CSG material ranges and root override pixel coverage."""

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
    environment = os.environ.copy()
    environment["MESHCRAFT_TEST_HIDE_SCENE_GIZMOS"] = "1"
    command = [binary, scene, "--screenshot", output]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        command = ["xvfb-run", "--auto-servernum"] + command
    result = subprocess.run(command, capture_output=True, text=True, env=environment)
    assert result.returncode == 0, result.stdout + result.stderr
    return parse_ppm(output)


def dominant_samples(image, channel, left_half):
    width, height, pixels = image
    x0, x1 = (width // 10, width // 2 - width // 25) if left_half else \
             (width // 2 + width // 25, width - width // 10)
    count = 0
    for y in range(height // 4, height * 3 // 4, 2):
        for x in range(x0, x1, 2):
            offset = (y * width + x) * 3
            values = pixels[offset:offset + 3]
            primary = values[channel]
            others = [values[index] for index in range(3) if index != channel]
            if primary > 120 and primary > others[0] + 80 and primary > others[1] + 80:
                count += 1
    return count


if len(sys.argv) != 4:
    print(f"Usage: {sys.argv[0]} <MeshCraft> <child-ranges> <root-override>", file=sys.stderr)
    raise SystemExit(2)


binary, child_scene, root_scene = sys.argv[1:]
paths = []
try:
    for suffix in ("child_ranges", "root_override"):
        descriptor, path = tempfile.mkstemp(suffix=".ppm", prefix=f"meshcraft_csg_{suffix}_")
        os.close(descriptor)
        paths.append(path)
    child = screenshot(binary, child_scene, paths[0])
    root = screenshot(binary, root_scene, paths[1])

    child_red = dominant_samples(child, 0, left_half=True)
    child_blue = dominant_samples(child, 2, left_half=False)
    assert child_red > 200 and child_blue > 200, (
        f"CSG child material ranges are inactive (red/blue={child_red}/{child_blue})")

    root_green_left = dominant_samples(root, 1, left_half=True)
    root_green_right = dominant_samples(root, 1, left_half=False)
    root_red = dominant_samples(root, 0, left_half=True)
    root_blue = dominant_samples(root, 2, left_half=False)
    assert root_green_left > 200 and root_green_right > 200, (
        f"CSG root override did not color both ranges green ({root_green_left}/{root_green_right})")
    assert root_red < 10 and root_blue < 10, (
        f"CSG root override leaked child colors (red/blue={root_red}/{root_blue})")
    print("PASS: CSG child material ranges and root override "
          f"(child red/blue={child_red}/{child_blue}, root green={root_green_left}/{root_green_right})")
finally:
    for path in paths:
        if os.path.exists(path):
            os.remove(path)
