#!/usr/bin/env python3
"""STAB-0548: definitions pulled in via <include> get real geometry on GLB export.

Instance resolution in GltfExporter.cpp:552-587 looks definitions up via
`ctx.definitions` (bound to doc.definitions), which Mc3Document::loadFromFile()
populates uniformly at parse time regardless of whether a definition came
from the scene's own <definitions> block or was pulled in via <include> --
so no special-casing should be needed at export time. This was previously
untested by mc3togltf's own test suite (only the CNA-free
mc3/roundtrip_test.cpp's testInclude() exercised <include>, and only for
XML round-tripping, never glTF export).

test/scene_with_include.mc3.xml <include>s test/mc3_library.mc3.xml and
instantiates its "pillar" and "crate" definitions twice/once respectively.
This test exports it to .glb and confirms: zero "unknown definition"
warnings, and that each Instance node actually has non-empty mesh geometry
(not a silently-empty node).

Usage: include_export_test.py <mc3togltf> <scene_with_include.mc3.xml>
"""
import json
import struct
import subprocess
import sys
import tempfile
import os


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <scene_with_include.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out_glb = os.path.join(tmpdir, "out.glb")
        r = run([mc3togltf, xml_path, out_glb])
        assert r.returncode == 0, f"Export failed (returncode={r.returncode}):\nstdout={r.stdout}\nstderr={r.stderr}"

        combined = r.stdout + r.stderr
        assert "unknown definition" not in combined.lower(), (
            f"Expected no 'unknown definition' warnings for instances of an "
            f"included definition, got:\n{combined}"
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

        nmap = {n.get("name", ""): n for n in gltf.get("nodes", [])}
        meshes = gltf.get("meshes", [])
        accs = gltf.get("accessors", [])

        for name in ("Pillar1", "Pillar2", "Crate1"):
            assert name in nmap, f"Expected node '{name}' in output, got: {sorted(nmap.keys())}"
            mesh_idx = nmap[name].get("mesh")
            assert mesh_idx is not None, (
                f"Expected node '{name}' (an Instance of an <include>d definition) to "
                f"have mesh geometry, got no 'mesh' key on its node -- included "
                f"definitions may not be resolving during GLB export"
            )
            pos_idx = meshes[mesh_idx]["primitives"][0]["attributes"]["POSITION"]
            vcount = accs[pos_idx]["count"]
            assert vcount > 0, f"Expected node '{name}' to have real vertex data, got {vcount} vertices"
            print(f"{name}: mesh {mesh_idx}, {vcount} vertices — PASS")

        # Pillar1 and Pillar2 share the same definition+material+deform, so the
        # exporter's instance-mesh cache should reuse one glTF mesh for both.
        assert nmap["Pillar1"]["mesh"] == nmap["Pillar2"]["mesh"], (
            "Expected Pillar1 and Pillar2 (same included definition, no material "
            "override) to share one glTF mesh via the instance-mesh cache"
        )

        print("\nInclude-during-GLB-export test: PASS")
