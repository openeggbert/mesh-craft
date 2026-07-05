#!/usr/bin/env python3
"""STAB-0537/0538 -- mc3tomcb CLI error-handling tests.

mc3tomcb/src/main.cpp wraps the whole conversion in a try/catch that
prints "Error: <message>" to stderr and exits 1 on any std::exception --
this test confirms that actually holds for two real failure modes:

  STAB-0537: a missing input file (Mc3Document::loadFromFile() throws
             tinyxml2's XML_ERROR_FILE_NOT_FOUND).
  STAB-0538: a write-protected output directory (Mcb::saveToFile() /
             Mc3Document::saveToFile() throw "Cannot open for writing").
             Uses a fresh temp dir with its write bit stripped (chmod
             0o555) rather than a hardcoded root-owned path, so this
             test doesn't depend on the runner's privilege level.

Usage: mc3tomcb_error_test.py <mc3tomcb-exe> <fixture.mc3.xml>
"""
import os
import stat
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <mc3tomcb-exe> <fixture.mc3.xml>")
        sys.exit(1)
    exe, fixture = sys.argv[1], sys.argv[2]

    # STAB-0537: missing input file.
    with tempfile.TemporaryDirectory() as tmpdir:
        missing = os.path.join(tmpdir, "does_not_exist.mc3.xml")
        out_mcb = os.path.join(tmpdir, "out.mcb")
        r = subprocess.run([exe, missing, out_mcb], capture_output=True, text=True)
        assert r.returncode != 0, (
            f"expected a non-zero exit for a missing input file, got 0.\n"
            f"stdout={r.stdout}\nstderr={r.stderr}"
        )
        assert r.stderr.strip(), "expected an error message on stderr for a missing input file"
        assert not os.path.exists(out_mcb), "expected no output file for a missing input file"
        print(f"PASS (STAB-0537): missing input rejected -- exit {r.returncode}, stderr: {r.stderr.strip()[:80]}")

    # STAB-0538: write-protected output directory.
    with tempfile.TemporaryDirectory() as tmpdir:
        readonly_dir = os.path.join(tmpdir, "readonly")
        os.mkdir(readonly_dir)
        os.chmod(readonly_dir, stat.S_IRUSR | stat.S_IXUSR)  # r-x, no write
        try:
            out_mcb = os.path.join(readonly_dir, "out.mcb")
            r = subprocess.run([exe, fixture, out_mcb], capture_output=True, text=True)
            assert r.returncode != 0, (
                f"expected a non-zero exit for a write-protected output path, got 0.\n"
                f"stdout={r.stdout}\nstderr={r.stderr}"
            )
            assert r.stderr.strip(), "expected an error message on stderr for a write-protected output path"
            assert not os.path.exists(out_mcb), "expected no output file for a write-protected output path"
            print(f"PASS (STAB-0538): write-protected output rejected -- exit {r.returncode}, stderr: {r.stderr.strip()[:80]}")
        finally:
            os.chmod(readonly_dir, stat.S_IRWXU)  # restore so TemporaryDirectory cleanup can remove it


if __name__ == "__main__":
    main()
