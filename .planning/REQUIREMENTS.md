# Requirements: Clickshack Parser Bridge

**Defined:** 2026-03-07
**Core Value:** Reliable parser-only extraction of ClickHouse SQL into a lightweight standalone Bazel C++ library that internal tooling can trust.

## v1 Requirements

### Build and Baseline

- [x] **BUILD-01**: Developer can build `//ported_clickhouse:parser_lib` successfully on current system Bazel version from a clean checkout.
- [x] **BUILD-02**: Developer can run parser smoke targets (`use`, `select_lite`, `select_from_lite`, `select_rich`) successfully via Bazel.
- [x] **BUILD-03**: Build/test flow is reproducible without requiring any tracked source from `tmp/`.

### Parser Behavior

- [ ] **PARS-01**: Library can parse prioritized internal ClickHouse `SELECT` workload queries deterministically (same input -> same parse result).
- [ ] **PARS-02**: Parser coverage includes in-scope SQL constructs required by current internal workload with explicit unsupported grammar list.
- [ ] **PARS-03**: Parser scope excludes non-SQL and MSSQL branches unless explicitly re-scoped in a future milestone.

### AST and IR Contract

- [ ] **IR-01**: Parser exposes structured AST output sufficient for downstream IR generation without analyzer semantics.
- [ ] **IR-02**: Project defines and emits a versioned parser IR v1 contract from query AST.
- [ ] **IR-03**: IR output is deterministic for semantically equivalent parser inputs under the supported subset.

### Diagnostics and Validation

- [ ] **DIAG-01**: Syntax failures include actionable diagnostics with source location (line/column or equivalent span metadata).
- [ ] **DIAG-02**: Regression suite includes representative workload fixtures and smoke checks that fail on parser behavior regressions.
- [ ] **DIAG-03**: Golden-style checks validate AST/IR stability for selected critical queries.

### Dependency and Provenance

- [ ] **DEPS-01**: Parser library dependency graph is pruned to avoid unnecessary heavy upstream pull-through from unrelated components.
- [ ] **DEPS-02**: Upstream-origin parser files maintain provenance updates in `docs/ported_clickhouse_manifest.md` when synchronized.
- [ ] **DEPS-03**: Source-of-truth boundary is enforced: curated parser bridge code in `ported_clickhouse/*`, upstream scratch/reference work in `tmp/*` only.

## v2 Requirements

### Transpilation and Expansion

- **TRAN-01**: Python transpiler converts parser IR to SQLGlot AST for downstream tooling integration.
- **PARS-04**: Broader near-full SQL parser parity expansion beyond immediate internal workload priorities.
- **IR-04**: IR compatibility policy formalization with explicit backward-compatibility guarantees and migration guidance.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Semantic analyzer, type resolution, or query planning/execution | Explicit parser-only scope for current project objective |
| Full parity for non-SQL or MSSQL parser branches | Not required for current workload and increases maintenance cost |
| Public SDK hardening and broad external-consumer ergonomics in v1 | Primary v1 user is internal tooling; externalization can follow stabilization |
| SQL dialect transpilation in current milestone | Deferred intentionally to later phase to protect parser stabilization focus |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| BUILD-01 | Phase 1 | Complete |
| BUILD-02 | Phase 1 | Complete |
| BUILD-03 | Phase 1 | Complete |
| PARS-01 | Phase 2 | Pending |
| PARS-02 | Phase 2 | Pending |
| PARS-03 | Phase 2 | Pending |
| IR-01 | Phase 3 | Pending |
| IR-02 | Phase 3 | Pending |
| IR-03 | Phase 3 | Pending |
| DIAG-01 | Phase 4 | Pending |
| DIAG-02 | Phase 4 | Pending |
| DIAG-03 | Phase 4 | Pending |
| DEPS-01 | Phase 5 | Pending |
| DEPS-02 | Phase 5 | Pending |
| DEPS-03 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 15 total
- Mapped to phases: 15
- Unmapped: 0 ✅

---
*Requirements defined: 2026-03-07*
*Last updated: 2026-03-07 after Phase 1 completion*
