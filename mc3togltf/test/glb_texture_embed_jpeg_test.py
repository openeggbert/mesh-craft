#!/usr/bin/env python3
"""STAB-0676: GLB texture embedding detects the real image format instead
of hardcoding image/png for every embedded texture.

Sibling to glb_texture_embed_test.py, which only exercises a PNG source
and would not have caught this: img.mimeType was previously hardcoded to
"image/png" unconditionally (GltfExporter.cpp), so an embedded JPEG got
mistagged, which spec-compliant loaders fail to decode (JPEG bytes parsed
as PNG). This test exports a real JPEG texture and confirms the embedded
data URI is correctly tagged image/jpeg and byte-identical to the source.

Usage: glb_texture_embed_jpeg_test.py <mc3togltf> <glb_texture_embed_jpeg.mc3.xml>
"""
import base64
import json
import os
import struct
import subprocess
import sys
import tempfile

JPEG_MAGIC = b"\xff\xd8\xff"
SOURCE_JPEG = os.path.join(os.path.dirname(__file__), "..", "..", "test", "textures", "jpeg_test.jpg")


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <glb_texture_embed_jpeg.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out_glb = os.path.join(tmpdir, "out.glb")
        r = run([mc3togltf, xml_path, out_glb])
        assert r.returncode == 0, (
            f"Export failed (returncode={r.returncode}):\nstdout={r.stdout}\nstderr={r.stderr}"
        )
        assert os.path.exists(out_glb) and os.path.getsize(out_glb) > 0, "Output GLB is missing or empty"

        with open(out_glb, "rb") as f:
            data = f.read()

        magic, version, length = struct.unpack_from("<III", data, 0)
        assert magic == 0x46546C67, f"Bad glTF magic: {magic:#x}"

        off = 12
        json_chunk_len, json_chunk_type = struct.unpack_from("<II", data, off)
        json_bytes = data[off + 8: off + 8 + json_chunk_len]
        gltf = json.loads(json_bytes)

        images = gltf.get("images", [])
        assert images, "Expected at least one image in the exported glTF"
        img = images[0]

        uri = img.get("uri", "")
        assert uri.startswith("data:image/jpeg;base64,"), (
            f"Expected the embedded JPEG to be tagged image/jpeg (STAB-0676), "
            f"got: {img}"
        )
        embedded_bytes = base64.b64decode(uri.split(",", 1)[1])

        assert embedded_bytes[:3] == JPEG_MAGIC, (
            f"Expected the embedded data URI's decoded bytes to start with the JPEG "
            f"magic header, got: {embedded_bytes[:3]!r}"
        )

        with open(SOURCE_JPEG, "rb") as f:
            source_bytes = f.read()
        assert embedded_bytes == source_bytes, (
            f"Expected the embedded image bytes to be byte-identical to the source "
            f"file ({len(source_bytes)} bytes), got {len(embedded_bytes)} bytes embedded"
        )

        print(f"PASS: GLB embeds real JPEG texture data as image/jpeg — "
              f"{len(embedded_bytes)}-byte base64 data URI, byte-identical to source JPEG")
