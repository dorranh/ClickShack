---
phase: 02
slug: parser-workload-coverage
status: draft
nyquist_compliant: true
wave_0_complete: true
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
| **Full suite command** | workload fixture harness over the full supported corpus, determinism repeat checks, excluded-fixture negatives, and `//ported_clickhouse:parser_lib` build |
| **Estimated runtime** | ~120 seconds |

---

## Sampling Rate

- **After every task commit:** Run `just smoke-suite` plus the targeted workload-summary subset for the touched area
- **After every plan wave:** Run the full workload fixture harness and parser-lib build
- **Before `$gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 120 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | PARS-02 | fixture corpus | workload fixture manifest/labeled queries checked in and loadable by validation runner | ❌ W0 | ⬜ pending |
| 02-01-02 | 01 | 1 | PARS-01, PARS-02 | canonical summary | workload-summary binary emits deterministic machine-comparable output for supported fixtures | ❌ W0 | ⬜ pending |
| 02-02-01 | 02 | 2 | PARS-02 | parser success matrix | full supported fixture suite parses successfully with exact expected summaries | ❌ W0 | ⬜ pending |
| 02-02-02 | 02 | 2 | PARS-01 | determinism | repeat-mode verification emits identical output for representative supported fixtures | ❌ W0 | ⬜ pending |
| 02-03-01 | 03 | 3 | PARS-03 | exclusions | excluded non-SQL, MSSQL, and deferred query-parameter fixtures fail deterministically | ❌ W0 | ⬜ pending |
| 02-03-02 | 03 | 3 | PARS-03 | docs sync | supported/excluded scope documentation matches fixture matrix and phase requirements | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing Bazel parser baseline from Phase 01 remains green via `just smoke-suite`
- [ ] Checked-in workload fixture corpus for supported and excluded queries
- [ ] Dedicated workload-summary validation binary or test-only utility
- [ ] Fixture harness command for expected-summary comparisons
- [ ] Repeat-mode command for deterministic output checks

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Prioritized workload corpus matches actual internal needs | PARS-02 | Source workload ownership is outside the parser codebase | Review the checked-in fixture corpus against the current internal query list and approve the included/excluded set |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 120s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved 2026-03-07
