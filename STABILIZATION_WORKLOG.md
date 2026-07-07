# STABILIZATION_WORKLOG.md

Running log of the conservative-maintainer stabilization audit. Newest entries at the top of each phase's section. Every entry that claims a result includes the exact command run.

---

## Session start: 2026-07-07

**Mandate**: act as a conservative maintainer. Re-verify plan.md from scratch rather than trusting prior claims. Do not mark anything ✅ without an actual command run and result recorded. Stabilization/tests/docs/bug-fixes only — no new features, no CNA changes, no MC3 redesign, no editor rewrite.

**Context**: this repo already has substantial stabilization history (419 commits total as of session start, most recent several sessions closing STAB-0001 through STAB-0650 in `plan.md`). Per the mandate, none of that prior work is being trusted at face value here — everything material is being re-checked with an actually-run command in this session before being reconfirmed.

---

## Phase 0 — Initial Safety Snapshot

**Command**: `git status`
```
On branch develop
Your branch is up to date with 'origin/develop'.
nothing to commit, working tree clean
```
Working tree is clean, HEAD in sync with `origin/develop` at commit `f663585`.

**Command**: `git log --oneline -20`
```
f663585 docs: confirm XML load path is already safe against the same recursion bug
f0dd333 docs: update NEXT.md with the post-650 bug-sweep findings
865d015 fix: MCB stack-overflow crash, resource-exhaustion, and registry temp-file leak
a4989fa docs: correct inaccurate bloom claim in STAB-0521's GL-check test
e89e899 docs: rewrite NEXT.md — stabilization plan essentially complete (620/650)
252f266 stab: close S16/S20 web+platform rows with real browser/Blender evidence
7f0bb2a stab: STAB-0521 — wire up a real per-frame GL error check; close S14
483c14c stab: close S11 (Materials, Textures, Visual Fidelity) — STAB-0421..0431
39d4aa2 stab: close S10 (AI Integration Stability) — STAB-0380..0410
0eee13b stab: STAB-0387/0388 — fix AiAssistant hang-on-close via detached thread
f9eefcc docs: rewrite NEXT.md — reflect S3-S9 closed (223 rows), S10 next
ce943e6 docs: recompute plan.md summary table after S9 closes
c4f3e61 stab: STAB-0353-0369 — S9 ModelRegistry; closes S9 (34/35)
958b7ea docs: recompute plan.md summary table after S8 closes
05dae8a stab: STAB-0309-0335 — S8 UI robustness; closes S8 (39/40)
e018d2a docs: rewrite NEXT.md — reflect S3-S7 closed (189 rows), S8 next
ee68b5b docs: recompute plan.md summary table after S7 closes
4b82832 stab: STAB-0269/0288/0289/0291/0293/0294/0295 — S7 editor workflows
3918f7d docs: recompute plan.md summary table after S6 closes
00cb691 stab: STAB-0259 — mark stale row done (missed in prior batch), closes S6
```
Full history: 419 commits total (`git log --oneline | wc -l`). The pattern of commits (build+test verification interleaved with plan.md updates, one focused batch per commit) is consistent with genuine incremental work rather than a single unverified bulk status change — but per the mandate this is re-checked in Phase 1/5 below, not just taken on faith from the commit messages.

**Command**: `grep -R "add_test" -n . --include="CMakeLists.txt" 2>/dev/null | wc -l` → **67** raw grep hits across the 5 project `CMakeLists.txt` files (`CMakeLists.txt`, `mc3togltf/CMakeLists.txt`, `mcb/CMakeLists.txt`, `mc3/CMakeLists.txt`, `mc3tomcb/CMakeLists.txt`). A precise regex extraction of `NAME` arguments (robust to `add_test(NAME x COMMAND y)` one-liners vs. multi-line calls) gives exactly **66 declared test names** — the 67th raw grep hit was a duplicate substring match, not a 67th test. Cross-checked against `ctest --test-dir cmake-build-debug -N`: **66 registered, exact 1:1 match** with the 66 declared names (`declared but not registered: []`, `registered but not declared: []`). No orphaned or phantom test registrations.

**Command**: `find test -name "*.mc3.xml" | wc -l` → **69** (in `test/`). `find . -name "*.mc3.xml" -not -path "*/cmake-build*" -not -path "*/_deps/*" | wc -l` → **71** repo-wide (2 more live under `mc3togltf/test/` and/or `mcb/test/` as fixtures for those subprojects' own tests).

**Command**: STAB task count and integrity check (`plan.md`):
```python
ids = sorted(int(m) for m in re.findall(r'^\| STAB-(\d+) \|', text, re.M))
# count: 650, min: 1, max: 650
# missing IDs (1-650): []
# extra/out-of-range IDs: []
```
Plus an explicit duplicate check: `grep -oP "^\| STAB-\d+" plan.md | sort | uniq -c | awk '$1>1'` → **empty output, zero duplicates**.

**Finding**: `plan.md` contains exactly one row per STAB ID from STAB-0001 through STAB-0650, contiguous, no gaps, no duplicates. (A prior session's worklog — see `plan.md`'s own "Post-650 Follow-Up Findings" section — records finding and removing one accidental duplicate STAB-0521 row; this snapshot confirms that fix is in place and the ID space is now clean.)

**Compiler/CMake versions**:
- `g++ (Debian 14.2.0-19) 14.2.0`
- System `cmake version 3.31.6`
- CLion-bundled `cmake version 4.2.2` at `/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake` — **this is the one that must be used to reconfigure `cmake-build-debug/`**; the system cmake has a documented bug affecting this project's reconfigure step (established in prior sessions, re-confirmed by continuing to use it without issue this session).

**Phase 0 conclusion**: no source changes made. Repo is in a clean, consistent state — no duplicate/missing STAB IDs, no orphaned test registrations. Proceeding to Phase 1 (clean build + full test run) to verify the *content* of what's claimed, not just the bookkeeping.

---

## Phase 1 — Clean Build and Test Verification

### CRITICAL FINDING: the literal `ninja` (default/`all`) build was broken from a truly clean state — masked by incremental build caching across the entire prior stabilization effort

**How this was found**: per the mandate to re-verify "from scratch," `cmake-build-debug/` (854MB, built up incrementally across many prior sessions, never fully wiped) was deleted entirely and reconfigured + rebuilt from zero.

**Command**: `rm -rf cmake-build-debug` then
```
<clion-cmake> -S . -B cmake-build-debug -DBUILD_TESTING=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -G Ninja
```
Configure succeeded (79.5s, no errors).

**Command**: `ninja -j$(nproc)` inside `cmake-build-debug/`
**Result**: **FAILED, exit code 1** (confirmed via a clean, unpiped exit-code check — an earlier piped check had silently shown exit 0 because it was `tail`'s exit code, not `ninja`'s; caught and corrected this in the same investigation):
```
/rv/data/development/github.com/openeggbert/cna/src/Microsoft/Xna/Framework/GamerServices/GamerProfile.cpp:9:54:
error: 'CurrentRegion' is not a member of 'System::Globalization::RegionInfo'
    9 |         , region_(System::Globalization::RegionInfo::CurrentRegion())
```

**Root cause investigation**:
- `../sharp-runtime`'s `RegionInfo` class was renamed from a `CurrentRegion()` static method to `getCurrentRegionProperty()` in sharp-runtime commit `4af2e31` ("Fix RegionInfo static naming..."), dated **2026-07-05** — a rename that predates this session by two days. `../cna`'s `GamerProfile.cpp` (in `CNA_GamerServices`) still calls the old name and was never updated to match.
- Re-ran with `ninja -k 0` (keep going past failures) to get the complete picture: **exactly one** compile error exists (confirmed via `grep -E "FAILED|error:" | sort -u` producing a single unique failure), not a cascade of many.
- Checked whether MeshCraft's own targets actually need the broken component: `grep -n "target_link_libraries" CMakeLists.txt | grep -i cna` → MeshCraft links `CNA ${_cna_backend_target} SHARP_RUNTIME Mc3 Mcb mc3togltf_lib imgui manifold tinyobjloader` — **`CNA_GamerServices` and `CNA_Net` are never referenced anywhere in MeshCraft's own CMakeLists.txt files.**
- Confirmed via `ninja -k 0` that despite the failure, `MeshCraft`, `ai_test`, `mc3_registry_test`, `mc3togltf`, `mc3tomcb`, `mc3_commands_test`, `mc3_roundtrip_test`, and `mcb_roundtrip_test` **all built and linked successfully** — every artifact this project actually needs.
- Confirmed `CNA_Net` transitively depends on `CNA_GamerServices` (`ninja CNA_Net` triggers building `GamerProfile.cpp.o` first), so both are equally blocked, and both are equally unused by MeshCraft.
- Found the root cause of *why* this is even in the default build graph: `../cna/CMakeLists.txt` has `option(CNA_ENABLE_NET "Build CNA networking (GamerServices + Net, requires ENet)" ON)` — **defaulting to ON**, unconditionally adding both targets to the default `ninja`/`all` target regardless of whether the consuming project needs them. MeshCraft's own `CMakeLists.txt` already sets several other CNA cache variables before `add_subdirectory(../cna CNA_dep)` (`CNA_BUILD_TESTS`, `CNA_BUILD_EXAMPLES`, `EASY_GL_BUILD_TESTS`, `EASY_GL_BUILD_EXAMPLES`) — it simply never set `CNA_ENABLE_NET`.
- Read `../cna/CMakeLists.txt:184-271` to confirm `CNA_ENABLE_NET` **cleanly and completely** gates both `CNA_GamerServices` and `CNA_Net` as separate, optional static-library targets — the core `CNA` library itself unconditionally *excludes* GamerServices/Net sources from its own compilation (lines 187-190) regardless of this flag. Setting it OFF has zero effect on anything MeshCraft actually links.

**Fix applied (MeshCraft-side only, zero CNA/SHARP_RUNTIME source changes)**: added `set(CNA_ENABLE_NET OFF CACHE BOOL "" FORCE)` to `CMakeLists.txt`, right alongside the existing `CNA_BUILD_TESTS`/`CNA_BUILD_EXAMPLES`/etc. cache-variable block before `add_subdirectory(../cna CNA_dep)`. This does **not** modify `../cna` or `../sharp-runtime` in any way — it declines to opt into an optional CNA feature this project never uses, via the exact mechanism CNA's own maintainers already built for this purpose. This is explicitly within the "Allowed source changes" list (Phase 6: "fixing build registration") and does not trigger the "Do NOT modify ../cna" boundary at all, since no file under `../cna` or `../sharp-runtime` was touched.

**Verification after the fix**:
- `ninja -j$(nproc)`: **exit code 0**, 120/120 build steps (down from 566 — GamerServices/Net's large XNA-compatibility-layer source tree is no longer compiled at all, a meaningful build-time win as a side effect).
- `ctest --output-on-failure`: **66/66 tests passed**, exit code 0.
- `python3 test/validate_xsd.py mc3/mc3.xsd test/*.mc3.xml`: **all 69 files valid**, exit code 0.

**Why this matters / what it means for trusting prior sessions' claims**: every prior session's "N/N tests pass" claim was checked against an incrementally-updated `cmake-build-debug/` that had compiled `GamerProfile.cpp.o` successfully at some point *before* 2026-07-05's sharp-runtime rename, and Ninja's dependency tracking never had a reason to invalidate that already-built object file since nothing in *this* repo's source touches it — so the break was real but invisible to every `ninja && ctest` run for at least two days, across many sessions, because none of them ever did a truly clean rebuild. **This does not mean the prior sessions' test-pass claims were false** — the 66/66 result just reconfirmed is the same 66/66 that's been passing all along, and every one of those tests genuinely exercises real MeshCraft code — but it does mean **"the project builds cleanly from scratch" was quietly untrue** until this fix, and no amount of re-running `ctest` in an already-populated build directory would ever have caught it. This is exactly the class of problem a "verify from scratch" mandate is designed to catch, and it did.

**Recommendation for CNA/SHARP_RUNTIME maintainers** (out of scope to act on here — a note only, not a demand): `../cna`'s `GamerProfile.cpp:9` should be updated to call `RegionInfo::getCurrentRegionProperty()` instead of the removed `RegionInfo::CurrentRegion()`. Since MeshCraft no longer builds this component by default, this won't block MeshCraft going forward, but the CNA repo's own build (and anything else that sets `CNA_ENABLE_NET=ON`) is presumably still broken until that's fixed on the CNA side.

### Standard Phase 1 verification commands and results (with the fix applied)

| Command | Result |
|---|---|
| `<clion-cmake> -S . -B cmake-build-debug -DBUILD_TESTING=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -G Ninja` | Configure succeeded, 0 errors |
| `ninja -j$(nproc)` | **Exit 0**, 120/120 steps |
| `ctest --test-dir cmake-build-debug --output-on-failure` | **66/66 passed**, exit 0 |
| `ctest --test-dir cmake-build-debug -N` | 66 tests listed; cross-checked against `add_test(NAME ...)` declarations across all 5 project `CMakeLists.txt` files — exact 1:1 match, no orphans, no phantoms |
| `python3 test/validate_xsd.py mc3/mc3.xsd test/*.mc3.xml` | **69/69 valid**, exit 0 |

Compiler: `g++ (Debian 14.2.0-19) 14.2.0`. CMake: CLion-bundled `4.2.2` (required for reconfigure; system `cmake 3.31.6` has a documented bug affecting this project). Test labels: `ai` (1), `commands` (1), `export` (44), `format` (3), `registry` (1), `render` (16) — total 66.

**Phase 1 conclusion**: after the `CNA_ENABLE_NET` fix, the project builds and tests genuinely clean from an absolute-zero state. This is the first time in this project's session history that this has been verified with a fully deleted build directory rather than an incremental one.
