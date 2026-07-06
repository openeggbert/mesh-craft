#!/bin/bash
# STAB-0521: verify no GL state leak survives a full frame's render passes
# (SSAO/bloom/skybox/gizmos/ImGui). checkGlStateLeak() drains glGetError()
# once per frame and EndDraw()'s --screenshot path prints "[GLCheck] clean"
# or "[GLCheck] error" accordingly.
# Usage: gl_state_leak_test.sh <MeshCraft-binary> <scene.mc3.xml>
set -e

BINARY="$1"
SCENE="$2"
PPM=$(mktemp /tmp/meshcraft_glcheck_XXXXXX.ppm)
OUT=$(mktemp /tmp/meshcraft_glcheck_out_XXXXXX.txt)
trap 'rm -f "$PPM" "$OUT"' EXIT

if [ -z "$BINARY" ] || [ -z "$SCENE" ]; then
    echo "Usage: $0 <binary> <scene>"
    exit 1
fi

if command -v xvfb-run >/dev/null 2>&1; then
    xvfb-run --auto-servernum "$BINARY" "$SCENE" --screenshot "$PPM" > "$OUT" 2>&1
else
    "$BINARY" "$SCENE" --screenshot "$PPM" > "$OUT" 2>&1
fi

if ! grep -q '^\[GLCheck\] clean$' "$OUT"; then
    echo "FAIL: expected '[GLCheck] clean' in output, got:"
    cat "$OUT"
    exit 1
fi

echo "PASS: gl_state_leak_test — no leaked GL error after a full render frame"
