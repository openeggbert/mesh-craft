#!/usr/bin/env python3
"""Verify MC3 Z-up export uses one explicit glTF Y-up conversion node."""
import json
import math
import os
import subprocess
import sys
import tempfile


def close(actual, expected, tolerance=1.0e-6):
    return abs(actual - expected) <= tolerance


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf-binary> <fixture.mc3.xml>", file=sys.stderr)
        return 2
    binary, fixture = sys.argv[1:]
    with tempfile.NamedTemporaryFile(suffix=".gltf", delete=False) as handle:
        output = handle.name
    try:
        result = subprocess.run([binary, fixture, output], capture_output=True, text=True)
        assert result.returncode == 0, (
            f"mc3togltf exited {result.returncode}:\nstdout={result.stdout}\nstderr={result.stderr}")
        with open(output, encoding="utf-8") as handle:
            gltf = json.load(handle)

        roots = gltf["scenes"][gltf.get("scene", 0)]["nodes"]
        assert len(roots) == 1, f"expected one coordinate root, got scene nodes {roots}"
        root = gltf["nodes"][roots[0]]
        assert root.get("name") == "MC3 right-handed Z-up to Y-up", "conversion root name missing"
        rotation = root.get("rotation")
        expected = -math.sqrt(0.5), math.sqrt(0.5)
        assert rotation and close(rotation[0], expected[0]) and close(rotation[1], 0.0) and \
            close(rotation[2], 0.0) and close(rotation[3], expected[1]), \
            f"expected -90 degree X conversion quaternion, got {rotation}"

        children = [gltf["nodes"][index].get("name") for index in root.get("children", [])]
        assert {"ZUpBox", "Spot", "Camera"}.issubset(children), (
            f"object/light/camera must all be coordinate-root children, got {children}")
        print("PASS: coordinate_system_export_test — Z-up conversion root contains object, light and camera")
        return 0
    finally:
        if os.path.exists(output):
            os.remove(output)
        binary_buffer = os.path.splitext(output)[0] + ".bin"
        if os.path.exists(binary_buffer):
            os.remove(binary_buffer)


if __name__ == "__main__":
    raise SystemExit(main())
