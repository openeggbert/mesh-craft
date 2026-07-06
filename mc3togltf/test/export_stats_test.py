#!/usr/bin/env python3
"""
STAB-0189/0190/0191/0192: `--stats` output must reflect the *actual*
exported glTF, not just some internal counter that could drift from it.

  - STAB-0189: "glTF nodes:" == len(gltf["nodes"])
  - STAB-0190: "Unique meshes:" == len(gltf["meshes"])
  - STAB-0191: "Materials:" == len(gltf["materials"]) (this stat did not
    exist before this task -- added ExportStats::materialCount /
    GltfExporter.cpp / main.cpp's printed "Materials:" line specifically
    to make this row's ask checkable)
  - STAB-0192: "Warnings:" equals the real number of warning-triggering
    conditions in the input, verified two ways: 0 for a clean scene
    (house.mc3.xml), and 3 for obj_robustness.mc3.xml (which references
    one negative-index OBJ, one infinite-coordinate OBJ, and one missing
    OBJ file -- three independently-known warning sources, see
    obj_robustness_test.py).
"""
import json
import os
import re
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def parse_stats(text):
    stats = {}
    for line in text.splitlines():
        m = re.match(r"\s*([A-Za-z ]+):\s*(\d+)\s*$", line)
        if m:
            stats[m.group(1).strip()] = int(m.group(2))
    return stats


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <house.mc3.xml> <obj_robustness.mc3.xml>")
        sys.exit(1)

    mc3togltf   = sys.argv[1]
    house_xml   = sys.argv[2]
    obj_rob_xml = sys.argv[3]

    with tempfile.TemporaryDirectory() as tmpdir:
        # --- Clean scene: node/mesh/material counts must match the real output ---
        out = os.path.join(tmpdir, "house.gltf")
        r = run([mc3togltf, house_xml, out, "--stats"])
        assert r.returncode == 0, f"Export failed:\n{r.stderr}"
        stats = parse_stats(r.stdout)
        for required in ("glTF nodes", "Unique meshes", "Materials", "Warnings"):
            assert required in stats, f"Expected '{required}:' in --stats output, got:\n{r.stdout}"

        with open(out) as f:
            gltf = json.load(f)

        assert stats["glTF nodes"] == len(gltf.get("nodes", [])), (
            f"STAB-0189: stats say {stats['glTF nodes']} nodes, "
            f"glTF actually has {len(gltf.get('nodes', []))}"
        )
        print(f"STAB-0189: glTF nodes stat ({stats['glTF nodes']}) matches actual output — PASS")

        assert stats["Unique meshes"] == len(gltf.get("meshes", [])), (
            f"STAB-0190: stats say {stats['Unique meshes']} meshes, "
            f"glTF actually has {len(gltf.get('meshes', []))}"
        )
        print(f"STAB-0190: Unique meshes stat ({stats['Unique meshes']}) matches actual output — PASS")

        assert stats["Materials"] == len(gltf.get("materials", [])), (
            f"STAB-0191: stats say {stats['Materials']} materials, "
            f"glTF actually has {len(gltf.get('materials', []))}"
        )
        print(f"STAB-0191: Materials stat ({stats['Materials']}) matches actual output — PASS")

        assert stats["Warnings"] == 0, (
            f"STAB-0192: expected 0 warnings for a clean scene, got {stats['Warnings']}"
        )
        print("STAB-0192: Warnings stat is 0 for a clean scene — PASS")

        # --- Scene with 3 known warning sources: warning count must reflect that exactly ---
        out2 = os.path.join(tmpdir, "obj_rob.glb")
        r2 = run([mc3togltf, obj_rob_xml, out2, "--stats"])
        assert r2.returncode == 0, f"Export failed:\n{r2.stderr}"
        stats2 = parse_stats(r2.stdout)
        assert stats2.get("Warnings") == 3, (
            f"STAB-0192: expected exactly 3 warnings (negative-index OBJ, "
            f"infinite-coord OBJ, missing OBJ file), got {stats2.get('Warnings')}\n"
            f"stdout:\n{r2.stdout}"
        )
        print(f"STAB-0192: Warnings stat (3) matches the 3 known bad-OBJ sources — PASS")

    print("\nExport statistics accuracy test: PASS")
