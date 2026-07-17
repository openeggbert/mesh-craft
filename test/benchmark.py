#!/usr/bin/env python3
"""SYS-W12-01 (Phase 1) -- CLI-level performance benchmarks + baselines.

Times the existing `mc3tomcb` and `mc3togltf` CLI tools end to end against
a small/medium/large fixture, covering these of the task's named
categories:

  - XML open + MCB save   (mc3tomcb fixture.mc3.xml out.mcb)
  - MCB load  + XML save  (mc3tomcb out.mcb out.mc3.xml)
  - XML open + glTF export, incl. mesh-gen/CSG/texture processing that
    a real export touches (mc3togltf fixture.mc3.xml out.glb)

Deliberately NOT covered here -- these need either code instrumentation
inside the editor/library or driving a live MeshCraftApplication, which is
materially more work than CLI-level timing: mesh-gen/CSG-cache/traversal/
picking/undo-snapshot/animation-eval/registry/startup/first-frame in
isolation (only the combined "export" number above exercises mesh-gen/CSG/
texture processing, not each broken out separately), texture processing in
isolation. Tracked as SYS-W12-02, a deliberate Phase 2, not silently
dropped.

This is INFORMATIONAL, not a pass/fail gate: wall-clock timing on a shared/
virtualized/loaded CI or dev machine is inherently noisy across runs and
machines, so a hard regression threshold here would be flaky, not a real
signal. Registered as an always-green ctest (fails only if a tool crashes
or produces no output) so the numbers get printed on every run for a human
(or a future session) to compare against test/BENCHMARK_BASELINE.md.

Usage:
    benchmark.py <mc3tomcb-exe> <mc3togltf-exe> <repo-root> [--runs N]
"""

import argparse
import os
import statistics
import subprocess
import sys
import tempfile
import time

# (label, fixture path relative to test/, rough size tier)
FIXTURES = [
    ("small",  "house.mc3.xml"),
    ("medium", "features.mc3.xml"),
    ("large",  "medieval_castle.mc3.xml"),
]


def timeit(cmd, runs):
    """Run `cmd` `runs` times; return (median_seconds, ok)."""
    samples = []
    ok = True
    for _ in range(runs):
        start = time.perf_counter()
        result = subprocess.run(cmd, capture_output=True, text=True)
        elapsed = time.perf_counter() - start
        if result.returncode != 0:
            print(f"    FAIL: {' '.join(cmd)} exited {result.returncode}: "
                  f"{result.stderr.strip()[:300]}", file=sys.stderr)
            ok = False
            break
        samples.append(elapsed)
    return (statistics.median(samples) if samples else None), ok


def bench_fixture(mc3tomcb_exe, mc3togltf_exe, fixture_path, runs, workdir):
    results = {}
    ok = True

    mcb_path = os.path.join(workdir, "bench.mcb")
    cmd_ok = True
    t, cmd_ok = timeit([mc3tomcb_exe, fixture_path, mcb_path], runs)
    results["xml_open_mcb_save_s"] = t
    ok &= cmd_ok

    if cmd_ok and os.path.exists(mcb_path):
        xml_path = os.path.join(workdir, "bench_roundtrip.mc3.xml")
        t, cmd_ok = timeit([mc3tomcb_exe, mcb_path, xml_path], runs)
        results["mcb_load_xml_save_s"] = t
        ok &= cmd_ok

    glb_path = os.path.join(workdir, "bench.glb")
    t, cmd_ok = timeit([mc3togltf_exe, fixture_path, glb_path], runs)
    results["xml_open_gltf_export_s"] = t
    ok &= cmd_ok

    return results, ok


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("mc3tomcb_exe")
    parser.add_argument("mc3togltf_exe")
    parser.add_argument("repo_root")
    parser.add_argument("--runs", type=int, default=5,
                         help="samples per measurement (median reported)")
    args = parser.parse_args(argv[1:])

    test_dir = os.path.join(args.repo_root, "test")
    overall_ok = True

    with tempfile.TemporaryDirectory(prefix="meshcraft_bench_") as workdir:
        for label, rel_path in FIXTURES:
            fixture_path = os.path.join(test_dir, rel_path)
            if not os.path.exists(fixture_path):
                print(f"SKIP: {label} fixture missing: {fixture_path}")
                continue
            size_kb = os.path.getsize(fixture_path) / 1024.0
            print(f"--- {label} ({rel_path}, {size_kb:.1f} KB), "
                  f"median of {args.runs} run(s) ---")
            results, ok = bench_fixture(args.mc3tomcb_exe, args.mc3togltf_exe,
                                         fixture_path, args.runs, workdir)
            overall_ok &= ok
            for key, seconds in results.items():
                if seconds is None:
                    print(f"    {key}: FAILED")
                else:
                    print(f"    {key}: {seconds * 1000:.1f} ms")

    print()
    if not overall_ok:
        print("FAIL: at least one benchmarked command crashed or produced no output.")
        return 1
    print("All benchmarks ran successfully. This is an informational run, not a "
          "pass/fail performance gate -- compare against test/BENCHMARK_BASELINE.md "
          "by eye; wall-clock timing is too machine-dependent for an automated "
          "threshold here.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
