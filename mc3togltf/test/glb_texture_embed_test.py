#!/usr/bin/env python3
"""STAB-0543: GLB export actually embeds real texture pixel data.

The existing texture_sampler_test.py deliberately uses nonexistent
texture files (to exercise the missing-texture-warning path, STAB-0420),
so no prior test proves that a *real, existing* texture actually gets
embedded into GLB output. GltfExporter.cpp:1109-1111 ties image
embedding to output format automatically (embedImagesNow = format ==
OutputFormat::GLB) -- this isn't a user-facing "Embed textures" setting
as the plan.md row's wording implies, but the underlying question ("does
GLB actually embed real texture bytes as bufferViews?") is real and,
until now, untested for a texture file that actually exists on disk.

Exports a box referencing test/textures/solid_green.png (a real,
committed 152-byte PNG) to .glb and confirms the resulting glTF JSON's
image entry is genuinely self-contained: tinygltf's embedImages=true
path (GltfExporter.cpp:1109-1111) turns out to encode images as base64
`data:image/png;base64,...` URIs rather than bufferView + BIN-chunk
references (a spec-legal, equally-embedded alternative this test
initially assumed was a bufferView, before checking actual output) --
decodes that payload and confirms it's byte-identical to the real
source PNG, not a placeholder or truncated stub.

Usage: glb_texture_embed_test.py <mc3togltf> <glb_texture_embed.mc3.xml>
"""
import base64
import json
import os
import struct
import subprocess
import sys
import tempfile

PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
SOURCE_PNG = os.path.join(os.path.dirname(__file__), "..", "..", "test", "textures", "solid_green.png")


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <glb_texture_embed.mc3.xml>")
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
        assert uri.startswith("data:image/png;base64,"), (
            f"Expected the GLB image to be a self-contained base64 data URI "
            f"(tinygltf's embedImages=true encoding, GltfExporter.cpp:1109-1111), "
            f"got: {img}"
        )
        embedded_bytes = base64.b64decode(uri.split(",", 1)[1])

        assert embedded_bytes[:8] == PNG_MAGIC, (
            f"Expected the embedded data URI's decoded bytes to start with the PNG "
            f"magic header, got: {embedded_bytes[:8]!r}"
        )

        with open(SOURCE_PNG, "rb") as f:
            source_bytes = f.read()
        assert embedded_bytes == source_bytes, (
            f"Expected the embedded image bytes to be byte-identical to the source "
            f"file ({len(source_bytes)} bytes), got {len(embedded_bytes)} bytes "
            f"embedded — texture data may be corrupted or truncated during GLB export"
        )

        print(f"PASS: GLB embeds real texture data — {len(embedded_bytes)}-byte base64 "
              f"data URI, byte-identical to source PNG")
