#!/usr/bin/env python3
"""Run a small, deterministic, resource-bounded set of libFuzzer targets."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=int, default=20,
                        help="maximum libFuzzer run time per target")
    # 512 (the original default) is too tight: a normal, non-malicious ASan
    # run legitimately grows RSS well past it just from libFuzzer's own
    # retained in-memory corpus (it keeps every input that finds new
    # coverage) plus ASan's own instrumentation overhead -- confirmed via
    # local repro (peaked at 505MB/20s and 822MB/20s across two seeds, 668MB
    # over a longer 40s run, zero crashes/leaks in any of them), matching a
    # CI run that hit 550MB and got killed by the old 512MB ceiling. Not a
    # per-input security bug (replaying the exact CI-reported "oom-" input
    # 2000x in one process found no leak). 2048 leaves real headroom above
    # the highest local measurement rather than just clearing one observed
    # number -- raise the ceiling, don't shrink real fuzzing coverage, same
    # fix pattern as mc3_json_document_budget's CTest TIMEOUT.
    parser.add_argument("--rss-mib", type=int, default=2048,
                        help="libFuzzer resident-memory ceiling per target")
    parser.add_argument("--work-dir", type=Path, required=True,
                        help="generated working corpora and crash artifacts; never a source corpus")
    parser.add_argument("--target", nargs=2, action="append", metavar=("BINARY", "CORPUS"),
                        required=True, help="one fuzzer executable and its non-empty seed corpus")
    args = parser.parse_args()

    seconds = max(1, args.seconds)
    rss_mib = max(64, args.rss_mib)
    work_root = args.work_dir
    work_root.mkdir(parents=True, exist_ok=True)
    for index, (binary_text, corpus_text) in enumerate(args.target, start=1):
        binary = Path(binary_text)
        corpus = Path(corpus_text)
        if not binary.is_file():
            parser.error(f"fuzzer binary does not exist: {binary}")
        if not corpus.is_dir() or not any(corpus.iterdir()):
            parser.error(f"fuzzer corpus is missing or empty: {corpus}")
        run_corpus = work_root / f"corpus-{index}"
        artifacts = work_root / f"artifacts-{index}"
        shutil.rmtree(run_corpus, ignore_errors=True)
        shutil.rmtree(artifacts, ignore_errors=True)
        shutil.copytree(corpus, run_corpus)
        artifacts.mkdir()
        command = [
            str(binary), str(run_corpus),
            f"-max_total_time={seconds}",
            "-timeout=5",
            f"-rss_limit_mb={rss_mib}",
            "-max_len=1048576",
            f"-seed={1000 + index}",
            "-detect_leaks=0",
            f"-artifact_prefix={artifacts}/",
            "-print_final_stats=1",
        ]
        print("+", " ".join(command), flush=True)
        environment = os.environ.copy()
        # libFuzzer launches under a debugger/ptrace wrapper on some local
        # runners, where LeakSanitizer cannot inspect threads and reports a
        # false fatal error after a clean run. ASan/UBSan remain enabled; the
        # ordinary sanitizer CTest job still performs leak checking.
        environment["ASAN_OPTIONS"] = "detect_leaks=0"
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, errors="replace", timeout=seconds + 15,
                                env=environment)
        output_lines = result.stdout.splitlines()
        if result.returncode:
            print("\n".join(output_lines[-30:]), file=sys.stderr)
            print(f"FAIL: fuzz target {binary} exited {result.returncode}; "
                  f"artifacts are in {artifacts}", file=sys.stderr)
            return result.returncode
        print("\n".join(output_lines[-10:]))
        shutil.rmtree(run_corpus)
        shutil.rmtree(artifacts)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.TimeoutExpired as error:
        print(f"FAIL: fuzz smoke exceeded outer time bound: {error}", file=sys.stderr)
        raise SystemExit(1)
