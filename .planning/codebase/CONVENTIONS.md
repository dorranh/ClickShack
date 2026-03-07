# Coding Conventions

## Scope and Intent
- This repository is a parser-focused bridge, not a full ClickHouse engine fork.
- Keep parser work inside `ported_clickhouse/parsers/*` and parser-adjacent shims in `ported_clickhouse/{base,common,core}/*`.
- Treat `tmp/clickhouse-patching/*` as upstream workspace material used for sourcing and pruning, not product code.

## Parser Boundary Rules
- Parser-only behavior is a hard boundary: no semantic analyzer/planner/runtime dependencies.
- Evolve ASTs as syntax carriers only (examples: `ported_clickhouse/parsers/ASTSelectRichQuery.h`, `ported_clickhouse/parsers/ASTJoinLite.h`).
- Keep recursive parser composition explicit through parser entry points (example: `parseNestedSelectRichQuery` in `ported_clickhouse/parsers/ParserSelectRichQuery.cpp`).
- Clause/grammar growth should extend layered parser modules, not a monolith:
  - `ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp`
  - `ported_clickhouse/parsers/TableSourcesLiteParsers.cpp`
  - `ported_clickhouse/parsers/SelectClausesLiteParsers.cpp`

## Source Layout and Layering
- Preserve thin Bazel layering in `ported_clickhouse/parsers/BUILD.bazel`:
  - `parser_core` -> `parser_use_stack` -> `parser_select_lite_stack` -> `parser_select_from_lite_stack`
  - `expression_ops_lite_stack` + `parser_table_sources_stack` + `parser_select_clauses_stack` -> `parser_select_rich_stack`
- Keep reusable parser aliases exported via `ported_clickhouse/BUILD.bazel`.
- Use examples as smoke utilities only under `examples/bootstrap/*_smoke.cc`.

## Include and Namespace Style
- Prefer project-root includes with the `ported_clickhouse/...` prefix.
- Keep C++ in `namespace DB` for ported parser code.
- Prefer minimal includes and explicit local headers before broad dependencies.

## AST and Parser Coding Patterns
- Use `make_intrusive<...>()` and `IAST::set(...)` for AST wiring consistency.
- Keep parser helper functions local in anonymous namespaces when file-scoped.
- Use keyword guards through parser/token helpers (`ParserKeyword`, `ParserToken`, `isKeyword(...)`) rather than ad-hoc string parsing.
- Ensure parser success requires full token consumption by callers (`if (!pos->isEnd()) ...`).

## Upstream-Derived Code Discipline
- Upstream-derived or bridge-imported files must stay in `ported_clickhouse/*` and be tracked in `docs/ported_clickhouse_manifest.md`.
- For every imported/pruned change, update manifest rows with rationale and dependent targets.
- Keep pruning aggressive and dependency surface narrow, as reflected in manifest summaries.

## Temporary Upstream Workspace Policy
- Upstream ClickHouse sources in `tmp/*` are local working inputs and should not be checked into VCS.
- Before commit, verify staged files exclude `tmp/` (`git status --short`, `git diff --cached --name-only`).
- Keep operational scripts under `tmp/clickhouse-patching/tools/*` for local maintenance only.

## Build and Workflow Norms
- Use Bazel as primary build system; `Justfile` wraps standard invocations.
- Prefer deterministic local root from `Justfile`: `bazel --batch --output_user_root=/tmp/bazel-root`.
- Maintain `//examples/bootstrap:hello_clickshack` health across parser phases.
