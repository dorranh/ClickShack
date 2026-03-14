# Design: Full SELECT Dialect + IR v1

**Date:** 2026-03-14
**Status:** Approved
**Goal:** Close all remaining SELECT dialect parser gaps and deliver a versioned JSON IR consumed by Python tooling for SQLGlot transpilation and custom linting.

---

## Context

Phases 1, 2, and 2.1 are complete. The ported ClickHouse SELECT parser is broad but the gap inventory doc (`docs/select_dialect_missing_features.md`) is stale — Phase 02.1 addressed many of the 22 listed items but the doc was not updated. The next milestone delivers:

1. A verified-complete SELECT dialect parser
2. A JSON IR v1 contract for downstream Python tooling

---

## Architecture

The work has two tracks behind a shared audit gate:

```
[AUDIT GATE]
  Fresh parser gap inventory: read C++ sources + run smoke suite
  Output: updated gap list with status (implemented / partial / missing)

[TRACK 1: Parser Gap Closure]      [TRACK 2: IR v1 JSON]
  Close remaining gaps               Design JSON schema
  in ported_clickhouse/parsers/      Implement C++ IRSerializer
  Add smoke fixtures per gap         (ir/ directory, nlohmann/json)
  Update manifest + scope docs       Add Python models + adapter

[INTEGRATION + VALIDATION]
  All existing smokes stay green
  IR JSON round-trip smoke
  Python pytest suite validates models + adapter
```

---

## Track 1: Parser Gap Closure

### Audit Process

Before writing any code, do a fresh read of the actual parser source files against the 22-item gap list in `docs/select_dialect_missing_features.md`. For each item, determine:

- **implemented** — fully handled and reflected in AST
- **partial** — token is consumed but not retained in AST, or only some forms work
- **missing** — not handled at all

Update `docs/select_dialect_missing_features.md` in-place by adding a `Status` column (`implemented | partial | missing`) to each of the 22 items. This updated document is the authoritative input to Track 1. Only items marked `partial` or `missing` get implementation work.

### Implementation Rules (carried from prior phases)

1. All parser changes stay in `ported_clickhouse/parsers/` (Apache-licensed ported sources)
2. Include rewrite style: `ported_clickhouse/...` paths only
3. Add narrow layered Bazel targets rather than bloating one giant target
4. Parser-only AST growth — no semantic/analyzer dependencies
5. Update `docs/ported_clickhouse_manifest.md` for every new or changed file
6. Keep `//examples/bootstrap:hello_clickshack` green throughout

### Validation Per Gap

For each closed gap:
- Add expected-success smoke case(s) covering the new form
- Add expected-failure smoke case for invalid variants where applicable
- Run `just run-ported-smokes` to confirm no regressions

---

## Track 2: IR v1 JSON

### Directory Structure

```
ir/                            # Original project code (not ported/upstream)
  IRSerializer.h
  IRSerializer.cpp
  BUILD.bazel                  # //ir:ir_json (cc_library)

clickshack/                    # Python package (original project code)
  ir/
    models.py                  # Pydantic v2 models for IR schema
    adapter.py                 # IR node tree → SQLGlot AST
  linter/
    base.py                    # LintRule protocol + runner
tests/
  test_ir_models.py            # pytest: model parse + validation
  test_adapter.py              # pytest: IR → SQLGlot round-trip
```

### C++ IRSerializer

- Lives in `ir/` (not `ported_clickhouse/`) — original code, no Apache license obligations
- `//ir:ir_json` is a `cc_library` that depends on `//ported_clickhouse:parser_lib` and `nlohmann_json`
- `nlohmann_json` is added to `MODULE.bazel` via `bazel_dep(name = "nlohmann_json", version = "3.11.3")` (BCR module name: `nlohmann_json`)
- `//examples/bootstrap:ir_dump` is a `cc_binary` that depends on `//ir:ir_json` and is the integration point for Python tooling: accepts a single SQL string on stdin, writes one JSON object to stdout. On parse failure: exit 1, error message to stderr, no JSON output.

### JSON IR Schema

**Envelope:**
```json
{
  "ir_version": "1",
  "query": { ...root node... }
}
```

**Node pattern:** Every node carries a `type` string discriminator and type-specific fields. `span` (byte offsets into source) is present on every node to support precise linter error locations. Missing optional fields are omitted rather than set to null.

```json
{
  "type": "Select",
  "span": { "start": 0, "end": 142 },
  ...fields...
}
```

**Root node types:**

When a statement begins with `WITH ... SELECT`, the top-level `query` node is `With`, which wraps the body query. `Select` never carries a `with` field — the CTE context is always hoisted to a `With` root.

| Type | Fields |
|------|--------|
| `With` | `recursive` (bool), `ctes` (list of `{name: string, query: Select\|SelectUnion}`), `body` (`Select` or `SelectUnion`) |
| `Select` | `distinct?`, `top?`, `all?`, `projections`, `from?`, `prewhere?`, `where?`, `group_by?`, `having?`, `window?`, `qualify?`, `order_by?`, `limit?`, `settings?`, `format?` |
| `SelectUnion` | `op` (UNION/INTERSECT/EXCEPT), `quantifier` (ALL/DISTINCT/default), `left`, `right` |

**Expression node types:**

| Type | Key Fields |
|------|-----------|
| `Literal` | `kind` (string/number/null), `value` |
| `Column` | `table?`, `name` |
| `Star` | — |
| `TableStar` | `table` |
| `Function` | `name`, `args`, `over?` |
| `BinaryOp` | `op`, `left`, `right` |
| `UnaryOp` | `op`, `expr` |
| `Case` | `operand?`, `branches` [{`when`, `then`}], `else?` |
| `Cast` | `expr`, `type` |
| `In` | `expr`, `negated`, `list?` or `subquery?` |
| `Between` | `expr`, `negated`, `low`, `high` |
| `Like` | `expr`, `negated`, `ilike`, `pattern`, `escape?` |
| `IsNull` | `expr`, `negated` |
| `Lambda` | `params`, `body` |
| `Array` | `elements` |
| `Tuple` | `elements` |
| `Subquery` | `query` |

**Table expression node types:**

| Type | Key Fields |
|------|-----------|
| `Table` | `database?`, `name`, `alias?`, `final?`, `sample?` |
| `TableFunction` | `function`, `alias?` |
| `SubquerySource` | `query`, `alias?` |
| `Join` | `kind`, `locality?`, `strictness?`, `table`, `on?`, `using?` |
| `ArrayJoin` | `left`, `exprs` |

**Versioning policy:** `ir_version` bumps on any breaking schema change. Additive changes (new optional fields) do not bump. A changelog section in this doc tracks version history.

### Python Layer

**`clickshack/ir/models.py`** — Pydantic v2 models with a discriminated union on `type`. Every JSON node maps to a typed Python class. `model_validate_json()` is the entry point.

**`clickshack/ir/adapter.py`** — Walks the Pydantic model tree and constructs SQLGlot AST nodes. Designed for correctness over performance. Unsupported node types raise `NotImplementedError` with a clear message.

**`clickshack/linter/base.py`** — A `LintRule` protocol and shared types:
```python
@dataclass
class LintViolation:
    rule: str          # rule identifier
    message: str       # human-readable description
    span: Span         # source location from IR node

class LintRule(Protocol):
    def check(self, node: IrNode) -> list[LintViolation]: ...
```
A `LintRunner` accepts a list of rules and a root IR node, walks the tree, and returns all violations.

**Packaging:** A `pyproject.toml` at repo root declares `clickshack` as an installable package with `sqlglot` and `pydantic>=2` as dependencies. Tests run via `pip install -e . && pytest tests/`.

**Tests** use pytest. Tests cover:
- Model parse + round-trip for representative IR JSON fixtures
- Adapter correctness for key node types
- Linter runner with a trivial rule

---

## Validation Contract

Required at phase completion:

```bash
bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib
bazel --batch --output_user_root=/tmp/bazel-root build //ir:ir_json
bazel --batch --output_user_root=/tmp/bazel-root build //examples/bootstrap:ir_dump
bazel --batch --output_user_root=/tmp/bazel-root build //examples/bootstrap:hello_clickshack
just run-ported-smokes
bash tools/parser_workload_suite.sh full --manifest testdata/parser_workload/manifest.json --source docs/select_dialect_missing_features.md --signoff docs/parser_workload_reconciliation.md
# from repo root:
pip install -e . && pytest tests/
```

Success criteria:
- All required builds pass
- All expected-success queries parse and fully consume tokens
- All expected-failure queries return non-zero with deterministic failure
- `pytest tests/` passes with no failures
- IR JSON round-trip smoke produces valid, parseable output

---

## Non-Goals

- Semantic analysis, name/type resolution, query planning or execution
- Full MSSQL parser branches
- Non-SELECT statements
- Python transpilation performance optimization (correctness first)
- Public SDK hardening or external-consumer ergonomics

---

## IR Version History

| Version | Date | Changes |
|---------|------|---------|
| 1 | 2026-03-14 | Initial schema |
