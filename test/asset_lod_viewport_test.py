#!/usr/bin/env python3
"""Exercises the live viewport's metadata definition-LOD and culling path."""

import os
import re
import subprocess
import sys
import tempfile


def render(binary, scene):
    handle, screenshot = tempfile.mkstemp(prefix="meshcraft_asset_lod_", suffix=".ppm")
    os.close(handle)
    try:
        command = [binary, scene, "--screenshot", screenshot]
        if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
            command = ["xvfb-run", "--auto-servernum"] + command
        result = subprocess.run(command, capture_output=True, text=True)
        assert result.returncode == 0, (
            f"MeshCraft exited {result.returncode}:\nstdout={result.stdout}\nstderr={result.stderr}"
        )
        assert os.path.getsize(screenshot) > 0, "viewport screenshot is missing or empty"
        match = re.search(r"\[AssetLOD\] tier=(\w+) culled=(\d+) definition=(\S*) reason=(.*)", result.stdout)
        assert match, f"expected AssetLOD debug record, got:\n{result.stdout}"
        return match.groups()
    finally:
        if os.path.exists(screenshot):
            os.remove(screenshot)


if len(sys.argv) != 4:
    print(f"Usage: {sys.argv[0]} <MeshCraft> <far-scene> <culled-scene>")
    sys.exit(1)

far = render(sys.argv[1], sys.argv[2])
assert far[0] == "far" and far[1] == "0" and far[2] == "tree.far", (
    f"expected authored far definition in viewport, got {far}"
)
assert "authored far" in far[3], f"far selection should name its debug reason, got {far[3]!r}"

culled = render(sys.argv[1], sys.argv[3])
assert culled[1] == "1" and "max visibility" in culled[3], (
    f"expected viewport culling record, got {culled}"
)

print("PASS: viewport selects the far metadata definition and culls beyond max visibility")
