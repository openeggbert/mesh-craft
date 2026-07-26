#!/usr/bin/env python3
"""Lock a compact, deterministic MC3 -> MCB/GLB fixture across platforms."""

from __future__ import annotations

import hashlib
import pathlib
import subprocess
import sys


EXPECTED_MCB_SHA256 = "4157f107e277a405207853c78f13b3b472093ba31882d73d5f7711d582eb706a"
EXPECTED_GLB_SHA256 = "0ef25953c8bce5f1163f3a9743cdcd7ca55cb05705104054e14d40788a4e1168"


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str]) -> None:
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode != 0:
        print(completed.stdout, end="")
        print(completed.stderr, end="", file=sys.stderr)
        raise RuntimeError(f"command failed ({completed.returncode}): {command[0]}")


def main() -> int:
    if len(sys.argv) != 5:
        print(
            "usage: cross_platform_cli_fixture_test.py "
            "<mc3tomcb> <mc3togltf> <fixture.mc3.xml> <work-dir>",
            file=sys.stderr,
        )
        return 2

    mc3tomcb, mc3togltf, fixture, work_dir = map(pathlib.Path, sys.argv[1:])
    work_dir.mkdir(parents=True, exist_ok=True)
    mcb_output = work_dir / "house.mcb"
    glb_output = work_dir / "house.glb"

    try:
        run([str(mc3tomcb), str(fixture), str(mcb_output)])
        run([str(mc3togltf), str(fixture), str(glb_output)])
    except RuntimeError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1

    actual_mcb = sha256(mcb_output)
    actual_glb = sha256(glb_output)
    if actual_mcb != EXPECTED_MCB_SHA256:
        print(
            f"FAIL: MCB SHA-256 changed: expected {EXPECTED_MCB_SHA256}, got {actual_mcb}",
            file=sys.stderr,
        )
        return 1
    if actual_glb != EXPECTED_GLB_SHA256:
        print(
            f"FAIL: GLB SHA-256 changed: expected {EXPECTED_GLB_SHA256}, got {actual_glb}",
            file=sys.stderr,
        )
        return 1

    print(f"PASS: deterministic MCB SHA-256 {actual_mcb}")
    print(f"PASS: deterministic GLB SHA-256 {actual_glb}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
