# SELECT Parser Roadmap (Parser-Only)

This document is the working roadmap for taking `ported_clickhouse` from the current bridge parser to broad `SELECT` parser coverage, without building a query analyzer.

## Current Baseline

Implemented and green:
- `//ported_clickhouse:parser_core`
- `//ported_clickhouse:parser_use_stack`
- `//ported_clickhouse:parser_select_lite_stack`
- `//ported_clickhouse:parser_select_from_lite_stack`
- `//ported_clickhouse:expression_ops_lite_stack`
- `//ported_clickhouse:parser_table_sources_stack`
- `//ported_clickhouse:parser_select_clauses_stack`
- `//ported_clickhouse:parser_select_rich_stack`
- smoke binaries under `//examples/bootstrap:*_smoke`

Supported in `select_rich` bridge:
- `SELECT ... FROM ...`
- joins: `INNER/LEFT/RIGHT/FULL/CROSS`, plus parser-only `GLOBAL`, strictness (`ANY/ALL/SEMI/ANTI/ASOF`), and `OUTER` forms
- source forms: table ref, table function, subquery source (recursive)
- `FROM` tail support: `FINAL`, `SAMPLE <expr> [OFFSET <expr>]`, `[LEFT] ARRAY JOIN <expr_list>`
- optional clauses: `WHERE`, `GROUP BY`, `HAVING`, `WINDOW`, `QUALIFY`, `ORDER BY`, `LIMIT [OFFSET]`, `LIMIT <n> BY ...`, `OFFSET ... FETCH ...`
- group modifiers: `GROUP BY ... WITH ROLLUP|CUBE|TOTALS`
- group variants: `GROUP BY ALL`
- optional limit variants: `LIMIT offset,count`, `LIMIT ... WITH TIES`, `LIMIT ... BY ALL`
- limit-by variants: `LIMIT <n> BY ...`, `LIMIT offset,count BY ...`, and `LIMIT <n> OFFSET <m> BY ...`
- set/surface forms: `WITH [RECURSIVE]`, `SELECT ALL|DISTINCT`, `UNION|INTERSECT|EXCEPT [ALL|DISTINCT]`, trailing `SETTINGS` and `FORMAT`
- projection aliases: `expr AS alias` and `expr alias` in SELECT list
- `WITH` supports parser-only CTE-style items: `name AS (SELECT ...)` and `(SELECT ...) AS name`
- expression ops-lite: unary/arithmetic/comparison/`AND`/`OR`/parentheses, `IN` (list or subquery RHS), `BETWEEN`, `LIKE/ILIKE [ESCAPE ...]`, `IS [NOT] NULL`, `IS [NOT] DISTINCT FROM`, `<=>`, `^`, `|`, `||`, lambda `->`, casts (`CAST`, `::`, complex type tokens in `CAST AS`), `CASE`, `EXISTS`, subquery atoms (`(SELECT ...)`), `*`/`table.*`, array literals, tuple literals
- `ORDER BY` parser-only modifiers: `NULLS FIRST/LAST`, `COLLATE <name>`
- `ORDER BY` parser-only ClickHouse fill modifiers: `WITH FILL [FROM/TO/STEP/STALENESS] [INTERPOLATE ...]`

## Operating Rules For Future Work

1. Keep all imported/upstream-derived code under `ported_clickhouse/*`.
2. Continue include rewrite style: `ported_clickhouse/...` only.
3. Add narrow layered targets rather than one giant parser target.
4. Prefer parser-only AST growth; do not add semantic/analyzer dependencies.
5. Update `docs/ported_clickhouse_manifest.md` for every new/changed imported or bridge file.
6. Every phase must keep `//examples/bootstrap:hello_clickshack` green.

## Next Steps (Recommended Order)

### Phase A: SELECT Surface Expansion

Goal: close major parser-only clause gaps while staying bridge-local.

Add:
- `DISTINCT`
- `HAVING`
- `WITH` clause (non-recursive first)
- `UNION ALL` and `UNION DISTINCT`

Implementation notes:
- Introduce `ASTSelectSetLite` (for UNION chains) and minimal clause fields in `ASTSelectRichQuery`.
- Add parser modules that compose existing `ParserSelectRichQuery` recursively for set operations.
- Keep validation syntactic; no aggregate correctness checking.

Acceptance:
- build `//ported_clickhouse:parser_lib`
- add smoke cases for each new clause and mixed combinations
- add negative cases (e.g. malformed UNION chains)

### Phase B: Expression Grammar Depth

Goal: reduce practical query parse failures caused by expression coverage.

Add:
- `IN`, `NOT IN`, `BETWEEN`, `LIKE`, `ILIKE`
- `IS NULL`, `IS NOT NULL`
- `CASE WHEN ... THEN ... ELSE ... END`
- cast forms: `CAST(x AS T)` and `x::T` (parser-only)
- tuple/array literal parsing in expression context

Implementation notes:
- grow `ExpressionOpsLiteParsers` with explicit precedence and keyword boundaries.
- add minimal AST bridge nodes where existing `ASTFunction` is not enough.

Acceptance:
- expression-heavy smoke matrix (success + expected failures)
- no regressions in prior smoke binaries

### Phase C: Join Syntax Completeness

Goal: broaden FROM/JOIN parser compatibility.

Add:
- `USING (...)`
- optional join strictness/modifiers handled syntactically only (where cheap)
- better alias handling across nested join trees

Implementation notes:
- extend `ASTJoinLite` with optional `using_columns` node.
- keep unsupported advanced join modes explicit and rejected deterministically.

Acceptance:
- join-focused smoke matrix covering each join form and invalid ON/USING combinations

### Phase D: SELECT Clause Variants

Goal: cover remaining parser-only clause variants commonly encountered.

Add:
- `LIMIT BY`
- `FETCH`/`OFFSET ... FETCH` (if needed)
- `TOP` (if needed)
- `WINDOW` and `QUALIFY` (minimal syntax acceptance)

Implementation notes:
- add dedicated clause parser module(s) rather than bloating one file.
- preserve clause-order checks and deterministic failure behavior.

Acceptance:
- clause matrix smoke cases, including ordering and mutual-exclusion invalid cases

## Validation Contract Per Phase

Required commands:
- `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib`
- `bazel --batch --output_user_root=/tmp/bazel-root build //examples/bootstrap:hello_clickshack`
- `bazel --batch --output_user_root=/tmp/bazel-root build //examples/bootstrap:select_rich_smoke`
- run all new success/failure smoke queries for the phase
- run `just run-ported-smokes`

Success criteria:
- all required builds pass
- all expected-success queries parse and fully consume tokens
- all expected-failure queries return non-zero with deterministic failure mode

## Manifest Discipline Checklist

For every phase update:
1. Add rows for all new files.
2. Update rows for modified bridge files (especially `CommonParsers.*`, major parser modules).
3. Include clear "Pruned Content Summary" for any imported/upstream-derived additions.
4. Keep `Dependent Targets` accurate for layered target changes.
5. Set `Last Synced` date for touched rows.

## Non-Goals (Keep Explicit)

- semantic analysis
- name/type resolution
- planner/optimizer integration
- runtime execution correctness

This roadmap is intentionally parser-only. If future work starts requiring semantic behavior, split it into a separate design track and do not couple it into `ported_clickhouse` parser bridge layers.
