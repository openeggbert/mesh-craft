#!/usr/bin/env python3
"""
STAB-0440: an SVG-sourced texture referenced by a material is not rasterized
(no code path reads Mc3Document::svgTextures at all) — the export must still
succeed (exit 0), but must now print a warning naming the unresolved SVG
texture, rather than silently dropping it.
"""
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <svg_material.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, (
            f"Expected export to succeed despite the unresolvable SVG texture, "
            f"got returncode={r.returncode}\nstderr: {r.stderr}"
        )
        assert os.path.exists(out) and os.path.getsize(out) > 0, \
            "Output glTF is missing or empty"

        combined = r.stdout + r.stderr
        assert "SVG" in combined and "logo" in combined, (
            f"Expected a warning naming the unresolved SVG texture 'logo', got:\n{combined}"
        )
        print(f"SVG texture export warning: PASS ({combined.strip()!r})")

    print("\nSVG texture export test: PASS")
