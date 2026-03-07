# Testing Patterns

## Current Testing Model
- Testing is currently smoke-test driven, centered on parser acceptance and token-consumption correctness.
- Primary executable tests live in `examples/bootstrap/*_smoke.cc`.
- Parser stacks are validated by building and running targeted binaries rather than a gtest suite.

## Core Test Entry Points
- `//examples/bootstrap:use_parser_smoke` -> `examples/bootstrap/use_parser_smoke.cc`
- `//examples/bootstrap:select_lite_smoke` -> `examples/bootstrap/select_lite_smoke.cc`
- `//examples/bootstrap:select_from_lite_smoke` -> `examples/bootstrap/select_from_lite_smoke.cc`
- `//examples/bootstrap:select_rich_smoke` -> `examples/bootstrap/select_rich_smoke.cc`
- Aggregate parser library build target: `//ported_clickhouse:parser_lib`

## Standard Validation Commands
- Use the scripted smoke flow in `Justfile`:
  - `just run-ported-smokes`
- Per roadmap contract (`docs/select_parser_roadmap.md`), phase validation includes:
  - `bazel --batch --output_user_root=/tmp/bazel-root build //ported_clickhouse:parser_lib`
  - `bazel --batch --output_user_root=/tmp/bazel-root build //examples/bootstrap:hello_clickshack`
  - `bazel --batch --output_user_root=/tmp/bazel-root build //examples/bootstrap:select_rich_smoke`

## Assertion Patterns in Smoke Binaries
- Parse success/failure is explicit via non-zero exit codes.
- Successful parses must consume all tokens (`!pos->isEnd()` triggers failure).
- Tests inspect AST shape and key flags instead of semantic outcomes.
- Output is compact and machine-readable enough for shell assertions:
  - field/value summaries in `examples/bootstrap/select_rich_smoke.cc`
  - expression/table summaries in `examples/bootstrap/select_lite_smoke.cc` and `examples/bootstrap/select_from_lite_smoke.cc`

## Parser-Boundary Testing Rules
- Keep tests syntax-only; do not assert planner/analyzer semantics.
- When adding grammar features, cover both:
  - expected-success queries with complete token consumption
  - expected-failure queries with deterministic failure mode
- Place clause-specific coverage near owning parser boundaries:
  - expressions: `ported_clickhouse/parsers/ExpressionOpsLiteParsers.cpp`
  - sources/joins: `ported_clickhouse/parsers/TableSourcesLiteParsers.cpp`
  - optional clauses: `ported_clickhouse/parsers/SelectClausesLiteParsers.cpp`

## Recommended Incremental Test Additions
- Add negative smoke cases for malformed set-operation chains and join `ON/USING` misuse.
- Expand matrix cases for `WITH`, `SETTINGS`, `FORMAT`, `LIMIT BY`, and `ORDER BY ... WITH FILL` combinations.
- Keep one high-signal happy-path query per feature in `select_rich_smoke` to detect regressions quickly.

## CI/Repo Hygiene Considerations
- Keep generated outputs and bazel artifacts out of VCS (`/bazel-*` already ignored in `.gitignore`).
- Treat `tmp/clickhouse-patching/*` as local upstream workspace; do not stage or commit it when adding tests.
- Before commit, confirm staged test changes are limited to intended files (`git diff --cached --name-only`).

## Practical Gaps
- No dedicated `cc_test`/gtest targets currently exist for parser units.
- Coverage is integration-smoke heavy; fine-grained parser-unit assertions are minimal.
- If parser complexity grows, add `cc_test` targets in `ported_clickhouse/parsers/BUILD.bazel` while preserving existing smoke binaries.
