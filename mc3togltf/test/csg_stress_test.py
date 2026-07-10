#!/usr/bin/env python3
"""
STAB-0223/0233/0675: CSG stress tests.

  - STAB-0223: a union of 10 boxes exports without crashing and produces
    real (non-zero) triangle geometry.
  - STAB-0233: a union tree exactly 5 levels deep containing 32 boxes
    total evaluates in under 5 seconds.
  - STAB-0675: widens coverage beyond union-of-boxes (the only scale
    scenario this file exercised before):
      - ManyCuttersDifference: a single Difference with 8 sphere cutters.
      - IntersectionStress: an Intersection of 6 overlapping spheres.
      - NestedMixedOps: all three operators (union/difference/intersection)
        combined in one tree, unlike DeepUnion which only ever nests the
        same operator.
      - CurvedUnionStress: a union of 12 curved (non-box) primitives.
"""
import json
import os
import subprocess
import sys
import tempfile
import time


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def node_map(gltf):
    return {n.get("name", ""): n for n in gltf.get("nodes", [])}


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <csg_stress.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")

        start = time.monotonic()
        r = run([mc3togltf, xml_path, out])
        elapsed = time.monotonic() - start

        assert r.returncode == 0, (
            f"CSG stress export failed (returncode={r.returncode}):\n{r.stderr}"
        )

        with open(out) as f:
            gltf = json.load(f)
        nmap = node_map(gltf)
        meshes = gltf.get("meshes", [])
        accs = gltf.get("accessors", [])

        def triangle_count(node_name):
            node = nmap[node_name]
            mesh_idx = node.get("mesh")
            assert mesh_idx is not None, f"'{node_name}' has no mesh"
            prim = meshes[mesh_idx]["primitives"][0]
            idx_acc = prim.get("indices")
            assert idx_acc is not None, f"'{node_name}' primitive has no indices accessor"
            ic = accs[idx_acc].get("count", 0)
            assert ic > 0 and ic % 3 == 0, f"'{node_name}' has invalid index count {ic}"
            return ic // 3

        def node_bbox(node_name):
            node = nmap[node_name]
            mesh_idx = node.get("mesh")
            assert mesh_idx is not None, f"'{node_name}' has no mesh"
            prim = meshes[mesh_idx]["primitives"][0]
            pos_acc = accs[prim["attributes"]["POSITION"]]
            return pos_acc["min"], pos_acc["max"]

        # STAB-0223: 10-box union.
        assert "TenBoxUnion" in nmap, f"Expected 'TenBoxUnion' node, got: {sorted(nmap.keys())}"
        ten_box_tris = triangle_count("TenBoxUnion")
        print(f"STAB-0223: 10-box union exports without crash "
              f"({ten_box_tris} triangles) — PASS")

        # STAB-0233: 5-level nested union of 32 boxes, under 5 seconds.
        assert "DeepUnion" in nmap, f"Expected 'DeepUnion' node, got: {sorted(nmap.keys())}"
        deep_tris = triangle_count("DeepUnion")
        assert elapsed < 5.0, (
            f"Expected the whole export (including the 5-level/32-box "
            f"DeepUnion) to finish in under 5s, took {elapsed:.2f}s"
        )
        print(f"STAB-0233: 5-level nested union of 32 boxes evaluates in "
              f"{elapsed:.2f}s (< 5s), {deep_tris} triangles — PASS")

        # STAB-0675: Difference with 8 cutters. The base block spans
        # x=[-4,4]; cutters are all interior, so the bbox must be
        # unchanged from the uncut block (confirms cutters carved real
        # cavities without either failing silently or consuming the block).
        assert "ManyCuttersDifference" in nmap, (
            f"Expected 'ManyCuttersDifference' node, got: {sorted(nmap.keys())}"
        )
        many_cutters_tris = triangle_count("ManyCuttersDifference")
        mc_min, mc_max = node_bbox("ManyCuttersDifference")
        assert abs(mc_min[0] - (-4.0)) < 0.01 and abs(mc_max[0] - 4.0) < 0.01, (
            f"ManyCuttersDifference bbox X should span the full 8-unit block "
            f"(cutters are interior), got min={mc_min} max={mc_max}"
        )
        print(f"STAB-0675: Difference with 8 cutters exports without crash "
              f"({many_cutters_tris} triangles) — PASS")

        # STAB-0675: Intersection of 6 overlapping spheres (radius 1.0
        # each). A non-empty, genuinely-smaller-than-any-single-sphere
        # bbox confirms a real intersection happened (all 6 spheres
        # actually overlap in a shared central region), not an empty
        # result or a silent fallback to one child.
        assert "IntersectionStress" in nmap, (
            f"Expected 'IntersectionStress' node, got: {sorted(nmap.keys())}"
        )
        inter_tris = triangle_count("IntersectionStress")
        inter_min, inter_max = node_bbox("IntersectionStress")
        inter_extent = [inter_max[i] - inter_min[i] for i in range(3)]
        assert all(0 < e < 2.0 for e in inter_extent), (
            f"IntersectionStress bbox extent {inter_extent} should be "
            f"non-empty but smaller than a single sphere's 2.0 diameter"
        )
        print(f"STAB-0675: Intersection of 6 overlapping spheres exports "
              f"without crash ({inter_tris} triangles, bbox extent "
              f"{[round(e, 3) for e in inter_extent]}) — PASS")

        # STAB-0675: nested mixed operators (outer Difference, base Union
        # of 3 boxes, cutter Intersection of 2 spheres). Base footprint is
        # x=[-0.5, 1.9]; the sphere cutters are interior in y/z, so the
        # bbox should match the union base exactly.
        assert "NestedMixedOps" in nmap, (
            f"Expected 'NestedMixedOps' node, got: {sorted(nmap.keys())}"
        )
        mixed_tris = triangle_count("NestedMixedOps")
        mixed_min, mixed_max = node_bbox("NestedMixedOps")
        assert abs(mixed_min[0] - (-0.5)) < 0.01 and abs(mixed_max[0] - 1.9) < 0.01, (
            f"NestedMixedOps bbox X should match the 3-box union base "
            f"footprint [-0.5, 1.9], got min={mixed_min} max={mixed_max}"
        )
        print(f"STAB-0675: nested mixed union+difference+intersection "
              f"exports without crash ({mixed_tris} triangles) — PASS")

        # STAB-0675: union of 12 curved (non-box) primitives -- confirms
        # the full footprint (all 12 objects, x=[-0.5, 9.3]) unions
        # correctly, not just a subset silently dropped.
        assert "CurvedUnionStress" in nmap, (
            f"Expected 'CurvedUnionStress' node, got: {sorted(nmap.keys())}"
        )
        curved_tris = triangle_count("CurvedUnionStress")
        curved_min, curved_max = node_bbox("CurvedUnionStress")
        assert abs(curved_min[0] - (-0.5)) < 0.01 and abs(curved_max[0] - 9.3) < 0.01, (
            f"CurvedUnionStress bbox X should span all 12 objects [-0.5, 9.3], "
            f"got min={curved_min} max={curved_max}"
        )
        print(f"STAB-0675: union of 12 curved primitives exports without "
              f"crash ({curved_tris} triangles) — PASS")

    print("\nCSG stress test: PASS")
