#!/usr/bin/env python3
"""STAB-0058 — mc3tomcb CLI binary round-trip test.

Drives the ``mc3tomcb`` executable through a full

    fixture.mc3.xml --> a.mcb --> b.mc3.xml --> c.mcb --> d.mc3.xml

pipeline for each fixture and asserts:

  1. **Header** — the produced ``.mcb`` stream starts with the MCB magic
     ``b"MCB\\0"``.
  2. **Fixpoint / determinism** — the second MCB blob is byte-identical to the
     first (``a.mcb == c.mcb``) and the second canonical XML is identical to the
     first (``b.mc3.xml == d.mc3.xml``). This proves the CLI round-trip is
     deterministic and lossless on the canonical form.
  3. **No content dropped** — the multiset of element tag names in the
     round-tripped XML matches the source, and the root ``model`` attribute is
     unchanged. Bezier keyframe handle tags are excluded because the XML writer
     canonicalises them on save (a non-MCB normalisation that also happens on a
     plain load+save), so comparing them against the hand-written source would
     yield false positives.

In-memory field-level fidelity of every N1–N7 type is already covered by the
C++ ``mcb_roundtrip`` test; this test covers the CLI wiring end to end.

Usage:
    mc3tomcb_roundtrip_test.py <mc3tomcb-exe> <fixture.mc3.xml> [more fixtures...]
"""

import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from collections import Counter

# Tags the XML writer may add/normalise on save (Bezier handles). Excluded from
# the structural fidelity comparison — see the module docstring.
CANONICALISED_TAGS = {"handle_left", "handle_right"}

MCB_MAGIC = b"MCB\x00"


def lname(tag):
    """Local tag name without any XML namespace prefix."""
    return tag.rsplit("}", 1)[-1]


def parse_tags(path):
    """Return (root_element, Counter of element tag names sans canonicalised)."""
    root = ET.parse(path).getroot()
    counts = Counter(lname(e.tag) for e in root.iter())
    for tag in CANONICALISED_TAGS:
        counts.pop(tag, None)
    return root, counts


def convert(exe, src, dst):
    """Run one mc3tomcb conversion; raise on failure or empty output."""
    result = subprocess.run([exe, src, dst], capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"mc3tomcb {os.path.basename(src)} -> {os.path.basename(dst)} "
            f"exited {result.returncode}: {result.stderr.strip()}"
        )
    if not os.path.exists(dst) or os.path.getsize(dst) == 0:
        raise RuntimeError(
            f"mc3tomcb produced no/empty output: {os.path.basename(dst)}"
        )


def check_fixture(exe, fixture, workdir):
    """Run the four-step round-trip for one fixture; return a list of errors."""
    base = os.path.splitext(os.path.basename(fixture))[0]
    a_mcb = os.path.join(workdir, base + ".a.mcb")
    b_xml = os.path.join(workdir, base + ".b.mc3.xml")
    c_mcb = os.path.join(workdir, base + ".c.mcb")
    d_xml = os.path.join(workdir, base + ".d.mc3.xml")

    convert(exe, fixture, a_mcb)  # XML -> MCB
    convert(exe, a_mcb, b_xml)    # MCB -> XML
    convert(exe, b_xml, c_mcb)    # XML -> MCB
    convert(exe, c_mcb, d_xml)    # MCB -> XML

    errors = []

    # 1. MCB header.
    with open(a_mcb, "rb") as fh:
        magic = fh.read(len(MCB_MAGIC))
    if magic != MCB_MAGIC:
        errors.append(f"bad MCB magic header: {magic!r}")

    # 2. Fixpoint / determinism.
    with open(a_mcb, "rb") as f1, open(c_mcb, "rb") as f2:
        if f1.read() != f2.read():
            errors.append("MCB not byte-stable across round-trip (a.mcb != c.mcb)")
    with open(b_xml, encoding="utf-8") as f1, open(d_xml, encoding="utf-8") as f2:
        if f1.read() != f2.read():
            errors.append("canonical XML not stable across round-trip (b != d)")

    # 3. No scene content dropped.
    src_root, src_tags = parse_tags(fixture)
    rt_root, rt_tags = parse_tags(b_xml)
    if src_root.attrib.get("model") != rt_root.attrib.get("model"):
        errors.append(
            f"model attribute changed: {src_root.attrib.get('model')!r} -> "
            f"{rt_root.attrib.get('model')!r}"
        )
    if src_tags != rt_tags:
        changed = {
            t: (src_tags.get(t, 0), rt_tags.get(t, 0))
            for t in set(src_tags) | set(rt_tags)
            if src_tags.get(t, 0) != rt_tags.get(t, 0)
        }
        errors.append(f"element tag multiset changed (src,rt): {changed}")

    return errors


def main(argv):
    if len(argv) < 3:
        print(
            "usage: mc3tomcb_roundtrip_test.py <mc3tomcb-exe> "
            "<fixture.mc3.xml> [more fixtures...]",
            file=sys.stderr,
        )
        return 2

    exe = argv[1]
    fixtures = argv[2:]
    failures = 0

    with tempfile.TemporaryDirectory(prefix="mc3tomcb_rt_") as workdir:
        for fixture in fixtures:
            name = os.path.basename(fixture)
            try:
                errors = check_fixture(exe, fixture, workdir)
            except (RuntimeError, ET.ParseError) as exc:
                errors = [str(exc)]
            if errors:
                failures += 1
                print(f"FAIL: {name}")
                for err in errors:
                    print(f"    - {err}")
            else:
                print(f"PASS: {name}")

    if failures:
        print(f"{failures} mc3tomcb round-trip fixture(s) FAILED.")
        return 1
    print(f"All {len(fixtures)} mc3tomcb round-trip fixtures passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
