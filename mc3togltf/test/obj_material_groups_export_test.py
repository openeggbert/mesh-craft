#!/usr/bin/env python3
"""SYS-W14-35: imported OBJ material children remain separate in glTF."""
import json
import os
import subprocess
import sys
import tempfile


def primitive_material(gltf, node_name):
    nodes = {node.get("name"): node for node in gltf.get("nodes", [])}
    assert node_name in nodes, f"Missing node {node_name!r}; got {sorted(nodes)}"
    mesh_index = nodes[node_name].get("mesh")
    assert mesh_index is not None, f"Node {node_name!r} has no mesh"
    primitives = gltf["meshes"][mesh_index].get("primitives", [])
    assert len(primitives) == 1, f"{node_name!r} should contain its one selected material group"
    material_index = primitives[0].get("material")
    assert material_index is not None, f"{node_name!r} primitive lacks material"
    position = primitives[0]["attributes"]["POSITION"]
    assert gltf["accessors"][position]["count"] == 3, (
        f"{node_name!r} must contain exactly one triangle, not a flattened OBJ")
    return gltf["materials"][material_index]


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <fixture.mc3.xml>", file=sys.stderr)
        sys.exit(2)
    binary, fixture = sys.argv[1:]
    with tempfile.TemporaryDirectory() as directory:
        output = os.path.join(directory, "obj-material-groups.gltf")
        result = subprocess.run([binary, fixture, output], capture_output=True, text=True)
        assert result.returncode == 0, f"Export failed:\n{result.stderr}"
        with open(output, encoding="utf-8") as stream:
            gltf = json.load(stream)
    red = primitive_material(gltf, "RedGroup")
    blue = primitive_material(gltf, "BlueGroup")
    assert red.get("name") == "red_paint", red
    assert blue.get("name") == "blue_metal", blue
    assert all(abs(actual - expected) < 1e-5 for actual, expected in zip(
        red["pbrMetallicRoughness"]["baseColorFactor"], [0.8, 0.1, 0.2, 0.75])), red
    assert abs(blue["pbrMetallicRoughness"]["metallicFactor"] - 0.9) < 1e-5, blue
    print("Material-aware OBJ glTF export: PASS (two selected groups, two mapped materials)")
