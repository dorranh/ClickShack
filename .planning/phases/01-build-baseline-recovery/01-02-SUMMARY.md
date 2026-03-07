---
phase: 01-build-baseline-recovery
plan: "02"
subsystem: infra
tags: [bazel, smoke, reproducibility, parser, portability]
requires:
  - phase: "01-build-baseline-recovery"
    provides: "Bazel 9 compatible parser and smoke target graph with portable rules_cc loads."
provides:
  - "Deterministic smoke entrypoints for quick and full parser validation."
  - "MacOS-safe Bazel C++ toolchain detection that avoids misdetected lld linker flags."
  - "Tmp-boundary guard and reproducibility evidence for clean-checkout and tmp-absent proofs."
affects: [phase-01, smoke-targets, parser-library, reproducibility]
tech-stack:
  added: []
  patterns: ["Repo-env compiler shim to suppress broken lld auto-detection", "Executable tmp-boundary guard for tracked Bazel inputs"]
key-files:
  created:
    - tools/cc_no_lld.sh
    - tools/parser_smoke_suite.sh
    - tools/check_tmp_boundary.sh
    - .planning/phases/01-build-baseline-recovery/01-02-SUMMARY.md
  modified:
    - Justfile
    - .planning/phases/01-build-baseline-recovery/01-VALIDATION.md
key-decisions:
  - "Used a PATH-resolved CC/CXX shim instead of .bazelrc repo_env paths because rules_cc rejects slash-containing compiler overrides during fresh toolchain detection."
  - "Validated smoke binaries by both building and executing them because the planned Bazel test commands target cc_binary smoke executables rather than cc_test rules."
patterns-established:
  - "Parser smoke entrypoints run through checked-in wrappers so clean worktrees reproduce host toolchain quirks consistently."
  - "Tmp-boundary enforcement is executable and can fail fast before longer Bazel runs."
requirements-completed: [BUILD-02, BUILD-03, BUILD-01]
duration: 15min
completed: 2026-03-07
---

# Phase 1 Plan 2: Smoke and Reproducibility Proof Summary

**Deterministic parser smoke entrypoints with clean-checkout and tmp-absent reproducibility proof on the current macOS Bazel toolchain**

## Performance

- **Duration:** 15 min
- **Started:** 2026-03-07T14:24:00Z
- **Completed:** 2026-03-07T14:39:07Z
- **Tasks:** 4
- **Files modified:** 5

## Accomplishments
- Added stable `just smoke-quick` and `just smoke-suite` entrypoints backed by checked-in toolchain handling.
- Proved all four smoke binaries build and execute successfully in both the active workspace and a fresh clean-checkout worktree.
- Enforced the `tmp/` source-of-truth boundary with an executable guard and a separate no-`tmp` worktree proof.
- Captured exact reproducibility evidence and outputs in the phase validation document.

## Task Commits

Each task was committed atomically:

1. **Task 1: Add deterministic smoke-suite entrypoint for fast repeat runs** - `e875655` (feat)
2. **Task 2: Execute full required smoke suite in one deterministic command** - `df53ce0` (chore, verification-only)
3. **Task 3: Enforce tmp boundary guard on tracked Bazel/config sources** - `3a6127a` (feat)
4. **Task 4: Capture reproducibility evidence and parser-lib regression check** - `31938cb` (docs)

## Files Created/Modified
- `Justfile` - exposes `smoke-quick` and `smoke-suite` entrypoints.
- `tools/parser_smoke_suite.sh` - builds quick/full smoke target sets with repo-env compiler overrides.
- `tools/cc_no_lld.sh` - prevents rules_cc from misdetecting `lld` or `gold` as valid Apple linker backends.
- `tools/check_tmp_boundary.sh` - fails if tracked Bazel/config sources reference `tmp/` or if `tmp/` becomes tracked.
- `.planning/phases/01-build-baseline-recovery/01-VALIDATION.md` - records executed commands, results, and runtime outputs for BUILD-02/03 evidence.

## Decisions Made
- Used a basename-only compiler shim on `PATH` because fresh Bazel/rules_cc toolchain detection rejects slash-based `CC` overrides from repo env.
- Kept the smoke entrypoint as a checked-in script so the same invocation works in the workspace and temporary worktrees.
- Treated runtime execution of the smoke binaries as part of acceptance because these targets are executable smoke binaries, not Bazel test rules.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Replaced invalid macOS linker auto-detection with a compiler shim**
- **Found during:** Task 1 (Add deterministic smoke-suite entrypoint for fast repeat runs)
- **Issue:** Bazel rules_cc generated `-fuse-ld=ld64.lld:` plus GNU-only linker flags on macOS, causing all smoke links to fail in fresh toolchain detection.
- **Fix:** Added `tools/cc_no_lld.sh` and routed smoke builds through repo-env `CC/CXX` overrides with the shim on `PATH`.
- **Files modified:** `Justfile`, `tools/parser_smoke_suite.sh`, `tools/cc_no_lld.sh`
- **Verification:** `just smoke-quick`, `just smoke-suite`, clean-checkout smoke build, and runtime smoke execution all passed.
- **Committed in:** `e875655`

**2. [Rule 3 - Blocking] Executed smoke binaries directly because the planned Bazel test commands target cc_binary rules**
- **Found during:** Task 2 (Execute full required smoke suite in one deterministic command)
- **Issue:** The plan's `bazel test` verification commands reference `cc_binary` smoke targets, so Bazel build success alone would not prove parser execution behavior.
- **Fix:** Added direct execution of each built smoke binary with canonical queries in both workspace and clean-checkout proofs.
- **Files modified:** None (verification flow only)
- **Verification:** All four binaries exited `0` and printed the expected parse summaries.
- **Committed in:** `df53ce0`

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both deviations were required to make the planned smoke/repro checks executable on the current macOS Bazel toolchain. No scope creep beyond acceptance proofing.

## Issues Encountered
- Fresh Bazel toolchain detection on macOS misdetected `lld` support and injected GNU-only linker flags that Apple `ld` rejects.
- Clean worktree proofs re-touched `MODULE.bazel.lock`; that churn was restored and excluded from the committed plan artifacts.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Phase 1 acceptance proof is complete: parser lib builds, smoke binaries execute, and no tracked `tmp/` dependency is required.
- Ready for phase-level verification and transition into Phase 2 planning/execution.

## Self-Check: PASSED

- FOUND: `.planning/phases/01-build-baseline-recovery/01-02-SUMMARY.md`
- FOUND: `e875655`
- FOUND: `df53ce0`
- FOUND: `3a6127a`
- FOUND: `31938cb`

---
*Phase: 01-build-baseline-recovery*
*Completed: 2026-03-07*
