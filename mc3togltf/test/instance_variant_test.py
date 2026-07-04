#!/usr/bin/env python3
"""
Regression test for STAB-0493 (Instance "variants" resolution in glTF export).

Mc3Object::resolvedInstanceDefinitionKey() deterministically picks one entry
from `variantDefinitions` by hashing the object's own `id`, so the same
Instance always resolves to the same variant across runs/consumers. Before
this fix, GltfExporter.cpp's regular (non-CSG) Instance path ignored
`variantDefinitions` entirely and always used `definition` — i.e. always the
FIRST variant — so every instance below (V0..V7, each with a distinct id and
the same 2-entry variants list) would have resolved to the same mesh
("defBox"), no variation at all.

This test does NOT hardcode which specific instance resolves to which
variant (that depends on std::hash<std::string>'s implementation, which is
unspecified/platform-dependent) — it only asserts the *shape* of correct
behavior: real variation occurs (not all 8 instances collapse onto a single
mesh) and resolution is deterministic (re-exporting produces the identical
per-instance assignment).
"""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def export_and_map(mc3togltf, xml_path, out_path):
    r = run([mc3togltf, xml_path, out_path])
    assert r.returncode == 0, (
        f"Export failed (returncode={r.returncode}):\n{r.stderr}"
    )
    assert "unknown definition" not in r.stderr, (
        f"Unexpected 'unknown definition' warning — both variants are "
        f"valid definitions in this fixture:\n{r.stderr}"
    )
    assert os.path.exists(out_path) and os.path.getsize(out_path) > 0, \
        "Output glTF is missing or empty"

    with open(out_path) as f:
        gltf = json.load(f)

    nodes = gltf.get("nodes", [])
    nmap = {n.get("name", ""): n for n in nodes}

    names = [f"V{i}" for i in range(8)]
    for name in names:
        assert name in nmap, f"Expected node '{name}' in glTF output"

    mesh_of = {}
    for name in names:
        m = nmap[name].get("mesh", -1)
        assert m >= 0, f"{name} has no mesh"
        mesh_of[name] = m
    return mesh_of


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <instance_variant.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out1 = os.path.join(tmpdir, "iv1.gltf")
        mesh_of_1 = export_and_map(mc3togltf, xml_path, out1)

        distinct_meshes = set(mesh_of_1.values())
        assert len(distinct_meshes) == 2, (
            f"Expected exactly 2 distinct meshes used across the 8 variant "
            f"instances (proving real per-instance variation, not every "
            f"instance silently falling back to the first variant), got "
            f"{len(distinct_meshes)}: {mesh_of_1}"
        )
        print(f"8 variant instances resolve to {len(distinct_meshes)} distinct meshes — PASS")

        # Determinism: re-exporting the identical scene must assign the exact
        # same mesh to each instance (hash(id) is stable, not a per-run reroll).
        out2 = os.path.join(tmpdir, "iv2.gltf")
        mesh_of_2 = export_and_map(mc3togltf, xml_path, out2)
        assert mesh_of_1 == mesh_of_2, (
            f"Variant resolution must be deterministic across export runs:\n"
            f"run1={mesh_of_1}\nrun2={mesh_of_2}"
        )
        print("Variant resolution is deterministic across repeated export runs — PASS")

    print("\nAll instance-variant tests: PASS")
