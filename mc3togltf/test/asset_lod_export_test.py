#!/usr/bin/env python3
"""Verifies that glTF exports metadata's explicit near/default Instance tier.

Export has no camera distance, therefore it must not bake the viewport's
distance culling or select the far tier even though this fixture's placement
is 100 metres away and its base definition has max_visibility_distance=1.
"""

import json
import os
import subprocess
import sys
import tempfile


if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <mc3togltf> <asset_lod_export.mc3.xml>")
    sys.exit(1)

with tempfile.TemporaryDirectory() as directory:
    output = os.path.join(directory, "asset-lod.gltf")
    result = subprocess.run([sys.argv[1], sys.argv[2], output], capture_output=True, text=True)
    assert result.returncode == 0, f"Export failed:\n{result.stderr}"
    with open(output, encoding="utf-8") as stream:
        gltf = json.load(stream)

    nodes = {node.get("name"): node for node in gltf.get("nodes", [])}
    assert "ExportTree" in nodes, "Instance node is missing from the glTF"
    mesh_index = nodes["ExportTree"].get("mesh", -1)
    assert mesh_index >= 0, "Default asset LOD must export a mesh, not viewport-cull the node"
    mesh = gltf.get("meshes", [])[mesh_index]
    assert mesh.get("name") == "NearSphere", (
        "glTF must export the explicit near/default metadata definition; "
        f"expected NearSphere, got {mesh.get('name')!r}"
    )

print("PASS: glTF exports the explicit near/default asset LOD tier")
