# Structure Map

## Repository Top Level
- `BUILD.bazel`: root exports and visibility setup.
- `MODULE.bazel`: Bazel module dependencies.
- `Justfile`: common build/test helper commands.
- `README.md`: bootstrap usage.
- `docs/`: architecture notes, parser roadmap, port manifest.
- `examples/bootstrap/`: sample binaries and parser smoke checks.
- `ported_clickhouse/`: versioned parser bridge implementation.
- `third_party/local/`: local third-party Bazel wiring.
- `tmp/`: upstream patching workspace and analysis artifacts (not VCS content).

## Parser-Centric Directory Structure
### `ported_clickhouse/parsers/`
Primary implementation area for parser architecture.
Key file groups:
- Core infra: `Lexer.*`, `TokenIterator.*`, `IParser.*`, `IParserBase.*`.
- Shared parser primitives: `CommonParsers.*`.
- Expression parsers: `ExpressionLiteParsers.*`, `ExpressionOpsLiteParsers.*`, `ExpressionElementParsers.*`.
- Statement/top-level parsers: `ParserUseQuery.*`, `ParserSelectLiteQuery.*`, `ParserSelectFromLiteQuery.*`, `ParserSelectRichQuery.*`.
- Source/clause parsers: `TableExpressionLiteParsers.*`, `TableSourcesLiteParsers.*`, `SelectClausesLiteParsers.*`.
- AST nodes: `AST*.h/.cpp` files for parser outputs.
- Build wiring: `ported_clickhouse/parsers/BUILD.bazel`.

### `ported_clickhouse/base/`, `ported_clickhouse/common/`, `ported_clickhouse/core/`
Support shims used by parser code:
- `base`: low-level types/macros/helpers.
- `common`: error codes, exception, parser-needed utility helpers.
- `core`: core macro header(s).
These are intentionally small and parser-support-only.

## Target-to-File Structure
Declared in `ported_clickhouse/parsers/BUILD.bazel` and re-exported in `ported_clickhouse/BUILD.bazel`.
Layering order:
1. `parser_core`
2. `parser_use_stack`
3. `parser_select_lite_stack`
4. `parser_select_from_lite_stack`
5. `expression_ops_lite_stack`
6. `parser_table_sources_stack`
7. `parser_select_clauses_stack`
8. `parser_select_rich_stack`
9. aggregate `parser_lib`

## Validation Structure
- `examples/bootstrap/BUILD.bazel` defines parser smoke binaries.
- `examples/bootstrap/use_parser_smoke.cc` validates `USE` parsing.
- `examples/bootstrap/select_lite_smoke.cc` validates projection-only SELECT.
- `examples/bootstrap/select_from_lite_smoke.cc` validates SELECT + FROM.
- `examples/bootstrap/select_rich_smoke.cc` validates broad SELECT-rich grammar.

## Upstream Provenance and Sync Structure
- `docs/ported_clickhouse_manifest.md`: source-of-truth mapping local files to upstream `src/Parsers` and related paths.
- `docs/select_parser_roadmap.md`: planned parser-surface evolution and acceptance checks.
Together they define what is ported, what is bridge-only, and what remains out of scope.

## `tmp/` Workspace Boundary
`tmp/clickhouse-patching/` holds upstream checkout/fork patches, prune tooling, and artifacts such as:
- `tmp/clickhouse-patching/artifacts/parser-prune/*`
- `tmp/clickhouse-patching/tools/prune/*`
- `tmp/clickhouse-patching/src/*`
This directory is an integration aid, not product source for this repo.
Per current repo state, `tmp/*` is not tracked (`git ls-files tmp` is empty).
Keep upstream sources and heavy artifacts in `tmp/` only; promote only curated bridge code into `ported_clickhouse/*`.

## Ownership Hotspots for Future Mapping
- Grammar behavior changes: `ported_clickhouse/parsers/ParserSelectRichQuery.cpp`, `SelectClausesLiteParsers.cpp`, `ExpressionOpsLiteParsers.cpp`, `TableSourcesLiteParsers.cpp`.
- AST shape changes: corresponding `AST*.h` files in `ported_clickhouse/parsers/`.
- Dependency/layer changes: `ported_clickhouse/parsers/BUILD.bazel` and `ported_clickhouse/BUILD.bazel`.
- Upstream sync bookkeeping: `docs/ported_clickhouse_manifest.md`.
