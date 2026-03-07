---
phase: 01
slug: build-baseline-recovery
status: approved
nyquist_compliant: true
wave_0_complete: true
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
| 01-01-02 | 01 | 1 | BUILD-01 | clean-checkout build | `git worktree add /tmp/clickshack-phase01-clean HEAD && (cd /tmp/clickshack-phase01-clean && bazel --batch --output_user_root=/tmp/bazel-root-clean build //ported_clickhouse:parser_lib) && git worktree remove /tmp/clickshack-phase01-clean` | ✅ | ⬜ pending |
| 01-02-01 | 02 | 2 | BUILD-02 | smoke | `bazel --batch --output_user_root=/tmp/bazel-root test //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke //examples/bootstrap:select_from_lite_smoke //examples/bootstrap:select_rich_smoke` | ✅ | ⬜ pending |
| 01-02-02 | 02 | 2 | BUILD-02 | clean-checkout smoke | `git worktree add /tmp/clickshack-phase01-clean-smoke HEAD && (cd /tmp/clickshack-phase01-clean-smoke && bazel --batch --output_user_root=/tmp/bazel-root-clean-smoke test //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke //examples/bootstrap:select_from_lite_smoke //examples/bootstrap:select_rich_smoke) && git worktree remove /tmp/clickshack-phase01-clean-smoke` | ✅ | ⬜ pending |
| 01-02-03 | 02 | 2 | BUILD-03 | tmp guard | `rg -n "(^|[\"' /])tmp/" MODULE.bazel MODULE.bazel.lock .bazelrc **/BUILD.bazel **/*.bzl` | ✅ | ⬜ pending |
| 01-02-04 | 02 | 2 | BUILD-03 | tmp-absent execution | `git worktree add /tmp/clickshack-phase01-no-tmp HEAD && (cd /tmp/clickshack-phase01-no-tmp && rm -rf tmp && test ! -e tmp && bazel --batch --output_user_root=/tmp/bazel-root-no-tmp build //ported_clickhouse:parser_lib && bazel --batch --output_user_root=/tmp/bazel-root-no-tmp test //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke) && git worktree remove /tmp/clickshack-phase01-no-tmp` | ✅ | ⬜ pending |
| 01-02-05 | 02 | 2 | BUILD-03 | tracked tmp count | `git ls-files tmp | wc -l` (expect `0`) | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Wave 0 is intentionally empty for Phase 01; all required acceptance checks are executable automated commands in Plan 01-01 and 01-02.
- [x] Clean-checkout and `tmp`-absence reproducibility proofs are captured as task-level automated verification, not deferred dependencies.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Clean-checkout reproducibility on fresh environment | BUILD-01, BUILD-02 | Requires cache/cross-machine confirmation beyond one local run | Clone fresh workspace, run build + full smoke commands, verify success without manual patch steps |
| Source-of-truth boundary (`tmp` remains untracked) | BUILD-03 | Requires human review of workflow/docs and accidental file additions | Run `git ls-files tmp`, inspect `git status`, verify no `tmp/*` tracked paths introduced |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 120s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved for execution
