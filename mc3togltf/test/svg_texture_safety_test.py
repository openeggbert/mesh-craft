#!/usr/bin/env python3
"""Malformed SVG must warn safely; enormous SVG must obey the 2048px cap."""
import json
import os
import struct
import subprocess
import sys
import tempfile


def run(command):
    return subprocess.run(command, capture_output=True, text=True)


def image_path(gltf_path, model):
    material = model["materials"][0]
    texture = model["textures"][material["pbrMetallicRoughness"]["baseColorTexture"]["index"]]
    return os.path.join(os.path.dirname(gltf_path), model["images"][texture["source"]]["uri"])


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <invalid-scene> <large-scene>")
        sys.exit(1)
    binary, invalid_scene, large_scene = sys.argv[1:]
    with tempfile.TemporaryDirectory() as directory:
        invalid_out = os.path.join(directory, "invalid.gltf")
        invalid = run([binary, invalid_scene, invalid_out])
        assert invalid.returncode == 0, invalid.stderr
        assert "SVG texture 'bad' skipped" in invalid.stderr, invalid.stderr
        with open(invalid_out, encoding="utf-8") as f:
            invalid_gltf = json.load(f)
        assert "baseColorTexture" not in invalid_gltf["materials"][0]["pbrMetallicRoughness"], \
            "invalid SVG must not leave a dangling glTF texture reference"

        large_out = os.path.join(directory, "large.gltf")
        large = run([binary, large_scene, large_out])
        assert large.returncode == 0, large.stderr
        with open(large_out, encoding="utf-8") as f:
            large_gltf = json.load(f)
        png = image_path(large_out, large_gltf)
        with open(png, "rb") as f:
            header = f.read(24)
        assert header.startswith(b"\x89PNG\r\n\x1a\n"), "generated SVG texture is not PNG"
        width, height = struct.unpack(">II", header[16:24])
        assert (width, height) == (2048, 1024), (width, height)
        print("SVG safety: PASS (invalid warning and 2048px cap)")
