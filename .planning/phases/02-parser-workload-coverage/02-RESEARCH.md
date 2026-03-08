# Phase 02 Research: Parser Workload Coverage

## Objective
Plan Phase 02 around a constrained goal: prove that the existing parser bridge can parse the prioritized internal ClickHouse `SELECT` workload deterministically, and close only the gaps required for that workload. Do not turn this phase into broad parser expansion, IR design, or diagnostics hardening.

## What Changed In Phase 01 That Matters Here
- Build and smoke entrypoints are already reproducible enough to use as the Phase 02 execution base.
- `just smoke-suite` currently builds the existing parser smoke targets successfully.
- The current environment can reuse already-resolved Bazel state, but fresh external registry fetches can fail under restricted network conditions. Phase 02 validation should therefore prefer checked-in fixtures and existing targets over workflows that assume new external resolution at test time.
- `tmp/` is already guarded as out-of-scope source-of-truth. Phase 02 should keep all parser and validation work under tracked bridge code and planning/docs files.

## Current Parser Baseline
The implemented parser surface is broader than the current smoke coverage suggests.

Current layered targets:
- `//ported_clickhouse:parser_core`
- `//ported_clickhouse:parser_use_stack`
- `//ported_clickhouse:parser_select_lite_stack`
- `//ported_clickhouse:parser_select_from_lite_stack`
- `//ported_clickhouse:expression_ops_lite_stack`
- `//ported_clickhouse:parser_table_sources_stack`
- `//ported_clickhouse:parser_select_clauses_stack`
- `//ported_clickhouse:parser_select_rich_stack`
- `//ported_clickhouse:parser_lib`

Current smoke binaries:
- `use_parser_smoke`
- `select_lite_smoke`
- `select_from_lite_smoke`
- `select_rich_smoke`

Current rich parser implementation already supports, syntactically:
- `WITH` and `WITH RECURSIVE` surface parsing
- `SELECT ALL` and `SELECT DISTINCT`
- `FROM` with table refs, table functions, and subquery sources
- joins: `INNER`, `LEFT`, `RIGHT`, `FULL`, `CROSS`, plus `GLOBAL`, `ANY`, `ALL`, `SEMI`, `ANTI`, `ASOF`, `OUTER`
- join conditions with `ON` or `USING`
- `FINAL`, `SAMPLE`, `[LEFT] ARRAY JOIN`
- `PREWHERE`, `WHERE`, `GROUP BY`, `HAVING`, `WINDOW`, `QUALIFY`, `ORDER BY`
- `LIMIT`, `OFFSET`, `FETCH`, `LIMIT BY`, `WITH TIES`
- `UNION`, `INTERSECT`, `EXCEPT`, trailing `SETTINGS`, trailing `FORMAT`
- expression features including arithmetic/comparison/logical ops, `IN`, `BETWEEN`, `LIKE`, `ILIKE`, `IS NULL`, `IS DISTINCT FROM`, `<=>`, casts, `CASE`, `EXISTS`, subquery atoms, array literals, tuple literals, lambda `->`, and alias wrapping in projections

Runtime spot checks confirmed the current binary accepts representative examples for:
- deterministic repeat output on a supported query
- `DISTINCT` + `WHERE` + `ORDER BY` + `LIMIT`
- `WITH` CTE-style surface
- `INNER JOIN ... ON ...`
- `LEFT ANY JOIN ... USING (...)`
- `GROUP BY ALL WITH ROLLUP HAVING ...`
- `ORDER BY ... WITH FILL ... INTERPOLATE ...`
- `LIMIT ... BY ALL`
- `OFFSET ... FETCH ...`
- `UNION ALL ... SETTINGS ... FORMAT ...`

## What Is Still Missing For Good Phase Planning
The code is ahead of the proof. Phase 02 is mostly about turning implicit parser breadth into explicit workload coverage.

You need these answers before planning tasks:
- What is the actual prioritized internal workload corpus for this milestone?
- Which query families are mandatory versus nice-to-have?
- Which accepted constructs need exact structural assertions versus simple parse-success assertions?
- Which current parser features are intentionally out of workload scope even if they already parse?
- Which unsupported forms must be documented explicitly as excluded under `PARS-03`?

Without a concrete workload list, planning will either underfit acceptance or sprawl into generic parser parity work.

## Recommended Phase Shape
Use Phase 02 to do four things only:
1. Define the workload corpus and classify it by construct.
2. Build a stable parse-output harness for supported workload queries.
3. Close parser gaps only where workload fixtures fail.
4. Document and negatively validate excluded non-SQL and MSSQL scope.

Do not use this phase to:
- invent the downstream IR contract
- add full diagnostics payloads
- pursue full ClickHouse statement coverage beyond `SELECT`
- widen support just because the upstream lexer/parser token set contains extra dialect artifacts

## Standard Stack
Use the existing local stack; do not add new parser frameworks.

- Bazel cc targets under `ported_clickhouse/parsers`
- existing bridge AST classes under `ported_clickhouse/parsers/AST*.h`
- existing parser modules:
  - `ParserSelectRichQuery.cpp`
  - `SelectClausesLiteParsers.cpp`
  - `TableSourcesLiteParsers.cpp`
  - `ExpressionOpsLiteParsers.cpp`
- existing smoke build flow via `tools/parser_smoke_suite.sh` and `Justfile`
- checked-in workload fixtures stored in the repo, not generated from `tmp/`
- a new narrow parser validation binary or test-only utility for canonical workload output

## Architecture Patterns
Be prescriptive here.

### 1. Fixture-First Workload Definition
Create a checked-in workload fixture set for the prioritized queries.
Each fixture should carry:
- query text
- classification tags for clauses/constructs used
- expected result kind: `success` or `excluded`
- expected canonical parse summary for `success` fixtures
- exclusion reason for `excluded` fixtures

### 2. Canonical Parse Summary, Not IR
Phase 02 needs a deterministic output contract for validation, but not the Phase 03 IR.
Use a canonical parse summary emitted by a dedicated validation binary.
That summary should be structural and stable, for example:
- top-level query kind (`select_rich` vs `select_set`)
- clause presence flags
- set-op sequence and modes
- join tree kinds and modifiers
- source kinds
- projection count
- limit / limit-by normalized values
- whether key optional fields are populated

If current `select_rich_smoke` output is too lossy for workload disambiguation, extend or fork it into a dedicated workload-summary binary rather than overloading production parser APIs.

### 3. Layered Parser Changes Only When Fixtures Demand Them
When workload fixtures fail, patch the narrowest parser layer that owns the grammar:
- expression failures -> `ExpressionOpsLiteParsers.cpp`
- table-source / join failures -> `TableSourcesLiteParsers.cpp`
- clause-order / clause-surface failures -> `SelectClausesLiteParsers.cpp`
- top-level query composition failures -> `ParserSelectRichQuery.cpp`

Do not collapse logic into one giant parser file.

### 4. Explicit Scope Guardrails
Add a checked-in scope document for Phase 02 that states:
- in-scope: prioritized internal ClickHouse `SELECT` workload subset
- out-of-scope: non-`SELECT` statements, non-SQL inputs, MSSQL-only syntax, deferred query-parameter forms

This should be paired with a small negative fixture set so exclusions are not just prose.

## Don't Hand-Roll
- Do not build a new AST or parallel parser model for workload validation.
- Do not design Phase 03 IR early just to satisfy determinism.
- Do not introduce semantic analysis, name resolution, or type validation.
- Do not create a giant generic SQL conformance suite.
- Do not use pointer addresses or incidental AST child ordering in validation output.
- Do not rely on ad hoc shell assertions over human-readable stdout once fixture count grows; use a canonical machine-comparable summary format.

## Common Pitfalls
- Mistaking existing parser breadth for validated workload coverage. Most constructs are implemented, but only a few are exercised by current smoke binaries.
- Letting Phase 02 drift into generic parser expansion because `docs/select_parser_roadmap.md` describes a broad feature surface.
- Making deterministic output too weak. The current `select_rich_smoke` summary proves clause presence, but not enough structure to distinguish many similar queries.
- Making deterministic output too strong. A full serialized AST is Phase 03/04 territory unless kept very narrow and test-only.
- Forgetting exclusions. `PARS-03` requires explicit documentation and continued exclusion of non-SQL and MSSQL branches.
- Implicitly expanding scope through lexer inheritance. The lexer contains some upstream/MySQL-oriented tokens, but that does not make them in-scope workload features.
- Missing currently deferred forms. Query parameters are intentionally deferred in `ExpressionElementParsers.cpp`; planning should treat them as excluded unless the workload proves otherwise.
- Forgetting manifest discipline. If imported/bridge parser files change, update `docs/ported_clickhouse_manifest.md` in the same phase.

## Known Scope Boundaries To Preserve
These are already visible from the implementation and docs and should stay explicit in planning:
- Parser-only project; no analyzer semantics.
- `SELECT` workload focus; not general statement coverage.
- Non-SQL and MSSQL remain excluded unless the roadmap is deliberately changed.
- Query parameters inside `{...}` are currently deferred and should stay excluded unless explicitly brought into scope.
- `ORDER BY` extras like `COLLATE` and `WITH FILL` are parser-acceptance features, not semantic features.
- `WINDOW` definitions currently preserve body text rather than parsing full internal semantics; that is acceptable for this phase.

## Suggested Exclusion Set For PARS-03
Unless the internal workload says otherwise, document these as explicitly excluded and keep them failing:
- non-`SELECT` statements such as `INSERT`, `CREATE`, `ALTER`, `DROP`, `SYSTEM`, `OPTIMIZE`, `WATCH`
- non-SQL inputs or command/meta shells
- MSSQL-specific syntax such as `TOP`, bracketed identifiers (`[dbo].[t]`), and table hints like `WITH (NOLOCK)`
- deferred query-parameter syntax using `{...}`

Use a small negative fixture set to prove these remain excluded.

## Code Examples
Useful implementation anchors:
- `ParserSelectRichQuery.cpp`: top-level `WITH`, `SELECT`, set ops, `SETTINGS`, `FORMAT`
- `SelectClausesLiteParsers.cpp`: `PREWHERE`/`WHERE`/`GROUP BY`/`HAVING`/`WINDOW`/`QUALIFY`/`ORDER BY`/`LIMIT` families
- `TableSourcesLiteParsers.cpp`: source atoms, aliases, join operator parsing, `ON`/`USING`
- `ExpressionOpsLiteParsers.cpp`: precedence parsing and most expression-surface coverage
- `examples/bootstrap/select_rich_smoke.cc`: current shape of stable parser summary output

## Validation Architecture
Use a three-layer validation architecture.

### Layer 1. Build and Baseline Guard
Keep the existing baseline commands green:
- `just smoke-suite`
- build `//ported_clickhouse:parser_lib`
- build the workload-summary validation target

This ensures Phase 02 does not regress the known parser baseline.

### Layer 2. Workload Fixture Matrix
Add checked-in workload fixtures grouped by construct and priority.
At minimum, maintain these buckets:
- core projection/select-only
- `FROM` sources
- joins
- clause combinations
- expression-heavy predicates/projections
- set operations / trailing settings-format
- excluded non-SQL / MSSQL cases

For each supported fixture:
- parse must succeed
- token stream must be fully consumed
- canonical summary output must match expected output exactly

For each excluded fixture:
- parse must fail deterministically with non-zero exit
- failure classification should be stable enough to show it remains out of scope

### Layer 3. Determinism Repetition Check
For every supported workload fixture, run the same fixture multiple times in one command and assert identical canonical output.
This satisfies `PARS-01` without requiring a versioned IR yet.

Recommended implementation:
- one binary reads either a single query or fixture file
- emits canonical single-line summary or compact structured text
- a shell wrapper or Bazel test compares actual vs expected fixture outputs
- a repeat mode runs the same query N times and diffs outputs internally

### Why This Validation Split Is Right
- Layer 1 protects Phase 01 achievements.
- Layer 2 proves workload coverage directly against requirements.
- Layer 3 proves determinism explicitly instead of assuming it from a single successful parse.
- None of the three layers requires designing the future IR contract.

## Planning Implications
A good Phase 02 plan should likely break into these workstreams:
1. Define and check in the workload fixture corpus plus explicit exclusion list.
2. Build the canonical workload-summary runner and fixture harness.
3. Fix parser gaps exposed by fixture failures, one parser layer at a time.
4. Document supported workload constructs and explicit exclusions.
5. Verify deterministic repeat behavior and baseline-smoke non-regression.

## Questions To Resolve Before Planning
- Where will the prioritized internal workload queries come from, and how many must be included in the acceptance corpus?
- Is a compact text summary enough for expected outputs, or do you want a minimal JSON-like stable format for fixture comparison?
- Do you want exclusion coverage limited to representative non-SQL/MSSQL samples, or a broader negative matrix?
- Are query parameters intentionally still excluded in this milestone?

## Recommended Planning Defaults
If those questions are not answered before planning, use these defaults:
- plan around a representative checked-in workload corpus, not exhaustive parity
- use a compact deterministic text summary emitted by a dedicated validation binary
- include a small but explicit negative matrix for non-SQL and MSSQL exclusions
- keep query parameters excluded
- keep all validation artifacts local to parser bridge tooling and phase docs
