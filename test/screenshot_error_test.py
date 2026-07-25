#!/usr/bin/env python3
"""Regression test for AUD-089: failed --screenshot output is a CLI error.

The destination is a directory, not a file. That is deterministically
unwritable as a PPM output on every supported platform without relying on
root permissions, filesystem ACLs, or an externally reserved path.
"""
import os
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <MeshCraft-binary> <scene>", file=sys.stderr)
        return 2
    binary, scene = sys.argv[1:]

    with tempfile.TemporaryDirectory(prefix="meshcraft_screenshot_error_") as output_dir:
        command = [binary, scene, "--screenshot", output_dir]
        if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
            command = ["xvfb-run", "--auto-servernum"] + command
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)

    assert result.returncode != 0, (
        "--screenshot accepted a directory as an output path:\n"
        f"stdout={result.stdout}\nstderr={result.stderr}"
    )
    assert "[Screenshot] failed" in result.stderr, (
        "failed screenshot did not report an output error:\n"
        f"stdout={result.stdout}\nstderr={result.stderr}"
    )
    assert "Auto-screenshot saved" not in result.stdout, (
        "failed screenshot emitted a false saved message:\n"
        f"stdout={result.stdout}"
    )
    print("PASS: --screenshot output failure returns non-zero without a false success message")
    return 0


if __name__ == "__main__":
    sys.exit(main())
