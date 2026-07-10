#!/usr/bin/env python3
"""
STAB-0686: mc3 only enforces id uniqueness, not name uniqueness, but
Mc3Channel::targetObject targets animation by NAME -- two same-named
sibling nodes silently collided in mc3togltf's nodeNameMap (last write
wins), with the earlier node left silently unanimated and no warning.

Usage: duplicate_object_name_test.py <mc3togltf-binary> <duplicate_object_name_anim.mc3.xml>
"""
import json
import os
import subprocess
import sys
import tempfile

failures = 0

def check(cond, msg):
    global failures
    if cond:
        print(f"PASS: {msg}")
    else:
        print(f"FAIL: {msg}", file=sys.stderr)
        failures += 1

def main():
    if len(sys.argv) != 3:
        print("usage: duplicate_object_name_test.py <mc3togltf-binary> <fixture.mc3.xml>", file=sys.stderr)
        sys.exit(2)
    binary, fixture = sys.argv[1], sys.argv[2]

    with tempfile.NamedTemporaryFile(suffix=".gltf", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, fixture, out], capture_output=True, text=True)
        check(r.returncode == 0, f"exits 0 despite the duplicate name (stderr: {r.stderr.strip()!r})")

        check("duplicate node name" in r.stderr.lower() and "Dup" in r.stderr,
              "a warning naming the duplicate ('Dup') is printed to stderr")

        with open(out) as f:
            g = json.load(f)
        dup_nodes = [n for n in g.get("nodes", []) if n.get("name") == "Dup"]
        check(len(dup_nodes) == 2,
              f"both same-named nodes are still present in the output (found {len(dup_nodes)})")

    finally:
        if os.path.exists(out):
            os.unlink(out)

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll duplicate-object-name checks passed.")

if __name__ == "__main__":
    main()
