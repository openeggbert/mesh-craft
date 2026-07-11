#!/usr/bin/env python3
"""mc3togltf rejects hostile/degenerate geometry instead of crashing or
emitting a spec-invalid glTF reported as success.

Two cases:

  1. Cyclic <instance>: a definition that contains an instance of itself makes
     GltfExporter::buildNode recurse forever. Without the depth cap this is a
     stack-overflow crash. With it, the tool must exit non-zero with a clear
     message and write no output.

  2. Degenerate helix (radius=0): the parametric helix tangent divides by
     (radius * totalAngle), so radius=0 produces NaN vertex positions. Those
     used to be written to the glTF (with NaN accessor min/max) while the tool
     reported "Written:" success. The exporter now validates finiteness and must
     fail loudly instead.

Usage: hostile_geometry_test.py <mc3togltf>
"""
import os
import subprocess
import sys
import tempfile


def run(cmd):
    # Guard against a real hang/inf-loop regression with a timeout.
    return subprocess.run(cmd, capture_output=True, text=True, timeout=30)


CYCLIC_INSTANCE = """<mc3 version="0.3" model="CyclicInstance">
  <definitions>
    <definition id="loop">
      <group name="g">
        <instance name="self" definition="loop"/>
      </group>
    </definition>
  </definitions>
  <objects>
    <instance name="root" definition="loop"/>
  </objects>
</mc3>
"""

DEGENERATE_HELIX = """<mc3 version="0.3" model="DegenerateHelix">
  <objects>
    <extrude name="h">
      <cross_section type="circle" radius="0.1"/>
      <path type="helix" radius="0" height="2" turns="4"/>
    </extrude>
  </objects>
</mc3>
"""


def check_rejected(mc3togltf, label, xml):
    with tempfile.TemporaryDirectory() as tmpdir:
        src = os.path.join(tmpdir, "in.mc3.xml")
        with open(src, "w") as f:
            f.write(xml)
        out = os.path.join(tmpdir, "out.glb")
        try:
            r = run([mc3togltf, src, out])
        except subprocess.TimeoutExpired:
            print(f"FAIL ({label}): mc3togltf hung (likely unbounded recursion)")
            return False
        ok = True
        if r.returncode == 0:
            print(f"FAIL ({label}): expected non-zero exit, got 0. "
                  f"stdout={r.stdout!r} stderr={r.stderr!r}")
            ok = False
        if not r.stderr.strip():
            print(f"FAIL ({label}): expected an error message on stderr")
            ok = False
        if os.path.exists(out):
            print(f"FAIL ({label}): a glTF was written despite the error")
            ok = False
        if ok:
            print(f"PASS ({label}): rejected -- exit {r.returncode}, "
                  f"msg={r.stderr.strip().splitlines()[-1][:80]!r}")
        return ok


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf>")
        sys.exit(1)
    mc3togltf = sys.argv[1]

    results = [
        check_rejected(mc3togltf, "cyclic-instance", CYCLIC_INSTANCE),
        check_rejected(mc3togltf, "degenerate-helix", DEGENERATE_HELIX),
    ]
    sys.exit(0 if all(results) else 1)
