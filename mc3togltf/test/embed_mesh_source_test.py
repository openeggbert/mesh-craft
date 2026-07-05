#!/usr/bin/env python3
"""STAB-0549: embed: meshSource during GLB export -- documented limitation,
not a bug, verified to degrade gracefully (warn + empty node), not crash.

buildMesh() (GltfExporter.cpp:417-424) treats obj.meshSource unconditionally
as a literal OBJ file path -- there is no special-case for the "embed:<id>"
form that Mc3XmlParser/Writer already parse/serialize/round-trip correctly
(mc3/test/roundtrip_test.cpp's testEmbedGltf). Actually resolving an embed
reference to real mesh data would mean parsing the referenced external .glb
(or decoding inline base64 GLB data) and merging its meshes into the
exported model -- a real new feature (this row's own wording anticipates
that: "(STAB-0194 extended) or marked as limitation"), not a quick fix, so
it is deliberately NOT implemented here.

This test exists to lock in the current, safe degradation: export must
still exit 0, print a "Warning:" (not crash), and produce a valid GLB with
an empty (meshless) node for the embed-referencing object -- so a future
change to buildMesh() can't silently regress this into a crash without a
test noticing.

Usage: embed_mesh_source_test.py <mc3togltf> <embed_mesh_source.mc3.xml>
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
        print(f"Usage: {sys.argv[0]} <mc3togltf> <embed_mesh_source.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out_glb = os.path.join(tmpdir, "out.glb")
        r = run([mc3togltf, xml_path, out_glb])
        combined = r.stdout + r.stderr

        assert r.returncode == 0, (
            f"Expected export to exit 0 despite the unresolved embed: reference "
            f"(known limitation, must degrade gracefully, not fail the whole "
            f"export), got returncode={r.returncode}\nstdout={r.stdout}\nstderr={r.stderr}"
        )
        assert "warning" in combined.lower(), (
            f"Expected a Warning: for the unresolved embed: mesh source, got:\n{combined}"
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
        assert "EmbeddedTree" in nmap, f"Expected node 'EmbeddedTree' in output, got: {sorted(nmap.keys())}"
        assert nmap["EmbeddedTree"].get("mesh") is None, (
            "Expected the embed:-referencing node to have no mesh (known "
            "limitation: embed: is not resolved to real geometry) -- if this "
            "now has a mesh, the limitation has been fixed and this test's "
            "docstring/plan.md row should be updated to reflect that"
        )

        print("PASS: embed: meshSource degrades gracefully (warns, exits 0, empty node) — documented limitation confirmed")
