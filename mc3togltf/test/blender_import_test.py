#!/usr/bin/env python3
"""
STAB-0252: verify a generated large scene's exported GLB actually imports
cleanly into Blender — a real headless Blender import, not just a
structural glTF-JSON check.

Generates the same 200-object scene as large_scene_generated_test.py,
exports it to GLB, then runs `blender --background --python <helper>` to
import it and report the resulting object/mesh count. Confirms:
  - the glTF import operator reports FINISHED (no error/cancel)
  - all 200 objects come through as real mesh objects (none dropped,
    none imported as some other/broken type)

Only registered as a ctest when a `blender` executable is found at
configure time (find_program) — this environment happens to have one,
but this is optional external tooling, not a build requirement.
"""
import json
import os
import subprocess
import sys
import tempfile


N_INSTANCES = 100
N_SPHERES   = 50
N_BOXES     = 50


BLENDER_HELPER_SCRIPT = """
import bpy
import sys
import json

glb_path = sys.argv[sys.argv.index("--") + 1]
report_path = sys.argv[sys.argv.index("--") + 2]

bpy.ops.wm.read_factory_settings(use_empty=True)
result = bpy.ops.import_scene.gltf(filepath=glb_path)

objects = bpy.context.scene.objects
mesh_objects = [o for o in objects if o.type == 'MESH']

report = {
    "result": list(result),
    "total_objects": len(objects),
    "mesh_objects": len(mesh_objects),
}
with open(report_path, "w") as f:
    json.dump(report, f)
"""


def generate_xml():
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<mc3 version="0.3" model="BlenderImportTest">',
        '  <definitions>',
        '    <definition id="unit_box">',
        '      <box name="UnitBox" size="1 1 1"/>',
        '    </definition>',
        '  </definitions>',
        '  <objects>',
    ]
    for i in range(N_INSTANCES):
        lines.append(f'    <instance name="Inst_{i:04d}" definition="unit_box" position="{i * 1.5:.1f} 0 0"/>')
    for i in range(N_SPHERES):
        lines.append(f'    <sphere name="Sph_{i:04d}" radius="0.5" segments="8" position="{i * 1.5:.1f} 5 0"/>')
    for i in range(N_BOXES):
        lines.append(f'    <box name="BigBox_{i:04d}" size="2 2 2" position="{i * 2.5:.1f} 0 10"/>')
    lines += ['  </objects>', '</mc3>']
    return '\n'.join(lines)


def run(cmd, timeout=60):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <blender>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    blender   = sys.argv[2]
    total = N_INSTANCES + N_SPHERES + N_BOXES

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
            f"Expected Blender's glTF import operator to report FINISHED, "
            f"got {report['result']}"
        )
        assert report["total_objects"] == total, (
            f"Expected {total} objects in Blender after import, got {report['total_objects']}"
        )
        assert report["mesh_objects"] == total, (
            f"Expected all {total} objects to import as real mesh objects, "
            f"got {report['mesh_objects']} mesh objects out of {report['total_objects']} total"
        )

        print(f"STAB-0252: {total}-object GLB imports cleanly into Blender "
              f"({report['mesh_objects']} mesh objects, import result FINISHED) — PASS")

    print("\nBlender import test: PASS")
