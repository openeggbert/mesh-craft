#!/usr/bin/env python3
"""
STAB-0630 + STAB-0165 + AUD-002: untrusted / missing OBJ file robustness.

Feeds mc3togltf a scene with five <mesh> nodes referencing OBJ files:
  - a valid tetrahedron
  - an OBJ with an out-of-range negative (relative) vertex index
  - an OBJ with a coordinate that overflows to infinity
  - a meshSource that does not exist on disk at all (STAB-0165)
  - a triangle face (tinyobjloader's fast triangulate=true path, which
    bypasses its own out-of-range guards) referencing a vertex index far
    beyond the vertex count (AUD-002) -- an out-of-bounds read on
    attrib.vertices before the loadObjMesh() bounds check this fixture
    regression-guards

Malformed/missing OBJs must not crash the exporter: mc3togltf must exit 0,
print a "Warning:" for each bad file (STAB-0165: the missing-file warning
must name the actual file it tried to open), still export the valid mesh's
geometry, and never emit non-finite floats into the glTF (which tinygltf
represents in the JSON accessor min/max as invalid `null` entries — a spec
violation).
"""
import json
import os
import struct
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <obj_robustness.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out_glb = os.path.join(tmpdir, "out.glb")

        r = run([mc3togltf, xml_path, out_glb])
        assert r.returncode == 0, (
            "Expected mc3togltf to exit 0 (malformed meshes skipped, not "
            f"crashed), got returncode={r.returncode}\n"
            f"stdout: {r.stdout}\nstderr: {r.stderr}"
        )

        combined = r.stdout + r.stderr
        assert combined.count("Warning:") >= 4, (
            f"Expected a Warning: for each of the 4 bad OBJ references (negative "
            f"index, infinite coord, missing file, out-of-range positive index), "
            f"got:\n{combined}"
        )
        assert "non-finite" in combined, (
            f"Expected the infinite-coordinate OBJ to be rejected with a "
            f"'non-finite' error, got:\n{combined}"
        )
        # STAB-0165: a missing meshSource must name the actual file it tried to
        # open, not just print a generic "file not found" with no context.
        assert "obj_does_not_exist.obj" in combined, (
            f"Expected the missing-file warning to name 'obj_does_not_exist.obj', "
            f"got:\n{combined}"
        )
        # AUD-002: an out-of-range vertex index on the triangle-face fast path
        # must be rejected with a clear bounds error, not read out-of-bounds.
        assert "out of range" in combined, (
            f"Expected the out-of-range-index OBJ to be rejected with an "
            f"'out of range' error, got:\n{combined}"
        )

        assert os.path.exists(out_glb) and os.path.getsize(out_glb) > 0, \
            "Output GLB is empty"

        with open(out_glb, "rb") as f:
            data = f.read()
        magic, version, length = struct.unpack_from("<III", data, 0)
        assert magic == 0x46546C67, f"Bad glTF magic: {magic:#x}"

        off = 12
        json_chunk_len, json_chunk_type = struct.unpack_from("<II", data, off)
        json_bytes = data[off + 8: off + 8 + json_chunk_len]
        gltf = json.loads(json_bytes)

        # The valid mesh must still have real geometry.
        nmap = {n.get("name", ""): n for n in gltf.get("nodes", [])}
        assert "Valid" in nmap, f"Expected node 'Valid' in output, got: {sorted(nmap.keys())}"
        meshes = gltf.get("meshes", [])
        accs = gltf.get("accessors", [])
        mesh_idx = nmap["Valid"].get("mesh")
        assert mesh_idx is not None, "Expected 'Valid' node to have mesh geometry"
        pos_idx = meshes[mesh_idx]["primitives"][0]["attributes"]["POSITION"]
        vcount = accs[pos_idx]["count"]
        assert vcount > 0, f"Expected 'Valid' mesh to have vertices, got {vcount}"

        # No accessor may contain a non-finite value in min/max (glTF spec
        # requires numbers; tinygltf serializes non-finite floats as `null`).
        for acc in accs:
            for key in ("min", "max"):
                for v in acc.get(key, []):
                    assert v is not None, (
                        f"Accessor has non-finite ({key}) value serialized as null: {acc}"
                    )

        print(f"Untrusted OBJ robustness: PASS (valid mesh has {vcount} vertices, "
              f"2 malformed OBJs rejected cleanly, no non-finite values in output)")
