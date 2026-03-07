# Pitfalls Research

**Domain:** Standalone C++ SQL parser extraction (ClickHouse parser bridge)
**Researched:** 2026-03-07
**Confidence:** HIGH

## Critical Pitfalls

### Pitfall 1: Treating build green as parser-correct

**What goes wrong:**
Bazel targets compile and smoke tests pass, but parser behavior regresses on real internal `SELECT` workloads (wrong AST shape, partial parse, silent fallback).

**Why it happens:**
Build stabilization is an urgent blocker, so teams over-index on compile/link success and under-sample query-behavior correctness.

**How to avoid:**
Maintain a workload corpus of representative internal queries and assert parse success plus AST/IR invariants in CI, not only smoke binaries.

**Warning signs:**
- New parser changes only update build targets/tests, not query fixtures
- Smoke tests stay green while downstream tooling reports more parse failures
- AST node counts or key fields drift between commits without intentional change notes

**Phase to address:**
Phase 1 (Bazel stabilization) and Phase 2 (workload coverage hardening)

---

### Pitfall 2: Canonical code drifting into `tmp/` workspace

**What goes wrong:**
Critical fixes are made in `tmp/` upstream material, then lost or diverge from tracked bridge code in `ported_clickhouse/*`.

**Why it happens:**
`tmp/` is convenient during extraction/debugging, and contributors treat it as active source instead of reference/patch workspace.

**How to avoid:**
Enforce a rule: only `ported_clickhouse/*` is canonical. Mirror any exploratory `tmp/` patch immediately into tracked bridge targets, and fail CI if tracked files under `tmp/` appear.

**Warning signs:**
- "Works locally" depends on untracked `tmp/` edits
- Diffs mention manual copy steps from `tmp/`
- Rebuilds on fresh clones lose parser behavior or fixes

**Phase to address:**
Phase 1 (source-boundary enforcement)

---

### Pitfall 3: Pulling heavy upstream dependencies through parser layers

**What goes wrong:**
Parser targets begin depending on broad `common`/adjacent ClickHouse subsystems, causing slow builds, fragile link graphs, and larger maintenance burden.

**Why it happens:**
Fastest short-term fix is often adding upstream deps instead of isolating interfaces or stubbing required pieces.

**How to avoid:**
Track dependency budget per parser target; require justification for each new external dep; prefer narrow adapters/stubs for non-parser concerns.

**Warning signs:**
- Rapid growth in transitive deps for `//ported_clickhouse:parser_lib`
- Frequent ODR/link errors tied to utility libs
- Build times increase after "small" parser changes

**Phase to address:**
Phase 4 (dependency-pruning and layering enforcement)

---

### Pitfall 4: Over-chasing full parity including excluded branches

**What goes wrong:**
Effort is spent porting MSSQL/non-SQL branches or analyzer-adjacent behavior that is explicitly out of scope, delaying value for internal `SELECT` workflows.

**Why it happens:**
"Near-full parity" is interpreted as "everything," ignoring explicit exclusions and parser-only scope.

**How to avoid:**
Maintain a parity matrix split into in-scope SQL vs excluded branches; reject roadmap/tasks that do not improve target workload coverage.

**Warning signs:**
- PRs add grammar paths for excluded dialects
- Planning docs reference semantic/analyzer parity
- Internal workload failure rate remains flat while code volume rises

**Phase to address:**
Phase 5 (parity tuning with exclusion guardrails)

---

### Pitfall 5: Unstable AST-to-IR contract for consumers

**What goes wrong:**
Downstream tooling breaks because IR shape changes between commits without versioning, compatibility checks, or migration guidance.

**Why it happens:**
IR generation is treated as implementation detail rather than a public internal contract.

**How to avoid:**
Define versioned IR schema, golden outputs for representative queries, and explicit compatibility policy (backward compatible vs version bump).

**Warning signs:**
- Frequent IR field renames/reordering without changelog
- Consumer adapters pinned to commit hashes
- "Parser update" PRs require immediate downstream hotfixes

**Phase to address:**
Phase 3 (stable IR contract definition and verification)

---

### Pitfall 6: Grammar extraction without provenance discipline

**What goes wrong:**
Ported parser behavior drifts from upstream ClickHouse in unclear ways; regressions become hard to diagnose because source lineage is opaque.

**Why it happens:**
Manifest/provenance docs are not updated when cherry-picking upstream changes or applying local adaptations.

**How to avoid:**
Keep `docs/ported_clickhouse_manifest.md` updated per sync change, annotate intentional deviations, and tie commits to upstream refs.

**Warning signs:**
- Parser files changed without manifest updates
- Team cannot answer "which upstream revision introduced this grammar"
- Regression triage requires manual archaeology

**Phase to address:**
Phase 2 (coverage work) and Phase 5 (parity maintenance)

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Add broad upstream `common` deps to unblock compile | Fast unblocking | Dependency bloat and brittle linking | Only as time-boxed bridge with follow-up removal task in same phase |
| Keep query fixtures only in ad hoc local files | Low setup overhead | Unrepeatable regressions and weak CI signal | Never for merged parser behavior changes |
| Modify `tmp/` directly during feature work | Quick experimentation | Lost fixes and non-reproducible builds | Only for throwaway debugging; merged fixes must land in tracked tree |
| Emit IR without schema/version | Faster first output | Frequent downstream breakage | Only for pre-consumer prototypes, not once integrated |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Bazel module/deps | Fix by adding global deps in root build graph | Add narrowly at parser layer; verify transitive impact and target boundaries |
| Internal tooling consuming parser IR | Consume raw struct layout directly | Consume versioned schema contract with compatibility checks |
| Upstream ClickHouse sync workflow | Cherry-pick code without manifest/provenance update | Update manifest with upstream source refs and local deviation rationale |
| Smoke binaries under `examples/bootstrap` | Treat as sufficient validation | Keep smoke checks plus corpus-based behavior/golden tests |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Re-parsing identical query text repeatedly | High CPU in parser-only tools under repetitive workloads | Add normalized query cache keyed by text and parser mode | Noticeable at batch workloads with repeated templates (>1k repeated parses per run) |
| Oversized include/link surface in parser targets | Slow incremental builds and link bottlenecks | Tighten layering, trim deps, split heavy utility boundaries | Developer iteration pain appears once target graph grows past a few hundred transitive libs |
| Unbounded AST traversal during IR emission | Latency spikes on nested queries | Use iterative/guarded traversal and depth checks | Appears on deeply nested generated SQL and large SELECT lists |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Logging full raw SQL from internal tools by default | Sensitive data leakage in logs | Redact literals/identifiers where possible; gate full-query logs behind debug flags |
| Accepting untrusted SQL input without parser resource limits | Denial-of-service via pathological queries | Enforce max query length, parse timeout, and nesting/recursion guards |
| Passing parser errors directly upstream without sanitization | Information disclosure via internal paths/state | Normalize error surface for consumers; keep detailed diagnostics internal |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Parser returns generic "failed" errors | Tooling teams cannot self-debug query issues | Return structured error kind, location, and actionable message |
| IR shape changes without migration notes | Consumer breakages feel random | Publish concise change notes with schema versions per release |
| Coverage targets not visible | Teams assume parser supports more SQL than it does | Document supported/unsupported constructs with examples tied to tests |

## "Looks Done But Isn't" Checklist

- [ ] **Build stabilization:** Build is green on clean checkout and CI, not only on developer machine with warm cache
- [ ] **Parser coverage:** Internal `SELECT` corpus has passing parse + AST/IR invariant checks
- [ ] **IR contract:** Versioned schema and golden outputs exist for representative queries
- [ ] **Dependency control:** Parser target dependency growth is measured and reviewed against budget
- [ ] **Provenance:** Manifest reflects upstream sync refs and intentional local deviations
- [ ] **Scope discipline:** Excluded non-SQL/MSSQL branches are not silently reintroduced

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Behavior regressions hidden by green builds | MEDIUM | Freeze parser merges, run full workload corpus, bisect failing range, add missing regression fixtures |
| `tmp/`-only fixes lost | MEDIUM | Reconstruct diff, port into tracked files, add guard check to prevent future `tmp/` reliance |
| Dependency explosion | HIGH | Audit transitive graph, isolate parser interfaces, replace broad deps with stubs/adapters, enforce per-target budgets |
| IR contract break | MEDIUM | Introduce schema version bump or compatibility shim, regenerate goldens, publish migration notes |
| Upstream provenance gaps | MEDIUM | Backfill manifest mapping, annotate intentional divergences, gate future syncs on manifest update |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Build green but parser incorrect | Phase 1-2 | CI runs corpus-based parse and AST/IR invariants alongside smoke binaries |
| `tmp/` as canonical source | Phase 1 | Fresh-clone reproducibility and no tracked/required changes under `tmp/` |
| Dependency pull-through from upstream common | Phase 4 | Parser target transitive deps and build time trend stay within agreed budget |
| Scope drift into excluded dialects/analyzers | Phase 5 | Parity matrix and PR checks show only in-scope SQL branches expanded |
| Unstable IR contract | Phase 3 | Versioned schema + golden diff checks pass; downstream compatibility tests green |
| Missing provenance on syncs | Phase 2/5 | Every parser sync PR updates manifest with upstream refs and deviation notes |

## Sources

- `.planning/PROJECT.md` (scope, constraints, active requirements)
- `docs/ported_clickhouse_manifest.md` (provenance intent referenced in project requirements)
- Existing project structure assumptions from `ported_clickhouse/*`, `examples/bootstrap/*`, and Bazel-first workflow in project context

---
*Pitfalls research for: Clickshack parser-only extraction project*
*Researched: 2026-03-07*
