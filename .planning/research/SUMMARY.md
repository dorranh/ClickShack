# Project Research Summary

**Project:** Clickshack Parser Bridge
**Domain:** Standalone C++ ClickHouse SQL parser extraction with stable AST->IR contract
**Researched:** 2026-03-07
**Confidence:** HIGH

## Executive Summary

Research converges on a parser-only architecture: C++20 + Bazel/Bzlmod, layered parser targets, and a strict public boundary at a versioned IR contract. The project should optimize for deterministic parsing of internal ClickHouse `SELECT` workloads, not full engine parity.

The recommended approach is to stabilize the Bazel/build baseline first, then harden parser behavior with fixture/corpus validation, then introduce a versioned IR schema with golden tests before expanding coverage. This sequence aligns with dependency and risk order found across stack, features, architecture, and pitfalls research.

The main risk is false confidence from green builds while parser behavior or IR compatibility regresses. Mitigation is to gate progress with workload corpus checks, AST/IR invariants, deterministic snapshots, dependency budget tracking, and provenance updates for each upstream sync.

## Key Findings

### Recommended Stack

Use C++20 with Bazel 8 + Bzlmod and keep the parser as layered targets (`parser_core` -> grammar slices -> aggregate `parser_lib`). Keep parser behavior in `ported_clickhouse/parsers`, expose only stable IR for consumers, and preserve strict source boundaries (`ported_clickhouse/*` canonical, `tmp/*` reference-only).

**Core technologies:**
- C++20: parser and AST/IR implementation surface — performance, safety, and parity with existing code patterns
- Bazel 8 + Bzlmod: build/test/dependency control — deterministic parser-only graph and reproducible CI
- Layered parser targets: scope isolation and incremental grammar growth — better bisectability and lower integration risk

### Expected Features

The MVP is not broad product expansion; it is reliable parser extraction with stable output contracts and controlled scope. Priority is deterministic parsing and contract reliability for internal tooling.

**Must have (table stakes):**
- Deterministic ClickHouse SQL parsing for targeted internal `SELECT` workloads — core project value
- Structured AST output and stable, versioned AST->IR contract — required for downstream consumption
- Actionable syntax diagnostics with locations — needed for debugging and adoption
- Bazel-first smoke + regression validation and grammar support/exclusion matrix — prevents hidden regressions and scope confusion
- Dependency-pruned standalone `parser_lib` packaging — keeps integration practical

**Should have (competitive):**
- Provenance-tracked upstream sync workflow — sustainable parity maintenance
- Layered parser linking options and fast smoke binaries — faster consumer iteration
- Normalized internal-tooling IR semantics — predictable integration surface

**Defer (v2+):**
- Semantic analyzer/type resolution in parser core
- Query planning/execution concepts in IR
- Cross-dialect transpilation
- Full non-SQL/MSSQL branch parity

### Architecture Approach

Use a layered parse pipeline (`SQL -> Lexer/TokenIterator -> parser stacks -> AST`) and a separate contract pipeline (`AST -> validator/normalizer -> versioned IR -> serializer`). Keep AST internal and IR external. Enforce canonical source and provenance controls to avoid drift.

**Major components:**
1. Parser core + grammar layers: deterministic syntactic parsing and AST construction
2. IR boundary module (`ir/schema/v1`, `ast_to_ir`, serialization): stable consumer-facing contract
3. Validation/provenance system: smoke tests, corpus+goldens, dependency budgets, and manifest-tracked upstream sync

### Critical Pitfalls

1. **Build green but parser wrong** — require corpus-based parse + AST/IR invariants in CI, not compile-only gates
2. **`tmp/` treated as canonical source** — enforce `ported_clickhouse/*` authority and disallow merged `tmp` dependency
3. **Dependency creep from upstream common layers** — track per-target dependency budget and use adapters/stubs
4. **Scope drift into excluded dialect/analyzer work** — maintain and enforce explicit parity/exclusion matrix
5. **Unversioned IR contract changes** — enforce schema versioning, golden outputs, and compatibility policy

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Build and Source Baseline Hardening
**Rationale:** All behavior work is unreliable until builds are reproducible and source boundaries are enforceable.
**Delivers:** Clean Bazel parser builds/tests on fresh checkout; no canonical dependency on `tmp`.
**Addresses:** Build/test stabilization and reproducibility table-stakes.
**Avoids:** Hidden regressions masked by local cache state and lost `tmp`-only fixes.

### Phase 2: Parser Coverage and Provenance Discipline
**Rationale:** Once stable build baseline exists, close parser gaps for target workloads with traceable upstream lineage.
**Delivers:** Expanded internal `SELECT` corpus pass rate, layered grammar growth, updated manifest on each sync.
**Uses:** Layered parser targets and smoke-first validation.
**Implements:** Parser domain architecture and provenance-driven porting pattern.

### Phase 3: Stable IR v1 Contract
**Rationale:** Consumers need compatibility guarantees before broad adoption.
**Delivers:** Versioned schema, deterministic AST->IR normalization, golden snapshots, compatibility policy.
**Uses:** Dedicated `ir/schema/v1` boundary and AST->IR visitor pipeline.
**Implements:** Stable output boundary pattern (AST internal, IR external).

### Phase 4: Dependency Budget and Packaging Optimization
**Rationale:** Prevent long-term build fragility and keep parser library embeddable.
**Delivers:** Pruned dependency graph, measured transitive budgets, slimmer `parser_lib` package.

### Phase 5: In-Scope Parity Expansion with Guardrails
**Rationale:** Expand value only where workload evidence exists while holding scope discipline.
**Delivers:** Broader SQL parity for in-scope constructs and maintained exclusion matrix.

### Phase Ordering Rationale

- Build/source determinism first, because it is the prerequisite for trustworthy parser and IR validation.
- Parser behavior before IR contract, because IR stability depends on AST stability.
- Dependency optimization after functional reliability, because early broad pruning can obscure parser correctness work.
- Parity expansion last, because uncontrolled breadth is a primary identified pitfall.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 2:** Upstream sync strategy details per grammar area and fixture corpus composition.
- **Phase 4:** Dependency replacement/stubbing strategy for specific heavy transitive edges.
- **Phase 5:** Priority model for remaining SQL constructs based on observed internal workload gaps.

Phases with standard patterns (skip research-phase):
- **Phase 1:** Bazel stabilization and source-boundary enforcement are well-understood and already scoped.
- **Phase 3:** Versioned schema + golden contract testing follows established internal API practices.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Derived from current repo constraints and explicit build system requirements |
| Features | HIGH | Clear table-stakes and exclusions documented in project context |
| Architecture | HIGH | Consistent with existing layered parser direction and boundary goals |
| Pitfalls | HIGH | Risks are concrete, recurring in parser extraction efforts, and directly mitigable |

**Overall confidence:** HIGH

### Gaps to Address

- Parser workload corpus completeness: grow fixtures with real failing internal queries during execution.
- IR schema evolution policy depth: define precise backward-compatibility rules and version bump triggers.
- Dependency budget thresholds: set quantitative CI thresholds (e.g., transitive edge/count and build-time limits).

## Sources

### Primary (HIGH confidence)
- `.planning/research/STACK.md` - stack and dependency recommendations
- `.planning/research/FEATURES.md` - priority feature set, scope boundaries, MVP
- `.planning/research/ARCHITECTURE.md` - architecture patterns, component model, data flow
- `.planning/research/PITFALLS.md` - critical risks, warning signs, and mitigations
- `.planning/PROJECT.md` and `.planning/codebase/ARCHITECTURE.md` (as cited by the above research docs)

### Secondary (MEDIUM confidence)
- `MODULE.bazel`, `.bazelrc`, `Justfile`, and repository structure references cited in research docs

### Tertiary (LOW confidence)
- None identified in synthesized inputs

---
*Research completed: 2026-03-07*
*Ready for roadmap: yes*
