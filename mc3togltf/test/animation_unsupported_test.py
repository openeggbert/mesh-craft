#!/usr/bin/env python3
"""
STAB-0447/STAB-0448: material and deform animation channels have no glTF
node-transform equivalent. Export must still succeed (exit 0), print a
warning naming the unsupported channel, and the action containing only such
channels must not appear in the exported glTF's animations array at all
(exportAnimations() skips an action entirely once it has zero
transform-compatible channels — same code path already used for
visible-only actions).
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
        print(f"Usage: {sys.argv[0]} <mc3togltf> <animation_unsupported.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, (
            f"Expected export to succeed despite unsupported animation channels, "
            f"got returncode={r.returncode}\nstderr: {r.stderr}"
        )
        assert os.path.exists(out) and os.path.getsize(out) > 0, \
            "Output glTF is missing or empty"

        combined = r.stdout + r.stderr
        assert "material.baseColor.r" in combined, (
            f"Expected a warning naming the unsupported 'material.baseColor.r' "
            f"channel, got:\n{combined}"
        )
        assert "deform.x" in combined, (
            f"Expected a warning naming the unsupported 'deform.x' channel, got:\n{combined}"
        )
        print("Unsupported-channel warnings: PASS")

        with open(out) as f:
            gltf = json.load(f)

        anims = gltf.get("animations", [])
        anim_names = {a.get("name", "") for a in anims}
        assert "Pulse" not in anim_names, (
            f"Expected 'Pulse' (material-only action) NOT in animations, got: {anim_names}"
        )
        assert "Squash" not in anim_names, (
            f"Expected 'Squash' (deform-only action) NOT in animations, got: {anim_names}"
        )
        assert len(anims) == 0, (
            f"Expected 0 exported animations (both actions are entirely "
            f"unsupported-property), got {len(anims)}: {anim_names}"
        )
        print(f"Unsupported-channel-only actions excluded from glTF: PASS (0 animations exported)")

    print("\nUnsupported animation channel test: PASS")
