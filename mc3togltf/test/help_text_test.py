#!/usr/bin/env python3
"""STAB-0547 -- mc3togltf --help / -h shows accurate CLI flag documentation.

Before this fix, mc3togltf had no --help/-h handling at all: `mc3togltf
--help` was parsed as a *positional input filename* ("--help"), which
doesn't exist on disk, so the tool printed "Error: input file not found:
--help" and exited 1 -- never showing usage text, contradicting this
row's own verification method. Fixed by recognizing --help/-h before the
positional-argument fallback, printing usage and exiting 0.

This test confirms --help and -h both exit 0 and that the printed text
actually documents both real CLI flags (--allow-approximate-csg,
--stats) -- not just that *some* text was printed.

Usage: help_text_test.py <mc3togltf>
"""
import subprocess
import sys

REQUIRED_FLAGS = ["--allow-approximate-csg", "--stats"]


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf>")
        sys.exit(1)

    mc3togltf = sys.argv[1]

    for flag in ("--help", "-h"):
        r = run([mc3togltf, flag])
        combined = r.stdout + r.stderr
        assert r.returncode == 0, (
            f"expected '{flag}' to exit 0, got {r.returncode}\n"
            f"stdout={r.stdout}\nstderr={r.stderr}"
        )
        for required in REQUIRED_FLAGS:
            assert required in combined, (
                f"expected '{flag}' output to document '{required}', got:\n{combined}"
            )
        assert "Usage:" in combined, f"expected '{flag}' output to include a Usage: line, got:\n{combined}"
        print(f"PASS: '{flag}' exits 0 and documents all {len(REQUIRED_FLAGS)} CLI flags")
