# Architecture Map

## Scope
This project is a Bazel-first C++ parser bridge focused on ClickHouse SQL parsing.
The architecture centers on `ported_clickhouse/parsers/*` and builds a layered parser stack through Bazel targets.
Main public aggregate target is `//ported_clickhouse:parser_lib` from `ported_clickhouse/BUILD.bazel`.

## Architectural Intent
- Keep parser functionality local and buildable without full ClickHouse server dependencies.
- Port only required parser-related code into `ported_clickhouse/*`.
- Keep grammar growth parser-only (no analyzer/planner/runtime coupling).
- Validate behavior via focused smoke binaries under `examples/bootstrap/*_smoke.cc`.

## Core Architectural Layers
1. Lexing and parser core layer:
   - `ported_clickhouse/parsers/Lexer.cpp`
   - `ported_clickhouse/parsers/TokenIterator.cpp`
   - `ported_clickhouse/parsers/IParser.cpp`
   - Target: `//ported_clickhouse/parsers:parser_core`
2. Statement bootstrap layer (USE + shared primitives):
   - `ported_clickhouse/parsers/ParserUseQuery.cpp`
   - `ported_clickhouse/parsers/CommonParsers.cpp`
   - Target: `//ported_clickhouse/parsers:parser_use_stack`
3. SELECT growth layers:
   - Lite projection: `ParserSelectLiteQuery.cpp` (`parser_select_lite_stack`)
   - Lite FROM: `ParserSelectFromLiteQuery.cpp` (`parser_select_from_lite_stack`)
   - Expression ops: `ExpressionOpsLiteParsers.cpp` (`expression_ops_lite_stack`)
   - Table/join sources: `TableSourcesLiteParsers.cpp` (`parser_table_sources_stack`)
   - Optional clauses: `SelectClausesLiteParsers.cpp` (`parser_select_clauses_stack`)
   - Rich SELECT orchestration: `ParserSelectRichQuery.cpp` (`parser_select_rich_stack`)

## Parser Data Model (AST)
AST nodes are intentionally minimal and parser-facing.
Representative nodes:
- `ported_clickhouse/parsers/IAST.h` (base node)
- `ported_clickhouse/parsers/ASTSelectRichQuery.h`
- `ported_clickhouse/parsers/ASTJoinLite.h`
- `ported_clickhouse/parsers/ASTExpressionOpsLite.h`
- `ported_clickhouse/parsers/ASTLimitByLite.h`
Most `.cpp` AST files are lightweight linkage/clone implementations by design.

## Dependency Boundaries
Shared low-level support is isolated into:
- `ported_clickhouse/base/*`
- `ported_clickhouse/common/*`
- `ported_clickhouse/core/*`
Build defs:
- `ported_clickhouse/base/BUILD.bazel`
- `ported_clickhouse/common/BUILD.bazel`
- `ported_clickhouse/core/BUILD.bazel`
External deps come from `MODULE.bazel` (abseil, fmt, re2, openssl, boost, etc.).

## Relation to Upstream `src/Parsers`
`docs/ported_clickhouse_manifest.md` maps local files to upstream origins (for example `src/Parsers/*`).
This manifest is the synchronization contract for ported parser code and prune rationale.
Architecture assumes upstream-derived logic is copied/pruned into `ported_clickhouse/*`, then consumed by local Bazel targets.

## Runtime Validation Path
Parser stack is exercised by smoke binaries in `examples/bootstrap/BUILD.bazel`:
- `//examples/bootstrap:use_parser_smoke`
- `//examples/bootstrap:select_lite_smoke`
- `//examples/bootstrap:select_from_lite_smoke`
- `//examples/bootstrap:select_rich_smoke`
These encode parse success/failure and token-consumption expectations.

## Critical Constraint: `tmp/` Upstream Workspace
`tmp/clickhouse-patching/*` contains upstream/fork working sources and tooling artifacts.
It is an auxiliary workspace for sourcing/patching parser code, not the canonical bridge source tree.
`git ls-files tmp` currently returns nothing; keep it that way.
Do not move `tmp/*` into versioned bridge code; only curated files should live under `ported_clickhouse/*` and be tracked.

## Practical Change Flow
1. Update or add parser bridge code in `ported_clickhouse/parsers/*`.
2. Maintain layer dependencies in `ported_clickhouse/parsers/BUILD.bazel`.
3. Update provenance in `docs/ported_clickhouse_manifest.md`.
4. Verify with smoke targets in `examples/bootstrap/*`.
