# Parser Workload Scope (Phase 02)

This document is the Phase 02 source of truth for parser workload coverage and explicit exclusions. It is intentionally constrained to what the checked-in fixture corpus proves.

## Supported Workload Families

The following forms are in-scope and enforced by `testdata/parser_workload/manifest.json` `success` fixtures:

- Core `SELECT ... FROM ...` projection shape (`p0_simple_select`)
- `SELECT DISTINCT` with predicate/order/limit (`p0_distinct_where_order_limit`)
- `WITH` CTE surface (`p0_with_cte`)
- `INNER JOIN ... ON ...` (`p0_inner_join_on`)
- `GROUP BY ALL WITH ROLLUP HAVING ...` (`p1_group_rollup_having`)
- `LIMIT ... BY ALL` (`p1_limit_by_all`)
- `UNION` plus trailing `SETTINGS` and `FORMAT` (`p1_union_settings_format`)

## Explicit Exclusions

The following out-of-scope families are intentionally rejected and enforced by `excluded` fixtures:

- Non-SQL input (`p0_non_sql_input`)
- Non-select statements (`p0_insert_statement`)
- MSSQL-only syntax:
  - `TOP` (`p0_mssql_top`)
  - Bracketed identifiers (`p0_mssql_bracket_identifier`)
  - Table hints like `WITH (NOLOCK)` (`p0_mssql_nolock_hint`)
- Deferred query-parameter form `{...}` (`p1_query_parameter`)

## Scope Guardrails

- Parser-only scope: no semantic analysis, execution planning, or runtime guarantees.
- The manifest and fixtures are authoritative; documentation must not claim support beyond fixture-backed evidence.
- Deferred forms remain out of scope unless they are explicitly added to the workload source, manifest, and expected-summary assertions.
