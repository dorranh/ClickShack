# Technology Stack Map

## Scope
This map describes the current stack in `/Users/dorran/dev/clickshack` with emphasis on parser-focused code under `ported_clickhouse/`.
Primary parser lineage comes from upstream ClickHouse `src/Parsers` and is mirrored/pruned locally under `ported_clickhouse/parsers/`.

## Build and Tooling
- Build system: Bazel modules (`MODULE.bazel`, `MODULE.bazel.lock`, `BUILD.bazel`).
- Task runner: `just` recipes in `Justfile`.
- Bazel output root is explicitly directed to `/tmp/bazel-root` via `Justfile` (`bazel --batch --output_user_root=/tmp/bazel-root`).
- Project-level parser entry aggregate target: `//ported_clickhouse:parser_lib` in `ported_clickhouse/BUILD.bazel`.

## Languages and Runtime
- Core language: C++ (`cc_library`, `cc_binary` targets).
- Shell tooling: bash/zsh scripts for pruning and dependency workflows (for example `tmp/clickhouse-patching/tools/prune/check_prune_integrity.sh`).
- Python helper tooling exists in upstream patch workspace (`tmp/clickhouse-patching/...`) for packaging automation.

## External Dependencies (Bazel BCR)
Declared in `MODULE.bazel`:
- `abseil-cpp` (`@abseil_cpp`)
- `rules_cc`
- `fmt` (`@fmt`)
- `re2` (`@re2`)
- `double-conversion` (`@double_conversion`)
- `magic_enum` (`@magic_enum`)
- `openssl` (`@openssl`)
- `boost` (`@boost`)
- `boost.config` (`@boost_config`)

## Parser Stack Layering
Parser targets are intentionally split into composable stages in `ported_clickhouse/parsers/BUILD.bazel`:
1. `parser_core`: lexer/token/parsing base (`Lexer.*`, `IParser*`, `TokenIterator*`).
2. `parser_use_stack`: baseline AST + `USE` grammar (`ParserUseQuery.*`, `CommonParsers.*`).
3. `parser_select_lite_stack`: minimal `SELECT expr[, ...]` coverage.
4. `parser_select_from_lite_stack`: adds simple `FROM` table refs.
5. `expression_ops_lite_stack`: operator precedence expression parsing.
6. `parser_table_sources_stack`: joins, source expressions, table functions, subquery source hooks.
7. `parser_select_clauses_stack`: optional clause chain (`WHERE/GROUP/ORDER/LIMIT/...`).
8. `parser_select_rich_stack`: top-level rich SELECT and set-ops (`UNION/INTERSECT/EXCEPT`).

## Parser Boundary vs Analyzer
- This repository is parser-only by design for the ported subset.
- `docs/select_parser_roadmap.md` explicitly keeps work parser-scoped and avoids analyzer/semantic coupling.
- Bridge AST types under `ported_clickhouse/parsers/*Lite*` keep syntax structure without full ClickHouse execution semantics.

## Source-of-Truth and Upstream Relationship
- Local parser code is tracked in `ported_clickhouse/` with sync notes in `docs/ported_clickhouse_manifest.md`.
- Upstream patch workspace exists in `tmp/clickhouse-patching/` and includes packaging/pruning references (`transpiler/`, `tools/prune/`, `smoke/`).
- User context constraint: upstream ClickHouse sources in `tmp/` are working material and should not be checked into VCS.

## Verification Surface
- Smoke binaries in `examples/bootstrap/BUILD.bazel`: `use_parser_smoke`, `select_lite_smoke`, `select_from_lite_smoke`, `select_rich_smoke`.
- Consolidated smoke runner in `Justfile` target `run-ported-smokes`.
- Dependency probe binary `//examples/bootstrap:deps_probe` validates linkage to major BCR dependencies.
