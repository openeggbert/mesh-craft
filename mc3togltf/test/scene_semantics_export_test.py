#!/usr/bin/env python3
"""SYS-W3-02: verify instance material precedence in an actual glTF export."""

import json
import os
import subprocess
import sys
import tempfile


if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <mc3togltf> <scene_semantics_export.mc3.xml>")
    sys.exit(1)


with tempfile.TemporaryDirectory() as directory:
    output = os.path.join(directory, "scene-semantics.gltf")
    result = subprocess.run([sys.argv[1], sys.argv[2], output], capture_output=True, text=True)
    assert result.returncode == 0, f"Export failed:\n{result.stderr}"
    with open(output, encoding="utf-8") as stream:
        gltf = json.load(stream)

    material_names = [material.get("name", "") for material in gltf.get("materials", [])]
    nodes = {node.get("name"): node for node in gltf.get("nodes", [])}

    expected = {
        "InheritedDefinitionMaterial": "definition-base",
        "InstanceMaterial": "instance-base",
        "InstanceOverride": "instance-override",
    }
    for node_name, material_name in expected.items():
        node = nodes.get(node_name)
        assert node is not None, f"Missing instance node {node_name!r}"
        mesh_index = node.get("mesh", -1)
        assert mesh_index >= 0, f"{node_name!r} has no direct definition mesh"
        primitive = gltf["meshes"][mesh_index]["primitives"][0]
        material_index = primitive.get("material", -1)
        assert material_index >= 0, f"{node_name!r} definition mesh has no material"
        assert material_names[material_index] == material_name, (
            f"{node_name!r}: expected {material_name!r}, got "
            f"{material_names[material_index]!r}"
        )

print("PASS: glTF instance material precedence matches shared scene semantics")
