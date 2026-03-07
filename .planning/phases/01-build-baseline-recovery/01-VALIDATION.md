---
phase: 01
slug: build-baseline-recovery
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-07
---

# Phase 01 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Bazel `cc_test`/`cc_binary` smoke executables |
| **Config file** | `.bazelrc` |
| **Quick run command** | `bazel --batch --output_user_root=/tmp/bazel-root test //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke` |
| **Full suite command** | `bazel --batch --output_user_root=/tmp/bazel-root test //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke //examples/bootstrap:select_from_lite_smoke //examples/bootstrap:select_rich_smoke` |
| **Estimated runtime** | ~90 seconds |

---

## Sampling Rate

- **After every task commit:** Run quick smoke tests.
- **After every plan wave:** Run full parser smoke suite.
- **Before `$gsd-verify-work`:** Full suite must be green.
- **Max feedback latency:** 120 seconds.

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 01-01-01 | 01 | 1 | BUILD-01 | build | `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib` | ✅ | ⬜ pending |
| 01-01-02 | 01 | 1 | BUILD-02 | smoke | `bazel --batch --output_user_root=/tmp/bazel-root test //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke` | ✅ | ⬜ pending |
| 01-02-01 | 02 | 2 | BUILD-02 | smoke | `bazel --batch --output_user_root=/tmp/bazel-root test //examples/bootstrap:select_from_lite_smoke //examples/bootstrap:select_rich_smoke` | ✅ | ⬜ pending |
| 01-02-02 | 02 | 2 | BUILD-03 | integration | `git ls-files tmp | wc -l` (expect `0`) | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/` harness expansion for parser assertions (deferred; current phase uses smoke targets)
- [ ] Standardized clean-checkout script for reproducibility checks (`tools/` script)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Clean-checkout reproducibility on fresh environment | BUILD-01, BUILD-02 | Requires cache/cross-machine confirmation beyond one local run | Clone fresh workspace, run build + full smoke commands, verify success without manual patch steps |
| Source-of-truth boundary (`tmp` remains untracked) | BUILD-03 | Requires human review of workflow/docs and accidental file additions | Run `git ls-files tmp`, inspect `git status`, verify no `tmp/*` tracked paths introduced |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 120s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
