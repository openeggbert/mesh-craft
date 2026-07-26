#!/bin/bash
# SYS-W12-02: smoke test for `MeshCraft <scene> --benchmark`. Informational,
# same philosophy as test/benchmark.py (SYS-W12-01) and test/smoke_test.sh --
# checks the mode runs to completion and prints every expected category, not
# that any specific timing value is fast (wall-clock on a shared/virtualized
# machine is too noisy across runs for a hard threshold to be a real signal).
# Usage: benchmark_editor_test.sh <MeshCraft-binary> <scene.mc3.xml>
set -e

BINARY="$1"
SCENE="$2"
OUT=$(mktemp /tmp/meshcraft_benchmark_XXXXXX.log)
trap 'rm -f "$OUT"' EXIT

if [ -z "$BINARY" ] || [ -z "$SCENE" ]; then
    echo "Usage: $0 <binary> <scene>"
    exit 1
fi

if command -v xvfb-run >/dev/null 2>&1; then
    xvfb-run --auto-servernum "$BINARY" "$SCENE" --benchmark > "$OUT" 2>&1
else
    "$BINARY" "$SCENE" --benchmark > "$OUT" 2>&1
fi

fail=0
for category in "startup" "first frame" "warm frame" "mesh generation" \
                "CSG CPU evaluator" "texture decode/upload" "traversal" "picking" \
                "GPU buffers" "undo snapshot" "animation eval" "registry"; do
    if ! grep -q "\[Benchmark\] $category" "$OUT"; then
        echo "FAIL: missing expected '[Benchmark] $category' line"
        fail=1
    fi
done

# The last of ten benchmark frames is warm. Ordinary meshes must not rebuild
# tint buffers there. The 100-instance fixture has one repeated authored UV
# projection, so it also proves the mapped GPU buffer is reused rather than
# reallocated across a representative repeated-geometry scene.
if ! grep -Eq '\[Benchmark\] GPU buffers .*tint created 0.*authored UV created 0.*authored UV cache hits [1-9][0-9]*' "$OUT"; then
    echo "FAIL: warm GPU-buffer metrics did not show zero creations and an authored-UV cache hit"
    fail=1
fi

if [ "$fail" -ne 0 ]; then
    echo "--- full output ---"
    cat "$OUT"
    exit 1
fi

echo "PASS: benchmark_editor — all categories printed"
