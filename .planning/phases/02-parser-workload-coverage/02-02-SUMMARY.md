---
phase: 02-parser-workload-coverage
plan: "02"
subsystem: parser
tags: [bazel, clickhouse-parser, workload-fixtures, determinism]
requires:
  - phase: 02-01
    provides: Workload fixture corpus, reconciliation, and harness command surface
provides:
  - Manifest-driven workload summary runner contract with quick/supported/excluded/repeat modes
  - Full-corpus determinism checks executed inside the runner repeat mode
  - Stable bazel wrapper defaults for direct parser workload runner invocations
affects: [phase-02, parser-validation, workload-suite]
tech-stack:
  added: []
  patterns:
    - Centralize canonical summary assertions in the runner binary
    - Keep harness scripts as orchestrators over runner modes
key-files:
  created: []
  modified:
    - .bazelrc
    - examples/bootstrap/parser_workload_summary.cc
    - tools/parser_workload_suite.sh
    - docs/ported_clickhouse_manifest.md
key-decisions:
  - "Moved supported/excluded/repeat assertion logic into parser_workload_summary so a single binary owns canonical summary and determinism enforcement."
  - "Configured bazel repo env PATH+CC/CXX defaults to resolve the local cc_no_lld wrapper for direct plan verification commands."
  - "Recorded Task 3 as an explicit no-op commit after fixture matrix validation showed no parser-layer defects."
patterns-established:
  - "Workload validation entrypoint: parser_workload_summary --manifest <path> --mode <mode>"
  - "Repeat determinism checks run inside the parser runner with fixed multi-run comparison."
requirements-completed: [PARS-01, PARS-02, PARS-03]
duration: 10 min
completed: 2026-03-10
---

# Phase 02 Plan 02: Canonical Summary Runner and Fixture-Driven Parser Fixes Summary

**Manifest-driven parser workload summary contract now enforces exact canonical outputs, full token consumption, and repeat determinism across all supported fixtures.**

## Performance

- **Duration:** 10 min
- **Started:** 2026-03-10T15:17:14Z
- **Completed:** 2026-03-10T15:26:43Z
- **Tasks:** 4
- **Files modified:** 4

## Accomplishments
- Added `--manifest` + `--mode <quick|supported|excluded|repeat>` support directly in the workload summary runner.
- Centralized exact summary comparisons, excluded failure-class checks, and repeat drift checks in the runner binary.
- Kept parser baseline green and updated provenance docs while validating no fixture-driven parser-layer changes were needed.

## Task Commits

1. **Task 1: Implement a dedicated workload-summary runner with stable structural output** - `4cb0c7b` (feat)
2. **Task 2: Implement exact summary comparison and repeat-mode determinism checks** - `20f4ac8` (feat)
3. **Task 3: Patch only the parser layers exposed by failing supported fixtures** - `7f64810` (chore)
4. **Task 4: Update parser provenance and keep smoke coverage green while landing workload fixes** - `e7b1dd4` (docs)

## Files Created/Modified
- `.bazelrc` - Added repo env defaults so direct bazel plan commands resolve `cc_no_lld.sh`.
- `examples/bootstrap/parser_workload_summary.cc` - Added manifest-mode contract, exact output checks, excluded classification checks, and repeat determinism logic.
- `tools/parser_workload_suite.sh` - Delegates supported/excluded/repeat checks to runner modes.
- `docs/ported_clickhouse_manifest.md` - Added provenance row for `parser_workload_summary.cc`.

## Decisions Made
- Centralized workload-summary assertions and repeat checks in one C++ runner instead of duplicating comparison logic in shell.
- Treated Bazel linker-wrapper resolution as a blocking environment issue and fixed it within plan scope for reliable verification commands.
- Kept parser-layer files unchanged after supported matrix confirmed no fixture-driven parse gaps.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed direct bazel runner command failures in this environment**
- **Found during:** Task 1
- **Issue:** Plan verification command `bazel ... run //examples/bootstrap:parser_workload_summary -- --manifest ... --mode quick` failed due linker-wrapper/toolchain resolution issues and Bazel run-context path resolution.
- **Fix:** Added repo env defaults in `.bazelrc` and workspace-aware path resolution in `parser_workload_summary.cc`.
- **Files modified:** `.bazelrc`, `examples/bootstrap/parser_workload_summary.cc`
- **Verification:** Plan quick command now passes; supported/repeat suite commands pass.
- **Committed in:** `4cb0c7b`

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Required to make mandated verification commands reproducible; no scope creep.

## Issues Encountered
- `std::filesystem` use in the runner failed under the repository's macOS deployment target constraints; replaced with portable string-based path helpers.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Plan 02-02 verification gates are green (`quick`, `supported`, `repeat`, `parser_lib`, `smoke-suite`).
- Ready for Plan 02-03 exclusion and documentation closure tasks.

## Self-Check: PASSED
- Found summary file: `.planning/phases/02-parser-workload-coverage/02-02-SUMMARY.md`
- Found task commits: `4cb0c7b`, `20f4ac8`, `7f64810`, `e7b1dd4`
