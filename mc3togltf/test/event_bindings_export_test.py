#!/usr/bin/env python3
"""SYS-W14-31: MC3 event bindings are explicitly warned-and-omitted in glTF."""

import json
import os
import subprocess
import sys
import tempfile


def check(condition, message):
    if condition:
        print(f"PASS: {message}")
        return 0
    print(f"FAIL: {message}", file=sys.stderr)
    return 1


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf-binary>", file=sys.stderr)
        return 2
    binary = sys.argv[1]
    fixture = """<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<mc3 version=\"0.3\" model=\"EventBindingsExport\">
  <triggers><trigger id=\"open_door\"/></triggers>
  <event-bindings>
    <binding id=\"door_enter\" source=\"door_area\" event=\"enter\"
             target_type=\"trigger\" target=\"open_door\" cooldown=\"0.25\"/>
  </event-bindings>
  <objects><area id=\"door_area\" name=\"Door Area\" size=\"2 2 2\"/></objects>
</mc3>
"""
    failures = 0
    with tempfile.TemporaryDirectory() as tmpdir:
        source = os.path.join(tmpdir, "events.mc3.xml")
        output = os.path.join(tmpdir, "events.gltf")
        with open(source, "w", encoding="utf-8") as f:
            f.write(fixture)
        result = subprocess.run([binary, source, output], capture_output=True, text=True)
        combined = result.stdout + result.stderr
        failures += check(result.returncode == 0, "export succeeds")
        failures += check("event binding" in combined.lower() and "omitted" in combined.lower(),
                          "export prints explicit event-binding omission warning "
                          f"(output: {combined.strip()!r})")
        with open(output, encoding="utf-8") as f:
            gltf = json.load(f)
        encoded = json.dumps(gltf)
        failures += check("eventBindings" not in encoded and "event-bindings" not in encoded and
                          "open_door" not in encoded,
                          "glTF contains no MC3 event-binding runtime payload")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
