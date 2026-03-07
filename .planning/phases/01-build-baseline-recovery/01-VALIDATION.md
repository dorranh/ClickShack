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
| **Quick run command** | `just smoke-quick` |
| **Full suite command** | `just smoke-suite` |
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
| 01-02-01 | 02 | 2 | BUILD-02 | smoke | `just smoke-suite` plus runtime execution of all four `bazel-bin/examples/bootstrap/*_smoke` binaries | ✅ | ✅ green |
| 01-02-02 | 02 | 2 | BUILD-02 | clean-checkout smoke | `git worktree add /tmp/clickshack-phase01-clean-smoke HEAD && (cd /tmp/clickshack-phase01-clean-smoke && PATH="$PWD/tools:$PATH" bazel --batch --output_user_root=/tmp/bazel-root-clean-smoke build --repo_env=CC=cc_no_lld.sh --repo_env=CXX=cc_no_lld.sh //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke //examples/bootstrap:select_from_lite_smoke //examples/bootstrap:select_rich_smoke && bazel-bin/examples/bootstrap/use_parser_smoke "USE mydb" && bazel-bin/examples/bootstrap/select_lite_smoke "SELECT a, 1, f(x)" && bazel-bin/examples/bootstrap/select_from_lite_smoke "SELECT a, 1, f(x) FROM mydb.mytable" && bazel-bin/examples/bootstrap/select_rich_smoke "SELECT a FROM t") && git worktree remove --force /tmp/clickshack-phase01-clean-smoke` | ✅ | ✅ green |
| 01-02-03 | 02 | 2 | BUILD-03 | tmp guard | `./tools/check_tmp_boundary.sh` | ✅ | ✅ green |
| 01-02-04 | 02 | 2 | BUILD-03 | tmp-absent execution | `git worktree add /tmp/clickshack-phase01-no-tmp HEAD && (cd /tmp/clickshack-phase01-no-tmp && rm -rf tmp && test ! -e tmp && PATH="$PWD/tools:$PATH" bazel --batch --output_user_root=/tmp/bazel-root-no-tmp build --repo_env=CC=cc_no_lld.sh --repo_env=CXX=cc_no_lld.sh //ported_clickhouse:parser_lib //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke && bazel-bin/examples/bootstrap/use_parser_smoke "USE mydb" && bazel-bin/examples/bootstrap/select_lite_smoke "SELECT a, 1, f(x)") && git worktree remove --force /tmp/clickshack-phase01-no-tmp` | ✅ | ✅ green |
| 01-02-05 | 02 | 2 | BUILD-03 | tracked tmp count | `git ls-files tmp | wc -l` (expect `0`) | ✅ | ✅ green |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

## Execution Evidence

### 2026-03-07

| Check | Result |
|-------|--------|
| Workspace quick smoke | `just smoke-quick` completed successfully |
| Workspace full smoke build | `just smoke-suite` completed successfully |
| Workspace runtime smoke outputs | `use_parser_smoke -> mydb`; `select_lite_smoke -> expressions=3 first=identifier:a`; `select_from_lite_smoke -> expressions=3 table=mytable database=mydb first=identifier:a`; `select_rich_smoke -> projections=1 ... source_kinds=table ...` |
| Clean-checkout full smoke proof | Fresh worktree at `/tmp/clickshack-phase01-clean-smoke` built all four smoke targets with `cc_no_lld.sh` repo env overrides and all four binaries exited `0` |
| Tmp boundary guard | `./tools/check_tmp_boundary.sh` exited `0` with no tracked build/config `tmp/` references and no tracked `tmp/` files |
| Tmp-absent proof | Fresh worktree at `/tmp/clickshack-phase01-no-tmp` removed `tmp/`, then built `//ported_clickhouse:parser_lib`, `use_parser_smoke`, and `select_lite_smoke`; both binaries exited `0` |
| Parser lib regression | `PATH="$PWD/tools:$PATH" bazel --batch --output_user_root=/tmp/bazel-root build --repo_env=CC=cc_no_lld.sh --repo_env=CXX=cc_no_lld.sh //ported_clickhouse:parser_lib` was up to date and successful |
| Tracked tmp count | `git ls-files tmp | wc -l` returned `0` |

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
