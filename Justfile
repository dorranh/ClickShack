set shell := ["bash", "-euo", "pipefail", "-c"]

# Shared Bazel invocation and target groups.
bazel := "bazel --batch --output_user_root=/tmp/bazel-root"
main_targets := "//examples/bootstrap:hello_clickshack"
all_targets := "//examples/bootstrap:hello_clickshack //examples/bootstrap:deps_probe"
ir_targets := "//ir:ir_json //examples/bootstrap:ir_dump"

# Build the primary Clickshack binary target.
build:
    {{bazel}} build {{main_targets}}

# Alias for the main build.
build-main: build

# Build all current example targets.
build-all:
    {{bazel}} build {{all_targets}}

# Run the quick parser smoke subset (fast feedback).
smoke-quick:
    ./tools/parser_smoke_suite.sh quick

# Run the deterministic parser smoke suite (quick subset first, full suite second).
smoke-suite:
    ./tools/parser_smoke_suite.sh suite

# Build the imported ClickHouse parser core subset.
build-parser-core:
    {{bazel}} build //ported_clickhouse:parser_core

# Run all current ported_clickhouse smoke tests.
run-ported-smokes:
    {{bazel}} build //ported_clickhouse:parser_lib //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke //examples/bootstrap:select_from_lite_smoke //examples/bootstrap:select_rich_smoke
    bazel-bin/examples/bootstrap/use_parser_smoke "USE mydb"
    bazel-bin/examples/bootstrap/select_lite_smoke "SELECT a, 1, f(x)"
    bazel-bin/examples/bootstrap/select_from_lite_smoke "SELECT a, 1, f(x) FROM mydb.mytable"
    bazel-bin/examples/bootstrap/select_rich_smoke "SELECT a FROM t"

# Build only hello_clickshack.
build-hello:
    {{bazel}} build //examples/bootstrap:hello_clickshack

# Build only the dependency probe binary.
build-deps-probe:
    {{bazel}} build //examples/bootstrap:deps_probe

# Build the IR JSON serializer and ir_dump binary.
build-ir:
    {{bazel}} build {{ir_targets}}

# Install Python dependencies (creates .venv automatically).
sync:
    uv sync --group dev

# Lint with ruff.
lint:
    uv run ruff check clickshack/ tests/

# Format with ruff.
fmt:
    uv run ruff format clickshack/ tests/

# Type-check with pyright.
typecheck:
    uv run pyright clickshack/

# Run all checks (lint + typecheck). Does not auto-format.
check: lint typecheck

# Run Python tests.
test-python:
    uv run pytest tests/ -v

# Remove Bazel outputs while keeping external downloads/cache metadata.
clean:
    {{bazel}} clean

# Fully reset Bazel output base for this workspace.
clean-expunge:
    {{bazel}} clean --expunge
