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

# Absolute texture path — would exfiltrate /etc/passwd bytes into the GLB.
ABSOLUTE_TEXTURE = """<mc3 version="0.3" model="AbsTex">
  <textures>
    <texture id="t" name="t" uri="/etc/passwd"/>
  </textures>
  <objects><box name="b" material="m"/></objects>
  <materials><material id="m" name="m" base_color_texture="t"/></materials>
</mc3>
"""

# Relative traversal escaping the document root.
TRAVERSAL_TEXTURE = """<mc3 version="0.3" model="TravTex">
  <textures>
    <texture id="t" name="t" uri="../../../../../../etc/passwd"/>
  </textures>
  <objects><box name="b" material="m"/></objects>
  <materials><material id="m" name="m" base_color_texture="t"/></materials>
</mc3>
"""


def check_rejected(mc3togltf, label, xml, extra_args=None):
    with tempfile.TemporaryDirectory() as tmpdir:
        src = os.path.join(tmpdir, "in.mc3.xml")
        with open(src, "w") as f:
            f.write(xml)
        out = os.path.join(tmpdir, "out.glb")
        try:
            r = run([mc3togltf] + (extra_args or []) + [src, out])
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


def check_flag_allows_past_confinement(mc3togltf, label, xml):
    """With --allow-external-resources the path check no longer fires; the
    export may still fail because the file is genuinely absent, but the error
    must NOT be the confinement rejection."""
    with tempfile.TemporaryDirectory() as tmpdir:
        src = os.path.join(tmpdir, "in.mc3.xml")
        with open(src, "w") as f:
            f.write(xml)
        out = os.path.join(tmpdir, "out.glb")
        r = run([mc3togltf, "--allow-external-resources", src, out])
        confinement = ("refusing to read" in r.stderr) or ("escapes the document root" in r.stderr)
        if confinement:
            print(f"FAIL ({label}): --allow-external-resources still hit confinement: "
                  f"{r.stderr.strip()!r}")
            return False
        print(f"PASS ({label}): --allow-external-resources bypasses the path check")
        return True


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf>")
        sys.exit(1)
    mc3togltf = sys.argv[1]

    results = [
        check_rejected(mc3togltf, "cyclic-instance", CYCLIC_INSTANCE),
        check_rejected(mc3togltf, "degenerate-helix", DEGENERATE_HELIX),
        # Resource-path confinement is on by default: these must be rejected.
        check_rejected(mc3togltf, "absolute-texture", ABSOLUTE_TEXTURE),
        check_rejected(mc3togltf, "traversal-texture", TRAVERSAL_TEXTURE),
        # ...and the opt-out flag must bypass the path check.
        check_flag_allows_past_confinement(mc3togltf, "absolute-texture+flag", ABSOLUTE_TEXTURE),
    ]
    sys.exit(0 if all(results) else 1)
