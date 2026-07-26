#!/usr/bin/env python3
"""SYS-W14-05: export external and inline GLB <embed> geometry safely.

The MC3 parser has long preserved ``embed:<id>`` references, but export used
to treat that identifier as a literal OBJ filename and deliberately produce an
empty node.  This test makes one small self-contained GLB at runtime, references
it both externally and as base64 CDATA, and proves the exported result contains
the flattened triangle at the GLB node's translated coordinates.

Usage: embed_mesh_source_test.py <mc3togltf> <unused-fixture-path> [MeshCraft]
"""
import base64
import json
import os
import struct
import subprocess
import sys
import tempfile


def make_glb():
    """A one-triangle GLB whose scene node translates the geometry +1.5 on X."""
    positions = struct.pack("<9f", 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0)
    normals = struct.pack("<9f", 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0)
    texcoords = struct.pack("<6f", 0.0, 0.0, 1.0, 0.0, 0.0, 1.0)
    indices = struct.pack("<3H", 0, 1, 2)
    payload = positions + normals + texcoords + indices
    gltf = {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "translation": [1.5, 0.0, 0.0]}],
        "meshes": [{"primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3,
            "mode": 4,
        }]}],
        "buffers": [{"byteLength": len(payload)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(positions)},
            {"buffer": 0, "byteOffset": len(positions), "byteLength": len(normals)},
            {"buffer": 0, "byteOffset": len(positions) + len(normals), "byteLength": len(texcoords)},
            {"buffer": 0, "byteOffset": len(positions) + len(normals) + len(texcoords), "byteLength": len(indices)},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR"},
        ],
    }
    json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    json_bytes += b" " * ((-len(json_bytes)) % 4)
    binary_chunk = payload + b"\0" * ((-len(payload)) % 4)
    total = 12 + 8 + len(json_bytes) + 8 + len(binary_chunk)
    return (struct.pack("<III", 0x46546C67, 2, total) +
            struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes +
            struct.pack("<II", len(binary_chunk), 0x004E4942) + binary_chunk)


def write_xml(path, embed):
    with open(path, "w", encoding="utf-8") as out:
        out.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
        out.write("<mc3 version=\"0.3\" model=\"EmbedFeatureTest\">\n")
        out.write("  <embeds>\n")
        out.write(embed)
        out.write("\n  </embeds>\n  <objects>\n")
        out.write("    <mesh name=\"EmbeddedTriangle\" src=\"embed:triangle\"/>\n")
        out.write("  </objects>\n</mc3>\n")


def read_glb_json_and_bin(path):
    data = open(path, "rb").read()
    magic, version, total_length = struct.unpack_from("<III", data, 0)
    assert magic == 0x46546C67 and version == 2 and total_length == len(data), "invalid GLB output"
    json_length, json_type = struct.unpack_from("<II", data, 12)
    assert json_type == 0x4E4F534A, "missing JSON GLB chunk"
    gltf = json.loads(data[20:20 + json_length])
    binary_offset = 20 + json_length
    binary_length, binary_type = struct.unpack_from("<II", data, binary_offset)
    assert binary_type == 0x004E4942, "missing binary GLB chunk"
    return gltf, data[binary_offset + 8:binary_offset + 8 + binary_length]


def assert_flattened_triangle(path, expected_name):
    gltf, binary = read_glb_json_and_bin(path)
    nodes = {node.get("name"): node for node in gltf.get("nodes", [])}
    assert expected_name in nodes, f"missing {expected_name!r} node: {sorted(nodes)}"
    node = nodes[expected_name]
    assert "mesh" in node, f"{expected_name} has no resolved mesh"
    primitive = gltf["meshes"][node["mesh"]]["primitives"][0]
    pos_accessor = gltf["accessors"][primitive["attributes"]["POSITION"]]
    assert pos_accessor["count"] == 3, f"expected one triangle, got {pos_accessor['count']} vertices"
    view = gltf["bufferViews"][pos_accessor["bufferView"]]
    offset = view.get("byteOffset", 0) + pos_accessor.get("byteOffset", 0)
    values = struct.unpack_from("<9f", binary, offset)
    xs = values[0::3]
    assert min(xs) > 1.49 and max(xs) > 2.49, (
        "GLB node transform was not flattened into embedded geometry: " + repr(values)
    )


def export_and_assert(exe, xml_path, out_path, expected_name):
    result = subprocess.run([exe, xml_path, out_path], capture_output=True, text=True)
    combined = result.stdout + result.stderr
    assert result.returncode == 0, f"export failed:\n{combined}"
    assert "warning:" not in combined.lower(), f"embed unexpectedly warned:\n{combined}"
    assert os.path.exists(out_path) and os.path.getsize(out_path) > 0, "missing GLB output"
    assert_flattened_triangle(out_path, expected_name)


def assert_viewport(meshcraft, xml_path, tmpdir):
    screenshot = os.path.join(tmpdir, "embedded-view.ppm")
    command = [meshcraft, xml_path, "--screenshot", screenshot]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        command = ["xvfb-run", "--auto-servernum"] + command
    result = subprocess.run(command, capture_output=True, text=True)
    combined = result.stdout + result.stderr
    assert result.returncode == 0, f"embedded viewport screenshot failed:\n{combined}"
    assert "failed to load embedded mesh" not in combined.lower(), (
        "viewport fell back to the placeholder instead of loading embed:\n" + combined
    )
    data = open(screenshot, "rb").read()
    assert data[:2] == b"P6", "embedded viewport screenshot was not PPM"
    payload_start = data.find(b"\n255\n")
    assert payload_start >= 0, "malformed PPM header"
    pixels = data[payload_start + 5:]
    # The dark default background cannot produce this many bright triangle pixels.
    bright = sum(1 for i in range(0, len(pixels), 3)
                 if pixels[i] > 80 or pixels[i + 1] > 80 or pixels[i + 2] > 80)
    assert bright > 100, f"embedded mesh was not visibly rendered ({bright} bright pixels)"


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        print(f"Usage: {sys.argv[0]} <mc3togltf> <unused-fixture-path> [MeshCraft]")
        sys.exit(1)

    with tempfile.TemporaryDirectory() as tmpdir:
        glb = make_glb()
        asset_path = os.path.join(tmpdir, "triangle.glb")
        with open(asset_path, "wb") as out:
            out.write(glb)

        external_xml = os.path.join(tmpdir, "external.mc3.xml")
        write_xml(external_xml, '    <embed type="gltf" id="triangle" src="triangle.glb"/>')
        export_and_assert(sys.argv[1], external_xml, os.path.join(tmpdir, "external-out.glb"), "EmbeddedTriangle")

        inline_xml = os.path.join(tmpdir, "inline.mc3.xml")
        encoded = base64.b64encode(glb).decode("ascii")
        write_xml(inline_xml, f'    <embed type="gltf" id="triangle"><![CDATA[{encoded}]]></embed>')
        export_and_assert(sys.argv[1], inline_xml, os.path.join(tmpdir, "inline-out.glb"), "EmbeddedTriangle")

        if len(sys.argv) == 4:
            viewport_xml = os.path.join(tmpdir, "viewport.mc3.xml")
            with open(viewport_xml, "w", encoding="utf-8") as out:
                out.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n")
                out.write("<mc3 version=\"0.3\" model=\"EmbedViewportTest\">\n")
                out.write("  <cameras default=\"Cam\"><camera name=\"Cam\" position=\"0 0 4\" target=\"0 0 0\" fov=\"50\"/></cameras>\n")
                out.write("  <materials><material id=\"white\"><base_color>0.9 0.9 0.9 1.0</base_color></material></materials>\n")
                out.write("  <embeds><embed type=\"gltf\" id=\"triangle\" src=\"triangle.glb\"/></embeds>\n")
                out.write("  <objects><mesh name=\"EmbeddedTriangle\" src=\"embed:triangle\" position=\"-2 0 0\" material=\"white\"/></objects>\n")
                out.write("</mc3>\n")
            assert_viewport(sys.argv[3], viewport_xml, tmpdir)

    print("Embedded external and inline GLB export: PASS")
