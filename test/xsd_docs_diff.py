#!/usr/bin/env python3
"""Diagnostic tool (not a pass/fail test): report mc3.xsd element/attribute
names that are never mentioned anywhere in MC3_FORMAT.md, so a human can
review whether each gap is a real documentation hole or an intentional
simplification (internal-only attribute, XSD plumbing, etc.).

Usage: python3 test/xsd_docs_diff.py [mc3.xsd] [MC3_FORMAT.md]
Exits 0 always (informational) -- this is a report generator, not a gate.
"""
import re
import sys
from pathlib import Path

DEFAULT_XSD = Path(__file__).resolve().parent.parent / "mc3" / "mc3.xsd"
DEFAULT_DOC = Path(__file__).resolve().parent.parent / "MC3_FORMAT.md"


def extract_names(xsd_text, tag):
    return sorted(set(re.findall(rf'<xs:{tag}\b[^>]*\bname="([a-zA-Z0-9_]+)"', xsd_text)))


def main():
    xsd_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_XSD
    doc_path = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_DOC

    xsd_text = xsd_path.read_text()
    doc_text = doc_path.read_text()
    doc_lower = doc_text.lower()

    elements = extract_names(xsd_text, "element")
    attributes = extract_names(xsd_text, "attribute")

    def missing(names):
        # Word-boundary-ish check: name appears literally somewhere in the doc
        # (as prose, a heading, a code span, or an attribute example).
        out = []
        for n in names:
            if re.search(rf'\b{re.escape(n)}\b', doc_text) is None and \
               re.search(rf'\b{re.escape(n.lower())}\b', doc_lower) is None:
                out.append(n)
        return out

    missing_elements = missing(elements)
    missing_attributes = missing(attributes)

    print(f"XSD elements:   {len(elements)} total, {len(missing_elements)} not mentioned in {doc_path.name}")
    for n in missing_elements:
        print(f"  MISSING element:   {n}")
    print(f"XSD attributes: {len(attributes)} total, {len(missing_attributes)} not mentioned in {doc_path.name}")
    for n in missing_attributes:
        print(f"  MISSING attribute: {n}")

    if not missing_elements and not missing_attributes:
        print("\nNo gaps found -- every XSD element/attribute name appears somewhere in the doc.")

    print("\nNote: presence-check only (name appears somewhere in the doc text) -- "
          "does not verify the surrounding prose is accurate or complete. "
          "Review each MISSING line manually before treating it as a real gap; "
          "some XSD-internal names (type names reused as element names, etc.) "
          "may be intentionally omitted from prose documentation.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
