---
phase: 02-parser-workload-coverage
plan: "01"
subsystem: testing
tags: [parser, workload-fixtures, bazel, reconciliation]
requires:
  - phase: 01-build-baseline-recovery
    provides: parser smoke/build baseline and deterministic Bazel invocation patterns
provides:
  - checked-in supported and excluded parser workload fixture corpus with manifest metadata
  - source-to-fixture reconciliation snapshot plus audited sign-off artifact
  - dedicated parser workload summary Bazel target for machine-comparable outputs
  - deterministic wrapper commands for corpus audit/list/quick/supported/excluded/repeat/full
affects: [02-02-PLAN, 02-03-PLAN, parser validation workflow]
tech-stack:
  added: [none]
  patterns: [fixture-first parser workload validation, signoff metadata auditing, bazel artifact fallback under sandbox]
key-files:
  created:
    - testdata/parser_workload/manifest.json
    - testdata/parser_workload/prioritized_internal_workload.json
    - docs/parser_workload_reconciliation.md
    - examples/bootstrap/parser_workload_summary.cc
  modified:
    - examples/bootstrap/BUILD.bazel
    - tools/parser_workload_suite.sh
    - tools/parser_smoke_suite.sh
key-decisions:
  - "Use a checked-in manifest keyed by normalized workload_source_id so every source row maps to exactly one fixture."
  - "Keep workload summary output aligned with select_rich_smoke field set for exact stable comparisons."
  - "Treat Bazel exit-37 sandbox shutdown crashes as non-fatal only when expected artifacts are confirmed on disk."
patterns-established:
  - "Manifest discipline: success fixtures must have exact expected_canonical_summary; excluded fixtures must carry explicit exclusion_reason."
  - "Reconciliation discipline: source snapshot + sign-off YAML block are audited against computed totals."
requirements-completed: [PARS-01, PARS-02, PARS-03]
duration: 11min
completed: 2026-03-10
---

# Phase 02 Plan 01: Workload Corpus and Harness Contract Summary

**Parser workload coverage now ships as a checked-in fixture corpus with audited source reconciliation, a dedicated summary binary target, and deterministic wrapper commands for fast and full validation paths.**

## Performance

- **Duration:** 11 min
- **Started:** 2026-03-10T14:57:33Z
- **Completed:** 2026-03-10T15:08:51Z
- **Tasks:** 5
- **Files modified:** 19

## Accomplishments
- Added a concrete Phase 02 workload boundary with 12 checked-in fixtures (7 supported, 5 excluded) and strict manifest schema validation.
- Added normalized workload-source snapshot reconciliation with auditable sign-off metadata and deterministic coverage checks.
- Added a dedicated `parser_workload_summary` Bazel target and wrapper command surface (`quick`, `supported`, `excluded`, `repeat`, `full`) for workload-driven parser validation.
- Preserved Phase 01 baseline checks by hardening smoke-suite handling for sandbox-specific Bazel shutdown exits.

## Task Commits

1. **Task 1: Check in the supported and excluded workload fixture corpus** - `8cc04af` (feat)
2. **Task 2: Reconcile the checked-in corpus against the real prioritized workload source and record sign-off** - `d8485b4` (feat)
3. **Task 3: Add Bazel wiring for a dedicated workload-summary validation target** - `1ce5623` (feat)
4. **Task 4: Add deterministic wrapper commands for fixture listing and targeted subset runs** - `3d229df` (feat)
5. **Task 5: Prove the new workload entrypoints do not regress the Phase 01 parser baseline** - `01dcd51` (fix)

## Files Created/Modified
- `testdata/parser_workload/` - supported/excluded SQL fixtures plus manifest and source snapshot.
- `docs/parser_workload_reconciliation.md` - checked-in reconciliation/sign-off artifact with auditable metadata.
- `examples/bootstrap/parser_workload_summary.cc` - parser summary runner with deterministic failure classification.
- `examples/bootstrap/BUILD.bazel` - new `//examples/bootstrap:parser_workload_summary` target.
- `tools/parser_workload_suite.sh` - audit/list/reconcile/audit-signoff/quick/supported/excluded/repeat/full command surface.
- `tools/parser_smoke_suite.sh` - artifact-aware fallback for sandbox Bazel shutdown crashes after successful builds.

## Decisions Made
- Standardized workload coverage around one manifest entry per normalized source identifier.
- Kept canonical summary comparisons text-exact to avoid ambiguous parser success assertions.
- Added explicit sandbox Bazel fallback checks instead of suppressing errors blindly.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Bazel exits non-zero in sandbox after successful build completion**
- **Found during:** Task 4 and Task 5 verification commands
- **Issue:** `bazel --batch` intermittently returned exit code `37` after reporting successful builds due sandbox JVM/sysctl restrictions.
- **Fix:** Added artifact-presence fallback handling in workload and smoke wrapper scripts so commands fail only when expected binaries are truly missing.
- **Files modified:** `tools/parser_workload_suite.sh`, `tools/parser_smoke_suite.sh`
- **Verification:** `quick`, `just smoke-suite`, and combined parser build commands complete while still requiring built artifacts.
- **Committed in:** `3d229df`, `01dcd51`

---

**Total deviations:** 1 auto-fixed (Rule 3 blocking)
**Impact on plan:** Required to make planned verification deterministic in this sandbox; no product-scope expansion.

## Issues Encountered
- Sandbox restrictions cause intermittent Bazel JVM shutdown crashes despite successful build outputs; wrapper-level artifact checks were used to keep validation reliable.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Phase 02 now has a stable, checked-in workload contract and executable validation entrypoints for parser gap-closing work in Plans 02 and 03.
- Reconciliation and sign-off audits are in place to prevent fixture/source drift.

---
*Phase: 02-parser-workload-coverage*
*Completed: 2026-03-10*

## Self-Check: PASSED

- Found summary file: `.planning/phases/02-parser-workload-coverage/02-01-SUMMARY.md`
- Found task commits: `8cc04af`, `d8485b4`, `1ce5623`, `3d229df`, `01dcd51`
