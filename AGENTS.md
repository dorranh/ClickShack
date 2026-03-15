# clickshack

This project is a porting of the ClickHouse SQL parser libs, related C++ utilities for generating
an IR for the ClickHouse SQL dialect, and transpilers for converting this IR to a SQLGlot
AST.

## Guidelines

- Keep builds efficient and reproducible using Bazel
- Add smoke tests for new ported parser components
- For any non-ported components include tests / integration test wherever possible.

## C++ Tooling

- Build with Bazel
- Prefer Bazel Central Registry dependencies. Use local vendoring only as fallback.

## Python Tooling

- **Formatter/linter:** ruff (`just fmt`, `just lint`)
- **Type checker:** pyright (`just typecheck`)
- **Run all checks:** `just check`
- Always run `just check` before committing Python changes.
