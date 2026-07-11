#!/usr/bin/env python3
"""Mechanical self-consistency validator for plan.md/NEXT.md.

This exists because the backlog itself has already gone stale twice: once
when the pre-audit plan.md self-contradicted (723 vs 650 rows), and again
when the post-audit plan.md claimed "93/93" in one place and "95/95" in
another while only 4 of 57 detailed findings were marked DONE despite 16+
already being fixed in git history. Both times a human reading the prose
would have had to cross-check every number by hand. This script does that
cross-check every time, so a future session cannot silently repeat it.

Checks:
  1. AUD-### and SYS-### task IDs are each unique.
  2. Every AUD-### task has non-empty, non-obviously-truncated required
     fields (Component, Evidence, Outcome).
  3. Every DONE AUD-### task cites a commit reference.
  4. The "Net across all N AUD-### rows" summary line's counts match the
     actual per-row status tally.
  5. The priority execution queue does not reference an AUD-### or SYS-###
     ID that is marked DONE.
  6. No line in the active backlog (plan.md) contains a machine-specific
     absolute source path (/home/..., /rv/..., /Users/..., C:\\...).
  7. If a CMake build directory is available, the ctest count documented in
     NEXT.md matches the live `ctest -N` count.

Usage: python3 test/validate_plan_consistency.py [repo_root] [build_dir]
Exit 0 if all checks pass, 1 otherwise. Prints PASS/FAIL per check.
"""
import re
import subprocess
import sys
from pathlib import Path

failures = 0


def check(cond, msg):
    global failures
    if cond:
        print(f"PASS: {msg}")
    else:
        print(f"FAIL: {msg}", file=sys.stderr)
        failures += 1


def parse_tasks(text, pattern):
    """Return list of (id, status, body_start, body_end) for each task header
    matching pattern. body spans from the header line to the next header or
    section break, used to check required fields."""
    matches = list(pattern.finditer(text))
    tasks = []
    for i, m in enumerate(matches):
        end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
        tasks.append({
            "id": m.group("id"),
            "status": m.group("status"),
            "body": text[m.start():end],
        })
    return tasks


AUD_RE = re.compile(r"^### (?P<id>AUD-\d+[a-z]?) `\[(?P<status>[A-Z_]+)\]`", re.MULTILINE)
SYS_RE = re.compile(r"^\- \*\*(?P<id>SYS-W\d+-\d+)\*\* `\[(?P<status>[A-Za-z0-9_, ->]+)\]`", re.MULTILINE)

ABS_PATH_RE = re.compile(r"(?:^|[\s`(\"'])(/home/[^\s`)\"']+|/rv/[^\s`)\"']+|/Users/[^\s`)\"']+|[A-Za-z]:\\\\[^\s`)\"']+)")


def main():
    repo = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent
    build_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else None

    plan_path = repo / "plan.md"
    next_path = repo / "NEXT.md"
    check(plan_path.exists(), f"plan.md exists at {plan_path}")
    check(next_path.exists(), f"NEXT.md exists at {next_path}")
    if failures:
        print(f"\n{failures} check(s) failed -- cannot continue without plan.md/NEXT.md.", file=sys.stderr)
        sys.exit(1)

    plan_text = plan_path.read_text()
    next_text = next_path.read_text()

    # --- 1. Unique IDs ---
    aud_tasks = parse_tasks(plan_text, AUD_RE)
    sys_tasks = parse_tasks(plan_text, SYS_RE)
    check(len(aud_tasks) > 0, "at least one AUD-### task found")
    check(len(sys_tasks) > 0, "at least one SYS-### task found")

    aud_ids = [t["id"] for t in aud_tasks]
    dup_aud = {i for i in aud_ids if aud_ids.count(i) > 1}
    check(not dup_aud, f"AUD-### IDs are unique (duplicates: {dup_aud})" if dup_aud else "AUD-### IDs are unique")

    sys_ids = [t["id"] for t in sys_tasks]
    dup_sys = {i for i in sys_ids if sys_ids.count(i) > 1}
    check(not dup_sys, f"SYS-### IDs are unique (duplicates: {dup_sys})" if dup_sys else "SYS-### IDs are unique")

    # --- 2. Required fields present and not obviously truncated ---
    # Component is often just a short file path/name (e.g. "plan.md", 7 chars),
    # so it gets a much lower bar than the prose fields.
    MIN_FIELD_LEN = {"Component": 3, "Evidence": 20, "Outcome": 20}
    for t in aud_tasks:
        for field, min_len in MIN_FIELD_LEN.items():
            m = re.search(rf"- \*\*{field}:\*\* (.+?)(?=\n- \*\*|\n\n|\Z)", t["body"], re.DOTALL)
            present = m is not None and len(m.group(1).strip()) >= min_len
            check(present, f"{t['id']}: {field} field present and non-trivial")

    # --- 3. DONE tasks cite a commit ---
    for t in aud_tasks:
        if t["status"] == "DONE":
            has_commit = "**Resolved:**" in t["body"] and ("commit" in t["body"].lower())
            check(has_commit, f"{t['id']} (DONE) cites a resolving commit")

    # --- 4. Summary counts match actual tally ---
    tally = {"DONE": 0, "TODO": 0, "DEFERRED": 0}
    for t in aud_tasks:
        tally[t["status"]] = tally.get(t["status"], 0) + 1

    m = re.search(
        r"Net across all (\d+) AUD-### rows.*?:\s*(\d+) DONE, (\d+) TODO, (\d+) DEFERRED",
        plan_text, re.DOTALL)
    if m:
        claimed_total, claimed_done, claimed_todo, claimed_deferred = (int(x) for x in m.groups())
        actual_total = len(aud_tasks)
        check(claimed_total == actual_total,
              f"claimed total AUD rows ({claimed_total}) matches actual ({actual_total})")
        check(claimed_done == tally.get("DONE", 0),
              f"claimed DONE count ({claimed_done}) matches actual ({tally.get('DONE', 0)})")
        check(claimed_todo == tally.get("TODO", 0),
              f"claimed TODO count ({claimed_todo}) matches actual ({tally.get('TODO', 0)})")
        check(claimed_deferred == tally.get("DEFERRED", 0),
              f"claimed DEFERRED count ({claimed_deferred}) matches actual ({tally.get('DEFERRED', 0)})")
    else:
        check(False, "found a 'Net across all N AUD-### rows' summary line to cross-check")

    # --- 5. Priority queue doesn't reference DONE tasks ---
    qmatch = re.search(r"## Priority execution queue.*?\n(.*?)\n---", plan_text, re.DOTALL)
    done_ids = {t["id"] for t in aud_tasks if t["status"] == "DONE"} | \
               {t["id"] for t in sys_tasks if t["status"] == "DONE"}
    if qmatch:
        queue_text = qmatch.group(1)
        referenced = set(re.findall(r"\b(AUD-\d+[a-z]?|SYS-W\d+-\d+)\b", queue_text))
        stale_refs = referenced & done_ids
        check(not stale_refs,
              f"priority queue does not reference DONE tasks (found: {stale_refs})" if stale_refs
              else "priority queue does not reference any DONE task")
    else:
        check(False, "found a 'Priority execution queue' section")

    # --- 6. No absolute machine-specific paths ---
    abs_hits = ABS_PATH_RE.findall(plan_text)
    check(not abs_hits, f"no machine-specific absolute paths in plan.md (found: {abs_hits[:3]})" if abs_hits
          else "no machine-specific absolute paths in plan.md")
    abs_hits_next = ABS_PATH_RE.findall(next_text)
    check(not abs_hits_next, f"no machine-specific absolute paths in NEXT.md (found: {abs_hits_next[:3]})" if abs_hits_next
          else "no machine-specific absolute paths in NEXT.md")

    # --- 7. ctest count matches NEXT.md's documented count (if build available) ---
    # Only match genuine "N/N pass"-style ratio claims (e.g. "**95/95** pass"),
    # not incidental digit-then-"test" text like "python3 test/foo.py" or
    # "93-vs-95 test-count inconsistency" (both would false-positive on a loose
    # "\d+\s+tests?" pattern).
    if build_dir and build_dir.exists():
        try:
            out = subprocess.run(["ctest", "-N"], cwd=str(build_dir), capture_output=True, text=True, timeout=30)
            m2 = re.search(r"Total Tests: (\d+)", out.stdout)
            if m2:
                live_count = int(m2.group(1))
                ratio_claims = [(int(a), int(b)) for a, b in re.findall(r"(\d+)/(\d+)\s*(?:pass|\*\*)", next_text)]
                bad_ratios = [(a, b) for a, b in ratio_claims if a != b or b != live_count]
                check(not bad_ratios,
                      f"NEXT.md's N/N pass claims match the live ctest -N count ({live_count}); "
                      f"mismatched: {bad_ratios}" if bad_ratios
                      else f"NEXT.md's N/N pass claims match the live ctest -N count ({live_count})")
            else:
                print("SKIP: could not parse ctest -N output")
        except Exception as e:
            print(f"SKIP: ctest -N check ({e})")
    else:
        print("SKIP: no build_dir given/found -- pass one as argv[2] to check the live ctest count")

    print()
    if failures == 0:
        print("All plan.md/NEXT.md consistency checks passed.")
        sys.exit(0)
    else:
        print(f"{failures} consistency check(s) FAILED.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
