# Roadmap: Clickshack Parser Bridge

## Overview

This roadmap delivers a reliable parser-only ClickHouse bridge in dependency order: first make builds reproducible, then lock parser behavior for the internal workload, then publish a deterministic IR contract, then enforce diagnostics/regression guardrails, and finally harden dependency and provenance boundaries for sustainable maintenance.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [x] **Phase 1: Build Baseline Recovery** - Re-establish reproducible Bazel build/test behavior from clean checkout. (completed 2026-03-07)
- [ ] **Phase 2: Parser Workload Coverage** - Make in-scope internal `SELECT` parsing deterministic with explicit scope guardrails.
- [ ] **Phase 3: AST to IR v1 Contract** - Deliver a deterministic, versioned parser IR output boundary.
- [ ] **Phase 4: Diagnostics and Regression Guardrails** - Ensure parse failures and output drift are caught quickly and reproducibly.
- [ ] **Phase 5: Dependency and Provenance Hardening** - Reduce dependency pull-through and enforce canonical source/provenance discipline.

## Phase Details

### Phase 1: Build Baseline Recovery
**Goal**: Developers can reliably build and run parser smoke targets on the current Bazel version from a clean checkout.
**Depends on**: Nothing (first phase)
**Requirements**: BUILD-01, BUILD-02, BUILD-03
**Success Criteria** (what must be TRUE):
  1. Developer can build `//ported_clickhouse:parser_lib` on current system Bazel from a clean checkout.
  2. Developer can run smoke targets (`use`, `select_lite`, `select_from_lite`, `select_rich`) successfully with Bazel.
  3. Build/test flow works without requiring any tracked source from `tmp/`.
**Plans**: 2 (01-01, 01-02)

### Phase 2: Parser Workload Coverage
**Goal**: Internal tooling can parse prioritized in-scope ClickHouse `SELECT` workload queries consistently.
**Depends on**: Phase 1
**Requirements**: PARS-01, PARS-02, PARS-03
**Success Criteria** (what must be TRUE):
  1. Running the same supported workload query repeatedly yields the same parse output.
  2. Required in-scope SQL constructs for the current internal workload parse successfully.
  3. Unsupported non-SQL and MSSQL branches are explicitly documented and remain excluded from parser scope.
**Plans**: 3 (02-01, 02-02, 02-03)

### Phase 3: AST to IR v1 Contract
**Goal**: Consumers receive a deterministic, versioned parser IR v1 contract generated from parser AST without analyzer semantics.
**Depends on**: Phase 2
**Requirements**: IR-01, IR-02, IR-03
**Success Criteria** (what must be TRUE):
  1. Parser output includes structured AST data sufficient for downstream IR generation.
  2. A versioned parser IR v1 artifact is emitted from AST for supported queries.
  3. Semantically equivalent supported inputs produce deterministic IR output.
**Plans**: TBD

### Phase 4: Diagnostics and Regression Guardrails
**Goal**: Developers can quickly diagnose parser failures and detect parser/AST/IR regressions before they ship.
**Depends on**: Phase 3
**Requirements**: DIAG-01, DIAG-02, DIAG-03
**Success Criteria** (what must be TRUE):
  1. Syntax failures return actionable diagnostics with source location metadata.
  2. Representative workload regression fixtures fail when parser behavior regresses.
  3. Golden checks detect unexpected AST/IR changes for selected critical queries.
**Plans**: TBD

### Phase 5: Dependency and Provenance Hardening
**Goal**: Parser library remains lightweight and maintainable with explicit upstream provenance and source-of-truth boundaries.
**Depends on**: Phase 4
**Requirements**: DEPS-01, DEPS-02, DEPS-03
**Success Criteria** (what must be TRUE):
  1. `parser_lib` avoids unnecessary heavy upstream dependency pull-through while preserving required parser functionality.
  2. Upstream parser syncs are reflected in `docs/ported_clickhouse_manifest.md` with updated provenance.
  3. Canonical source boundary is enforceable: curated bridge code lives in `ported_clickhouse/*` and `tmp/*` remains scratch/reference only.
**Plans**: TBD

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Build Baseline Recovery | 2/2 | Complete | 2026-03-07 |
| 2. Parser Workload Coverage | 1/3 | In Progress | - |
| 3. AST to IR v1 Contract | 0/TBD | Not started | - |
| 4. Diagnostics and Regression Guardrails | 0/TBD | Not started | - |
| 5. Dependency and Provenance Hardening | 0/TBD | Not started | - |
