#!/usr/bin/env python3
"""
STAB-0200: mc3togltf must handle a completely empty scene (<objects/>,
zero objects) without crashing -- exit 0, produce a structurally valid
(if geometry-empty) GLB, with no nodes/meshes.

Reuses test/empty_scene.mc3.xml (originally added for STAB-0498 to test
the editor's handling of an empty scene) -- this is the first test that
exercises it through mc3togltf specifically.
"""
import json
import os
import struct
import subprocess
import sys
import tempfile

GLTF_MAGIC = 0x46546C67


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <empty_scene.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out_glb = os.path.join(tmpdir, "out.glb")
        r = run([mc3togltf, xml_path, out_glb])
        assert r.returncode == 0, (
            f"Expected mc3togltf to exit 0 on a zero-object scene, "
            f"got returncode={r.returncode}\nstdout={r.stdout}\nstderr={r.stderr}"
        )
        assert os.path.exists(out_glb) and os.path.getsize(out_glb) > 0, \
            "Output GLB for an empty scene is missing or empty"

        with open(out_glb, "rb") as f:
            data = f.read()
        magic, _version, _length = struct.unpack_from("<III", data, 0)
        assert magic == GLTF_MAGIC, f"Bad GLB magic: {magic:#x}"

        off = 12
        json_chunk_len, _json_chunk_type = struct.unpack_from("<II", data, off)
        gltf = json.loads(data[off + 8: off + 8 + json_chunk_len])

        assert gltf.get("asset", {}).get("version") == "2.0", (
            f"Expected asset.version == '2.0', got {gltf.get('asset')}"
        )
        assert len(gltf.get("nodes", [])) == 0, (
            f"Expected 0 nodes for an empty scene, got {len(gltf.get('nodes', []))}"
        )
        assert len(gltf.get("meshes", [])) == 0, (
            f"Expected 0 meshes for an empty scene, got {len(gltf.get('meshes', []))}"
        )

        print(f"Empty scene (0 objects): exit 0, valid GLB, 0 nodes/meshes — PASS")

    print("\nEmpty scene export test: PASS")
