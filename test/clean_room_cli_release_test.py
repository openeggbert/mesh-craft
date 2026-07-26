#!/usr/bin/env python3
"""SYS-W11-08: verify the CLI release archive is a self-contained artifact.

Runs entirely against an already-extracted copy of the release archive
(see cmake/CreateCliRelease.cmake and the clean_room_cli_release_smoke
CTest driver, which extracts into a path containing a space and a
non-ASCII character before invoking this). Every subprocess below runs
with a minimal PATH that contains no build-tree directory at all, and every
tool is invoked by its full extracted path -- neither can silently fall
back to an in-tree binary or dependency.
"""

from __future__ import annotations

import glob
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys


EXPECTED_MCB_SHA256 = "4157f107e277a405207853c78f13b3b472093ba31882d73d5f7711d582eb706a"
EXPECTED_GLB_SHA256 = "0ef25953c8bce5f1163f3a9743cdcd7ca55cb05705104054e14d40788a4e1168"

# Deliberately excludes any build-tree/checkout path -- proves the tools
# don't need anything from the environment beyond a standard system PATH.
_CLEAN_ENV = {"PATH": "/usr/bin:/bin"}
if os.name == "nt":
    _CLEAN_ENV = {"PATH": r"C:\Windows\System32;C:\Windows"}
# Some libc/runtime lookups need HOME/TEMP to exist; carry those through
# without carrying through anything build-tree-specific.
for _carry in ("HOME", "TEMP", "TMP", "USERPROFILE"):
    if _carry in os.environ:
        _CLEAN_ENV[_carry] = os.environ[_carry]


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[str]) -> subprocess.CompletedProcess:
    completed = subprocess.run(command, text=True, capture_output=True, env=_CLEAN_ENV)
    if completed.returncode != 0:
        print(completed.stdout, end="")
        print(completed.stderr, end="", file=sys.stderr)
        raise RuntimeError(f"command failed ({completed.returncode}): {command}")
    return completed


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: clean_room_cli_release_test.py <extracted-archive-root> <fixture.mc3.xml>",
            file=sys.stderr,
        )
        return 2

    root, fixture = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    exe_suffix = ".exe" if os.name == "nt" else ""
    mc3tomcb = root / "bin" / f"mc3tomcb{exe_suffix}"
    mc3togltf = root / "bin" / f"mc3togltf{exe_suffix}"

    failures = 0

    def check(cond: bool, message: str) -> None:
        nonlocal failures
        if cond:
            print(f"PASS: {message}")
        else:
            print(f"FAIL: {message}", file=sys.stderr)
            failures += 1

    check(mc3tomcb.is_file(), f"extracted archive contains {mc3tomcb}")
    check(mc3togltf.is_file(), f"extracted archive contains {mc3togltf}")
    check((root / "LICENSE").is_file(), "extracted archive contains LICENSE")
    check((root / "THIRD_PARTY.md").is_file(), "extracted archive contains THIRD_PARTY.md")
    manifest = root / "SHA256SUMS.txt"
    check(manifest.is_file(), "extracted archive contains SHA256SUMS.txt")
    if failures:
        return 1  # nothing further is safe to try without the binaries/manifest

    # The manifest itself: every listed file's hash must match what's
    # actually on disk right now, using this script's own hashing (not
    # trusting sha256sum -c's own correctness circularly).
    manifest_entries = 0
    for line in manifest.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        expected_hash, _, relative_path = line.partition("  ")
        manifest_entries += 1
        actual_path = root / relative_path
        if not actual_path.is_file():
            check(False, f"manifest entry '{relative_path}' exists on disk")
            continue
        check(sha256(actual_path) == expected_hash,
              f"manifest hash for '{relative_path}' matches the extracted file")
    check(manifest_entries > 10, f"manifest lists a plausible number of files ({manifest_entries})")

    try:
        version_mcb = run([str(mc3tomcb), "--version"])
        check(bool(version_mcb.stdout.strip()), "mc3tomcb --version prints something, from a clean PATH")
        version_gltf = run([str(mc3togltf), "--version"])
        check(bool(version_gltf.stdout.strip()), "mc3togltf --version prints something, from a clean PATH")

        work_dir = root / "clean-room-work"
        work_dir.mkdir(exist_ok=True)
        mcb_output = work_dir / "house.mcb"
        glb_output = work_dir / "house.glb"
        run([str(mc3tomcb), str(fixture), str(mcb_output)])
        run([str(mc3togltf), str(fixture), str(glb_output)])

        actual_mcb = sha256(mcb_output)
        actual_glb = sha256(glb_output)
        check(actual_mcb == EXPECTED_MCB_SHA256,
              f"MC3->MCB conversion from the clean-room archive is byte-identical to the "
              f"in-tree fixture (got {actual_mcb})")
        check(actual_glb == EXPECTED_GLB_SHA256,
              f"MC3->GLB conversion from the clean-room archive is byte-identical to the "
              f"in-tree fixture (got {actual_glb})")
    except RuntimeError as error:
        check(False, f"clean-room conversion did not crash or fail to launch: {error}")

    # Missing-runtime detection: mc3togltf links libmanifold/libtinyobjloader
    # as shared libraries via a relative ($ORIGIN/../lib) rpath (SYS-W11-05).
    # Removing them must produce a clean dynamic-linker failure (a real
    # process launch failure, non-zero exit), not a hang or a crash with no
    # diagnostic at all.
    runtime_libs = [pathlib.Path(p) for p in
                     glob.glob(str(root / "lib" / "libmanifold.so*")) +
                     glob.glob(str(root / "lib" / "libtinyobjloader.so*"))]
    if runtime_libs:
        missing_runtime_root = root.parent / "clean-room-missing-runtime"
        shutil.rmtree(missing_runtime_root, ignore_errors=True)
        shutil.copytree(root, missing_runtime_root)
        for lib in runtime_libs:
            (missing_runtime_root / "lib" / lib.name).unlink()
        broken_exe = missing_runtime_root / "bin" / f"mc3togltf{exe_suffix}"
        result = subprocess.run([str(broken_exe), "--version"], text=True,
                                 capture_output=True, env=_CLEAN_ENV, timeout=15)
        check(result.returncode != 0,
              "removing mc3togltf's runtime libraries produces a launch failure, "
              "not a silent success")
    else:
        check(False, "found at least one manifold/tinyobjloader runtime library to remove "
              "(nothing to test missing-runtime detection against)")

    if failures:
        print(f"{failures} clean-room release check(s) failed.", file=sys.stderr)
        return 1
    print("All clean-room release checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
