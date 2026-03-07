# Integrations Map

## Scope
This file maps integration points across build, parser modules, external dependencies, and upstream ClickHouse artifacts for `/Users/dorran/dev/clickshack`.

## Integration Topology
- Public parser facade: `//ported_clickhouse:parser_lib` (`ported_clickhouse/BUILD.bazel`).
- Facade composes parser sub-libraries from `ported_clickhouse/parsers/BUILD.bazel`.
- Consumer executables live in `examples/bootstrap/` and integrate via Bazel deps only.

## Internal Integration Boundaries
- `parser_core` integrates with foundational shim libraries:
  - `//ported_clickhouse/base:base_headers`
  - `//ported_clickhouse/common:error_codes`
  - `//ported_clickhouse/core:core_headers`
- `parser_use_stack` depends on `parser_core` and introduces AST/keyword parser glue.
- SELECT-related stacks build strictly upward from `parser_use_stack` through rich SELECT layers.
- Clause parsing (`SelectClausesLiteParsers.*`) and table source parsing (`TableSourcesLiteParsers.*`) integrate via `parser_select_rich_stack`, not via direct executable coupling.

## External Dependency Integration
Bazel module integration is declared in `MODULE.bazel` and consumed in examples/ported parser code.
- Directly exercised in `examples/bootstrap:deps_probe`: `absl`, `boost.config`, `double-conversion`, `fmt`, `magic_enum`, `openssl`, `re2`.
- Parser core specifically references Abseil containers in `ported_clickhouse/parsers/BUILD.bazel` (`flat_hash_map`, `inlined_vector`).

## Upstream ClickHouse Integration Boundary
- Canonical upstream parser origin is ClickHouse `src/Parsers`.
- Imported/pruned mirror is tracked in `docs/ported_clickhouse_manifest.md` with row-level source mapping.
- Practical rule: modify local parser bridge under `ported_clickhouse/` first; treat `tmp/clickhouse-patching/` as upstream-sync support workspace.

## Packaging and CMake Side Integration (Upstream Workspace)
Artifacts in `tmp/clickhouse-patching/transpiler/cmake-parser-approach.md` define a separate integration surface:
- CMake package target: `ClickHouseSQLParser::sql_parser`.
- Export generation uses compile-command extraction (`generate_sql_parser_targets.py`).
- Smoke consumer for package compatibility: `tmp/clickhouse-patching/smoke/sql-parser-package/`.
This is adjacent to, but separate from, Bazel-native integration in this repo root.

## Parser-Relevant Dependency Boundaries
- `CommonParsers.*` acts as shared token/keyword boundary used by multiple parser stacks.
- `ExpressionOpsLiteParsers.*` is the main expression grammar boundary; it intentionally excludes full analyzer/type-system coupling.
- `TableSourcesLiteParsers.*` captures JOIN/source syntax boundaries while avoiding planner integration.
- `ParserSelectRichQuery.*` is the orchestration boundary that composes source, clause, and set-op parsers.

## VCS and Workspace Hygiene Boundary
- `tmp/` currently appears as untracked (`git status`), so accidental commit risk exists.
- User requirement: upstream ClickHouse sources in `tmp/` must remain out of versioned project history.
- Operational guidance:
  - Keep durable project code/docs in tracked paths like `ported_clickhouse/`, `docs/`, `.planning/`.
  - Treat `tmp/clickhouse-patching/` as transient integration reference, not primary repo content.

## Verification Integrations
- `Justfile` `run-ported-smokes` integrates build + runtime checks across parser layers.
- Rich parser integration is sanity-checked by `examples/bootstrap/select_rich_smoke.cc` against success and failure query matrices.
- `docs/select_parser_roadmap.md` defines planned compatibility growth while preserving parser-only boundaries.
