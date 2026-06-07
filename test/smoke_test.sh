#!/bin/bash
# Smoke test: load a scene, take a screenshot, verify the PPM is non-empty.
# Usage: smoke_test.sh <MeshCraft-binary> <scene.mc3.xml>
set -e

BINARY="$1"
SCENE="$2"
PPM=$(mktemp /tmp/meshcraft_smoke_XXXXXX.ppm)
trap 'rm -f "$PPM"' EXIT

if [ -z "$BINARY" ] || [ -z "$SCENE" ]; then
    echo "Usage: $0 <binary> <scene>"
    exit 1
fi

if command -v xvfb-run >/dev/null 2>&1; then
    xvfb-run --auto-servernum "$BINARY" "$SCENE" --screenshot "$PPM"
else
    "$BINARY" "$SCENE" --screenshot "$PPM"
fi

if [ ! -s "$PPM" ]; then
    echo "FAIL: screenshot PPM is empty or was not created"
    exit 1
fi

echo "PASS: smoke_test — $(wc -c < "$PPM") bytes"
