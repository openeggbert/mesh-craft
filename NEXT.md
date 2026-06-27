# NEXT.md — MeshCraft Operational Handoff

_Last updated: 2026-06-27. Stabilization plan created: 650 STAB tasks across S0–S20._

---

## Current Mode

**Stabilization only.**  
No new features until gates S0–S6 are green. See `plan.md`.

---

## Current Phase: Gate 0 → Gate 1

**Gate 0 (Build)** — Verify build is reproducible and all 15 tests are registered and passing.  
**Gate 1 (Format)** — MCB roundtrip tests for N1-N7, XSD fixtures for N3-N7, missing roundtrip coverage.

---

## Next 10 Tasks (in order)

| ID | Pri | Title | Key file(s) |
|----|-----|-------|-------------|
| STAB-0001 | P0 | Verify clean debug build exits 0 with no errors | `CMakeLists.txt` |
| STAB-0004 | P0 | Confirm all 15 CTest tests are registered | `CMakeLists.txt` |
| STAB-0019 | P0 | All 15 tests pass on current branch | all |
| STAB-0121 | P1 | Create `mcb_roundtrip_test` binary + CTest target | `mcb/CMakeLists.txt` or root `CMakeLists.txt` |
| STAB-0122 | P1 | MCB roundtrip: basic scene | new `mcb_roundtrip_test.cpp` |
| STAB-0123–0130 | P1 | MCB roundtrip: N1-N7 new types | new `mcb_roundtrip_test.cpp` |
| STAB-0041 | P1 | XSD validation fixture: N3 scripts element | new `test/n3_scripts_test.mc3.xml` |
| STAB-0042 | P1 | XSD validation fixture: N4 sounds/music elements | new `test/n4_sounds_test.mc3.xml` |
| STAB-0043 | P1 | XSD validation fixture: N5 triggers elements | new `test/n5_triggers_test.mc3.xml` |
| STAB-0044–0046 | P1 | XSD fixtures N6-N7 + update features.mc3.xml | `test/features.mc3.xml` |

---

## Commands to Run

```bash
# Configure (if not already done)
cd cmake-build-debug
cmake .. -DFETCHCONTENT_UPDATES_DISCONNECTED=ON -DBUILD_TESTING=ON

# Build all targets
ninja

# Verify all 15 tests pass (Gate 0)
ctest --output-on-failure

# Check test count
ctest -N | grep "Total Tests:"

# XSD validation
python3 test/validate_xsd.py mc3/mc3.xsd test/*.mc3.xml

# After adding mcb_roundtrip_test:
ninja mcb_roundtrip_test
ctest -R mcb_roundtrip --output-on-failure
```

---

## Status at 2026-06-27

- **15/15 CTest tests pass** (smoke, xsd_validation, mc3_registry, mc3_roundtrip, mc3_commands, 10 mc3togltf tests)
- **N1-N7 schema extensions complete** (parser, writer, MCB, XSD, roundtrip tests)
- **Missing**: MCB binary roundtrip test, XSD fixtures for N3-N7, many S1-S18 hardening tasks
- **Next commit should be**: MCB roundtrip test binary (STAB-0121 through STAB-0130)

---

## Do Not Do Yet

- No new product features (N8+, O-group, or any feature from old plan.md)
- No CNA changes (separate Claude Code instance)
- No `${meta-gl_SOURCE_DIR}/include` in CMakeLists.txt
- No Mc3Document public API changes without checking mc3togltf
- No task marked ✅ without: test written + registered + run + passes
