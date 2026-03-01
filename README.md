# Clickshack

Minimal Bazel bootstrap project for Clickshack examples.

## Prerequisites

- `bazel`
- `just`

## Common Commands

- `just build` or `just build-main`: build the primary binary (`hello_clickshack`)
- `just build-hello`: build only `hello_clickshack`
- `just build-deps-probe`: build only `deps_probe`
- `just build-all`: build both example binaries
- `just clean`: remove Bazel build outputs
- `just clean-expunge`: fully reset Bazel output state

## Output Binary

Primary executable path after build:

`bazel-bin/examples/bootstrap/hello_clickshack`
