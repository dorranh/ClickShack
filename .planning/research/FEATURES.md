# Feature Research

**Domain:** Parser-only SQL extraction and AST/IR pipeline (ClickHouse dialect, standalone C++)
**Researched:** 2026-03-07
**Confidence:** HIGH

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Deterministic SQL parsing for target ClickHouse `SELECT` workloads | Core promise is reliable parser-only extraction for internal tooling | HIGH | Must restore post-upgrade Bazel stability first; requires hardened lexer/parser integration and repeatable parse outputs for same input. |
| Structured AST output from parser entrypoints | Downstream tools cannot consume raw tokens/errors only | MEDIUM | AST must preserve query structure and aliases while staying parser-only (no semantic analysis). |
| Stable AST-to-IR conversion contract | Active requirement calls for stable parser IR as application output contract | HIGH | Define versioned IR schema, stable field ordering, and compatibility guarantees to prevent downstream breakage. |
| Clear syntax diagnostics with source locations | Users expect actionable failures for malformed SQL | MEDIUM | Include line/column spans and parser context; avoid analyzer-like semantic diagnostics to keep scope clean. |
| Bazel-first build/test smoke coverage (`USE`, lite `SELECT`, `FROM`, rich `SELECT`) | Existing workflow depends on Bazel + smoke binaries as trust gate | MEDIUM | Required to prevent regressions during parser sync/refactors; ties directly to current blocker (Bazel upgrade regressions). |
| SQL parser branch coverage with explicit exclusions (non-SQL/MSSQL out) | Requirement is near-full SQL parity where valuable, with exclusions documented | HIGH | Maintain explicit compatibility matrix so stakeholders know what is intentionally unsupported. |
| Minimal dependency footprint for parser library | Project constraint explicitly requires reducing heavy upstream pull-through | HIGH | Needs dependency pruning/stubbing in `common` adjacencies without changing parser behavior. |

### Differentiators (Competitive Advantage)

Features that set the product apart. Not required, but valuable.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Layered parser targets (`parser_core` -> focused dialect slices -> aggregate lib) | Faster builds and selective embedding for internal tools | MEDIUM | Leverages existing staged target architecture; enables consumers to link only what they need. |
| Provenance-tracked upstream sync workflow for parser sources | Safer long-term maintenance versus one-off fork drift | MEDIUM | Use manifest + patch discipline to track origin and intent of ported files. |
| Normalized IR tuned for internal tooling (parser-only semantics) | Provides a lightweight, predictable interface unlike full DB planner ASTs | HIGH | Requires strict boundary: syntactic structure only, no type inference or execution planning fields. |
| Smoke-binary-first validation for representative workload classes | Fast confidence loop for parser behavior changes | LOW | Extend existing smoke programs as guardrails before heavier test suites. |
| Dependency-slim standalone packaging (`//ported_clickhouse:parser_lib`) | Easier embedding into internal systems with lower compile/link overhead | MEDIUM | Differentiates from full-server embeddings and simplifies CI adoption. |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Semantic analyzer/type resolution in parser bridge | Consumers want "fully validated" queries in one step | Breaks parser-only scope, balloons dependencies, and slows stabilization | Keep parser syntactic; expose IR plus optional downstream analyzer plug-in point outside core library. |
| Query planning/execution hooks in IR | Desire to "future-proof" by embedding runtime concepts early | Couples IR to engine internals and destabilizes contract | Keep IR syntax-focused and versioned; map to execution models in separate adapter layer later. |
| Full parity including non-SQL/MSSQL branches | "Parity" sounds safer for future unknown use cases | Adds major maintenance burden with little current workload value | Maintain explicit excluded-grammar list and add branches only when concrete workloads justify. |
| Multi-dialect transpilation (e.g., sqlglot-like translation) in this milestone | Stakeholders want broader interoperability | Deferred by project requirements; distracts from build/parser reliability | Preserve dialect-agnostic extension seam but defer transpilation to later milestone. |
| Aggressive auto-recovery that rewrites invalid SQL | User-friendly on the surface | Can hide real syntax defects and produce misleading AST/IR | Return precise parse failures; optionally provide non-mutating hints separately. |

## Feature Dependencies

```text
[Bazel build/test stability]
    └──requires──> [Dependency pruning/stubbing for heavy upstream common]

[Deterministic SQL parsing]
    └──requires──> [Bazel build/test stability]
        └──enables──> [Smoke-binary regression gates]

[Structured AST output]
    └──requires──> [Deterministic SQL parsing]
        └──requires──> [Grammar coverage matrix (SQL-only)]

[Stable AST-to-IR contract]
    └──requires──> [Structured AST output]
        └──requires──> [IR schema versioning policy]

[Near-full SQL parity (with exclusions)]
    └──requires──> [Upstream provenance + sync workflow]

[Analyzer/execution features] ──conflicts──> [Parser-only dependency budget + scope]
[Non-SQL/MSSQL parity expansion] ──conflicts──> [Milestone focus on internal SELECT workloads]
```

### Dependency Notes

- **Deterministic SQL parsing requires Bazel build/test stability:** parser fixes are not trustworthy until builds/tests are reproducible after the Bazel upgrade.
- **Structured AST output requires deterministic parsing:** AST shape must not fluctuate across equivalent runs/inputs.
- **Stable AST-to-IR contract requires AST stability plus schema versioning:** downstream integrations need predictable semantics and controlled evolution.
- **Near-full SQL parity requires provenance-tracked sync:** without clear upstream lineage, parity maintenance degrades into brittle local divergence.
- **Analyzer/execution capabilities conflict with parser-only scope:** they introduce heavy dependencies and violate explicit project boundaries.

## MVP Definition

### Launch With (v1)

Minimum viable product — what's needed to validate the concept.

- [ ] Bazel build/test stabilization for parser targets and smoke binaries — unblocks all parser iteration.
- [ ] Deterministic parser-only AST extraction for prioritized internal `SELECT` workloads — core value delivery.
- [ ] Versioned AST-to-IR output contract with documented schema and compatibility rules — enables reliable downstream consumption.
- [ ] Explicit grammar support/exclusion matrix (SQL yes, non-SQL/MSSQL no) — sets clear user expectations.
- [ ] Dependency-pruned standalone `parser_lib` packaging — keeps adoption practical for internal tooling.

### Add After Validation (v1.x)

Features to add once core is working.

- [ ] Broaden SQL grammar coverage toward near-full parser parity — add as real internal query gaps are observed.
- [ ] Golden corpus expansion with workload-driven regression tests — trigger when integration teams contribute failing examples.
- [ ] Optional IR adapters for specific internal consumers — add when at least two consumers need divergent shapes.

### Future Consideration (v2+)

Features to defer until product-market fit is established.

- [ ] External-facing SDK ergonomics and hardening — defer until internal interfaces stabilize.
- [ ] Cross-dialect transpilation pipeline — explicitly out of current milestone and requires separate success criteria.
- [ ] Optional semantic validation layer as separate module — only after parser core is stable and dependency budget is reassessed.

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Bazel build/test stabilization | HIGH | MEDIUM | P1 |
| Deterministic SQL parsing (target workloads) | HIGH | HIGH | P1 |
| Structured AST output | HIGH | MEDIUM | P1 |
| Stable AST-to-IR versioned contract | HIGH | HIGH | P1 |
| Dependency pruning/stubbing for standalone parser lib | HIGH | HIGH | P1 |
| Grammar support/exclusion matrix + compatibility docs | MEDIUM | LOW | P2 |
| Provenance-tracked upstream sync workflow hardening | MEDIUM | MEDIUM | P2 |
| Broader near-full SQL parity expansion | MEDIUM | HIGH | P2 |
| Consumer-specific IR adapters | MEDIUM | MEDIUM | P3 |
| Semantic analyzer/execution integration | LOW (for current users) | HIGH | P3 (defer) |

## Competitor Feature Analysis

| Feature | Competitor A | Competitor B | Our Approach |
|---------|--------------|--------------|--------------|
| Parser coverage strategy | Full DB engines typically bundle parser with analyzer/planner | Lightweight SQL parsers often trade dialect fidelity for simplicity | Keep high-fidelity ClickHouse parser behavior while enforcing parser-only boundaries. |
| Output contract | Many engines expose unstable internal ASTs | Some parser libs expose generic JSON trees with weak compatibility guarantees | Provide a versioned, stable IR contract designed for internal tooling integration. |
| Build/dependency profile | Server-centric projects carry broad transitive dependencies | Minimal parsers are easy to embed but may lack required dialect depth | Deliver dependency-slim standalone Bazel library without sacrificing target workload coverage. |
| Upstream maintenance | Forks often drift without provenance tooling | Smaller libraries may not track upstream grammar changes rapidly | Preserve manifest-driven provenance and explicit sync intent for sustainable parity updates. |

## Sources

- `.planning/PROJECT.md` (project scope, requirements, constraints, decisions)
- `.planning/codebase/ARCHITECTURE.md` (referenced by PROJECT.md for current staged parser layout)
- `/Users/dorran/.codex/get-shit-done/templates/research-project/FEATURES.md` (research structure/template)

---
*Feature research for: Clickshack Parser Bridge (parser-only SQL extraction + AST/IR pipeline)*
*Researched: 2026-03-07*
