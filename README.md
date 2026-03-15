# Clickshack

ClickHouse SELECT dialect parser (C++17/Bazel) with a versioned JSON IR and a Python analysis layer.

## Prerequisites

- `bazel`, `just`, `uv`
- Python 3.11+ (managed automatically by uv)

## Setup

```bash
# Install Python dependencies (creates .venv automatically)
just sync
```

## Project Structure

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

## Common Commands

| Command | What it does |
|---|---|
| `just build` | Build primary binary (`hello_clickshack`) |
| `just build-all` | Build all example binaries |
| `just build-ir` | Build IR serializer and `ir_dump` binary |
| `just run-ported-smokes` | Run C++ parser smoke tests |
| `just sync` | Install / sync Python dependencies |
| `just test-python` | Run Python tests |
| `just clean` | Remove Bazel build outputs |
| `just clean-expunge` | Fully reset Bazel output state |

## IR Dump

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
