#!/usr/bin/env python3
"""Validate mc3.xml test files against mc3.xsd using lxml."""

import sys
import os
from lxml import etree

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <schema.xsd> <file1.xml> [file2.xml ...]")
        sys.exit(1)

    xsd_path = sys.argv[1]
    xml_paths = sys.argv[2:]

    with open(xsd_path, "rb") as f:
        schema_doc = etree.parse(f)
    schema = etree.XMLSchema(schema_doc)

    failures = 0
    for xml_path in xml_paths:
        with open(xml_path, "rb") as f:
            doc = etree.parse(f)
        if schema.validate(doc):
            print(f"PASS: {os.path.basename(xml_path)}")
        else:
            print(f"FAIL: {os.path.basename(xml_path)}")
            for err in schema.error_log:
                print(f"  line {err.line}: {err.message}")
            failures += 1

    if failures:
        print(f"\n{failures}/{len(xml_paths)} files failed validation.")
        sys.exit(1)
    else:
        print(f"\nAll {len(xml_paths)} files valid.")

if __name__ == "__main__":
    main()
