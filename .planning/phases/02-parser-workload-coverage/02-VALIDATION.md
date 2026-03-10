---
phase: 02
slug: parser-workload-coverage
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-03-07
---

# Phase 02 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Bazel `cc_binary` smoke/validation runners plus checked-in workload fixtures |
| **Config file** | `.bazelrc` |
| **Quick run command** | `just smoke-suite` plus targeted workload-summary fixture subset |
| **Full suite command** | Wave 1: `list` + `quick` + reconciliation/sign-off audit + `just smoke-suite` + `//ported_clickhouse:parser_lib` build. Wave 2: `supported` + `repeat` + reconciliation/sign-off audit + `just smoke-suite` + `//ported_clickhouse:parser_lib` build. Wave 3+: `bash tools/parser_workload_suite.sh full --manifest testdata/parser_workload/manifest.json` + reconciliation/sign-off audit + `just smoke-suite` + `//ported_clickhouse:parser_lib` build |
| **Estimated runtime** | ~120 seconds |

---

## Sampling Rate

- **After every task commit:** Run `just smoke-suite` plus the targeted workload-summary subset for the touched area
- **After Wave 1:** Run `list`, `quick`, reconciliation/sign-off audit, `just smoke-suite`, and parser-lib build
- **After Wave 2:** Run `supported`, `repeat`, reconciliation/sign-off audit, `just smoke-suite`, and parser-lib build
- **After Wave 3:** Run the aggregate `full` harness command, reconciliation/sign-off audit, `just smoke-suite`, and parser-lib build
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 120 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | PARS-02 | fixture corpus | `bash tools/parser_workload_suite.sh audit --manifest testdata/parser_workload/manifest.json` plus `bash tools/parser_workload_suite.sh list --manifest testdata/parser_workload/manifest.json` prove the fixture manifest is complete and enumerable | ❌ W0 | ⬜ pending |
| 02-01-02 | 01 | 1 | PARS-02 | workload reconciliation | `bash tools/parser_workload_suite.sh reconcile --manifest testdata/parser_workload/manifest.json --source testdata/parser_workload/prioritized_internal_workload.json --signoff docs/parser_workload_reconciliation.md` plus `bash tools/parser_workload_suite.sh audit-signoff --manifest testdata/parser_workload/manifest.json --source testdata/parser_workload/prioritized_internal_workload.json --signoff docs/parser_workload_reconciliation.md` prove the fixture corpus matches the prioritized workload source and the sign-off artifact is complete and in sync | ❌ W0 | ⬜ pending |
| 02-01-03 | 01 | 1 | PARS-02 | Bazel target contract | `bazel --batch --output_user_root=/tmp/bazel-root build //examples/bootstrap:parser_workload_summary` proves the dedicated workload-summary target exists | ❌ W0 | ⬜ pending |
| 02-01-04 | 01 | 1 | PARS-02 | wrapper commands | `bash tools/parser_workload_suite.sh list --manifest testdata/parser_workload/manifest.json`, `bash tools/parser_workload_suite.sh quick --manifest testdata/parser_workload/manifest.json`, and `bash tools/parser_workload_suite.sh audit-signoff --manifest testdata/parser_workload/manifest.json --source testdata/parser_workload/prioritized_internal_workload.json --signoff docs/parser_workload_reconciliation.md` prove deterministic entrypoints and sign-off validation commands exist | ❌ W0 | ⬜ pending |
| 02-01-05 | 01 | 1 | PARS-02 | baseline non-regression | `just smoke-suite` plus `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib //examples/bootstrap:parser_workload_summary` | ✅ | ⬜ pending |
| 02-02-01 | 02 | 2 | PARS-01, PARS-02 | canonical summary runner | `bazel --batch --output_user_root=/tmp/bazel-root run //examples/bootstrap:parser_workload_summary -- --manifest testdata/parser_workload/manifest.json --mode quick` proves the runner consumes the manifest-backed corpus through the planned CLI contract, emits stable summaries, and fails on unconsumed tokens for the targeted fixture set | ❌ W0 | ⬜ pending |
| 02-02-02 | 02 | 2 | PARS-01, PARS-02 | parser success matrix and determinism | `bash tools/parser_workload_suite.sh supported --manifest testdata/parser_workload/manifest.json` plus `bash tools/parser_workload_suite.sh repeat --manifest testdata/parser_workload/manifest.json` pass exact expected-summary checks, full token-consumption checks, and full-corpus repeat checks for all supported fixtures | ❌ W0 | ⬜ pending |
| 02-02-03 | 02 | 2 | PARS-02 | parser-layer fixes | `bash tools/parser_workload_suite.sh supported --manifest testdata/parser_workload/manifest.json` plus `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib` prove any fixes close workload gaps, fully consume tokens, and avoid breaking parser_lib | ✅ | ⬜ pending |
| 02-02-04 | 02 | 2 | PARS-02 | parser-layer fixes, provenance, and smoke non-regression | `bash tools/parser_workload_suite.sh supported --manifest testdata/parser_workload/manifest.json`, `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib`, `just smoke-suite`, and `rg -n "ported_clickhouse/parsers" docs/ported_clickhouse_manifest.md` prove workload fixes close parser gaps, preserve provenance, and keep the Phase 01 smoke baseline green | ✅ | ⬜ pending |
| 02-03-01 | 03 | 3 | PARS-03 | exclusions | `bash tools/parser_workload_suite.sh excluded --manifest testdata/parser_workload/manifest.json` proves excluded non-SQL, MSSQL, and deferred query-parameter fixtures fail deterministically | ✅ | ✅ pass |
| 02-03-02 | 03 | 3 | PARS-02, PARS-03 | scope docs | `bash tools/parser_workload_suite.sh audit-docs --manifest testdata/parser_workload/manifest.json --scope-doc docs/parser_workload_scope.md --roadmap-doc docs/select_parser_roadmap.md` proves supported/excluded docs match the manifest | ✅ | ✅ pass |
| 02-03-03 | 03 | 3 | PARS-01, PARS-02 | final acceptance and sign-off evidence | `bash tools/parser_workload_suite.sh full --manifest testdata/parser_workload/manifest.json`, `bash tools/parser_workload_suite.sh audit-signoff --manifest testdata/parser_workload/manifest.json --source testdata/parser_workload/prioritized_internal_workload.json --signoff docs/parser_workload_reconciliation.md`, `just smoke-suite`, and `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib` record the final full-suite acceptance run and prove the reconciliation sign-off still matches the final manifest | ✅ | ✅ pass |
| 02-03-04 | 03 | 3 | PARS-02, PARS-03 | final consistency | `bash tools/parser_workload_suite.sh audit-docs --manifest testdata/parser_workload/manifest.json --scope-doc docs/parser_workload_scope.md --validation-doc .planning/phases/02-parser-workload-coverage/02-VALIDATION.md --signoff docs/parser_workload_reconciliation.md` proves fixtures, docs, validation, and sign-off are aligned | ✅ | ✅ pass |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing Bazel parser baseline from Phase 01 remains green via `just smoke-suite`
- [ ] Checked-in workload fixture corpus for supported and excluded queries
- [ ] Normalized prioritized workload source snapshot plus checked-in reconciliation/sign-off artifact
- [ ] Dedicated workload-summary validation binary or test-only utility
- [ ] Fixture harness command for expected-summary comparisons
- [ ] Repeat-mode command for deterministic output checks across the full supported corpus

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 120s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-03-07

## Plan 02-03 Execution Evidence (2026-03-10)

### Command Log

1. `bash tools/parser_workload_suite.sh excluded --manifest testdata/parser_workload/manifest.json`
- Result: `excluded ok: 6 fixtures`
- Notes: Confirms deterministic excluded failure-class enforcement for non-SQL, non-select, MSSQL, and deferred query-parameter cases.

2. `bash tools/parser_workload_suite.sh audit-docs --manifest testdata/parser_workload/manifest.json --scope-doc docs/parser_workload_scope.md --roadmap-doc docs/select_parser_roadmap.md`
- Result: `audit-docs ok: supported=7 excluded=6 scope=docs/parser_workload_scope.md`

3. `bash tools/parser_workload_suite.sh full --manifest testdata/parser_workload/manifest.json`
- Result: `audit ok`, `reconcile report`, `audit-signoff ok`, `supported ok: 7 fixtures`, `excluded ok: 6 fixtures`, `repeat ok: 7 fixtures runs=3`

4. `bash tools/parser_workload_suite.sh audit-signoff --manifest testdata/parser_workload/manifest.json --source testdata/parser_workload/prioritized_internal_workload.json --signoff docs/parser_workload_reconciliation.md`
- Result: `audit-signoff ok: signoff metadata and source mappings are in sync`

5. `just smoke-suite`
- Result: green; smoke binaries and parser build targets completed successfully.

6. `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib`
- Result: build succeeded.

### Acceptance Status

- Full Phase 02 suite command set is reproducible and green in one run.
- Reconciliation sign-off metadata matches the final fixture manifest.
- Scope docs, roadmap scope anchor, and executable fixture matrix are aligned.
