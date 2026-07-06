#!/usr/bin/env python3
"""
Cache-correctness test for per-instance deform overrides.

Verifies that the defMeshCache separates instances of the same definition that
carry different per-instance deform scales:
  - BlockA / BlockB      (no deform)       → share exactly one mesh
  - WideBlockA / WideBlockB (deform 2 1 1) → share exactly one mesh
  - TallBlock             (deform 1 2 1)   → its own mesh
  - no-deform mesh ≠ wide-deform mesh ≠ tall-deform mesh
"""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <instance_deform_cache.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "idc.gltf")
        r   = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, (
            f"Export failed (returncode={r.returncode}):\n{r.stderr}"
        )
        assert os.path.exists(out) and os.path.getsize(out) > 0, \
            "Output glTF is missing or empty"

        with open(out) as f:
            gltf = json.load(f)

        nodes  = gltf.get("nodes",  [])
        meshes = gltf.get("meshes", [])
        nmap   = {n.get("name", ""): n for n in nodes}

        # All 5 named nodes must be present
        for name in ("BlockA", "BlockB", "WideBlockA", "WideBlockB", "TallBlock"):
            assert name in nmap, f"Expected node '{name}' in glTF output"

        # BlockA and BlockB share one mesh (no deform — same definition)
        block_a_mesh = nmap["BlockA"].get("mesh", -1)
        block_b_mesh = nmap["BlockB"].get("mesh", -1)
        assert block_a_mesh >= 0, "BlockA has no mesh"
        assert block_a_mesh == block_b_mesh, (
            f"BlockA (mesh {block_a_mesh}) and BlockB (mesh {block_b_mesh}) "
            f"must share one mesh"
        )
        print(f"BlockA and BlockB share mesh {block_a_mesh} — PASS")

        # WideBlockA and WideBlockB share one mesh (same deform 2 1 1)
        wide_a_mesh = nmap["WideBlockA"].get("mesh", -1)
        wide_b_mesh = nmap["WideBlockB"].get("mesh", -1)
        assert wide_a_mesh >= 0, "WideBlockA has no mesh"
        assert wide_a_mesh == wide_b_mesh, (
            f"WideBlockA (mesh {wide_a_mesh}) and WideBlockB (mesh {wide_b_mesh}) "
            f"must share one mesh"
        )
        print(f"WideBlockA and WideBlockB share mesh {wide_a_mesh} — PASS")

        tall_mesh = nmap["TallBlock"].get("mesh", -1)
        assert tall_mesh >= 0, "TallBlock has no mesh"
        print(f"TallBlock has mesh {tall_mesh} — PASS")

        # Meshes must be distinct across deform variants
        assert block_a_mesh != wide_a_mesh, (
            f"Undeformed block (mesh {block_a_mesh}) and wide-deformed block "
            f"(mesh {wide_a_mesh}) must NOT share a mesh"
        )
        print(f"No-deform mesh ({block_a_mesh}) ≠ wide-deform mesh ({wide_a_mesh}) — PASS")

        assert wide_a_mesh != tall_mesh, (
            f"Wide-deformed block (mesh {wide_a_mesh}) and tall-deformed block "
            f"(mesh {tall_mesh}) must NOT share a mesh"
        )
        print(f"Wide-deform mesh ({wide_a_mesh}) ≠ tall-deform mesh ({tall_mesh}) — PASS")

        assert block_a_mesh != tall_mesh, (
            f"Undeformed block (mesh {block_a_mesh}) and tall-deformed block "
            f"(mesh {tall_mesh}) must NOT share a mesh"
        )
        print(f"No-deform mesh ({block_a_mesh}) ≠ tall-deform mesh ({tall_mesh}) — PASS")

        # Total unique meshes: 3 (undeformed, wide, tall)
        assert len(meshes) == 3, (
            f"Expected exactly 3 unique meshes (undeformed, wide, tall), "
            f"got {len(meshes)}"
        )
        print(f"Unique mesh count: {len(meshes)} — PASS")

        # STAB-0254: BlockA/BlockB share a mesh (checked above) but sit at
        # different positions -- each node's own translation must still be
        # correct, proving the geometry cache never leaks a shared mesh's
        # transform onto sibling nodes (transform lives on the node, the
        # cache only ever holds a mesh index).
        EXPECTED_TRANSLATION = {
            "BlockA": None,              # (0,0,0) is the default -- correctly omitted
            "BlockB": [3.0, 0.0, 0.0],
            "WideBlockA": [0.0, 0.0, 3.0],
            "WideBlockB": [3.0, 0.0, 3.0],
            "TallBlock": [0.0, 0.0, 6.0],
        }
        for name, expected in EXPECTED_TRANSLATION.items():
            actual = nmap[name].get("translation")
            assert actual == expected, (
                f"STAB-0254: '{name}'.translation: expected {expected}, got {actual} "
                f"(mesh-cache transform leak?)"
            )
        print("STAB-0254: each node's own translation is correct despite mesh "
              "sharing (no cache transform leak) — PASS")

    print("\nAll instance-deform cache tests: PASS")
