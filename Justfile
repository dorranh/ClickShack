set shell := ["bash", "-euo", "pipefail", "-c"]

# Shared Bazel invocation and target groups.
bazel := "bazel --batch --output_user_root=/tmp/bazel-root"
main_targets := "//examples/bootstrap:hello_clickshack"
all_targets := "//examples/bootstrap:hello_clickshack //examples/bootstrap:deps_probe"

# Build the primary Clickshack binary target.
build:
    {{bazel}} build {{main_targets}}

# Alias for the main build.
build-main: build

# Build all current example targets.
build-all:
    {{bazel}} build {{all_targets}}

# Build the imported ClickHouse parser core subset.
build-parser-core:
    {{bazel}} build //ported_clickhouse:parser_core

# Run all current ported_clickhouse smoke tests.
run-ported-smokes:
    {{bazel}} build //ported_clickhouse:parser_lib //examples/bootstrap:use_parser_smoke //examples/bootstrap:select_lite_smoke //examples/bootstrap:select_from_lite_smoke
    bazel-bin/examples/bootstrap/use_parser_smoke "USE mydb"
    bazel-bin/examples/bootstrap/select_lite_smoke "SELECT a, 1, f(x)"
    bazel-bin/examples/bootstrap/select_from_lite_smoke "SELECT a, 1, f(x) FROM mydb.mytable"

# Build only hello_clickshack.
build-hello:
    {{bazel}} build //examples/bootstrap:hello_clickshack

# Build only the dependency probe binary.
build-deps-probe:
    {{bazel}} build //examples/bootstrap:deps_probe

# Remove Bazel outputs while keeping external downloads/cache metadata.
clean:
    {{bazel}} clean

# Fully reset Bazel output base for this workspace.
clean-expunge:
    {{bazel}} clean --expunge
