#!/usr/bin/env python3
"""CTest display preflight for render-labelled MeshCraft tests (AUD-090)."""
import shutil
import subprocess
import sys


def main():
    xvfb_run = shutil.which("xvfb-run")
    xdpyinfo = shutil.which("xdpyinfo")
    if not xvfb_run or not xdpyinfo:
        missing = ", ".join(name for name, value in
                            (("xvfb-run", xvfb_run), ("xdpyinfo", xdpyinfo))
                            if not value)
        print(f"SKIP: render display unavailable; missing {missing}")
        return 77

    try:
        result = subprocess.run(
            [xvfb_run, "--auto-servernum", xdpyinfo],
            capture_output=True,
            text=True,
            timeout=10,
        )
    except subprocess.TimeoutExpired:
        print("SKIP: render display unavailable; xvfb-run/xdpyinfo timed out")
        return 77

    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip().replace("\n", " ")
        print("SKIP: render display unavailable; xvfb-run could not establish "
              f"a usable X listener ({detail[:300]})")
        return 77

    print("PASS: xvfb-run started a usable X display for render CTests")
    return 0


if __name__ == "__main__":
    sys.exit(main())
