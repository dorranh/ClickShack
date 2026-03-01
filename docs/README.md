# Clickshack Phase 1

This directory bootstraps a BCR-first Bazel build with no ClickHouse parser source.

## Targets

- `//examples/bootstrap:core`
- `//examples/bootstrap:hello_clickshack`
- `//examples/bootstrap:deps_probe`

## Third-party strategy

Prefer Bazel Central Registry dependencies. Use local vendoring only as fallback.
