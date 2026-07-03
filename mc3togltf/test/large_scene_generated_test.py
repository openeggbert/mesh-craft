#!/usr/bin/env python3
"""
Generated large-scene regression test for geometry reuse.

Builds a temporary MC3 scene entirely in Python (no static XML file needed):
  - 1 definition "unit_box" (1x1x1 box)
  - N_INSTANCES instance nodes of unit_box  → all share exactly 1 glTF mesh
  - N_SPHERES identical sphere nodes (r=0.5) → all share exactly 1 glTF mesh
  - N_BOXES identical box nodes (2x2x2)      → all share exactly 1 glTF mesh

Default (SCALE=1): 200 MC3 objects, ≤ 3 unique glTF meshes expected.
An optional second CLI arg scales the object counts (STAB-0244 passes 2.5
for a 500-object export, asserting it completes in under 30s).

This verifies that geometry reuse scales correctly beyond the static
large_scene.mc3.xml fixture.
"""
import json
import os
import subprocess
import sys
import tempfile
import time


SCALE       = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0
N_INSTANCES = round(100 * SCALE)
N_SPHERES   = round(50 * SCALE)
N_BOXES     = round(50 * SCALE)
MAX_SECONDS = 30.0


def generate_xml():
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<mc3 version="0.3" model="GeneratedLargeScene">',
        '  <definitions>',
        '    <definition id="unit_box">',
        '      <box name="UnitBox" size="1 1 1"/>',
        '    </definition>',
        '  </definitions>',
        '  <objects>',
    ]
    for i in range(N_INSTANCES):
        lines.append(
            f'    <instance name="Inst_{i:04d}" definition="unit_box"'
            f' position="{i * 1.5:.1f} 0 0"/>'
        )
    for i in range(N_SPHERES):
        lines.append(
            f'    <sphere name="Sph_{i:04d}" radius="0.5" segments="8"'
            f' position="{i * 1.5:.1f} 5 0"/>'
        )
    for i in range(N_BOXES):
        lines.append(
            f'    <box name="BigBox_{i:04d}" size="2 2 2"'
            f' position="{i * 2.5:.1f} 0 10"/>'
        )
    lines += ['  </objects>', '</mc3>']
    return '\n'.join(lines)


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    total     = N_INSTANCES + N_SPHERES + N_BOXES

    with tempfile.TemporaryDirectory() as tmpdir:
        xml_path = os.path.join(tmpdir, "generated.mc3.xml")
        with open(xml_path, "w") as f:
            f.write(generate_xml())

        out        = os.path.join(tmpdir, "generated.gltf")
        started    = time.monotonic()
        r          = run([mc3togltf, xml_path, out])
        elapsed    = time.monotonic() - started
        assert r.returncode == 0, (
            f"Export failed (returncode={r.returncode}):\n{r.stderr}"
        )
        assert os.path.exists(out) and os.path.getsize(out) > 0, \
            "Output glTF is missing or empty"
        assert elapsed < MAX_SECONDS, (
            f"Export of {total} objects took {elapsed:.2f}s, "
            f"expected < {MAX_SECONDS:.0f}s (STAB-0244)"
        )
        print(f"Export of {total} objects completed in {elapsed:.2f}s "
              f"(expected < {MAX_SECONDS:.0f}s) — PASS")

        with open(out) as f:
            gltf = json.load(f)

        nodes  = gltf.get("nodes",  [])
        meshes = gltf.get("meshes", [])
        nmap   = {n.get("name", ""): n for n in nodes}

        # ----------------------------------------------------------------
        # Node count: expect at least total objects.
        # ----------------------------------------------------------------
        assert len(nodes) >= total, (
            f"Expected at least {total} glTF nodes, got {len(nodes)}"
        )
        print(f"Node count: {len(nodes)} (expected ≥ {total}) — PASS")

        # ----------------------------------------------------------------
        # Mesh reuse: 3 unique mesh geometries for 200 nodes.
        # Allow headroom of 6 for edge cases.
        # ----------------------------------------------------------------
        assert len(meshes) <= 6, (
            f"Expected at most 6 unique glTF meshes for {total} objects, "
            f"got {len(meshes)} — geometry reuse is not working"
        )
        assert len(nodes) >= len(meshes) * 10, (
            f"Expected nodes ({len(nodes)}) ≥ 10× meshes ({len(meshes)})"
        )
        print(f"Unique meshes: {len(meshes)} vs {len(nodes)} nodes — PASS")

        # ----------------------------------------------------------------
        # All instances share one mesh.
        # ----------------------------------------------------------------
        inst_meshes = {nmap[f"Inst_{i:04d}"]["mesh"]
                       for i in range(N_INSTANCES)
                       if f"Inst_{i:04d}" in nmap and "mesh" in nmap[f"Inst_{i:04d}"]}
        assert len(inst_meshes) == 1, (
            f"All {N_INSTANCES} instances must share exactly one mesh, "
            f"got {len(inst_meshes)} distinct mesh indices: {inst_meshes}"
        )
        print(f"All {N_INSTANCES} instances share mesh {inst_meshes.pop()} — PASS")

        # ----------------------------------------------------------------
        # All spheres share one mesh.
        # ----------------------------------------------------------------
        sph_meshes = {nmap[f"Sph_{i:04d}"]["mesh"]
                      for i in range(N_SPHERES)
                      if f"Sph_{i:04d}" in nmap and "mesh" in nmap[f"Sph_{i:04d}"]}
        assert len(sph_meshes) == 1, (
            f"All {N_SPHERES} spheres must share exactly one mesh, "
            f"got {len(sph_meshes)} distinct mesh indices: {sph_meshes}"
        )
        print(f"All {N_SPHERES} spheres share mesh {sph_meshes.pop()} — PASS")

        # ----------------------------------------------------------------
        # All big boxes share one mesh.
        # ----------------------------------------------------------------
        box_meshes = {nmap[f"BigBox_{i:04d}"]["mesh"]
                      for i in range(N_BOXES)
                      if f"BigBox_{i:04d}" in nmap and "mesh" in nmap[f"BigBox_{i:04d}"]}
        assert len(box_meshes) == 1, (
            f"All {N_BOXES} big boxes must share exactly one mesh, "
            f"got {len(box_meshes)} distinct mesh indices: {box_meshes}"
        )
        print(f"All {N_BOXES} big boxes share mesh {box_meshes.pop()} — PASS")

    print(f"\nGenerated large-scene reuse test ({total} objects): PASS")
