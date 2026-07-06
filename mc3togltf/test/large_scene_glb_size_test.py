#!/usr/bin/env python3
"""
STAB-0251: exported GLB file size stays proportional to *unique* geometry,
not object count -- i.e. mesh reuse actually shrinks the file, it isn't
just a mesh-index optimization that doesn't affect the binary payload.

Generates the same 3-shape-group scene as large_scene_generated_test.py at
two scales (200 objects, 500 objects -- a 2.5x object-count increase) and
confirms the GLB file-size ratio is measurably *less* than the object-count
ratio. If geometry reuse were broken (each object got its own copy of the
mesh data), file size would grow close to linearly with object count (ratio
~= 2.5x); with real reuse, growth is dominated by the fixed per-node JSON
overhead only, since all objects in each group still share one mesh.
"""
import os
import subprocess
import sys
import tempfile


def generate_xml(n_instances, n_spheres, n_boxes):
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<mc3 version="0.3" model="GlbSizeTest">',
        '  <definitions>',
        '    <definition id="unit_box">',
        '      <box name="UnitBox" size="1 1 1"/>',
        '    </definition>',
        '  </definitions>',
        '  <objects>',
    ]
    for i in range(n_instances):
        lines.append(f'    <instance name="Inst_{i:04d}" definition="unit_box" position="{i * 1.5:.1f} 0 0"/>')
    for i in range(n_spheres):
        lines.append(f'    <sphere name="Sph_{i:04d}" radius="0.5" segments="8" position="{i * 1.5:.1f} 5 0"/>')
    for i in range(n_boxes):
        lines.append(f'    <box name="BigBox_{i:04d}" size="2 2 2" position="{i * 2.5:.1f} 0 10"/>')
    lines += ['  </objects>', '</mc3>']
    return '\n'.join(lines)


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def export_glb_size(mc3togltf, tmpdir, scale, n_instances, n_spheres, n_boxes):
    xml_path = os.path.join(tmpdir, f"scene_{scale}.mc3.xml")
    with open(xml_path, "w") as f:
        f.write(generate_xml(n_instances, n_spheres, n_boxes))
    out = os.path.join(tmpdir, f"scene_{scale}.glb")
    r = run([mc3togltf, xml_path, out])
    assert r.returncode == 0, f"Export failed for scale {scale} (returncode={r.returncode}):\n{r.stderr}"
    return os.path.getsize(out)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf>")
        sys.exit(1)

    mc3togltf = sys.argv[1]

    with tempfile.TemporaryDirectory() as tmpdir:
        size_200 = export_glb_size(mc3togltf, tmpdir, 200, 100, 50, 50)
        size_500 = export_glb_size(mc3togltf, tmpdir, 500, 250, 125, 125)

        object_count_ratio = 500.0 / 200.0  # 2.5x
        size_ratio = size_500 / size_200

        assert size_ratio < object_count_ratio * 0.9, (
            f"STAB-0251: GLB size grew {size_ratio:.2f}x for a {object_count_ratio:.1f}x "
            f"object-count increase ({size_200} -> {size_500} bytes) -- expected "
            f"measurably sub-linear growth (< {object_count_ratio * 0.9:.2f}x), proving "
            f"geometry reuse actually shrinks the binary payload, not just mesh indices"
        )
        print(f"STAB-0251: GLB size grew {size_ratio:.2f}x ({size_200}->{size_500} bytes) "
              f"for a {object_count_ratio:.1f}x object-count increase -- sub-linear, "
              f"reuse confirmed — PASS")

    print("\nGLB file size proportionality test: PASS")
