#!/usr/bin/env python3
"""STAB-0056 -- mc3togltf produces bit-identical GLB output across repeated
runs on the same input.

Runs mc3togltf twice on the same fixture, into two separate output files,
and asserts the two .glb files are byte-for-byte identical. This is a
different, stronger claim than instance_variant_test.py's narrower
"variant resolution index is stable" check or glb_texture_embed_test.py's
"embedded bytes match the source image" check -- this compares the
*entire* output file across two independent process invocations.

Usage: determinism_test.py <mc3togltf-binary> <fixture.mc3.xml>
"""
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <fixture.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    fixture = sys.argv[2]
    failures = 0

    with tempfile.TemporaryDirectory() as tmpdir:
        out_a = os.path.join(tmpdir, "a.glb")
        out_b = os.path.join(tmpdir, "b.glb")

        r1 = run([mc3togltf, fixture, out_a])
        if r1.returncode != 0:
            print(f"FAIL: first mc3togltf run exited {r1.returncode}: {r1.stderr.strip()}",
                  file=sys.stderr)
            sys.exit(1)
        print("PASS: first mc3togltf run exits 0")

        r2 = run([mc3togltf, fixture, out_b])
        if r2.returncode != 0:
            print(f"FAIL: second mc3togltf run exited {r2.returncode}: {r2.stderr.strip()}",
                  file=sys.stderr)
            sys.exit(1)
        print("PASS: second mc3togltf run exits 0")

        with open(out_a, "rb") as f:
            data_a = f.read()
        with open(out_b, "rb") as f:
            data_b = f.read()

        if len(data_a) == 0:
            print("FAIL: first run produced an empty output file", file=sys.stderr)
            failures += 1
        else:
            print(f"PASS: first run produced non-empty output ({len(data_a)} bytes)")

        if data_a == data_b:
            print(f"PASS: determinism: two independent runs produce byte-identical "
                  f"output ({len(data_a)} bytes)")
        else:
            print(f"FAIL: determinism: outputs differ (first {len(data_a)} bytes, "
                  f"second {len(data_b)} bytes)", file=sys.stderr)
            failures += 1

    sys.exit(1 if failures else 0)
