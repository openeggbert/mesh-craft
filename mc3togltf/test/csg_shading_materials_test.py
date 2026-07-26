#!/usr/bin/env python3
"""SYS-W14-06 regression test for real CSG shading data and material runs.

The fixture has an unmaterialed CSG root, so the output must retain the red
box and blue sphere as separate glTF primitives. It also reads the real GLB
buffer to establish that normals and generated root UVs are meaningful rather
than the former flat-normal / (0, 0)-UV placeholders.
"""
import json
import os
import struct
import subprocess
import sys
import tempfile


def parse_glb(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, version, _ = struct.unpack_from("<III", data, 0)
    assert magic == 0x46546C67 and version == 2, "output is not GLB v2"
    offset = 12
    json_len, json_type = struct.unpack_from("<II", data, offset)
    assert json_type == 0x4E4F534A, "first GLB chunk is not JSON"
    gltf = json.loads(data[offset + 8:offset + 8 + json_len])
    offset += 8 + json_len
    bin_len, bin_type = struct.unpack_from("<II", data, offset)
    assert bin_type == 0x004E4942, "second GLB chunk is not BIN"
    return gltf, data[offset + 8:offset + 8 + bin_len]


def read_vec(gltf, data, accessor_index, components):
    accessor = gltf["accessors"][accessor_index]
    assert accessor["componentType"] == 5126, "expected float accessor"
    view = gltf["bufferViews"][accessor["bufferView"]]
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    stride = view.get("byteStride", components * 4)
    return [struct.unpack_from("<" + "f" * components, data, offset + i * stride)
            for i in range(accessor["count"])]


def read_indices(gltf, data, accessor_index):
    accessor = gltf["accessors"][accessor_index]
    formats = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4)}
    fmt, width = formats[accessor["componentType"]]
    view = gltf["bufferViews"][accessor["bufferView"]]
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    return [struct.unpack_from("<" + fmt, data, offset + i * width)[0]
            for i in range(accessor["count"])]


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <fixture>", file=sys.stderr)
        return 1
    binary, fixture = sys.argv[1:3]
    with tempfile.NamedTemporaryFile(suffix=".glb", delete=False) as f:
        output = f.name
    try:
        result = subprocess.run([binary, fixture, output], capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        gltf, buffer = parse_glb(output)
        node = next(node for node in gltf["nodes"] if node.get("name") == "ChildMaterials")
        primitives = gltf["meshes"][node["mesh"]]["primitives"]
        materials = gltf["materials"]
        material_names = {materials[prim["material"]]["name"] for prim in primitives
                          if "material" in prim}
        assert material_names == {"red", "blue"}, (
            f"CSG child material runs lost: expected red/blue, got {material_names}")

        attrs = primitives[0]["attributes"]
        assert "POSITION" in attrs and "NORMAL" in attrs and "TEXCOORD_0" in attrs
        positions = read_vec(gltf, buffer, attrs["POSITION"], 3)
        normals = read_vec(gltf, buffer, attrs["NORMAL"], 3)
        texcoords = read_vec(gltf, buffer, attrs["TEXCOORD_0"], 2)
        assert len(positions) == len(normals) == len(texcoords) > 100
        assert len({tuple(round(v, 4) for v in uv) for uv in texcoords}) > 20, (
            "CSG UVs are still degenerate instead of using the root projection")

        # The sphere leaves smoothly-shaded triangles in the union. A flat
        # triangle-soup conversion gives all three corners of every triangle
        # one shared normal; find a blue-material triangle whose vertex normals
        # vary instead.
        blue_index = next(index for index, material in enumerate(materials)
                          if material.get("name") == "blue")
        blue_prim = next(prim for prim in primitives if prim.get("material") == blue_index)
        indices = read_indices(gltf, buffer, blue_prim["indices"])
        assert any(len({tuple(round(v, 4) for v in normals[index])
                        for index in indices[offset:offset + 3]}) > 1
                   for offset in range(0, len(indices), 3)), (
            "CSG normals are still flat triangle normals; expected smooth Manifold normals")

        print("PASS: CSG child materials, smooth normals, and generated UVs")
        return 0
    finally:
        if os.path.exists(output):
            os.unlink(output)


if __name__ == "__main__":
    sys.exit(main())
