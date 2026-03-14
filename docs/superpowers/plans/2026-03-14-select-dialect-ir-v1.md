# Full SELECT Dialect + IR v1 Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close all remaining SELECT dialect parser gaps and deliver a versioned JSON IR v1 with C++ serializer, Python Pydantic models, SQLGlot adapter, and linter framework.

**Architecture:** Audit gate first to establish ground truth on remaining parser gaps. Then two tracks: (1) close missing/partial parser gaps in `ported_clickhouse/parsers/`; (2) implement JSON IR serializer in `ir/` using `nlohmann/json`. Python layer in `clickshack/` consumes the JSON IR via Pydantic v2 models, a SQLGlot adapter, and a linter protocol. The C++ binary `//examples/bootstrap:ir_dump` bridges C++ and Python via stdin/stdout JSON.

**Tech Stack:** C++17, Bazel, nlohmann/json (BCR), Python 3.11+, Pydantic v2, SQLGlot, pytest

**Spec:** `docs/superpowers/specs/2026-03-14-select-parsing-ir-v1-design.md`

---

## File Map

### Modified (parser gaps — exact changes depend on audit)
- `ported_clickhouse/parsers/ASTLiteral.h` — add `Null` kind to enum
- `ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp` — simple CASE, NULL literal
- `ported_clickhouse/parsers/SelectClausesLiteParsers.cpp` — ORDER BY modifiers, GROUPING SETS, ORDER BY ALL, LIMIT expressions (only items audit marks partial/missing)
- `docs/select_dialect_missing_features.md` — add Status column to all 22 items
- `testdata/parser_workload/manifest.json` — add fixtures for newly closed gaps
- `docs/parser_workload_scope.md` — update supported items
- `docs/parser_workload_reconciliation.md` — update reconciliation

### New (C++ IR)
- `ir/IRSerializer.h` — IRSerializer class declaration
- `ir/IRSerializer.cpp` — AST walker → nlohmann::json
- `ir/BUILD.bazel` — `//ir:ir_json` cc_library
- `examples/bootstrap/ir_dump.cc` — stdin SQL → stdout JSON binary
- `examples/bootstrap/BUILD.bazel` — add `ir_dump` target
- `MODULE.bazel` — add `nlohmann_json` bazel_dep

### New (Python)
- `pyproject.toml` — package declaration
- `clickshack/__init__.py` — empty
- `clickshack/ir/__init__.py` — empty
- `clickshack/ir/models.py` — Pydantic v2 IR models
- `clickshack/ir/adapter.py` — IR → SQLGlot AST adapter
- `clickshack/linter/__init__.py` — empty
- `clickshack/linter/base.py` — LintRule protocol + LintRunner
- `tests/test_ir_models.py` — pytest model tests
- `tests/test_adapter.py` — pytest adapter tests
- `tests/test_linter.py` — pytest linter tests

---

## Chunk 1: Audit Gate

### Task 1: Audit Parser Gaps and Update Status Doc

**Files:**
- Modify: `docs/select_dialect_missing_features.md`

The gap doc is stale after Phase 02.1. Audit the actual C++ source to determine which of the 22 items are now implemented, partial, or missing.

**Status taxonomy (four values):**
- `implemented` — fully handled and reflected in AST
- `partial` — token consumed but not fully retained in AST, or only some forms work
- `missing` — not handled at all
- `excluded (by design)` — intentionally out of scope (items 20–22 only, do not change)

**Status line format:** For each numbered item, insert `**Status: <value>**` as the **first bullet** under the item heading, before any existing sub-bullets. Example:
```
1. `DISTINCT ON (expr_list)` is missing.
   - **Status: implemented**
   - Upstream supports it in SELECT-head parsing.
   ...
```

- [ ] **Step 1: Read ParserSelectRichQuery.cpp for SELECT-head items**

Open `ported_clickhouse/parsers/ParserSelectRichQuery.cpp`. For each item, search for the AST field assignment listed and record status:

| Doc item | Description | Search for | Implemented if... |
|----------|-------------|-----------|-------------------|
| 1 | `DISTINCT ON (expr_list)` | `distinct_on_expressions` | `q->distinct_on_expressions` is assigned |
| 2 | `TOP N [WITH TIES]` | `top_present`, `top_count`, `top_with_ties` | all three fields are set |
| 3 | `SELECT ALL` | `select_all` | `q->select_all = true` is set |
| 4 | `WITH RECURSIVE` | `with_recursive` | `q->with_recursive = true` is set |
| 5 | `WITH expr AS alias` retention | `with_expressions` children | alias payload of non-CTE WITH items is represented |

- [ ] **Step 2: Read TableSourcesLiteParsers.cpp for FROM/JOIN items**

| Doc item | Description | Search for | Implemented if... |
|----------|-------------|-----------|-------------------|
| 6 | `LOCAL` join modifier | `JoinLocality::Local` | `join->join_locality = JoinLocality::Local` assigned |
| 7 | `PASTE JOIN` | `JoinType::Paste` | `join->join_type = JoinType::Paste` assigned |
| 8 | Parenthesized table exprs in FROM | `(` handling in table atom | non-subquery `(...)` forms accepted as table source |

- [ ] **Step 3: Read ExpressionOpsLiteParsers.cpp for expression items**

| Doc item | Description | Search for | Implemented if... |
|----------|-------------|-----------|-------------------|
| 9 | `OVER (...)` in expressions | `OVER` keyword near function parsing | after a function call, `OVER` token causes additional args or window annotation to be appended to `ASTFunction` (check whether `OVER` is consumed and retained, or consumed and discarded) |
| 17 | Simple CASE (`CASE <expr> WHEN`) | CASE parsing block | handles both `CASE WHEN` (searched) AND `CASE <expr> WHEN` (simple) — look for operand expression parse before WHEN |
| 18 | `NULL` literal fidelity | `NULL` token handling | emits `ASTLiteral` with `Kind::Null` (not `Kind::String` with value `"NULL"`) |

- [ ] **Step 4: Read SelectClausesLiteParsers.cpp for clause items**

| Doc item | Description | Search for | Implemented if... |
|----------|-------------|-----------|-------------------|
| 10 | Window definitions structured in AST | WINDOW clause parsing | `ASTWindowDefinitionLite` fields populated beyond `body` string (structured parse) |
| 11 | `NULLS FIRST\|LAST` retention | `nulls_order` | `elem->nulls_order` assigned (not just token consumed) |
| 12 | `COLLATE <locale>` retention | `collate_name` | `elem->collate_name` assigned |
| 13 | `WITH FILL` retention | `with_fill`, `fill_from` | `elem->with_fill = true` AND fill expression fields are set |
| 14 | `INTERPOLATE` retention | `interpolate_expressions` | `elem->interpolate_expressions` assigned |
| 15 | `ORDER BY ALL` | `order_by_all` | `q->order_by_all = true` set when `ALL` follows `ORDER BY` |
| 16 | `GROUPING SETS (...)` | `grouping_sets_expressions` | `q->grouping_sets_expressions` assigned |
| 19 | LIMIT/OFFSET expression flexibility | `limit_expression`, `offset_expression` | `lim->limit_expression` is assigned from a full expression parse (not just a numeric token read) |

- [ ] **Step 5: Update docs/select_dialect_missing_features.md**

Using the status taxonomy and placement format defined above, add `**Status: <value>**` as the first bullet to each of the 22 numbered items. Items 20–22 get `**Status: excluded (by design)**`.

- [ ] **Step 6: Run baseline verification**

```bash
bash tools/parser_workload_suite.sh full \
  --manifest testdata/parser_workload/manifest.json \
  --source docs/select_dialect_missing_features.md \
  --signoff docs/parser_workload_reconciliation.md
just run-ported-smokes
```

Expected: both pass (same as Phase 02.1 result). Note the count of supported fixtures before making any code changes.

- [ ] **Step 7: Commit the audit doc**

```bash
git add docs/select_dialect_missing_features.md
git commit -m "docs: audit parser gap status after phase 02.1 — add Status to each item"
```

---

## Chunk 2: Parser Gap Closure

**For every task in this chunk: skip entirely if the audit (Task 1) shows status=implemented for that item.**

### Task 2: NULL Literal Kind (skip if implemented)

**Files:**
- Modify: `ported_clickhouse/parsers/ASTLiteral.h`
- Modify: `ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp`

- [ ] **Step 1: Add `Null` kind to ASTLiteral**

In `ported_clickhouse/parsers/ASTLiteral.h`, add `Null` to the `Kind` enum and update `getID`:

```cpp
enum class Kind : UInt8
{
    Number,
    String,
    Null,
};

String getID(char delim) const override
{
    if (kind == Kind::Null)
        return "Literal" + String(1, delim) + "NULL";
    return "Literal" + String(1, delim) + value;
}
```

Also update `clone()` — it constructs `make_intrusive<ASTLiteral>(kind, value)` which already works since Null kind has an empty value.

- [ ] **Step 2: Find and fix NULL token handling in ExpressionOpsLiteParsers.cpp**

Search for where the `NULL` keyword is parsed (look for `"NULL"` near `ASTLiteral` construction). Change:

```cpp
// Before (emits null as a string literal):
result = make_intrusive<ASTLiteral>(ASTLiteral::Kind::String, String("NULL"));

// After (use Null kind):
result = make_intrusive<ASTLiteral>(ASTLiteral::Kind::Null, String{});
```

- [ ] **Step 3: Build**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_select_rich_stack
```

Expected: compiles cleanly.

- [ ] **Step 4: Quick smoke**

```bash
echo "SELECT NULL, 1, 'hello' FROM t" | \
  bazel --batch --output_user_root=/tmp/bazel-root run //examples/bootstrap:select_rich_smoke
```

Expected: parses successfully.

- [ ] **Step 5: Commit**

```bash
git add ported_clickhouse/parsers/ASTLiteral.h ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp
git commit -m "fix: add Null kind to ASTLiteral for proper NULL literal representation"
```

### Task 3: Simple CASE Expression (skip if implemented)

**Files:**
- Modify: `ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp`

Simple CASE: `CASE <operand> WHEN <val> THEN <result> ... [ELSE <default>] END`
Searched CASE (already works): `CASE WHEN <cond> THEN <result> ... [ELSE <default>] END`

Both forms use `ASTFunction` with name `"CASE"`. For simple CASE, the operand is inserted as the first argument before the WHEN/THEN pairs.

- [ ] **Step 1: Locate the CASE parsing block**

Search `ExpressionOpsLiteParsers.cpp` for `isKeyword(pos, "CASE")` or `"CASE"`. Find the block that parses WHEN/THEN pairs.

- [ ] **Step 2: Add simple CASE path**

After consuming the `CASE` keyword, peek at the next token. If it is NOT `WHEN` and NOT `END`, it is a simple CASE operand:

```cpp
// After ++pos (consumed CASE):
ASTPtr operand;
if (!isKeyword(pos, "WHEN") && !isKeyword(pos, "END")) {
    // Simple CASE: parse operand expression, stop before WHEN keyword
    if (!parseExpressionElement(pos, end, operand, expected))
        return false;
    fn->arguments->children.push_back(std::move(operand));
}
// Then fall through to existing WHEN/THEN parsing loop
```

The exact function name (`parseExpressionElement` or similar) must match what is used elsewhere in the same file.

- [ ] **Step 3: Build and smoke test**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_select_rich_stack
echo "SELECT CASE x WHEN 1 THEN 'a' WHEN 2 THEN 'b' ELSE 'c' END FROM t" | \
  bazel --batch --output_user_root=/tmp/bazel-root run //examples/bootstrap:select_rich_smoke
```

Expected: parses without error.

Also verify searched CASE still works:

```bash
echo "SELECT CASE WHEN x = 1 THEN 'a' ELSE 'b' END FROM t" | \
  bazel --batch --output_user_root=/tmp/bazel-root run //examples/bootstrap:select_rich_smoke
```

- [ ] **Step 4: Commit**

```bash
git add ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp
git commit -m "fix: add simple CASE expression (CASE expr WHEN val THEN result)"
```

### Task 4: ORDER BY Modifier AST Retention (skip if all four are implemented)

**Files:**
- Modify: `ported_clickhouse/parsers/SelectClausesLiteParsers.cpp`

The `ASTOrderByElementLite` already has all fields (`nulls_order`, `collate_name`, `with_fill`/fill fields, `interpolate`/`interpolate_expressions`). If the parser consumes these tokens but doesn't assign the fields, fix each one.

- [ ] **Step 1: Locate ORDER BY element parsing**

In `SelectClausesLiteParsers.cpp`, find the section that parses a single ORDER BY element (after the expression). Check each of the four modifiers:

**NULLS FIRST/LAST** — if the field is not set, add:
```cpp
if (isKeyword(pos, "NULLS")) {
    ++pos;
    if (isKeyword(pos, "FIRST")) {
        elem->nulls_order = ASTOrderByElementLite::NullsOrder::First; ++pos;
    } else if (isKeyword(pos, "LAST")) {
        elem->nulls_order = ASTOrderByElementLite::NullsOrder::Last; ++pos;
    }
}
```

**COLLATE** — if `collate_name` is not set, add:
```cpp
if (isKeyword(pos, "COLLATE")) {
    ++pos;
    if (pos->type == DB::TokenType::StringLiteral || pos->type == DB::TokenType::BareWord) {
        elem->collate_name = pos->toString(); ++pos;
    }
}
```

**WITH FILL** — if `with_fill` and fill fields are not set, add assignment `elem->with_fill = true` and parse FROM/TO/STEP/STALENESS subexpressions into `elem->fill_from`, `elem->fill_to`, `elem->fill_step`, `elem->fill_staleness`.

**INTERPOLATE** — if `interpolate` and `interpolate_expressions` are not set, parse the expression list and assign.

- [ ] **Step 2: Build**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_select_rich_stack
```

- [ ] **Step 3: Commit**

```bash
git add ported_clickhouse/parsers/SelectClausesLiteParsers.cpp
git commit -m "fix: retain ORDER BY modifiers (NULLS, COLLATE, WITH FILL, INTERPOLATE) in AST"
```

### Task 5: Remaining Audit-Driven Gaps (skip items that are implemented)

**Files:** Varies by audit result for GROUPING SETS, ORDER BY ALL, LIMIT/OFFSET expressions.

For each remaining `partial` or `missing` item from the audit not covered by Tasks 2–4:

**GROUPING SETS** (`SelectClausesLiteParsers.cpp`): If `grouping_sets_expressions` not assigned, find GROUP BY parsing and add handling for `GROUPING SETS (...)` — parse as a list of expression lists assigned to `q->grouping_sets_expressions`.

**ORDER BY ALL** (`SelectClausesLiteParsers.cpp`): If `order_by_all` not set, add check before the expression list parse:
```cpp
if (isKeyword(pos, "ALL")) { q->order_by_all = true; ++pos; } else { /* parse list */ }
```

**LIMIT/OFFSET expressions** (`SelectClausesLiteParsers.cpp`): If `limit_expression`/`offset_expression` not set from a full expression parse, change the LIMIT slot from a bare number parse to call the expression parser and assign `lim->limit_expression`.

For each fix:
1. Make the change
2. Build: `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_select_rich_stack`
3. Smoke with a representative query
4. Commit with a descriptive message

### Task 6: Add Workload Fixtures for Newly Closed Gaps

**Files:**
- Modify: `testdata/parser_workload/manifest.json`
- Modify: `docs/parser_workload_scope.md`
- Modify: `docs/parser_workload_reconciliation.md`

For each item changed from partial/missing to implemented (Tasks 2–5):

- [ ] **Step 1: Add a manifest fixture for each closed gap**

Follow the existing manifest JSON format. Example entry:

```json
{
  "workload_source_id": "null_literal_p1",
  "sql": "SELECT NULL, x FROM t WHERE y IS NULL",
  "priority": "p1",
  "status": "supported",
  "expected_summary": "<run parser to get this — see Step 2>"
}
```

- [ ] **Step 2: Get canonical summary for each new fixture**

Run the `parser_workload_summary` binary against each new SQL:

```bash
echo "SELECT NULL, x FROM t WHERE y IS NULL" | \
  bazel --batch --output_user_root=/tmp/bazel-root run //examples/bootstrap:parser_workload_summary -- quick
```

Capture the output and paste it as `expected_summary`. Repeat for each new fixture.

- [ ] **Step 3: Run supported suite**

```bash
bash tools/parser_workload_suite.sh supported \
  --manifest testdata/parser_workload/manifest.json
```

Expected: `supported ok: N fixtures` (N is now larger than before).

- [ ] **Step 4: Run full suite**

```bash
bash tools/parser_workload_suite.sh full \
  --manifest testdata/parser_workload/manifest.json \
  --source docs/select_dialect_missing_features.md \
  --signoff docs/parser_workload_reconciliation.md
just run-ported-smokes
```

Expected: passes.

- [ ] **Step 5: Commit**

```bash
git add testdata/parser_workload/manifest.json \
        docs/parser_workload_scope.md \
        docs/parser_workload_reconciliation.md \
        docs/select_dialect_missing_features.md
git commit -m "test: add workload fixtures for newly closed parser gaps"
```

---

## Chunk 3: C++ IR Serializer

### Task 7: Add nlohmann/json Dependency

**Files:**
- Modify: `MODULE.bazel`

- [ ] **Step 1: Add nlohmann_json to MODULE.bazel**

Open `MODULE.bazel` and add after the existing `bazel_dep` lines:

```python
bazel_dep(name = "nlohmann_json", version = "3.11.3", repo_name = "nlohmann_json")
```

- [ ] **Step 2: Verify the dep resolves**

```bash
bazel --batch --output_user_root=/tmp/bazel-root query @nlohmann_json//...
```

Expected: prints targets including something like `@nlohmann_json//:json`. If the version or module name differs in BCR, adjust. The include path will be `#include "nlohmann/json.hpp"`.

> **Fallback if 3.11.3 is unavailable:** Check BCR with `bazel mod deps` or try `3.11.2`. The API is stable across 3.x minor versions.

- [ ] **Step 3: Commit**

```bash
git add MODULE.bazel MODULE.bazel.lock
git commit -m "deps: add nlohmann/json 3.11.3 for IR JSON serialization"
```

### Task 8: Create ir/ Directory with IRSerializer Skeleton

**Files:**
- Create: `ir/BUILD.bazel`
- Create: `ir/IRSerializer.h`
- Create: `ir/IRSerializer.cpp`

- [ ] **Step 1: Create `ir/BUILD.bazel`**

```python
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "ir_json",
    srcs = ["IRSerializer.cpp"],
    hdrs = ["IRSerializer.h"],
    deps = [
        "//ported_clickhouse:parser_select_rich_stack",
        "@nlohmann_json//:json",
    ],
    visibility = ["//visibility:public"],
)
```

> Confirm the nlohmann_json target label with `bazel query @nlohmann_json//...`. Adjust if the label differs (e.g., `@nlohmann_json//:nlohmann_json`).

- [ ] **Step 2: Create `ir/IRSerializer.h`**

```cpp
#pragma once

#include "nlohmann/json.hpp"
#include "ported_clickhouse/parsers/IAST.h"

namespace clickshack
{

class IRSerializer
{
public:
    // Entry point. Accepts ASTSelectRichQuery* or ASTSelectSetLite* at root.
    // Returns envelope: {"ir_version": "1", "query": {...}}
    static nlohmann::json serializeQuery(const DB::IAST * ast);

private:
    static nlohmann::json serializeNode(const DB::IAST * node);
    static nlohmann::json serializeSelect(const DB::IAST * node);
    static nlohmann::json serializeUnion(const DB::IAST * node);
    static nlohmann::json serializeExpr(const DB::IAST * node);
    static nlohmann::json serializeExprList(const DB::IAST * node);
    static nlohmann::json serializeTableExpr(const DB::IAST * node);
    static nlohmann::json serializeOrderByList(const DB::IAST * node);
    static nlohmann::json serializeWindowList(const DB::IAST * node);
    static nlohmann::json serializeLimitClause(const DB::IAST * node);
    static nlohmann::json serializeLimitByClause(const DB::IAST * node);
};

} // namespace clickshack
```

- [ ] **Step 3: Create `ir/IRSerializer.cpp` skeleton**

```cpp
#include "ir/IRSerializer.h"

#include "ported_clickhouse/parsers/ASTExpressionList.h"
#include "ported_clickhouse/parsers/ASTExpressionOpsLite.h"
#include "ported_clickhouse/parsers/ASTFunction.h"
#include "ported_clickhouse/parsers/ASTIdentifier.h"
#include "ported_clickhouse/parsers/ASTJoinLite.h"
#include "ported_clickhouse/parsers/ASTLimitByLite.h"
#include "ported_clickhouse/parsers/ASTLimitLite.h"
#include "ported_clickhouse/parsers/ASTLiteral.h"
#include "ported_clickhouse/parsers/ASTOrderByElementLite.h"
#include "ported_clickhouse/parsers/ASTOrderByListLite.h"
#include "ported_clickhouse/parsers/ASTSelectRichQuery.h"
#include "ported_clickhouse/parsers/ASTSelectSetLite.h"
#include "ported_clickhouse/parsers/ASTTableAliasLite.h"
#include "ported_clickhouse/parsers/ASTTableExprLite.h"
#include "ported_clickhouse/parsers/ASTTableFunctionLite.h"
#include "ported_clickhouse/parsers/ASTTableIdentifierLite.h"
#include "ported_clickhouse/parsers/ASTWindowDefinitionLite.h"
#include "ported_clickhouse/parsers/ASTWindowListLite.h"

#include <stdexcept>

namespace clickshack
{

using json = nlohmann::json;
using namespace DB;

// v1: spans are placeholders — AST does not carry source positions yet
static json span0() { return {{"start", 0}, {"end", 0}}; }

nlohmann::json IRSerializer::serializeQuery(const IAST * ast)
{
    if (!ast) throw std::runtime_error("IRSerializer: null AST");
    return {{"ir_version", "1"}, {"query", serializeNode(ast)}};
}

nlohmann::json IRSerializer::serializeNode(const IAST * node)
{
    if (!node) throw std::runtime_error("IRSerializer: null node");
    if (node->as<ASTSelectRichQuery>()) return serializeSelect(node);
    if (node->as<ASTSelectSetLite>())   return serializeUnion(node);
    return serializeExpr(node);
}

// Stubs — replaced in Tasks 9 and 11
nlohmann::json IRSerializer::serializeSelect(const IAST *) { return {{"type","Select"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeUnion(const IAST *)  { return {{"type","SelectUnion"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeExpr(const IAST * n) { return {{"type","Raw"},{"id",n->getID('_')},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeExprList(const IAST *) { return json::array(); }
nlohmann::json IRSerializer::serializeTableExpr(const IAST *) { return {{"type","Raw"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeOrderByList(const IAST *) { return json::array(); }
nlohmann::json IRSerializer::serializeWindowList(const IAST *) { return json::array(); }
nlohmann::json IRSerializer::serializeLimitClause(const IAST *) { return {{"type","Limit"},{"span",span0()}}; }
nlohmann::json IRSerializer::serializeLimitByClause(const IAST *) { return {{"type","LimitBy"},{"span",span0()}}; }

} // namespace clickshack
```

- [ ] **Step 4: Build the skeleton**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build //ir:ir_json
```

Expected: compiles cleanly.

- [ ] **Step 5: Commit**

```bash
git add ir/
git commit -m "feat: add ir/ directory with IRSerializer skeleton"
```

### Task 9: Implement IRSerializer — Select and Union Nodes

**Files:**
- Modify: `ir/IRSerializer.cpp`

Replace the `serializeSelect` and `serializeUnion` stubs.

- [ ] **Step 1: Implement serializeSelect**

```cpp
nlohmann::json IRSerializer::serializeSelect(const IAST * node)
{
    const auto * q = node->as<ASTSelectRichQuery>();
    json j;

    if (q->distinct)                j["distinct"] = true;
    if (q->select_all)              j["all"] = true;
    if (q->distinct_on_expressions) j["distinct_on"] = serializeExprList(q->distinct_on_expressions);
    if (q->top_present)             j["top"] = {{"count", q->top_count}, {"with_ties", q->top_with_ties}};

    j["projections"] = q->expressions ? serializeExprList(q->expressions) : json::array();

    if (q->from_source) {
        json from = serializeTableExpr(q->from_source);
        if (q->from_final) from["final"] = true;
        if (q->sample_size)   from["sample"]        = serializeExpr(q->sample_size);
        if (q->sample_offset) from["sample_offset"] = serializeExpr(q->sample_offset);
        j["from"] = std::move(from);
    }
    if (q->array_join_expressions)
        j["array_join"] = {{"left", q->array_join_is_left},
                           {"exprs", serializeExprList(q->array_join_expressions)}};
    if (q->prewhere_expression)  j["prewhere"]  = serializeExpr(q->prewhere_expression);
    if (q->where_expression)     j["where"]     = serializeExpr(q->where_expression);

    if (q->group_by_all) {
        j["group_by"] = {{"all", true}};
    } else if (q->grouping_sets_expressions) {
        j["group_by"] = {{"grouping_sets", serializeExprList(q->grouping_sets_expressions)}};
    } else if (q->group_by_expressions) {
        json gb = {{"exprs", serializeExprList(q->group_by_expressions)}};
        if (q->group_by_with_rollup) gb["rollup"] = true;
        if (q->group_by_with_cube)   gb["cube"]   = true;
        if (q->group_by_with_totals) gb["totals"]  = true;
        j["group_by"] = std::move(gb);
    }

    if (q->having_expression)   j["having"]  = serializeExpr(q->having_expression);
    if (q->window_list)         j["window"]  = serializeWindowList(q->window_list);
    if (q->qualify_expression)  j["qualify"] = serializeExpr(q->qualify_expression);

    if (q->order_by_all) {
        j["order_by"] = {{"all", true}};
    } else if (q->order_by_list) {
        j["order_by"] = serializeOrderByList(q->order_by_list);
    }

    if (q->limit)    j["limit"]    = serializeLimitClause(q->limit);
    if (q->limit_by) j["limit_by"] = serializeLimitByClause(q->limit_by);
    if (q->settings) j["settings"] = serializeExprList(q->settings);
    if (q->has_format) j["format"] = q->format_name;

    // If WITH clause present, wrap Select in a With root
    if (q->with_expressions) {
        json with_node;
        with_node["type"]      = "With";
        with_node["span"]      = span0();
        with_node["recursive"] = q->with_recursive;

        // CTE items: read how WITH items are stored in with_expressions->children.
        // Common: each child is an ASTExpressionOpsLite with op="AS",
        //   left=ASTIdentifier (name), right=subquery ASTSelectRichQuery.
        // OR: ASTFunction with name=alias, args=[subquery].
        // Read ParserSelectRichQuery.cpp WITH section to confirm, then implement here.
        json ctes = json::array();
        for (const auto & child : q->with_expressions->children) {
            if (!child) continue;
            // Placeholder: serialize raw until CTE storage is confirmed
            ctes.push_back({{"raw", child->getID('_')}});
        }
        with_node["ctes"] = std::move(ctes);
        j["type"] = "Select";
        j["span"] = span0();
        with_node["body"] = std::move(j);
        return with_node;
    }

    j["type"] = "Select";
    j["span"] = span0();
    return j;
}
```

> **CTE placeholder note:** The CTE children serialization above uses a raw placeholder. Before Task 9 is complete, read `ParserSelectRichQuery.cpp`'s WITH clause parsing to find the exact AST node type used for each CTE item, then replace the placeholder loop with proper serialization of `{name, query}` pairs.

- [ ] **Step 2: Implement serializeUnion**

`ASTSelectSetLite` stores children as a flat list of SELECT nodes and `set_ops`/`union_modes` as parallel vectors. Build a left-associative tree of SelectUnion nodes:

```cpp
nlohmann::json IRSerializer::serializeUnion(const IAST * node)
{
    const auto * u = node->as<ASTSelectSetLite>();
    if (u->children.empty())
        throw std::runtime_error("IRSerializer: SelectSetLite with no children");

    json result = serializeNode(u->children[0].get());
    for (size_t i = 0; i < u->set_ops.size() && i + 1 < u->children.size(); ++i) {
        json un;
        un["type"]       = "SelectUnion";
        un["span"]       = span0();
        un["op"]         = u->set_ops[i];
        un["quantifier"] = (i < u->union_modes.size()) ? u->union_modes[i] : "";
        un["left"]       = std::move(result);
        un["right"]      = serializeNode(u->children[i + 1].get());
        result = std::move(un);
    }
    return result;
}
```

- [ ] **Step 3: Build**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build //ir:ir_json
```

- [ ] **Step 4: Commit**

```bash
git add ir/IRSerializer.cpp
git commit -m "feat: implement IRSerializer Select and Union node serialization"
```

### Task 10: Implement IRSerializer — Expressions

**Files:**
- Modify: `ir/IRSerializer.cpp`

**Before starting:** Read `ExpressionOpsLiteParsers.cpp` to identify the exact string values assigned to `ASTExpressionOpsLite::op` for: CASE, CAST, `::`, IS NULL, IS NOT NULL, BETWEEN, NOT BETWEEN, LIKE, NOT LIKE, ILIKE, NOT ILIKE, IN, NOT IN, lambda `->`, ARRAY, TUPLE, SUBQUERY, STAR, TABLE_STAR. The comparisons in the code below must match those exact strings.

- [ ] **Step 1: Implement serializeExprList**

```cpp
nlohmann::json IRSerializer::serializeExprList(const IAST * node)
{
    json arr = json::array();
    if (!node) return arr;
    for (const auto & child : node->children)
        if (child) arr.push_back(serializeExpr(child.get()));
    return arr;
}
```

- [ ] **Step 2: Implement serializeExpr**

```cpp
nlohmann::json IRSerializer::serializeExpr(const IAST * node)
{
    if (!node) return nullptr;

    if (auto * lit = node->as<ASTLiteral>()) {
        json j = {{"span", span0()}};
        switch (lit->kind) {
            case ASTLiteral::Kind::Number: return j.merge_patch({{"type","Literal"},{"kind","number"},{"value",lit->value}});
            case ASTLiteral::Kind::String: return j.merge_patch({{"type","Literal"},{"kind","string"},{"value",lit->value}});
            case ASTLiteral::Kind::Null:   return j.merge_patch({{"type","Literal"},{"kind","null"}});
        }
    }

    if (auto * ident = node->as<ASTIdentifier>()) {
        const auto & n = ident->name();
        auto dot = n.find('.');
        if (dot != std::string::npos)
            return {{"type","Column"},{"table",n.substr(0,dot)},{"name",n.substr(dot+1)},{"span",span0()}};
        return {{"type","Column"},{"name",n},{"span",span0()}};
    }

    if (auto * fn = node->as<ASTFunction>()) {
        return {{"type","Function"},{"name",fn->name},
                {"args", fn->arguments ? serializeExprList(fn->arguments) : json::array()},
                {"span",span0()}};
    }

    if (auto * ops = node->as<ASTExpressionOpsLite>()) {
        // NOTE: Before finalizing, read ExpressionOpsLiteParsers.cpp to confirm:
        // 1. Exact op string values for CASE, CAST, ::, STAR, TABLE_STAR, SUBQUERY, ARRAY, TUPLE,
        //    IS NULL, IS NOT NULL, BETWEEN, NOT BETWEEN, LIKE, NOT LIKE, ILIKE, NOT ILIKE, IN, NOT IN, ->
        // 2. For ARRAY/TUPLE: verify ops->left is an ASTExpressionList whose children are the elements
        // 3. For IN: verify ops->right is an ASTExpressionList of the list items
        if (ops->is_unary)
            return {{"type","UnaryOp"},{"op",ops->op},{"expr",serializeExpr(ops->left)},{"span",span0()}};
        if (ops->op == "STAR")
            return {{"type","Star"},{"span",span0()}};
        if (ops->op == "TABLE_STAR")
            return {{"type","TableStar"},{"table",ops->left?ops->left->getID('_'):""},{"span",span0()}};
        if (ops->op == "SUBQUERY")
            return {{"type","Subquery"},{"query",serializeNode(ops->left)},{"span",span0()}};
        if (ops->op == "ARRAY")
            return {{"type","Array"},{"elements",ops->left?serializeExprList(ops->left):json::array()},{"span",span0()}};
        if (ops->op == "TUPLE")
            return {{"type","Tuple"},{"elements",ops->left?serializeExprList(ops->left):json::array()},{"span",span0()}};
        if (ops->op == "->" )
            return {{"type","Lambda"},{"params",ops->left?serializeExprList(ops->left):json::array()},
                    {"body",serializeExpr(ops->right)},{"span",span0()}};
        if (ops->op == "CAST" || ops->op == "::")
            return {{"type","Cast"},{"expr",serializeExpr(ops->left)},
                    {"cast_type",ops->right?ops->right->getID('_'):""},{"span",span0()}};
        if (ops->op == "IS NULL" || ops->op == "IS NOT NULL")
            return {{"type","IsNull"},{"negated",ops->op=="IS NOT NULL"},
                    {"expr",serializeExpr(ops->left)},{"span",span0()}};
        if (ops->op == "IN" || ops->op == "NOT IN")
            // ops->right holds an ASTExpressionList — use serializeExprList to emit an array
            return {{"type","In"},{"negated",ops->op=="NOT IN"},
                    {"expr",serializeExpr(ops->left)},
                    {"list",ops->right?serializeExprList(ops->right):json::array()},{"span",span0()}};
        if (ops->op == "BETWEEN" || ops->op == "NOT BETWEEN") {
            json j = {{"type","Between"},{"negated",ops->op=="NOT BETWEEN"},
                      {"expr",serializeExpr(ops->left)},{"span",span0()}};
            if (ops->right && !ops->right->children.empty()) {
                const auto & ch = ops->right->children;
                if (ch.size() >= 2) { j["low"] = serializeExpr(ch[0].get()); j["high"] = serializeExpr(ch[1].get()); }
            }
            return j;
        }
        if (ops->op == "LIKE" || ops->op == "NOT LIKE" || ops->op == "ILIKE" || ops->op == "NOT ILIKE") {
            bool neg   = ops->op.find("NOT")   != std::string::npos;
            bool ilike = ops->op.find("ILIKE") != std::string::npos;
            return {{"type","Like"},{"negated",neg},{"ilike",ilike},
                    {"expr",serializeExpr(ops->left)},{"pattern",serializeExpr(ops->right)},{"span",span0()}};
        }
        if (ops->op == "CASE")
            return {{"type","Function"},{"name","CASE"},
                    {"args",ops->left?serializeExprList(ops->left):json::array()},{"span",span0()}};
        // Default: BinaryOp
        return {{"type","BinaryOp"},{"op",ops->op},
                {"left",serializeExpr(ops->left)},{"right",serializeExpr(ops->right)},{"span",span0()}};
    }

    if (auto * list = node->as<ASTExpressionList>()) {
        json arr = json::array();
        for (const auto & ch : list->children) arr.push_back(serializeExpr(ch.get()));
        return arr;
    }

    // Inline subquery
    if (node->as<ASTSelectRichQuery>() || node->as<ASTSelectSetLite>())
        return {{"type","Subquery"},{"query",serializeNode(node)},{"span",span0()}};

    // Fallback
    return {{"type","Raw"},{"id",node->getID('_')},{"span",span0()}};
}
```

- [ ] **Step 3: Build**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build //ir:ir_json
```

- [ ] **Step 4: Commit**

```bash
git add ir/IRSerializer.cpp
git commit -m "feat: implement IRSerializer expression node serialization"
```

### Task 11: Implement IRSerializer — Table Exprs, Order By, Window, Limit

**Files:**
- Modify: `ir/IRSerializer.cpp`

- [ ] **Step 1: Implement serializeTableExpr**

```cpp
nlohmann::json IRSerializer::serializeTableExpr(const IAST * node)
{
    if (!node) return nullptr;

    if (auto * join = node->as<ASTJoinLite>()) {
        static const char * types[]     = {"INNER","LEFT","RIGHT","FULL","CROSS","PASTE"};
        static const char * localities[] = {"","GLOBAL","LOCAL"};
        json j = {{"type","Join"},{"span",span0()}};
        j["kind"] = types[static_cast<int>(join->join_type)];
        if (join->join_locality != ASTJoinLite::JoinLocality::Unspecified)
            j["locality"] = localities[static_cast<int>(join->join_locality)];
        if (!join->strictness.empty()) j["strictness"] = join->strictness;
        j["left"]  = serializeTableExpr(join->left);
        j["right"] = serializeTableExpr(join->right);
        if (join->on_expression)  j["on"]    = serializeExpr(join->on_expression);
        if (join->using_columns)  j["using"] = serializeExprList(join->using_columns);
        return j;
    }

    if (auto * te = node->as<ASTTableExprLite>()) {
        json j;
        switch (te->kind) {
            case ASTTableExprLite::Kind::TableIdentifier:
                j = {{"type","Table"},{"span",span0()}};
                if (te->table_identifier) {
                    j["name"] = te->table_identifier->table;
                    if (!te->table_identifier->database.empty())
                        j["database"] = te->table_identifier->database;
                }
                break;
            case ASTTableExprLite::Kind::TableFunction:
                j = {{"type","TableFunction"},{"span",span0()}};
                if (te->table_function) {
                    // Emit function as a nested Function node per the IR schema
                    j["function"] = {
                        {"type","Function"},
                        {"name", te->table_function->name},
                        {"args", te->table_function->arguments
                            ? serializeExprList(te->table_function->arguments) : json::array()},
                        {"span", span0()},
                    };
                }
                break;
            case ASTTableExprLite::Kind::Subquery:
                j = {{"type","SubquerySource"},{"span",span0()}};
                if (te->subquery) j["query"] = serializeNode(te->subquery);
                break;
        }
        if (te->alias) j["alias"] = te->alias->alias;
        return j;
    }

    return {{"type","Raw"},{"id",node->getID('_')},{"span",span0()}};
}
```

- [ ] **Step 2: Implement serializeOrderByList**

```cpp
nlohmann::json IRSerializer::serializeOrderByList(const IAST * node)
{
    json arr = json::array();
    if (!node) return arr;
    for (const auto & child : node->children) {
        auto * elem = child ? child->as<ASTOrderByElementLite>() : nullptr;
        if (!elem) continue;
        json e = {{"span",span0()}};
        e["expr"]      = serializeExpr(elem->expression);
        e["direction"] = (elem->direction == ASTOrderByElementLite::Direction::Asc) ? "ASC" : "DESC";
        if (elem->nulls_order == ASTOrderByElementLite::NullsOrder::First) e["nulls"] = "FIRST";
        if (elem->nulls_order == ASTOrderByElementLite::NullsOrder::Last)  e["nulls"] = "LAST";
        if (!elem->collate_name.empty()) e["collate"] = elem->collate_name;
        if (elem->with_fill) {
            json fill;
            if (elem->fill_from)      fill["from"]      = serializeExpr(elem->fill_from);
            if (elem->fill_to)        fill["to"]        = serializeExpr(elem->fill_to);
            if (elem->fill_step)      fill["step"]      = serializeExpr(elem->fill_step);
            if (elem->fill_staleness) fill["staleness"] = serializeExpr(elem->fill_staleness);
            e["with_fill"] = std::move(fill);
        }
        if (elem->interpolate && elem->interpolate_expressions)
            e["interpolate"] = serializeExprList(elem->interpolate_expressions);
        arr.push_back(std::move(e));
    }
    return arr;
}
```

- [ ] **Step 3: Implement serializeWindowList**

```cpp
nlohmann::json IRSerializer::serializeWindowList(const IAST * node)
{
    json arr = json::array();
    if (!node) return arr;
    for (const auto & child : node->children) {
        auto * wd = child ? child->as<ASTWindowDefinitionLite>() : nullptr;
        if (!wd) continue;
        json w = {{"span",span0()},{"is_reference",wd->is_reference},{"body",wd->body}};
        if (!wd->name.empty()) w["name"] = wd->name;
        arr.push_back(std::move(w));
    }
    return arr;
}
```

- [ ] **Step 4: Implement serializeLimitClause and serializeLimitByClause**

```cpp
nlohmann::json IRSerializer::serializeLimitClause(const IAST * node)
{
    const auto * lim = node->as<ASTLimitLite>();
    if (!lim) return nullptr;
    json j = {{"type","Limit"},{"span",span0()}};
    if (lim->limit_expression)
        j["count"] = serializeExpr(lim->limit_expression);
    else if (!lim->limit.empty())
        j["count"] = {{"type","Literal"},{"kind","number"},{"value",lim->limit},{"span",span0()}};
    if (lim->offset_present) {
        if (lim->offset_expression)
            j["offset"] = serializeExpr(lim->offset_expression);
        else if (!lim->offset.empty())
            j["offset"] = {{"type","Literal"},{"kind","number"},{"value",lim->offset},{"span",span0()}};
    }
    if (lim->with_ties) j["with_ties"] = true;
    return j;
}

nlohmann::json IRSerializer::serializeLimitByClause(const IAST * node)
{
    const auto * lb = node->as<ASTLimitByLite>();
    if (!lb) return nullptr;
    json j = {{"type","LimitBy"},{"span",span0()}};
    if (lb->limit_expression)
        j["count"] = serializeExpr(lb->limit_expression);
    else if (!lb->limit.empty())
        j["count"] = {{"type","Literal"},{"kind","number"},{"value",lb->limit},{"span",span0()}};
    if (lb->offset_present) {
        if (lb->offset_expression)
            j["offset"] = serializeExpr(lb->offset_expression);
        else if (!lb->offset.empty())
            j["offset"] = {{"type","Literal"},{"kind","number"},{"value",lb->offset},{"span",span0()}};
    }
    if (lb->by_all)
        j["by"] = {{"type","Star"},{"span",span0()}};
    else if (lb->by_expressions)
        j["by"] = serializeExprList(lb->by_expressions);
    return j;
}
```

- [ ] **Step 5: Build**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build //ir:ir_json
```

- [ ] **Step 6: Commit**

```bash
git add ir/IRSerializer.cpp
git commit -m "feat: implement IRSerializer table/order/window/limit serialization"
```

### Task 12: Create ir_dump Binary and Smoke Test

**Files:**
- Create: `examples/bootstrap/ir_dump.cc`
- Modify: `examples/bootstrap/BUILD.bazel`

- [ ] **Step 1: Create `examples/bootstrap/ir_dump.cc`**

The `parse()` signature is: `bool parse(Pos & pos, ASTPtr & node, Expected & expected)` (from `IParser.h`). `Expected` is declared in `IParser.h`.

```cpp
#include "ir/IRSerializer.h"
#include "ported_clickhouse/parsers/IParser.h"
#include "ported_clickhouse/parsers/Lexer.h"
#include "ported_clickhouse/parsers/ParserSelectRichQuery.h"
#include "ported_clickhouse/parsers/TokenIterator.h"

#include <iostream>
#include <sstream>
#include <string>

int main()
{
    std::ostringstream buf;
    buf << std::cin.rdbuf();
    const std::string sql = buf.str();

    if (sql.empty()) {
        std::cerr << "ir_dump: empty input\n";
        return 1;
    }

    // Lexer is a stateful iterator — use Tokens + IParser::Pos (confirmed from select_rich_smoke.cc)
    DB::Tokens tokens(sql.data(), sql.data() + sql.size(), 0, true);
    DB::IParser::Pos pos(tokens, 4000, 4000);
    DB::Expected expected;

    DB::ParserSelectRichQuery parser;
    DB::ASTPtr ast;
    if (!parser.parse(pos, ast, expected)) {
        std::cerr << "ir_dump: parse error\n";
        return 1;
    }

    try {
        auto ir = clickshack::IRSerializer::serializeQuery(ast.get());
        std::cout << ir.dump(2) << "\n";
    } catch (const std::exception & e) {
        std::cerr << "ir_dump: serialization error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

> **Note on Lexer/Tokens API:** Check `Lexer.h` for exact method names (`tokenize()` may be named differently). Also verify `DB::Tokens` type and `TokenIterator` constructor. Look at `select_rich_smoke.cc` for the exact usage pattern — it already does this correctly.

- [ ] **Step 2: Add ir_dump to examples/bootstrap/BUILD.bazel**

After the `parser_workload_summary` target:

```python
cc_binary(
    name = "ir_dump",
    srcs = ["ir_dump.cc"],
    deps = [
        "//ir:ir_json",
        "//ported_clickhouse:parser_select_rich_stack",
    ],
    visibility = ["//visibility:public"],
)
```

- [ ] **Step 3: Build**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build //examples/bootstrap:ir_dump
```

- [ ] **Step 4: Smoke — simple select**

```bash
echo "SELECT 1 + 2" | \
  bazel --batch --output_user_root=/tmp/bazel-root run //examples/bootstrap:ir_dump
```

Expected: JSON output with `"ir_version": "1"` and `"type": "Select"`.

- [ ] **Step 5: Smoke — full select**

```bash
echo "SELECT id, name FROM users WHERE active = 1 ORDER BY name LIMIT 10" | \
  bazel --batch --output_user_root=/tmp/bazel-root run //examples/bootstrap:ir_dump
```

Expected: JSON with `from`, `where`, `order_by`, `limit` fields present.

- [ ] **Step 6: Smoke — parse failure exits 1**

```bash
echo "THIS IS NOT SQL" | \
  bazel --batch --output_user_root=/tmp/bazel-root run //examples/bootstrap:ir_dump
echo "Exit: $?"
```

Expected: exit 1, error on stderr.

- [ ] **Step 7: Build all required targets**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build \
  //ported_clickhouse:parser_lib \
  //ir:ir_json \
  //examples/bootstrap:ir_dump \
  //examples/bootstrap:hello_clickshack
just run-ported-smokes
```

Expected: all pass.

- [ ] **Step 8: Commit**

```bash
git add examples/bootstrap/ir_dump.cc examples/bootstrap/BUILD.bazel
git commit -m "feat: add ir_dump binary (SQL stdin → JSON IR stdout)"
```

---

## Chunk 4: Python IR Layer

### Task 13: Python Package Setup

**Files:**
- Create: `pyproject.toml`
- Create: `clickshack/__init__.py`
- Create: `clickshack/ir/__init__.py`
- Create: `clickshack/linter/__init__.py`

- [ ] **Step 1: Create `pyproject.toml`**

```toml
[build-system]
requires = ["setuptools>=70"]
build-backend = "setuptools.backends.legacy:build"

[project]
name = "clickshack"
version = "0.1.0"
requires-python = ">=3.11"
dependencies = [
    "pydantic>=2.0",
    "sqlglot>=25.0",
]

[project.optional-dependencies]
dev = ["pytest>=8.0"]

[tool.setuptools.packages.find]
where = ["."]
include = ["clickshack*"]
```

- [ ] **Step 2: Create empty `__init__.py` files**

Create empty files at:
- `clickshack/__init__.py`
- `clickshack/ir/__init__.py`
- `clickshack/linter/__init__.py`

- [ ] **Step 3: Install and verify**

```bash
# From repo root:
pip install -e ".[dev]"
python -c "import clickshack; print('OK')"
```

Expected: `OK`.

- [ ] **Step 4: Commit**

```bash
git add pyproject.toml clickshack/
git commit -m "feat: add Python package skeleton (clickshack)"
```

### Task 14: Pydantic v2 IR Models

**Files:**
- Create: `clickshack/ir/models.py`
- Create: `tests/test_ir_models.py`

- [ ] **Step 1: Write failing tests**

Create `tests/test_ir_models.py`:

```python
import pytest
from clickshack.ir.models import IrEnvelope, SelectNode, LiteralNode, ColumnNode, parse_node


def test_envelope_parses_simple_select():
    raw = {
        "ir_version": "1",
        "query": {
            "type": "Select",
            "span": {"start": 0, "end": 10},
            "projections": [
                {"type": "Literal", "kind": "number", "value": "1",
                 "span": {"start": 0, "end": 1}}
            ]
        }
    }
    envelope = IrEnvelope.model_validate(raw)
    assert envelope.ir_version == "1"
    assert envelope.query.type == "Select"
    assert isinstance(envelope.query, SelectNode)


def test_literal_null():
    node = {"type": "Literal", "kind": "null", "span": {"start": 0, "end": 4}}
    lit = parse_node(node)
    assert lit.kind == "null"
    assert lit.value is None


def test_select_with_where():
    raw = {
        "ir_version": "1",
        "query": {
            "type": "Select",
            "span": {"start": 0, "end": 50},
            "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
            "from": {"type": "Table", "name": "users", "span": {"start": 14, "end": 19}},
            "where": {
                "type": "BinaryOp", "op": "=",
                "left": {"type": "Column", "name": "id", "span": {"start": 26, "end": 28}},
                "right": {"type": "Literal", "kind": "number", "value": "1",
                          "span": {"start": 31, "end": 32}},
                "span": {"start": 26, "end": 32}
            }
        }
    }
    env = IrEnvelope.model_validate(raw)
    assert env.query.type == "Select"
    assert env.query.where is not None
    assert env.query.where.type == "BinaryOp"


def test_with_node():
    raw = {
        "ir_version": "1",
        "query": {
            "type": "With",
            "span": {"start": 0, "end": 80},
            "recursive": False,
            "ctes": [{"name": "cte1", "query": {
                "type": "Select", "span": {"start": 14, "end": 30},
                "projections": [{"type": "Star", "span": {"start": 21, "end": 22}}]
            }}],
            "body": {
                "type": "Select", "span": {"start": 34, "end": 60},
                "projections": [{"type": "Star", "span": {"start": 41, "end": 42}}],
                "from": {"type": "Table", "name": "cte1", "span": {"start": 48, "end": 52}}
            }
        }
    }
    env = IrEnvelope.model_validate(raw)
    assert env.query.type == "With"
    assert len(env.query.ctes) == 1
    assert env.query.ctes[0].name == "cte1"
```

- [ ] **Step 2: Run to verify failure**

```bash
# From repo root:
pytest tests/test_ir_models.py -v
```

Expected: `ImportError` — `clickshack.ir.models` does not exist yet.

- [ ] **Step 3: Create `clickshack/ir/models.py`**

```python
from __future__ import annotations

from typing import Annotated, Any, Literal, Optional, Union

from pydantic import BaseModel, Field


class Span(BaseModel):
    start: int
    end: int


# ── Expression nodes ──────────────────────────────────────────────────────────

class LiteralNode(BaseModel):
    type: Literal["Literal"]
    span: Span
    kind: Literal["number", "string", "null"]
    value: Optional[str] = None


class ColumnNode(BaseModel):
    type: Literal["Column"]
    span: Span
    name: str
    table: Optional[str] = None


class StarNode(BaseModel):
    type: Literal["Star"]
    span: Span


class TableStarNode(BaseModel):
    type: Literal["TableStar"]
    span: Span
    table: str


class FunctionNode(BaseModel):
    type: Literal["Function"]
    span: Span
    name: str
    args: list[IrExpr] = Field(default_factory=list)
    over: Optional[WindowSpecInline] = None


class BinaryOpNode(BaseModel):
    type: Literal["BinaryOp"]
    span: Span
    op: str
    left: IrExpr
    right: IrExpr


class UnaryOpNode(BaseModel):
    type: Literal["UnaryOp"]
    span: Span
    op: str
    expr: IrExpr


class CaseBranch(BaseModel):
    when: IrExpr
    then: IrExpr


class CaseNode(BaseModel):
    type: Literal["Case"]
    span: Span
    operand: Optional[IrExpr] = None
    branches: list[CaseBranch] = Field(default_factory=list)
    else_expr: Optional[IrExpr] = Field(None, alias="else")
    model_config = {"populate_by_name": True}


class CastNode(BaseModel):
    type: Literal["Cast"]
    span: Span
    expr: IrExpr
    cast_type: str


class InNode(BaseModel):
    type: Literal["In"]
    span: Span
    negated: bool
    expr: IrExpr
    list: Optional[list[IrExpr]] = None
    subquery: Optional[SubqueryNode] = None


class BetweenNode(BaseModel):
    type: Literal["Between"]
    span: Span
    negated: bool
    expr: IrExpr
    low: IrExpr
    high: IrExpr


class LikeNode(BaseModel):
    type: Literal["Like"]
    span: Span
    negated: bool
    ilike: bool
    expr: IrExpr
    pattern: IrExpr
    escape: Optional[IrExpr] = None


class IsNullNode(BaseModel):
    type: Literal["IsNull"]
    span: Span
    negated: bool
    expr: IrExpr


class LambdaNode(BaseModel):
    type: Literal["Lambda"]
    span: Span
    params: list[IrExpr]
    body: IrExpr


class ArrayNode(BaseModel):
    type: Literal["Array"]
    span: Span
    elements: list[IrExpr]


class TupleNode(BaseModel):
    type: Literal["Tuple"]
    span: Span
    elements: list[IrExpr]


class SubqueryNode(BaseModel):
    type: Literal["Subquery"]
    span: Span
    query: IrQuery


class RawNode(BaseModel):
    type: Literal["Raw"]
    span: Span
    id: Optional[str] = None


# ── Table expression nodes ────────────────────────────────────────────────────

class TableNode(BaseModel):
    type: Literal["Table"]
    span: Span
    name: str
    database: Optional[str] = None
    alias: Optional[str] = None
    final: Optional[bool] = None
    sample: Optional[IrExpr] = None
    sample_offset: Optional[IrExpr] = None


class TableFunctionNode(BaseModel):
    type: Literal["TableFunction"]
    span: Span
    function: FunctionNode  # nested Function node (name + args)
    alias: Optional[str] = None


class SubquerySourceNode(BaseModel):
    type: Literal["SubquerySource"]
    span: Span
    query: IrQuery
    alias: Optional[str] = None


class JoinNode(BaseModel):
    type: Literal["Join"]
    span: Span
    kind: str
    locality: Optional[str] = None
    strictness: Optional[str] = None
    left: IrTableExpr
    right: IrTableExpr
    on: Optional[IrExpr] = None
    using: Optional[list[IrExpr]] = None


# ── Inline window spec (on Function.over) ────────────────────────────────────

class WindowSpecInline(BaseModel):
    name: Optional[str] = None
    is_reference: bool = False
    body: Optional[str] = None


# ── Clause types ─────────────────────────────────────────────────────────────

class LimitNode(BaseModel):
    type: Literal["Limit"]
    span: Span
    count: Optional[IrExpr] = None
    offset: Optional[IrExpr] = None
    with_ties: bool = False


class LimitByNode(BaseModel):
    type: Literal["LimitBy"]
    span: Span
    count: Optional[IrExpr] = None
    offset: Optional[IrExpr] = None
    by: Optional[Any] = None  # StarNode or list[IrExpr]


class OrderByElement(BaseModel):
    span: Span
    expr: IrExpr
    direction: Literal["ASC", "DESC"] = "ASC"
    nulls: Optional[Literal["FIRST", "LAST"]] = None
    collate: Optional[str] = None
    with_fill: Optional[dict[str, Any]] = None
    interpolate: Optional[list[IrExpr]] = None


class WindowDefNode(BaseModel):
    span: Span
    name: Optional[str] = None
    is_reference: bool = False
    body: Optional[str] = None


class CteDef(BaseModel):
    name: str
    query: IrQuery


# ── Root nodes ────────────────────────────────────────────────────────────────

class ArrayJoinInline(BaseModel):
    """Inline array join on a Select node — not a table expression."""
    left: bool = False   # True = LEFT ARRAY JOIN
    exprs: list[IrExpr] = Field(default_factory=list)


class SelectNode(BaseModel):
    type: Literal["Select"]
    span: Span
    distinct: bool = False
    all: bool = False
    distinct_on: Optional[list[IrExpr]] = None
    top: Optional[dict[str, Any]] = None
    projections: list[IrExpr] = Field(default_factory=list)
    from_: Optional[IrTableExpr] = Field(None, alias="from")
    array_join: Optional[ArrayJoinInline] = None
    prewhere: Optional[IrExpr] = None
    where: Optional[IrExpr] = None
    group_by: Optional[dict[str, Any]] = None
    having: Optional[IrExpr] = None
    window: Optional[list[WindowDefNode]] = None
    qualify: Optional[IrExpr] = None
    order_by: Optional[Any] = None  # list[OrderByElement] or {"all": True}
    limit: Optional[LimitNode] = None
    limit_by: Optional[LimitByNode] = None
    settings: Optional[list[IrExpr]] = None
    format: Optional[str] = None
    model_config = {"populate_by_name": True}


class SelectUnionNode(BaseModel):
    type: Literal["SelectUnion"]
    span: Span
    op: str
    quantifier: str = ""
    left: IrQuery
    right: IrQuery


class WithNode(BaseModel):
    type: Literal["With"]
    span: Span
    recursive: bool = False
    ctes: list[CteDef] = Field(default_factory=list)
    body: IrQuery


# ── Discriminated unions ──────────────────────────────────────────────────────

IrExpr = Annotated[
    Union[
        LiteralNode, ColumnNode, StarNode, TableStarNode,
        FunctionNode, BinaryOpNode, UnaryOpNode, CaseNode,
        CastNode, InNode, BetweenNode, LikeNode, IsNullNode,
        LambdaNode, ArrayNode, TupleNode, SubqueryNode, RawNode,
    ],
    Field(discriminator="type"),
]

IrTableExpr = Annotated[
    Union[TableNode, TableFunctionNode, SubquerySourceNode, JoinNode],
    Field(discriminator="type"),
]

IrQuery = Annotated[
    Union[WithNode, SelectUnionNode, SelectNode],
    Field(discriminator="type"),
]

# Rebuild all models that contain forward references.
# CaseBranch must come before CaseNode since CaseNode references it.
for _cls in [
    CaseBranch, FunctionNode, SubqueryNode, SubquerySourceNode, JoinNode,
    ArrayJoinInline, CaseNode, InNode, BetweenNode, LikeNode, IsNullNode,
    LambdaNode, ArrayNode, TupleNode, BinaryOpNode, UnaryOpNode,
    CastNode, WithNode, SelectUnionNode, SelectNode,
    LimitNode, LimitByNode, OrderByElement, CteDef, TableNode,
    TableFunctionNode,
]:
    _cls.model_rebuild()


def parse_node(data: dict) -> Any:
    """Parse a single IR expression node dict into the appropriate typed model."""
    from pydantic import TypeAdapter
    return TypeAdapter(IrExpr).validate_python(data)


# ── Envelope ──────────────────────────────────────────────────────────────────

class IrEnvelope(BaseModel):
    ir_version: str
    query: IrQuery
```

- [ ] **Step 4: Run tests**

```bash
# From repo root:
pytest tests/test_ir_models.py -v
```

Expected: all 4 tests pass.

- [ ] **Step 5: Commit**

```bash
git add clickshack/ir/models.py tests/test_ir_models.py
git commit -m "feat: add Pydantic v2 IR models for JSON IR schema"
```

### Task 15: SQLGlot Adapter

**Files:**
- Create: `clickshack/ir/adapter.py`
- Create: `tests/test_adapter.py`

- [ ] **Step 1: Write failing tests**

Create `tests/test_adapter.py`:

```python
import pytest
import sqlglot.expressions as exp

from clickshack.ir.models import IrEnvelope
from clickshack.ir.adapter import to_sqlglot, expr_to_sqlglot


def _ir(raw: dict):
    return IrEnvelope.model_validate(raw).query


def test_simple_select_star():
    query = _ir({"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 14},
        "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
        "from": {"type": "Table", "name": "t", "span": {"start": 14, "end": 15}},
    }})
    result = to_sqlglot(query)
    assert isinstance(result, exp.Select)
    assert len(result.expressions) == 1
    assert isinstance(result.expressions[0], exp.Star)


def test_select_literal():
    query = _ir({"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 8},
        "projections": [{"type": "Literal", "kind": "number", "value": "42",
                         "span": {"start": 7, "end": 9}}]
    }})
    result = to_sqlglot(query)
    assert isinstance(result, exp.Select)


def test_select_column_with_table():
    query = _ir({"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 20},
        "projections": [{"type": "Column", "name": "id", "table": "u",
                         "span": {"start": 7, "end": 11}}],
        "from": {"type": "Table", "name": "users", "alias": "u",
                 "span": {"start": 15, "end": 20}}
    }})
    result = to_sqlglot(query)
    assert isinstance(result, exp.Select)


def test_unsupported_raw_node_raises():
    from clickshack.ir.models import RawNode, Span
    node = RawNode(type="Raw", span=Span(start=0, end=0), id="unknown")
    with pytest.raises(NotImplementedError):
        expr_to_sqlglot(node)


def test_union():
    query = _ir({"ir_version": "1", "query": {
        "type": "SelectUnion", "span": {"start": 0, "end": 40},
        "op": "UNION", "quantifier": "ALL",
        "left": {"type": "Select", "span": {"start": 0, "end": 15},
                 "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
                 "from": {"type": "Table", "name": "a", "span": {"start": 14, "end": 15}}},
        "right": {"type": "Select", "span": {"start": 25, "end": 40},
                  "projections": [{"type": "Star", "span": {"start": 32, "end": 33}}],
                  "from": {"type": "Table", "name": "b", "span": {"start": 39, "end": 40}}}
    }})
    result = to_sqlglot(query)
    assert isinstance(result, exp.Union)
```

- [ ] **Step 2: Run to verify failure**

```bash
pytest tests/test_adapter.py -v
```

Expected: `ImportError` — adapter doesn't exist yet.

- [ ] **Step 3: Create `clickshack/ir/adapter.py`**

```python
"""SQLGlot adapter: converts IR node tree → sqlglot AST."""
from __future__ import annotations

from typing import Union

import sqlglot.expressions as exp

from clickshack.ir.models import (
    ArrayNode, BetweenNode, BinaryOpNode, CaseNode, CastNode,
    ColumnNode, FunctionNode, InNode, IrExpr, IrQuery, IrTableExpr,
    IsNullNode, JoinNode, LambdaNode, LikeNode, LiteralNode,
    RawNode, SelectNode, SelectUnionNode, StarNode, SubqueryNode,
    SubquerySourceNode, TableFunctionNode, TableNode, TableStarNode,
    TupleNode, UnaryOpNode, WithNode,
)


def to_sqlglot(query: IrQuery) -> exp.Expression:
    """Convert an IR root node to a sqlglot Expression."""
    if isinstance(query, WithNode):
        return _with_to_sqlglot(query)
    if isinstance(query, SelectUnionNode):
        return _union_to_sqlglot(query)
    if isinstance(query, SelectNode):
        return _select_to_sqlglot(query)
    raise NotImplementedError(f"to_sqlglot: unsupported root type {type(query).__name__}")


def expr_to_sqlglot(node: IrExpr) -> exp.Expression:
    """Convert an IR expression node to a sqlglot Expression."""
    if isinstance(node, LiteralNode):
        if node.kind == "null":
            return exp.Null()
        if node.kind == "number":
            return exp.Literal.number(node.value or "0")
        return exp.Literal.string(node.value or "")

    if isinstance(node, ColumnNode):
        col = exp.Column(this=exp.Identifier(this=node.name))
        if node.table:
            col = exp.Column(
                this=exp.Identifier(this=node.name),
                table=exp.Identifier(this=node.table),
            )
        return col

    if isinstance(node, StarNode):
        return exp.Star()

    if isinstance(node, TableStarNode):
        return exp.Column(this=exp.Star(), table=exp.Identifier(this=node.table))

    if isinstance(node, FunctionNode):
        return exp.Anonymous(
            this=node.name,
            expressions=[expr_to_sqlglot(a) for a in node.args],
        )

    if isinstance(node, BinaryOpNode):
        left = expr_to_sqlglot(node.left)
        right = expr_to_sqlglot(node.right)
        _op_map = {
            "=": exp.EQ, "!=": exp.NEQ, "<>": exp.NEQ,
            "<": exp.LT, "<=": exp.LTE, ">": exp.GT, ">=": exp.GTE,
            "+": exp.Add, "-": exp.Sub, "*": exp.Mul, "/": exp.Div,
            "AND": exp.And, "OR": exp.Or,
            "||": exp.DPipe,
        }
        cls = _op_map.get(node.op.upper())
        if cls:
            return cls(this=left, expression=right)
        return exp.Anonymous(this=f"_op_{node.op}", expressions=[left, right])

    if isinstance(node, UnaryOpNode):
        inner = expr_to_sqlglot(node.expr)
        if node.op.upper() == "NOT":
            return exp.Not(this=inner)
        if node.op == "-":
            return exp.Neg(this=inner)
        return exp.Anonymous(this=f"_unary_{node.op}", expressions=[inner])

    if isinstance(node, IsNullNode):
        inner = expr_to_sqlglot(node.expr)
        is_null = exp.Is(this=inner, expression=exp.Null())
        return exp.Not(this=is_null) if node.negated else is_null

    if isinstance(node, InNode):
        inner = expr_to_sqlglot(node.expr)
        if node.list:
            result = exp.In(this=inner, expressions=[expr_to_sqlglot(x) for x in node.list])
        elif node.subquery:
            result = exp.In(this=inner, query=to_sqlglot(node.subquery.query))
        else:
            result = exp.In(this=inner, expressions=[])
        return exp.Not(this=result) if node.negated else result

    if isinstance(node, BetweenNode):
        result = exp.Between(
            this=expr_to_sqlglot(node.expr),
            low=expr_to_sqlglot(node.low),
            high=expr_to_sqlglot(node.high),
        )
        return exp.Not(this=result) if node.negated else result

    if isinstance(node, LikeNode):
        this = expr_to_sqlglot(node.expr)
        pattern = expr_to_sqlglot(node.pattern)
        cls = exp.ILike if node.ilike else exp.Like
        result = cls(this=this, expression=pattern)
        return exp.Not(this=result) if node.negated else result

    if isinstance(node, SubqueryNode):
        return exp.Subquery(this=to_sqlglot(node.query))

    if isinstance(node, ArrayNode):
        return exp.Array(expressions=[expr_to_sqlglot(e) for e in node.elements])

    if isinstance(node, TupleNode):
        return exp.Tuple(expressions=[expr_to_sqlglot(e) for e in node.elements])

    if isinstance(node, CastNode):
        return exp.Cast(
            this=expr_to_sqlglot(node.expr),
            to=exp.DataType.build(node.cast_type),
        )

    if isinstance(node, LambdaNode):
        return exp.Lambda(
            this=expr_to_sqlglot(node.body),
            expressions=[expr_to_sqlglot(p) for p in node.params],
        )

    if isinstance(node, CaseNode):
        ifs = [
            exp.If(this=expr_to_sqlglot(b.when), true=expr_to_sqlglot(b.then))
            for b in node.branches
        ]
        default = expr_to_sqlglot(node.else_expr) if node.else_expr else None
        return exp.Case(
            this=expr_to_sqlglot(node.operand) if node.operand else None,
            ifs=ifs,
            default=default,
        )

    if isinstance(node, RawNode):
        raise NotImplementedError(
            f"expr_to_sqlglot: Raw node not supported (id={node.id!r})"
        )

    raise NotImplementedError(
        f"expr_to_sqlglot: unsupported node type {type(node).__name__}"
    )


# ── Private helpers ───────────────────────────────────────────────────────────

def _select_to_sqlglot(sel: SelectNode) -> exp.Select:
    projections = [expr_to_sqlglot(p) for p in sel.projections]
    result = exp.select(*projections)
    if sel.from_ is not None:
        result = result.from_(_table_to_sqlglot(sel.from_))
    if sel.where is not None:
        result = result.where(expr_to_sqlglot(sel.where))
    if sel.distinct:
        result = result.distinct()
    return result


def _table_to_sqlglot(te: IrTableExpr) -> exp.Expression:
    if isinstance(te, TableNode):
        if te.database:
            tbl = exp.Table(
                db=exp.Identifier(this=te.database),
                this=exp.Identifier(this=te.name),
            )
        else:
            tbl = exp.Table(this=exp.Identifier(this=te.name))
        if te.alias:
            return exp.Alias(this=tbl, alias=exp.Identifier(this=te.alias))
        return tbl

    if isinstance(te, SubquerySourceNode):
        sq = exp.Subquery(this=to_sqlglot(te.query))
        if te.alias:
            return exp.Alias(this=sq, alias=exp.Identifier(this=te.alias))
        return sq

    if isinstance(te, TableFunctionNode):
        return exp.Anonymous(
            this=te.function.name,
            expressions=[expr_to_sqlglot(a) for a in te.function.args],
        )

    if isinstance(te, JoinNode):
        raise NotImplementedError(
            "JoinNode: use _select_with_joins for join trees (not yet implemented)"
        )

    raise NotImplementedError(f"_table_to_sqlglot: unsupported {type(te).__name__}")


def _union_to_sqlglot(node: SelectUnionNode) -> exp.Expression:
    left = to_sqlglot(node.left)
    right = to_sqlglot(node.right)
    distinct = node.quantifier.upper() != "ALL"
    op = node.op.upper()
    if op == "UNION":
        return exp.Union(this=left, expression=right, distinct=distinct)
    if op == "INTERSECT":
        return exp.Intersect(this=left, expression=right, distinct=distinct)
    if op == "EXCEPT":
        return exp.Except(this=left, expression=right, distinct=distinct)
    raise NotImplementedError(f"_union_to_sqlglot: unknown op {node.op!r}")


def _with_to_sqlglot(node: WithNode) -> exp.Expression:
    body = to_sqlglot(node.body)
    ctes = [
        exp.CTE(
            this=to_sqlglot(cte.query),
            alias=exp.TableAlias(this=exp.Identifier(this=cte.name)),
        )
        for cte in node.ctes
    ]
    return exp.With(expressions=ctes, this=body)
```

- [ ] **Step 4: Run tests**

```bash
pytest tests/test_adapter.py -v
```

Expected: all 5 tests pass.

- [ ] **Step 5: Commit**

```bash
git add clickshack/ir/adapter.py tests/test_adapter.py
git commit -m "feat: add SQLGlot adapter (IR → sqlglot AST)"
```

### Task 16: Linter Base

**Files:**
- Create: `clickshack/linter/base.py`
- Create: `tests/test_linter.py`

- [ ] **Step 1: Write failing tests**

Create `tests/test_linter.py`:

```python
import pytest
from clickshack.ir.models import IrEnvelope, StarNode, Span
from clickshack.linter.base import LintRule, LintRunner, LintViolation


class NoStarRule:
    """Prohibit SELECT *"""
    def check(self, node) -> list[LintViolation]:
        if isinstance(node, StarNode):
            return [LintViolation(
                rule="no-star",
                message="SELECT * is not allowed",
                span=node.span,
            )]
        return []


def test_lint_violation_found():
    raw = {"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 14},
        "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
        "from": {"type": "Table", "name": "t", "span": {"start": 14, "end": 15}},
    }}
    query = IrEnvelope.model_validate(raw).query
    runner = LintRunner(rules=[NoStarRule()])
    violations = runner.run(query)
    assert len(violations) == 1
    assert violations[0].rule == "no-star"
    assert violations[0].span.start == 7


def test_lint_no_violations():
    raw = {"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 20},
        "projections": [{"type": "Column", "name": "id", "span": {"start": 7, "end": 9}}],
    }}
    query = IrEnvelope.model_validate(raw).query
    runner = LintRunner(rules=[NoStarRule()])
    violations = runner.run(query)
    assert violations == []


def test_lint_multiple_rules():
    class AlwaysViolates:
        def check(self, node) -> list[LintViolation]:
            if hasattr(node, "span"):
                return [LintViolation(rule="always", message="x", span=node.span)]
            return []

    raw = {"ir_version": "1", "query": {
        "type": "Select", "span": {"start": 0, "end": 8},
        "projections": [{"type": "Star", "span": {"start": 7, "end": 8}}],
    }}
    query = IrEnvelope.model_validate(raw).query
    runner = LintRunner(rules=[NoStarRule(), AlwaysViolates()])
    violations = runner.run(query)
    # NoStarRule fires once (on Star node), AlwaysViolates fires on every node with span
    assert any(v.rule == "no-star" for v in violations)
    assert any(v.rule == "always" for v in violations)
```

- [ ] **Step 2: Run to verify failure**

```bash
pytest tests/test_linter.py -v
```

Expected: `ImportError`.

- [ ] **Step 3: Create `clickshack/linter/base.py`**

```python
from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol, runtime_checkable

from pydantic import BaseModel

from clickshack.ir.models import IrQuery, Span


@dataclass
class LintViolation:
    rule: str       # rule identifier, e.g. "no-star"
    message: str    # human-readable description
    span: Span      # source location from IR node


@runtime_checkable
class LintRule(Protocol):
    def check(self, node: object) -> list[LintViolation]: ...


class LintRunner:
    def __init__(self, rules: list[LintRule]) -> None:
        self.rules = rules

    def run(self, root: IrQuery) -> list[LintViolation]:
        violations: list[LintViolation] = []
        self._visit(root, violations)
        return violations

    def _visit(self, node: object, acc: list[LintViolation]) -> None:
        if node is None:
            return
        for rule in self.rules:
            acc.extend(rule.check(node))
        # Recurse into Pydantic model fields
        if isinstance(node, BaseModel):
            for value in node.__dict__.values():
                self._recurse(value, acc)

    def _recurse(self, value: object, acc: list[LintViolation]) -> None:
        if value is None:
            return
        if isinstance(value, BaseModel):
            self._visit(value, acc)
        elif isinstance(value, list):
            for item in value:
                self._recurse(item, acc)
        elif isinstance(value, dict):
            for v in value.values():
                self._recurse(v, acc)
```

- [ ] **Step 4: Run tests**

```bash
pytest tests/test_linter.py -v
```

Expected: all 3 tests pass.

- [ ] **Step 5: Commit**

```bash
git add clickshack/linter/base.py tests/test_linter.py
git commit -m "feat: add linter base (LintRule protocol, LintRunner, LintViolation)"
```

### Task 17: Final Validation

- [ ] **Step 1: Run full pytest suite**

```bash
# From repo root:
pytest tests/ -v
```

Expected: all tests pass.

- [ ] **Step 2: Run all Bazel builds**

```bash
bazel --batch --output_user_root=/tmp/bazel-root build \
  //ported_clickhouse:parser_lib \
  //ir:ir_json \
  //examples/bootstrap:ir_dump \
  //examples/bootstrap:hello_clickshack
```

Expected: all build successfully.

- [ ] **Step 3: Run workload suite and smokes**

```bash
bash tools/parser_workload_suite.sh full \
  --manifest testdata/parser_workload/manifest.json \
  --source docs/select_dialect_missing_features.md \
  --signoff docs/parser_workload_reconciliation.md
just run-ported-smokes
```

Expected: all pass.

- [ ] **Step 4: End-to-end IR smoke**

```bash
echo "SELECT id, name FROM users WHERE active = 1 AND region IN ('us', 'eu') ORDER BY name ASC LIMIT 100" | \
  bazel --batch --output_user_root=/tmp/bazel-root run //examples/bootstrap:ir_dump | \
  python -c "
import sys, json
d = json.load(sys.stdin)
print('IR version:', d['ir_version'])
print('Root type:', d['query']['type'])
print('Has projections:', 'projections' in d['query'])
print('Has from:', 'from' in d['query'])
print('Has where:', 'where' in d['query'])
print('Has order_by:', 'order_by' in d['query'])
print('Has limit:', 'limit' in d['query'])
"
```

Expected: all five lines print `True` or the correct value.

- [ ] **Step 5: Final commit (if any cleanup needed)**

```bash
git add -A
git commit -m "chore: final cleanup and validation for SELECT dialect + IR v1"
```
