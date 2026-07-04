#!/bin/bash
# STAB-0499: load a scene with a <mesh src="..."> pointing to a nonexistent
# file, take a screenshot, verify no crash AND that a warning is printed
# (not silently swallowed).
# Usage: missing_mesh_test.sh <MeshCraft-binary> <missing_mesh.mc3.xml>
set -e

BINARY="$1"
SCENE="$2"
PPM=$(mktemp /tmp/meshcraft_missing_mesh_XXXXXX.ppm)
LOG=$(mktemp /tmp/meshcraft_missing_mesh_XXXXXX.log)
trap 'rm -f "$PPM" "$LOG"' EXIT

if [ -z "$BINARY" ] || [ -z "$SCENE" ]; then
    echo "Usage: $0 <binary> <scene>"
    exit 1
fi

if command -v xvfb-run >/dev/null 2>&1; then
    xvfb-run --auto-servernum "$BINARY" "$SCENE" --screenshot "$PPM" > "$LOG" 2>&1
else
    "$BINARY" "$SCENE" --screenshot "$PPM" > "$LOG" 2>&1
fi

if [ ! -s "$PPM" ]; then
    echo "FAIL: screenshot PPM is empty or was not created"
    cat "$LOG"
    exit 1
fi

if ! grep -q "failed to load mesh" "$LOG"; then
    echo "FAIL: expected a 'failed to load mesh' warning, found none:"
    cat "$LOG"
    exit 1
fi

echo "PASS: missing_mesh_test — no crash, warning printed, $(wc -c < "$PPM") bytes"
