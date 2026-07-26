#!/usr/bin/env python3
"""SYS-W14-33: point range and spotlight cone must shade live geometry."""

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


def render(binary, scene, output):
    environment = os.environ.copy()
    # Dedicated light-gizmo fixtures cover overlays. This test must measure
    # only shaded mesh pixels, otherwise the bright red/green editor glyphs
    # could satisfy the assertion even if the ShaderEffect were bypassed.
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
            red, green, blue = pixels[offset:offset + 3]
            values = (red, green, blue)
            primary = values[channel]
            others = [values[index] for index in range(3) if index != channel]
            if primary > 120 and primary > others[0] + 80 and primary > others[1] + 80:
                count += 1
    return count


if len(sys.argv) != 4:
    print(f"Usage: {sys.argv[0]} <MeshCraft> <point-scene> <spot-scene>", file=sys.stderr)
    raise SystemExit(2)


binary, point_scene, spot_scene = sys.argv[1:]
paths = []
try:
    for kind in ("point", "spot"):
        fd, path = tempfile.mkstemp(suffix=".ppm", prefix=f"meshcraft_{kind}_light_")
        os.close(fd)
        paths.append(path)
    point = render(binary, point_scene, paths[0])
    spot = render(binary, spot_scene, paths[1])

    point_in_range = dominant_samples(point, 0, left_half=True)
    point_out_of_range = dominant_samples(point, 0, left_half=False)
    assert point_in_range > 200, (
        f"point light illuminated only {point_in_range} red object samples; "
        "the live ShaderEffect point-light path appears inactive"
    )
    assert point_out_of_range < 10, (
        f"point light leaked into {point_out_of_range} red samples outside its authored range"
    )

    spot_in_cone = dominant_samples(spot, 1, left_half=True)
    spot_outside_cone = dominant_samples(spot, 1, left_half=False)
    assert spot_in_cone > 200, (
        f"spot light illuminated only {spot_in_cone} green object samples; "
        "the live ShaderEffect spotlight path appears inactive"
    )
    assert spot_outside_cone < 10, (
        f"spot light leaked into {spot_outside_cone} green samples outside its outer cone"
    )
    print("PASS: point/spot live lighting ("
          f"point in/out={point_in_range}/{point_out_of_range}, "
          f"spot in/out={spot_in_cone}/{spot_outside_cone})")
finally:
    for path in paths:
        if os.path.exists(path):
            os.remove(path)
