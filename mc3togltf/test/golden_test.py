#!/usr/bin/env python3
"""STAB-0050 -- golden-file test for the glTF exporter's JSON structure.

Exports mc3togltf/test/golden/basic_scene.mc3.xml (a fixed, dedicated
fixture -- do not reuse it for other tests) and compares the resulting
glTF JSON, parsed and re-serialized, against the committed golden file
mc3togltf/test/golden/basic_scene.gltf.golden.json.

The output filename is fixed (not a random tempfile name) because the
exporter embeds it in buffers[0].uri -- using a random name would make
every run "diff" against the golden file on that field alone. Any other
unintentional change to the exporter's JSON structure (attribute order
aside, since this compares parsed dicts, not raw bytes) shows up as a
real structural diff here.

Usage: golden_test.py <mc3togltf-binary>
"""
import json
import os
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

failures = 0


def check(cond, msg):
    global failures
    if cond:
        print(f"PASS: {msg}")
    else:
        print(f"FAIL: {msg}", file=sys.stderr)
        failures += 1


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf-binary>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    fixture = os.path.join(SCRIPT_DIR, "golden", "basic_scene.mc3.xml")
    golden_path = os.path.join(SCRIPT_DIR, "golden", "basic_scene.gltf.golden.json")

    with open(golden_path) as f:
        golden = json.load(f)

    with tempfile.TemporaryDirectory() as tmpdir:
        # Fixed filename (not tempfile-random) -- see module docstring.
        out = os.path.join(tmpdir, "basic_scene.gltf")
        r = subprocess.run([mc3togltf, fixture, out], capture_output=True, text=True)
        check(r.returncode == 0, f"mc3togltf exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            sys.exit(1)

        with open(out) as f:
            produced = json.load(f)

    check(produced == golden,
          "golden: exported glTF JSON structurally matches committed golden/basic_scene.gltf.golden.json")
    if produced != golden:
        print("--- produced ---", file=sys.stderr)
        print(json.dumps(produced, indent=2, sort_keys=True), file=sys.stderr)
        print("--- golden ---", file=sys.stderr)
        print(json.dumps(golden, indent=2, sort_keys=True), file=sys.stderr)

    sys.exit(1 if failures else 0)
