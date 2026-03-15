# SELECT Dialect Missing Features (ClickHouse vs `ported_clickhouse`)

This document enumerates language features present in ClickHouse `src/Parsers` that are missing or incomplete in the local `ported_clickhouse/parsers` SELECT stack.

Primary baseline sources:
- `/Users/dorran/dev/clickshack/tmp/clickhouse-patching/src/Parsers/ParserSelectQuery.cpp`
- `/Users/dorran/dev/clickshack/tmp/clickhouse-patching/src/Parsers/ParserSelectWithUnionQuery.cpp`
- `/Users/dorran/dev/clickshack/tmp/clickhouse-patching/src/Parsers/ParserTablesInSelectQuery.cpp`
- `/Users/dorran/dev/clickshack/tmp/clickhouse-patching/src/Parsers/ExpressionElementParsers.cpp`

Implementation sources:
- `/Users/dorran/dev/clickshack/ported_clickhouse/parsers/ParserSelectRichQuery.cpp`
- `/Users/dorran/dev/clickshack/ported_clickhouse/parsers/SelectClausesLiteParsers.cpp`
- `/Users/dorran/dev/clickshack/ported_clickhouse/parsers/TableSourcesLiteParsers.cpp`
- `/Users/dorran/dev/clickshack/ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp`

## 1) SELECT Modifiers and CTE Surface

1. `DISTINCT ON (expr_list)` is missing.
   - **Status: implemented**
   - Upstream supports it in SELECT-head parsing.
   - Local parser supports `DISTINCT`, but no `ON (...)` payload.

2. `TOP N [WITH TIES]` is missing.
   - **Status: implemented**
   - Upstream supports `TOP` syntax.
   - Local clause parser explicitly excludes TOP support.

3. `SELECT ALL` is parsed but not preserved in AST state.
   - **Status: implemented**
   - Upstream preserves equivalent semantics in AST/query tree.
   - Local parser consumes token without explicit AST representation.

4. `WITH RECURSIVE` is parsed but not preserved in AST state.
   - **Status: implemented**
   - Upstream tracks recursive CTE behavior in structured AST path.
   - Local parser accepts `RECURSIVE` token but does not model it.

5. `WITH expr AS alias` / bare alias retention is incomplete.
   - **Status: partial**
   - Local parser accepts expression-with alias forms in WITH.
   - Alias payload is not fully represented except CTE subquery alias path.

## 2) FROM/JOIN Dialect Gaps

6. `LOCAL` join modifier is missing.
   - **Status: implemented**
   - Upstream join parser supports `GLOBAL|LOCAL`.
   - Local parser models `GLOBAL` but not `LOCAL`.

7. `PASTE JOIN` is missing.
   - **Status: implemented**
   - Upstream supports join kind `PASTE`.
   - Local join kind enum/parser excludes `PASTE`.

8. Parenthesized table expressions in `FROM` (non-subquery forms) are missing.
   - **Status: missing**
   - Local `(...)` table atom path only accepts nested SELECT/WITH subqueries.
   - Upstream table expression surface is broader.

## 3) WINDOW / Analytic Expression Gaps

9. `OVER (...)` parsing in expressions is missing.
   - **Status: partial**
   - Upstream expression parser supports window application on functions.
   - Local expression parser does not handle `OVER`.

10. Window definitions are not structured in AST.
    - **Status: partial**
    - Local `WINDOW` clause stores definition body as raw text.
    - Upstream uses structured AST for window definitions/components.

## 4) ORDER BY Feature/AST Gaps

11. `NULLS FIRST|LAST` is parser-accepted but dropped from AST.
    - **Status: implemented**

12. `COLLATE <locale>` is parser-accepted but dropped from AST.
    - **Status: implemented**

13. `WITH FILL [FROM/TO/STEP/STALENESS]` is parser-accepted but dropped from AST.
    - **Status: implemented**

14. `INTERPOLATE (...)` is parser-accepted but dropped from AST.
    - **Status: implemented**

15. `ORDER BY ALL` is missing.
    - **Status: implemented**
    - Upstream has ORDER BY ALL handling.
    - Local parser expects explicit expression list.

## 5) GROUP BY Gaps

16. `GROUPING SETS (...)` is missing.
    - **Status: implemented**
    - Upstream supports GROUPING SETS family.
    - Local parser currently supports `GROUP BY ALL` and `WITH ROLLUP|CUBE|TOTALS` only.

## 6) Expression Grammar Gaps

17. Simple CASE (`CASE <expr> WHEN ... THEN ... END`) is missing.
    - **Status: implemented**
    - Local parser supports searched CASE (`CASE WHEN ...`) only.

18. `NULL` literal fidelity is missing.
    - **Status: missing**
    - Local AST literal kind lacks native null-kind representation.
    - `NULL` is currently encoded as string-like payload.

## 7) LIMIT/OFFSET/FETCH Fidelity Gaps

19. LIMIT/OFFSET numeric-expression flexibility is missing.
    - **Status: implemented**
    - Local parser restricts LIMIT/OFFSET slots to numeric tokens.
    - Upstream parser accepts broader expression forms in these positions.

## 8) Scope-Declared Dialect Exclusions (Still Missing by Design)

These are currently excluded by workload contract, but still missing relative to full dialect breadth:

20. Query parameter syntax (`{name:Type}` family) is deferred.
    - **Status: excluded (by design)**
    - See parser workload exclusions and identifier parser note.

21. MSSQL-specific `TOP` (already listed above), bracket identifiers (`[x]`), and table-hint `WITH (NOLOCK)` are excluded.
    - **Status: excluded (by design)**

22. Non-SELECT statements are out of scope for current parser phase.
    - **Status: excluded (by design)**

## Coverage Conclusion

The local SELECT parser is broad for core query forms (projection, FROM/JOIN basics, PREWHERE/WHERE/GROUP/HAVING/WINDOW/QUALIFY, ORDER/LIMIT, set ops, SETTINGS/FORMAT), but it does not fully cover ClickHouse dialect surface or AST fidelity yet.

The highest-value implementation targets are:
1. `DISTINCT ON`, `TOP`, `LOCAL/PASTE JOIN`, `GROUPING SETS`, `OVER (...)`.
2. Structured AST retention for `RECURSIVE`, SELECT ALL, ORDER BY modifiers, WINDOW definitions.
3. Expression-fidelity fixes (simple CASE, true NULL literal kind, richer LIMIT/OFFSET expressions).
