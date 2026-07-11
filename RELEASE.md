# Release Checklist

MeshCraft is currently in a **stabilization phase** (see
`STABILIZATION.md`) — no gate is fully green yet (each requires its
entire `STAB-XXXX` range in `plan.md`, not just the priority-list
subset). This checklist is for when a release is actually being cut;
until then, treat it as the bar to clear, not a completed process.

## Build clean

- [ ] `cmake-build-debug/` (Debug): full reconfigure (CLion's bundled
      cmake — see `CONTRIBUTING.md`) + full rebuild, 0 errors/warnings
      of note.
      ```sh
      "$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON
      ninja -C cmake-build-debug
      ```
- [ ] Release build (fresh directory, system cmake is fine): full
      configure + build.
      ```sh
      cmake -S . -B b-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
            -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
      cmake --build b-release -j4
      ```
- [ ] Offline mode builds cleanly
      (`-DFETCHCONTENT_UPDATES_DISCONNECTED=ON` with dependencies
      already fetched from a prior build).
- [ ] Each CNA-free component builds standalone, without the root
      project: `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` (see
      `TESTING.md`'s standalone-build loop).

## All tests pass

- [ ] `ctest --output-on-failure` in `cmake-build-debug/`: **all**
      tests pass — run `ctest -N` for the current count (95 as of
      2026-07-11; this number grows over time, do not hard-code it here).
- [ ] Same in the Release build directory.
- [ ] Each standalone component's own test(s) pass — check with
      `ctest -N -L <label>` (`format`/`export`/`unit`/`lint`/`ai`/
      `commands`/`registry`/`render`) rather than trusting a hard-coded
      per-component count here.
- [ ] No test was skipped due to a missing optional dependency
      (SQLite3, OpenSSL, LibXml2) that should actually be present on
      the release build machine — check the CMake configure log for
      "not found" messages for any of these.

## Docs current

- [ ] `NEXT.md` reflects actual current state (status/blocker/next
      task) — not stale from a prior session.
- [ ] `plan.md`'s summary table matches the actual row markers (a
      `python3` one-liner recomputing `✅`/`🟡`/`🧪`/`📋`/`🔴` counts per
      section from the row markers is the reliable way to check this —
      the table has drifted from hand-editing before).
- [ ] `README.md`'s "Current Features"/"Current Limitations" reflect
      what actually works — spot-check a few claims against the code,
      don't just trust the prose (this has caught real staleness
      before — see `NEXT.md`'s history for an example).
- [ ] `MC3_FORMAT.md` covers every element `mc3.xsd` declares (no
      N1-N7 gaps).
- [ ] `mc3.xsd` declares every attribute/element `Mc3XmlWriter.cpp`
      actually writes (see `CONTRIBUTING.md`'s "schema/writer symmetry"
      note — this has been a real, recurring source of bugs).
- [ ] `TESTING.md`'s test count and per-test descriptions match
      `ctest -N`'s actual output.

## Sample files valid

- [ ] Every `test/*.mc3.xml` fixture validates against `mc3/mc3.xsd`
      (covered by the `xsd_validation` ctest, but worth a manual spot
      check if any fixture changed recently).
- [ ] `mc3togltf` exports at least one representative scene
      (`test/features.mc3.xml` or similar) to `.glb` and the result
      opens cleanly in a glTF viewer (not just "exit code 0" — a
      malformed-but-technically-valid GLB can still fail to render).
- [ ] `mc3tomcb` round-trips a representative scene
      (`.mc3.xml → .mcb → .mc3.xml`) losslessly.

## Before tagging

- [ ] `git status` clean, all intended changes committed.
- [ ] `develop` (or the release branch) pushed and in sync with the
      remote.
- [ ] No secrets committed (API keys, tokens) — this project has had a
      PAT-in-`.git/config` issue before; double-check nothing similar
      snuck into a tracked file.
