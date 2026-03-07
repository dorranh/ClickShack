# Clickshack Parser Bridge

## What This Is

Clickshack Parser Bridge is a standalone C++ library, built with Bazel, that ports the ClickHouse SQL parser stack into an independent codebase. It focuses on query parsing and AST/IR generation only, intentionally excluding analyzers and execution semantics. The immediate mission is to make the ported parser reliable and build-stable while preserving practical SQL coverage for internal tooling.

## Core Value

Reliable parser-only extraction of ClickHouse SQL into a lightweight standalone Bazel C++ library that internal tooling can trust.

## Requirements

### Validated

- ✓ Bazel-based C++ parser library composition exists under `ported_clickhouse/parsers/*` and aggregates via `//ported_clickhouse:parser_lib` — existing
- ✓ Layered parser targets and smoke binaries for `USE`, lite `SELECT`, `FROM`, and rich `SELECT` flows are implemented (`examples/bootstrap/*_smoke.cc`) — existing
- ✓ Upstream provenance and sync intent for parser sources is documented (`docs/ported_clickhouse_manifest.md`) — existing
- ✓ `tmp/` is treated as non-versioned upstream working area and not canonical project source (`git ls-files tmp` empty) — existing

### Active

- [ ] Restore and stabilize Bazel builds/tests after system Bazel upgrade
- [ ] Improve parser behavior for key internal `SELECT` workload coverage with parser-only semantics
- [ ] Generate stable parser IR from query AST as a C++ application output contract
- [ ] Preserve near-full SQL parser parity where valuable, while excluding non-SQL/MSSQL parser branches
- [ ] Minimize dependency surface by stubbing/removing unneeded heavy upstream `common`/adjacent dependencies

### Out of Scope

- SQL analysis, semantic resolution, query planning, or execution runtime — parser-only project scope
- Python sqlglot transpilation in current milestone — explicitly deferred to later phase
- Public SDK polish and general external-consumer hardening for v1 — internal tooling is primary user
- One-to-one parity for non-SQL and MSSQL parser branches — not needed for target workload

## Context

- Existing codebase already contains a substantial parser port in `ported_clickhouse/` with staged target layering documented in `.planning/codebase/ARCHITECTURE.md`.
- Build orchestration uses Bazel modules (`MODULE.bazel`) and `just` recipes (`Justfile`), with smoke binaries under `examples/bootstrap/` as main behavior checks.
- Upstream ClickHouse workspace material is present in `tmp/` for patching/reference and must remain out of VCS; curated bridge code lives in tracked `ported_clickhouse/*`.
- Current pain point: recent system Bazel upgrade introduced regressions, making build reliability an immediate blocker.

## Constraints

- **Scope**: Parser-only extraction — no analyzers/execution paths to be introduced
- **Source Boundary**: `tmp/` upstream sources are reference/patch workspace only, not committed canonical code
- **Dependency Budget**: Keep build light; avoid unnecessary heavy dependency pull-through from upstream `common` and unrelated components
- **Compatibility**: Maintain practical ClickHouse parser behavior for targeted internal SQL workloads
- **Build System**: Bazel is mandatory build/test system for this project

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Prioritize internal tooling consumers for v1 | Allows faster iteration and pragmatic interfaces before external hardening | — Pending |
| Phase 1 focuses on Bazel/build stabilization | Current regressions from Bazel upgrade block safe parser iteration | — Pending |
| Parser parity target is near-full SQL with explicit exclusions | Keeps utility high while avoiding irrelevant/non-SQL branches | — Pending |
| Exclude non-SQL/MSSQL parser branches | Not required for target workloads and adds maintenance overhead | — Pending |
| Keep Python sqlglot transpiler out of current milestone | Prevents scope dilution; parser reliability first | — Pending |

---
*Last updated: 2026-03-07 after initialization*
