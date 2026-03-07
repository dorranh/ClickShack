---
phase: 01-build-baseline-recovery
plan: "01"
subsystem: infra
tags: [bazel, rules_cc, build, portability, clickhouse-parser]
requires:
  - phase: "00-discovery"
    provides: "Baseline roadmap and requirement IDs for build recovery work."
provides:
  - "Bazel 9 compatible rules_cc cc_* loading across parser and smoke BUILD targets."
  - "Portable default Bazel compiler environment without machine-local absolute CC/CXX paths."
  - "Verified parser_lib build in both workspace and clean-checkout worktree."
affects: [build-baseline, parser-library, smoke-targets, phase-01]
tech-stack:
  added: []
  patterns: ["Explicit rules_cc loads in BUILD files using cc_* rules", "Portable .bazelrc defaults with opt-in wrapper configs"]
key-files:
  created: []
  modified:
    - .bazelrc
    - examples/bootstrap/BUILD.bazel
    - ported_clickhouse/BUILD.bazel
    - ported_clickhouse/base/BUILD.bazel
    - ported_clickhouse/common/BUILD.bazel
    - ported_clickhouse/core/BUILD.bazel
    - ported_clickhouse/parsers/BUILD.bazel
key-decisions:
  - "Kept compiler wrapper usage opt-in via build:sanitize_wrapper instead of defaulting CC/CXX repo_env globally."
  - "Auto-fixed missing rules_cc load in ported_clickhouse/core/BUILD.bazel when Task 4 build verification exposed it."
patterns-established:
  - "Bazel 9 migration: every BUILD file using cc_* declares load(\"@rules_cc//cc:defs.bzl\", ...)."
  - "Portable defaults first; local toolchain wrappers must be explicit opt-in configs."
requirements-completed: [BUILD-01, BUILD-03]
duration: 5min
completed: 2026-03-07
---

# Phase 1 Plan 1: Bazel 9 Baseline Unblockers Summary

**Bazel 9 parser build path restored by adding rules_cc cc_* loads and removing machine-local compiler defaults from tracked Bazel config**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-07T12:55:55Z
- **Completed:** 2026-03-07T13:00:45Z
- **Tasks:** 4
- **Files modified:** 7

## Accomplishments
- Added Bazel 9 compatible `rules_cc` loads to all planned parser/smoke BUILD files that declare `cc_*` rules.
- Removed machine-local absolute CC/CXX defaults from `.bazelrc`, preserving sanitizer wrapper use as explicit opt-in.
- Verified `//ported_clickhouse:parser_lib` resolution and build viability in both active workspace and a temporary clean worktree.

## Task Commits

Each task was committed atomically:

1. **Task 1: Add rules_cc loads to all in-scope BUILD files using cc_* rules** - `07de39f` (feat)
2. **Task 2: Validate smoke label parse health with fast query checks** - `9aa9141` (chore, verification-only)
3. **Task 3: Remove required machine-local CC/CXX paths from tracked Bazel config** - `24f39f2` (fix)
4. **Task 4: Confirm parser library still builds with updated portable config** - `7ccdbe9` (chore, verification-only)

Additional deviation fix commit during Task 4: `a8cf333` (fix).

## Files Created/Modified
- `.bazelrc` - removed required machine-local compiler wrapper defaults; added opt-in sanitize wrapper config.
- `ported_clickhouse/BUILD.bazel` - loaded `cc_library` from `@rules_cc//cc:defs.bzl`.
- `ported_clickhouse/parsers/BUILD.bazel` - loaded `cc_library` from `@rules_cc//cc:defs.bzl`.
- `ported_clickhouse/common/BUILD.bazel` - loaded `cc_library` from `@rules_cc//cc:defs.bzl`.
- `ported_clickhouse/base/BUILD.bazel` - loaded `cc_library` from `@rules_cc//cc:defs.bzl`.
- `examples/bootstrap/BUILD.bazel` - loaded `cc_library`/`cc_binary` from `@rules_cc//cc:defs.bzl`.
- `ported_clickhouse/core/BUILD.bazel` - loaded `cc_library` from `@rules_cc//cc:defs.bzl` to unblock parser_lib analysis.

## Decisions Made
- Used a valid Bazel query expression (`set(...)`) for the smoke-label group because the literal multi-label query in the plan is syntactically invalid.
- Preserved optional wrapper support without coupling default builds to absolute local filesystem paths.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added missing rules_cc load in core BUILD file**
- **Found during:** Task 4 (Confirm parser library still builds with updated portable config)
- **Issue:** `ported_clickhouse/core/BUILD.bazel` still used removed native `cc_library`, causing parser_lib analysis failure.
- **Fix:** Added `load("@rules_cc//cc:defs.bzl", "cc_library")` at file top.
- **Files modified:** `ported_clickhouse/core/BUILD.bazel`
- **Verification:** `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib` and clean-worktree build both succeeded after fix.
- **Committed in:** `a8cf333`

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Deviation was required to satisfy Task 4 done criteria and clean-checkout baseline proof; no scope creep beyond parser build path.

## Issues Encountered
- Task 2 plan verification command listed multiple labels as a single query string, which Bazel rejects. Resolved by using `set(...)`.
- Worktree cleanup after clean build required `git worktree remove --force` because Bazel generated local files in the temporary worktree.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Parser build baseline is stable on Bazel 9 for targeted parser/smoke graph.
- Ready for Phase 1 Plan 2 execution.

## Self-Check: PASSED

- FOUND: `.planning/phases/01-build-baseline-recovery/01-01-SUMMARY.md`
- FOUND: `07de39f`
- FOUND: `9aa9141`
- FOUND: `24f39f2`
- FOUND: `a8cf333`
- FOUND: `7ccdbe9`

---
*Phase: 01-build-baseline-recovery*
*Completed: 2026-03-07*
