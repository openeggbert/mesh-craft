#!/usr/bin/env python3
"""
AUD-029: obj.metadata (the opaque <metadata><property name=.. value=../></metadata>
pass-through store, mc3/include/MeshCraft/Mc3/Mc3Object.hpp:94) survives every
other mc3/mcb round-trip but was silently dropped on glTF export -- unlike
tags/collision, which are already preserved via node.extras. Asserts a
tagged object's metadata key/value pairs land in node.extras.metadata, and
that an object with no <metadata> gets no extras.metadata key at all (no
spurious empty object).

Usage: object_metadata_export_test.py <mc3togltf-binary> <object_metadata_export.mc3.xml>
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
        print("usage: object_metadata_export_test.py <mc3togltf-binary> <fixture.mc3.xml>", file=sys.stderr)
        sys.exit(2)
    binary, fixture = sys.argv[1], sys.argv[2]

    with tempfile.NamedTemporaryFile(suffix=".gltf", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, fixture, out], capture_output=True, text=True)
        check(r.returncode == 0, f"exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            return

        with open(out) as f:
            gltf = json.load(f)

        node_by_name = {n["name"]: n for n in gltf["nodes"] if "name" in n}
        check("TaggedCrate" in node_by_name, "node 'TaggedCrate' present")
        check("PlainCrate" in node_by_name, "node 'PlainCrate' present")
        if "TaggedCrate" not in node_by_name or "PlainCrate" not in node_by_name:
            return

        tagged = node_by_name["TaggedCrate"]
        extras = tagged.get("extras", {})
        meta = extras.get("metadata")
        check(meta is not None, "TaggedCrate: node.extras.metadata present")
        if meta is not None:
            check(meta.get("foo") == "bar",
                  f"TaggedCrate: metadata['foo']=='bar', got {meta.get('foo')!r}")
            check(meta.get("quest_id") == "17",
                  f"TaggedCrate: metadata['quest_id']=='17', got {meta.get('quest_id')!r}")
            check(len(meta) == 2,
                  f"TaggedCrate: exactly 2 metadata keys round-tripped, got {len(meta)}")

        plain = node_by_name["PlainCrate"]
        plain_extras = plain.get("extras", {})
        check("metadata" not in plain_extras,
              "PlainCrate: no <metadata> in source -> no extras.metadata key (not an empty object)")
    finally:
        if os.path.exists(out):
            os.unlink(out)

    print(f"\n{'All tests passed.' if failures == 0 else f'FAILURES: {failures}'}")
    sys.exit(1 if failures > 0 else 0)

if __name__ == "__main__":
    main()
