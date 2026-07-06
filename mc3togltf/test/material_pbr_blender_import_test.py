#!/usr/bin/env python3
"""
STAB-0431: verify a GLB exported from an mc3 scene with one PBR material
imports into Blender with matching Base Color / Roughness / Metallic values —
a real headless Blender import, not just a structural glTF-JSON check (the
existing mc3togltf_material_pbr test already checks the glTF JSON directly;
this one confirms an actual DCC tool reads the same numbers back out).

Only registered as a ctest when a `blender` executable is found at configure
time (find_program) -- same optional-tooling gate as mc3togltf_blender_import.
"""
import json
import os
import subprocess
import sys
import tempfile


BASE_COLOR = (0.8, 0.2, 0.1, 1.0)
ROUGHNESS  = 0.3
METALLIC   = 0.7
TOLERANCE  = 0.02


BLENDER_HELPER_SCRIPT = """
import bpy
import sys
import json

glb_path = sys.argv[sys.argv.index("--") + 1]
report_path = sys.argv[sys.argv.index("--") + 2]

bpy.ops.wm.read_factory_settings(use_empty=True)
result = bpy.ops.import_scene.gltf(filepath=glb_path)

mesh_objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
report = {"result": list(result), "mesh_count": len(mesh_objects)}

if mesh_objects and mesh_objects[0].data.materials:
    mat = mesh_objects[0].data.materials[0]
    report["material_name"] = mat.name
    bsdf = None
    if mat.use_nodes:
        for node in mat.node_tree.nodes:
            if node.type == 'BSDF_PRINCIPLED':
                bsdf = node
                break
    if bsdf:
        report["base_color"] = list(bsdf.inputs["Base Color"].default_value)
        report["roughness"]  = bsdf.inputs["Roughness"].default_value
        report["metallic"]   = bsdf.inputs["Metallic"].default_value

with open(report_path, "w") as f:
    json.dump(report, f)
"""


def generate_xml():
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<mc3 version="0.3" model="MaterialBlenderImportTest">\n'
        '  <materials>\n'
        f'    <material id="testmat" roughness="{ROUGHNESS}" metallic="{METALLIC}">\n'
        f'      <base_color>{BASE_COLOR[0]} {BASE_COLOR[1]} {BASE_COLOR[2]} {BASE_COLOR[3]}</base_color>\n'
        '    </material>\n'
        '  </materials>\n'
        '  <objects>\n'
        '    <box name="TestBox" size="1 1 1" material="testmat"/>\n'
        '  </objects>\n'
        '</mc3>\n'
    )


def run(cmd, timeout=60):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <blender>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    blender   = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        xml_path = os.path.join(tmpdir, "scene.mc3.xml")
        with open(xml_path, "w") as f:
            f.write(generate_xml())

        glb_path = os.path.join(tmpdir, "scene.glb")
        r = run([mc3togltf, xml_path, glb_path])
        assert r.returncode == 0, f"Export failed (returncode={r.returncode}):\n{r.stderr}"

        helper_path = os.path.join(tmpdir, "blender_helper.py")
        with open(helper_path, "w") as f:
            f.write(BLENDER_HELPER_SCRIPT)

        report_path = os.path.join(tmpdir, "report.json")
        r = run([blender, "--background", "--python", helper_path, "--", glb_path, report_path],
                timeout=60)
        assert r.returncode == 0, (
            f"Blender import script failed (returncode={r.returncode}):\n"
            f"stdout={r.stdout}\nstderr={r.stderr}"
        )
        assert os.path.exists(report_path), (
            f"Blender did not produce a report file -- likely crashed or the "
            f"import script errored before writing it.\nstdout={r.stdout}\nstderr={r.stderr}"
        )

        with open(report_path) as f:
            report = json.load(f)

        assert report["result"] == ["FINISHED"], (
            f"Expected Blender's glTF import operator to report FINISHED, got {report['result']}"
        )
        assert report["mesh_count"] == 1, f"Expected 1 mesh object, got {report['mesh_count']}"
        assert "base_color" in report, (
            f"No Principled BSDF material found after import: {report}"
        )

        bc = report["base_color"]
        for i, expected in enumerate(BASE_COLOR):
            assert abs(bc[i] - expected) < TOLERANCE, (
                f"base_color[{i}]={bc[i]} does not match expected {expected} "
                f"(full report: {report})"
            )
        assert abs(report["roughness"] - ROUGHNESS) < TOLERANCE, (
            f"roughness={report['roughness']} does not match expected {ROUGHNESS}"
        )
        assert abs(report["metallic"] - METALLIC) < TOLERANCE, (
            f"metallic={report['metallic']} does not match expected {METALLIC}"
        )

        print(f"STAB-0431: PBR material (base_color={BASE_COLOR}, roughness={ROUGHNESS}, "
              f"metallic={METALLIC}) survives export -> Blender import with matching values — PASS")


if __name__ == "__main__":
    main()
