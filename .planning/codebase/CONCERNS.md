# Codebase Concerns (Technical Debt, Known Issues, Risks)

## Scope
This document highlights debt and delivery risk in `clickshack`, with priority on parser extraction/sync and `Parsers` source-of-truth drift.

## Highest-Risk Concerns

### 1) Source-of-truth drift between upstream ClickHouse Parsers and local bridge code
- Risk: `ported_clickhouse/parsers/*` is partially imported and heavily pruned, so behavior can diverge from upstream `src/Parsers/*` without clear detection.
- Evidence: `docs/ported_clickhouse_manifest.md` explicitly marks many rows as high prune risk and local bridge-only design.
- Hotspots: `ported_clickhouse/parsers/CommonParsers.cpp`, `ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp`, `ported_clickhouse/parsers/TableSourcesLiteParsers.cpp`, `ported_clickhouse/parsers/SelectClausesLiteParsers.cpp`, `ported_clickhouse/parsers/ParserSelectRichQuery.cpp`.
- Impact: silent parse incompatibilities, regressions on upgrade/re-sync, and increased debugging cost when query syntax differs from upstream expectations.
- Current control: manual manifest process in `docs/ported_clickhouse_manifest.md` and checklist in `docs/select_parser_roadmap.md`.
- Gap: no automated sync gate to detect divergence from upstream parser files.

### 2) Upstream staging in `tmp/` is currently easy to accidentally commit
- Risk: upstream ClickHouse workspace is under `tmp/` (for example `tmp/clickhouse-patching/*`), and `.gitignore` does not ignore `tmp/`.
- Evidence: `.gitignore` has no `tmp/` rule; `git status --short` currently reports `?? tmp/`.
- Impact: accidental VCS inclusion of large upstream trees, legal/compliance noise, review overload, and repo bloat.
- Required user context alignment: upstream sources in `tmp/` should remain outside VCS.
- Gap: no repository guardrail preventing accidental add/commit of `tmp/clickhouse-patching` content.

### 3) Parser correctness coverage is smoke-heavy and shallow for a fast-growing grammar
- Risk: validation is centered on smoke binaries in `examples/bootstrap/*_smoke.cc` and `just run-ported-smokes`, which is useful but not exhaustive.
- Evidence: `Justfile` and `docs/select_parser_roadmap.md` define smoke-based validation as default contract.
- Impact: subtle precedence/keyword edge cases may regress undetected, especially in rich grammar paths.
- Hotspots: `ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp` and `ported_clickhouse/parsers/TableSourcesLiteParsers.cpp` where grammar complexity is concentrated.
- Gap: no broader differential/fuzz/property test layer tied to parser re-sync.

## Structural Technical Debt

### 4) Manifest discipline is manual and therefore fragile under parallel development
- Debt: `docs/ported_clickhouse_manifest.md` is detailed but maintained manually per change.
- Evidence: roadmap checklist in `docs/select_parser_roadmap.md` requires humans to update rows and sync dates.
- Risk: stale `Last Synced` values or missing rows lead to false confidence during upgrade planning.

### 5) Intentional pruning increases long-term maintenance burden
- Debt: many parser/core files are aggressively minimized (`Prune Risk: High`) to stay parser-only.
- Evidence: high-risk rows across `ported_clickhouse/common/*`, `ported_clickhouse/parsers/IAST*`, `ported_clickhouse/parsers/CommonParsers*`, and expression/table clause parser units.
- Risk: future feature work can unknowingly depend on removed semantics or utility behavior and produce non-obvious breakage.

### 6) Runtime safety hooks are intentionally weakened in ported subset
- Debt: safety-related behavior is reduced (for example `ported_clickhouse/common/checkStackSize.h` is a no-op shim).
- Risk: pathological parser inputs may have weaker guard behavior versus upstream.
- Note: this may be acceptable for scope, but it is still a risk boundary that should be explicit in parser acceptance policy.

## Known Delivery Risks

### 7) Layered parser targets are clean, but change blast radius is still high in shared keyword/expression code
- Risk: shared files feed multiple targets, so small keyword/precedence edits can ripple across `parser_use_stack`, `parser_select_lite_stack`, `parser_select_from_lite_stack`, `expression_ops_lite_stack`, `parser_table_sources_stack`, `parser_select_clauses_stack`, and `parser_select_rich_stack`.
- Evidence: dependency spread documented in `docs/ported_clickhouse_manifest.md` entries for `CommonParsers.*` and `ExpressionOpsLiteParsers.*`.
- Impact: regressions can appear in distant parser surfaces after localized edits.

### 8) Parser-only scope is clear, but boundary pressure is growing
- Risk: as grammar support expands, pressure to add semantic behavior grows, which can contaminate the parser-only architecture.
- Evidence: explicit non-goals in `docs/select_parser_roadmap.md` and parser-only AST design in `ported_clickhouse/parsers/AST*Lite*`.
- Impact: architecture drift, more dependencies, and harder re-sync with upstream parser sources.

## Practical Risk Controls To Prioritize Next
- Add `tmp/` guardrails immediately: ignore rules and/or commit-time checks so `tmp/clickhouse-patching` cannot enter VCS.
- Add an automated parser sync check that compares key `ported_clickhouse/parsers/*` files against mapped upstream `src/Parsers/*` references from `docs/ported_clickhouse_manifest.md`.
- Expand parser verification beyond smokes with targeted negative suites for precedence/join/clause edge cases, anchored to `ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp`, `ported_clickhouse/parsers/TableSourcesLiteParsers.cpp`, and `ported_clickhouse/parsers/SelectClausesLiteParsers.cpp`.
- Treat `docs/ported_clickhouse_manifest.md` as a release artifact: fail CI when touched parser files lack manifest row/date updates.
