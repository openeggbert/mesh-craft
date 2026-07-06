#!/usr/bin/env python3
"""
STAB-0256: animation does not corrupt the geometry cache.

Two identical boxes (same size) share one glTF mesh via the geometry
cache; only one of them ("BoxAnimated") has an animation channel. This
confirms:
  - both boxes still share exactly one mesh (geometry cache unaffected
    by one of them being animated)
  - each box's own static translation is still correct (no cross-talk)
  - the animation channel targets the correct node (BoxAnimated, not
    BoxStatic or some cache-confused index) with the right property path
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
        print(f"Usage: {sys.argv[0]} <mc3togltf> <anim_geom_cache.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, f"Export failed (returncode={r.returncode}):\n{r.stderr}"

        with open(out) as f:
            gltf = json.load(f)

        nodes  = gltf.get("nodes", [])
        meshes = gltf.get("meshes", [])
        nmap = {n.get("name", ""): n for n in nodes}

        for name in ("BoxStatic", "BoxAnimated"):
            assert name in nmap, f"Expected node '{name}', got: {sorted(nmap.keys())}"

        # Geometry cache: both boxes still share exactly one mesh.
        static_mesh   = nmap["BoxStatic"].get("mesh")
        animated_mesh = nmap["BoxAnimated"].get("mesh")
        assert static_mesh is not None and static_mesh == animated_mesh, (
            f"Expected BoxStatic and BoxAnimated to share one mesh, got "
            f"{static_mesh} vs {animated_mesh}"
        )
        assert len(meshes) == 1, f"Expected exactly 1 unique mesh, got {len(meshes)}"
        print(f"STAB-0256: BoxStatic/BoxAnimated still share mesh {static_mesh} "
              f"despite one being animated — PASS")

        # Static transforms are still correct (no cross-talk from caching).
        assert nmap["BoxStatic"].get("translation") == [-2.0, 0.0, 0.0], (
            f"BoxStatic.translation corrupted: {nmap['BoxStatic'].get('translation')}"
        )
        assert nmap["BoxAnimated"].get("translation") == [2.0, 0.0, 0.0], (
            f"BoxAnimated.translation (rest pose) corrupted: {nmap['BoxAnimated'].get('translation')}"
        )
        print("STAB-0256: both nodes' static translations are correct — PASS")

        # Animation channel targets the correct node with the correct property.
        animations = gltf.get("animations", [])
        assert len(animations) == 1 and animations[0].get("name") == "Bounce", (
            f"Expected 1 animation named 'Bounce', got: {[a.get('name') for a in animations]}"
        )
        channels = animations[0].get("channels", [])
        assert len(channels) == 1, f"Expected 1 channel, got {len(channels)}"
        target_node_idx = channels[0]["target"]["node"]
        assert nodes[target_node_idx].get("name") == "BoxAnimated", (
            f"Expected the animation channel to target 'BoxAnimated' "
            f"(node {[i for i,n in enumerate(nodes) if n.get('name')=='BoxAnimated'][0]}), "
            f"got node {target_node_idx} ('{nodes[target_node_idx].get('name')}')"
        )
        assert channels[0]["target"]["path"] == "translation", (
            f"Expected channel path 'translation', got '{channels[0]['target']['path']}'"
        )
        print("STAB-0256: animation channel correctly targets BoxAnimated.translation — PASS")

    print("\nAnimation/geometry-cache interaction test: PASS")
