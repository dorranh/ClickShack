---
phase: 01
slug: build-baseline-recovery
verified: 2026-03-07
status: passed
verifier: codex
---

# Phase 01 Verification

## Verdict

**Status: `passed`**

Phase 01 achieved its goal. Parser library builds, all four smoke binaries build and execute, and the build flow remains reproducible without tracked `tmp/` dependencies, including clean-worktree and no-`tmp` worktree proofs completed in this execution session.

## Phase Goal Check

**Goal:** Developers can reliably build and run parser smoke targets on the current Bazel version from a clean checkout.

### Current conclusion

- `//ported_clickhouse:parser_lib` builds successfully in the current workspace.
- All four parser smoke binaries build and execute successfully in the current workspace.
- Tracked Bazel/config sources do not reference `tmp/`, and `git ls-files tmp | wc -l` returns `0`.
- Clean-worktree and no-`tmp` worktree proofs were executed successfully in this session using elevated local-shell commands.

## Requirement Accounting

### BUILD-01

**Requirement:** Developer can build `//ported_clickhouse:parser_lib` successfully on current system Bazel version from a clean checkout.

**Code / artifact checks**

- `.bazelrc` no longer hard-codes machine-local default `CC`/`CXX` paths.
- `ported_clickhouse` BUILD files now load `cc_*` symbols from `@rules_cc//cc:defs.bzl`.
- `01-01-SUMMARY.md` documents the extra fix in `ported_clickhouse/core/BUILD.bazel`.

**Direct verification performed**

- `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib` -> passed.

**Recorded evidence**

- `01-01-SUMMARY.md` states workspace and clean-worktree parser-lib builds succeeded.
- `01-VALIDATION.md` records a green clean-checkout build proof for BUILD-01.

**Verifier assessment**

- Workspace behavior: passed.
- Clean-checkout behavior: passed.
- Requirement status: `passed`.

### BUILD-02

**Requirement:** Developer can run parser smoke targets (`use`, `select_lite`, `select_from_lite`, `select_rich`) successfully via Bazel.

**Code / artifact checks**

- `Justfile` provides `smoke-quick` and `smoke-suite`.
- `tools/parser_smoke_suite.sh` builds the required smoke target sets deterministically.
- `tools/cc_no_lld.sh` supplies the repo-env compiler shim described in the summary.
- `examples/bootstrap/BUILD.bazel` defines all four smoke binaries.

**Direct verification performed**

- `just smoke-suite` -> passed.
- Runtime execution passed for all four binaries:
  - `bazel-bin/examples/bootstrap/use_parser_smoke "USE mydb"` -> `mydb`
  - `bazel-bin/examples/bootstrap/select_lite_smoke "SELECT a, 1, f(x)"` -> `expressions=3 first=identifier:a`
  - `bazel-bin/examples/bootstrap/select_from_lite_smoke "SELECT a, 1, f(x) FROM mydb.mytable"` -> `expressions=3 table=mytable database=mydb first=identifier:a`
  - `bazel-bin/examples/bootstrap/select_rich_smoke "SELECT a FROM t"` -> rich-query summary output, exit `0`

**Recorded evidence**

- `01-02-SUMMARY.md` states workspace and clean-checkout smoke execution succeeded.
- `01-VALIDATION.md` records green workspace and clean-checkout smoke proofs.

**Verifier assessment**

- Workspace behavior: passed.
- Clean-checkout behavior: passed.
- Requirement status: `passed`.

### BUILD-03

**Requirement:** Build/test flow is reproducible without requiring any tracked source from `tmp/`.

**Code / artifact checks**

- `tools/check_tmp_boundary.sh` checks tracked build/config paths for `tmp/` references and fails if `tmp/` becomes tracked.
- `.bazelrc`, `ported_clickhouse/*`, `examples/bootstrap/*`, and `tools/*` contain no tracked `tmp/` references relevant to Bazel/config flow.

**Direct verification performed**

- `./tools/check_tmp_boundary.sh` -> passed.
- `git ls-files tmp | wc -l` -> `0`.
- `rg -n '(^|["'"'"' /])tmp/' MODULE.bazel MODULE.bazel.lock .bazelrc examples/bootstrap ported_clickhouse tools --glob '*.bzl' --glob 'BUILD.bazel'` -> no matches.

**Recorded evidence**

- `01-02-SUMMARY.md` states tmp-boundary guard and tmp-absent proof passed.
- `01-VALIDATION.md` records green tmp-boundary and tmp-absent execution proofs.

**Verifier assessment**

- Current tracked-source boundary: passed.
- Tmp-absent clean-worktree execution: passed.
- Requirement status: `passed`.

## Evidence Reviewed

- `01-01-PLAN.md`
- `01-02-PLAN.md`
- `01-01-SUMMARY.md`
- `01-02-SUMMARY.md`
- `01-VALIDATION.md`
- `ROADMAP.md`
- `REQUIREMENTS.md`
- `.bazelrc`
- `Justfile`
- `ported_clickhouse/BUILD.bazel`
- `ported_clickhouse/base/BUILD.bazel`
- `ported_clickhouse/common/BUILD.bazel`
- `ported_clickhouse/core/BUILD.bazel`
- `ported_clickhouse/parsers/BUILD.bazel`
- `examples/bootstrap/BUILD.bazel`
- `tools/parser_smoke_suite.sh`
- `tools/cc_no_lld.sh`
- `tools/check_tmp_boundary.sh`

## Notes

- `REQUIREMENTS.md` still needed a final status sync for `BUILD-02` when this report was created. Phase completion tracking should reconcile that documentation state.
