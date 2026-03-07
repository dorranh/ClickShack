# Phase 1 Research: Build Baseline Recovery

## Scope and Inputs

Phase: `01-build-baseline-recovery`

Primary requirements in scope:
- `BUILD-01`: Build `//ported_clickhouse:parser_lib` from clean checkout on current system Bazel
- `BUILD-02`: Run smoke targets (`use`, `select_lite`, `select_from_lite`, `select_rich`) via Bazel
- `BUILD-03`: Build/test flow must not require tracked source from `tmp/`

Reviewed planning/project context:
- `.planning/ROADMAP.md`
- `.planning/REQUIREMENTS.md`
- `.planning/STATE.md`

No `CLAUDE.md`, `.claude/skills/**`, or `.agents/skills/**` were present in this repo checkout.

## Current Baseline Observations (Measured)

Environment snapshot:
- Bazel: `bazel 9.0.0 Homebrew`
- `buildifier`: not installed (`NO_BUILDIFIER`)

Measured failures:
1. `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib`
   - Fails during BUILD evaluation with: native rule removed, add `load()` statement
   - Result: `//ported_clickhouse:parser_lib` not declared
2. `bazel --batch --output_user_root=/tmp/bazel-root build //examples/bootstrap:use_parser_smoke ...`
   - Same failure class in `examples/bootstrap/BUILD.bazel`

Repository-wide rule usage:
- `cc_*` rules are used in all key BUILD files (`examples/bootstrap`, `ported_clickhouse/*`), and none currently load `@rules_cc//cc:defs.bzl`.

Compiler wrapper mismatch:
- `.bazelrc` sets:
  - `repo_env=CC=/Users/dorran/dev/clickhouse-patching/clickshack/tools/clang_sanitize_fuse_ld.sh`
  - `repo_env=CXX=/Users/dorran/dev/clickhouse-patching/clickshack/tools/clang_sanitize_fuse_ld.sh`
- That absolute path does not exist in this checkout.
- The wrapper exists at local repo path: `tools/clang_sanitize_fuse_ld.sh`.

## Likely Root Causes After Bazel Upgrade

### RC-1 (High confidence): `cc_*` native symbol removal in Bazel 9

Why likely:
- Error is deterministic and immediate at BUILD parse time.
- Bazel error explicitly states the rule was removed and needs `load()`.
- Affects all targets in scope, blocking both `BUILD-01` and `BUILD-02`.

Expected remediation direction:
- Add top-level load lines in each BUILD file using `cc_library`/`cc_binary`, e.g.:
  - `load("@rules_cc//cc:defs.bzl", "cc_library", "cc_binary")`

### RC-2 (High confidence): Non-portable absolute `CC/CXX` path in `.bazelrc`

Why likely:
- `.bazelrc` points outside workspace to missing path.
- This creates machine-specific and checkout-location-specific behavior.
- Violates clean-checkout reproducibility intent (`BUILD-01`, `BUILD-03`).

Expected remediation direction:
- Replace absolute path with workspace-relative strategy.
- Prefer toolchain default unless sanitizer wrapper is strictly required.
- If wrapper required, use stable repo-root-derived pathing or configurable opt-in flag.

### RC-3 (Medium confidence): Output-base contention causing noisy/flake-like startup

Why likely:
- Concurrent Bazel invocations reported output base lock contention.
- Not root cause for failure, but can obscure diagnostics and appear flaky.

Expected remediation direction:
- Keep baseline verification serial for phase acceptance.
- If parallel CI is needed, shard by distinct `--output_user_root` or CI worker isolation.

## Required Checks Before Planning Implementation

### Check Group A: Bazel parsing and target definition health
- `bazel query //ported_clickhouse:parser_lib`
- `bazel query //examples/bootstrap:use_parser_smoke`
- `bazel query //examples/bootstrap:select_lite_smoke`
- `bazel query //examples/bootstrap:select_from_lite_smoke`
- `bazel query //examples/bootstrap:select_rich_smoke`

Pass criteria:
- Query resolves all labels without BUILD parse errors.

### Check Group B: Build execution (no test runtime yet)
- Build parser lib target from clean state.
- Build four smoke binaries from same invocation or deterministic sequence.

Pass criteria:
- Exit code 0; artifacts produced under `bazel-bin/examples/bootstrap/*_smoke`.

### Check Group C: Smoke runtime behavior
- Execute each smoke binary with representative SQL from Justfile.
- Confirm runtime exit code 0 for each command.

Pass criteria:
- All four smoke executables run successfully and parse expected baseline inputs.

### Check Group D: `tmp/` boundary enforcement
- Verify no build graph dependency on workspace `tmp/*`.
- Verify success with `tmp/` absent (or moved away) in a clean clone scenario.
- Verify tracked build config does not embed absolute paths to sibling repos.

Pass criteria:
- Build+smoke succeed when `tmp/` is unavailable.
- No tracked Bazel config references `tmp/` as required source-of-truth.

## Recommended Build/Test Matrix for Phase 1

### Core acceptance matrix (minimum)
1. **Clean checkout, default environment**
   - `bazel clean --expunge`
   - build `//ported_clickhouse:parser_lib`
   - build smoke binaries
   - run smoke binaries
2. **Clean checkout, no `tmp/` available**
   - same steps as above with `tmp/` temporarily moved/unavailable
3. **Clean checkout, fresh Bazel module resolution**
   - remove local output root and rerun to force module fetch/resolve

### Optional stretch matrix (if time permits in Phase 1)
1. Repeat core acceptance on second machine/runner.
2. Repeat with empty Bazel disk cache.

## Reproducibility Strategy (Clean Checkout)

### Baseline workflow to standardize
1. Clone repo fresh.
2. Ensure `bazel --version` is captured in logs.
3. Run deterministic bootstrap command set (single script or `just` target).
4. Build parser + smoke targets.
5. Execute smoke binaries with pinned input strings.
6. Persist command transcript for comparison.

### Controls to increase determinism
- Use fixed `--output_user_root=/tmp/bazel-root` during Phase 1 checks.
- Keep acceptance runs single-threaded at orchestration level (avoid concurrent Bazel client lock contention).
- Prefer committed `MODULE.bazel.lock` as dependency resolution anchor.
- Keep environment contract explicit (required tools, Bazel major version, compiler assumptions).

## Tmp Boundary Enforcement Strategy

Objective: satisfy `BUILD-03` by proving `tmp/` is not required input for tracked build/test.

Recommended controls:
1. Add CI/precheck that fails if tracked Bazel files reference workspace `tmp/` paths.
2. Add a clean-checkout check that runs build/smokes with `tmp/` absent.
3. Keep provenance and scratch policy explicit:
   - `ported_clickhouse/*` is source of truth for curated parser bridge.
   - `tmp/*` remains non-required scratch/reference.
4. Remove machine-local absolute paths from `.bazelrc`.

## Phase 1 Implementation Guidance (for planning)

Proposed task ordering:
1. Fix Bazel 9 compatibility in BUILD files by adding required `rules_cc` loads.
2. Fix `.bazelrc` portability (remove/replace absolute `CC/CXX` pathing).
3. Add/adjust a deterministic smoke-run command (likely in `just` or script).
4. Add tmp-boundary guard check.
5. Run clean-checkout validation matrix and capture evidence.

Risk notes:
- Introducing loads must be consistent across all affected BUILD files; partial edits leave phase blocked.
- Compiler wrapper behavior may have hidden assumptions; validate both with and without wrapper if needed.

## Requirement Coverage Mapping

- `BUILD-01`: unblocked by RC-1 + RC-2 remediation and clean build validation.
- `BUILD-02`: unblocked by RC-1 remediation; validated by smoke binary build/run checks.
- `BUILD-03`: enforced by tmp-boundary checks and removal of non-portable path assumptions.

## Validation Architecture

### Validation Layers
1. **Static config validation**
   - Detect forbidden tracked references (`tmp/`, machine-local absolute compiler wrapper paths).
2. **Target resolution validation**
   - `bazel query` for phase labels to catch BUILD parse/regression early.
3. **Build validation**
   - Deterministic build of `//ported_clickhouse:parser_lib` and smoke binaries from clean state.
4. **Runtime smoke validation**
   - Execute `use`, `select_lite`, `select_from_lite`, `select_rich` with fixed inputs.
5. **Reproducibility validation**
   - Repeat from fresh output root and with `tmp/` absent.

### Evidence Artifacts to Capture
- Bazel version line
- Query results for phase labels
- Build logs for parser and smokes
- Smoke runtime stdout/stderr and exit codes
- Explicit check result proving no required `tmp/` dependency

### Nyquist-ready Validation Assertions
- A1: All phase labels are queryable on current Bazel.
- A2: Parser lib builds from clean checkout.
- A3: All four smoke targets build and run successfully.
- A4: Build/test succeeds without `tmp/` being present.
- A5: No tracked Bazel config hardcodes machine-local compiler wrapper paths.
