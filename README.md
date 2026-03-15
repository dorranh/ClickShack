# ClickShack

A collection of ClickHouse query parsing and analysis tools built on a rickety, hollowed-out version of ClickHouse.

## Description

One of the challenges of building tooling around SQL is that some dialects can provide a large set of unique features beyond
standard ANSI SQL. ClickHouse is one of these, providing a rich SQL dialect that can prove to be difficult for generic tooling to
keep up with.

This project is an attempt at building more reliable tooling for ClickHouse queries by providing abstractions over the actual
ClickHouse parser.

> Note: Parsing is currently limited to `SELECT` queries, though other query types are on the roadmap.

## Usage

The primary entrypoint is a Python package, `clickshack`. Features include:

- A near like-for-like port of the actual ClickHouse SQL parser (currently supports version `26.2`)
- Converters for integrating with SQLGlot - parse queries with `clickshack` and manipulate them with SQLGlot's growing library of SQL utilities.
- (Experimental) - A minimal Python library for writing custom SQL linting rules.
  - Note: I am not entirely sure where this one is headed, but it might prove to be a useful counterpart to something like sqlcheck for ClickHouse.

Install from PyPI with:

```bash
pip install clickshack
```

Then parse a query with:

```python
from clickshack import parse_sql

envelope = parse_sql("SELECT id, name FROM users WHERE active = 1")
print(envelope.ir_version)  # "1"
print(envelope.query)       # SelectNode(...)
```

Integrate with SQLGlot:

```python
import sqlglot.expressions as exp
from sqlglot.lineage import lineage
from clickshack import parse_sql
from clickshack.ir.adapter import to_sqlglot

envelope = parse_sql("SELECT id, name FROM users WHERE active = 1")
sqlglot_expr = to_sqlglot(envelope.query)

# Trace which source column feeds 'name' in the output
node = lineage("name", sqlglot_expr)
for n in node.walk():
    print(n.name)
# name
# users.name
```

Or run one of the experimental linters with:

```python
from clickshack import parse_sql
from clickshack.ir.models import StarNode
from clickshack.linter.base import LintRunner, LintViolation


class NoSelectStar:
    """Prohibit SELECT *."""
    def check(self, node) -> list[LintViolation]:
        if isinstance(node, StarNode):
            return [LintViolation(rule="no-star", message="SELECT * is not allowed", span=node.span)]
        return []


envelope = parse_sql("SELECT * FROM events")
runner = LintRunner(rules=[NoSelectStar()])
violations = runner.run(envelope.query)
for v in violations:
    print(f"{v.rule} at offset {v.span.start}: {v.message}")
# no-star at offset 7: SELECT * is not allowed
```

See the modules under [`./clickshack/`](./clickshack) for more details (docs are still in the works).

## Development

### Prerequisites

- `bazel`, `just`, `uv`
- Python 3.11+ (managed automatically by uv)

### Setup

```bash
# Install Python dependencies (creates .venv automatically)
just sync
```

### Project Structure

```
ported_clickhouse/   C++ parser (ported ClickHouse SELECT dialect)
ir/                  C++ IR serializer (AST → JSON, uses nlohmann/json)
examples/bootstrap/  Smoke binaries incl. ir_dump (SQL stdin → JSON IR stdout)
clickshack/
  ir/                Pydantic v2 models + SQLGlot adapter for the JSON IR
  linter/            LintRule protocol + LintRunner for IR-based linting
tests/               Python tests (pytest)
testdata/            Parser workload fixtures and manifest
```

### Common Commands

| Command                  | What it does                              |
| ------------------------ | ----------------------------------------- |
| `just build`             | Build primary binary (`hello_clickshack`) |
| `just build-all`         | Build all example binaries                |
| `just build-ir`          | Build IR serializer and `ir_dump` binary  |
| `just run-ported-smokes` | Run C++ parser smoke tests                |
| `just sync`              | Install / sync Python dependencies        |
| `just test-python`       | Run Python tests                          |
| `just clean`             | Remove Bazel build outputs                |
| `just clean-expunge`     | Fully reset Bazel output state            |

### IR Dump

Parse SQL and emit the JSON IR:

```bash
echo "SELECT id FROM users WHERE active = 1" | \
  bazel-bin/examples/bootstrap/ir_dump
```

(Build with `just build-ir` first.)

## License

The `clickshack` Python package and original C++ code are licensed under the
[MIT License](LICENSE).

The C++ parser in `ported_clickhouse/` is derived from
[ClickHouse](https://github.com/ClickHouse/ClickHouse) and is licensed under
the [Apache License 2.0](ported_clickhouse/LICENSE). See [NOTICE](NOTICE) for
attribution details and a description of modifications.
